#include "Common.h"
#include "Utils.h"
#include "Menu.h"
#include "Themes.h"

// Tuning delays after rx.setFrequency()
#define TUNE_DELAY_DEFAULT 30
#define TUNE_DELAY_FM      100
#define TUNE_DELAY_AM_SSB  80

#define SCAN_POLL_TIME    10 // Tuning status polling interval (msecs)
#define SCAN_POINTS      200 // Number of frequencies to scan

#define SCAN_OFF    0   // Scanner off, no data
#define SCAN_RUN    1   // Scanner running
#define SCAN_DONE   2   // Scanner done, valid data in scanData[]

static struct
{
  uint8_t rssi;
  uint8_t snr;
} scanData[SCAN_POINTS];

// Per-band scan channel list and selected index (persisted to NVS)
ScanChannelList scanChannels = {0, {0}};
int8_t scanChannelIdx = -1;

static uint32_t scanTime = millis();
static uint8_t  scanStatus = SCAN_OFF;

static uint16_t scanStartFreq;
static uint16_t scanStep;
static uint16_t scanCount;
static uint8_t  scanMinRSSI;
static uint8_t  scanMaxRSSI;
static uint8_t  scanMinSNR;
static uint8_t  scanMaxSNR;

static inline uint8_t min(uint8_t a, uint8_t b) { return(a<b? a:b); }
static inline uint8_t max(uint8_t a, uint8_t b) { return(a>b? a:b); }

// Raw data accessors used by the BLE JSON protocol
bool     scanIsDone()               { return scanStatus == SCAN_DONE; }
uint16_t scanGetStartFreq()         { return scanStartFreq; }
uint16_t scanGetStep()              { return scanStep; }
uint16_t scanGetCount()             { return scanCount; }
uint8_t  scanGetRawRSSI(uint16_t i) { return (i < scanCount) ? scanData[i].rssi : 0; }
uint8_t  scanGetRawSNR(uint16_t i)  { return (i < scanCount) ? scanData[i].snr  : 0; }

float scanGetRSSI(uint16_t freq)
{
  // Input frequency must be in range of existing data
  if((scanStatus!=SCAN_DONE) || (freq<scanStartFreq) || (freq>=scanStartFreq+scanStep*scanCount))
    return(0.0);

  uint8_t result = scanData[(freq - scanStartFreq) / scanStep].rssi;
  return((result - scanMinRSSI) / (float)(scanMaxRSSI - scanMinRSSI + 1));
}

float scanGetSNR(uint16_t freq)
{
  // Input frequency must be in range of existing data
  if((scanStatus!=SCAN_DONE) || (freq<scanStartFreq) || (freq>=scanStartFreq+scanStep*scanCount))
    return(0.0);

  uint8_t result = scanData[(freq - scanStartFreq) / scanStep].snr;
  return((result - scanMinSNR) / (float)(scanMaxSNR - scanMinSNR + 1));
}

static void scanInit(uint16_t centerFreq, uint16_t step)
{
  scanStep    = step;
  scanCount   = 0;
  scanMinRSSI = 255;
  scanMaxRSSI = 0;
  scanMinSNR  = 255;
  scanMaxSNR  = 0;
  scanStatus  = SCAN_RUN;
  scanTime    = millis();

  const Band *band = getCurrentBand();
  int freq = scanStep * (centerFreq / scanStep - SCAN_POINTS / 2);

  // Adjust to band boundaries
  if(freq + scanStep * (SCAN_POINTS - 1) > band->maximumFreq)
    freq = band->maximumFreq - scanStep * (SCAN_POINTS - 1);
  if(freq < band->minimumFreq)
    freq = band->minimumFreq;
  scanStartFreq = freq;

  // Clear scan data
  memset(scanData, 0, sizeof(scanData));
}

