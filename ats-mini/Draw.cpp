#include "Common.h"
#include "Themes.h"
#include "Storage.h"
#include "Utils.h"
#include "Menu.h"
#include "Ble.h"
#include "Draw.h"
#include <WiFi.h>
#include "Button.h"
#include <math.h>
#include "EIBI.h"

//
// Draw preferences write indicator
//
// Draw two horizontal CPU-load bars (core 0 and core 1).
// Always shows a 2-px stub so bars are visible even at 0 % load.
void drawCpuBars(int x, int y)
{
  const int barW  = 35;
  const int barH  = 4;
  const int gap   = 3;
  const int stubs = 2;  // minimum filled pixels

  int fill0 = stubs + (barW - stubs) * getCpuLoad(0) / 100;
  int fill1 = stubs + (barW - stubs) * getCpuLoad(1) / 100;

  // Core 0 bar (blue/cyan)
  spr.fillRect(x, y,          barW, barH, TH.bg);
  spr.fillRect(x, y,          fill0, barH, TH.smeter_bar);

  // Core 1 bar (orange/red)
  spr.fillRect(x, y+barH+gap, barW, barH, TH.bg);
  spr.fillRect(x, y+barH+gap, fill1, barH, TH.smeter_bar_plus);
}

void drawSaveIndicator(int x, int y)
{
  if(prefsAreWritten() || switchThemeEditor())
  {
    // Draw preferences write request icon
    spr.fillRect(x+3, y+2, 3, 5, TH.save_icon);
    spr.fillTriangle(x+1, y+7, x+7, y+7, x+4, y+10, TH.save_icon);
    spr.drawLine(x, y+12, x, y+13, TH.save_icon);
    spr.drawLine(x, y+13, x+8, y+13, TH.save_icon);
    spr.drawLine(x+8, y+13, x+8, y+12, TH.save_icon);
  }
}

//
// Draw Bluetooth indicator
//
void drawBleIndicator(int x, int y)
{
  int8_t status = getBleStatus();

  // If need to draw BLE icon...
  if(status || switchThemeEditor())
  {
    uint16_t color = (status>0) ? TH.rf_icon_conn : TH.rf_icon;

    // For the editor, alternate between BLE states every ~8 seconds
    if(switchThemeEditor())
      color = millis()&0x2000? TH.rf_icon_conn : TH.rf_icon;

    spr.drawLine(x+3, y+1, x+3, y+13, color);
    spr.drawLine(x+3, y+1, x+6, y+4, color);
    spr.drawLine(x+6, y+4, x, y+10, color);
    spr.drawLine(x, y+4, x+6, y+10, color);
    spr.drawLine(x+6, y+10, x+3, y+13, color);
  }
}

//
// Draw WiFi indicator
//
void drawWiFiIndicator(int x, int y)
{
  int8_t status = getWiFiStatus();
  bool showIcon = (status == 2 && internetConnected) || (status == 1) || switchThemeEditor();

  // If need to draw WiFi icon...
  if(showIcon)
  {
    uint16_t outerColor = TH.rf_icon;
    uint16_t middleColor = TH.rf_icon;
    uint16_t innerColor = TH.rf_icon;

    if (switchThemeEditor())
    {
      uint16_t color = millis() & 0x2000 ? TH.rf_icon_conn : TH.rf_icon;
      outerColor = middleColor = innerColor = color;
    }
    else if (status == 2) // Connected STA
    {
      // 3 arcs if RSSI >= -65, 2 arcs if >= -80, 1 arc otherwise
      int numArcs = 1;
      if (cachedWiFiRSSI >= -65 && cachedWiFiRSSI < 0) {
        numArcs = 3;
      } else if (cachedWiFiRSSI >= -80 && cachedWiFiRSSI < 0) {
        numArcs = 2;
      }

      innerColor = TH.rf_icon_conn;
      middleColor = (numArcs >= 2) ? TH.rf_icon_conn : TH.bg;
      outerColor = (numArcs >= 3) ? TH.rf_icon_conn : TH.bg;
    }
    else if (status > 0) // Other active connection (e.g. AP mode)
    {
      outerColor = middleColor = innerColor = TH.rf_icon_conn;
    }
    else
    {
      outerColor = middleColor = innerColor = TH.rf_icon;
    }

    spr.drawSmoothArc(x, 15+y, 4, 3, 150, 210, innerColor, TH.bg);
    if (middleColor != TH.bg) spr.drawSmoothArc(x, 15+y, 9, 8, 150, 210, middleColor, TH.bg);
    if (outerColor != TH.bg) spr.drawSmoothArc(x, 15+y, 14, 13, 150, 210, outerColor, TH.bg);
  }
}

