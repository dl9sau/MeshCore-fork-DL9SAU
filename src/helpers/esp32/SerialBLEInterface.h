#pragma once

#include "../BaseSerialInterface.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

class SerialBLEInterface : public BaseSerialInterface, BLESecurityCallbacks, BLEServerCallbacks, BLECharacteristicCallbacks {
  BLEServer *pServer;
  BLEService *pService;
  BLECharacteristic * pTxCharacteristic;
  bool deviceConnected;
  bool oldDeviceConnected;
  bool _isEnabled;
  // Wunschliste 58 Phase F Retry 2026-06-14: Guard-Flag fuer
  // esp_bt_controller_disable. Wenn true, returnen alle public-API
  // Methoden (isEnabled, isConnected, writeFrame, checkRecvFrame)
  // sofort false/0 ohne in den BLE-Stack zu rufen. Verhindert dass
  // manageBlePower() oder andere Loop-Code Wege in den abgeschalteten
  // Stack rufen und sich aufhaengen (Lockup im 1. Versuch Commit
  // 8ae3b492, revert a4293243).
  bool _ctrl_disabled;
  // Wunschliste 58 Phase F Retry 2026-06-14: nach esp_bt_controller_enable
  // muss der Device-Name neu gesetzt werden -- der GAP-Layer behaelt ihn
  // nicht. Saved Copy aus begin(); enable() ruft esp_ble_gap_set_device_name
  // wenn der Controller neu hochfaehrt.
  char _saved_dev_name[48];
  uint16_t last_conn_id;
  uint32_t _pin_code;
  unsigned long _last_write;
  unsigned long adv_restart_time;
  // ENTFERNBAR (Wunschliste 40): BLE-Diagnose-Counter. Wenn 'bleinfo' CLI
  // entfernt wird, koennen diese 6 member-vars + die zugehoerigen Override-
  // Getter + die Increments in onWrite/writeFrame/onDisconnect raus. Der
  // 2-param onDisconnect-Callback selbst (Library-API) kann bleiben, da
  // harmlos (ohne diese Diag-Felder ist er einfach no-op).
  uint32_t _disconnect_count;
  uint8_t  _last_disconnect_reason;
  uint32_t _recv_overflow_count;
  uint32_t _send_overflow_count;
  uint8_t  _recv_queue_high_water;
  uint8_t  _send_queue_high_water;

  struct Frame {
    uint8_t len;
    uint8_t buf[MAX_FRAME_SIZE];
  };

  // DL9SAU 2026-06-05 (Wunschliste 40): 4 -> 16. User-Daten zeigten
  // send_ovf=188 bei mehrfachem 'stats'. stats produziert viele
  // pushCompanionMessage in Folge, jede triggert eine
  // PUSH_CODE_MSG_WAITING-Tickle via writeFrame. Mit 60ms BLE-Throttle
  // und 4-Slot-Queue kein Burst-Buffer. RAM-Kosten: (16-4)*173 = ~2KB,
  // verteilt auf recv+send. Vertretbar -- ESP32-S3 hat reichlich RAM.
  #define FRAME_QUEUE_SIZE  16
  // Upstream 1.17.0 #3007: recv_queue als FreeRTOS-Static-Queue (thread-safe --
  // der BLE onWrite-Callback fuellt aus dem BLE-Task, loop() drained aus dem
  // loopTask -> ohne Queue eine echte Race auf recv_queue_len/[]). DL9SAU:
  // Groesse bleibt FRAME_QUEUE_SIZE (16); Diag-Counter (overflow/high-water)
  // unten erhalten, jetzt ueber den Queue-Fuellstand gepflegt.
  StaticQueue_t recv_queue_state;
  uint8_t recv_queue_storage[FRAME_QUEUE_SIZE * sizeof(Frame)];
  QueueHandle_t recv_queue;
  int send_queue_len;
  Frame send_queue[FRAME_QUEUE_SIZE];

  void clearBuffers();

