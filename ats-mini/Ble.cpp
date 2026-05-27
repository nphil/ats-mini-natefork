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
  BLEServer* srv = BLEDevice::getServer();
  if(!srv) return 0;
  return srv->getConnectedCount() > 0 ? 1 : -1;
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
}

int bleDoCommand(Stream* stream, RemoteState* state, uint8_t bleMode)
{
  if(bleMode == BLE_OFF) return 0;
  if(!BLESerial.isStarted()) return 0;
  BLEServer* srv = BLEDevice::getServer();
  if(!srv || srv->getConnectedCount() == 0) return 0;
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
  if(!BLESerial.isStarted()) return;
  BLEServer* srv = BLEDevice::getServer();
  if(!srv) return;

  if(srv->getConnectedCount() > 0)
    remoteTickTime(stream, state);
}
