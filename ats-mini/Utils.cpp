#include "driver/rtc_io.h"
#include "driver/gpio.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "Common.h"
#include "Ble.h"
#include "Themes.h"
#include "Button.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"

// SSB patch for whole SSBRX initialization string
#include "patch_init.h"

extern ButtonTracker pb1;

// Current sleep status, returned by sleepOn()
static bool sleep_on = false;

// Current SSB patch status
static bool ssbLoaded = false;

// Time
static bool clockHasBeenSet = false;
static uint32_t clockTimer  = 0;
static uint8_t clockSeconds = 0;
static uint8_t clockMinutes = 0;
static uint8_t clockHours   = 0;
static char    clockText[8] = {0};

//
// Get firmware version and build time, as a string
//
const char *getVersion(bool shorter)
{
  static char versionString[35] = "\0";

  sprintf(versionString, "%s%sF/W: v%1.1d.%2.2d %s",
    shorter ? "" : RECEIVER_NAME,
    shorter ? "" : " ",
    VER_APP / 100,
    VER_APP % 100,
    __DATE__
  );

  return(versionString);
}

//
// Get MAC address
//
const char *getMACAddress()
{
  static char macString[20] = "\0";

  if(!macString[0])
  {
    uint64_t mac = ESP.getEfuseMac();
    sprintf(
      macString,
      "%02X:%02X:%02X:%02X:%02X:%02X",
      (uint8_t)mac,
      (uint8_t)(mac >> 8),
      (uint8_t)(mac >> 16),
      (uint8_t)(mac >> 24),
      (uint8_t)(mac >> 32),
      (uint8_t)(mac >> 40)
    );
  }
  return(macString);
}

//
// Load SSB patch into SI4735
//
void loadSSB(uint8_t bandwidth, bool draw)
{
  if(!ssbLoaded)
  {
    if(draw) drawMessage("Loading SSB");
    rx.loadPatch(ssb_patch_content, sizeof(ssb_patch_content), bandwidth);
    ssbLoaded = true;
  }
}

void unloadSSB()
{
  // Just mark SSB patch as unloaded
  ssbLoaded = false;
}

//
// Mute sound on (x=1) or off (x=0), or get current status (x=2)
// Do not call this too often because a short PIN_AMP_EN impulse can trigger amplifier mode D,
// see the NS4160 datasheet https://esp32-si4732.github.io/ats-mini/hardware.html#datasheets
//
bool muteOn(uint8_t mode, int x)
{
  // Current mute status
  static bool muted = false;

  // Current squelch status
  static bool squelched = false;

  // Effective mute status
  static bool status = false;

  bool unmute = false;
  bool mute = false;

  if(x==1) {
    status = true;
    switch(mode) {
    case MUTE_FORCE:
      mute = true;
      break;
    case MUTE_MAIN:
      if(!muted && !squelched) {
        mute = true;
      }
      muted = true;
      break;
    case MUTE_SQUELCH:
      if(!muted && !squelched) {
        mute = true;
      }
      squelched = true;
      break;
    case MUTE_TEMP:
      if(!muted && !squelched) {
        mute = true;
      }
      break;
    }
  } else if(x==0) {
    status = false;
    switch(mode) {
    case MUTE_FORCE:
      unmute = true;
      break;
    case MUTE_MAIN:
      if(muted && !squelched) {
        unmute = true;
      }
      muted = false;
      break;
    case MUTE_SQUELCH:
      if(!muted && squelched) {
        unmute = true;
      }
      squelched = false;
      break;
    case MUTE_TEMP:
      if(!muted && !squelched) {
        unmute = true;
      }
      break;
    }
  }

  if(mute) {
    // Disable audio amplifier to silence speaker
    digitalWrite(PIN_AMP_EN, LOW);
    // Activate the mute circuit
    digitalWrite(AUDIO_MUTE, HIGH);
    delay(50);
    rx.setAudioMute(true);
  }

  if(unmute) {
    // Deactivate the mute circuit
    digitalWrite(AUDIO_MUTE, LOW);
    delay(50);
    rx.setAudioMute(false);
    // Enable audio amplifier to restore speaker output
    digitalWrite(PIN_AMP_EN, HIGH);
  }

  switch(mode) {
  case MUTE_MAIN:
    return muted;
  case MUTE_SQUELCH:
    return squelched;
  case MUTE_FORCE:
  case MUTE_TEMP:
  default:
    return status;
  }
}

