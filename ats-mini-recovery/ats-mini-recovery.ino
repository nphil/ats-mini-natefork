/*
 * ATS-Mini Recovery Firmware — Dracula UI, encoder navigation
 *
 * Boot trigger: hold encoder button during KQ4TXO splash → recovery UI.
 * Otherwise auto-boots main firmware after 2s. Boot-loop detection at 3 fails.
 *
 * Main menu: Boot / Erase Data / Network / Reboot
 * Network:   WiFi Scan → keyboard → connect | OTA list | Web Server | Back
 *
 * Networking is brought up in the background (AP+STA): the "ATS-Recovery" AP is
 * always available for the Android app, while STA connects to a known router for
 * internet (GitHub OTA). The menu appears instantly — no blocking connect screen.
 *
 * Encoder: GPIO2 (A), GPIO1 (B), GPIO21 (button)
 * Display: ST7789 320×170 (rotation 3, 8-bit parallel)
 */

#include <Arduino.h>
#include <FS.h>
using namespace fs;
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <nvs_flash.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <driver/gpio.h>
#include <TFT_eSPI.h>

// ── Pins ──────────────────────────────────────────────────────────────────────
#define PIN_LCD_BL 38
#define PIN_PWR    15
#define PIN_BTN    21
#define PIN_ENC_A   2
#define PIN_ENC_B   1

// ── Dracula palette (RGB565) ──────────────────────────────────────────────────
#define D_BG      0x2947u
#define D_LINE    0x424Bu
#define D_FG      0xF7BDu
#define D_COMMENT 0x6394u
#define D_CYAN    0x8F5Fu
#define D_GREEN   0x57CFu
#define D_ORANGE  0xFDADu
#define D_PINK    0xFBD8u
#define D_PURPLE  0xBC9Eu
#define D_RED     0xFAAAu
#define D_YELLOW  0xEFD1u

// ── Layout (320×170 landscape) ────────────────────────────────────────────────
static const int W = 320, H = 170;
static const int HDR_H = 22;
static const int FTR_H = 20;
static const int FTR_Y = H - FTR_H;
static const int BODY_Y = HDR_H + 2;
static const int BODY_H = FTR_Y - BODY_Y;
static const int ITEM_H = 24;   // 5 items fit: 5*24=120 <= BODY_H(126), no footer overlap

// ── Screens ───────────────────────────────────────────────────────────────────
enum Screen : uint8_t {
  SCR_MAIN, SCR_ERASE_CONFIRM, SCR_NETWORK,
  SCR_WIFI_SCAN, SCR_WIFI_KB,
  SCR_OTA_LIST, SCR_OTA_PROGRESS, SCR_WEBSERVER,
};

// ── Known networks + AP fallback ──────────────────────────────────────────────
struct KnownNet { const char* ssid; const char* pass; };
static const KnownNet KNOWN[] = {
  { "BEAST_ROUTER", "appu1989" },
  { "IoT",          "appu1989" },
};
static const int KNOWN_CNT = sizeof(KNOWN) / sizeof(KNOWN[0]);
static const char* AP_SSID = "ATS-Recovery";
static const char* AP_PASS = "ats12345";

// ── GitHub ────────────────────────────────────────────────────────────────────
static const char* GITHUB_REPO = "nphil/ats-mini-natefork";

// ── Objects ───────────────────────────────────────────────────────────────────
static TFT_eSPI tft;
static WebServer server(80);

// ── Runtime state ─────────────────────────────────────────────────────────────
static Screen cur = SCR_MAIN;
static int    sel = 0;
static bool   dirty = true;
static bool   fullDraw = true;     // true → clear + redraw all; false → only changed rows
static int    lastSel = -1;        // previously highlighted row (for partial redraw)
static int    lastStart = -1;      // previous scroll offset (list screens)
static bool   footerDirty = false; // footer needs refresh (network state changed)
static bool   wifiOK   = false;
static bool   adhocOn  = false;
static bool   webOn    = false;

// Toast shown in footer; takes priority over network status text
static char     toastMsg[80] = "";
static uint32_t toastExp     = 0;
static void showToast(const char* msg, uint32_t ms = 4000) {
  strncpy(toastMsg, msg, sizeof(toastMsg) - 1);
  toastMsg[sizeof(toastMsg) - 1] = 0;
  toastExp = millis() + ms;
  footerDirty = true; dirty = true;
}

// ── Background STA connection state ────────────────────────────────────────────
static bool     staTrying  = false;
static int      staTryIdx  = 0;
static uint32_t staTryStart = 0;

// ── Encoder (ISR) ─────────────────────────────────────────────────────────────
static volatile int8_t encAcc = 0;
static int encALast = HIGH;

static void IRAM_ATTR encISR() {
  int a = digitalRead(PIN_ENC_A);
  int b = digitalRead(PIN_ENC_B);
  if (a != encALast) { encAcc += (a != b) ? 1 : -1; encALast = a; }
}

static int encPop() {
  if (encAcc >= 2)  { encAcc -= 2; return  1; }
  if (encAcc <= -2) { encAcc += 2; return -1; }
  return 0;
}

static uint32_t btnDownMs = 0;
static bool btnWas = false;
static int  btnPhase = 0; // 0=none, 1=medium(450ms) fired, 2=long(2s) fired
static int btnEvent() {
  bool dn = (digitalRead(PIN_BTN) == LOW);
  if (dn && !btnWas) { btnWas = true; btnDownMs = millis(); btnPhase = 0; }
  if (!dn && btnWas) { btnWas = false; return (btnPhase == 0) ? 1 : 0; }
  if (dn && btnPhase == 0 && millis() - btnDownMs >  450) { btnPhase = 1; return 2; }
  if (dn && btnPhase == 1 && millis() - btnDownMs > 2000) { btnPhase = 2; return 3; }
  return 0;
}

// ── WiFi scan results ─────────────────────────────────────────────────────────
struct ScannedNet { String ssid; int32_t rssi; };
static ScannedNet scanned[20];
static int scannedCnt = 0;
static bool scanning  = false;

// ── WiFi keyboard state ───────────────────────────────────────────────────────
static String kbSSID, kbPass;
static int    kbRow = 0, kbCol = 0;
static int    lastKbR = -1, lastKbC = -1; // for partial keyboard redraw
static bool   kbShift = false, kbNums = false;

// Step the keyboard cursor linearly across all rows (encoder navigation).
static void kbStep(int dir) {
  if (dir > 0) {
    kbCol++;
    if (kbCol >= kbRowLen(kbRow)) { kbCol = 0; kbRow = (kbRow + 1) % 4; }
  } else if (dir < 0) {
    kbCol--;
    if (kbCol < 0) { kbRow = (kbRow + 3) % 4; kbCol = kbRowLen(kbRow) - 1; }
  }
}

// Keyboard layout: rows of: printable | \x7f=DEL | \r=OK | \x01=SHIFT | \x02=NUM
static const char KB_ALPHA[4][12] = {
  "qwertyuiop",
  "asdfghjkl",
  "zxcvbnm\x7f",
  "\x01\x02 \r",
};
static const char KB_NUMS[4][12] = {
  "1234567890",
  "!@#$%^&*(",
  ")-_=+[]\x7f",
  "\x01\x02 \r",
};

static int kbRowLen(int row) {
  return strlen(kbNums ? KB_NUMS[row] : KB_ALPHA[row]);
}
static char kbChar(int row, int col) {
  char c = (kbNums ? KB_NUMS : KB_ALPHA)[row][col];
  if (c > 0x20 && c < 0x7f && !kbNums && kbShift) return toupper(c);
  return c;
}

// ── GitHub releases ───────────────────────────────────────────────────────────
struct Release { char tag[16]; char url[160]; };
static Release releases[25];
static int  relCnt    = 0;
static bool relLoaded = false;
static char relErr[64] = "";

// ── OTA ───────────────────────────────────────────────────────────────────────
static esp_ota_handle_t       otaH    = 0;
static const esp_partition_t* otaPartP = nullptr;
static size_t otaTotal = 0, otaWritten = 0;
static bool   otaOn   = false;
static char   otaTag[32] = "";

static const esp_partition_t* getOta0() {
  if (!otaPartP)
    otaPartP = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  return otaPartP;
}
static bool otaBegin(size_t sz = OTA_WITH_SEQUENTIAL_WRITES) {
  if (otaOn) esp_ota_abort(otaH);
  if (!getOta0()) return false;
  if (esp_ota_begin(otaPartP, OTA_WITH_SEQUENTIAL_WRITES, &otaH) != ESP_OK) return false;
  otaTotal = sz; otaWritten = 0; otaOn = true;
  return true;
}
static bool otaWrite(const uint8_t* d, size_t n) {
  if (!otaOn) return false;
  if (esp_ota_write(otaH, d, n) != ESP_OK)
    { esp_ota_abort(otaH); otaOn = false; return false; }
  otaWritten += n; return true;
}
static bool otaFinish() {
  if (!otaOn) return false; otaOn = false;
  if (esp_ota_end(otaH) != ESP_OK) return false;
  if (esp_ota_set_boot_partition(getOta0()) != ESP_OK) return false;
  Preferences p; p.begin("recovery", false, "settings");
  p.putInt("bootcount", 0); p.end();
  return true;
}
static void otaAbortAll() { if (otaOn) { esp_ota_abort(otaH); otaOn = false; } }

