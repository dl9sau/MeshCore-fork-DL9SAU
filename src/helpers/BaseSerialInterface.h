#pragma once

#include <Arduino.h>

#define MAX_FRAME_SIZE  176   // +4 for transport codes (region scoping)

class BaseSerialInterface {
protected:
  BaseSerialInterface() { }

public:
  virtual void enable() = 0;
  virtual void disable() = 0;
  virtual bool isEnabled() const = 0;

  virtual bool isConnected() const = 0;
  virtual void loop() {};

  virtual bool isWriteBusy() const = 0;
  virtual size_t writeFrame(const uint8_t src[], size_t len) = 0;
  virtual size_t checkRecvFrame(uint8_t dest[]) = 0;

  // ENTFERNBAR (Wunschliste 40): BLE-Diagnose-Virtuals. Wenn 'bleinfo'
  // CLI komplett entfernt wird, koennen auch diese 6 Methoden + ihre
  // Override-Implementierungen in SerialBLEInterface ersatzlos geloescht
  // werden. Default-Implementierungen geben 0/0xFF zurueck, kein Caller-
  // Code muss angepasst werden.
  // Reason-Codes siehe esp_gatt_conn_reason_t (z.B. 0x08=timeout,
  // 0x13=remote-terminated, 0x16=local-host-terminated).
  virtual uint32_t getDisconnectCount() const { return 0; }
  virtual uint8_t  getLastDisconnectReason() const { return 0xFF; }
  virtual uint32_t getRecvOverflowCount() const { return 0; }
  virtual uint32_t getSendOverflowCount() const { return 0; }
  virtual uint8_t  getRecvQueueHighWater() const { return 0; }
  virtual uint8_t  getSendQueueHighWater() const { return 0; }
};
