#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"
#include "Remote.h"
#include "Storage.h"

static void bleScanProgress(uint8_t pct)
{
  drawMessageProgress("Scanning...", pct);
}


static uint8_t char2nibble(char key)
{
  if((key >= '0') && (key <= '9')) return(key - '0');
  if((key >= 'A') && (key <= 'F')) return(key - 'A' + 10);
  if((key >= 'a') && (key <= 'f')) return(key - 'a' + 10);
  return(0);
}

//
// Capture current screen image to the remote
//
static void remoteCaptureScreen(Stream* stream)
{
  uint16_t width  = spr.width();
  uint16_t height = spr.height();

  // 14 bytes of BMP header
  stream->println("");
  stream->print("424d"); // BM
  // Image size
  stream->printf("%08x", (unsigned int)htonl(14 + 40 + 12 + width * height * 2));
  stream->print("00000000");
  // Offset to image data
  stream->printf("%08x", (unsigned int)htonl(14 + 40 + 12));
  // Image header
  stream->print("28000000"); // Header size
  stream->printf("%08x", (unsigned int)htonl(width));
  stream->printf("%08x", (unsigned int)htonl(height));
  stream->print("01001000"); // 1 plane, 16 bpp
  stream->print("03000000"); // Compression
  stream->print("00000000"); // Compressed image size
  stream->print("00000000"); // X res
  stream->print("00000000"); // Y res
  stream->print("00000000"); // Color map
  stream->print("00000000"); // Colors
  stream->print("00f80000"); // Red mask
  stream->print("e0070000"); // Green mask
  stream->println("1f000000"); // Blue mask

  // Image data
  for(int y=height-1 ; y>=0 ; y--)
  {
    for(int x=0 ; x<width ; x++)
    {
      stream->printf("%04x", htons(spr.readPixel(x, y)));
    }
    stream->println("");
  }
}

char remoteReadChar(Stream* stream)
{
  char key;

  while (!stream->available());
  key = stream->read();
  stream->print(key);
  return key;
}

long int remoteReadInteger(Stream* stream)
{
  long int result = 0;
  while (true) {
    char ch = stream->peek();
    if (ch == 0xFF) {
      continue;
    } else if ((ch >= '0') && (ch <= '9')) {
      ch = remoteReadChar(stream);
      // Can overflow, but it's ok
      result = result * 10 + (ch - '0');
    } else {
      return result;
    }
  }
}

void remoteReadString(Stream* stream, char *bufStr, uint8_t bufLen)
{
  uint8_t length = 0;
  while (true) {
    char ch = stream->peek();
    if (ch == 0xFF) {
      continue;
    } else if (ch == ',' || ch < ' ') {
      bufStr[length] = '\0';
      return;
    } else {
      ch = remoteReadChar(stream);
      bufStr[length] = ch;
      if (++length >= bufLen - 1) {
        bufStr[length] = '\0';
        return;
      }
    }
  }
}

static bool expectNewline(Stream* stream)
{
  char ch;
  while ((ch = stream->peek()) == 0xFF);
  if (ch == '\r') {
    stream->read();
    return true;
  }
  return false;
}

static bool remoteShowError(Stream* stream, const char *message)
{
  // Consume the remaining input
  while (stream->available()) remoteReadChar(stream);
  stream->printf("\r\nError: %s\r\n", message);
  return false;
}

static void remoteGetMemories(Stream* stream)
{
  for (uint8_t i = 0; i < getTotalMemories(); i++) {
    if (memories[i].freq) {
      stream->printf("#%02d,%s,%ld,%s\r\n", i + 1, bands[memories[i].band].bandName, memories[i].freq, bandModeDesc[memories[i].mode]);
    }
  }
}

