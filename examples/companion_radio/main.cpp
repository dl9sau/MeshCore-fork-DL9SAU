#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>
#include "MyMesh.h"

#if defined(ESP32) && !defined(WIFI_SSID)
  #include <WiFi.h>
  #include <esp_wifi.h>
#endif

// Wunschliste 53 (2026-06-14): Hardware-Watchdog. Schuetzt vor Hang im
// loop() (Memory-Corruption, Stack-Overflow, infinite Callback-Loop) --
// loest dann Panic-Reboot statt 'tot im Feld'.
// Aktivierung gated von Pref watchdog_mode (Default off) -- Stage 1+2
// nach Aenderung 2026-06-14: ESP32 TWDT + NRF52 nrfx_wdt.
// Aktivierung am Ende vom ersten loop() (nicht setup()), damit BLE/
// LoRa/Sensor-Init nicht durch falsche Pet-Annahmen reboot triggern
// koennen -- Meshtastic-Pattern.
#ifndef WATCHDOG_TIMEOUT_S
  #define WATCHDOG_TIMEOUT_S 90   // 90s: Meshtastic-bewaehrt. LoRa-TX
                                  // ~10s + flash + Margin -- async
                                  // Companion-Ops blockieren loop() eh
                                  // nicht. Override -D moeglich.
#endif
#ifndef WATCHDOG_SKIP_WIN_MS
  #define WATCHDOG_SKIP_WIN_MS (10UL * 60UL * 1000UL)  // 10 min Skip-
                                    // Window wenn letzter Reset WDT war.
                                    // Schuetzt vor Boot-Loop und gibt
                                    // dem User Zeit das Log anzuschauen.
#endif

#if defined(ESP32)
  #include "esp_task_wdt.h"
#endif
#if defined(NRF52_PLATFORM)
  // HAL-only (header-inline). nrfx_wdt-Driver waere die High-Level-API,
  // braucht aber dass nrfx_wdt.c im Build inkludiert wird (Meshtastic
  // umgeht das mit '#include <nrfx_wdt.c>'). HAL ist direkter +
  // ausreichend fuer unser One-Channel-Pet-Pattern.
  #include "nrf_wdt.h"
  #include <nrf_soc.h>
  #include <nrf_sdm.h>
  // 2026-06-15: 'dfu'-CLI fuer NRF52 (SenseCap T1000E etc.). Spart die
  // fehlertraechtige Reset-Button-Doppelklick-Sequenz beim Firmware-
  // Update. enterUf2Dfu()/enterSerialDfu() von Adafruit-nRF52 wiring.h:
  // setzt GPREGRET-Magic + NVIC_SystemReset -> Bootloader landet im
  // DFU-Mode statt App-Start. Mechanismus von Meshtastic uebernommen.
  extern "C" void enterUf2Dfu(void);
  extern "C" void enterSerialDfu(void);
  // 2026-06-15 BUGFIX: SenseCap T1000E hing nach Flash in early boot.
  // Root cause: NRF_POWER->RESETREAS direkt zu lesen+clearen ist
  // erlaubt SOLANGE SoftDevice noch NICHT aktiv ist. Wenn wir's spaeter
  // in setup() versuchen (nach board.begin() -> Bluefruit/SoftDevice
  // aktiv), gibt's HardFault -> Reboot-Loop -> BLE nie sichtbar.
  // Fix: Pre-SoftDevice-Capture via Constructor-Priority 101 (laeuft
  // vor SystemInit (102) und vor C++-Constructors). Pattern stammt
  // aus src/helpers/NRF52Board.cpp. Spaeter im map-Code lesen wir nur
  // diese Variable; das Clear muss SoftDevice-safe sein (SVC oder
  // direkt -- je nach Status).
  static uint32_t s_nrf52_resetreas_captured = 0;
  static void __attribute__((constructor(101)))
              wdtCaptureResetReason(void) {
    s_nrf52_resetreas_captured = NRF_POWER->RESETREAS;
  }
#endif

// Reset-Reason Mapping (Plattform-uebergreifend einheitlich):
enum WdtResetKind : uint8_t {
  WDT_RESET_UNKNOWN  = 0,
  WDT_RESET_COLD     = 1,  // power-on, BOR
  WDT_RESET_WARM     = 2,  // user "reboot" CLI, Reset-Button, sw-reset
  WDT_RESET_WDT      = 3,  // watchdog timeout
  WDT_RESET_PANIC    = 4,  // panic / abort / lockup
  WDT_RESET_BROWNOUT = 5,  // ESP32 only -- NRF52 hat keine Direkt-
                           // entsprechung, wir mappen das auf COLD.
};