  // Wunschliste 58 Phase F Retry 2026-06-14: Stack-State der nach
  // esp_bt_controller_enable wieder gesetzt werden muss. Wird auch von
  // enable() aufgerufen wenn der Controller neu hochfuhr. Eigene Methode
  // damit kein duplizierter Code zwischen begin() und enable() entsteht
  // (Flash-Spar). Erweiterbar falls weitere Stack-Settings nach Wake
  // verloren gehen.
  void reapplyControllerState();

protected:
  // BLESecurityCallbacks methods
  uint32_t onPassKeyRequest() override;
  void onPassKeyNotify(uint32_t pass_key) override;
  bool onConfirmPIN(uint32_t pass_key) override;
  bool onSecurityRequest() override;
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override;

  // BLEServerCallbacks methods
  void onConnect(BLEServer* pServer) override;
  void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) override;
  void onMtuChanged(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) override;
  void onDisconnect(BLEServer* pServer) override;
  void onDisconnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) override;

  // BLECharacteristicCallbacks methods
  void onWrite(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param) override;

public:
  SerialBLEInterface() {
    pServer = NULL;
    pService = NULL;
    deviceConnected = false;
    oldDeviceConnected = false;
    adv_restart_time = 0;
    _isEnabled = false;
    _last_write = 0;
    last_conn_id = 0;
    send_queue_len = 0;
    _disconnect_count = 0;
    _last_disconnect_reason = 0xFF;
    _recv_overflow_count = 0;
    _send_overflow_count = 0;
    _recv_queue_high_water = 0;
    _send_queue_high_water = 0;
    _ctrl_disabled = false;
    _saved_dev_name[0] = 0;
    // Upstream 1.17.0 #3007: thread-safe recv_queue (siehe oben).
    recv_queue = xQueueCreateStatic(
      FRAME_QUEUE_SIZE, sizeof(Frame), recv_queue_storage, &recv_queue_state
    );
  }

  uint32_t getDisconnectCount() const override { return _disconnect_count; }
  uint8_t  getLastDisconnectReason() const override { return _last_disconnect_reason; }
  uint32_t getRecvOverflowCount() const override { return _recv_overflow_count; }
  uint32_t getSendOverflowCount() const override { return _send_overflow_count; }
  uint8_t  getRecvQueueHighWater() const override { return _recv_queue_high_water; }
  uint8_t  getSendQueueHighWater() const override { return _send_queue_high_water; }

  /**
   * init the BLE interface.
   * @param prefix   a prefix for the device name
   * @param name  IN/OUT - a name for the device (combined with prefix). If "@@MAC", is modified and returned
   * @param pin_code   the BLE security pin
   */
  void begin(const char* prefix, char* name, uint32_t pin_code);

  // BaseSerialInterface methods
  void enable() override;
  void disable() override;
  // Wunschliste 58 Phase F Retry: isEnabled muss auch _ctrl_disabled
  // pruefen damit Loop-Code (manageBlePower) waehrend Controller-Down
  // den State sauber 'aus' sieht. _isEnabled wird in disable()
  // sowieso auf false gesetzt; doppelt sicher schadet nicht.
  bool isEnabled() const override { return _isEnabled && !_ctrl_disabled; }

  bool isConnected() const override;

  bool isWriteBusy() const override;
  size_t writeFrame(const uint8_t src[], size_t len) override;
  size_t checkRecvFrame(uint8_t dest[]) override;
};

#if BLE_DEBUG_LOGGING && ARDUINO
  #include <Arduino.h>
  #define BLE_DEBUG_PRINT(F, ...) Serial.printf("BLE: " F, ##__VA_ARGS__)
  #define BLE_DEBUG_PRINTLN(F, ...) Serial.printf("BLE: " F "\n", ##__VA_ARGS__)
#else
  #define BLE_DEBUG_PRINT(...) {}
  #define BLE_DEBUG_PRINTLN(...) {}
#endif