//
// Draw network status
//
bool drawWiFiStatus(const char *statusLine1, const char *statusLine2, int x, int y)
{
  if(statusLine1 || statusLine2)
  {
    // Draw two lines of network status
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(TH.rds_text);
    if(statusLine1) spr.drawString(statusLine1, x, y, 2);
    if(statusLine2) spr.drawString(statusLine2, x, y+17, 2);
    return(true);
  }

  return(false);
}

//
// Draw zoomed menu item
//
void drawZoomedMenu(const char *text, bool force)
{
  if (!zoomMenu && !force) return;

  spr.fillSmoothRoundRect(RDS_OFFSET_X - 72 + 1, RDS_OFFSET_Y - 3 + 1, 152, 26, 4, TH.menu_bg);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.menu_item);
  spr.drawString(text, RDS_OFFSET_X + 5, RDS_OFFSET_Y, 4);
  spr.drawSmoothRoundRect(RDS_OFFSET_X - 72, RDS_OFFSET_Y - 3, 4, 4, 154, 28, TH.menu_border, TH.menu_bg);
}

//
// Show overlay message in large letters
//
void drawMessage(const char *msg)
{
  if(sleepOn()) return;

  drawZoomedMenu(msg, true);
  spr.pushSprite(0, 0);
}

//
// Show overlay message with a progress bar (0-100)
//
void drawMessageProgress(const char *msg, uint8_t pct)
{
  if(sleepOn()) return;

  const int x = RDS_OFFSET_X - 72;
  const int y = RDS_OFFSET_Y - 3;
  const int w = 154;
  const int h = 42;

  spr.fillSmoothRoundRect(x + 1, y + 1, w - 2, h - 2, 4, TH.menu_bg);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.menu_item);
  spr.drawString(msg, RDS_OFFSET_X + 5, RDS_OFFSET_Y, 4);
  spr.drawSmoothRoundRect(x, y, 4, 4, w, h, TH.menu_border, TH.menu_bg);

  // Progress bar inside the box
  const int bx = x + 8;
  const int by = y + h - 11;
  const int bw = w - 16;
  const int bh = 5;
  spr.fillRect(bx, by, bw, bh, TH.bg);
  if(pct > 0) spr.fillRect(bx, by, (bw * pct) / 100, bh, TH.menu_border);
  spr.drawRect(bx - 1, by - 1, bw + 2, bh + 2, TH.menu_border);

  spr.pushSprite(0, 0);
}

//
// Draw band and mode indicators
//
void drawBandAndMode(const char *band, const char *mode, int x, int y)
{
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.band_text);
  uint16_t band_width = spr.drawString(band, x, y);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.mode_text);
  uint16_t mode_width = spr.drawString(mode, x + band_width / 2 + 12, y + 8, 2);

  spr.drawSmoothRoundRect(x + band_width / 2 + 7, y + 7, 4, 4, mode_width + 8, 17, TH.mode_border, TH.bg);
}

//
// Draw radio text
//
void drawRadioText(int y, int ymax)
{
  const char *rt = getRadioText();

  // Draw potentially multi-line radio text
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.rds_text);
  for(; *rt && (y<ymax) ; y+=17, rt+=strlen(rt)+1)
    spr.drawString(rt, 160, y, 2);

  // Show program info if we have it and there is enough space
  if((y<ymax) && *getProgramInfo())
    spr.drawString(getProgramInfo(), 160, y, 2);
}