// ── CRC32 (zlib / IEEE-802.3, matches java.util.zip.CRC32) ─────────────────────
// Running form: start state 0xFFFFFFFF, accumulate, final result = state ^ 0xFFFFFFFF.
static uint32_t crc32Run(uint32_t crc, const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int k = 0; k < 8; k++)
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return crc;
}

// CRC32 of the first [size] bytes of a partition.
static uint32_t partCrc32(const esp_partition_t* p, size_t size) {
  static uint8_t cbuf[1024];
  uint32_t crc = 0xFFFFFFFFu;
  size_t off = 0;
  while (off < size) {
    size_t n = min((size_t)sizeof(cbuf), size - off);
    if (esp_partition_read(p, off, cbuf, n) != ESP_OK) return 0;
    crc = crc32Run(crc, cbuf, n);
    off += n;
  }
  return crc ^ 0xFFFFFFFFu;
}

static const esp_partition_t* getOta1() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
}
static const esp_partition_t* getFactory() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
}

// ── Serial OTA ────────────────────────────────────────────────────────────────
// SS_OTA  → app firmware into ota_0 (main app)
// SS_REC  → new recovery image into ota_1 (stage 1 of self-migration to factory)
enum SerialSt { SS_IDLE, SS_OTA, SS_REC };
static SerialSt sst = SS_IDLE;
static size_t sOtaExp = 0;

// Stage-1 (receive new recovery into ota_1) state.
static esp_ota_handle_t       recH = 0;
static const esp_partition_t* recPart = nullptr;
static size_t   recExp = 0, recWritten = 0;
static uint32_t recCrcRun = 0xFFFFFFFFu;   // running CRC of received bytes
static uint32_t recCrcExp = 0;             // expected CRC (from app); 0 = skip check
static bool     recOn = false;

// ─────────────────────────────────────────────────────────────────────────────
// Redraw bookkeeping
// ─────────────────────────────────────────────────────────────────────────────

// Request a full screen redraw (used when non-selection content changes).
static void needFull() { dirty = true; fullDraw = true; lastSel = -1; lastStart = -1; }

// Compute scroll offset that keeps `sel` visible, biased to the previous offset.
static int listStart(int s, int total, int vis) {
  int start = (lastStart < 0) ? 0 : lastStart;
  if (s < start) start = s;
  if (s >= start + vis) start = s - vis + 1;
  if (start > total - vis) start = total - vis;
  if (start < 0) start = 0;
  return start;
}

// ─────────────────────────────────────────────────────────────────────────────
// Display helpers
// ─────────────────────────────────────────────────────────────────────────────

static void drawHeader(const char* title) {
  tft.fillRect(0, 0, W, HDR_H, D_PURPLE);
  tft.setTextColor(D_BG, D_PURPLE);
  tft.setTextFont(1);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("ATS-Mini Recovery", 5, HDR_H / 2);
  tft.setTextFont(2);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(title, W - 5, HDR_H / 2);
}

