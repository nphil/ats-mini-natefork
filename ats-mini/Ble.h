#ifndef BLE_H
#define BLE_H

#include <NimBLEDevice.h>
#include <semaphore>

#include "Remote.h"

#define NORDIC_UART_SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NORDIC_UART_CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NORDIC_UART_CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class NordicUART : public Stream,
                   private NimBLEServerCallbacks,
                   private NimBLECharacteristicCallbacks {
private:
  NimBLEServer*         pServer;
  NimBLEService*        pService;
  NimBLECharacteristic* pTxCharacteristic;
  NimBLECharacteristic* pRxCharacteristic;

  bool started;

  std::binary_semaphore dataConsumed{1};
  std::string           incomingData;
  size_t                unreadByteCount;

  const char* deviceName;

  // ── Server callbacks ──────────────────────────────────────────────────────

  void onConnect(NimBLEServer* srv, NimBLEConnInfo& info) override {
    // Tighten connection interval: 7.5–15 ms, no slave latency, 2 s timeout
    srv->updateConnParams(info.getConnHandle(), 6, 12, 0, 200);
  }

  void onDisconnect(NimBLEServer* srv, NimBLEConnInfo& info, int reason) override {
    // If stop() is mid-teardown bail out; touching a torn-down server or
    // releasing the semaphore twice is UB.
    if (!started) return;
    dataConsumed.release();
    NimBLEDevice::getAdvertising()->start();
  }

  // ── Characteristic callbacks ──────────────────────────────────────────────

  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& info) override {
    if (pChar != pRxCharacteristic) return;
    dataConsumed.acquire();
    const auto& val = pChar->getValue();
    incomingData    = std::string(reinterpret_cast<const char*>(val.data()), val.size());
    unreadByteCount = incomingData.size();
  }

public:
  explicit NordicUART(const char* name)
      : deviceName(name), pServer(nullptr), pService(nullptr),
        pTxCharacteristic(nullptr), pRxCharacteristic(nullptr),
        started(false), unreadByteCount(0) {}

  void start()
  {
    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(3);   // 3 dBm
    NimBLEDevice::setMTU(517);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(this);

    pService = pServer->createService(NORDIC_UART_SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        NORDIC_UART_CHARACTERISTIC_UUID_TX,
        NIMBLE_PROPERTY::NOTIFY);

    pRxCharacteristic = pService->createCharacteristic(
        NORDIC_UART_CHARACTERISTIC_UUID_RX,
        NIMBLE_PROPERTY::WRITE);
    pRxCharacteristic->setCallbacks(this);

    pService->start();

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(NORDIC_UART_SERVICE_UUID);
    pAdv->start();

    started = true;
  }

  void stop()
  {
    // Set started = false first so any in-flight callbacks bail out before
    // we tear down GATT state (avoids semaphore double-release / UB).
    if (!started) return;
    started = false;

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    if (pAdv) pAdv->stop();

    if (pServer && pServer->getConnectedCount() > 0) {
      size_t n = pServer->getConnectedCount();
      for (size_t i = 0; i < n; i++) {
        NimBLEConnInfo info = pServer->getPeerInfo(0);
        pServer->disconnect(info.getConnHandle());
      }
      delay(50);
    }

    NimBLEDevice::deinit(false);
    pServer           = nullptr;
    pService          = nullptr;
    pTxCharacteristic = nullptr;
    pRxCharacteristic = nullptr;
  }

  bool isStarted() { return started; }

  // ── Stream interface ──────────────────────────────────────────────────────

  int available() override { return (int)unreadByteCount; }

  int peek() override {
    if (unreadByteCount == 0) return -1;
    return (uint8_t)incomingData[incomingData.size() - unreadByteCount];
  }

  int read() override {
    if (unreadByteCount == 0) return -1;
    int ch = (uint8_t)incomingData[incomingData.size() - unreadByteCount];
    if (--unreadByteCount == 0)
      dataConsumed.release();
    return ch;
  }

  size_t write(const uint8_t* data, size_t size) override
  {
    if (!pTxCharacteristic || size == 0) return 0;
    // ATT notification payload = MTU - 3 (ATT header)
    size_t mtu       = NimBLEDevice::getMTU();
    size_t chunkSize = mtu > 3 ? mtu - 3 : 20;
    size_t remaining = size;
    while (remaining > 0) {
      delay(20);
      size_t n = remaining > chunkSize ? chunkSize : remaining;
      pTxCharacteristic->setValue(data, n);
      pTxCharacteristic->notify();
      data      += n;
      remaining -= n;
    }
    return size;
  }

  size_t write(uint8_t byte) override { return write(&byte, 1); }

  size_t print(const std::string& str)
  {
    return write(reinterpret_cast<const uint8_t*>(str.data()), str.size());
  }

  size_t printf(const char* format, ...)
  {
    char dummy;
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(&dummy, 1, format, args);
    va_end(args);
    if (needed <= 0) return 0;
    char* buf = static_cast<char*>(malloc(needed + 1));
    if (!buf) return 0;
    va_start(args, format);
    vsnprintf(buf, needed + 1, format, args);
    va_end(args);
    size_t written = write(reinterpret_cast<const uint8_t*>(buf), needed);
    free(buf);
    return written;
  }
};

void     bleInit(uint8_t bleMode);
void     bleStop();
int8_t   getBleStatus();
void     remoteBLETickTime(Stream* stream, RemoteState* state, uint8_t bleMode);
int      bleDoCommand(Stream* stream, RemoteState* state, uint8_t bleMode);
extern NordicUART BLESerial;

#endif // BLE_H