//
// Draw frequency
//
void drawFrequency(uint32_t freq, int x, int y, int ux, int uy, uint8_t hl)
{
  struct Line { int x, y, w; };

  const Line hlDigitsFM[] =
  {
    { x - 30 - 32 * 0 -  0, y + 28, 27 }, //         .01
    { x - 30 - 32 * 0 - 16, y + 28, 27 + 16 }, //    .05
    { x - 30 - 32 * 1 -  0, y + 28, 27 }, //         .10
    { x - 30 - 32 * 1 - 22, y + 28, 27 + 22 }, //    .50
    { x - 30 - 32 * 2 - 12, y + 28, 27 }, //        1.00
    { x - 30 - 32 * 2 - 28, y + 28, 27 + 16 }, //   5.00
    { x - 30 - 32 * 3 - 12, y + 28, 27 }, //       10.00
    { x - 30 - 32 * 3 - 28, y + 28, 27 + 16 }, //  50.00
    { x - 30 - 32 * 4 +  4, y + 28, 11 }, //      100.00
  };

  const Line hlDigitsAMSSB[] =
  {
    { x + 12 + 14 * 2 -  0, y + 28, 12 }, //           .001
    { x + 12 + 14 * 2 -  7, y + 28, 12 + 7 }, //       .005
    { x + 12 + 14 * 1 -  0, y + 28, 12 }, //           .010
    { x + 12 + 14 * 1 -  7, y + 28, 12 + 7 }, //       .050
    { x + 12 + 14 * 0 -  0, y + 28, 12 }, //           .100
    { x + 12 + 14 * 0 - 11, y + 28, 12 + 11 }, //      .500
    { x - 30 - 32 * 0 -  0, y + 28, 27 }, //          1.000
    { x - 30 - 32 * 0 - 16, y + 28, 27 + 16 }, //     5.000
    { x - 30 - 32 * 1 -  0, y + 28, 27 }, //         10.000
    { x - 30 - 32 * 1 - 16, y + 28, 27 + 16 }, //    50.000
    { x - 30 - 32 * 2 -  0, y + 28, 27 }, //        100.000
    { x - 30 - 32 * 2 - 16, y + 28, 27 + 16 }, //   500.000
    { x - 30 - 32 * 3 -  0, y + 28, 27 }, //       1000.000
    { x - 30 - 32 * 3 - 16, y + 28, 27 + 16 }, //  5000.000
    { x - 30 - 32 * 4 -  0, y + 28, 27 }, //      10000.000
  };

  // Top bit specifies if the digit selector is on
  bool selectOn = hl & 0x80;
  const struct Line *li;

  // Lower 7 bits specify the selected digit
  hl &= 0x7F;

  spr.setTextDatum(MR_DATUM);
  spr.setTextColor(TH.freq_text);

  if(currentMode==FM)
  {
    // Determine where underscore is located
    li = hl<ITEM_COUNT(hlDigitsFM)? &hlDigitsFM[hl] : 0;

    // FM frequency
    spr.drawFloat(freq/100.00, 2, x, y, 7);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(TH.funit_text);
    spr.drawString("MHz", ux, uy);
  }
  else
  {
    // Determine where underscore is located
    li = hl<ITEM_COUNT(hlDigitsAMSSB)? &hlDigitsAMSSB[hl] : 0;

    if(isSSB())
    {
      // SSB frequency
      char text[32];
      freq = freq * 1000 + currentBFO;
      sprintf(text, "%3.3lu", freq / 1000);
      spr.drawString(text, x, y, 7);
      spr.setTextDatum(ML_DATUM);
      sprintf(text, ".%3.3lu", freq % 1000);
      spr.drawString(text, 4+x, 17+y, 4);
    }
    else
    {
      // AM frequency
      spr.drawNumber(freq, x, y, 7);
      spr.setTextDatum(ML_DATUM);
      spr.drawString(".000", 4+x, 17+y, 4);
    }

    // SSB/AM frequencies are measured in kHz
    spr.setTextColor(TH.funit_text);
    spr.drawString("kHz", ux, uy);
  }

  // If drawing an underscore...
  if(li)
  {
    if(selectOn)
    {
      spr.fillRoundRect(li->x + 1, li->y - 1, li->w - 2, 3, 1, TH.freq_hl_sel);
      spr.fillTriangle(li->x, li->y, li->x + 2, li->y - 2, li->x + 2, li->y + 2, TH.freq_hl_sel);
      spr.fillTriangle(li->x + li->w - 1, li->y, li->x + li->w - 3, li->y - 2, li->x + li->w - 3, li->y + 2, TH.freq_hl_sel);
    }
    else
    {
      spr.fillRoundRect(li->x, li->y - 1, li->w, 3, 1, TH.freq_hl);
    }
  }
}