static bool scanTickTime()
{
  // Scan must be on
  if((scanStatus!=SCAN_RUN) || (scanCount>=SCAN_POINTS)) return(false);

  // Wait for the right time
  if(millis() - scanTime < SCAN_POLL_TIME) return(true);

  // This is our current frequency to scan
  uint16_t freq = scanStartFreq + scanStep * scanCount;

  // Poll for the tuning status
  rx.getStatus(0, 0);
  if(!rx.getTuneCompleteTriggered())
  {
    scanTime = millis();
    return(true);
  }

  // If frequency not yet set, set it and wait until next call to measure
  if(rx.getCurrentFrequency() != freq)
  {
    rx.setFrequency(freq); // Implies tuning delay
    scanTime = millis() - SCAN_POLL_TIME;
    return(true);
  }

  // Measure RSSI/SNR values
  rx.getCurrentReceivedSignalQuality();
  scanData[scanCount].rssi = rx.getCurrentRSSI();
  scanData[scanCount].snr  = rx.getCurrentSNR();

  // Measure range of values
  scanMinRSSI = min(scanData[scanCount].rssi, scanMinRSSI);
  scanMaxRSSI = max(scanData[scanCount].rssi, scanMaxRSSI);
  scanMinSNR  = min(scanData[scanCount].snr, scanMinSNR);
  scanMaxSNR  = max(scanData[scanCount].snr, scanMaxSNR);

  // Next frequency to scan
  freq += scanStep;

  // Set next frequency to scan or expire scan
  if((++scanCount >= SCAN_POINTS) || !isFreqInBand(getCurrentBand(), freq) || checkStopSeeking())
    scanStatus = SCAN_DONE;
  else
    rx.setFrequency(freq); // Implies tuning delay

  // Save last scan time
  scanTime = millis() - SCAN_POLL_TIME;

  // Return current scan status
  return(scanStatus==SCAN_RUN);
}

//
// Extract station channels from completed scan data.
// Finds local RSSI peaks above a noise threshold and stores
// their frequencies into the global scanChannels list.
//
void scanExtractChannels()
{
  scanChannels.count = 0;

  if(scanStatus != SCAN_DONE || scanCount < 3) return;

  // Need enough RSSI range to distinguish stations from noise
  uint8_t range = scanMaxRSSI - scanMinRSSI;
  if(range < 4) return;

  // Threshold: 25% above the noise floor
  uint8_t threshold = scanMinRSSI + range / 4;

  for(int i = 1; i < (int)scanCount - 1; i++)
  {
    uint8_t r = scanData[i].rssi;
    if(r <= threshold) continue;

    // Must be a local maximum (strictly higher than both neighbours)
    if(r < scanData[i-1].rssi || r < scanData[i+1].rssi) continue;

    uint16_t freq = scanStartFreq + scanStep * (uint16_t)i;

    // Merge with previous entry if they are fewer than 3 steps apart,
    // keeping the one with the stronger signal
    if(scanChannels.count > 0)
    {
      uint16_t prevFreq = scanChannels.freq[scanChannels.count - 1];
      if((int)freq - (int)prevFreq < (int)scanStep * 3)
      {
        int prevIdx = ((int)prevFreq - (int)scanStartFreq) / (int)scanStep;
        if(prevIdx >= 0 && prevIdx < (int)scanCount && r > scanData[prevIdx].rssi)
          scanChannels.freq[scanChannels.count - 1] = freq;
        continue;
      }
    }

    if(scanChannels.count < SCAN_CH_MAX)
      scanChannels.freq[scanChannels.count++] = freq;
  }
}

