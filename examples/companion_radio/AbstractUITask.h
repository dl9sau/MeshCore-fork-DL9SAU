#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/MultiSerialInterface.h>
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
  MultiSerialInterface* _interfaceManager;
  bool _connected;

  AbstractUITask(mesh::MainBoard* board, MultiSerialInterface* interfaceManager) : _board(board), _interfaceManager(interfaceManager) {
    _connected = false;
  }

public:
  void setHasConnection(bool connected) { _connected = connected; }
  bool hasConnection() const { return _connected; }
  uint16_t getBattMilliVolts() const { return _board->getBattMilliVolts(); }
  bool isBluetoothEnabled() const { return _interfaceManager->isBluetoothEnabled(); }
  void enableBluetooth() { _interfaceManager->enableBluetooth(); }
  void disableBluetooth() { _interfaceManager->disableBluetooth(); }
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
  // DL9SAU Wunschliste 94 (2026-06-19): UI-koordinierter Shutdown +
  // Reboot mit Buzzer-Sound. shutdown(restart) macht alles in einem
  // Aufruf (Sound + reboot/powerOff). playShutdownSound() nur Sound,
  // returns -- damit MyMesh den eigenen NRF52-Reset-Pfad (SD-disable
  // + NVIC-ICER + NVIC_SystemReset) selbst machen kann.
  virtual void shutdown(bool restart = false) {
    if (restart) _board->reboot();
    else         _board->powerOff();
  }
  virtual void playShutdownSound(uint32_t max_wait_ms = 3000) {
    (void)max_wait_ms;
  }
};