//
// Turn sleep on (1) or off (0), or get current status (2)
//
bool sleepOn(int x)
{
  if((x==1) && !sleep_on)
  {
    sleep_on = true;
    ledcWrite(PIN_LCD_BL, 0);
    spr.fillSprite(TFT_BLACK);
    spr.pushSprite(0, 0);
    tft.writecommand(ST7789_DISPOFF);
    tft.writecommand(ST7789_SLPIN);

    // Wait till the button is released to prevent immediate wakeup
    while(pb1.update(digitalRead(ENCODER_PUSH_BUTTON) == LOW).isPressed)
      delay(100);

    if(sleepModeIdx == SLEEP_LIGHT)
    {
      // BLE-friendly light sleep: instead of calling esp_light_sleep_start()
      // in a blocking loop (which kills the BLE controller), enable the IDF
      // power-management framework. The FreeRTOS idle hook then drops the
      // CPU into light sleep between BLE/timer events automatically, so an
      // already-connected iOS client stays connected and advertising
      // continues throughout sleep.
      //
      // The min freq must be 40 MHz (XTAL). The max freq is whatever the
      // user picked in Settings -> CPU Freq — clamped against 80 MHz lower
      // bound because the radio needs PLL-rate I2C.
      const int cpuFreqValues[] = {80, 160, 240};
      int maxFreq = (cpuFreqIdx < 3) ? cpuFreqValues[cpuFreqIdx] : 80;

      esp_pm_config_t pmCfg = {
        .max_freq_mhz       = maxFreq,
        .min_freq_mhz       = 40,
        .light_sleep_enable = true,
      };
      esp_err_t pmErr = esp_pm_configure(&pmCfg);
      if(pmErr != ESP_OK) {
        Serial.printf("esp_pm_configure failed: %d, falling back to manual sleep\n", pmErr);
      }

      // Enable GPIO wakeup for the encoder button (works with PM auto-sleep,
      // unlike ext0 which only fires on a manual esp_light_sleep_start()).
      gpio_wakeup_enable((gpio_num_t)ENCODER_PUSH_BUTTON, GPIO_INTR_LOW_LEVEL);
      esp_sleep_enable_gpio_wakeup();

      // Unmute squelch (the speaker amp stays powered so audio still plays
      // when waking briefly for BLE events, but squelch can stay disabled).
      if(muteOn(MUTE_SQUELCH) && !muteOn(MUTE_MAIN)) muteOn(MUTE_FORCE, false);

      // Wait for the user to long-press the encoder to exit sleep.
      // The CPU dips in and out of light sleep between iterations thanks to
      // automatic light-sleep PM; this loop is mostly idle.
      pb1.reset();
      bool wasLongPressed = false;
      while(!wasLongPressed)
      {
        // Short click exits sleep when timeout is enabled (legacy behaviour)
        ButtonTracker::State pb1st = pb1.update(digitalRead(ENCODER_PUSH_BUTTON) == LOW, 0);
        if(currentSleep && pb1st.isPressed)
        {
          // Drain the press so the wake doesn't re-trigger a menu click
          while(pb1.update(digitalRead(ENCODER_PUSH_BUTTON) == LOW, 0).isPressed)
            delay(50);
          break;
        }
        wasLongPressed = pb1st.isLongPressed;
        // Service BLE while sleeping — the PM idle hook puts us back to
        // sleep between events, so this stays power-frugal.
        delay(50);
      }

      // Tear down PM auto-sleep
      gpio_wakeup_disable((gpio_num_t)ENCODER_PUSH_BUTTON);
      esp_pm_config_t pmOff = {
        .max_freq_mhz       = maxFreq,
        .min_freq_mhz       = maxFreq,
        .light_sleep_enable = false,
      };
      esp_pm_configure(&pmOff);

      pinMode(ENCODER_PUSH_BUTTON, INPUT_PULLUP);
      if(muteOn(MUTE_SQUELCH) && !muteOn(MUTE_MAIN)) muteOn(MUTE_FORCE, true);

      // Restore CPU frequency to whatever the user picked (PM may have
      // settled it at min during sleep)
      setCpuFrequencyMhz(maxFreq);

      sleepOn(false);
      // BLE and WiFi stayed alive — nothing to restart.
    }
  }
  else if((x==0) && sleep_on)
  {
    sleep_on = false;
    tft.writecommand(ST7789_SLPOUT);
    delay(120);
    tft.writecommand(ST7789_DISPON);
    drawScreen();
    ledcWrite(PIN_LCD_BL, currentBrt);
    // Wait till the button is released to prevent the main loop clicks
    pb1.reset(); // Reset the button state (its timers could be stale due to CPU sleep)
    while(pb1.update(digitalRead(ENCODER_PUSH_BUTTON) == LOW, 0).isPressed)
      delay(100);
  }

  return(sleep_on);
}

//
// Set and count time
//

bool clockAvailable()
{
  return(clockHasBeenSet);
}

const char *clockGet()
{
  if(switchThemeEditor())
    return("00:00");
  else
    return(clockHasBeenSet? clockText : NULL);
}

bool clockGetHM(uint8_t *hours, uint8_t *minutes)
{
  if(!clockHasBeenSet) return(false);
  else
  {
    *hours   = clockHours;
    *minutes = clockMinutes;
    return(true);
  }
}