//
// Run entire scan once
//
void scanRun(uint16_t centerFreq, uint16_t step, Stream* stream, ScanProgressFn onProgress)
{
  // Set tuning delay
  rx.setMaxDelaySetFrequency(currentMode == FM ? TUNE_DELAY_FM : TUNE_DELAY_AM_SSB);
  // Mute the audio
  muteOn(MUTE_TEMP, true);
  // Flag is set by rotary encoder and cleared on seek/scan entry
  seekStop = false;
  // Save current frequency
  uint16_t curFreq = rx.getFrequency();
  // Scan the whole range, sending progress updates every 10%
  uint8_t lastPct = 255;
  for(scanInit(centerFreq, step); scanTickTime();)
  {
    uint8_t pct = (uint8_t)((uint32_t)scanGetCount() * 100 / SCAN_POINTS);
    if(pct != lastPct && pct % 10 == 0)
    {
      if(stream)
      {
        char pbuf[48];
        int plen = snprintf(pbuf, sizeof(pbuf), "{\"t\":\"scan_prog\",\"pct\":%u}\r\n", pct);
        stream->write((uint8_t*)pbuf, plen);
      }
      if(onProgress) onProgress(pct);
      lastPct = pct;
    }
  }
  // Restore current frequency
  rx.setFrequency(curFreq);
  // Unmute the audio
  muteOn(MUTE_TEMP, false);
  // Restore tuning delay
  rx.setMaxDelaySetFrequency(TUNE_DELAY_DEFAULT);
}

//
// Waterfall display
//
// 256-entry heat color LUT: index 0 = coldest (dark blue), 255 = hottest (bright red).
// Built once on first use; stored in RAM (512 bytes).
//
static uint16_t heatLUT[256];
static bool     heatLUTReady = false;

static void buildHeatLUT()
{
  if (heatLUTReady) return;
  for (int i = 0; i < 256; i++) {
    float v   = i / 255.0f;
    float hue = 240.0f - v * 240.0f; // 240=blue (cold) → 0=red (hot)
    float brt = 0.10f + v * 0.90f;   // 0.10 (dark) → 1.00 (bright)
    heatLUT[i] = hsvToRgb565(hue, 1.0f, brt);
  }
  heatLUTReady = true;
}

// Format a band-unit frequency as a human-readable label.
// FM units are 10 kHz (e.g. 6400 = 64.0 MHz); AM/SSB units are kHz.
static void formatWfFreq(char *buf, size_t bufSize, uint16_t freq)
{
  if (currentMode == FM) {
    snprintf(buf, bufSize, "%.1fM", freq / 100.0f);
  } else if (freq >= 10000) {
    snprintf(buf, bufSize, "%uM",   (unsigned)(freq / 1000));
  } else if (freq >= 1000) {
    snprintf(buf, bufSize, "%.1fM", freq / 1000.0f);
  } else {
    snprintf(buf, bufSize, "%uk",   (unsigned)freq);
  }
}

// Draw the 12-pixel label strip below the waterfall area (rows 158..169).
// Clears the strip and prints min / mid / max frequencies.
static void drawWaterfallLabels(uint16_t bandMin, uint16_t bandMax)
{
  const int WFALL_H = 158;

  spr.fillRect(0, WFALL_H, 320, 170 - WFALL_H, TH.bg);
  spr.drawFastHLine(0, WFALL_H, 320, TH.scale_line);

  char lbl[20];

  spr.setTextColor(TH.scale_text, TH.bg);

  // Min freq — left edge
  formatWfFreq(lbl, sizeof(lbl), bandMin);
  spr.setTextDatum(TL_DATUM);
  spr.drawString(lbl, 2, WFALL_H + 2, 1);

  // Max freq — right edge
  formatWfFreq(lbl, sizeof(lbl), bandMax);
  spr.setTextDatum(TR_DATUM);
  spr.drawString(lbl, 318, WFALL_H + 2, 1);

  // Mid freq — centre
  formatWfFreq(lbl, sizeof(lbl), (uint16_t)((bandMin + bandMax) / 2));
  spr.setTextDatum(TC_DATUM);
  spr.drawString(lbl, 160, WFALL_H + 2, 1);
}

