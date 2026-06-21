#ifndef REMOTE_H
#define REMOTE_H

typedef struct {
  uint32_t remoteTimer  = 0;
  uint8_t  remoteSeqnum = 0;
  bool     remoteLogOn  = false; // legacy char-protocol periodic log

  // JSON subscription: send status JSON every jsonSubMs ms (0 = off).
  // Also sends immediately when key radio state changes (change-detect below).
  uint32_t jsonSubMs  = 0;
  uint32_t jsonTimer  = 0;

  // Snapshot of last-sent state for change-detection.
  // Only fields that change during normal listening are tracked.
  uint16_t lastFreq    = 0xFFFF;
  int16_t  lastBFO     = 0x7FFF;
  uint8_t  lastMode    = 0xFF;
  int      lastBandIdx = -1;
  uint8_t  lastVol     = 0xFF;
  int8_t   lastAgc     = -99;
  uint8_t  lastRSSI    = 0xFF;
  uint8_t  lastSNR     = 0xFF;
} RemoteState;

void remoteTickTime(Stream* stream, RemoteState* state);
int  remoteDoCommand(Stream* stream, RemoteState* state, char key);
int  remoteDoJsonCommand(Stream* stream, RemoteState* state, const char* json);
void remoteJsonStatus(Stream* stream, uint8_t seq);
int  serialDoCommand(Stream* stream, RemoteState* state, uint8_t usbMode);
void serialTickTime(Stream* stream, RemoteState* state, uint8_t usbMode);

#endif