static bool remoteSetMemory(Stream* stream)
{
  stream->print('#');
  Memory mem;
  uint32_t freq = 0;

  long int slot = remoteReadInteger(stream);
  if (remoteReadChar(stream) != ',')
    return remoteShowError(stream, "Expected ','");
  if (slot < 1 || slot > getTotalMemories())
    return remoteShowError(stream, "Invalid memory slot number");

  char band[8];
  remoteReadString(stream, band, 8);
  if (remoteReadChar(stream) != ',')
    return remoteShowError(stream, "Expected ','");
  mem.band = 0xFF;
  for (int i = 0; i < getTotalBands(); i++) {
    if (strcmp(bands[i].bandName, band) == 0) {
      mem.band = i;
      break;
    }
  }
  if (mem.band == 0xFF)
    return remoteShowError(stream, "No such band");

  freq = remoteReadInteger(stream);
  if (remoteReadChar(stream) != ',')
    return remoteShowError(stream, "Expected ','");

  char mode[4];
  remoteReadString(stream, mode, 4);
  if (!expectNewline(stream))
    return remoteShowError(stream, "Expected newline");
  stream->println();
  mem.mode = 15;
  for (int i = 0; i < getTotalModes(); i++) {
    if (strcmp(bandModeDesc[i], mode) == 0) {
      mem.mode = i;
      break;
    }
  }
  if (mem.mode == 15)
    return remoteShowError(stream, "No such mode");

  mem.freq = freq;

  if (!isMemoryInBand(&bands[mem.band], &mem)) {
    if (!freq) {
      // Clear slot
      memories[slot-1] = mem;
      return true;
    } else {
      // Handle duplicate band names (15M)
      mem.band = 0xFF;
      for (int i = getTotalBands()-1; i >= 0; i--) {
        if (strcmp(bands[i].bandName, band) == 0) {
          mem.band = i;
          break;
        }
      }
      if (mem.band == 0xFF)
        return remoteShowError(stream, "No such band");
      if (!isMemoryInBand(&bands[mem.band], &mem))
        return remoteShowError(stream, "Invalid frequency or mode");
    }
  }

  memories[slot-1] = mem;
  return true;
}

//
// Set current color theme from the remote
//
static void remoteSetColorTheme(Stream* stream)
{
  stream->print("Enter a string of hex colors (x0001x0002...): ");

  uint8_t *p = (uint8_t *)&(TH.bg);

  for(int i=0 ; ; i+=sizeof(uint16_t))
  {
    if(i >= sizeof(ColorTheme)-offsetof(ColorTheme, bg))
    {
      stream->println(" Ok");
      break;
    }

    if(remoteReadChar(stream) != 'x')
    {
      stream->println(" Err");
      break;
    }

    p[i + 1]  = char2nibble(remoteReadChar(stream)) * 16;
    p[i + 1] |= char2nibble(remoteReadChar(stream));
    p[i]      = char2nibble(remoteReadChar(stream)) * 16;
    p[i]     |= char2nibble(remoteReadChar(stream));
  }

  // Redraw screen
  drawScreen();
}

//
// Print current color theme to the remote
//
static void remoteGetColorTheme(Stream* stream)
{
  stream->printf("Color theme %s: ", TH.name);
  const uint8_t *p = (uint8_t *)&(TH.bg);

  for(int i=0 ; i<sizeof(ColorTheme)-offsetof(ColorTheme, bg) ; i+=sizeof(uint16_t))
  {
    stream->printf("x%02X%02X", p[i+1], p[i]);
  }

  stream->println();
}

//
// Print current status to the remote
//
void remotePrintStatus(Stream* stream, RemoteState* state)
{
  // Prepare information ready to be sent
  float remoteVoltage = batteryMonitor();

  // S-Meter conditional on compile option
  rx.getCurrentReceivedSignalQuality();
  uint8_t remoteRssi = rx.getCurrentRSSI();
  uint8_t remoteSnr = rx.getCurrentSNR();

  // Use rx.getFrequency to force read of capacitor value from SI4732/5
  rx.getFrequency();
  uint16_t tuningCapacitor = rx.getAntennaTuningCapacitor();

  // Remote serial
  stream->printf("%u,%u,%d,%d,%s,%s,%s,%s,%hu,%hu,%hu,%hu,%hu,%.2f,%hu\r\n",
                VER_APP,
                currentFrequency,
                currentBFO,
                ((currentMode == USB) ? getCurrentBand()->usbCal :
                 (currentMode == LSB) ? getCurrentBand()->lsbCal : 0),
                getCurrentBand()->bandName,
                bandModeDesc[currentMode],
                getCurrentStep()->desc,
                getCurrentBandwidth()->desc,
                agcIdx,
                volume,
                remoteRssi,
                remoteSnr,
                tuningCapacitor,
                remoteVoltage,
                state->remoteSeqnum
                );
}

//
// Tick remote time, periodically printing status
//
void remoteTickTime(Stream* stream, RemoteState* state)
{
  uint32_t now = millis();

  // Legacy char-protocol periodic log
  if(state->remoteLogOn && (now - state->remoteTimer >= 500))
  {
    state->remoteTimer = now;
    state->remoteSeqnum++;
    remotePrintStatus(stream, state);
  }

  // JSON subscription: send status at the requested interval
  if(state->jsonSubMs > 0 && (now - state->jsonTimer >= state->jsonSubMs))
  {
    state->jsonTimer = now;
    remoteJsonStatus(stream, state->remoteSeqnum++);
  }
}

