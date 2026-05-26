#pragma once
#include <cstdint> // For uint8_t, uint32_t

#define TELEM_MODE_DENY            0
#define TELEM_MODE_ALLOW_FLAGS     1     // use contact.flags
#define TELEM_MODE_ALLOW_ALL       2

#define ADVERT_LOC_NONE       0
#define ADVERT_LOC_SHARE      1

struct NodePrefs {  // persisted to file
  float airtime_factor;
  char node_name[32];
  float freq;
  uint8_t sf;
  uint8_t cr;
  uint8_t multi_acks;
  uint8_t manual_add_contacts;
  float bw;
  int8_t tx_power_dbm;
  uint8_t telemetry_mode_base;
  uint8_t telemetry_mode_loc;
  uint8_t telemetry_mode_env;
  float rx_delay_base;
  uint32_t ble_pin;
  uint8_t  advert_loc_policy;
  uint8_t  buzzer_quiet;
  uint8_t  gps_enabled;      // GPS enabled flag (0=disabled, 1=enabled)
  uint32_t gps_interval;     // GPS read interval in seconds
  uint8_t autoadd_config;    // bitmask for auto-add contacts config
  uint8_t rx_boosted_gain; // SX126x RX boosted gain mode (0=power saving, 1=boosted)
  uint8_t client_repeat;
  uint8_t path_hash_mode;    // which path mode to use when sending
  uint8_t autoadd_max_hops;  // 0 = no limit, 1 = direct (0 hops), N = up to N-1 hops (max 64)
  char default_scope_name[31];
  uint8_t default_scope_key[16];
  // Konfigurierbarer Chat-Sendername (Group-Channel-Messages).
  //   0 = erste 2 Wörter aus node_name (Default, backward-compatible)
  //   1 = nur erstes Wort aus node_name
  //   2 = custom (chat_name_custom wird verwendet)
  // Werte über die NodePrefs am Ende angehängt damit alte persistierte
  // /new_prefs-Dateien weiterhin lesbar bleiben (file.read auf EOF lässt
  // die memset(0)-Defaults stehen, mode=0 = bisheriges Verhalten).
  uint8_t chat_name_mode;
  char    chat_name_custom[32];
  // Auto-Adverts ein/aus (zerohop periodic UND nightly flood gemeinsam).
  // Default 0 = aus: nach einem frischen Flash sendet die Firmware NICHTS
  // von selbst. Aktivierung explizit ueber den Companion-Chat-Befehl
  // "auto advert on". Vermeidet versehentliches Aussenden bei einer
  // Konfiguration die der User noch nicht freigegeben hat.
  uint8_t auto_advert_enabled;
};