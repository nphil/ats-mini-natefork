/*
 * ATS-Mini Recovery Firmware — Dracula UI, encoder navigation
 *
 * Boot trigger: hold encoder button during KQ4TXO splash → recovery UI.
 * Otherwise auto-boots main firmware after 2s. Boot-loop detection at 3 fails.
 *
 * Main menu: Boot / Erase Data / Network / Reboot
 * Network:   WiFi Scan → keyboard → connect | OTA list | Web Server
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
static const int ITEM_H = 27;

// ── Screens ───────────────────────────────────────────────────────────────────
enum Screen : uint8_t {
  SCR_MAIN, SCR_ERASE_CONFIRM, SCR_NETWORK,
  SCR_WIFI_SCAN, SCR_WIFI_KB, SCR_WIFI_CONN,
  SCR_OTA_LIST, SCR_OTA_PROGRESS, SCR_WEBSERVER,
};

// ── Known networks + AP fallback ──────────────────────────────────────────────
struct KnownNet { const char* ssid; const char* pass; };
static const KnownNet KNOWN[] = {
  { "BEAST_ROUTER", "appu1989" },
  { "IoT",          "appu1989" },
};
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
static bool   wifiOK = false;
static bool   apMode = false;
static bool   webOn  = false;

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
static bool btnWas = false, btnUsed = false;
static int btnEvent() {
  bool dn = (digitalRead(PIN_BTN) == LOW);
  if (dn && !btnWas) { btnWas = true; btnDownMs = millis(); btnUsed = false; }
  if (!dn && btnWas) { btnWas = false; return btnUsed ? 0 : 1; }
  if (dn && !btnUsed && millis() - btnDownMs > 450) { btnUsed = true; return 2; }
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
static bool   kbShift = false, kbNums = false;

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

// ── Serial OTA ────────────────────────────────────────────────────────────────
enum SerialSt { SS_IDLE, SS_OTA };
static SerialSt sst = SS_IDLE;
static size_t sOtaExp = 0;

// ─────────────────────────────────────────────────────────────────────────────
// Display helpers
// ─────────────────────────────────────────────────────────────────────────────

static void drawHeader(const char* title) {
  tft.fillRect(0, 0, W, HDR_H, D_PURPLE);
  tft.setTextColor(D_BG, D_PURPLE);
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("ATS-Mini", 5, HDR_H / 2);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(title, W - 5, HDR_H / 2);
}

static void drawFooter() {
  tft.fillRect(0, FTR_Y, W, FTR_H, D_LINE);
  tft.setTextColor(D_COMMENT, D_LINE);
  tft.setTextFont(1);
  tft.setTextDatum(ML_DATUM);
  char buf[72];
  if (wifiOK)
    snprintf(buf, sizeof(buf), "WiFi: %s   %s",
             WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  else if (apMode)
    snprintf(buf, sizeof(buf), "AP: %s   %s", AP_SSID,
             WiFi.softAPIP().toString().c_str());
  else
    strcpy(buf, "WiFi: not connected");
  tft.drawString(buf, 4, FTR_Y + FTR_H / 2);
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

static void drawProgressBar(int y, size_t done, size_t total) {
  static const int bw = W - 24;
  tft.drawRect(12, y, bw, 14, D_PURPLE);
  if (total > 0) {
    int fill = (int)((uint64_t)done * (bw - 2) / total);
    tft.fillRect(13, y + 1, fill, 12, D_PURPLE);
  }
  if (total > 0) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%u%%  (%u / %u KB)",
             (unsigned)(done * 100 / total),
             (unsigned)(done / 1024), (unsigned)(total / 1024));
    tft.setTextColor(D_FG, D_BG);
    tft.setTextFont(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(buf, W / 2, y + 22);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Splash screen
// ─────────────────────────────────────────────────────────────────────────────

static bool splashAndDecide() {
  tft.fillScreen(D_BG);

  // Callsign — large (Font4 = 26px)
  tft.setTextColor(D_CYAN, D_BG);
  tft.setTextFont(4);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("KQ4TXO", W / 2, 52);

  // Sub-title
  tft.setTextColor(D_PURPLE, D_BG);
  tft.setTextFont(2);
  tft.drawString("ATS-Mini Recovery", W / 2, 88);

  // Hint
  tft.setTextColor(D_YELLOW, D_BG);
  tft.setTextFont(1);
  tft.drawString("Hold encoder to enter recovery UI", W / 2, 112);

  // Check boot-loop counter
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

  // 2-second countdown with progress bar; button hold cancels
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
    // Increment counter before forwarding
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
    // Fall through if ota_0 not available
  }

  // Arrived here: button was held or no ota_0 partition
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Screen draw functions
// ─────────────────────────────────────────────────────────────────────────────

static void drawMain() {
  tft.fillScreen(D_BG);
  drawHeader("Recovery");
  static const char* labels[] = { "Boot Firmware", "Erase Data", "Network", "Reboot" };
  for (int i = 0; i < 4; i++) drawItem(i, labels[i], nullptr, i == sel);
  drawFooter();
}

static void drawEraseConfirm() {
  tft.fillScreen(D_BG);
  drawHeader("Erase Data?");
  clearBody();
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(D_FG, D_BG);
  tft.drawString("Erase all saved settings?", W / 2, 68);
  tft.setTextColor(D_COMMENT, D_BG);
  tft.setTextFont(1);
  tft.drawString("Band presets, themes, WiFi — all cleared", W / 2, 88);

  // sel 0 = No (left), sel 1 = Yes (right)
  uint16_t noBg  = (sel == 0) ? D_GREEN : D_LINE;
  uint16_t yesBg = (sel == 1) ? D_RED   : D_LINE;
  tft.fillRoundRect(40,  115, 90, 28, 5, noBg);
  tft.setTextColor((sel == 0) ? D_BG : D_FG, noBg);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("No",  85,  129);

  tft.fillRoundRect(190, 115, 90, 28, 5, yesBg);
  tft.setTextColor(D_FG, yesBg);
  tft.drawString("Yes", 235, 129);
  drawFooter();
}

static void drawNetwork() {
  tft.fillScreen(D_BG);
  drawHeader("Network");
  drawItem(0, "WiFi Scan",  nullptr,             sel == 0);
  drawItem(1, "OTA Update", nullptr,             sel == 1);
  drawItem(2, "Web Server", webOn ? "ON" : "OFF", sel == 2);
  // Back hint
  tft.setTextColor(D_COMMENT, D_BG);
  tft.setTextFont(1);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("Long press = back", W - 4, BODY_Y + 3 * ITEM_H + ITEM_H / 2);
  drawFooter();
}

static void drawWifiScan() {
  tft.fillScreen(D_BG);
  drawHeader("Select Network");
  if (scanning) {
    clearBody();
    tft.setTextColor(D_CYAN, D_BG);
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Scanning...", W / 2, BODY_Y + BODY_H / 2);
  } else if (scannedCnt == 0) {
    clearBody();
    tft.setTextColor(D_COMMENT, D_BG);
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("No networks found", W / 2, BODY_Y + BODY_H / 2);
  } else {
    int total = scannedCnt + 1;
    int vis = BODY_H / ITEM_H;
    int start = max(0, min(sel, total - vis));
    for (int i = start; i < start + vis && i < total; i++) {
      int row = i - start;
      if (i < scannedCnt) {
        char sig[6];
        int bars = scanned[i].rssi > -55 ? 4 : scanned[i].rssi > -67 ? 3 :
                   scanned[i].rssi > -78 ? 2 : 1;
        snprintf(sig, sizeof(sig), " %d\xe2\x96\x8a", bars);
        drawItem(row, scanned[i].ssid.c_str(), sig, i == sel);
      } else {
        drawItem(row, "[Back]", nullptr, i == sel);
      }
    }
  }
  drawFooter();
}

// Keyboard: 4 rows, encoder left/right = col, long press = next row
static void drawKbKey(int row, int col) {
  // Key geometry per row
  static const int KEY_H = 23, GAP = 2;
  const int KY = 56; // keyboard area starts at y=56

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
    // row 3: [SHF/ABC][123/SYM][SPACE][OK]
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

static void drawKb() {
  tft.fillScreen(D_BG);
  drawHeader("WiFi Password");

  // Pass display area y=22..54
  tft.fillRect(0, HDR_H, W, 32, D_LINE);
  tft.setTextFont(1);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(D_COMMENT, D_LINE);
  tft.drawString(kbSSID.c_str(), 5, HDR_H + 9);

  // Password as dots
  String dots;
  for (size_t i = 0; i < kbPass.length(); i++) dots += '\xb7';
  dots += '_';
  tft.setTextColor(D_FG, D_LINE);
  tft.drawString(("Pass: " + dots).c_str(), 5, HDR_H + 22);

  // Draw all keys
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < kbRowLen(r); c++)
      drawKbKey(r, c);
}

static void drawWifiConn() {
  tft.fillScreen(D_BG);
  drawHeader("Connecting...");
  clearBody();
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(D_FG, D_BG);
  tft.drawString("Connecting to:", W / 2, BODY_Y + 22);
  tft.setTextColor(D_CYAN, D_BG);
  tft.drawString(kbSSID.c_str(), W / 2, BODY_Y + 48);
  drawFooter();
}

static void drawOtaList() {
  tft.fillScreen(D_BG);
  drawHeader("OTA: Pick Release");
  if (!relLoaded) {
    clearBody();
    tft.setTextColor(relErr[0] ? D_RED : D_CYAN, D_BG);
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(relErr[0] ? relErr : "Loading...", W / 2, BODY_Y + BODY_H / 2);
  } else {
    int total = relCnt + 1;
    int vis = BODY_H / ITEM_H;
    int start = max(0, min(sel, total - vis));
    for (int i = start; i < start + vis && i < total; i++) {
      int row = i - start;
      if (i < relCnt) drawItem(row, releases[i].tag, nullptr, i == sel);
      else            drawItem(row, "[Back]",         nullptr, i == sel);
    }
  }
  drawFooter();
}

static void drawOtaProgress() {
  // Called repeatedly during download — don't clear full screen each time
  int y = BODY_Y;
  tft.fillRect(0, y, W, FTR_Y - y, D_BG);
  tft.setTextColor(D_FG, D_BG);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(otaTag[0] ? otaTag : "Downloading...", W / 2, y + 22);
  drawProgressBar(y + 48, otaWritten, otaTotal);
  // Don't call drawFooter every time to reduce flicker
}

static void drawWebSrv() {
  tft.fillScreen(D_BG);
  drawHeader("Web Server");
  clearBody();
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  if (webOn) {
    tft.setTextColor(D_GREEN, D_BG);
    tft.drawString("Running", W / 2, BODY_Y + 16);
    String url = "http://" +
      (wifiOK ? WiFi.localIP().toString() : WiFi.softAPIP().toString());
    tft.setTextColor(D_CYAN, D_BG);
    tft.setTextFont(1);
    tft.drawString(url.c_str(), W / 2, BODY_Y + 36);
    tft.setTextColor(D_COMMENT, D_BG);
    tft.drawString("Open in browser to flash / manage device", W / 2, BODY_Y + 50);
    tft.fillRoundRect(W/2 - 55, BODY_Y + 65, 110, 28, 5, D_RED);
    tft.setTextColor(D_FG, D_RED);
    tft.setTextFont(2);
    tft.drawString("Turn OFF", W / 2, BODY_Y + 79);
  } else {
    tft.setTextColor(D_RED, D_BG);
    tft.drawString("Stopped", W / 2, BODY_Y + 16);
    tft.fillRoundRect(W/2 - 55, BODY_Y + 65, 110, 28, 5, D_GREEN);
    tft.setTextColor(D_BG, D_GREEN);
    tft.drawString("Turn ON", W / 2, BODY_Y + 79);
  }
  tft.setTextColor(D_COMMENT, D_BG);
  tft.setTextFont(1);
  tft.drawString("Press = toggle   Long press = back", W / 2, FTR_Y - 8);
  drawFooter();
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
  dirty = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi helpers
// ─────────────────────────────────────────────────────────────────────────────

static void startScan() {
  scannedCnt = 0; scanning = true;
  if (WiFi.getMode() == WIFI_MODE_NULL) WiFi.mode(WIFI_STA);
  WiFi.scanNetworks(true, false);
  dirty = true;
}

static bool connectToNetwork(const char* ssid, const char* pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  uint32_t t = millis();
  int spin = 0;
  while (millis() - t < 12000) {
    wl_status_t s = WiFi.status();
    if (s == WL_CONNECTED) return true;
    if (s == WL_NO_SSID_AVAIL || s == WL_CONNECT_FAILED) break;
    // Animate spinner on screen
    static const char spins[] = "|/-\\";
    char sp[3] = { spins[spin++ % 4], 0 };
    tft.setTextColor(D_PURPLE, D_BG);
    tft.setTextFont(4);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(sp, W / 2, BODY_Y + 78);
    delay(200);
  }
  WiFi.disconnect(true); delay(100);
  return false;
}

static void startAP() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(300);
}

// ─────────────────────────────────────────────────────────────────────────────
// GitHub releases fetch (streaming JSON parse)
// ─────────────────────────────────────────────────────────────────────────────

static bool fetchReleases(int perPage = 25) {
  relCnt = 0; relLoaded = false; relErr[0] = 0;
  if (!wifiOK) { strcpy(relErr, "No WiFi"); return false; }

  char apiUrl[128];
  snprintf(apiUrl, sizeof(apiUrl),
    "https://api.github.com/repos/%s/releases?per_page=%d",
    GITHUB_REPO, perPage);

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(20000);
  http.begin(client, apiUrl);
  http.addHeader("User-Agent", "ATS-Mini-Recovery/2");
  http.addHeader("Accept", "application/vnd.github.v3+json");

  int code = http.GET();
  if (code != 200) {
    snprintf(relErr, sizeof(relErr), "HTTP %d", code);
    http.end(); return false;
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

  // Parse: find "tag_name":"..." then look for "-ospi-ota.bin" asset URL within ~12 KB forward
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
  cur = SCR_OTA_PROGRESS;
  drawHeader("Flashing...");
  otaTotal = 0; otaWritten = 0;
  drawOtaProgress();

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(60000);
  http.begin(client, url);
  http.addHeader("User-Agent", "ATS-Mini-Recovery/2");

  int code = http.GET();
  if (code != 200) {
    http.end();
    tft.setTextColor(D_RED, D_BG); tft.setTextFont(1);
    tft.setTextDatum(MC_DATUM);
    char err[32]; snprintf(err, sizeof(err), "HTTP %d", code);
    tft.drawString(err, W / 2, BODY_Y + 90);
    delay(3000);
    return false;
  }

  int total = http.getSize();
  if (!otaBegin(total > 0 ? (size_t)total : OTA_WITH_SEQUENTIAL_WRITES)) {
    http.end(); return false;
  }
  otaTotal = (size_t)(total > 0 ? total : 0);

  static uint8_t dlBuf[4096];
  WiFiClient* stream = http.getStreamPtr();
  uint32_t lastDraw = 0;

  while (http.connected()) {
    size_t av = stream->available();
    if (!av) { delay(1); continue; }
    int n = stream->readBytes(dlBuf, min(av, sizeof(dlBuf)));
    if (n <= 0) break;
    if (!otaWrite(dlBuf, (size_t)n)) { http.end(); return false; }
    if (millis() - lastDraw > 150) {
      lastDraw = millis();
      drawOtaProgress();
    }
  }
  http.end();

  if (otaWritten == 0) return false;
  bool ok = otaFinish();
  if (ok) {
    tft.setTextColor(D_GREEN, D_BG); tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Done! Rebooting...", W / 2, BODY_Y + 90);
    Serial.println("{\"ota\":\"complete\",\"ok\":true}");
    delay(2000); esp_restart();
  }
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
              apMode ? WiFi.softAPIP().toString() : "0.0.0.0";
  String ssid = wifiOK ? WiFi.SSID() : "";
  String wifi = wifiOK ? "ok" : apMode ? "ap" : "none";
  String json = "{\"wifi\":\"" + wifi + "\",\"ssid\":\"" + ssid +
                "\",\"ip\":\"" + ip + "\"}";
  server.send(200, "application/json", json);
}

static void webHandleReleases() {
  if (!relLoaded) {
    // Block and fetch (caller waits)
    fetchReleases(50);
  }
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
    if (!otaBegin()) return;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    otaWrite(up.buf, up.currentSize);
    if (otaTotal > 0 && (otaWritten % 65536) < up.currentSize) {
      Serial.printf("{\"web_ota_progress\":%u}\n", (unsigned)otaWritten);
    }
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
  server.send(200, "text/plain", "Starting download — watch device display");
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
  dirty = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Serial JSON (Android OTA protocol — unchanged from v1)
// ─────────────────────────────────────────────────────────────────────────────

static void handleSerialCmd(const char* line) {
  const char* cv = strstr(line, "\"cmd\":");
  if (!cv) return;
  const char* v = strchr(cv, ':'); if (!v) return;
  v++; while (*v == ' ' || *v == '"') v++;

  if (!strncmp(v, "ota_begin", 9)) {
    const char* sk = strstr(line, "\"size\":");
    size_t sz = sk ? (size_t)atol(sk + 7) : 0;
    if (!sz) { Serial.println("{\"ok\":false,\"err\":\"no size\"}"); return; }
    if (!otaBegin(sz)) { Serial.println("{\"ok\":false,\"err\":\"begin\"}"); return; }
    sst = SS_OTA; sOtaExp = sz;
    Serial.printf("{\"ok\":true,\"size\":%u}\n", (unsigned)sz);
  } else if (!strncmp(v, "ota_abort", 9)) {
    otaAbortAll(); sst = SS_IDLE; Serial.println("{\"ok\":true}");
  } else if (!strncmp(v, "status", 6)) {
    Serial.printf("{\"mode\":\"recovery\",\"wifi\":\"%s\",\"ip\":\"%s\"}\n",
      wifiOK ? "connected" : apMode ? "ap" : "none",
      wifiOK ? WiFi.localIP().toString().c_str() : WiFi.softAPIP().toString().c_str());
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
  cur = s; sel = initSel; dirty = true;
}

static void eraseSettings() {
  nvs_flash_erase();
  nvs_flash_init();
}

static void bootMain() {
  const esp_partition_t* p = getOta0();
  if (p && esp_ota_set_boot_partition(p) == ESP_OK) {
    tft.fillScreen(D_BG);
    tft.setTextColor(D_GREEN, D_BG); tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Booting main firmware...", W / 2, H / 2);
    delay(300); esp_restart();
  }
  // No ota_0 partition — stay in recovery
  dirty = true;
}

static void processInput() {
  int enc = encPop();
  int btn = btnEvent();

  // WiFi scan poll (background)
  if (scanning && cur == SCR_WIFI_SCAN) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      scanning = false;
      scannedCnt = min(n, 20);
      for (int i = 0; i < scannedCnt; i++) {
        scanned[i].ssid = WiFi.SSID(i);
        scanned[i].rssi = WiFi.RSSI(i);
      }
      WiFi.scanDelete();
      sel = 0; dirty = true;
    } else {
      if (enc || btn) dirty = true;
    }
    if (scanning) return;
  }

  // Spinner redraw for connecting screen
  if (cur == SCR_WIFI_CONN) {
    static uint32_t lastSpin = 0;
    if (millis() - lastSpin > 200) { lastSpin = millis(); dirty = true; }
    return; // connecting is blocking, handled in separate call
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
      if (enc) { sel = (sel + enc + 3) % 3; dirty = true; }
      if (btn == 1) {
        if (sel == 0) { // WiFi scan
          if (!wifiOK && !apMode) {
            WiFi.mode(WIFI_STA);
          }
          startScan();
          goTo(SCR_WIFI_SCAN);
        } else if (sel == 1) { // OTA
          if (!wifiOK) {
            // Try to connect to known networks first
            for (auto& net : KNOWN) {
              drawWifiConn();
              if (connectToNetwork(net.ssid, net.pass)) {
                wifiOK = true; apMode = false; break;
              }
            }
          }
          if (!relLoaded) {
            goTo(SCR_OTA_LIST);
            dirty = true; redraw();
            fetchReleases(25);
          }
          goTo(SCR_OTA_LIST);
        } else if (sel == 2) { // Web server
          goTo(SCR_WEBSERVER);
        }
      }
      if (btn == 2) goTo(SCR_MAIN);
      break;

    // ── WiFi scan list ────────────────────────────────────────────────────────
    case SCR_WIFI_SCAN: {
      int total = scannedCnt + 1;
      if (enc) { sel = (sel + enc + total) % total; dirty = true; }
      if (btn == 1) {
        if (sel < scannedCnt) {
          kbSSID = scanned[sel].ssid;
          kbPass = "";
          kbRow  = 0; kbCol = 0;
          kbShift = false; kbNums = false;
          goTo(SCR_WIFI_KB);
        } else {
          goTo(SCR_NETWORK);
        }
      }
      if (btn == 2) goTo(SCR_NETWORK);
      break;
    }

    // ── WiFi password keyboard ────────────────────────────────────────────────
    case SCR_WIFI_KB:
      if (enc) {
        kbCol = (kbCol + enc + kbRowLen(kbRow)) % kbRowLen(kbRow);
        dirty = true;
      }
      if (btn == 2) { // long press = next row
        kbRow = (kbRow + 1) % 4;
        kbCol = min(kbCol, kbRowLen(kbRow) - 1);
        dirty = true;
      }
      if (btn == 1) {
        char c = kbChar(kbRow, kbCol);
        switch ((uint8_t)c) {
          case 0x7f: // DEL
            if (kbPass.length() > 0) kbPass.remove(kbPass.length() - 1);
            break;
          case '\r': // OK — start connecting
            goTo(SCR_WIFI_CONN);
            drawWifiConn();
            if (connectToNetwork(kbSSID.c_str(), kbPass.c_str())) {
              wifiOK = true; apMode = false;
              tft.setTextColor(D_GREEN, D_BG); tft.setTextFont(2);
              tft.setTextDatum(MC_DATUM);
              tft.drawString("Connected!", W / 2, BODY_Y + 90);
              delay(1200);
              if (!webOn) { startWebServer(); }
              goTo(SCR_NETWORK);
            } else {
              tft.setTextColor(D_RED, D_BG); tft.setTextFont(2);
              tft.setTextDatum(MC_DATUM);
              tft.drawString("Failed — check password", W / 2, BODY_Y + 90);
              delay(2000);
              goTo(SCR_WIFI_KB);
            }
            return;
          case '\x01': // SHIFT
            if (kbNums) { kbNums = false; kbShift = false; }
            else kbShift = !kbShift;
            break;
          case '\x02': // NUM mode
            kbNums = !kbNums; kbShift = false;
            kbCol = min(kbCol, kbRowLen(kbRow) - 1);
            break;
          default:
            if (kbPass.length() < 63) kbPass += c;
            kbShift = false; // auto-release shift after one char
            break;
        }
        dirty = true;
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
          // Returns only on failure
          goTo(SCR_OTA_LIST);
        } else {
          goTo(SCR_NETWORK);
        }
      }
      if (btn == 2) goTo(SCR_NETWORK);
      break;
    }

    // ── Web server toggle ──────────────────────────────────────────────────────
    case SCR_WEBSERVER:
      if (btn == 1) {
        if (!webOn) { startWebServer(); }
        else        { server.stop(); webOn = false; }
        dirty = true;
      }
      if (btn == 2) goTo(SCR_NETWORK);
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

  // Power on LDO
  pinMode(PIN_PWR, OUTPUT); digitalWrite(PIN_PWR, HIGH); delay(50);

  // Encoder + button
  pinMode(PIN_BTN,   INPUT_PULLUP);
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  encALast = digitalRead(PIN_ENC_A);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encISR, CHANGE);

  // Backlight
  ledcAttach(PIN_LCD_BL, 16000, 8);
  ledcWrite(PIN_LCD_BL, 210);

  // Display
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

  // Splash + boot decision
  splashAndDecide();

  // We're in recovery UI — connect WiFi
  tft.fillScreen(D_BG);
  drawHeader("Recovery");
  clearBody();
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(D_COMMENT, D_BG);

  // Try known networks
  for (const auto& net : KNOWN) {
    char buf[64];
    snprintf(buf, sizeof(buf), "Trying %s...", net.ssid);
    tft.setCursor(8, BODY_Y + 10);
    tft.setTextColor(D_COMMENT, D_BG);
    tft.setTextFont(2);
    tft.print(buf);
    WiFi.mode(WIFI_STA);
    WiFi.begin(net.ssid, net.pass);
    if (WiFi.waitForConnectResult(10000) == WL_CONNECTED) {
      wifiOK = true;
      Serial.printf("{\"wifi\":\"connected\",\"ssid\":\"%s\",\"ip\":\"%s\"}\n",
        net.ssid, WiFi.localIP().toString().c_str());
      break;
    }
    WiFi.disconnect(true); delay(200);
  }

  if (!wifiOK) {
    apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    delay(300);
    Serial.printf("{\"wifi\":\"ap\",\"ip\":\"%s\"}\n",
      WiFi.softAPIP().toString().c_str());
  }

  // Always start web server
  startWebServer();

  dirty = true;
}

void loop() {
  processInput();
  if (dirty) redraw();
  if (webOn) server.handleClient();
  handleSerial();
  delay(5);
}
