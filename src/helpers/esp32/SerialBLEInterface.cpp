#include "SerialBLEInterface.h"
#include "../BleNameHelper.h"
#include "esp_mac.h"
// Wunschliste 58 Phase F Retry 2026-06-14: esp_bt_controller_disable
// schaltet das BT-Radio echt aus (1. Versuch in Commit 8ae3b492 fuhrte
// zu Lockup, revert in a4293243). Diesmal mit Guard-Flag _ctrl_disabled
// dass alle public-API Methoden sauber returnen ohne in den
// abgeschalteten Stack zu rufen.
#include "esp_bt.h"
// fuer esp_ble_gap_set_device_name (Phase F Retry: Name nach Controller-
// Re-Enable wieder setzen).
#include "esp_gap_ble_api.h"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define ADVERT_RESTART_DELAY  1000   // millis

void SerialBLEInterface::begin(const char* prefix, char* name, uint32_t pin_code) {
  _pin_code = pin_code;

  uint8_t addr[8];
  memset(addr, 0, sizeof(addr));
  esp_efuse_mac_get_default(addr);

  if (strcmp(name, "@@MAC") == 0) {
    sprintf(name, "%02X%02X%02X%02X%02X%02X",    // modify (IN-OUT param)
          addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
  }
  char dev_name[32+16];
  sprintf(dev_name, "%s%s", prefix, name);

  // DL9SAU 2026-06-01 / 2026-06-16: BLE-Name sanitisieren via Helper.
  // esp_ble_gap_set_device_name() lehnt Namen mit Non-ASCII-Bytes ab
  // (UTF-8-Multibyte, Steuerzeichen) und schlaegt mit ESP_ERR_INVALID_ARG
  // (rc=258) fehl -- das Geraet erscheint dann generisch als "ESP32".
  // BLE-Name-Helper macht Sanitize + Word-boundary-Truncate + Trim.
  // Aenderung wirkt nur fuer BLE -- _prefs.node_name (Chat, Advert) bleibt
  // unveraendert mit User-Originalstring inklusive UTF-8.
  char clean[64];
  size_t cn = sanitizeBleName(dev_name, clean, sizeof(clean), 28);

  if (cn <= strlen(prefix)) {
    // Name-Teil komplett rausgefiltert (User-Name war voll-UTF-8).
    // MAC-Fallback damit Geraet nicht generisch 'ESP32' wird.
    snprintf(clean, sizeof(clean), "%sNode-%02X%02X",
             prefix, addr[1], addr[0]);
  }

  // Wunschliste 58 Phase F Retry 2026-06-14: Device-Name sichern damit
  // wir ihn nach esp_bt_controller_enable wieder setzen koennen
  // (GAP-Layer behaelt den Namen nicht ueber Controller-Disable hinweg --
  // User-Bug 2026-06-14: nach Wake meldete sich Geraet generisch als
  // 'ESP32').
  {
    size_t cl = strlen(clean);
    if (cl >= sizeof(_saved_dev_name)) cl = sizeof(_saved_dev_name) - 1;
    memcpy(_saved_dev_name, clean, cl);
    _saved_dev_name[cl] = 0;
  }

  // Create the BLE Device
  BLEDevice::init(clean);
  BLEDevice::setSecurityCallbacks(this);
  BLEDevice::setMTU(MAX_FRAME_SIZE);

  BLESecurity  sec;
  sec.setStaticPIN(pin_code);
  sec.setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);

  //BLEDevice::setPower(ESP_PWR_LVL_N8);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(this);

  // Create the BLE Service
  pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM);
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setAccessPermissions(ESP_GATT_PERM_WRITE_ENC_MITM);
  pRxCharacteristic->setCallbacks(this);

  pServer->getAdvertising()->addServiceUUID(SERVICE_UUID);
  // DL9SAU 2026-06-18 (Wunschliste 84 Hebel C): Adv-Interval explizit
  // setzen. arduino-esp32-BLE-Default ist ca 1280ms (= 0x0800), aber
  // beim ersten begin() ist eher 'sehr aggressiv' (32-64). Wir setzen
  // analog NRF52: Min 20ms, Max 417.5ms -- spart Strom im Disconnected-
  // Advertising waehrend des 20s BLE-Wake-Cycles.
  pServer->getAdvertising()->setMinInterval(32);    // 32 * 0.625 = 20ms
  pServer->getAdvertising()->setMaxInterval(668);   // 668 * 0.625 = 417.5ms

  // DL9SAU 2026-06-13: pService->start() einmalig hier in begin()
  // statt frueher in enable() bei jedem Wake-up. Hintergrund: arduino-esp32
  // BLEService::start() versucht alle Characteristics neu zu registrieren.
  // Beim 2./3./N-ten Aufruf haben sie bereits Handles aus dem ersten
  // start() -> ERROR-Log "Characteristic already has a handle." und
  // verschwendete CPU-Arbeit. Im BLE-Cycle-Modus passierte das alle 60s
  // (User-Beobachtung 2026-06-13 mit USB-cu Mitlauf). Folge-Effekt:
  // ESP_LOGE-Output an Serial; bei USB-Power ohne Host-Reader (Powerbank)
  // kann Serial.print blocken -> erhoehter Strom durch CPU-Spin statt
  // Idle-Yield. Fix: Service start einmalig. enable()/disable() steuern
  // nur noch Advertising + Connection-State.
  pService->start();
}

