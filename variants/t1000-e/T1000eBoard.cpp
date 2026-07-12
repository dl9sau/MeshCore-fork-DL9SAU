#include <Arduino.h>
#include <Wire.h>

#include "T1000eBoard.h"

void T1000eBoard::begin() {
  NRF52BoardDCDC::begin();
  btn_prev_state = HIGH;

#ifdef BUTTON_PIN
  pinMode(BATTERY_PIN, INPUT);
  // DL9SAU 2026-06-20 (Wunschliste 94/90 Phase 2): BUTTON_PIN war
  // INPUT (NOPULL = floating). T1000-E hat keinen externen Pulldown
  // -> digitalRead random HIGH -> die while(digitalRead(BUTTON_PIN))
  // Schleife in powerOff() (T1000eBoard.h Z 81) lief endlos -> WDT
  // triggerte nach 90s. Symptom: 'shutdown' CLI + 'usb_loss_shutdown'
  // hingen 90s, danach WDT-Reset = Reboot statt Aus.
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  pinMode(LED_PIN, OUTPUT);
#endif

#if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
  Wire.setPins(PIN_BOARD_SDA, PIN_BOARD_SCL);
#endif

  Wire.begin();

  delay(10);   // give sx1262 some time to power up
}

// DL9SAU 2026-07-12 (Weg A): LPCOMP + VBUS als Recovery-Wake armieren. Nach
// einem Low-Battery-SYSTEMOFF weckt der HW-Comparator, sobald die Akkuspannung
// recovery_mv ueberschreitet (Auto-Boot). VBUS weckt zusaetzlich beim USB-Plug.
// mV -> refsel: AIN0 = Akku/2 (ADC_MULTIPLIER), LPCOMP vergleicht gegen
// refsel-Bruchteile von PWRMGT_VDD_MV (KALIBRIER-Knopf). refsel wird auf die
// kleinste /8-Stufe aufgerundet, deren Schwelle >= Ziel liegt (nie zu frueh).
void T1000eBoard::armBatteryWake(uint16_t recovery_mv) {
#ifdef NRF52_POWER_MANAGEMENT
  if (recovery_mv == 0) return;   // Feature aus (chemistry=none)
  uint32_t ain_mv  = (uint32_t)recovery_mv / 2;
  uint32_t eighths = (ain_mv * 8 + (PWRMGT_VDD_MV - 1)) / PWRMGT_VDD_MV;  // ceil
  if (eighths < 1) eighths = 1;
  if (eighths > 7) eighths = 7;
  uint8_t  refsel      = (uint8_t)(eighths - 1);              // 0..6 = 1/8..7/8 VDD
  uint32_t est_wake_mv = (eighths * (uint32_t)PWRMGT_VDD_MV / 8) * 2;  // zurueck auf Akku
  configureVoltageWake(PWRMGT_LPCOMP_AIN, refsel);           // LPCOMP + VBUS
  _wake_keep_3v3 = true;   // powerOff() haelt PIN_3V3_EN HIGH fuer den Divider
  _wake_refsel   = refsel;                 // fuer CLI-Anzeige / Kalibrierung
  _wake_est_mv   = (uint16_t)est_wake_mv;
  MESH_DEBUG_PRINTLN("PWRMGT: batt-wake armed refsel=%u (~%lumV est, target %umV) -- CALIBRATE PWRMGT_VDD_MV",
                     (unsigned)refsel, (unsigned long)est_wake_mv, (unsigned)recovery_mv);
#else
  (void)recovery_mv;
#endif
}