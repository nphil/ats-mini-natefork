#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"

void drawLayoutDefault(const char *statusLine1, const char *statusLine2)
{
  // Draw preferences write request icon
  drawSaveIndicator(SAVE_OFFSET_X, SAVE_OFFSET_Y);

  // Draw BLE icon
  drawBleIndicator(BLE_OFFSET_X, BLE_OFFSET_Y);

  // Draw battery indicator & voltage
  bool has_voltage = drawBattery(BATT_OFFSET_X, BATT_OFFSET_Y);

  // Draw WiFi icon
  drawWiFiIndicator(has_voltage ? WIFI_OFFSET_X : BATT_OFFSET_X - 13, WIFI_OFFSET_Y);

  // Draw CPU load bars (two 30-px bars, to the left of the WiFi icon)
  if (cpuDisplayIdx)
    drawCpuBars(has_voltage ? WIFI_OFFSET_X - 45 : BATT_OFFSET_X - 13 - 45, 2);

  // Set font we are going to use
  spr.setFreeFont(&Orbitron_Light_24);

  // Draw band and mode
  drawBandAndMode(
    getCurrentBand()->bandName,
    bandModeDesc[currentMode],
    BAND_OFFSET_X, BAND_OFFSET_Y
  );

  if(switchThemeEditor())
  {
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(TH.text_warn);
    spr.drawString(TH.name, 319, BATT_OFFSET_Y + 17, 2);
  }

  // Draw frequency, units, and optionally highlight a digit
  drawFrequency(
    currentFrequency,
    FREQ_OFFSET_X, FREQ_OFFSET_Y,
    FUNIT_OFFSET_X, FUNIT_OFFSET_Y,
    currentCmd == CMD_FREQ ? getFreqInputPos() + (pushAndRotate ? 0x80 : 0) : 100
  );

  // Show station name (PS) and/or PTY combined on one line
  {
    const char *ps  = getStationName();
    const char *pty = getProgramInfo();
    if(*ps == 0xFF)
      drawLongStationName(ps + 1, MENU_OFFSET_X + 1 + 76 + MENU_DELTA_X + 2, RDS_OFFSET_Y);
    else if(*ps || *pty)
    {
      char combined[32] = "";
      if(*ps && *pty)
        snprintf(combined, sizeof(combined), "%s | %s", ps, pty);
      else if(*ps)
        snprintf(combined, sizeof(combined), "%s", ps);
      else
        snprintf(combined, sizeof(combined), "%s", pty);
      drawStationName(combined, RDS_OFFSET_X, RDS_OFFSET_Y);
    }
  }

  // Draw left-side menu/info bar
  // @@@ FIXME: Frequency display (above) intersects the side bar!
  drawSideBar(currentCmd, MENU_OFFSET_X, MENU_OFFSET_Y, MENU_DELTA_X);

  // Draw S-meter
  drawSMeter(getStrength(rssi), METER_OFFSET_X, METER_OFFSET_Y);

  // Indicate FM pilot detection (stereo indicator)
  drawStereoIndicator(METER_OFFSET_X, METER_OFFSET_Y, (currentMode==FM) && rx.getCurrentPilot());

  if(currentCmd == CMD_SCAN)
  {
    drawScanGraphs(isSSB()? (currentFrequency + currentBFO/1000) : currentFrequency);
  }
  else if(!drawWiFiStatus(statusLine1, statusLine2, STATUS_OFFSET_X, STATUS_OFFSET_Y))
  {
    // Always show the frequency scale
    drawScale(isSSB()? (currentFrequency + currentBFO/1000) : currentFrequency);
  }
}