// -------- BLESecurityCallbacks methods

uint32_t SerialBLEInterface::onPassKeyRequest() {
  BLE_DEBUG_PRINTLN("onPassKeyRequest()");
  return _pin_code;
}

void SerialBLEInterface::onPassKeyNotify(uint32_t pass_key) {
  BLE_DEBUG_PRINTLN("onPassKeyNotify(%u)", pass_key);
}

bool SerialBLEInterface::onConfirmPIN(uint32_t pass_key) {
  BLE_DEBUG_PRINTLN("onConfirmPIN(%u)", pass_key);
  return true;
}

bool SerialBLEInterface::onSecurityRequest() {
  BLE_DEBUG_PRINTLN("onSecurityRequest()");
  return true;  // allow
}

void SerialBLEInterface::onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
  if (cmpl.success) {
    BLE_DEBUG_PRINTLN(" - SecurityCallback - Authentication Success");
    deviceConnected = true;
  } else {
    BLE_DEBUG_PRINTLN(" - SecurityCallback - Authentication Failure*");

    //pServer->removePeerDevice(pServer->getConnId(), true);
    pServer->disconnect(pServer->getConnId());
    adv_restart_time = millis() + ADVERT_RESTART_DELAY;
  }
}

// -------- BLEServerCallbacks methods

void SerialBLEInterface::onConnect(BLEServer* pServer) {
}

void SerialBLEInterface::onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
  BLE_DEBUG_PRINTLN("onConnect(), conn_id=%d, mtu=%d", param->connect.conn_id, pServer->getPeerMTU(param->connect.conn_id));
  last_conn_id = param->connect.conn_id;
}

void SerialBLEInterface::onMtuChanged(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) {
  BLE_DEBUG_PRINTLN("onMtuChanged(), mtu=%d", pServer->getPeerMTU(param->mtu.conn_id));
}

void SerialBLEInterface::onDisconnect(BLEServer* pServer) {
  BLE_DEBUG_PRINTLN("onDisconnect()");
  deviceConnected = false;
  if (_isEnabled) {
    adv_restart_time = millis() + ADVERT_RESTART_DELAY;
  }
}

// DL9SAU 2026-06-04 (Wunschliste 40): 2-Param-Variante mit Reason-Code.
// Wird zusaetzlich zur 1-Param-Variante aufgerufen, daher hier NUR die
// Diagnose-Felder befuellen -- restart-advertising-Logik laeuft schon
// in der 1-Param-Variante.
void SerialBLEInterface::onDisconnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
  if (param != NULL) {
    _last_disconnect_reason = param->disconnect.reason;
  }
  _disconnect_count++;
}

// -------- BLECharacteristicCallbacks methods

void SerialBLEInterface::onWrite(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param) {
  uint8_t* rxValue = pCharacteristic->getData();
  int len = pCharacteristic->getLength();

  if (len > MAX_FRAME_SIZE) {
    BLE_DEBUG_PRINTLN("ERROR: onWrite(), frame too big, len=%d", len);
  } else {
    Frame frame = {};
    frame.len = len;
    memcpy(frame.buf, rxValue, len);

    // Upstream 1.17.0 #3007: thread-safer Push via FreeRTOS-Queue.
    if (xQueueSend(recv_queue, &frame, 0) != pdTRUE) {
      BLE_DEBUG_PRINTLN("ERROR: onWrite(), recv_queue is full!");
      _recv_overflow_count++;  // DL9SAU 2026-06-05: Diag-Counter
    } else {
      // DL9SAU 2026-06-05: High-Water jetzt ueber den Queue-Fuellstand.
      UBaseType_t _rq_fill = uxQueueMessagesWaiting(recv_queue);
      if (_rq_fill > _recv_queue_high_water)
        _recv_queue_high_water = (uint8_t)_rq_fill;
    }
  }
}