// =========================================================
// JSON protocol helpers
// =========================================================

// Extract a string value from a JSON object.
// Returns true and fills val[] on success.
static bool jsonStr(const char *json, const char *key, char *val, int valLen)
{
  char pat[48];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char *p = strstr(json, pat);
  if(!p) return false;
  p += strlen(pat);
  while(*p == ' ') p++;
  if(*p != '"') return false;
  p++;
  int i = 0;
  while(*p && *p != '"' && i < valLen - 1) val[i++] = *p++;
  val[i] = '\0';
  return true;
}

// Extract an integer value from a JSON object.
// Returns true and sets *val on success.
static bool jsonInt(const char *json, const char *key, long *val)
{
  char pat[48];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char *p = strstr(json, pat);
  if(!p) return false;
  p += strlen(pat);
  while(*p == ' ') p++;
  if(*p == '-' || (*p >= '0' && *p <= '9'))
  {
    *val = atol(p);
    return true;
  }
  return false;
}

// Sanitize a string for JSON embedding: replace " with ', \ with /, control chars with space
static void jsonSanitize(char *dst, const char *src, size_t maxlen)
{
  if(!src || !maxlen) { if(dst && maxlen) dst[0] = '\0'; return; }
  size_t i = 0;
  for(; src[i] && i < maxlen - 1; i++) {
    char c = src[i];
    if(c == '"')       c = '\'';
    else if(c == '\\') c = '/';
    else if((uint8_t)c < 0x20 || (uint8_t)c == 0xFF) c = ' ';
    dst[i] = c;
  }
  dst[i] = '\0';
}

// Send a compact JSON status packet.
void remoteJsonStatus(Stream *stream, uint8_t seq)
{
  rx.getCurrentReceivedSignalQuality();
  uint8_t r  = rx.getCurrentRSSI();
  uint8_t sn = rx.getCurrentSNR();
  float   bat = batteryMonitor();
  int16_t cal = (currentMode == USB) ? getCurrentBand()->usbCal :
                (currentMode == LSB) ? getCurrentBand()->lsbCal : 0;

  static char buf[700];
  int n = snprintf(buf, sizeof(buf),
    "{\"t\":\"s\","
    "\"f\":%u,\"bfo\":%d,\"cal\":%d,"
    "\"m\":%u,\"mn\":\"%s\","
    "\"b\":%u,\"bn\":\"%s\","
    "\"st\":\"%s\",\"bw\":\"%s\","
    "\"agc\":%d,\"v\":%u,"
    "\"r\":%u,\"sn\":%u,"
    "\"bat\":%.2f,\"seq\":%u,"
    "\"sch\":%u,\"sci\":%d",
    currentFrequency, currentBFO, cal,
    currentMode, bandModeDesc[currentMode],
    (unsigned)bandIdx, getCurrentBand()->bandName,
    getCurrentStep()->desc, getCurrentBandwidth()->desc,
    agcIdx, volume,
    r, sn,
    bat, seq,
    scanChannels.count, scanChannelIdx);

  // Append RDS fields when in FM mode
  if(n > 0 && currentMode == FM)
  {
    const char *ps  = getRdsPsRaw();
    const char *rt  = getRdsRtRaw();
    const char *pty = getRdsPtyRaw();
    // Strip 0xFF EiBi prefix from station name
    if(ps && (uint8_t)ps[0] == 0xFF) ps++;
    char ps_buf[16]  = "";
    char rt_buf[80]  = "";
    char pty_buf[32] = "";
    jsonSanitize(ps_buf,  ps,  sizeof(ps_buf));
    jsonSanitize(rt_buf,  rt,  sizeof(rt_buf));
    jsonSanitize(pty_buf, pty, sizeof(pty_buf));
    const char *ct = clockGet(); // RDS/NTP time ("HH:MM") or NULL
    int added = snprintf(buf + n, sizeof(buf) - n,
      ",\"ps\":\"%s\",\"rt\":\"%s\",\"pty\":\"%s\"%s%s%s",
      ps_buf, rt_buf, pty_buf,
      ct ? ",\"ct\":\"" : "", ct ? ct : "", ct ? "\"" : "");
    if(added > 0) n += added;
  }

  // Append CPU load fields
  if(n > 0) {
    int added = snprintf(buf + n, sizeof(buf) - n,
      ",\"cpu0\":%u,\"cpu1\":%u",
      (unsigned)getCpuLoad(0), (unsigned)getCpuLoad(1));
    if(added > 0) n += added;
  }

  // Append WiFi info for OTA: wip=best IP to POST firmware to, wm=mode
  if(n > 0) {
    int added = snprintf(buf + n, sizeof(buf) - n,
      ",\"wip\":\"%s\",\"wm\":%d,\"fw\":%u",
      getOTAIPAddress(), (int)getWiFiStatus(), (unsigned)VER_APP);
    if(added > 0) n += added;
  }

  // Close JSON object
  if(n > 0 && n < (int)sizeof(buf) - 3) {
    buf[n++] = '}';
    buf[n++] = '\r';
    buf[n++] = '\n';
  }

  stream->write((uint8_t*)buf, n);
}