// State-Machine fuer WDT-Activation:
enum WdtState : uint8_t {
  WDT_STATE_OFF      = 0,  // Pref off -- niemals aktivieren
  WDT_STATE_PENDING  = 1,  // arm am Ende vom ersten loop()
  WDT_STATE_SKIP_WIN = 2,  // letzter Reset war WDT -- wait 10 min
  WDT_STATE_ACTIVE   = 3,  // aktiv -- pet bei jedem loop()
};

static WdtResetKind _wdt_last_reset    = WDT_RESET_UNKNOWN;
static WdtState     _wdt_state         = WDT_STATE_OFF;
static uint32_t     _wdt_skip_until_ms = 0;

static WdtResetKind wdtMapResetReason() {
#if defined(ESP32)
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return WDT_RESET_COLD;
    case ESP_RST_SW:       return WDT_RESET_WARM;
    case ESP_RST_EXT:      return WDT_RESET_WARM;
    case ESP_RST_PANIC:    return WDT_RESET_PANIC;
    case ESP_RST_INT_WDT:  return WDT_RESET_WDT;
    case ESP_RST_TASK_WDT: return WDT_RESET_WDT;
    case ESP_RST_WDT:      return WDT_RESET_WDT;
    case ESP_RST_BROWNOUT: return WDT_RESET_BROWNOUT;
    default:               return WDT_RESET_UNKNOWN;
  }
#elif defined(NRF52_PLATFORM)
  // 2026-06-15 BUGFIX: NICHT direkt von NRF_POWER->RESETREAS lesen --
  // SoftDevice ist hier (nach board.begin()) schon aktiv, direkter
  // Zugriff = HardFault. Wir lesen den vor SystemInit captured Wert
  // (siehe constructor wdtCaptureResetReason oben) und clearen via
  // SoftDevice-aware Pfad.
  uint32_t r = s_nrf52_resetreas_captured;
  uint8_t sd_enabled = 0;
  sd_softdevice_is_enabled(&sd_enabled);
  if (sd_enabled) {
    sd_power_reset_reason_clr(0xFFFFFFFFUL);
  } else {
    NRF_POWER->RESETREAS = 0xFFFFFFFFUL;
  }
  if (r & POWER_RESETREAS_DOG_Msk)      return WDT_RESET_WDT;
  if (r & POWER_RESETREAS_LOCKUP_Msk)   return WDT_RESET_PANIC;
  if (r & POWER_RESETREAS_SREQ_Msk)     return WDT_RESET_WARM;
  if (r & POWER_RESETREAS_RESETPIN_Msk) return WDT_RESET_WARM;
  return WDT_RESET_COLD;  // OFF / POR / BOR / r==0 -> kalt
#else
  return WDT_RESET_UNKNOWN;
#endif
}

static const char* resetKindStr(WdtResetKind k) {
  switch (k) {
    case WDT_RESET_COLD:     return "COLD";
    case WDT_RESET_WARM:     return "WARM";
    case WDT_RESET_WDT:      return "WDT";
    case WDT_RESET_PANIC:    return "PANIC";
    case WDT_RESET_BROWNOUT: return "BROWNOUT";
    default:                 return "UNKNOWN";
  }
}

static void activateWatchdog() {
#if defined(ESP32)
  // arduino-esp32 hat den TWDT schon initialisiert (5s default fuer IDLE-
  // Tasks). Wir rekonfigurieren auf unser Timeout + adden loopTask.
  // API-Unterschied: ESP-IDF 4.x (espressif32@6.x, arduino-esp32 2.x)
  // bietet nur die alte init(timeout, panic)-Form, IDF 5.x die neue
  // reconfigure(config_t*)-Form.
  #if ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t cfg = {
      .timeout_ms     = (uint32_t)WATCHDOG_TIMEOUT_S * 1000UL,
      .idle_core_mask = 0,
      .trigger_panic  = true,
    };
    esp_task_wdt_reconfigure(&cfg);
  #else
    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
  #endif
  esp_task_wdt_add(NULL);   // loopTask