//
// Draw tuner scale
//
void drawScale(uint32_t freq)
{
  // Scale pointer: small upward triangle sitting just above the tick marks.
  // Kept inside the scale zone (y≥144) so it never overlaps RDS or content.
  spr.fillTriangle(157, 150, 163, 150, 160, 144, TH.scale_pointer);

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.scale_text);

  // Extra frequencies to draw outside the screen boundaries
  // (ensures frequency numbers don't disappear at the edges)
  int16_t slack = 3;

  // Scale offset
  int16_t offset = ((freq % 10) / 10.0 + slack) * 8;

  // Start drawing frequencies from the left
  freq = freq / 10 - 20 - slack;

  // Get band edges
  const Band *band = getCurrentBand();
  uint32_t minFreq = band->minimumFreq / 10;
  uint32_t maxFreq = band->maximumFreq / 10;

  for(int i=0 ; i<(slack + 41 + slack) ; i++, freq++)
  {
    int16_t x = i * 8 - offset;
    if(freq >= minFreq && freq <= maxFreq)
    {
      uint16_t lineColor = (i==20) && (!offset || (!(freq%5) && offset==1))?
        TH.scale_pointer : TH.scale_line;

      if((freq % 10) == 0)
      {
        spr.drawLine(x, 169, x, 150, lineColor);
        spr.drawLine(x + 1, 169, x + 1, 150, lineColor);
        if(currentMode == FM)
          spr.drawFloat(freq / 10.0, 1, x, 140, 2);
        else if(freq >= 100)
          spr.drawFloat(freq / 100.0, 3, x, 140, 2);
        else
          spr.drawNumber(freq * 10, x, 140, 2);
      }
      else if((freq % 5) == 0 && (freq % 10) != 0)
      {
        spr.drawLine(x, 169, x, 155, lineColor);
        spr.drawLine(x + 1, 169, x + 1, 155, lineColor);
      }
      else
      {
        spr.drawLine(x, 169, x, 160, lineColor);
      }
    }
  }
}

//
// Draw S-meter
//
void drawSMeter(int strength, int x, int y)
{
  spr.drawTriangle(x + 1, y + 1, x + 11, y + 1, x + 6, y + 6, TH.smeter_icon);
  spr.drawLine(x + 6, y + 1, x + 6, y + 14, TH.smeter_icon);

  for(int i=0 ; i<strength ; i++)
  {
    if(i<10)
      spr.fillRect(15+x + (i*4), 2+y, 2, 12, TH.smeter_bar);
    else
      spr.fillRect(15+x + (i*4), 2+y, 2, 12, TH.smeter_bar_plus);
  }
}

//
// Draw stereo indicator
//
void drawStereoIndicator(int x, int y, bool stereo)
{
  if(stereo)
  {
    // Split S-meter into two rows
    spr.fillRect(15 + x, 7 + y, 4 * 17 - 2, 2, TH.bg);
  }
  // Add an "else" statement here to draw a mono indicator
}

//
// Draw RDS station name (also CB channel, etc)
//
void drawStationName(const char *name, int x, int y)
{
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.rds_text);
  spr.drawString(name, x, y, 2);
}

//
// Draw long (EIBI) station name
//
void drawLongStationName(const char *name, int x, int y)
{
  int width = spr.textWidth(name, 2);
  spr.setTextColor(TH.rds_text);

  if((x + width) >= 320)
  {
    spr.setTextDatum(TL_DATUM);
    spr.drawString(name, x, y, 2);
  }
  else if(width <= 60)
  {
    spr.setTextDatum(TC_DATUM);
    spr.drawString(name, x + (320 - x) / 3, y, 2);
  }
  else
  {
    spr.setTextDatum(TC_DATUM);
    spr.drawString(name, x + (320 - x + width) / 4, y, 2);
  }
}

