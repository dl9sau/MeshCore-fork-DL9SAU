#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <helpers/NRF52Board.h>

class T1000eBoard : public NRF52BoardDCDC {
protected:
  uint8_t btn_prev_state;
  // DL9SAU 2026-07-12: true = powerOff() haelt PIN_3V3_EN HIGH, damit der
  // Batterie-Divider im SYSTEMOFF versorgt bleibt und der LPCOMP-Wake
  // (armBatteryWake) die Spannung lesen kann. Wird in armBatteryWake gesetzt.
  bool _wake_keep_3v3 = false;
  // DL9SAU 2026-07-12: letzter armierter LPCOMP-refsel + geschaetzte Weck-
  // Akkuspannung (fuer CLI-Anzeige/Kalibrierung). 0xFF/0 = nicht armiert.
  uint8_t  _wake_refsel  = 0xFF;
  uint16_t _wake_est_mv  = 0;

public:
  T1000eBoard() : NRF52Board("T1000E_OTA") {}
  void begin();
  // DL9SAU 2026-07-12 (Weg A): LPCOMP+VBUS-Recovery-Wake mit recovery_mv
  // armieren (mappt mV -> refsel via PWRMGT_VDD_MV). No-op ohne
  // NRF52_POWER_MANAGEMENT bzw. recovery_mv==0.
  void armBatteryWake(uint16_t recovery_mv);
  // Letzter armierter refsel (0xFF = nicht armiert) + geschaetzte Weck-mV.
  uint8_t  wakeRefsel() const { return _wake_refsel; }
  uint16_t wakeEstMv()  const { return _wake_est_mv; }

  uint16_t getBattMilliVolts() override {
  #ifdef BATTERY_PIN
   #ifdef PIN_3V3_EN
    digitalWrite(PIN_3V3_EN, HIGH);
   #endif
    analogReference(AR_INTERNAL_3_0);
    analogReadResolution(12);
    delay(10);
    float volts = (analogRead(BATTERY_PIN) * ADC_MULTIPLIER * AREF_VOLTAGE) / 4096;
   #ifdef PIN_3V3_EN
    digitalWrite(PIN_3V3_EN, LOW);
   #endif

    analogReference(AR_DEFAULT);  // put back to default
    analogReadResolution(10);

    return volts * 1000;
  #else
    return 0;
  #endif
  }

  const char* getManufacturerName() const override {
    return "Seeed Tracker T1000-E";
  }

  int buttonStateChanged() {
  #ifdef BUTTON_PIN
    uint8_t v = digitalRead(BUTTON_PIN);
    if (v != btn_prev_state) {
      btn_prev_state = v;
      return (v == USER_BTN_PRESSED) ? 1 : -1;
    }
  #endif
    return 0;
  }

  void powerOff() override {
    #ifdef HAS_GPS
        digitalWrite(GPS_VRTC_EN, LOW);
        digitalWrite(GPS_RESET, LOW);
        digitalWrite(GPS_SLEEP_INT, LOW);
        digitalWrite(GPS_RTC_INT, LOW);
        digitalWrite(GPS_EN, LOW);
    #endif

    #ifdef BUZZER_EN
        digitalWrite(BUZZER_EN, LOW);
    #endif

    #ifdef PIN_3V3_EN
        // DL9SAU 2026-07-12: bei armiertem LPCOMP-Wake HIGH lassen, damit der
        // Batterie-Divider im SYSTEMOFF versorgt bleibt (Weg A). Sonst LOW.
        digitalWrite(PIN_3V3_EN, _wake_keep_3v3 ? HIGH : LOW);
    #endif

    #ifdef PIN_3V3_ACC_EN
        digitalWrite(PIN_3V3_ACC_EN, LOW);
    #endif
    #ifdef SENSOR_EN
        digitalWrite(SENSOR_EN, LOW);
    #endif

    // set led on and wait for button release before poweroff
    #ifdef LED_PIN
    digitalWrite(LED_PIN, HIGH);
    #endif
    #ifdef BUTTON_PIN
    while(digitalRead(BUTTON_PIN));
    #endif
    #ifdef LED_PIN
    digitalWrite(LED_PIN, LOW);
    #endif

    #ifdef BUTTON_PIN
    // DL9SAU 2026-06-20: NOPULL zurueck. PULLDOWN-Versuch (war kurz
    // aktiv) hat Button-Press-Wake unzuverlaessig gemacht -- der
    // schwache Pulldown gegen schwachen Button-Pull = Pin kommt
    // nicht ueber SENSE_HIGH-Threshold. Phantom-Wake via Floating
    // wird jetzt durch shutdown_pending-Pref-Check in
    // MyMesh::applyShutdownPendingCheck() abgefangen (50/500ms USB-
    // Sampling). NOPULL ist Hardware-Standard von Adafruit-Pattern.
    nrf_gpio_cfg_sense_input(BUTTON_PIN, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_SENSE_HIGH);
    #endif

    NRF52Board::powerOff();
  }
};
