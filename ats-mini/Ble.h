#ifndef BLE_H
#define BLE_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

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

  SemaphoreHandle_t dataConsumed;
  // Binary-safe RX buffer for one notification packet. Using a raw byte buffer
  // (not Arduino String) so embedded NUL/high bytes in a streamed firmware image
  // survive intact — String's C-string semantics would truncate at the first NUL.
  // Sized for the largest MTU we negotiate (517 → 514-byte payload), rounded up.
  uint8_t rxBuf[600];
  size_t  rxLen = 0;   // bytes currently in rxBuf
  size_t  rxPos = 0;   // read cursor into rxBuf

  const char *deviceName;

public:
  NordicUART(const char *name) : deviceName(name) {
    started = false;
    pServer = nullptr;
    pService = nullptr;
    pTxCharacteristic = nullptr;
    pRxCharacteristic = nullptr;
    dataConsumed = xSemaphoreCreateBinary();
    xSemaphoreGive(dataConsumed);
  }

  void start()
  {
    BLEDevice::init(deviceName);
    BLEDevice::setPower(ESP_PWR_LVL_N0);
    BLEDevice::getAdvertising()->setName(deviceName);

    pServer = BLEDevice::getServer();
    if (pServer == nullptr)
      pServer = BLEDevice::createServer();

    pServer->setCallbacks(this);
    pServer->getAdvertising()->addServiceUUID(NORDIC_UART_SERVICE_UUID);
    pService = pServer->createService(NORDIC_UART_SERVICE_UUID);
    pTxCharacteristic = pService->createCharacteristic(NORDIC_UART_CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
    pTxCharacteristic->addDescriptor(new BLE2902());
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

        if (pRxCharacteristic)
        {
          pService->removeCharacteristic(pRxCharacteristic, true);
          pRxCharacteristic = nullptr;
        }
        if (pTxCharacteristic)
        {
          pService->removeCharacteristic(pTxCharacteristic, true);
          pTxCharacteristic = nullptr;
        }

        pServer->removeService(pService);
        pService = nullptr;
      }
    }
    BLEDevice::deinit(false);
  }

  bool isStarted()
  {
    return started;
  }

  void onConnect(BLEServer *pServer) override {
    // Request a balanced connection interval: 20–40 ms min/max.
    // Tighter than the default (7.5 ms) which wastes radio slots; looser than
    // the 100+ ms that hurts responsiveness. Supervision timeout = 4 s.
    pServer->updateConnParams(pServer->getConnId(), 0x10, 0x20, 0, 400);
  }

  void onDisconnect(BLEServer *pServer) override {
    if (!started) return;
    rxLen = 0; rxPos = 0;            // drop any partial packet
    xSemaphoreGive(dataConsumed);    // unblock a write that's waiting on the consumer
    remoteOtaAbort();                // if a BLE OTA was in-flight, reset state so the next
                                     // connection gets a clean command dispatcher
    pServer->getAdvertising()->start();
  }

  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    if(pCharacteristic == pRxCharacteristic)
    {
      // Block until the previous packet has been fully read out (back-pressure):
      // the BLE stack won't deliver the next write until we release the
      // semaphore in read(), so a slow consumer can't drop bytes.
      xSemaphoreTake(dataConsumed, portMAX_DELAY);
      size_t len = pCharacteristic->getLength();
      if(len > sizeof(rxBuf)) len = sizeof(rxBuf);
      memcpy(rxBuf, pCharacteristic->getData(), len);
      rxLen = len;
      rxPos = 0;
    }
  }

  void onStatus(BLECharacteristic *pCharacteristic, Status s, uint32_t code) override
  {
  }

  int available()
  {
    return (int)(rxLen - rxPos);
  }

  int peek()
  {
    if (rxPos < rxLen)
      return rxBuf[rxPos];
    return -1;
  }

  int read()
  {
    if (rxPos < rxLen)
    {
      int result = rxBuf[rxPos++];
      // Last byte consumed → release the packet slot so onWrite can accept the
      // next notification.
      if (rxPos >= rxLen)
        xSemaphoreGive(dataConsumed);
      return result;
    }
    return -1;
  }

  size_t write(const uint8_t *data, size_t size)
  {
    if (!pTxCharacteristic) return 0;

    size_t chunkSize = BLEDevice::getMTU() - 3;
    if (chunkSize < 20) chunkSize = 20;

    const uint8_t* ptr = data;
    size_t remaining   = size;
    while (remaining > 0) {
      size_t n = (remaining < chunkSize) ? remaining : chunkSize;
      pTxCharacteristic->setValue(const_cast<uint8_t*>(ptr), n);
      pTxCharacteristic->notify();

      ptr       += n;
      remaining -= n;
      // Inter-chunk pause: yield to FreeRTOS (BLE task, encoder ISR, display)
      // instead of busy-waiting with delay(). One BLE connection event is ~20 ms;
      // 12 ms gives the stack time to queue the packet without over-blocking.
      if (remaining > 0) vTaskDelay(pdMS_TO_TICKS(12));
    }
    return size;
  }

  size_t write(uint8_t byte)
  {
    return write(&byte, 1);
  }

  size_t print(std::string str)
  {
    return write((const uint8_t *)str.data(), str.length());
  }

  size_t printf(const char *format, ...)
  {
    char dummy;
    va_list args;
    va_start(args, format);
    int requiredSize = vsnprintf(&dummy, 1, format, args);
    va_end(args);
    if (requiredSize == 0)
    {
      return write((uint8_t *)&dummy, 1);
    }
    else if (requiredSize > 0)
    {
      char *buffer = (char *)malloc(requiredSize + 1);
      if (buffer)
      {
        va_start(args, format);
        int result = vsnprintf(buffer, requiredSize + 1, format, args);
        va_end(args);
        if ((result >= 0) && (result <= requiredSize))
        {
          size_t writtenBytesCount = write((uint8_t *)buffer, result + 1);
          free(buffer);
          return writtenBytesCount;
        }
        free(buffer);
      }
    }
    return 0;
  }
};

void bleInit(uint8_t bleMode);
void bleStop();
int8_t getBleStatus();
void remoteBLETickTime(Stream* stream, RemoteState* state, uint8_t bleMode);
int bleDoCommand(Stream* stream, RemoteState* state, uint8_t bleMode);
extern NordicUART BLESerial;

#endif