// Stream the last completed scan as a JSON object.
// Data is sent in small chunks (~100 bytes each) to stay within BLE MTU limits.
#define SCAN_CHUNK 100
static void remoteJsonScanData(Stream *stream)
{
  if(!scanIsDone())
  {
    stream->print("{\"t\":\"scan\",\"err\":\"no data\"}\r\n");
    return;
  }

  uint16_t n = scanGetCount();
  static char buf[SCAN_CHUNK + 16];
  int pos;

  // Header
  pos = snprintf(buf, sizeof(buf),
    "{\"t\":\"scan\",\"sf\":%u,\"step\":%u,\"n\":%u,\"r\":[",
    scanGetStartFreq(), scanGetStep(), n);
  stream->write((uint8_t*)buf, pos);

  // RSSI array in chunks
  for(uint16_t i = 0; i < n; )
  {
    pos = 0;
    while(i < n && pos < SCAN_CHUNK)
    {
      pos += snprintf(buf + pos, sizeof(buf) - pos, "%s%u", (i > 0) ? "," : "", scanGetRawRSSI(i));
      i++;
    }
    stream->write((uint8_t*)buf, pos);
    delay(5);
  }

  // SNR array
  stream->print("],\"sn\":[");
  for(uint16_t i = 0; i < n; )
  {
    pos = 0;
    while(i < n && pos < SCAN_CHUNK)
    {
      pos += snprintf(buf + pos, sizeof(buf) - pos, "%s%u", (i > 0) ? "," : "", scanGetRawSNR(i));
      i++;
    }
    stream->write((uint8_t*)buf, pos);
    delay(5);
  }

  // Channel list and close
  stream->print("],\"ch\":[");
  for(uint8_t i = 0; i < scanChannels.count; i++)
  {
    pos = snprintf(buf, sizeof(buf), "%s%u", i ? "," : "", scanChannels.freq[i]);
    stream->write((uint8_t*)buf, pos);
  }
  stream->print("]}\r\n");
}

