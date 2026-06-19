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