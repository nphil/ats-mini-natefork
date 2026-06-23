#include "Common.h"
#include "Themes.h"
#include "Remote.h"
#include "Ble.h"

//
// Get current connection status
// (-1 - not connected, 0 - disabled, 1 - connected)
//
int8_t getBleStatus()
{
  if(!BLESerial.isStarted()) return 0;
  return BLEDevice::getServer()->getConnectedCount() > 0 ? 1 : -1;
}

//
// Stop BLE hardware
//
void bleStop()
{
  if(!BLESerial.isStarted()) return;
  BLESerial.stop();
}

void bleInit(uint8_t bleMode)
{
  bleStop();

  if(bleMode == BLE_OFF) return;
  BLESerial.start();
  // Send a JSON status greeting once a client connects (handled by onConnect callback).
  // The auto-off timer in ats-mini.ino seeds itself on the first loop iteration.
}

int bleDoCommand(Stream* stream, RemoteState* state, uint8_t bleMode)
{
  if(bleMode == BLE_OFF) return 0;
  // Guard against teardown races: isStarted() flips false before stop()
  // tears down the server, so check it before dereferencing getServer().
  if(!BLESerial.isStarted()) return 0;
  BLEServer* srv = BLEDevice::getServer();
  if(!srv || srv->getConnectedCount() == 0) return 0;

  // An in-progress OTA takes over BLE input entirely — raw firmware bytes must
  // go to the staging sink, not the JSON/char parser (mirrors serialDoCommand).
  // Checked before the available() early-return so finalization still runs when
  // the last block has landed but no new bytes are pending.
  if(remoteOtaActive()) { remoteOtaPump(stream); return 0; }

  if(!BLESerial.available()) return 0;

  // JSON command: peek for opening brace, read full packet, dispatch
  if(BLESerial.peek() == '{')
  {
    static char buf[512];
    int len = 0;
    while(BLESerial.available() && len < (int)sizeof(buf) - 1)
      buf[len++] = BLESerial.read();
    buf[len] = '\0';
    return remoteDoJsonCommand(stream, state, buf);
  }

  // Legacy single-char command
  return remoteDoCommand(stream, state, BLESerial.read());
}

void remoteBLETickTime(Stream* stream, RemoteState* state, uint8_t bleMode)
{
  if(bleMode == BLE_OFF) return;
  // Same teardown guard as bleDoCommand(): without this, the periodic status
  // tick can race with bleStop() and dereference a null/destroyed server.
  if(!BLESerial.isStarted()) return;
  BLEServer* srv = BLEDevice::getServer();
  if(!srv) return;

  // Don't inject periodic status while an OTA is streaming — its packets would
  // interleave with the raw image / ACK traffic (mirrors serialTickTime).
  if(remoteOtaActive()) return;

  if (srv->getConnectedCount() > 0)
    remoteTickTime(stream, state);
}