//
// Full-screen scrolling waterfall display.
//
// Maps the current band (min..max frequency) across the 320-pixel display
// width.  On each sweep a new pixel row is written at the bottom; older
// rows scroll upward.  The colour of each pixel encodes the measured RSSI
// using a cold-blue → hot-red heat map.
//
// Performance note: each sweep performs 320 I2C tune+measure cycles at
// TUNE_DELAY_DEFAULT (30 ms) each, giving roughly a 10-second refresh
// interval.  Core 1 CPU load will be high while the waterfall is running.
// This is expected and mirrors the load of scanRun().
//
// Exit: press the encoder button or rotate the encoder.
//
void waterfallRun()
{
  buildHeatLUT();

  const Band    *band   = getCurrentBand();
  const int      W      = 320;   // display width in pixels
  const int      WFALL_H = 158;  // waterfall area height (rows 0..157)
                                  // rows 158..169 = 12-px label strip

  const uint16_t bandMin = band->minimumFreq;
  const uint16_t bandMax = band->maximumFreq;

  // Use the fast default tune delay for all modes (speed > accuracy for waterfall)
  rx.setMaxDelaySetFrequency(TUNE_DELAY_DEFAULT);

  muteOn(MUTE_TEMP, true);
  uint16_t savedFreq = rx.getFrequency();
  seekStop = false;

  // Access the sprite's raw RGB565 pixel buffer
  uint16_t *buf = (uint16_t *)spr.getPointer();

  // Clear the waterfall area to black
  if (buf) {
    memset(buf, 0, (size_t)W * WFALL_H * sizeof(uint16_t));
  }

  // Draw the initial label strip and push to display
  drawWaterfallLabels(bandMin, bandMax);
  spr.pushSprite(0, 0);

  bool running = true;

  while (running) {
    // Scroll waterfall up by one row: move rows 1..(WFALL_H-1) → 0..(WFALL_H-2)
    if (buf) {
      memmove(buf, buf + W, (size_t)W * (WFALL_H - 1) * sizeof(uint16_t));
    }

    // Sweep all 320 columns left-to-right
    for (int x = 0; x < W; x++) {
      // Map column x linearly to the band frequency range
      uint16_t freq = (uint16_t)(bandMin + (uint32_t)x * (bandMax - bandMin) / (W - 1));

      // Tune the radio (blocks for TUNE_DELAY_DEFAULT ms internally)
      rx.setFrequency(freq);

      // Poll for tune-complete (up to 20 ms additional, in 2 ms steps)
      uint32_t t0 = millis();
      while (millis() - t0 < 20) {
        rx.getStatus(0, 0);
        if (rx.getTuneCompleteTriggered()) break;
        delay(2);
      }

      // Read RSSI (0..~120 dBuV from SI4732; scale ×2 to fill the 0-255 LUT)
      rx.getCurrentReceivedSignalQuality();
      uint8_t r = rx.getCurrentRSSI();
      uint8_t idx = (r >= 128) ? 255 : (uint8_t)(r * 2);

      // Write the heat pixel at the bottom row of the waterfall
      if (buf) {
        buf[(WFALL_H - 1) * W + x] = heatLUT[idx];
      }

      // Check for exit: button press or encoder rotation
      if (digitalRead(ENCODER_PUSH_BUTTON) == LOW) {
        while (digitalRead(ENCODER_PUSH_BUTTON) == LOW) delay(10);
        running = false;
        break;
      }
      if (seekStop) { running = false; break; }

      // Refresh display every 8 columns (balances visual feedback vs. SPI overhead)
      if ((x & 7) == 7 || x == W - 1) {
        drawWaterfallLabels(bandMin, bandMax);
        spr.pushSprite(0, 0);
      }
    }
  }

  // Restore radio state
  seekStop = false;
  rx.setFrequency(savedFreq);
  muteOn(MUTE_TEMP, false);
  rx.setMaxDelaySetFrequency(TUNE_DELAY_DEFAULT);
}