//
// Draw scan graphs
//
// Blend two RGB565 colours: alpha=0 → bg, alpha=255 → fg
static uint16_t blendColor(uint16_t fg, uint16_t bg, uint8_t alpha)
{
  uint8_t r = (uint8_t)((((fg >> 11) & 0x1F) * alpha + ((bg >> 11) & 0x1F) * (255 - alpha)) / 255);
  uint8_t g = (uint8_t)((((fg >>  5) & 0x3F) * alpha + ((bg >>  5) & 0x3F) * (255 - alpha)) / 255);
  uint8_t b = (uint8_t)((( fg        & 0x1F) * alpha + ( bg        & 0x1F) * (255 - alpha)) / 255);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

void drawScanGraphs(uint32_t freq, bool ghost)
{
  // Save original center frequency before it is modified below
  uint32_t centerFreq = freq;

  // In ghost mode blend all colours with the background at ~40 % opacity.
  // Also clamp the drawable height to the scale zone (y=144–169 = 25 px) so
  // the ghost doesn't bleed into the display content area above the scale.
  const uint8_t GHOST_ALPHA = 100;
  const int     maxH = ghost ? 25 : 40;  // max bar height in pixels
  uint16_t c_grid = ghost ? blendColor(TH.scan_grid, TH.bg, GHOST_ALPHA) : TH.scan_grid;
  uint16_t c_snr  = ghost ? blendColor(TH.scan_snr,  TH.bg, GHOST_ALPHA) : TH.scan_snr;
  uint16_t c_rssi = ghost ? blendColor(TH.scan_rssi, TH.bg, GHOST_ALPHA) : TH.scan_rssi;

  // Scale offset
  int16_t offset = (freq % 10) / 10.0 * 8;

  // Start drawing frequencies from the left
  freq = freq / 10 - 20;

  // Get band edges
  const Band *band = getCurrentBand();
  uint32_t minFreq = band->minimumFreq / 10;
  uint32_t maxFreq = band->maximumFreq / 10;

  for(int i=0 ; i<41 ; i++, freq++)
  {
    int16_t x = i * 8 - offset;

    if(freq >= minFreq && freq <= maxFreq)
    {
      if((freq % 5) == 0) {
        for(int y=0; y<maxH+2; y+=2) {
          spr.drawPixel(x, 169-y, c_grid);
        }
      }

      if((freq+1) <= maxFreq) {
        for(int xd=x; xd<(x+8); xd+=2) {
          // Only draw horizontal gridlines that fall within the drawable zone
          if(40 <= maxH) spr.drawPixel(xd, 169-40, c_grid);
          if(30 <= maxH) spr.drawPixel(xd, 169-30, c_grid);
          if(20 <= maxH) spr.drawPixel(xd, 169-20, c_grid);
          if(10 <= maxH) spr.drawPixel(xd, 169-10, c_grid);
          spr.drawPixel(xd, 169-0, c_grid);
        }
        int snr1 = maxH * scanGetSNR(freq * 10);
        int snr2 = maxH * scanGetSNR((freq+1) * 10);
        spr.drawLine(x, 169-snr1, x+8, 169-snr2, c_snr);
        int rssi1 = maxH * scanGetRSSI(freq * 10);
        int rssi2 = maxH * scanGetRSSI((freq+1) * 10);
        spr.drawLine(x, 169-rssi1, x+8, 169-rssi2, c_rssi);
      }
    }
  }
  // Draw channel markers only in non-ghost mode
  if (!ghost)
  for(int ch = 0; ch < scanChannels.count; ch++)
  {
    // Map channel frequency to screen X (same formula as the scale loop above)
    int16_t chX = ((int)(scanChannels.freq[ch] - centerFreq) / 10 + 20) * 8 - offset;
    if(chX < 2 || chX >= 318) continue;

    bool selected = (scanChannelIdx >= 0 && ch == scanChannelIdx);
    uint16_t color = selected ? TH.scale_pointer : TH.band_text;

    // Vertical line from graph baseline up to the marker head
    spr.drawLine(chX, 130, chX, 169, color);

    if(selected)
    {
      // Downward-pointing triangle for the currently-selected channel
      spr.fillTriangle(chX - 3, 124, chX + 3, 124, chX, 130, color);
    }
    else
    {
      // Small square tick for unselected channels
      spr.fillRect(chX - 1, 126, 3, 4, color);
    }
  }

  // Scale pointer showing the current tuned position
  spr.fillTriangle(156, 125, 160, 130, 164, 125, TH.scale_pointer);
  spr.drawLine(160, 130, 160, 169, TH.scale_pointer);
}

//
// Render Wifi Networks Management Screen
//
void drawWifiNetworksScreen()
{
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.menu_hdr, TH.menu_bg);
  spr.drawString("Select Wi-Fi Network", 10, 8, 4);

  spr.drawLine(10, 32, 310, 32, TH.menu_border);

  if (wifiScanning && discoveredCount == 0)
  {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(TH.menu_item);
    spr.drawString("Scanning for networks...", 160, 90, 4);
    return;
  }

  int startIdx = 0;
  if (wifiNetIdx >= 5)
  {
    startIdx = wifiNetIdx - 4;
  }

  int displayCount = discoveredCount - startIdx;
  if (displayCount > 5) displayCount = 5;

  if (discoveredCount == 0)
  {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(TH.menu_item);
    spr.drawString("No networks found", 160, 90, 4);
  }
  else
  {
    for (int i = 0; i < displayCount; i++)
    {
      int netIndex = startIdx + i;
      int yPos = 38 + i * 24;

      if (netIndex == wifiNetIdx)
      {
        spr.fillSmoothRoundRect(8, yPos - 2, 304, 22, 4, TH.menu_hl_bg);
        spr.setTextColor(TH.menu_hl_text);
      }
      else
      {
        spr.setTextColor(TH.menu_item);
      }

      spr.setTextDatum(ML_DATUM);
      String ssidName = String(discoveredNets[netIndex].ssid);
      if (ssidName.length() > 22) ssidName = ssidName.substring(0, 20) + "..";
      spr.drawString(ssidName, 15, yPos + 8, 2);

      bool isConnected = (WiFi.status() == WL_CONNECTED) && (WiFi.SSID() == discoveredNets[netIndex].ssid);
      if (isConnected)
      {
        spr.setTextColor(netIndex == wifiNetIdx ? TH.menu_hl_text : TH.menu_param);
        spr.drawString("[Connected]", 170, yPos + 8, 2);
        spr.setTextColor(netIndex == wifiNetIdx ? TH.menu_hl_text : TH.menu_item);
      }

      if (discoveredNets[netIndex].isSecure)
      {
        spr.drawCircle(265, yPos + 6, 3, netIndex == wifiNetIdx ? TH.menu_hl_text : TH.menu_item);
        spr.fillRect(262, yPos + 8, 7, 6, netIndex == wifiNetIdx ? TH.menu_hl_text : TH.menu_item);
      }

      int rssi = discoveredNets[netIndex].rssi;
      int bars = 1;
      if (rssi >= -60) bars = 4;
      else if (rssi >= -70) bars = 3;
      else if (rssi >= -80) bars = 2;

      uint16_t barColor = netIndex == wifiNetIdx ? TH.menu_hl_text : TH.menu_item;
      for (int b = 0; b < 4; b++)
      {
        int barH = 4 + b * 3;
        uint16_t color = (b < bars) ? barColor : TH.menu_bg;
        if (color == TH.menu_bg && netIndex == wifiNetIdx) color = TH.menu_hl_bg;
        if (b < bars)
        {
          spr.fillRect(280 + b * 5, yPos + 14 - barH, 3, barH, color);
        }
        else
        {
          spr.drawRect(280 + b * 5, yPos + 14 - barH, 3, barH, netIndex == wifiNetIdx ? TH.menu_hl_text : TH.menu_border);
        }
      }
    }
  }

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.menu_item);
  spr.drawString("[Press & Hold Button to Exit]", 160, 160, 2);
}