#endif
#if defined(NRF52_PLATFORM)
  // NRF52 WDT ist NICHT stoppbar einmal gestartet -- nur Reset stoppt ihn.
  // Daher Timeout grosszuegig + PAUSE_SLEEP_HALT-Behavior:
  // pausiert im System-ON-Sleep (Bluefruit/SoftDevice WFE) und im
  // Debugger-HALT -- kein faelschliches Trigger bei legitimen Idle-Phasen.
  // CRV-Register: Zaehler in 32.768kHz-Ticks. 90s * 32768 = 2949120.
  nrf_wdt_behaviour_set(NRF_WDT, NRF_WDT_BEHAVIOUR_PAUSE_SLEEP_HALT);
  nrf_wdt_reload_value_set(NRF_WDT,
      (uint32_t)WATCHDOG_TIMEOUT_S * 32768UL);
  nrf_wdt_reload_request_enable(NRF_WDT, NRF_WDT_RR0);
  nrf_wdt_task_trigger(NRF_WDT, NRF_WDT_TASK_START);
#endif
}

static void petWatchdog() {
#if defined(NRF52_PLATFORM)
  // 2026-06-15 T1000-E Workaround: wenn CLI 'reboot' oder 'dfu' das
  // shouldStopPettingWdt-Flag gesetzt hat, NICHT mehr petten. WDT
  // feuert dann nach <=5s (frisch gestartet via armWdtReset) bzw.
  // <=90s (bestehender App-WDT). Universeller Reset-Mechanismus weil
  // NVIC_SystemReset auf T1000-E nicht zuverlaessig greift.
  if (the_mesh.shouldStopPettingWdt()) return;
#endif
  if (_wdt_state != WDT_STATE_ACTIVE) return;
#if defined(ESP32)
  esp_task_wdt_reset();
#endif
#if defined(NRF52_PLATFORM)
  nrf_wdt_reload_request_set(NRF_WDT, NRF_WDT_RR0);
#endif
}

// Wird am Ende vom loop() aufgerufen -- handle State-Transitions:
//   PENDING  -> ACTIVE  (nach erstem loop())
//   SKIP_WIN -> ACTIVE  (nach 10 min wenn letzter Reset WDT war)
static void maintainWatchdog() {
  if (_wdt_state == WDT_STATE_PENDING) {
    activateWatchdog();
    _wdt_state = WDT_STATE_ACTIVE;
  } else if (_wdt_state == WDT_STATE_SKIP_WIN) {
    // Wraparound-sicher: signed-Diff fuer 'jetzt >= until'
    if ((int32_t)(millis() - _wdt_skip_until_ms) >= 0) {
      activateWatchdog();
      _wdt_state = WDT_STATE_ACTIVE;
    }
  }
}

// Believe it or not, this std C function is busted on some platforms!
static uint32_t _atoi(const char* sp) {
  uint32_t n = 0;
  while (*sp && *sp >= '0' && *sp <= '9') {
    n *= 10;
    n += (*sp++ - '0');
  }
  return n;
}

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  #include <InternalFileSystem.h>
  #if defined(QSPIFLASH)
    #include <CustomLFS_QSPIFlash.h>
    DataStore store(InternalFS, QSPIFlash, rtc_clock);
  #else
  #if defined(EXTRAFS)
    #include <CustomLFS.h>
    CustomLFS ExtraFS(0xD4000, 0x19000, 128);
    DataStore store(InternalFS, ExtraFS, rtc_clock);
  #else
    DataStore store(InternalFS, rtc_clock);
  #endif
  #endif
#elif defined(RP2040_PLATFORM)
  #include <LittleFS.h>
  DataStore store(LittleFS, rtc_clock);
#elif defined(ESP32)
  #include <SPIFFS.h>
  DataStore store(SPIFFS, rtc_clock);
#endif

#ifdef ESP32
  #ifdef WIFI_SSID
    #include <helpers/esp32/SerialWifiInterface.h>
    SerialWifiInterface serial_interface;
    #ifndef TCP_PORT
      #define TCP_PORT 5000
    #endif
  #elif defined(BLE_PIN_CODE)
    #include <helpers/esp32/SerialBLEInterface.h>
    SerialBLEInterface serial_interface;
  #elif defined(SERIAL_RX)
    #include <helpers/ArduinoSerialInterface.h>
    ArduinoSerialInterface serial_interface;
    HardwareSerial companion_serial(1);
  #else
    #include <helpers/ArduinoSerialInterface.h>
    ArduinoSerialInterface serial_interface;
  #endif
