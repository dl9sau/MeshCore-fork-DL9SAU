#pragma once

#include <Arduino.h>

#define MAX_FRAME_SIZE  172

class BaseSerialInterface {
protected:
  BaseSerialInterface() { }

public:
  virtual void enable() = 0;
  virtual void disable() = 0;
  virtual bool isEnabled() const = 0;

  virtual bool isConnected() const = 0;

  virtual bool isWriteBusy() const = 0;
  virtual size_t writeFrame(const uint8_t src[], size_t len) = 0;
  virtual size_t checkRecvFrame(uint8_t dest[]) = 0;

  // DL9SAU 2026-06-04 (Wunschliste 40): Disconnect-Diagnose. Default
  // 0/0 -- nur BLE-Interfaces ueberschreiben dies sinnvoll.
  // Reason-Codes siehe esp_gatt_conn_reason_t (z.B. 0x08=timeout,
  // 0x13=remote-terminated, 0x16=local-host-terminated).
  virtual uint32_t getDisconnectCount() const { return 0; }
  virtual uint8_t  getLastDisconnectReason() const { return 0xFF; }
  // DL9SAU 2026-06-05: Queue-Overflow-Counter (Wunschliste 40).
  // Counts ueberschritten der FRAME_QUEUE_SIZE waehrend App-Sync.
  // Symptom: App-Timeout ohne BLE-Disconnect -- Frame ging silent
  // verloren weil queue voll war.
  virtual uint32_t getRecvOverflowCount() const { return 0; }
  virtual uint32_t getSendOverflowCount() const { return 0; }
  virtual uint8_t  getRecvQueueHighWater() const { return 0; }
  virtual uint8_t  getSendQueueHighWater() const { return 0; }
};