//
// Render Virtual Keyboard Entry Screen
//
void drawWifiKeyboardScreen()
{
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.menu_hdr, TH.menu_bg);
  spr.drawString("Enter Wi-Fi Password", 10, 6, 4);

  spr.setTextColor(TH.menu_item);
  spr.drawString("SSID: " + selectedSSID, 10, 32, 2);

  spr.drawRect(10, 48, 300, 22, TH.box_border);
  spr.fillRect(11, 49, 298, 20, TH.box_bg);

  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(TH.box_text);
  String displayPass = wifiPassword + "_";
  if (displayPass.length() > 30) displayPass = ".." + displayPass.substring(displayPass.length() - 28);
  spr.drawString(displayPass, 15, 59, 2);

  int kw = 24;
  int kh = 16;
  int startX = 4;
  int startY = 72;

  const char keyboardChars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_.";

  for (int r = 0; r < 5; r++)
  {
    for (int c = 0; c < 13; c++)
    {
      int idx = r * 13 + c;
      int x = startX + c * kw;
      int y = startY + r * kh;

      bool isSelected = (idx == keyboardIndex);

      if (isSelected)
      {
        spr.fillRect(x + 1, y + 1, kw - 2, kh - 2, TH.menu_hl_bg);
        spr.setTextColor(TH.menu_hl_text);
      }
      else
      {
        spr.drawRect(x, y, kw, kh, TH.menu_border);
        spr.setTextColor(TH.menu_item);
      }

      spr.setTextDatum(MC_DATUM);
      char label[2] = { keyboardChars[idx], '\0' };
      spr.drawString(label, x + kw / 2, y + kh / 2 + 1, 2);
    }
  }

  int rowY = startY + 5 * kh;

  int activeCol = keyboardIndex % 13;
  bool isRow5Selected = (keyboardIndex >= 65);

  auto drawActionButton = [&](int colStart, int colSpan, const char* label, int actionId) {
    int x = startX + colStart * kw;
    int w = colSpan * kw;
    bool isHighlighted = isRow5Selected && (activeCol >= colStart && activeCol < colStart + colSpan);

    if (isHighlighted)
    {
      spr.fillRect(x + 1, rowY + 1, w - 2, kh - 2, TH.menu_hl_bg);
      spr.setTextColor(TH.menu_hl_text);
    }
    else
    {
      spr.drawRect(x, rowY, w, kh, TH.menu_border);
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(label, x + w / 2, rowY + kh / 2 + 1, 2);
  };

  drawActionButton(0, 3, "Space", 0);
  drawActionButton(3, 3, "Del", 1);
  drawActionButton(6, 3, "Cancel", 2);
  drawActionButton(9, 4, "Connect", 3);
}