#elif defined(RP2040_PLATFORM)
  //#ifdef WIFI_SSID
  //  #include <helpers/rp2040/SerialWifiInterface.h>
  //  SerialWifiInterface serial_interface;
  //  #ifndef TCP_PORT
  //    #define TCP_PORT 5000
  //  #endif
  // #elif defined(BLE_PIN_CODE)
  //   #include <helpers/rp2040/SerialBLEInterface.h>
  //   SerialBLEInterface serial_interface;
  #if defined(SERIAL_RX)
    #include <helpers/ArduinoSerialInterface.h>
    ArduinoSerialInterface serial_interface;
    HardwareSerial companion_serial(1);
  #else
    #include <helpers/ArduinoSerialInterface.h>
    ArduinoSerialInterface serial_interface;
  #endif
#elif defined(NRF52_PLATFORM)
  #ifdef BLE_PIN_CODE
    #include <helpers/nrf52/SerialBLEInterface.h>
    SerialBLEInterface serial_interface;
  #else
    #include <helpers/ArduinoSerialInterface.h>
    ArduinoSerialInterface serial_interface;
  #endif
#elif defined(STM32_PLATFORM)
  #include <helpers/ArduinoSerialInterface.h>
  ArduinoSerialInterface serial_interface;
#else
  #error "need to define a serial interface"
#endif

/* GLOBAL OBJECTS */
#ifdef DISPLAY_CLASS
  #include "UITask.h"
  UITask ui_task(&board, &serial_interface);
#endif

StdRNG fast_rng;
SimpleMeshTables tables;
MyMesh the_mesh(radio_driver, fast_rng, rtc_clock, tables, store
   #ifdef DISPLAY_CLASS
      , &ui_task
   #endif
);

/* END GLOBAL OBJECTS */

void halt() {
  while (1) ;
}

/* WIFI RECONNECT TRACKERS */
#if defined(ESP32) && defined(WIFI_SSID)
  bool wifi_needs_reconnect = false;
  unsigned long last_wifi_reconnect_attempt = 0;
#endif

void setup() {
  Serial.begin(115200);
  // DL9SAU 2026-06-01 v2: USB-CDC TX-Timeout sehr klein halten. Verhindert
  // loop()-Stalls wenn das Geraet ohne USB-Host laeuft (z.B. Powerbank) und
  // der TX-FIFO sich fuellt -- ohne diesen Hint koennte Serial.write() bis
  // zu 100 ms blocken (HWCDC-Default), was iOS-BLE-Verbindungen abreissen
  // laesst.
  //
  // ACHTUNG: setTxTimeoutMs(0) NICHT benutzen. Arduino-ESP32 HWCDC.cpp hat
  // einen Underflow-Bug -- die innere 'tries'-Variable (uint32_t) startet
  // bei tx_timeout_ms, wird auf 'kein Progress' dekrementiert, und wrappt
  // zu 0xFFFFFFFF wenn sie bei 0 startet. Die 'tries == 0'-Abbruchbedingung
  // wird dann nie wahr. Folge: Serial.write() blockiert effektiv unendlich
  // wenn HWCDC den Host als 'connected' sieht aber das FIFO nicht drained
  // (Boot ohne USB-Host -> Tracker bleibt an "Loading..." haengen, bis
  // jemand cu / Serial-Console oeffnet). Empirisch nachgewiesen 2026-05-31.
  //
  // Mit einem kleinen aber non-zero Wert wird der Underflow vermieden
  // und Blockzeiten sind unter der BLE-Supervision-Schwelle.
  // 1 ms statt frueher 5 ms (User-Report 2026-06-02): bei Powerbank-Use
  // und vielen kurz aufeinander folgenden Serial.write-Aufrufen (z.B.
  // 'backup save' mit hunderten Lines) summieren sich 5 ms × N zu BLE-
  // Supervision-Reiss-Time. 1 ms × N ist 5x sicherer; zusaetzlich gibt
  // es jetzt 'if (Serial)'-Guards an den heissen Stellen (backupSave,
  // pushDebugLog).
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(1);
#endif

#ifdef ESP32
  // DL9SAU 2026-06-02: ESP_LOG-Flut der Arduino-ESP32-Libraries
  // (insb. BLE-Stack) komplett stillstellen. Beobachtet: bei BLE-
  // disconnect spammed BLECharacteristic::notify() 'rc=-1 Unknown
  // ESP_ERR' im Sekundentakt -- jeder log_e()-Aufruf erzeugt
  // Serial-Output, was bei nicht-drained CDC zu Block-Akkumulation
  // fuehrt. Wir haben unseren eigenen pushDebugLog (gated von
  // log_flags), brauchen die generischen ESP-LIB-Errors nicht im
  // Companion-Channel-Build.
  // Falls jemand fuer ESP-IDF-Debugging die Logs wieder will,
  // diesen Aufruf temporaer auskommentieren.
  esp_log_level_set("*", ESP_LOG_NONE);
#endif

#if defined(ESP32) && !defined(WIFI_SSID)
  // Power down the WiFi side of the radio when only BLE/USB is used.
  // Saves ~10-20 mA on ESP32-S3 idle current.
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
  esp_wifi_deinit();
#endif

  board.begin();

#ifdef DISPLAY_CLASS
  DisplayDriver* disp = NULL;
  if (display.begin()) {
    disp = &display;
    disp->startFrame();
  #ifdef ST7789
    disp->setTextSize(2);
  #endif
    disp->drawTextCentered(disp->width() / 2, 28, "Loading...");
    disp->endFrame();
  }
#endif

  if (!radio_init()) { halt(); }

  fast_rng.begin(radio_driver.getRngSeed());

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  InternalFS.begin();
  #if defined(QSPIFLASH)
    if (!QSPIFlash.begin()) {
      // debug output might not be available at this point, might be too early. maybe should fall back to InternalFS here?
      MESH_DEBUG_PRINTLN("CustomLFS_QSPIFlash: failed to initialize");
    } else {
      MESH_DEBUG_PRINTLN("CustomLFS_QSPIFlash: initialized successfully");
    }
  #else
  #if defined(EXTRAFS)
      ExtraFS.begin();
  #endif
  #endif
  store.begin();
  the_mesh.begin(
    #ifdef DISPLAY_CLASS
        disp != NULL
    #else
        false
    #endif
  );

#ifdef BLE_PIN_CODE
  serial_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin());