// ---------- public methods

// Wunschliste 58 Phase F Retry 2026-06-14: nach esp_bt_controller_enable
// muessen Stack-Settings die der Controller verliert wieder gesetzt
// werden. Aktuell: Device-Name (sonst meldet sich Geraet generisch als
// 'ESP32'). Erweiterbar wenn weitere Settings (Advertising-Data,
// Connection-Parameter) verloren gehen.
// Code-Spar: eine Methode statt duplizierten Setup in mehreren Pfaden.
void SerialBLEInterface::reapplyControllerState() {
  if (_saved_dev_name[0]) {
    esp_ble_gap_set_device_name(_saved_dev_name);
  }
}

// Upstream 1.17.0 #3007: recv_queue ist jetzt eine FreeRTOS-Queue -> reset
// via xQueueReset statt recv_queue_len = 0.
void SerialBLEInterface::clearBuffers() {
  xQueueReset(recv_queue);
  send_queue_len = 0;
}

void SerialBLEInterface::enable() {
  if (_isEnabled) return;

  // Wunschliste 58 Phase F Retry 2026-06-14: BT-Controller wieder
  // hochfahren falls er schlaeft. Reihenfolge wichtig:
  //   1) Guard-Flag ZUERST loeschen (interne Re-Aktivierung darf
  //      Stack-Calls erlauben), aber NACH erfolgreichem
  //      esp_bt_controller_enable -- vorher wuerden potentielle
  //      Aufrufe an den noch nicht hochgefahrenen Stack hangen.
  //   2) Advertising starten.
  // Status-Check: INITED bedeutet 'initialisiert aber nicht enabled'
  // (= unser disabled-Zustand). ENABLED ist Boot-Zustand wo BLEDevice::
  // init bereits den Controller hochgefahren hat -- dort kein
  // Re-Enable noetig.
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    esp_err_t err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK) {
      BLE_DEBUG_PRINTLN("enable: ctrl_enable failed err=%d", (int)err);
      // Guard bleibt gesetzt -- weiterer Stack-Call wuerde sonst hangen.
      return;
    }
    // Controller wurde gerade hochgefahren -> Stack-State (Device-Name
    // etc.) wieder anwenden bevor Advertising startet.
    reapplyControllerState();
  }
  _ctrl_disabled = false;  // Stack-Calls jetzt sicher

  _isEnabled = true;
  clearBuffers();

  // pService->start() entfernt -- ist jetzt in begin() einmalig (siehe
  // Kommentar dort). Wir steuern nur noch Advertising. Wenn die
  // Verbindung lebt, weckt das einen Disconnect nicht; wenn nicht,
  // beginnt neues Advertising.
  pServer->getAdvertising()->start();
  adv_restart_time = 0;
}

void SerialBLEInterface::disable() {
  _isEnabled = false;

  BLE_DEBUG_PRINTLN("SerialBLEInterface::disable");

  // Wunschliste 58 Phase F Retry 2026-06-14: Guard-Flag VOR Stack-
  // Manipulation setzen. Damit returnen alle parallel laufenden
  // Aufrufer (manageBlePower-Loop in MyMesh) sofort false/0 ohne in
  // den gleich abgeschalteten Stack zu rufen.
  _ctrl_disabled = true;
  pServer->disconnect(last_conn_id);
  oldDeviceConnected = deviceConnected = false;

  // Phase F: BT-Controller wirklich aus. Status-Check verhindert
  // Doppel-Disable. esp_bt_controller_disable schaltet das BT-Radio
  // ab -- bestaetigt 70+ mA Strom-Spar im 1. Versuch (Commit 8ae3b492
  // 2026-06-13). Damals durch Lockup revertiert. Diesmal mit Guard.
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    esp_err_t err = esp_bt_controller_disable();
    if (err != ESP_OK) {
      BLE_DEBUG_PRINTLN("disable: ctrl_disable failed err=%d", (int)err);
    }
  }
}

