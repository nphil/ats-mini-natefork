#ifndef BLE_H
#define BLE_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "host/ble_gap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "Remote.h"

#define NORDIC_UART_SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NORDIC_UART_CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NORDIC_UART_CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class NordicUART : public Stream, public BLEServerCallbacks, public BLECharacteristicCallbacks {
private:
  BLEServer* pServer;
  BLEService* pService;
  BLECharacteristic* pTxCharacteristic;
  BLECharacteristic* pRxCharacteristic;

  bool started;

  // Created in start() once FreeRTOS is running; nullptr before that.
  // Starts in the "given" state (slot available for new incoming data).
  SemaphoreHandle_t dataConsumed;
  String incomingPacket;
  size_t unreadByteCount = 0;

  const char *deviceName;

public:
  NordicUART(const char *name) : deviceName(name) {
    started = false;
    dataConsumed = nullptr;
    pServer = nullptr;
    pService = nullptr;
    pTxCharacteristic = nullptr;
    pRxCharacteristic = nullptr;
  }

  void start()
  {
    // Create the semaphore here (FreeRTOS is running by the time start() is called).
    // xSemaphoreCreateBinary starts in the "taken" state; give it once so the first
    // onWrite() can acquire it immediately.
    if (!dataConsumed) {
      dataConsumed = xSemaphoreCreateBinary();
      xSemaphoreGive(dataConsumed);
    }

    BLEDevice::init(deviceName);
    BLEDevice::setPower(ESP_PWR_LVL_N0);
    BLEDevice::getAdvertising()->setName(deviceName);

    BLEDevice::setMTU(517);
    ble_gap_set_prefered_default_le_phy(BLE_GAP_LE_PHY_ANY_MASK, BLE_GAP_LE_PHY_ANY_MASK);
    ble_gap_write_sugg_def_data_len(251, (251 + 14) * 8);

    pServer = BLEDevice::getServer();
    if (pServer == nullptr)
      pServer = BLEDevice::createServer();

    pServer->setCallbacks(this);
    pServer->getAdvertising()->addServiceUUID(NORDIC_UART_SERVICE_UUID);
    pService = pServer->createService(NORDIC_UART_SERVICE_UUID);
    pTxCharacteristic = pService->createCharacteristic(NORDIC_UART_CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
    pTxCharacteristic->setCallbacks(this);
    pRxCharacteristic = pService->createCharacteristic(NORDIC_UART_CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
    pRxCharacteristic->setCallbacks(this);
    pService->start();
    pServer->getAdvertising()->start();
    started = true;
  }

  void stop()
  {
    if (!started) return;
    started = false;

    // Unblock any onWrite() that is waiting to acquire the semaphore.
    if (dataConsumed) xSemaphoreGive(dataConsumed);

    pServer = BLEDevice::getServer();
    if (pServer)
    {
      pServer->getAdvertising()->stop();

      if (pServer->getConnectedCount() > 0)
      {
        auto peerInfo = pServer->getPeerDevices(true);
        for (auto& p : peerInfo) pServer->disconnect(p.first);
        delay(50);
      }

      if (pService)
      {
        pService->stop();
        if (pRxCharacteristic) { pService->removeCharacteristic(pRxCharacteristic, true); pRxCharacteristic = nullptr; }
        if (pTxCharacteristic) { pService->removeCharacteristic(pTxCharacteristic, true); pTxCharacteristic = nullptr; }
        pServer->removeService(pService);
        pService = nullptr;
      }
    }
    BLEDevice::deinit(false);

    if (dataConsumed) { vSemaphoreDelete(dataConsumed); dataConsumed = nullptr; }
  }

  bool isStarted() { return started; }

  void onConnect(BLEServer *pServer, ble_gap_conn_desc *desc) {
    ble_gap_set_prefered_le_phy(desc->conn_handle, BLE_GAP_LE_PHY_ANY_MASK, BLE_GAP_LE_PHY_ANY_MASK, BLE_GAP_LE_PHY_CODED_ANY);
    ble_gap_set_data_len(desc->conn_handle, 251, (251 + 14) * 8);
    pServer->updateConnParams(desc->conn_handle, 6, 12, 0, 200);
  }

  void onDisconnect(BLEServer *pServer, ble_gap_conn_desc *desc) {
    if (!started) return;
    if (dataConsumed) xSemaphoreGive(dataConsumed);
    pServer->getAdvertising()->start();
  }

  void onWrite(BLECharacteristic *pCharacteristic, ble_gap_conn_desc *desc)
  {
    if (pCharacteristic == pRxCharacteristic && dataConsumed)
    {
      xSemaphoreTake(dataConsumed, portMAX_DELAY);
      incomingPacket = pCharacteristic->getValue();
      unreadByteCount = incomingPacket.length();
    }
  }

  void onStatus(BLECharacteristic *pCharacteristic, Status s, uint32_t code) {}

  int available() override { return unreadByteCount; }

  int peek() override {
    if (unreadByteCount == 0) return -1;
    return incomingPacket[incomingPacket.length() - unreadByteCount];
  }

  int read() override {
    if (unreadByteCount == 0) return -1;
    int result = incomingPacket[incomingPacket.length() - unreadByteCount];
    if (--unreadByteCount == 0 && dataConsumed) xSemaphoreGive(dataConsumed);
    return result;
  }

  size_t write(const uint8_t *data, size_t size) override
  {
    if (!pTxCharacteristic) return 0;
    size_t chunkSize = BLEDevice::getMTU();
    size_t remaining = size;
    while (remaining >= chunkSize) {
      delay(20);
      pTxCharacteristic->setValue(data, chunkSize);
      pTxCharacteristic->notify();
      data += chunkSize; remaining -= chunkSize;
    }
    if (remaining > 0) {
      delay(20);
      pTxCharacteristic->setValue(data, remaining);
      pTxCharacteristic->notify();
    }
    return size;
  }

  size_t write(uint8_t byte) override { return write(&byte, 1); }

  size_t print(std::string str) { return write((const uint8_t *)str.data(), str.length()); }

  size_t printf(const char *format, ...)
  {
    char dummy;
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(&dummy, 1, format, args);
    va_end(args);
    if (needed <= 0) return 0;
    char *buf = (char *)malloc(needed + 1);
    if (!buf) return 0;
    va_start(args, format);
    vsnprintf(buf, needed + 1, format, args);
    va_end(args);
    size_t written = write((uint8_t *)buf, needed);
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
