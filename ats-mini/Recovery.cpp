#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#include "Common.h"
#include "Recovery.h"

// ── Recovery AP credentials ───────────────────────────────────────────────────
#define RECOVERY_SSID "ATS-Mini Recovery"
#define RECOVERY_PASS "atsrecover"
#define RECOVERY_IP   "192.168.4.1"

// ── Web pages (stored in flash) ───────────────────────────────────────────────

static const char PAGE_MAIN[] PROGMEM = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ATS-Mini Recovery</title>
  <style>
    body{font-family:sans-serif;max-width:480px;margin:2em auto;padding:0 1em;background:#111;color:#eee}
    h1{color:#f90;font-size:1.4em}
    code{background:#222;padding:.1em .4em;border-radius:3px;color:#4f4}
    .btn{margin-top:1em;padding:.6em 1.6em;background:#f90;border:0;border-radius:4px;
         font-size:1em;cursor:pointer;color:#000}
    progress{width:100%;margin-top:1em}
    #status{margin-top:.8em;color:#4f4}
  </style>
</head>
<body>
  <h1>&#9889; ATS-Mini Recovery</h1>
  <p>Upload <code>ats-mini-vX.XX-ospi-ota.bin</code> from the GitHub releases page.</p>
  <form id="form" enctype="multipart/form-data">
    <input type="file" name="firmware" accept=".bin" id="file"><br>
    <button class="btn" type="button" onclick="doUpload()">Flash Firmware</button>
  </form>
  <progress id="bar" value="0" max="100" style="display:none"></progress>
  <div id="status"></div>
  <script>
  function doUpload(){
    var f=document.getElementById('file').files[0];
    if(!f){alert('Select a .bin file first');return;}
    var fd=new FormData();fd.append('firmware',f);
    var xhr=new XMLHttpRequest();
    xhr.open('POST','/update',true);
    xhr.upload.onprogress=function(e){
      if(e.lengthComputable){
        var p=Math.round(e.loaded/e.total*100);
        document.getElementById('bar').style.display='';
        document.getElementById('bar').value=p;
        document.getElementById('status').textContent='Uploading: '+p+'%';
      }
    };
    xhr.onload=function(){
      document.getElementById('status').textContent=xhr.responseText;
    };
    xhr.onerror=function(){
      document.getElementById('status').textContent='Connection lost — device may be rebooting.';
    };
    xhr.send(fd);
  }
  </script>
</body>
</html>
)html";

// ── Display helpers ───────────────────────────────────────────────────────────

static void recoveryDisplayInit()
{
  // LDO, backlight PWM, and TFT are already initialised in setup() before
  // checkRecoveryBoot() is called — just ramp brightness and clear.
  ledcWrite(PIN_LCD_BL, 255);
  tft.fillScreen(TFT_BLACK);
}

static void recoveryDraw(const char* line1, const char* line2 = nullptr,
                          const char* line3 = nullptr, uint16_t col1 = TFT_WHITE)
{
  tft.fillScreen(TFT_BLACK);

  // Header bar
  tft.fillRect(0, 0, 320, 28, 0xFB00 /*orange*/);
  tft.setTextColor(TFT_BLACK, 0xFB00);
  tft.setTextSize(2);
  tft.setCursor(6, 6);
  tft.print("RECOVERY MODE");

  tft.setTextSize(1);
  int y = 38;
  tft.setTextColor(col1, TFT_BLACK);
  tft.setCursor(6, y); tft.println(line1); y += 14;
  if (line2) { tft.setTextColor(TFT_CYAN,  TFT_BLACK); tft.setCursor(6, y); tft.println(line2); y += 14; }
  if (line3) { tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setCursor(6, y); tft.println(line3); }
}

static void recoveryDrawIdle()
{
  tft.fillScreen(TFT_BLACK);

  tft.fillRect(0, 0, 320, 28, 0xFB00);
  tft.setTextColor(TFT_BLACK, 0xFB00);
  tft.setTextSize(2);
  tft.setCursor(6, 6);
  tft.print("RECOVERY MODE");

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  int y = 38;
  auto row = [&](uint16_t c, const char* t){ tft.setTextColor(c,TFT_BLACK); tft.setCursor(6,y); tft.println(t); y+=16; };

  row(TFT_YELLOW, "1. On your phone / PC, connect to WiFi:");
  row(TFT_GREEN,  "   SSID:  " RECOVERY_SSID);
  row(TFT_GREEN,  "   Pass:  " RECOVERY_PASS);
  y += 4;
  row(TFT_YELLOW, "2. Open a browser and go to:");
  row(TFT_GREEN,  "   http://" RECOVERY_IP);
  y += 4;
  row(TFT_YELLOW, "3. Upload *-ospi-ota.bin from GitHub.");
  y += 4;
  row(TFT_CYAN,   "Waiting for connection...");
}

// ── Entry point ───────────────────────────────────────────────────────────────

void checkRecoveryBoot()
{
  // Configure encoder pin and sample — bail immediately if not held
  pinMode(ENCODER_PUSH_BUTTON, INPUT_PULLUP);
  delay(20);
  if (digitalRead(ENCODER_PUSH_BUTTON) != LOW) return;

  // Debounce: must still be held after 1 s to confirm it's intentional
  delay(1000);
  if (digitalRead(ENCODER_PUSH_BUTTON) != LOW) return;

  // ── Recovery mode entered ─────────────────────────────────────────────
  recoveryDisplayInit();
  recoveryDraw("Starting recovery...", nullptr, nullptr, TFT_YELLOW);
  ledcWrite(PIN_LCD_BL, 220);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(RECOVERY_SSID, RECOVERY_PASS);

  recoveryDrawIdle();

  // ── HTTP server ───────────────────────────────────────────────────────
  static WebServer srv(80);

  srv.on("/", HTTP_GET, [&srv]() {
    srv.send_P(200, "text/html", PAGE_MAIN);
  });

  srv.on("/update", HTTP_POST,
    // Completion handler
    [&srv]() {
      if (Update.hasError()) {
        String err = "Flash failed: " + String(Update.errorString());
        recoveryDraw("Flash failed!", Update.errorString(), "Try again — use *-ospi-ota.bin", TFT_RED);
        srv.send(500, "text/plain", err);
      } else {
        recoveryDraw("Done!", "Rebooting into new firmware...", nullptr, TFT_GREEN);
        srv.send(200, "text/plain", "Done! Device is rebooting...");
        delay(1500);
        esp_restart();
      }
    },
    // Upload chunk handler
    [&srv]() {
      HTTPUpload& up = srv.upload();

      if (up.status == UPLOAD_FILE_START) {
        recoveryDraw("Receiving firmware...", up.filename.c_str(), nullptr, TFT_YELLOW);
        // U_FLASH writes to the next OTA slot — never touches the running partition
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
          recoveryDraw("Update.begin failed!", Update.errorString(), nullptr, TFT_RED);
        }
      } else if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize) {
          recoveryDraw("Write error!", Update.errorString(), nullptr, TFT_RED);
        } else {
          // Show progress
          char prog[40];
          snprintf(prog, sizeof(prog), "Received: %u KB", (unsigned)(up.totalSize / 1024));
          recoveryDraw("Flashing...", prog, nullptr, TFT_YELLOW);
        }
      } else if (up.status == UPLOAD_FILE_END) {
        if (!Update.end(true)) {
          recoveryDraw("Verify failed!", Update.errorString(), nullptr, TFT_RED);
        }
        // Completion handler fires next
      }
    }
  );

  srv.begin();

  // Loop forever — main app never starts
  while (true) {
    srv.handleClient();
    delay(1);
  }
}