void drawWifiConnectingScreen()
{
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.menu_hdr, TH.menu_bg);
  spr.drawString("Wi-Fi Connection", 10, 8, 4);

  spr.drawLine(10, 32, 310, 32, TH.menu_border);

  spr.setTextDatum(MC_DATUM);

  if (wifiConnectStatus == 1) // Connecting
  {
    spr.setTextColor(TH.menu_item);
    spr.drawString("Connecting to:", 160, 60, 4);
    spr.setTextColor(TH.menu_param);
    spr.drawString(connectingSSID, 160, 95, 4);

    // Draw simple animated indicator or progress dot
    static int dotCount = 0;
    static uint32_t lastDotUpdate = 0;
    if (millis() - lastDotUpdate > 400)
    {
      dotCount = (dotCount + 1) % 4;
      lastDotUpdate = millis();
    }
    String dots = "";
    for (int d = 0; d < dotCount; d++) dots += ".";
    spr.setTextColor(TH.menu_item);
    spr.drawString(dots, 160, 130, 4);
  }
  else if (wifiConnectStatus == 2) // Success
  {
    spr.setTextColor(TH.menu_hl_text);
    spr.drawString("Successful!", 160, 75, 4);
    spr.setTextColor(TH.menu_item);
    spr.drawString("Connected to network.", 160, 115, 2);
  }
  else if (wifiConnectStatus == 3) // Failed
  {
    spr.setTextColor(TH.menu_item);
    spr.drawString("Connection Failed", 160, 75, 4);
    spr.setTextColor(TH.menu_param);
    spr.drawString("Check password / settings", 160, 105, 2);
    spr.setTextColor(TH.menu_hl_text);
    spr.drawString("[Click Encoder to Return]", 160, 140, 2);
  }
}

void drawWifiStatusScreen()
{
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.menu_hdr, TH.menu_bg);
  spr.drawString("Wi-Fi Status", 10, 8, 4);

  spr.drawLine(10, 32, 310, 32, TH.menu_border);

  spr.setTextColor(TH.menu_item);
  spr.setTextDatum(TL_DATUM);

  wl_status_t status = WiFi.status();
  String connState = "Disconnected";
  uint16_t stateColor = TH.menu_param;

  if (status == WL_CONNECTED)
  {
    connState = internetConnected ? "Connected (Internet OK)" : "Connected (No Internet)";
    stateColor = internetConnected ? TH.menu_hl_text : 0xFFE0; // Yellow
  }
  else if (status == WL_IDLE_STATUS)
  {
    connState = "Idle / Transitioning";
  }
  else if (status == WL_NO_SSID_AVAIL)
  {
    connState = "SSID Not Found";
    stateColor = TH.batt_low;
  }
  else if (status == WL_CONNECT_FAILED)
  {
    connState = "Connection Failed";
    stateColor = TH.batt_low;
  }
  else if (status == WL_CONNECTION_LOST)
  {
    connState = "Connection Lost";
    stateColor = TH.batt_low;
  }

  spr.drawString("State:", 15, 45, 2);
  spr.setTextColor(stateColor);
  spr.drawString(connState, 110, 45, 2);

  spr.setTextColor(TH.menu_item);
  spr.drawString("Network:", 15, 70, 2);
  spr.setTextColor(TH.menu_param);
  String ssid = (status == WL_CONNECTED) ? WiFi.SSID() : "None";
  spr.drawString(ssid, 110, 70, 2);

  spr.setTextColor(TH.menu_item);
  spr.drawString("IP Address:", 15, 95, 2);
  spr.setTextColor(TH.menu_param);
  String ipAddr = (status == WL_CONNECTED) ? WiFi.localIP().toString() : "0.0.0.0";
  spr.drawString(ipAddr, 110, 95, 2);

  spr.setTextColor(TH.menu_item);
  spr.drawString("Signal:", 15, 120, 2);
  spr.setTextColor(TH.menu_param);
  String signalStr = "0%";
  if (status == WL_CONNECTED)
  {
    int pct = (cachedWiFiRSSI <= -100) ? 0 : (cachedWiFiRSSI >= -50) ? 100 : (cachedWiFiRSSI + 100) * 2;
    signalStr = String(pct) + "% (" + String(cachedWiFiRSSI) + " dBm)";
  }
  spr.drawString(signalStr, 110, 120, 2);

  spr.setTextColor(TH.menu_item);
  spr.setTextDatum(MC_DATUM);
  spr.drawString("[Click Encoder to Return]", 160, 160, 2);
}