#else
  serial_interface.begin(Serial);
#endif
  the_mesh.startInterface(serial_interface);
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  store.begin();
  the_mesh.begin(
    #ifdef DISPLAY_CLASS
        disp != NULL
    #else
        false
    #endif
  );

  //#ifdef WIFI_SSID
  //  WiFi.begin(WIFI_SSID, WIFI_PWD);
  //  serial_interface.begin(TCP_PORT);
  // #elif defined(BLE_PIN_CODE)
  //   char dev_name[32+16];
  //   sprintf(dev_name, "%s%s", BLE_NAME_PREFIX, the_mesh.getNodeName());
  //   serial_interface.begin(dev_name, the_mesh.getBLEPin());
  #if defined(SERIAL_RX)
    companion_serial.setPins(SERIAL_RX, SERIAL_TX);
    companion_serial.begin(115200);
    serial_interface.begin(companion_serial);
  #else
    serial_interface.begin(Serial);
  #endif
    the_mesh.startInterface(serial_interface);
#elif defined(ESP32)
  SPIFFS.begin(true);
  store.begin();
  the_mesh.begin(
    #ifdef DISPLAY_CLASS
        disp != NULL
    #else
        false
    #endif
  );

  // Wunschliste 58 Phase B (2026-06-13): CPU-Clock-Frequenz aus Pref
  // anwenden. Wert 0 = "Default behalten" (auf ESP32-S3 = 240 MHz).
  // Anwendung NACH the_mesh.begin() (Prefs geladen) und VOR
  // serial_interface.begin() / WiFi.begin() (BLE/WiFi-Stack-Init laeuft
  // dann mit neuer Frequenz, Stack-Timing-Kalibrierung stimmt).
  // User-Vergleich LoRa-APRS-Firmware: 80 MHz, 155 mA (kein BLE).
  //
  // SAFETY-FLOOR 80 MHz (User-Lockout 2026-06-14): 'set cpu.clock 40'
  // sah harmlos aus aber das Geraet locked sich mit Boot-Loop (abort()
  // an BLE/Radio-Init waehrend Stack-Initialisierung). User kommt dann
  // nicht mehr an die CLI um zurueckzusetzen. -> Werte < 80 hier
  // ignorieren (Default 240 wird angewendet); Pref bleibt fuer
  // Diagnose im Flash, kann via CLI 'set cpu.clock max' geheilt werden.
  {
    uint8_t cclk = the_mesh.getNodePrefs()->cpu_clock_mhz;
    if (cclk != 0 && cclk >= 80) {
      setCpuFrequencyMhz((uint32_t)cclk);
    }
    // else: Pref ungueltig oder 0 -> Build-Default behalten.
  }

