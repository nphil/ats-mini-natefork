#include "Common.h"
#include "Storage.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"
#include <LittleFS.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <qrcode.h>

void drawSplash()
{
  spr.fillSprite(TH.bg);

  // --- Antenna tower (left side, x=60) ---
  const int ax = 60, atop = 12, abase = 158;
  uint16_t antCol = TH.text;
  spr.drawLine(ax, atop, ax, abase, antCol);          // mast
  spr.drawLine(ax-22, 55, ax+22, 55, antCol);         // crossbar 1
  spr.drawLine(ax-16, 85, ax+16, 85, antCol);         // crossbar 2
  spr.drawLine(ax-10, 112, ax+10, 112, antCol);       // crossbar 3
  spr.drawLine(ax-28, abase, ax+28, abase, antCol);   // base
  spr.drawLine(ax-28, abase, ax-35, abase+8, antCol); // base foot L
  spr.drawLine(ax+28, abase, ax+35, abase+8, antCol); // base foot R

  // --- Radio waves (arcs from antenna top, going right) ---
  uint16_t waveColors[] = { 0x07FF, 0x05DF, 0x039F }; // bright → dim cyan
  int radii[] = { 22, 40, 58 };
  for(int i = 0; i < 3; i++) {
    spr.drawArc(ax, atop, radii[i], radii[i]-3, 300, 60, waveColors[i], TH.bg);
  }

  // --- Callsign (center-right) ---
  spr.setFreeFont(&Orbitron_Light_24);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(0xFFE0, TH.bg);             // yellow
  spr.drawString("KQ4TXO", 210, 65);

  // --- Subtitle lines ---
  spr.setTextColor(TH.text, TH.bg);
  spr.drawString("Ham Radio Controller", 210, 108, 2);
  spr.setTextColor(TH.text_muted, TH.bg);
  spr.drawString(RECEIVER_DESC, 210, 126, 2);
  spr.drawString(getVersion(), 210, 144, 2);

  spr.pushSprite(0, 0);
  delay(2500);
}

static void displayQRCode(esp_qrcode_handle_t qrcode)
{
  int size = esp_qrcode_get_size(qrcode);

  for(int y = 0 ; y < size ; y++)
    for(int x = 0 ; x < size ; x++)
      if(esp_qrcode_get_module(qrcode, x, y))
        spr.fillRect(2 + x * 4, 170 - 2 - size * 4 + y * 4, 4, 4, TH.text);
}

static void drawAboutCommon(uint8_t arrow)
{
  if(arrow & 3) spr.fillRect(282, 11, 22, 3, TH.text_muted);
  if(arrow & 2) spr.fillTriangle(279, 12, 285, 8, 285, 16, TH.text_muted);
  if(arrow & 1) spr.fillTriangle(307, 12, 301, 8, 301, 16, TH.text_muted);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.text_muted);
  spr.drawString(RECEIVER_DESC, 0, 0, 4);
  spr.setTextColor(TH.text);
  spr.drawString(getVersion(), 2, 25, 2);
}

//
// Show HELP screen
//
void drawAboutHelp(uint8_t arrow)
{
  drawAboutCommon(arrow);
  esp_qrcode_config_t qrcode_config = ESP_QRCODE_CONFIG_DEFAULT();
  qrcode_config.display_func = displayQRCode;
  esp_qrcode_generate(&qrcode_config, MANUAL_URL);
  spr.drawString("Scan the QR code to read", 130, 70 + 16 * -1, 2);
  spr.drawString("the User Manual.", 130, 70 + 16 * 0, 2);
  spr.drawString("Click the encoder button", 130, 70 + 16 * 1, 2);
  spr.drawString("to continue.", 130, 70 + 16 * 2, 2);
  if(arrow)
  {
    spr.drawString("Rotate the encoder to see", 130, 70 + 16 * 3, 2);
    spr.drawString("the next page.", 130, 70 + 16 * 4, 2);
  }
  else
  {
    spr.drawString("To see this screen again,", 130, 70 + 16 * 4, 2);
    spr.drawString("go to Menu->Settings->About.", 130, 70 + 16 * 5, 2);
  }
  spr.pushSprite(0, 0);
}

//
// Show SYSTEM screen
//
static void drawAboutSystem(uint8_t arrow)
{
  drawAboutCommon(arrow);

  char text[100];
  sprintf(
    text,
    "CPU: %s r%i, %lu MHz",
    ESP.getChipModel(),
    ESP.getChipRevision(),
    ESP.getCpuFreqMHz()
  );
  spr.drawString(text, 2, 70 + 16 * -1, 2);

  sprintf(
    text,
    "FLASH: %luM, %luk (%luk), FS %luk (%luk)",
    ESP.getFlashChipSize() / (1024U * 1024U),
    ESP.getFreeSketchSpace() / 1024U,
    (ESP.getFreeSketchSpace() - ESP.getSketchSize()) / 1024U,
    (unsigned long)LittleFS.totalBytes() / 1024U,
    (unsigned long)(LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024U
  );
  spr.drawString(text, 2, 70 + 16 * 0, 2);

  nvs_stats_t nvs_stats;
  nvs_get_stats(STORAGE_PARTITION, &nvs_stats);
  sprintf(
    text,
    "NVS: TOTAL %u, USED %u, FREE %u",
    nvs_stats.total_entries,
    nvs_stats.used_entries,
    nvs_stats.free_entries
  );
  spr.drawString(text, 2, 70 + 16 * 1, 2);

  sprintf(
    text,
    "MEM: HEAP %luk (%luk), PSRAM %luk (%luk)",
    ESP.getHeapSize()/1024U, ESP.getFreeHeap()/1024U,
    ESP.getPsramSize()/1024U, ESP.getFreePsram()/1024U
  );
  spr.drawString(text, 2, 70 + 16 * 2, 2);

  sprintf(
    text,
    "Display ID: %08lX, STAT: %02X%08lX",
    tft.readcommand32(ST7789_RDDID, 1),
    tft.readcommand8(ST7789_RDDST, 1),
    tft.readcommand32(ST7789_RDDST, 2)
  );
  spr.drawString(text, 2, 70 + 16 * 3, 2);

  char *ip = getWiFiIPAddress();
  sprintf(text, "WiFi MAC: %s%s%s", getMACAddress(), *ip ? ", IP: " : "", *ip ? ip : "");
  spr.drawString(text, 2, 70 + 16 * 4, 2);

  for(int i=0 ; i<8 ; i++)
  {
    uint16_t rgb = (i&1? 0x001F:0) | (i&2? 0x07E0:0) | (i&4? 0xF800:0);
    spr.fillRect(i*40, 160, 40, 20, rgb);
  }
  spr.pushSprite(0, 0);
}

//
// Show AUTHORS screen
//
static void drawAboutAuthors(uint8_t arrow)
{
  drawAboutCommon(arrow);
  spr.drawString(FIRMWARE_URL, 2, 25 + 16, 2);
  spr.drawString(AUTHORS_LINE1, 2, 70, 2);
  spr.drawString(AUTHORS_LINE2, 2, 70 + 16, 2);
  spr.drawString(AUTHORS_LINE3, 2, 70 + 16 * 2, 2);
  spr.drawString(AUTHORS_LINE4, 2, 70 + 16 * 3, 2);
  spr.pushSprite(0, 0);
}

//
// Draw ABOUT screens
//
void drawAbout()
{
  switch(doAbout(0))
  {
    case 0: drawAboutHelp(1); break;
    case 1: drawAboutAuthors(3); break;
    case 2: drawAboutSystem(2); break;
    default: break;
  }
}