size_t SerialBLEInterface::writeFrame(const uint8_t src[], size_t len) {
  // Phase F Retry: sofort raus wenn Controller schlaeft. Sonst
  // wuerde pTxCharacteristic->notify() in checkRecvFrame in den
  // toten Stack rufen.
  if (_ctrl_disabled) return 0;
  if (len > MAX_FRAME_SIZE) {
    BLE_DEBUG_PRINTLN("writeFrame(), frame too big, len=%d", len);
    return 0;
  }

  if (deviceConnected && len > 0) {
    if (send_queue_len >= FRAME_QUEUE_SIZE) {
      BLE_DEBUG_PRINTLN("writeFrame(), send_queue is full!");
      _send_overflow_count++;  // DL9SAU 2026-06-05: Diag-Counter
      return 0;
    }

    send_queue[send_queue_len].len = len;  // add to send queue
    memcpy(send_queue[send_queue_len].buf, src, len);
    send_queue_len++;
    if (send_queue_len > _send_queue_high_water)
      _send_queue_high_water = send_queue_len;

    return len;
  }
  return 0;
}

#define  BLE_WRITE_MIN_INTERVAL   60

bool SerialBLEInterface::isWriteBusy() const {
  // Phase F Retry: nicht busy wenn Controller schlaeft -- writeFrame
  // wuerde sowieso 0 returnen.
  if (_ctrl_disabled) return false;
  return millis() < _last_write + BLE_WRITE_MIN_INTERVAL;   // still too soon to start another write?
}

size_t SerialBLEInterface::checkRecvFrame(uint8_t dest[]) {
  // Phase F Retry: sofort raus wenn Controller schlaeft. Schuetzt vor
  // pTxCharacteristic->notify(), pServer->getConnectedCount(),
  // pServer->getAdvertising()->start/stop -- alles Stack-Calls die
  // ohne Controller blockten/abstuerzten im 1. Versuch.
  if (_ctrl_disabled) return 0;
  if (send_queue_len > 0   // first, check send queue
    && millis() >= _last_write + BLE_WRITE_MIN_INTERVAL    // space the writes apart
  ) {
    _last_write = millis();
    pTxCharacteristic->setValue(send_queue[0].buf, send_queue[0].len);
    pTxCharacteristic->notify();

    BLE_DEBUG_PRINTLN("writeBytes: sz=%d, hdr=%d", (uint32_t)send_queue[0].len, (uint32_t) send_queue[0].buf[0]);

    send_queue_len--;
    for (int i = 0; i < send_queue_len; i++) {   // delete top item from queue
      send_queue[i] = send_queue[i + 1];
    }
  }

  Frame frame;
  if (xQueueReceive(recv_queue, &frame, 0) == pdTRUE) {
    memcpy(dest, frame.buf, frame.len);
    BLE_DEBUG_PRINTLN("readBytes: sz=%d, hdr=%d", (uint32_t) frame.len, (uint32_t) dest[0]);
    return frame.len;
  }

  if (deviceConnected != oldDeviceConnected) {
    if (!deviceConnected) {    // disconnecting
      clearBuffers();

      BLE_DEBUG_PRINTLN("SerialBLEInterface -> disconnecting...");

      //pServer->getAdvertising()->setMinInterval(500);
      //pServer->getAdvertising()->setMaxInterval(1000);

      adv_restart_time = millis() + ADVERT_RESTART_DELAY;
    } else {
      BLE_DEBUG_PRINTLN("SerialBLEInterface -> stopping advertising");
      BLE_DEBUG_PRINTLN("SerialBLEInterface -> connecting...");
      // connecting
      // do stuff here on connecting
      pServer->getAdvertising()->stop();
      adv_restart_time = 0;
    }
    oldDeviceConnected = deviceConnected;
  }

  if (adv_restart_time && millis() >= adv_restart_time) {
    if (pServer->getConnectedCount() == 0) {
      BLE_DEBUG_PRINTLN("SerialBLEInterface -> re-starting advertising");
      pServer->getAdvertising()->start();  // re-Start advertising
    }
    adv_restart_time = 0;
  }
  return 0;
}

bool SerialBLEInterface::isConnected() const {
  // Phase F Retry: bei abgeschaltetem Controller sicher false.
  if (_ctrl_disabled) return false;
  return deviceConnected;  //pServer != NULL && pServer->getConnectedCount() > 0;
}