//
// Handle a JSON command string received over BLE (or serial).
// Supports both control commands and data queries.
// Returns a bitmask of REMOTE_* flags identical to remoteDoCommand().
//
int remoteDoJsonCommand(Stream* stream, RemoteState* state, const char* json)
{
  char cmd[24] = {0};
  if(!jsonStr(json, "cmd", cmd, sizeof(cmd))) return 0;

  long val = 0;
  int  event = REMOTE_CHANGED;

  // ---- One-shot status ------------------------------------------------
  if(!strcmp(cmd, "status"))
  {
    remoteJsonStatus(stream, state->remoteSeqnum++);
    return REMOTE_CHANGED;
  }

  // ---- Subscribe: periodic JSON status --------------------------------
  if(!strcmp(cmd, "sub"))
  {
    state->jsonSubMs = jsonInt(json, "ms", &val) ? (uint32_t)max(0L, val) : 0;
    state->jsonTimer = millis();
    remoteJsonStatus(stream, state->remoteSeqnum++);
    return REMOTE_CHANGED;
  }

  // ---- Direct frequency set (kHz) ------------------------------------
  if(!strcmp(cmd, "freq"))
  {
    if(jsonInt(json, "val", &val) && val > 0)
    {
      updateFrequency((int)val, false);
      event |= REMOTE_PREFS;
    }
    remoteJsonStatus(stream, state->remoteSeqnum++);
    return event;
  }

  // ---- BFO offset (Hz) -----------------------------------------------
  if(!strcmp(cmd, "bfo"))
  {
    if(jsonInt(json, "val", &val))
    {
      updateBFO((int)val, false);
      event |= REMOTE_PREFS;
    }
    remoteJsonStatus(stream, state->remoteSeqnum++);
    return event;
  }

  // ---- Encoder-style delta commands ----------------------------------
  if(!strcmp(cmd, "band"))        { if(jsonInt(json,"d",&val)) { doBand((int)val);       event|=REMOTE_PREFS; } }
  else if(!strcmp(cmd, "mode"))   { if(jsonInt(json,"d",&val)) { doMode((int)val);       event|=REMOTE_PREFS; } }
  else if(!strcmp(cmd, "step"))   { if(jsonInt(json,"d",&val)) { doStep((int)val);       event|=REMOTE_PREFS; } }
  else if(!strcmp(cmd, "bw"))     { if(jsonInt(json,"d",&val)) { doBandwidth((int)val);  event|=REMOTE_PREFS; } }
  else if(!strcmp(cmd, "agc"))    { if(jsonInt(json,"d",&val)) { doAgc((int)val);        event|=REMOTE_PREFS; } }
  else if(!strcmp(cmd, "vol"))    { if(jsonInt(json,"d",&val)) { doVolume((int)val);     event|=REMOTE_PREFS; } }
  else if(!strcmp(cmd, "brt"))    { if(jsonInt(json,"d",&val)) { doBrt((int)val);        event|=REMOTE_PREFS; } }
  else if(!strcmp(cmd, "squelch")){ if(jsonInt(json,"d",&val)) { doSquelch((int)val);   event|=REMOTE_PREFS; } }

  // ---- Encoder click -------------------------------------------------
  else if(!strcmp(cmd, "click"))
  {
    event |= REMOTE_CLICK;
    remoteJsonStatus(stream, state->remoteSeqnum++);
    return event;
  }

  // ---- Set clock from webapp local time -----------------------------
  else if(!strcmp(cmd, "settime"))
  {
    long hh = 0, mm = 0, ss = 0;
    if(jsonInt(json, "hh", &hh) && jsonInt(json, "mm", &mm))
    {
      jsonInt(json, "ss", &ss);
      clockSet((uint8_t)hh, (uint8_t)mm, (uint8_t)ss);
    }
  }

  // ---- Sleep ---------------------------------------------------------
  else if(!strcmp(cmd, "sleep"))
  {
    if(jsonInt(json, "on", &val)) sleepOn(val ? true : false);
  }

  // ---- Scan: run a new scan, then return data ------------------------
  else if(!strcmp(cmd, "scan"))
  {
    long step = 10;
    jsonInt(json, "step", &step);
    if(step < 5)   step = 5;
    if(step > 100) step = 100;
    stream->print("{\"t\":\"ack\",\"cmd\":\"scan\"}\r\n");
    drawScreen();
    drawMessageProgress("Scanning...", 0);
    scanRun(currentFrequency, (uint16_t)step, stream, bleScanProgress);
    scanExtractChannels();
    prefsSaveScanChannels(bandIdx);
    remoteJsonScanData(stream);
    remoteJsonStatus(stream, state->remoteSeqnum++);
    return event | REMOTE_PREFS;
  }

  // ---- Seek: find next station in given direction --------------------
  else if(!strcmp(cmd, "seek"))
  {
    long dir = 1;
    jsonInt(json, "d", &dir);
    stream->print("{\"t\":\"ack\",\"cmd\":\"seek\"}\r\n");
    doSeek(dir > 0 ? 1 : -1);
    remoteJsonStatus(stream, state->remoteSeqnum++);
    return event | REMOTE_PREFS;
  }

  // ---- Scan data: return last scan without re-scanning ---------------
  else if(!strcmp(cmd, "scandata"))
  {
    remoteJsonScanData(stream);
    return event;
  }

  // ---- Save scan result as a named preset to NVS -------------------
  else if(!strcmp(cmd, "save_preset"))
  {
    char name[PRESET_NAME_LEN] = {0};
    jsonStr(json, "name", name, sizeof(name));
    if(!name[0]) strncpy(name, "Preset", sizeof(name));

    if(!scanIsDone())
    {
      static const char e1[] = "{\"t\":\"err\",\"cmd\":\"save_preset\",\"msg\":\"No scan data to save\"}\r\n";
      stream->write((const uint8_t*)e1, sizeof(e1)-1);
      return event;
    }
    uint8_t idx = prefsGetPresetCount();
    if(idx >= PRESET_MAX)
    {
      static char errbuf[64];
      int n = snprintf(errbuf, sizeof(errbuf),
        "{\"t\":\"err\",\"cmd\":\"save_preset\",\"msg\":\"Max %d presets reached\"}\r\n", PRESET_MAX);
      stream->write((uint8_t*)errbuf, n);
      return event;
    }
    if(!prefsSavePreset(name))
    {
      static const char e2[] = "{\"t\":\"err\",\"cmd\":\"save_preset\",\"msg\":\"Save failed\"}\r\n";
      stream->write((const uint8_t*)e2, sizeof(e2)-1);
      return event;
    }
    {
      static char okbuf[96];
      int n = snprintf(okbuf, sizeof(okbuf),
        "{\"t\":\"preset_saved\",\"name\":\"%s\",\"idx\":%u,\"total\":%u}\r\n",
        name, idx, idx + 1);
      stream->write((uint8_t*)okbuf, n);
    }
    return event;
  }

  // ---- List all saved presets --------------------------------------
  else if(!strcmp(cmd, "list_presets"))
  {
    uint8_t count = prefsGetPresetCount();
    static char lbuf[32];
    int ln = snprintf(lbuf, sizeof(lbuf), "{\"t\":\"presets\",\"list\":[");
    stream->write((uint8_t*)lbuf, ln);
    for(uint8_t i = 0; i < count; i++)
    {
      char pname[PRESET_NAME_LEN];
      uint8_t pband;
      ScanChannelList pch;
      if(prefsGetPreset(i, pname, sizeof(pname), &pband, &pch))
      {
        static char buf[72];
        int bn = snprintf(buf, sizeof(buf), "%s{\"idx\":%u,\"name\":\"%s\",\"band\":%u,\"ch\":%u}",
                 i ? "," : "", i, pname, pband, pch.count);
        stream->write((uint8_t*)buf, bn);
      }
    }
    static const char lend[] = "]}\r\n";
    stream->write((const uint8_t*)lend, sizeof(lend)-1);
    return event;
  }

  // ---- Delete a preset by index ------------------------------------
  else if(!strcmp(cmd, "delete_preset"))
  {
    if(!jsonInt(json, "idx", &val))
    {
      static const char e[] = "{\"t\":\"err\",\"cmd\":\"delete_preset\",\"msg\":\"Missing idx\"}\r\n";
      stream->write((const uint8_t*)e, sizeof(e)-1);
      return event;
    }
    uint8_t didx = (uint8_t)val;
    if(!prefsDeletePreset(didx))
    {
      static const char e[] = "{\"t\":\"err\",\"cmd\":\"delete_preset\",\"msg\":\"Not found\"}\r\n";
      stream->write((const uint8_t*)e, sizeof(e)-1);
      return event;
    }
    {
      static char okbuf[64];
      int n = snprintf(okbuf, sizeof(okbuf),
        "{\"t\":\"preset_deleted\",\"idx\":%u,\"total\":%u}\r\n",
        didx, prefsGetPresetCount());
      stream->write((uint8_t*)okbuf, n);
    }
    return event;
  }

  // ---- Rename a preset by index ------------------------------------
  else if(!strcmp(cmd, "rename_preset"))
  {
    if(!jsonInt(json, "idx", &val))
    {
      static const char e[] = "{\"t\":\"err\",\"cmd\":\"rename_preset\",\"msg\":\"Missing idx\"}\r\n";
      stream->write((const uint8_t*)e, sizeof(e)-1);
      return event;
    }
    uint8_t ridx = (uint8_t)val;
    char rname[PRESET_NAME_LEN] = {0};
    jsonStr(json, "name", rname, sizeof(rname));
    if(!rname[0])
    {
      static const char e[] = "{\"t\":\"err\",\"cmd\":\"rename_preset\",\"msg\":\"Missing name\"}\r\n";
      stream->write((const uint8_t*)e, sizeof(e)-1);
      return event;
    }
    if(!prefsRenamePreset(ridx, rname))
    {
      static const char e[] = "{\"t\":\"err\",\"cmd\":\"rename_preset\",\"msg\":\"Not found\"}\r\n";
      stream->write((const uint8_t*)e, sizeof(e)-1);
      return event;
    }
    {
      static char okbuf[64];
      int n = snprintf(okbuf, sizeof(okbuf),
        "{\"t\":\"preset_renamed\",\"idx\":%u,\"name\":\"%s\"}\r\n", ridx, rname);
      stream->write((uint8_t*)okbuf, n);
    }
    return event;
  }

  // ---- Load a preset's channel list into current view -------------
  else if(!strcmp(cmd, "load_preset"))
  {
    if(!jsonInt(json, "idx", &val))
    {
      static const char e[] = "{\"t\":\"err\",\"cmd\":\"load_preset\",\"msg\":\"Missing idx\"}\r\n";
      stream->write((const uint8_t*)e, sizeof(e)-1);
      return event;
    }
    uint8_t pidx = (uint8_t)val;
    char pname[PRESET_NAME_LEN];
    uint8_t pband;
    ScanChannelList pch;
    if(!prefsGetPreset(pidx, pname, sizeof(pname), &pband, &pch))
    {
      static const char e[] = "{\"t\":\"err\",\"cmd\":\"load_preset\",\"msg\":\"Not found\"}\r\n";
      stream->write((const uint8_t*)e, sizeof(e)-1);
      return event;
    }
    // Send header
    {
      static char hbuf[64];
      int n = snprintf(hbuf, sizeof(hbuf),
        "{\"t\":\"preset_loaded\",\"idx\":%u,\"name\":\"%s\",\"ch\":[", pidx, pname);
      stream->write((uint8_t*)hbuf, n);
    }
    // Channel frequencies
    for(uint8_t i = 0; i < pch.count; i++)
    {
      static char fbuf[12];
      int n = snprintf(fbuf, sizeof(fbuf), "%s%u", i ? "," : "", pch.freq[i]);
      stream->write((uint8_t*)fbuf, n);
    }
    static const char pend[] = "]}\r\n";
    stream->write((const uint8_t*)pend, sizeof(pend)-1);
    return event;
  }

  // ---- Waterfall: stream RSSI sweep rows over BLE -----------------
  else if(!strcmp(cmd, "wf_start"))
  {
    // Optional window parameters; defaults to current band full span
    long sfParam = 0, stepParam = 0, nParam = 0;
    jsonInt(json, "sf",   &sfParam);
    jsonInt(json, "step", &stepParam);
    jsonInt(json, "n",    &nParam);

    const Band *band = getCurrentBand();
    uint16_t wfSf   = sfParam   ? (uint16_t)sfParam   : band->minimumFreq;
    uint16_t wfMax  = band->maximumFreq;
    uint16_t wfStep = stepParam ? (uint16_t)stepParam
                                : (currentMode == FM ? 10 : 5);
    uint16_t wfN    = nParam    ? (uint16_t)nParam    : 100;
    if(wfN < 4)    wfN = 4;
    if(wfN > 200)  wfN = 200;
    if(wfStep < 1) wfStep = 1;

    // Show "Waterfall" message on device screen
    drawMessage("Waterfall");

    // Ack so webapp knows parameters were accepted
    {
      static char ackbuf[80];
      int an = snprintf(ackbuf, sizeof(ackbuf),
        "{\"t\":\"ack\",\"cmd\":\"wf_start\",\"sf\":%u,\"step\":%u,\"n\":%u}\r\n",
        wfSf, wfStep, wfN);
      stream->write((uint8_t*)ackbuf, an);
    }

    // Save radio state and enter scan mode
    // Use 15 ms settle time — sufficient for waterfall RSSI accuracy and
    // much faster than the 100 ms used for the precision band scan.
    uint16_t savedFreq = rx.getFrequency();
    muteOn(MUTE_TEMP, true);
    rx.setMaxDelaySetFrequency(15);
    seekStop = false;

    static uint8_t  wfRow[200];
    static char     wfJson[1200];
    bool firstRow = true;

    while(true)
    {
      // Sweep wfN frequencies across the window
      uint16_t lastFreq = 0xFFFF;
      uint8_t  lastRSSI = 0;
      uint16_t n = wfN;

      for(uint16_t i = 0; i < n; i++)
      {
        uint16_t freq = wfSf + wfStep * i;
        if(freq > wfMax) { n = i; break; }

        uint8_t r;
        if(freq != lastFreq)
        {
          // setFrequency() waits internally up to setMaxDelaySetFrequency ms
          rx.setFrequency(freq);
          rx.getCurrentReceivedSignalQuality();
          r = rx.getCurrentRSSI();
          lastFreq = freq;
          lastRSSI = r;
        }
        else
        {
          r = lastRSSI;
        }
        wfRow[i] = r;
      }

      // Build JSON row — include metadata on first row only
      int pos = 0;
      if(firstRow)
      {
        pos = snprintf(wfJson, sizeof(wfJson),
          "{\"t\":\"wf\",\"sf\":%u,\"step\":%u,\"n\":%u,\"r\":[",
          wfSf, wfStep, n);
        firstRow = false;
      }
      else
      {
        pos = snprintf(wfJson, sizeof(wfJson), "{\"t\":\"wf\",\"r\":[");
      }
      for(uint16_t i = 0; i < n && pos < (int)sizeof(wfJson) - 8; i++)
        pos += snprintf(wfJson + pos, sizeof(wfJson) - pos,
                        "%s%u", i ? "," : "", wfRow[i]);
      if(pos < (int)sizeof(wfJson) - 4)
      {
        wfJson[pos++] = ']';
        wfJson[pos++] = '}';
        wfJson[pos++] = '\r';
        wfJson[pos++] = '\n';
      }
      stream->write((uint8_t*)wfJson, pos);

      // Exit: encoder button press
      if(digitalRead(ENCODER_PUSH_BUTTON) == LOW)
      {
        while(digitalRead(ENCODER_PUSH_BUTTON) == LOW) delay(10);
        break;
      }
      // Exit: any incoming BLE data (wf_stop or new wf_start from webapp)
      if(stream->available()) break;

      // Consume encoder rotation so main loop doesn't act on it after exit
      seekStop = false;
    }

    // Notify webapp that session ended
    stream->print("{\"t\":\"wf_done\"}\r\n");

    // Restore radio state
    rx.setFrequency(savedFreq);
    muteOn(MUTE_TEMP, false);
    rx.setMaxDelaySetFrequency(30);
    drawScreen();
    return event;
  }

  else { return 0; } // unknown command

  // Send updated status after all delta commands
  remoteJsonStatus(stream, state->remoteSeqnum++);
  return event;
}