static void drawFooter() {
  tft.fillRect(0, FTR_Y, W, FTR_H, D_LINE);
  tft.setTextFont(1);
  tft.setTextDatum(ML_DATUM);
  char buf[80];
  if (toastMsg[0] && millis() < toastExp) {
    tft.setTextColor(D_YELLOW, D_LINE);
    tft.drawString(toastMsg, 4, FTR_Y + FTR_H / 2);
  } else {
    toastMsg[0] = 0;
    tft.setTextColor(D_COMMENT, D_LINE);
    if (wifiOK)
      snprintf(buf, sizeof(buf), "WiFi: %s  %s",
               WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    else if (adhocOn)
      snprintf(buf, sizeof(buf), "Adhoc: %s  %s",
               AP_SSID, WiFi.softAPIP().toString().c_str());
    else if (staTrying)
      snprintf(buf, sizeof(buf), "Connecting: %s...",
               KNOWN[staTryIdx < KNOWN_CNT ? staTryIdx : 0].ssid);
    else
      strcpy(buf, "WiFi: not connected");
    tft.drawString(buf, 4, FTR_Y + FTR_H / 2);
  }
}

static void clearBody() {
  tft.fillRect(0, BODY_Y, W, FTR_Y - BODY_Y, D_BG);
}

static void drawItem(int row, const char* label, const char* value, bool hi) {
  int y = BODY_Y + row * ITEM_H;
  uint16_t bg = hi ? D_LINE : D_BG;
  uint16_t tc = hi ? D_CYAN : D_FG;
  tft.fillRect(0, y, W, ITEM_H, bg);
  if (hi) tft.fillRect(0, y, 3, ITEM_H, D_PURPLE);
  tft.setTextColor(tc, bg);
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(label, 10, y + ITEM_H / 2);
  if (value && *value) {
    tft.setTextColor(hi ? D_YELLOW : D_COMMENT, bg);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(value, W - 5, y + ITEM_H / 2);
  }
}

// ── OTA progress display (flicker-free: frame drawn once, only bar+text update) ─
static int otaBarY = 0;

static void drawOtaFrame(const char* phase) {
  int y0 = BODY_Y;
  tft.fillRect(0, y0, W, FTR_Y - y0, D_BG);   // one-time body clear
  tft.setTextColor(D_CYAN, D_BG);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(otaTag[0] ? otaTag : phase, W / 2, y0 + 12);
  otaBarY = y0 + 34;
  tft.drawRect(12, otaBarY, W - 24, 16, D_PURPLE);   // empty bar frame
}

static void drawOtaUpdate(uint32_t kbps) {
  int bw = W - 24;
  if (otaTotal > 0) {
    int fill = (int)((uint64_t)otaWritten * (bw - 2) / otaTotal);
    tft.fillRect(13, otaBarY + 1, fill, 14, D_GREEN);  // monotonic — just paint done portion
  }
  char buf[48];
  int ly = otaBarY + 28;
  tft.fillRect(0, ly - 8, W, 16, D_BG);
  if (otaTotal > 0)
    snprintf(buf, sizeof(buf), "%u%%   %u / %u KB",
             (unsigned)(otaWritten * 100 / otaTotal),
             (unsigned)(otaWritten / 1024), (unsigned)(otaTotal / 1024));
  else
    snprintf(buf, sizeof(buf), "%u KB", (unsigned)(otaWritten / 1024));
  tft.setTextColor(D_FG, D_BG);
  tft.setTextFont(2); tft.setTextDatum(MC_DATUM);
  tft.drawString(buf, W / 2, ly);

  int sy = otaBarY + 48;
  tft.fillRect(0, sy - 6, W, 12, D_BG);
  if (otaTotal > 0 && kbps > 0) {
    uint32_t remain = (otaTotal - otaWritten) / 1024;
    snprintf(buf, sizeof(buf), "%u KB/s   ETA %us", (unsigned)kbps, (unsigned)(remain / (kbps ? kbps : 1)));
  } else {
    snprintf(buf, sizeof(buf), "%u KB/s", (unsigned)kbps);
  }
  tft.setTextColor(D_COMMENT, D_BG);
  tft.setTextFont(1); tft.setTextDatum(MC_DATUM);
  tft.drawString(buf, W / 2, sy);
}

static void drawOtaError(const char* msg) {
  int sy = otaBarY + 48;
  tft.fillRect(0, sy - 8, W, 16, D_BG);
  tft.setTextColor(D_RED, D_BG);
  tft.setTextFont(2); tft.setTextDatum(MC_DATUM);
  tft.drawString(msg, W / 2, sy);
}

// Generic small menu renderer with flicker-free partial redraw.
static void renderMenu(const char* title, const char* const* labels,
                       const char* const* values, int n) {
  if (fullDraw) {
    tft.fillScreen(D_BG);
    drawHeader(title);
    drawFooter();
    for (int i = 0; i < n; i++)
      drawItem(i, labels[i], values ? values[i] : nullptr, i == sel);
  } else {
    if (lastSel >= 0 && lastSel < n && lastSel != sel)
      drawItem(lastSel, labels[lastSel], values ? values[lastSel] : nullptr, false);
    if (sel >= 0 && sel < n)
      drawItem(sel, labels[sel], values ? values[sel] : nullptr, true);
    if (footerDirty) drawFooter();
  }
}

// Generic scrollable list renderer with flicker-free partial redraw.
// itemLabel(i) returns the label; itemValue(i) the right-aligned value (or "").
static void renderList(const char* title, int total,
                       const char* (*itemLabel)(int, char*),
                       const char* (*itemValue)(int, char*)) {
  int vis = BODY_H / ITEM_H;
  if (fullDraw) { tft.fillScreen(D_BG); drawHeader(title); drawFooter(); }
  int start = listStart(sel, total, vis);
  bool scrolled = (start != lastStart);
  char lbuf[40], vbuf[16];
  for (int row = 0; row < vis; row++) {
    int i = start + row;
    bool redraw = fullDraw || scrolled || (i == sel) || (i == lastSel);
    if (!redraw) continue;
    if (i < total) {
      const char* lab = itemLabel(i, lbuf);
      const char* val = itemValue ? itemValue(i, vbuf) : nullptr;
      drawItem(row, lab, val, i == sel);
    } else {
      tft.fillRect(0, BODY_Y + row * ITEM_H, W, ITEM_H, D_BG);
    }
  }
  if (footerDirty && !fullDraw) drawFooter();
  lastStart = start;
}

// ─────────────────────────────────────────────────────────────────────────────
// Splash screen
// ─────────────────────────────────────────────────────────────────────────────

// Set true when the factory partition is known to be damaged and a repair could
// not complete — recovery then refuses to hand off to the main app (which would
// erase otadata and leave the bootloader trying a broken factory → brick).
static bool gFacBad = false;

// Simple full-screen migration progress (own drawing; runs before the menu).
static void drawMig(const char* msg, int pct, uint16_t color) {
  tft.fillScreen(D_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(D_CYAN, D_BG); tft.setTextFont(2);
  tft.drawString("Updating Recovery", W / 2, 40);
  tft.setTextColor(color, D_BG); tft.setTextFont(2);
  tft.drawString(msg, W / 2, 80);
  if (pct >= 0) {
    int bw = W - 24;
    tft.drawRect(12, 110, bw, 16, D_PURPLE);
    tft.fillRect(13, 111, (int)((uint64_t)pct * (bw - 2) / 100), 14, D_GREEN);
  }
}

// Stage 2 of recovery self-migration. Runs at boot when a migration is pending.
// Power-loss safe: factory boot is only re-enabled (otadata erased) AFTER the
// factory copy is CRC-verified, so an interrupted copy simply re-runs from ota_1.
static void recoveryMigrationStage2() {
  Preferences p; p.begin("recovery", false, "settings");
  uint8_t  pend    = p.getUChar("migPend", 0);
  uint32_t size    = p.getULong("migSize", 0);
  uint32_t expCrc  = p.getULong("migCrc", 0);
  uint8_t  failCnt = p.getUChar("migFail", 0);
  p.end();
  if (!pend) return;

  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* fac = getFactory();
  const esp_partition_t* o1  = getOta1();
  if (!fac || !o1) return;

  // If we're running from factory with a pending flag, the install already
  // completed (or was never needed) — just clear the flag.
  if (running == fac) {
    Preferences q; q.begin("recovery", false, "settings"); q.putUChar("migPend", 0); q.end();
    return;
  }
  // Stage 2 only runs from the freshly-received recovery in ota_1.
  if (running != o1) return;

  // 1) Re-verify the staged image in ota_1 before touching factory.
  drawMig("Verifying image...", -1, D_FG);
  if (size == 0 || size > fac->size || partCrc32(o1, size) != expCrc) {
    drawMig("Image check failed", -1, D_RED);
    // Factory is still intact (untouched). Abandon the migration safely:
    // clear pending and erase otadata so the (good) factory boots next.
    const esp_partition_t* otad = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
    if (otad) esp_partition_erase_range(otad, 0, otad->size);
    Preferences q; q.begin("recovery", false, "settings");
    q.putUChar("migPend", 0); q.end();
    delay(2500); esp_restart();
  }

  // 2) Erase + copy ota_1 → factory, then read-back CRC-verify. 3 in-boot tries.
  bool ok = false;
  for (int attempt = 0; attempt < 3 && !ok; attempt++) {
    drawMig("Installing...", 0, D_FG);
    if (esp_partition_erase_range(fac, 0, ((size_t)size + 0xFFF) & ~(size_t)0xFFF) != ESP_OK)
      continue;
    static uint8_t mbuf[1024];
    size_t off = 0; bool werr = false;
    while (off < size) {
      size_t n = min((size_t)sizeof(mbuf), (size_t)size - off);
      if (esp_partition_read(o1, off, mbuf, n) != ESP_OK)  { werr = true; break; }
      if (esp_partition_write(fac, off, mbuf, n) != ESP_OK) { werr = true; break; }
      off += n;
      if ((off % 65536) < n) drawMig("Installing...", (int)(off * 100 / size), D_FG);
    }
    if (werr) continue;
    drawMig("Verifying install...", 100, D_FG);
    ok = (partCrc32(fac, size) == expCrc);
  }

  if (ok) {
    // Factory verified good. Now (and only now) make it bootable: erasing otadata
    // makes the bootloader select factory. Clear the pending flag last.
    const esp_partition_t* otad = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
    if (otad) esp_partition_erase_range(otad, 0, otad->size);
    Preferences q; q.begin("recovery", false, "settings");
    q.putUChar("migPend", 0); q.putInt("bootcount", 0); q.end();
    drawMig("Recovery updated!", 100, D_GREEN);
    delay(1200); esp_restart();
  }

  // Copy failed this boot. otadata still points at ota_1 (set in stage 1), so a
  // power cycle safely re-runs stage 2 — never boots the half-written factory.
  failCnt++;
  Preferences q; q.begin("recovery", false, "settings"); q.putUChar("migFail", failCnt); q.end();
  if (failCnt < 5) {
    drawMig("Install retry...", -1, D_YELLOW);
    delay(1500); esp_restart();
  }
  // Persistent failure (very rare): stay on the intact new recovery in ota_1 and
  // refuse to hand off to main (factory is damaged). User repairs via USB bootloader.
  gFacBad = true;
  Preferences r; r.begin("recovery", false, "settings");
  r.putUChar("migPend", 0); r.end();
  esp_ota_set_boot_partition(o1);   // keep booting this (good) recovery
  drawMig("Repair failed - use USB", -1, D_RED);
  delay(3500);
}

static bool splashAndDecide() {
  // If factory is known-damaged, never auto-forward to the main app.
  if (gFacBad) return true;

  tft.fillScreen(D_BG);

  tft.setTextColor(D_CYAN, D_BG);
  tft.setTextFont(4);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("KQ4TXO", W / 2, 52);

  tft.setTextColor(D_PURPLE, D_BG);
  tft.setTextFont(2);
  tft.drawString("ATS-Mini Recovery", W / 2, 88);

  tft.setTextColor(D_YELLOW, D_BG);
  tft.setTextFont(1);
  tft.drawString("Press encoder to enter recovery", W / 2, 112);

  Preferences bc;
  bc.begin("recovery", false, "settings");
  int bootCount = bc.getInt("bootcount", 0);
  bc.end();
  if (bootCount >= 3) {
    tft.setTextColor(D_RED, D_BG);
    tft.setTextFont(1);
    tft.setTextDatum(MC_DATUM);
    char buf[40];
    snprintf(buf, sizeof(buf), "Boot-loop x%d detected!", bootCount);
    tft.drawString(buf, W / 2, 128);
    Preferences bc2;
    bc2.begin("recovery", false, "settings");
    bc2.putInt("bootcount", 0);
    bc2.end();
    return true;
  }

  bool held = (digitalRead(PIN_BTN) == LOW);
  uint32_t start = millis();
  while (!held && millis() - start < 2000) {
    uint32_t elapsed = millis() - start;
    int fill = (int)((uint64_t)elapsed * (W - 24) / 2000);
    tft.fillRect(12, 145, fill, 10, D_COMMENT);
    tft.drawRect(12, 145, W - 24, 10, D_LINE);
    if (digitalRead(PIN_BTN) == LOW) { held = true; break; }
    delay(20);
  }

  if (!held) {
    Preferences bc3;
    bc3.begin("recovery", false, "settings");
    bc3.putInt("bootcount", bootCount + 1);
    bc3.end();

    const esp_partition_t* p = getOta0();
    if (p && esp_ota_set_boot_partition(p) == ESP_OK) {
      tft.setTextColor(D_GREEN, D_BG);
      tft.setTextFont(1);
      tft.drawString("Booting...", W / 2, 162);
      delay(200);
      esp_restart();
    }
  }

  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Screen draw functions
// ─────────────────────────────────────────────────────────────────────────────

static void drawMain() {
  static const char* labels[] = { "Boot Firmware", "Erase Data", "Network", "Reboot" };
  renderMenu("Recovery", labels, nullptr, 4);
}

static void drawNetwork() {
  static const char* labels[] = { "WiFi Scan", "OTA Update", "Adhoc Network", "Web Server", "Back" };
  const char* values[] = { nullptr, nullptr, adhocOn ? "ON" : "OFF", webOn ? "ON" : "OFF", nullptr };
  renderMenu("Network", labels, values, 5);
}

static void drawEraseConfirm() {
  if (fullDraw) {
    tft.fillScreen(D_BG);
    drawHeader("Erase Data?");
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(D_FG, D_BG);
    tft.drawString("Erase all saved settings?", W / 2, 60);
    tft.setTextColor(D_COMMENT, D_BG);
    tft.setTextFont(1);
    tft.drawString("Band presets, themes, WiFi - all cleared", W / 2, 82);
    drawFooter();
  }
  // sel 0 = No (left/green), sel 1 = Yes (right/red)
  uint16_t noBg  = (sel == 0) ? D_GREEN : D_LINE;
  uint16_t yesBg = (sel == 1) ? D_RED   : D_LINE;
  tft.fillRoundRect(40,  108, 90, 30, 5, noBg);
  tft.setTextColor((sel == 0) ? D_BG : D_FG, noBg);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("No",  85,  123);
  tft.fillRoundRect(190, 108, 90, 30, 5, yesBg);
  tft.setTextColor(D_FG, yesBg);
  tft.drawString("Yes", 235, 123);
}

static const char* scanLabel(int i, char* buf) {
  if (i >= scannedCnt) { strcpy(buf, "Back"); return buf; }
  strncpy(buf, scanned[i].ssid.c_str(), 38); buf[38] = 0;
  return buf;
}
static const char* scanValue(int i, char* buf) {
  if (i >= scannedCnt) { buf[0] = 0; return buf; }
  int bars = scanned[i].rssi > -55 ? 4 : scanned[i].rssi > -67 ? 3 :
             scanned[i].rssi > -78 ? 2 : 1;
  snprintf(buf, 16, "%ddBm  %d/4", (int)scanned[i].rssi, bars);
  return buf;
}

static void drawWifiScan() {
  if (scanning) {
    if (fullDraw) {
      tft.fillScreen(D_BG); drawHeader("Select Network"); drawFooter();
      tft.setTextColor(D_CYAN, D_BG);
      tft.setTextFont(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Scanning...", W / 2, BODY_Y + BODY_H / 2);
    }
    return;
  }
  if (scannedCnt == 0) {
    // Only the [Back] item
    static const char* one[] = { "Back" };
    renderMenu("Select Network", one, nullptr, 1);
    return;
  }
  renderList("Select Network", scannedCnt + 1, scanLabel, scanValue);
}

// Keyboard: 4 rows, encoder left/right = col, long press = next row
static void drawKbKey(int row, int col) {
  static const int KEY_H = 23, GAP = 2;
  const int KY = 56;

  bool hi = (row == kbRow && col == kbCol);
  char c = kbChar(row, col);

  int x, y = KY + row * (KEY_H + GAP);
  int kw = 30;

  int n = kbRowLen(row);
  if (row == 0) {
    x = col * 32;
  } else if (row == 1) {
    x = 16 + col * 32;
  } else if (row == 2) {
    if (col < n - 1) { x = 2 + col * 32; }
    else             { x = 2 + (n - 1) * 32; kw = 320 - x - 2; }
  } else {
    static const int r3x[] = { 2, 66, 132, 248 };
    static const int r3w[] = { 62, 64, 114, 70 };
    x = r3x[col]; kw = r3w[col];
  }

  uint16_t bg = hi ? D_LINE : D_BG;
  uint16_t fg = hi ? D_CYAN : D_FG;
  uint16_t bd = hi ? D_PURPLE : D_COMMENT;

  tft.fillRoundRect(x, y, kw, KEY_H, 3, bg);
  tft.drawRoundRect(x, y, kw, KEY_H, 3, bd);
  tft.setTextColor(fg, bg);
  tft.setTextFont(1);
  tft.setTextDatum(MC_DATUM);

  char label[6] = {};
  switch ((uint8_t)c) {
    case 0x7f: strcpy(label, "DEL");  break;
    case '\r': strcpy(label, "OK");   break;
    case '\x01':
      strcpy(label, kbNums ? "ABC" : (kbShift ? "SH*" : "SHF")); break;
    case '\x02':
      strcpy(label, kbNums ? "SYM" : "123"); break;
    case ' ':  strcpy(label, "SPC"); break;
    default:   label[0] = c; break;
  }
  tft.drawString(label, x + kw / 2, y + KEY_H / 2);
}

// Redraw just the SSID + masked-password band at the top.
static void drawKbBand() {
  tft.fillRect(0, HDR_H, W, 32, D_LINE);
  tft.setTextFont(1);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(D_COMMENT, D_LINE);
  tft.drawString(kbSSID.c_str(), 5, HDR_H + 9);

  String dots;
  for (size_t i = 0; i < kbPass.length(); i++) dots += '*';
  dots += '_';
  tft.setTextColor(D_FG, D_LINE);
  tft.drawString(("Pass: " + dots).c_str(), 5, HDR_H + 22);
}

static void drawKb() {
  if (fullDraw) {
    tft.fillScreen(D_BG);
    drawHeader("WiFi Password");
    drawKbBand();
    for (int r = 0; r < 4; r++)
      for (int c = 0; c < kbRowLen(r); c++)
        drawKbKey(r, c);
  } else {
    // Always refresh the band (password may have changed); only repaint the
    // two keys whose highlight changed to avoid flicker.
    drawKbBand();
    if (lastKbR >= 0 && (lastKbR != kbRow || lastKbC != kbCol) &&
        lastKbC < kbRowLen(lastKbR))
      drawKbKey(lastKbR, lastKbC);
    drawKbKey(kbRow, kbCol);
  }
  lastKbR = kbRow; lastKbC = kbCol;
}

static const char* otaLabel(int i, char* buf) {
  if (i >= relCnt) { strcpy(buf, "Back"); return buf; }
  strncpy(buf, releases[i].tag, 38); buf[38] = 0;
  return buf;
}

static void drawOtaList() {
  if (!relLoaded) {
    if (fullDraw) {
      tft.fillScreen(D_BG); drawHeader("OTA: Pick Release"); drawFooter();
      tft.setTextColor(relErr[0] ? D_RED : D_CYAN, D_BG);
      tft.setTextFont(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(relErr[0] ? relErr : "Loading...", W / 2, BODY_Y + BODY_H / 2);
    }
    return;
  }
  renderList("OTA: Pick Release", relCnt + 1, otaLabel, nullptr);
}

static void drawWebSrv() {
  static const char* labels[] = { "Web Server", "Back" };
  const char* values[] = { webOn ? "ON" : "OFF", nullptr };
  if (fullDraw) {
    tft.fillScreen(D_BG);
    drawHeader("Web Server");
    drawFooter();
    // Two selectable rows at top
    drawItem(0, labels[0], values[0], sel == 0);
    drawItem(1, labels[1], values[1], sel == 1);
    // Status + URL below the items
    int sy = BODY_Y + 2 * ITEM_H + 8;
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    if (webOn) {
      tft.setTextColor(D_GREEN, D_BG);
      tft.drawString("Running", 10, sy);
      String ip = wifiOK ? WiFi.localIP().toString()
                         : adhocOn ? WiFi.softAPIP().toString() : String("0.0.0.0");
      tft.setTextColor(D_CYAN, D_BG);
      tft.setTextFont(1);
      tft.drawString(("http://" + ip).c_str(), 10, sy + 20);
      tft.setTextColor(D_COMMENT, D_BG);
      if (adhocOn && !wifiOK)
        tft.drawString("Join 'ATS-Recovery' (open) then open URL", 10, sy + 34);
      else if (wifiOK)
        tft.drawString("Connect to same WiFi network", 10, sy + 34);
      else
        tft.drawString("Start Adhoc or connect WiFi first", 10, sy + 34);
    } else {
      tft.setTextColor(D_RED, D_BG);
      tft.drawString("Stopped", 10, sy);
      tft.setTextColor(D_COMMENT, D_BG);
      tft.setTextFont(1);
      tft.drawString("Turn ON to flash / manage over WiFi", 10, sy + 20);
    }
  } else {
    if (lastSel >= 0 && lastSel < 2 && lastSel != sel)
      drawItem(lastSel, labels[lastSel], values[lastSel], false);
    drawItem(sel, labels[sel], values[sel], true);
    if (footerDirty) drawFooter();
  }
}

static void redraw() {
  switch (cur) {
    case SCR_MAIN:          drawMain();         break;
    case SCR_ERASE_CONFIRM: drawEraseConfirm(); break;
    case SCR_NETWORK:       drawNetwork();      break;
    case SCR_WIFI_SCAN:     drawWifiScan();     break;
    case SCR_WIFI_KB:       drawKb();           break;
    case SCR_OTA_LIST:      drawOtaList();      break;
    case SCR_WEBSERVER:     drawWebSrv();       break;
    default: break;
  }
  lastSel = sel;
  fullDraw = false;
  footerDirty = false;
  dirty = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi helpers
// ─────────────────────────────────────────────────────────────────────────────

// Start STA-only connection to known networks in the background (non-blocking).
// No AP is started by default; user can start adhoc from Network menu.
static void netInit() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                   // disable modem sleep — big latency/throughput win
  WiFi.setTxPower(WIFI_POWER_19_5dBm);    // max TX power for range/stability
  staTrying = true; staTryIdx = 0; staTryStart = millis();
  WiFi.begin(KNOWN[0].ssid, KNOWN[0].pass);
  showToast("Connecting to WiFi...", 8000);
}

// Advance the background STA connection. Call every loop; never blocks.
static void netTick() {
  // Expire toast when its timer runs out
  if (toastMsg[0] && millis() >= toastExp) {
    toastMsg[0] = 0; footerDirty = true; dirty = true;
  }
  if (!staTrying) return;
  if (WiFi.status() == WL_CONNECTED) {
    wifiOK = true; staTrying = false;
    char buf[80];
    snprintf(buf, sizeof(buf), "Connected: %s  %s",
             WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    showToast(buf, 5000);
    Serial.printf("{\"wifi\":\"connected\",\"ssid\":\"%s\",\"ip\":\"%s\"}\n",
      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    return;
  }
  if (millis() - staTryStart > 7000) {
    staTryIdx++;
    if (staTryIdx >= KNOWN_CNT) {
      staTrying = false;
      showToast("WiFi: no known networks found", 5000);
      return;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "Trying %s...", KNOWN[staTryIdx].ssid);
    showToast(buf, 8000);
    WiFi.begin(KNOWN[staTryIdx].ssid, KNOWN[staTryIdx].pass);
    staTryStart = millis();
  }
}

// Blocking connect for the password-entry flow (draws a spinner). Keeps adhoc AP up if running.
static bool connectToNetwork(const char* ssid, const char* pass) {
  WiFi.mode(adhocOn ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(ssid, pass);
  uint32_t t = millis();
  int spin = 0;
  while (millis() - t < 12000) {
    wl_status_t s = WiFi.status();
    if (s == WL_CONNECTED) return true;
    if (s == WL_NO_SSID_AVAIL || s == WL_CONNECT_FAILED) break;
    static const char spins[] = "|/-\\";
    char sp[3] = { spins[spin++ % 4], 0 };
    tft.setTextColor(D_PURPLE, D_BG);
    tft.setTextFont(4);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(sp, W / 2, BODY_Y + 78);
    delay(180);
  }
  WiFi.disconnect();
  return false;
}

// Quick TCP-based internet reachability check (uses Google DNS port 53).
static bool hasInternet() {
  if (!wifiOK) return false;
  WiFiClient c;
  bool ok = c.connect(IPAddress(8, 8, 8, 8), 53);
  c.stop();
  return ok;
}

// Start an open (no-password) adhoc AP for local flashing.
static void startAdhoc() {
  WiFi.mode(wifiOK ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(AP_SSID); // open network, no password
  adhocOn = true;
  showToast("Adhoc 'ATS-Recovery' started (open)", 5000);
  footerDirty = true; dirty = true;
}

static void stopAdhoc() {
  WiFi.softAPdisconnect(true);
  adhocOn = false;
  WiFi.mode(wifiOK ? WIFI_STA : WIFI_OFF);
  if (wifiOK) WiFi.reconnect();
  showToast("Adhoc network stopped", 3000);
  footerDirty = true; dirty = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// GitHub releases fetch (streaming JSON parse)
// ─────────────────────────────────────────────────────────────────────────────

static bool fetchReleases(int perPage = 25) {
  relCnt = 0; relLoaded = false; relErr[0] = 0;
  if (!wifiOK) { strcpy(relErr, "No internet (STA not connected)"); return false; }

  char apiUrl[128];
  snprintf(apiUrl, sizeof(apiUrl),
    "https://api.github.com/repos/%s/releases?per_page=%d",
    GITHUB_REPO, perPage);

  WiFiClientSecure client;
  HTTPClient http;
  int code = 0;
  bool ok = false;
  for (int attempt = 1; attempt <= 3 && !ok; attempt++) {
    client.setInsecure();
    client.setHandshakeTimeout(20);
    http.setReuse(false);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(20000);
    if (!http.begin(client, apiUrl)) { delay(500); continue; }
    http.addHeader("User-Agent", "ATS-Mini-Recovery/2");
    http.addHeader("Accept", "application/vnd.github.v3+json");
    code = http.GET();
    if (code == 200) { ok = true; break; }
    http.end();
    delay(500);
  }
  if (!ok) {
    snprintf(relErr, sizeof(relErr), "Fetch failed (HTTP %d)", code);
    return false;
  }

  int contentLen = http.getSize();
  int bufSz = (contentLen > 0 && contentLen < 120000) ? contentLen + 1 : 120 * 1024;
  char* buf = (char*)malloc(bufSz);
  if (!buf) { strcpy(relErr, "Out of memory"); http.end(); return false; }

  WiFiClient* stream = http.getStreamPtr();
  int got = 0;
  uint32_t t0 = millis();
  while (got < bufSz - 1 && http.connected() && millis() - t0 < 25000) {
    size_t avail = stream->available();
    if (!avail) { delay(1); continue; }
    int n = stream->readBytes(buf + got, min((size_t)(bufSz - 1 - got), avail));
    if (n <= 0) break;
    got += n;
    if (contentLen > 0 && got >= contentLen) break;
  }
  http.end();
  buf[got] = 0;

  const char* p = buf;
  while (relCnt < 25) {
    const char* tp = strstr(p, "\"tag_name\":\"");
    if (!tp) break;
    tp += 12;
    const char* te = strchr(tp, '"'); if (!te) break;
    int tlen = min((int)(te - tp), 15);
    memcpy(releases[relCnt].tag, tp, tlen);
    releases[relCnt].tag[tlen] = 0;
    releases[relCnt].url[0]    = 0;

    const char* searchEnd = tp + 12000;
    if (searchEnd > buf + got) searchEnd = buf + got;
    const char* ap = tp;
    while (ap < searchEnd) {
      const char* nm = strstr(ap, "-ospi-ota.bin\"");
      if (!nm || nm >= searchEnd) break;
      const char* up = strstr(nm, "\"browser_download_url\":\"");
      if (!up || up >= searchEnd) { ap = nm + 1; continue; }
      up += 24;
      const char* ue = strchr(up, '"');
      if (!ue || ue >= searchEnd) { ap = nm + 1; continue; }
      int ulen = min((int)(ue - up), 159);
      memcpy(releases[relCnt].url, up, ulen);
      releases[relCnt].url[ulen] = 0;
      break;
    }
    if (releases[relCnt].url[0]) relCnt++;
    p = te + 1;
  }
  free(buf);
  relLoaded = true;
  if (relCnt == 0) strcpy(relErr, "No OTA assets found");
  return relCnt > 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// OTA download from URL (blocking, updates TFT)
// ─────────────────────────────────────────────────────────────────────────────

static bool downloadAndFlash(const char* url, const char* tag = "") {
  strncpy(otaTag, tag, sizeof(otaTag) - 1);
  otaTag[sizeof(otaTag) - 1] = 0;
  cur = SCR_OTA_PROGRESS;
  tft.fillScreen(D_BG);
  drawHeader("Flashing...");
  otaTotal = 0; otaWritten = 0;
  drawOtaFrame("Connecting...");
  drawOtaUpdate(0);

  // Connect with retries — GitHub's CDN redirect + TLS handshake can flake (HTTP -1).
  WiFiClientSecure client;
  HTTPClient http;
  int  code = 0;
  bool started = false;
  for (int attempt = 1; attempt <= 3 && !started; attempt++) {
    if (attempt > 1) {
      char m[24]; snprintf(m, sizeof(m), "Retry %d/3...", attempt);
      drawOtaError(m);
      delay(700);
    }
    client.setInsecure();
    client.setHandshakeTimeout(20);     // seconds — TLS handshake headroom
    http.setReuse(false);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(30000);
    if (!http.begin(client, url)) { continue; }
    http.addHeader("User-Agent", "ATS-Mini-Recovery/2");
    code = http.GET();
    if (code == 200) { started = true; break; }
    http.end();
  }
  if (!started) {
    char err[28]; snprintf(err, sizeof(err), "Failed: HTTP %d", code);
    drawOtaError(err);
    Serial.printf("{\"ota\":\"error\",\"http\":%d}\n", code);
    delay(3000);
    return false;
  }

  int total = http.getSize();
  if (total <= 0) { drawOtaError("No content-length"); http.end(); delay(3000); return false; }
  if (!otaBegin((size_t)total)) { drawOtaError("OTA begin failed"); http.end(); delay(3000); return false; }
  otaTotal = (size_t)total;

  static uint8_t dlBuf[4096];
  WiFiClient* stream = http.getStreamPtr();
  uint32_t lastDraw = 0, lastData = millis();
  uint32_t spkBase = 0; uint32_t spkTime = millis(); uint32_t kbps = 0;

  // Drive by byte count, not http.connected() — the connection can drop after
  // the final bytes are buffered (the old loop stalled near 99%).
  while (otaWritten < otaTotal) {
    size_t av = stream->available();
    if (av) {
      int n = stream->readBytes(dlBuf, min(av, min(sizeof(dlBuf), otaTotal - otaWritten)));
      if (n > 0) {
        if (!otaWrite(dlBuf, (size_t)n)) { drawOtaError("Flash write error"); http.end(); delay(3000); return false; }
        lastData = millis();
      }
    } else {
      // No data right now — bail only if the socket is closed AND drained.
      if (!http.connected() && stream->available() == 0) break;
      if (millis() - lastData > 20000) { drawOtaError("Stalled - timeout"); http.end(); delay(3000); return false; }
      delay(1);
    }
    uint32_t now = millis();
    if (now - spkTime >= 500) {                       // sample speed every 0.5s
      kbps = ((otaWritten - spkBase) / 1024) * 1000 / (now - spkTime);
      spkBase = otaWritten; spkTime = now;
    }
    if (now - lastDraw > 200) { lastDraw = now; drawOtaUpdate(kbps); }
  }
  http.end();
  drawOtaUpdate(kbps);

  if (otaWritten < otaTotal) {
    char e[32]; snprintf(e, sizeof(e), "Incomplete %u/%uKB",
      (unsigned)(otaWritten / 1024), (unsigned)(otaTotal / 1024));
    drawOtaError(e);
    Serial.printf("{\"ota\":\"incomplete\",\"got\":%u,\"need\":%u}\n",
      (unsigned)otaWritten, (unsigned)otaTotal);
    delay(3500);
    return false;
  }

  if (otaFinish()) {
    tft.setTextColor(D_GREEN, D_BG); tft.setTextFont(2); tft.setTextDatum(MC_DATUM);
    tft.drawString("Done! Rebooting...", W / 2, otaBarY + 70);
    Serial.println("{\"ota\":\"complete\",\"ok\":true}");
    delay(1800); esp_restart();
  }
  drawOtaError("Verify failed");
  delay(3000);
  return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Web server
// ─────────────────────────────────────────────────────────────────────────────

static const char HTML_MAIN[] = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ATS-Mini Recovery</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#282a36;color:#f8f8f2;min-height:100vh;padding:16px}
.c{max-width:620px;margin:0 auto}
h1{color:#bd93f9;font-size:1.5rem;margin-bottom:4px}
.sub{color:#6272a4;font-size:.8rem;margin-bottom:14px}
.card{background:#44475a;border-radius:10px;padding:16px;margin-bottom:12px}
.card h2{color:#8be9fd;font-size:.9rem;font-weight:600;margin-bottom:12px}
.chip{display:inline-flex;align-items:center;gap:5px;background:#282a36;border-radius:20px;padding:4px 10px;font-size:.78rem;margin:2px}
.dot{width:8px;height:8px;border-radius:50%}
.g{background:#50fa7b}.r{background:#ff5555}.y{background:#ffb86c}
.tabs{display:flex;gap:2px;margin-bottom:12px}
.tab{flex:1;padding:7px;background:#282a36;border:none;color:#6272a4;border-radius:6px;cursor:pointer;font-size:.82rem}
.tab.on{background:#6272a4;color:#f8f8f2}
.tc{display:none}.tc.on{display:block}
.drop{border:2px dashed #6272a4;border-radius:8px;padding:18px;text-align:center;cursor:pointer;color:#6272a4;font-size:.85rem}
.drop:hover,.drop.ov{border-color:#bd93f9;color:#bd93f9}
.drop input{display:none}
.btn{border:0;border-radius:6px;padding:8px 18px;cursor:pointer;font-size:.85rem;font-weight:500;margin:2px}
.btn:disabled{opacity:.4;cursor:not-allowed}
.bp{background:#bd93f9;color:#282a36}
.bg{background:#50fa7b;color:#282a36}
.br{background:#ff5555;color:#f8f8f2}
.bo{background:#ffb86c;color:#282a36}
.bs{padding:4px 10px;font-size:.75rem}
.rl{max-height:280px;overflow-y:auto;border-radius:6px;margin-top:8px}
.ri{padding:8px 10px;display:flex;align-items:center;justify-content:space-between;border-bottom:1px solid #282a36;background:#282a36;margin-bottom:1px;border-radius:4px}
.ri:hover{background:#44475a}
.rt{font-family:monospace;color:#8be9fd;font-size:.85rem}
.pw{margin-top:10px;display:none}
.pb{height:10px;background:#282a36;border-radius:5px;overflow:hidden}
.pf{height:100%;background:linear-gradient(90deg,#bd93f9,#8be9fd);width:0;transition:width .3s}
.pl{color:#6272a4;font-size:.72rem;margin-top:3px;text-align:center}
.msg{padding:9px;border-radius:6px;font-size:.82rem;margin-top:8px;display:none}
.ok{background:#1a4731;color:#50fa7b;border:1px solid #50fa7b}
.err{background:#4a1a1a;color:#ff5555;border:1px solid #ff5555}
.row{display:flex;gap:8px;flex-wrap:wrap}
footer{text-align:center;color:#44475a;font-size:.72rem;margin-top:16px;padding-bottom:8px}
</style></head><body><div class="c">
<h1>&#128295; ATS-Mini Recovery</h1>
<div class="sub" id="sub">Loading...</div>
<div id="chips"></div>

<div class="card">
<h2>&#128190; Flash Firmware</h2>
<div class="tabs">
  <button class="tab on" onclick="tab(0)">Upload File</button>
  <button class="tab"    onclick="tab(1)">GitHub Releases</button>
</div>
<div class="tc on" id="tc0">
  <div class="drop" id="da" onclick="document.getElementById('fi').click()">
    <div>&#128190; Click or drag <code>*-ospi-ota.bin</code> here</div>
    <input type="file" id="fi" accept=".bin">
  </div>
  <div style="margin-top:10px;text-align:center">
    <button class="btn bp" id="ubtn" onclick="doUp()" disabled>Flash</button>
  </div>
  <div class="pw" id="upw">
    <div class="pb"><div class="pf" id="upf"></div></div>
    <div class="pl" id="upl">0%</div>
  </div>
  <div class="msg" id="umsg"></div>
</div>
<div class="tc" id="tc1">
  <div id="rload" style="color:#6272a4;font-size:.85rem">Loading releases...</div>
  <div class="rl" id="rlist" style="display:none"></div>
  <div class="msg" id="rmsg"></div>
</div>
</div>

<div class="card">
<h2>&#9881;&#65039; System</h2>
<div class="row">
  <button class="btn br" onclick="doErase()">Erase Settings</button>
  <button class="btn bo" onclick="doReboot()">Reboot</button>
</div>
<div class="msg" id="smsg"></div>
</div>
<footer>ATS-Mini Recovery &bull; KQ4TXO</footer>
</div><script>
var tcs=[document.getElementById('tc0'),document.getElementById('tc1')];
var tbs=document.querySelectorAll('.tab');
function tab(i){
  tcs.forEach(function(e,j){e.className='tc'+(j===i?' on':'')});
  tbs.forEach(function(e,j){e.className='tab'+(j===i?' on':'')});
  if(i===1&&!rlDone)loadRels();
}
function showMsg(id,t,x){var e=document.getElementById(id);e.className='msg '+t;e.textContent=x;e.style.display='block';}

fetch('/status').then(r=>r.json()).then(d=>{
  document.getElementById('sub').textContent='IP: '+d.ip+'   Mode: Recovery';
  var h='';
  if(d.wifi==='ok')h+='<span class="chip"><span class="dot g"></span>WiFi: '+d.ssid+'</span>';
  else if(d.wifi==='ap')h+='<span class="chip"><span class="dot y"></span>AP Mode</span>';
  else h+='<span class="chip"><span class="dot r"></span>No WiFi</span>';
  h+='<span class="chip"><span class="dot g"></span>Recovery</span>';
  document.getElementById('chips').innerHTML=h;
}).catch(function(){});

var selFile=null;
document.getElementById('fi').addEventListener('change',function(e){
  selFile=e.target.files[0];
  document.getElementById('ubtn').disabled=!selFile;
  if(selFile)document.getElementById('da').innerHTML='<div style="color:#50fa7b">&#10003; '+selFile.name+' ('+Math.round(selFile.size/1024)+' KB)</div>';
});
var da=document.getElementById('da');
da.addEventListener('dragover',function(e){e.preventDefault();da.classList.add('ov');});
da.addEventListener('dragleave',function(){da.classList.remove('ov');});
da.addEventListener('drop',function(e){
  e.preventDefault();da.classList.remove('ov');
  selFile=e.dataTransfer.files[0];
  document.getElementById('ubtn').disabled=!selFile;
  if(selFile)da.innerHTML='<div style="color:#50fa7b">&#10003; '+selFile.name+' ('+Math.round(selFile.size/1024)+' KB)</div>';
});
function doUp(){
  if(!selFile)return;
  var fd=new FormData();fd.append('fw',selFile);
  document.getElementById('upw').style.display='block';
  document.getElementById('ubtn').disabled=true;
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/update');
  xhr.upload.onprogress=function(e){
    if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);
    document.getElementById('upf').style.width=p+'%';
    document.getElementById('upl').textContent=p+'%';}
  };
  xhr.onload=function(){
    document.getElementById('upf').style.width='100%';
    document.getElementById('upl').textContent='Done';
    if(xhr.status===200)showMsg('umsg','ok','Flash complete! Device rebooting...');
    else showMsg('umsg','err','Failed: '+xhr.responseText);
  };
  xhr.onerror=function(){showMsg('umsg','err','Connection error');};
  xhr.send(fd);
}

var rlDone=false;
function loadRels(){
  fetch('/api/releases').then(r=>r.json()).then(function(data){
    document.getElementById('rload').style.display='none';
    if(!data.length){document.getElementById('rload').textContent='No releases found';document.getElementById('rload').style.display='block';return;}
    var el=document.getElementById('rlist');el.innerHTML='';
    data.forEach(function(r){
      var d=document.createElement('div');d.className='ri';
      d.innerHTML='<span class="rt">'+r.tag+'</span><button class="btn bp bs" onclick="flashUrl(\''+r.url+'\',\''+r.tag+'\')">Flash</button>';
      el.appendChild(d);
    });
    el.style.display='block';rlDone=true;
  }).catch(function(e){showMsg('rmsg','err','Load failed: '+e);});
}
function flashUrl(url,tag){
  if(!confirm('Flash '+tag+'?\nDevice will reboot when done.'))return;
  showMsg('rmsg','ok','Flashing '+tag+'... watch device display.');
  fetch('/flash-url',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'url='+encodeURIComponent(url)})
    .catch(function(){});
}
function doErase(){
  if(!confirm('Erase ALL saved settings?\nThis cannot be undone.'))return;
  fetch('/erase',{method:'POST'}).then(function(){showMsg('smsg','ok','Erased. Rebooting...');}).catch(function(e){showMsg('smsg','err',''+e);});
}
function doReboot(){
  if(!confirm('Reboot now?'))return;
  showMsg('smsg','ok','Rebooting...');
  fetch('/reboot',{method:'POST'}).catch(function(){});
}
</script></body></html>)HTML";

static void webHandleRoot() { server.send(200, "text/html", HTML_MAIN); }

static void webHandleStatus() {
  String ip = wifiOK ? WiFi.localIP().toString() :
              adhocOn ? WiFi.softAPIP().toString() : "0.0.0.0";
  String ssid = wifiOK ? WiFi.SSID() : "";
  String wifi = wifiOK ? "ok" : adhocOn ? "ap" : "none";
  String json = "{\"wifi\":\"" + wifi + "\",\"ssid\":\"" + ssid +
                "\",\"ip\":\"" + ip + "\"}";
  server.send(200, "application/json", json);
}

static void webHandleReleases() {
  if (!relLoaded) fetchReleases(50);
  String json = "[";
  for (int i = 0; i < relCnt; i++) {
    if (i) json += ",";
    json += "{\"tag\":\"";
    json += releases[i].tag;
    json += "\",\"url\":\"";
    json += releases[i].url;
    json += "\"}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

static void webHandleUploadBody() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaBegin();
  } else if (up.status == UPLOAD_FILE_WRITE) {
    otaWrite(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (otaOn) otaFinish();
  }
}

static void webHandleUploadEnd() {
  if (!otaOn && otaWritten > 0) {
    server.send(200, "text/plain", "OK");
    delay(1500); esp_restart();
  } else {
    server.send(500, "text/plain", "Flash failed");
  }
}

static void webHandleFlashUrl() {
  if (!server.hasArg("url")) { server.send(400, "text/plain", "Missing url"); return; }
  String url = server.arg("url");
  server.send(200, "text/plain", "Starting download - watch device display");
  server.client().stop();
  delay(100);
  downloadAndFlash(url.c_str());
}

static void webHandleErase() {
  nvs_flash_erase();
  nvs_flash_init();
  server.send(200, "text/plain", "Erased");
  delay(1500); esp_restart();
}

static void webHandleReboot() {
  server.send(200, "text/plain", "Rebooting");
  delay(500); esp_restart();
}

static void startWebServer() {
  server.on("/",            HTTP_GET,  webHandleRoot);
  server.on("/status",      HTTP_GET,  webHandleStatus);
  server.on("/api/releases",HTTP_GET,  webHandleReleases);
  server.on("/update",      HTTP_POST, webHandleUploadEnd, webHandleUploadBody);
  server.on("/flash-url",   HTTP_POST, webHandleFlashUrl);
  server.on("/erase",       HTTP_POST, webHandleErase);
  server.on("/reboot",      HTTP_POST, webHandleReboot);
  server.begin();
  webOn = true;
}

static void stopWebServer() {
  server.stop();
  webOn = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Serial JSON (Android OTA protocol)
// ─────────────────────────────────────────────────────────────────────────────

static void handleSerialCmd(const char* line) {
  const char* cv = strstr(line, "\"cmd\":");
  if (!cv) return;
  const char* v = strchr(cv, ':'); if (!v) return;
  v++; while (*v == ' ' || *v == '"') v++;

  if (!strncmp(v, "ping", 4)) {
    // Liveness probe — lets the app confirm two-way USB comms before an OTA.
    Serial.println("{\"t\":\"pong\"}");
  } else if (!strncmp(v, "ota_begin", 9)) {
    const char* sk = strstr(line, "\"size\":");
    size_t sz = sk ? (size_t)atol(sk + 7) : 0;
    if (!sz) { Serial.println("{\"ok\":false,\"err\":\"no size\"}"); return; }
    if (!otaBegin(sz)) { Serial.println("{\"ok\":false,\"err\":\"begin\"}"); return; }
    sst = SS_OTA; sOtaExp = sz;
    Serial.printf("{\"ok\":true,\"size\":%u}\n", (unsigned)sz);
  } else if (!strncmp(v, "rec_begin", 9)) {
    // Stage 1: receive a new recovery image into ota_1. The actual factory
    // install (stage 2) happens after reboot, running the new recovery from
    // ota_1 — we can't rewrite factory while executing from it.
    const char* sk = strstr(line, "\"size\":");
    size_t sz = sk ? (size_t)atol(sk + 7) : 0;
    const char* ck = strstr(line, "\"crc\":");
    uint32_t crc = ck ? (uint32_t)strtoul(ck + 6, nullptr, 10) : 0;
    recPart = getOta1();
    const esp_partition_t* fac = getFactory();
    if (!sz || !recPart || !fac || sz > recPart->size || sz > fac->size) {
      Serial.println("{\"ok\":false,\"err\":\"recsize\"}"); return;
    }
    if (esp_ota_begin(recPart, sz, &recH) != ESP_OK) {
      Serial.println("{\"ok\":false,\"err\":\"begin\"}"); return;
    }
    recExp = sz; recWritten = 0; recCrcRun = 0xFFFFFFFFu; recCrcExp = crc; recOn = true;
    sst = SS_REC;
    Serial.printf("{\"ok\":true,\"size\":%u,\"rec\":true}\n", (unsigned)sz);
  } else if (!strncmp(v, "ota_abort", 9)) {
    if (recOn) { esp_ota_abort(recH); recOn = false; }
    otaAbortAll(); sst = SS_IDLE; Serial.println("{\"ok\":true}");
  } else if (!strncmp(v, "status", 6)) {
    Serial.printf("{\"mode\":\"recovery\",\"wifi\":\"%s\",\"ip\":\"%s\"}\n",
      wifiOK ? "connected" : adhocOn ? "ap" : "none",
      wifiOK ? WiFi.localIP().toString().c_str()
             : adhocOn ? WiFi.softAPIP().toString().c_str() : "0.0.0.0");
  } else if (!strncmp(v, "reboot", 6)) {
    Serial.println("{\"ok\":true}"); delay(300); esp_restart();
  }
}

static void handleSerial() {
  if (sst == SS_OTA) {
    static uint8_t buf[1024];
    while (Serial.available() && otaWritten < sOtaExp) {
      size_t want = min((size_t)Serial.available(), min(sizeof(buf), sOtaExp - otaWritten));
      int n = Serial.readBytes(buf, want); if (n <= 0) break;
      if (!otaWrite(buf, n)) { sst = SS_IDLE; Serial.println("{\"ok\":false,\"err\":\"write\"}"); return; }
      if ((otaWritten % 32768) < (size_t)n)
        Serial.printf("{\"progress\":%u,\"total\":%u}\n", (unsigned)otaWritten, (unsigned)sOtaExp);
    }
    if (otaWritten >= sOtaExp) {
      bool ok = otaFinish(); sst = SS_IDLE;
      Serial.printf("{\"ok\":%s,\"bytes\":%u}\n", ok ? "true":"false", (unsigned)otaWritten);
      if (ok) { delay(1000); esp_restart(); }
    }
  } else if (sst == SS_REC) {
    static uint8_t rbuf[1024];
    while (Serial.available() && recWritten < recExp) {
      size_t want = min((size_t)Serial.available(), min(sizeof(rbuf), recExp - recWritten));
      int n = Serial.readBytes(rbuf, want); if (n <= 0) break;
      if (!recOn || esp_ota_write(recH, rbuf, n) != ESP_OK) {
        if (recOn) { esp_ota_abort(recH); recOn = false; }
        sst = SS_IDLE; Serial.println("{\"ok\":false,\"err\":\"write\"}"); return;
      }
      recCrcRun = crc32Run(recCrcRun, rbuf, n);
      recWritten += n;
      if ((recWritten % 32768) < (size_t)n)
        Serial.printf("{\"progress\":%u,\"total\":%u}\n", (unsigned)recWritten, (unsigned)recExp);
    }
    if (recWritten >= recExp) {
      sst = SS_IDLE; recOn = false;
      uint32_t crc = recCrcRun ^ 0xFFFFFFFFu;
      bool crcOk = (recCrcExp == 0 || crc == recCrcExp);
      bool endOk = (esp_ota_end(recH) == ESP_OK);
      if (!crcOk || !endOk) {
        Serial.printf("{\"ok\":false,\"err\":\"%s\"}\n", !crcOk ? "crc" : "verify");
        return;   // factory untouched; nothing to roll back
      }
      // Record the pending migration and boot ota_1 so stage 2 can install it.
      Preferences p; p.begin("recovery", false, "settings");
      p.putUChar("migPend", 1);
      p.putULong("migSize", (uint32_t)recExp);
      p.putULong("migCrc",  crc);
      p.putUChar("migFail", 0);
      p.end();
      if (esp_ota_set_boot_partition(recPart) != ESP_OK) {
        Serial.println("{\"ok\":false,\"err\":\"setboot\"}"); return;
      }
      Serial.printf("{\"ok\":true,\"bytes\":%u,\"rec\":true,\"stage\":1}\n", (unsigned)recWritten);
      delay(800); esp_restart();
    }
  } else {
    static char lbuf[256]; static int llen = 0;
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\n' || c == '\r') {
        if (llen > 0) { lbuf[llen] = 0; handleSerialCmd(lbuf); llen = 0; }
      } else if (llen < (int)sizeof(lbuf) - 1) { lbuf[llen++] = c; }
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Input / state transitions
// ─────────────────────────────────────────────────────────────────────────────

static void goTo(Screen s, int initSel = 0) {
  cur = s; sel = initSel;
  dirty = true; fullDraw = true; lastSel = -1; lastStart = -1;
}

static void eraseSettings() {
  nvs_flash_erase();
  nvs_flash_init();
}

static void bootMain() {
  // Refuse to hand off to main while the factory partition is damaged — main
  // erases otadata on boot, which would leave the bootloader trying a broken
  // factory next cold boot (brick). Repair recovery via USB first.
  if (gFacBad) {
    tft.fillScreen(D_BG);
    tft.setTextColor(D_RED, D_BG); tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Recovery damaged", W / 2, H / 2 - 12);
    tft.drawString("Flash via USB bootloader", W / 2, H / 2 + 12);
    delay(3000);
    return;
  }
  const esp_partition_t* p = getOta0();
  if (p && esp_ota_set_boot_partition(p) == ESP_OK) {
    tft.fillScreen(D_BG);
    tft.setTextColor(D_GREEN, D_BG); tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Booting main firmware...", W / 2, H / 2);
    delay(300); esp_restart();
  }
  dirty = true;
}

static void startScan() {
  scannedCnt = 0; scanning = true;
  WiFi.scanNetworks(true, false);
  needFull();
}


static void processInput() {
  int enc = encPop();
  int btn = btnEvent();

  // Global 2-second long press → escape to main menu from any screen
  if (btn == 3) { goTo(SCR_MAIN, 0); return; }

  // WiFi scan poll (background) — dedupe SSIDs (mesh networks repeat)
  if (scanning && cur == SCR_WIFI_SCAN) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      scanning = false;
      scannedCnt = 0;
      for (int i = 0; i < n && scannedCnt < 20; i++) {
        String s = WiFi.SSID(i);
        if (s.length() == 0) continue;            // skip hidden
        int found = -1;
        for (int j = 0; j < scannedCnt; j++)
          if (scanned[j].ssid == s) { found = j; break; }
        if (found >= 0) {
          if (WiFi.RSSI(i) > scanned[found].rssi) scanned[found].rssi = WiFi.RSSI(i);
        } else {
          scanned[scannedCnt].ssid = s;
          scanned[scannedCnt].rssi = WiFi.RSSI(i);
          scannedCnt++;
        }
      }
      WiFi.scanDelete();
      sel = 0; needFull();
    }
    if (scanning) return;
  }

  switch (cur) {
    // ── Main menu ─────────────────────────────────────────────────────────────
    case SCR_MAIN:
      if (enc) { sel = (sel + enc + 4) % 4; dirty = true; }
      if (btn == 1) {
        switch (sel) {
          case 0: bootMain(); break;
          case 1: goTo(SCR_ERASE_CONFIRM); break;
          case 2: goTo(SCR_NETWORK); break;
          case 3: esp_restart(); break;
        }
      }
      break;

    // ── Erase confirm ──────────────────────────────────────────────────────────
    case SCR_ERASE_CONFIRM:
      if (enc) { sel = (sel + enc + 2) % 2; dirty = true; }
      if (btn == 1) {
        if (sel == 1) { eraseSettings(); goTo(SCR_MAIN); }
        else goTo(SCR_MAIN);
      }
      if (btn == 2) goTo(SCR_MAIN);
      break;

    // ── Network menu ──────────────────────────────────────────────────────────
    case SCR_NETWORK:
      if (enc) { sel = (sel + enc + 5) % 5; dirty = true; }
      if (btn == 1) {
        switch (sel) {
          case 0: goTo(SCR_WIFI_SCAN); startScan(); break;
          case 1: {
            if (!wifiOK) {
              showToast("Need WiFi (not adhoc) for OTA", 4000);
              break;
            }
            // Show checking message, then verify internet
            showToast("Checking internet...", 3000);
            redraw();
            if (!hasInternet()) {
              showToast("No internet - check router/DNS", 5000);
              break;
            }
            goTo(SCR_OTA_LIST);
            redraw();
            fetchReleases(25);
            needFull();
            break;
          }
          case 2: if (adhocOn) stopAdhoc(); else startAdhoc(); needFull(); break;
          case 3: goTo(SCR_WEBSERVER); break;
          case 4: goTo(SCR_MAIN); break;
        }
      }
      if (btn == 2) goTo(SCR_MAIN);
      break;

    // ── WiFi scan list ────────────────────────────────────────────────────────
    case SCR_WIFI_SCAN: {
      int total = scannedCnt + 1; // +[Back]
      if (enc) { sel = (sel + enc + total) % total; dirty = true; }
      if (btn == 1) {
        if (sel < scannedCnt) {
          kbSSID = scanned[sel].ssid;
          kbPass = "";
          kbRow = 0; kbCol = 0;
          kbShift = false; kbNums = false;
          goTo(SCR_WIFI_KB);
        } else {
          goTo(SCR_NETWORK, 0);
        }
      }
      if (btn == 2) goTo(SCR_NETWORK, 0);
      break;
    }

    // ── WiFi password keyboard ────────────────────────────────────────────────
    case SCR_WIFI_KB:
      // Encoder walks the whole keyboard linearly (wraps across rows).
      if (enc) { kbStep(enc); dirty = true; }
      if (btn == 1) {
        char c = kbChar(kbRow, kbCol);
        bool layoutChanged = false;
        switch ((uint8_t)c) {
          case 0x7f:
            if (kbPass.length() > 0) kbPass.remove(kbPass.length() - 1);
            break;
          case '\r': { // OK — connect
            tft.fillScreen(D_BG);
            drawHeader("Connecting...");
            tft.setTextFont(2);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(D_FG, D_BG);
            tft.drawString("Connecting to:", W / 2, BODY_Y + 22);
            tft.setTextColor(D_CYAN, D_BG);
            tft.drawString(kbSSID.c_str(), W / 2, BODY_Y + 48);
            if (connectToNetwork(kbSSID.c_str(), kbPass.c_str())) {
              wifiOK = true; staTrying = false;
              tft.setTextColor(D_GREEN, D_BG); tft.setTextFont(2);
              tft.setTextDatum(MC_DATUM);
              tft.drawString("Connected!", W / 2, BODY_Y + 90);
              delay(1200);
              goTo(SCR_NETWORK, 0);
            } else {
              tft.setTextColor(D_RED, D_BG); tft.setTextFont(2);
              tft.setTextDatum(MC_DATUM);
              tft.drawString("Failed - check password", W / 2, BODY_Y + 90);
              delay(2000);
              goTo(SCR_WIFI_KB);
            }
            return;
          }
          case '\x01':
            if (kbNums) { kbNums = false; kbShift = false; }
            else kbShift = !kbShift;
            layoutChanged = true;     // all letter cases change
            break;
          case '\x02':
            kbNums = !kbNums; kbShift = false;
            kbCol = min(kbCol, kbRowLen(kbRow) - 1);
            layoutChanged = true;     // whole layout changes
            break;
          default:
            if (kbPass.length() < 63) kbPass += c;
            if (kbShift) { kbShift = false; layoutChanged = true; } // case flips back
            break;
        }
        if (layoutChanged) needFull(); else dirty = true;
      }
      break;

    // ── OTA release list ──────────────────────────────────────────────────────
    case SCR_OTA_LIST: {
      if (!relLoaded) break;
      int total = relCnt + 1;
      if (enc) { sel = (sel + enc + total) % total; dirty = true; }
      if (btn == 1) {
        if (sel < relCnt) {
          downloadAndFlash(releases[sel].url, releases[sel].tag);
          goTo(SCR_OTA_LIST);
        } else {
          goTo(SCR_NETWORK, 1);
        }
      }
      if (btn == 2) goTo(SCR_NETWORK, 1);
      break;
    }

    // ── Web server (2-item menu: toggle / back) ────────────────────────────────
    case SCR_WEBSERVER:
      if (enc) { sel = (sel + enc + 2) % 2; dirty = true; }
      if (btn == 1) {
        if (sel == 0) { if (webOn) stopWebServer(); else startWebServer(); needFull(); }
        else          { goTo(SCR_NETWORK, 2); }
      }
      if (btn == 2) goTo(SCR_NETWORK, 2);
      break;

    default: break;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// setup / loop
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  // Fix Core 3.3.10 IPC1 crash: install ISR service before any attachInterrupt
  gpio_install_isr_service(0);

  Serial.begin(115200);
  Serial.println("{\"mode\":\"recovery\",\"fw\":2}");

  pinMode(PIN_PWR, OUTPUT); digitalWrite(PIN_PWR, HIGH); delay(50);

  pinMode(PIN_BTN,   INPUT_PULLUP);
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  encALast = digitalRead(PIN_ENC_A);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encISR, CHANGE);

  ledcAttach(PIN_LCD_BL, 16000, 8);
  ledcWrite(PIN_LCD_BL, 210);

  tft.init();
  tft.setRotation(3);
  uint8_t did3 = tft.readcommand8(ST7789_RDDID, 3);
  if (did3 == 0x93) {
    tft.invertDisplay(0);
    tft.writecommand(TFT_MADCTL);
    tft.writedata(TFT_MAD_MV | TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_BGR);
  } else if (did3 == 0x85) {
    tft.writecommand(0x26); tft.writedata(8);
    tft.writecommand(0x55); tft.writedata(0xB1);
  }
  tft.fillScreen(D_BG);

  // Install a pending recovery update (stage 2 of self-migration) before anything
  // else — this reboots when done, so it never falls through to normal boot.
  recoveryMigrationStage2();

  // Splash + boot decision (boots main firmware unless encoder held / boot-loop)
  splashAndDecide();

  // Bring networking up in the background — AP is instant, STA connects async.
  // No blocking "Trying..." screen: the menu shows immediately.
  netInit();
  startWebServer();

  goTo(SCR_MAIN, 0);
}

void loop() {
  netTick();
  processInput();
  if (dirty) redraw();
  if (webOn) server.handleClient();
  handleSerial();
  delay(5);
}
