/*
 * ATS-Mini Recovery Firmware
 *
 * Lives in the factory partition (0x10000). Boots when OTA data is
 * erased — triggered by:
 *   - Main app: button held 3 s at power-on
 *   - Main app: 3 consecutive boot failures (boot-loop detection)
 *   - Android app: {"cmd":"recovery"} serial command to main app
 *
 * Features:
 *   - WiFi: tries BEAST_ROUTER then IoT; falls back to AP mode
 *   - Web UI: browse to device IP to upload a .bin or trigger GitHub DL
 *   - GitHub download: fetches latest *-ospi-ota.bin and writes to ota_0
 *   - Serial JSON: always-on status + OTA upload protocol for Android app
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// ── Pin definitions ───────────────────────────────────────────────────────────
#define PIN_LCD_BL    38
#define PIN_POWER_ON  15

// ── Known WiFi networks (tried in order before AP fallback) ───────────────────
struct Network { const char* ssid; const char* pass; };
static const Network NETWORKS[] = {
  { "BEAST_ROUTER", "appu1989" },
  { "IoT",          "appu1989" },
};
static const char* AP_SSID = "ATS-Recovery";
static const char* AP_PASS = "ats12345";

// ── GitHub API endpoint for latest release ────────────────────────────────────
static const char* GITHUB_API =
  "https://api.github.com/repos/nphil/ats-mini-natefork/releases/latest";

// ── TFT / display ─────────────────────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();
static int tftY  = 0;
static const int LINE_H = 16;
static const int TFT_H  = 170;  // after rotation 3: height = 170

// ── Web server ────────────────────────────────────────────────────────────────
WebServer server(80);

// ── Runtime state ─────────────────────────────────────────────────────────────
static bool wifiOK = false;
static bool apMode = false;

// ── OTA session (shared by web-upload, serial, and GitHub download) ───────────
static esp_ota_handle_t       otaHandle   = 0;
static const esp_partition_t* otaPart     = nullptr;
static size_t                 otaExpected = 0;
static size_t                 otaReceived = 0;
static bool                   otaActive   = false;

// ── Serial state machine ──────────────────────────────────────────────────────
enum SerialState { SS_IDLE, SS_OTA_RX };
static SerialState serialState = SS_IDLE;

// ─────────────────────────────────────────────────────────────────────────────
// Display helpers
// ─────────────────────────────────────────────────────────────────────────────

static void tftPrint(const char* msg) {
  if (tftY + LINE_H > TFT_H) {
    tft.fillScreen(TFT_BLACK);
    tftY = 0;
  }
  tft.setCursor(2, tftY);
  tft.print(msg);
  tftY += LINE_H;
  Serial.println(msg);
}

static void tftPrintf(const char* fmt, ...) {
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  tftPrint(buf);
}

// ─────────────────────────────────────────────────────────────────────────────
// OTA helpers — always target ota_0 (main app slot)
// ─────────────────────────────────────────────────────────────────────────────

static const esp_partition_t* ota0() {
  if (!otaPart)
    otaPart = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  return otaPart;
}

static bool otaBegin(size_t size) {
  if (otaActive) { esp_ota_abort(otaHandle); otaActive = false; }
  const esp_partition_t* part = ota0();
  if (!part) { tftPrint("ERR: no ota_0 partition"); return false; }
  if (esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &otaHandle) != ESP_OK) {
    tftPrint("ERR: ota_begin failed"); return false;
  }
  otaExpected = size;
  otaReceived = 0;
  otaActive   = true;
  return true;
}

static bool otaWrite(const uint8_t* data, size_t len) {
  if (!otaActive) return false;
  if (esp_ota_write(otaHandle, data, len) != ESP_OK) {
    tftPrint("ERR: ota_write"); esp_ota_abort(otaHandle); otaActive = false; return false;
  }
  otaReceived += len;
  return true;
}

static bool otaFinish() {
  if (!otaActive) return false;
  otaActive = false;
  if (esp_ota_end(otaHandle) != ESP_OK)            { tftPrint("ERR: ota_end");   return false; }
  if (esp_ota_set_boot_partition(ota0()) != ESP_OK) { tftPrint("ERR: set_boot");  return false; }
  return true;
}

static void otaAbort() {
  if (otaActive) { esp_ota_abort(otaHandle); otaActive = false; }
}

// ─────────────────────────────────────────────────────────────────────────────
// GitHub download
// ─────────────────────────────────────────────────────────────────────────────

static String extractOtaUrl(const String& json) {
  // Find the asset whose "name" contains "-ospi-ota.bin" and return its
  // browser_download_url. GitHub JSON puts "name" before "browser_download_url"
  // in each asset object, so search forward from the name match.
  int namePos = json.indexOf("-ospi-ota.bin\"");
  if (namePos < 0) return "";
  int k = json.indexOf("\"browser_download_url\":\"", namePos);
  if (k < 0) return "";
  int start = k + 24;
  int end   = json.indexOf("\"", start);
  if (end < 0) return "";
  return json.substring(start, end);
}

static void downloadFromGitHub() {
  if (!wifiOK) { tftPrint("Need WiFi for GitHub DL"); return; }

  tftPrint("GitHub: fetching release info...");

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.begin(client, GITHUB_API);
  http.addHeader("User-Agent",  "ATS-Mini-Recovery/1");
  http.addHeader("Accept",      "application/vnd.github.v3+json");

  int code = http.GET();
  if (code != 200) {
    tftPrintf("GitHub API err: %d", code);
    http.end(); return;
  }

  String json = http.getString();
  http.end();

  String url = extractOtaUrl(json);
  json = "";  // free heap
  if (url.isEmpty()) { tftPrint("ERR: no ota asset in release"); return; }

  String fname = url.substring(url.lastIndexOf('/') + 1);
  tftPrintf("DL: %s", fname.c_str());
  Serial.printf("{\"github\":\"downloading\",\"file\":\"%s\"}\n", fname.c_str());

  // Download binary, streaming to ota_0
  http.begin(client, url);
  code = http.GET();
  if (code != 200) {
    tftPrintf("DL http err: %d", code);
    http.end(); return;
  }

  int total = http.getSize();
  tftPrintf("Size: %d bytes", total);
  if (!otaBegin((size_t)(total > 0 ? total : OTA_WITH_SEQUENTIAL_WRITES))) {
    http.end(); return;
  }

  WiFiClient* stream = http.getStreamPtr();
  static uint8_t buf[4096];
  int remaining = total;
  uint32_t lastPct = 0;

  while (http.connected() && (remaining > 0 || total < 0)) {
    size_t avail = stream->available();
    if (!avail) { delay(1); continue; }
    size_t toRead = min(avail, sizeof(buf));
    if (remaining > 0) toRead = min(toRead, (size_t)remaining);
    int n = stream->readBytes(buf, toRead);
    if (n <= 0) break;
    if (!otaWrite(buf, (size_t)n)) { http.end(); return; }
    if (remaining > 0) remaining -= n;

    if (total > 0) {
      uint32_t pct = (uint32_t)((uint64_t)otaReceived * 100 / total);
      if (pct / 10 != lastPct / 10) {
        tftPrintf("  %u%% (%u KB)", pct, (unsigned)(otaReceived / 1024));
        lastPct = pct;
      }
    }
  }
  http.end();

  if (otaReceived == 0) { tftPrint("ERR: no data received"); otaAbort(); return; }

  if (otaFinish()) {
    tftPrint("Done! Rebooting into main app...");
    Serial.println("{\"github\":\"complete\",\"ok\":true}");
    delay(2000);
    esp_restart();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Serial JSON command handler
// ─────────────────────────────────────────────────────────────────────────────

static void handleSerialCommand(const char* line) {
  // Minimal JSON key extraction — no library needed for this small command set
  const char* cmdKey = strstr(line, "\"cmd\":");
  if (!cmdKey) return;
  const char* v = strchr(cmdKey, ':');
  if (!v) return;
  v++;
  while (*v == ' ' || *v == '"') v++;

  if (strncmp(v, "ota_begin", 9) == 0) {
    const char* sk = strstr(line, "\"size\":");
    if (!sk) { Serial.println("{\"ok\":false,\"err\":\"no size\"}"); return; }
    size_t sz = (size_t)atol(sk + 7);
    if (sz == 0) { Serial.println("{\"ok\":false,\"err\":\"size=0\"}"); return; }
    if (!otaBegin(sz)) { Serial.println("{\"ok\":false,\"err\":\"begin\"}"); return; }
    serialState = SS_OTA_RX;
    tftPrintf("Serial OTA: %u B", (unsigned)sz);
    Serial.printf("{\"ok\":true,\"size\":%u}\n", (unsigned)sz);

  } else if (strncmp(v, "ota_abort", 9) == 0) {
    otaAbort();
    serialState = SS_IDLE;
    Serial.println("{\"ok\":true}");

  } else if (strncmp(v, "status", 6) == 0) {
    Serial.printf(
      "{\"mode\":\"recovery\",\"fw\":1,\"wifi\":\"%s\",\"ip\":\"%s\"}\n",
      wifiOK ? "connected" : (apMode ? "ap" : "none"),
      wifiOK ? WiFi.localIP().toString().c_str()
             : WiFi.softAPIP().toString().c_str());

  } else if (strncmp(v, "download", 8) == 0) {
    server.handleClient();  // drain any pending before blocking
    downloadFromGitHub();

  } else if (strncmp(v, "reboot", 6) == 0) {
    Serial.println("{\"ok\":true}");
    delay(500);
    esp_restart();

  } else if (strncmp(v, "recovery", 8) == 0) {
    // Acknowledged; we're already in recovery
    Serial.println("{\"mode\":\"recovery\"}");
  }
}

static void handleSerialInput() {
  if (serialState == SS_OTA_RX) {
    // Binary receive mode: drain Serial into ota_0
    static uint8_t buf[1024];
    while (Serial.available() && otaReceived < otaExpected) {
      size_t want = min((size_t)Serial.available(),
                        min(sizeof(buf), otaExpected - otaReceived));
      int n = Serial.readBytes(buf, want);
      if (n <= 0) break;
      if (!otaWrite(buf, (size_t)n)) {
        serialState = SS_IDLE;
        Serial.println("{\"ok\":false,\"err\":\"write\"}");
        return;
      }
      if ((otaReceived % 32768) < (size_t)n)
        Serial.printf("{\"progress\":%u,\"total\":%u}\n",
          (unsigned)otaReceived, (unsigned)otaExpected);
    }

    if (otaReceived >= otaExpected) {
      bool ok = otaFinish();
      serialState = SS_IDLE;
      Serial.printf("{\"ok\":%s,\"bytes\":%u}\n",
        ok ? "true" : "false", (unsigned)otaReceived);
      if (ok) {
        tftPrint("Serial OTA done, rebooting...");
        delay(1000);
        esp_restart();
      }
    }

  } else {
    // JSON line accumulator
    static char lineBuf[256];
    static int  lineLen = 0;
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\n' || c == '\r') {
        if (lineLen > 0) {
          lineBuf[lineLen] = '\0';
          handleSerialCommand(lineBuf);
          lineLen = 0;
        }
      } else if (lineLen < (int)sizeof(lineBuf) - 1) {
        lineBuf[lineLen++] = c;
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Web server handlers
// ─────────────────────────────────────────────────────────────────────────────

static void handleWebRoot() {
  String ip = wifiOK ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  String page =
    "<!DOCTYPE html><html><head>"
    "<title>ATS-Mini Recovery</title>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>"
    "body{font-family:sans-serif;max-width:580px;margin:20px auto;padding:0 12px;background:#111;color:#eee}"
    "h1{color:#f55}h2{color:#fa5}"
    "input[type=submit],button{background:#c33;color:#fff;border:0;padding:10px 22px;"
    "border-radius:5px;cursor:pointer;font-size:15px;margin-top:8px}"
    "input[type=file]{color:#eee}"
    "p{color:#aaa}"
    "</style></head><body>"
    "<h1>ATS-Mini Recovery</h1>"
    "<p>Device IP: <b>" + ip + "</b></p>"
    "<h2>Flash Firmware</h2>"
    "<p>Upload the <code>*-ospi-ota.bin</code> file from a GitHub release.</p>"
    "<form method='post' action='/update' enctype='multipart/form-data'>"
    "<input type='file' name='fw' accept='.bin'><br>"
    "<input type='submit' value='Flash'>"
    "</form>"
    "<h2>Download from GitHub</h2>"
    "<p>Requires WiFi. Downloads the latest <code>-ospi-ota.bin</code> automatically.</p>"
    "<form method='post' action='/github'>"
    "<button type='submit'>Download &amp; Flash Latest</button>"
    "</form>"
    "</body></html>";
  server.send(200, "text/html", page);
}

static void handleWebUploadBody() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    tftPrintf("Web upload: %s", up.filename.c_str());
    if (!otaBegin(OTA_WITH_SEQUENTIAL_WRITES)) return;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (!otaWrite(up.buf, up.currentSize)) return;
    if ((otaReceived % 65536) < up.currentSize)
      tftPrintf("  %u KB written", (unsigned)(otaReceived / 1024));
  } else if (up.status == UPLOAD_FILE_END) {
    if (otaActive) {
      bool ok = otaFinish();
      tftPrintf("Upload %s: %u bytes", ok ? "OK" : "FAIL", (unsigned)otaReceived);
    }
  }
}

static void handleWebUploadEnd() {
  if (!otaActive && otaReceived > 0) {
    // Already finished in UPLOAD_FILE_END
    server.send(200, "text/html",
      "<h2>Flash complete — rebooting in 2 s</h2>"
      "<script>setTimeout(()=>location.href='/',4000)</script>");
    delay(2000);
    esp_restart();
  } else {
    server.send(500, "text/plain", "Flash failed");
  }
}

static void handleWebGitHub() {
  server.send(200, "text/html",
    "<h2>Downloading from GitHub…</h2>"
    "<p>Watch the device display. It will reboot when done.</p>"
    "<p><a href='/'>Back</a></p>");
  delay(100);  // flush response
  downloadFromGitHub();
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi setup
// ─────────────────────────────────────────────────────────────────────────────

static void connectWifi() {
  for (const auto& net : NETWORKS) {
    tftPrintf("WiFi: %s", net.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(net.ssid, net.pass);
    if (WiFi.waitForConnectResult(12000) == WL_CONNECTED) {
      wifiOK = true;
      String ip = WiFi.localIP().toString();
      tftPrintf("  IP: %s", ip.c_str());
      Serial.printf("{\"wifi\":\"connected\",\"ssid\":\"%s\",\"ip\":\"%s\"}\n",
        net.ssid, ip.c_str());
      return;
    }
    WiFi.disconnect(true);
    delay(200);
  }

  // AP fallback
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(500);
  String ip = WiFi.softAPIP().toString();
  tftPrintf("AP: %s", AP_SSID);
  tftPrintf("PW: %s", AP_PASS);
  tftPrintf("IP: %s", ip.c_str());
  Serial.printf("{\"wifi\":\"ap\",\"ssid\":\"%s\",\"ip\":\"%s\"}\n",
    AP_SSID, ip.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup & loop
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  // Power on the LDO so display SPI bus isn't floating
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  delay(50);

  // Backlight
  ledcAttach(PIN_LCD_BL, 16000, 8);
  ledcWrite(PIN_LCD_BL, 200);

  // Display — minimal init, no PSRAM, no sprite
  tft.init();
  tft.setRotation(3);

  // Match main firmware's display variant detection
  uint8_t did3 = tft.readcommand8(ST7789_RDDID, 3);
  if (did3 == 0x93) {
    tft.invertDisplay(0);
    tft.writecommand(TFT_MADCTL);
    tft.writedata(TFT_MAD_MV | TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_BGR);
  } else if (did3 == 0x85) {
    tft.writecommand(0x26);
    tft.writedata(8);
    tft.writecommand(0x55);
    tft.writedata(0xB1);
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextFont(2);

  tftPrint("=== RECOVERY MODE ===");

  // Serial greeting — Android app detects "recovery" mode from this
  Serial.println("{\"mode\":\"recovery\",\"fw\":1}");

  connectWifi();

  // Web server
  server.on("/",        HTTP_GET,  handleWebRoot);
  server.on("/update",  HTTP_POST, handleWebUploadEnd, handleWebUploadBody);
  server.on("/github",  HTTP_POST, handleWebGitHub);
  server.begin();

  String ip = wifiOK ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  tftPrintf("http://%s", ip.c_str());
  tftPrint("Serial: {\"cmd\":\"status\"}");
}

void loop() {
  server.handleClient();
  handleSerialInput();
  delay(1);
}