//
// Recognize and execute given remote command
//
int remoteDoCommand(Stream* stream, RemoteState* state, char key)
{
  int event = 0;

  switch(key)
  {
    case 'R': // Rotate Encoder Clockwise
      event |= 1 << REMOTE_DIRECTION;
      event |= REMOTE_PREFS;
      break;
    case 'r': // Rotate Encoder Counterclockwise
      event |= -1 << REMOTE_DIRECTION;
      event |= REMOTE_PREFS;
      break;
    case 'e': // Encoder Push Button
      event |= REMOTE_CLICK;
      break;
    case 'B': // Band Up
      doBand(1);
      event |= REMOTE_PREFS;
      break;
    case 'b': // Band Down
      doBand(-1);
      event |= REMOTE_PREFS;
      break;
    case 'M': // Mode Up
      doMode(1);
      event |= REMOTE_PREFS;
      break;
    case 'm': // Mode Down
      doMode(-1);
      event |= REMOTE_PREFS;
      break;
    case 'S': // Step Up
      doStep(1);
      event |= REMOTE_PREFS;
      break;
    case 's': // Step Down
      doStep(-1);
      event |= REMOTE_PREFS;
      break;
    case 'W': // Bandwidth Up
      doBandwidth(1);
      event |= REMOTE_PREFS;
      break;
    case 'w': // Bandwidth Down
      doBandwidth(-1);
      event |= REMOTE_PREFS;
      break;
    case 'A': // AGC/ATTN Up
      doAgc(1);
      event |= REMOTE_PREFS;
      break;
    case 'a': // AGC/ATTN Down
      doAgc(-1);
      event |= REMOTE_PREFS;
      break;
    case 'V': // Volume Up
      doVolume(1);
      event |= REMOTE_PREFS;
      break;
    case 'v': // Volume Down
      doVolume(-1);
      event |= REMOTE_PREFS;
      break;
    case 'L': // Backlight Up
      doBrt(1);
      event |= REMOTE_PREFS;
      break;
    case 'l': // Backlight Down
      doBrt(-1);
      event |= REMOTE_PREFS;
      break;
    case 'O':
      sleepOn(true);
      break;
    case 'o':
      sleepOn(false);
      break;
    case 'I':
      doCal(1);
      event |= REMOTE_PREFS;
      break;
    case 'i':
      doCal(-1);
      event |= REMOTE_PREFS;
      break;
    case 'C':
      state->remoteLogOn = false;
      remoteCaptureScreen(stream);
      break;
    case 't':
      state->remoteLogOn = !state->remoteLogOn;
      break;

    case '$':
      remoteGetMemories(stream);
      break;
    case '#':
      if (remoteSetMemory(stream))
        event |= REMOTE_PREFS;
      break;

    case 'T':
      stream->println(switchThemeEditor(!switchThemeEditor()) ? "Theme editor enabled" : "Theme editor disabled");
      break;
    case '^':
      if(switchThemeEditor()) remoteSetColorTheme(stream);
      break;
    case '@':
      if(switchThemeEditor()) remoteGetColorTheme(stream);
      break;

    default:
      // Command not recognized
      return(event);
  }

  // Command recognized
  return(event | REMOTE_CHANGED);
}

int serialDoCommand(Stream* stream, RemoteState* state, uint8_t usbMode)
{
  if(usbMode == USB_OFF) return 0;

  if (Serial.available())
    return remoteDoCommand(stream, state, Serial.read());
  return 0;
}

void serialTickTime(Stream* stream, RemoteState* state, uint8_t usbMode)
{
  if(usbMode == USB_OFF) return;

  remoteTickTime(stream, state);
}
