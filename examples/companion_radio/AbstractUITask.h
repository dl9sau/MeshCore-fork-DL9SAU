#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/BaseSerialInterface.h>
#include <Arduino.h>

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif

#include "NodePrefs.h"

enum class UIEventType {
    none,
    contactMessage,
    channelMessage,
    // DL9SAU 2026-06-17 (Wunschliste 75): Channel-Type-Discriminator
    // fuer buzzer_profile. channelMessage bleibt als Alias fuer
    // channelMessagePublic (Default-Behavior bei alten Callsites).
    channelMessagePublic  = channelMessage,
    channelMessagePrivate,
    roomMessage,
    newContactMessage,
    ack
};

class AbstractUITask {
protected:
  mesh::MainBoard* _board;
  BaseSerialInterface* _serial;
  bool _connected;

  AbstractUITask(mesh::MainBoard* board, BaseSerialInterface* serial) : _board(board), _serial(serial) {
    _connected = false;
  }

public:
  void setHasConnection(bool connected) { _connected = connected; }
  bool hasConnection() const { return _connected; }
  uint16_t getBattMilliVolts() const { return _board->getBattMilliVolts(); }
  bool isSerialEnabled() const { return _serial->isEnabled(); }
  void enableSerial() { _serial->enable(); }
  void disableSerial() { _serial->disable(); }
  virtual void msgRead(int msgcount) = 0;
  virtual void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) = 0;
  virtual void notify(UIEventType t = UIEventType::none) = 0;
  virtual void loop() = 0;
  // Wunschliste 58 Phase A (2026-06-14): nach 'set display' direkt
  // anwenden statt erst beim naechsten Event/Refresh. Liest
  // _node_prefs->display_wake_mode:
  //   0 = off                -> display.turnOff() sofort
  //   1 = on                 -> display.turnOn() + Auto-Off canceln
  //   2 = on-at-new-messages -> aktueller Zustand bleibt
  // Default-Implementation als No-Op fuer UI-Varianten ohne Display.
  virtual void applyDisplayWakeMode() {}
};