void clockReset()
{
  clockHasBeenSet = false;
  clockText[0] = '\0';
  clockTimer = 0;
  clockHours = clockMinutes = clockSeconds = 0;
}

static void formatClock(uint8_t hours, uint8_t minutes)
{
  int t = (int)hours * 60 + minutes + getCurrentUTCOffset() * 15;
  t = t < 0? t + 24*60 : t;
  sprintf(clockText, "%02d:%02d", (t / 60) % 24, t % 60);
}

void clockRefreshTime()
{
  if(clockHasBeenSet) formatClock(clockHours, clockMinutes);
}

bool clockSet(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
  // Verify input before setting clock
  if(hours < 24 && minutes < 60 && seconds < 60)
  {
    clockHasBeenSet = true;
    clockTimer   = micros();
    clockHours   = hours;
    clockMinutes = minutes;
    clockSeconds = seconds;
    clockRefreshTime();
    identifyFrequency(currentFrequency + currentBFO / 1000);
    return(true);
  }

  // Failed
  return(false);
}

bool clockTickTime()
{
  // Need to set the clock first, then accumulate one second of time
  if(clockHasBeenSet && (micros() - clockTimer >= 1000000))
  {
    uint32_t delta;

    delta = (micros() - clockTimer) / 1000000;
    clockTimer += delta * 1000000;
    clockSeconds += delta;

    if(clockSeconds>=60)
    {
      delta = clockSeconds / 60;
      clockSeconds -= delta * 60;
      clockMinutes += delta;

      if(clockMinutes>=60)
      {
        delta = clockMinutes / 60;
        clockMinutes -= delta * 60;
        clockHours = (clockHours + delta) % 24;
      }

      // Format clock for display and ask for screen update
      clockRefreshTime();
      return(true);
    }
  }

  // No screen update
  return(false);
}

//
// Check if given frequency belongs to given band
//
bool isFreqInBand(const Band *band, uint16_t freq)
{
  return((freq>=band->minimumFreq) && (freq<=band->maximumFreq));
}

//
// Convert a frequency from Hz to mode-specific units
// (TODO: use Hz across the whole codebase)
//
uint16_t freqFromHz(uint32_t freq, uint8_t mode)
{
  return(mode == FM ? freq / 10000 : freq / 1000);
}

//
// Convert a frequency from mode-specific units to Hz
//
uint32_t freqToHz(uint16_t freq, uint8_t mode)
{
  return(mode == FM ? freq * 10000 : freq * 1000);
}

//
// Extract BFO from a frequency in Hz
//
uint16_t bfoFromHz(uint32_t freq)
{
  return(freq % 1000);
}

//
// Check if given memory entry belongs to given band
//
bool isMemoryInBand(const Band *band, const Memory *memory)
{
  uint16_t freq = freqFromHz(memory->freq, memory->mode);
  if(freq<band->minimumFreq) return(false);
  if(freq>band->maximumFreq) return(false);
  if(freq==band->maximumFreq && bfoFromHz(memory->freq)) return(false);
  if(memory->mode==FM && band->bandMode!=FM) return(false);
  if(memory->mode!=FM && band->bandMode==FM) return(false);
  return(true);
}

//
// Get S-level signal strength from RSSI value
//
int getStrength(int rssi)
{
  if(switchThemeEditor()) return(17);

  if(currentMode!=FM)
  {
    // dBuV to S point conversion HF
    if (rssi <=  1) return  1; // S0
    if (rssi <=  2) return  2; // S1
    if (rssi <=  3) return  3; // S2
    if (rssi <=  4) return  4; // S3
    if (rssi <= 10) return  5; // S4
    if (rssi <= 16) return  6; // S5
    if (rssi <= 22) return  7; // S6
    if (rssi <= 28) return  8; // S7
    if (rssi <= 34) return  9; // S8
    if (rssi <= 44) return 10; // S9
    if (rssi <= 54) return 11; // S9 +10
    if (rssi <= 64) return 12; // S9 +20
    if (rssi <= 74) return 13; // S9 +30
    if (rssi <= 84) return 14; // S9 +40
    if (rssi <= 94) return 15; // S9 +50
    if (rssi <= 95) return 16; // S9 +60
    return                 17; //>S9 +60
  }
  else
  {
    // dBuV to S point conversion FM
    if (rssi <=  1) return  1; // S0
    if (rssi <=  2) return  7; // S6
    if (rssi <=  8) return  8; // S7
    if (rssi <= 14) return  9; // S8
    if (rssi <= 24) return 10; // S9
    if (rssi <= 34) return 11; // S9 +10
    if (rssi <= 44) return 12; // S9 +20
    if (rssi <= 54) return 13; // S9 +30
    if (rssi <= 64) return 14; // S9 +40
    if (rssi <= 74) return 15; // S9 +50
    if (rssi <= 76) return 16; // S9 +60
    return                 17; //>S9 +60
  }
}