#ifdef WIFI_SSID
  board.setInhibitSleep(true);   // prevent sleep when WiFi is active
  WiFi.setAutoReconnect(true);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
      if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
          WIFI_DEBUG_PRINTLN("WiFi disconnected. Flagging for reconnect...");
          wifi_needs_reconnect = true;
      } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
          WIFI_DEBUG_PRINTLN("WiFi connected successfully!");
          wifi_needs_reconnect = false;
      }
  });

  WiFi.begin(WIFI_SSID, WIFI_PWD);
  serial_interface.begin(TCP_PORT);
#elif defined(BLE_PIN_CODE)
  serial_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin());
#elif defined(SERIAL_RX)
  companion_serial.setPins(SERIAL_RX, SERIAL_TX);
  companion_serial.begin(115200);
  serial_interface.begin(companion_serial);
#else
  serial_interface.begin(Serial);
#endif
  the_mesh.startInterface(serial_interface);
#else
  #error "need to define filesystem"
#endif

  sensors.begin();

#if ENV_INCLUDE_GPS == 1
  the_mesh.applyGpsPrefs();
#endif

#ifdef DISPLAY_CLASS
  ui_task.begin(disp, &sensors, the_mesh.getNodePrefs());  // still want to pass this in as dependency, as prefs might be moved
#endif

  board.onBootComplete();

  // Wunschliste 53 (2026-06-14): Reset-Reason auslesen + an MyMesh
  // weitergeben fuer stats-core 'last_reset'-Anzeige + Boot-Log.
  _wdt_last_reset = wdtMapResetReason();
  the_mesh.setLastResetReason((uint8_t)_wdt_last_reset);
  // Persistenten Boot-Log um neuen Eintrag erweitern (Ring 10, oldest
  // out). Pusht zugleich '[boot] ...' an $companion-Channel damit User
  // den Reboot-Grund direkt im Chat sieht.
  the_mesh.bootLogAppend();
  // WDT-State initialisieren (kein activate hier -- erst am Ende vom
  // ersten loop(), damit BLE/LoRa-/Sensor-Init mit ihren langlaufenden
  // Stack-Inits den Pet-Cycle nicht verzoegern).
  if (the_mesh.getNodePrefs()->watchdog_mode == 1) {
    if (_wdt_last_reset == WDT_RESET_WDT) {
      // Letzter Reset war WDT-Trigger -- 10 min Skip-Window, dann scharf.
      _wdt_state         = WDT_STATE_SKIP_WIN;
      _wdt_skip_until_ms = millis() + WATCHDOG_SKIP_WIN_MS;
    } else {
      _wdt_state = WDT_STATE_PENDING;
    }
  }
}

void loop() {
  petWatchdog();   // No-op wenn nicht ACTIVE
  the_mesh.loop();
  sensors.loop();
#ifdef DISPLAY_CLASS
  ui_task.loop();
#endif
  rtc_clock.tick();
  maintainWatchdog();   // PENDING -> ACTIVE (nach 1. loop), SKIP -> ACTIVE
  delay(1);   // yield to FreeRTOS idle task; lets ESP32 idle-tick run (and, if
              // esp_pm light sleep is ever enabled, lets the CPU actually sleep)

  if (!the_mesh.hasPendingWork()) {
#if defined(NRF52_PLATFORM)
    board.sleep(0); // nrf ignores seconds param, sleeps whenever possible
#endif
  }

#if defined(ESP32) && defined(WIFI_SSID)
  // Safely attempt to reconnect every 10 seconds if flagged
  if (wifi_needs_reconnect && (millis() - last_wifi_reconnect_attempt > 10000)) {
    WIFI_DEBUG_PRINTLN("Attempting manual WiFi reconnect...");
    WiFi.disconnect();
    WiFi.reconnect();
    last_wifi_reconnect_attempt = millis();
  }
#endif
}