//
// Render Scrollable EiBi Database Browser Screen
//
void drawEibiBrowseScreen()
{
  // Title
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.menu_hdr, TH.menu_bg);
  spr.drawString("EiBi Database", 10, 8, 4);

  spr.drawLine(10, 32, 310, 32, TH.menu_border);

  // Column headers
  spr.setTextColor(TH.menu_border);
  spr.setTextDatum(TL_DATUM);
  spr.drawString("kHz", 10, 35, 1);
  spr.drawString("Station", 68, 35, 1);
  spr.setTextDatum(TR_DATUM);
  spr.drawString("Time", 310, 35, 1);

  int totalEntries = eibiGetCount();
  if (totalEntries == 0)
  {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(TH.menu_item);
    spr.drawString("No EiBi data", 160, 90, 4);
    spr.drawString("Download from Settings", 160, 115, 2);
    spr.drawString("[Long Press to Exit]", 160, 158, 2);
    return;
  }

  // Clamp browse index
  if (eibiBrowseIdx >= totalEntries) eibiBrowseIdx = totalEntries - 1;
  if (eibiBrowseIdx < 0) eibiBrowseIdx = 0;

  // Calculate visible window (5 entries)
  int maxVisible = 5;
  int startIdx = 0;
  if (eibiBrowseIdx >= maxVisible)
  {
    startIdx = eibiBrowseIdx - maxVisible + 1;
  }

  // Read entries from file
  StationSchedule entries[5];
  int readCount = eibiReadEntries(startIdx, entries, maxVisible);

  for (int i = 0; i < readCount; i++)
  {
    int entryIdx = startIdx + i;
    int yPos = 46 + i * 22;

    if (entryIdx == eibiBrowseIdx)
    {
      spr.fillSmoothRoundRect(5, yPos - 2, 310, 20, 4, TH.menu_hl_bg);
      spr.setTextColor(TH.menu_hl_text);
    }
    else
    {
      spr.setTextColor(TH.menu_item);
    }

    // Frequency (kHz)
    char freqStr[10];
    sprintf(freqStr, "%d", entries[i].freq);
    spr.setTextDatum(TL_DATUM);
    spr.drawString(freqStr, 10, yPos + 2, 2);

    // Station name (truncate to fit)
    char nameStr[24];
    strncpy(nameStr, entries[i].name, 20);
    nameStr[20] = '\0';
    if (strlen(entries[i].name) > 20)
    {
      nameStr[18] = '.';
      nameStr[19] = '.';
    }
    spr.drawString(nameStr, 68, yPos + 2, 2);

    // Time window
    char timeStr[16];
    if (entries[i].start_h < 0)
    {
      strcpy(timeStr, "24hr");
    }
    else
    {
      sprintf(timeStr, "%02d:%02d-%02d:%02d",
        entries[i].start_h, entries[i].start_m,
        entries[i].end_h, entries[i].end_m);
    }
    spr.setTextDatum(TR_DATUM);
    spr.drawString(timeStr, 310, yPos + 2, 2);
  }

  // Scroll position indicator
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.menu_border);
  char posStr[16];
  sprintf(posStr, "%d / %d", eibiBrowseIdx + 1, totalEntries);
  spr.drawString(posStr, 270, 158, 1);

  // Footer
  spr.setTextColor(TH.menu_item);
  spr.drawString("[Long Press to Exit]", 130, 158, 2);
}

//
// Draw screen according to given command
//
void drawScreen(const char *statusLine1, const char *statusLine2)
{
  if(sleepOn()) return;

  // Clear screen buffer
  spr.fillSprite(TH.bg);

  // About screen is a special case
  if(currentCmd==CMD_ABOUT)
  {
    drawAbout();
    return;
  }

  if(currentCmd==CMD_WIFI_NETWORKS)
  {
    spr.fillSprite(TH.menu_bg);
    drawWifiNetworksScreen();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd==CMD_WIFI_KEYBOARD)
  {
    spr.fillSprite(TH.menu_bg);
    drawWifiKeyboardScreen();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd==CMD_WIFI_CONNECTING || currentCmd==CMD_WIFI_CONNECT_FAILED)
  {
    spr.fillSprite(TH.menu_bg);
    drawWifiConnectingScreen();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd==CMD_WIFI_STATUS)
  {
    spr.fillSprite(TH.menu_bg);
    drawWifiStatusScreen();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd==CMD_EIBI_BROWSE)
  {
    spr.fillSprite(TH.menu_bg);
    drawEibiBrowseScreen();
    spr.pushSprite(0, 0);
    return;
  }

  drawLayoutDefault(statusLine1, statusLine2);

  spr.pushSprite(0, 0);
}
