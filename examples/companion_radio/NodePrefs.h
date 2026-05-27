#pragma once
#include <cstdint> // For uint8_t, uint32_t

#define TELEM_MODE_DENY            0
#define TELEM_MODE_ALLOW_FLAGS     1     // use contact.flags
#define TELEM_MODE_ALLOW_ALL       2

#define ADVERT_LOC_NONE       0
#define ADVERT_LOC_SHARE      1

// Scope-Registry: Liste A — bekannte Scopes (Region-Name + 16-Byte
// TransportKey = SHA-256("#name") + optionale Geo-Bbox). Pre-populated
// beim ersten Boot mit den hartcodierten Boxen (#bebb, #ostfriesland).
// User erweiterbar per 'scope add'. Liste B (Repeat-Allowlist) wird
// ueber das Flag SCOPE_FLAG_IN_REPEAT_LIST pro Eintrag markiert.
//
// Vergleich gegen ein eingehendes Paket erfolgt nicht ueber den Key-Inhalt
// direkt: transport_codes[0] in Paketen ist ein 2-Byte HMAC-Wert. Wir
// nutzen TransportKey::calcTransportCode(packet) wie in
// lookupRegionByTransportCode() — daher das Speichern der vollen 16 Byte.
#define SCOPE_REG_SLOTS              16
#define SCOPE_FLAG_HAS_GEO_BOX       0x01   // bbox_* sind gueltig
#define SCOPE_FLAG_GEO_MANAGED       0x02   // bei Geo-Eintritt/Austritt
                                            //   IN_REPEAT_LIST automatisch
                                            //   setzen/loeschen
#define SCOPE_FLAG_IN_REPEAT_LIST    0x04   // Liste B (Repeat-Policy)
#define SCOPE_FLAG_DISABLED          0x08   // in repeat-list konfiguriert,
                                            //   aber inaktiv (nicht repeated).
                                            //   Damit kann der User einzelne
                                            //   Eintraege voruebergehend
                                            //   stilllegen ohne sie zu
                                            //   verlieren.

#define REPEAT_SCOPE_MODE_ALL        0      // heute: jeden scoped repeaten
#define REPEAT_SCOPE_MODE_ALLOWLIST  1      // nur Eintraege mit IN_REPEAT_LIST

struct ScopeRegEntry {
  char     name[16];          // ohne fuehrendes '#', null-terminated
  uint8_t  key[16];           // SHA-256("#" + name) (= TransportKey-Bytes)
  uint8_t  flags;
  uint8_t  _reserved[3];      // padding fuer 4-Byte Alignment der floats
  float    bbox_lat_min;
  float    bbox_lat_max;
  float    bbox_lon_min;
  float    bbox_lon_max;
};                            // = 16 + 16 + 1 + 3 + 16 = 52 Byte/Slot

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
  // Auto-Adverts als Bitmask:
  //   bit 0 (AUTO_ADV_ZEROHOP, 0x01) = periodic zero-hop (15/60/180 min adaptiv)
  //   bit 1 (AUTO_ADV_NIGHTLY, 0x02) = nightly scoped flood (random 23-5 lokal)
  // Default 0 = beides aus. Aktivierung via Companion-CLI "autoadv on" oder
  // einzeln per "autoadv zerohop on" / "autoadv nightly on".
  // Migration: alter Wert 1 (= "on" mit beiden flags zusammen) wird beim
  // ersten Boot mit dieser Firmware-Version auf 3 erhoben.
  uint8_t auto_advert_enabled;
  // Persistente Erinnerung dass der Repeater explizit per Companion-Chat
  // "repeater on force" aktiviert wurde - App-Pfad CMD_SET_RADIO_PARAMS
  // ueberspringt damit den strict-Range-Check und schreibt _prefs durch.
  // Wird gecleared sobald der Repeater per App auf 0 gesetzt wird ODER
  // per Companion "repeater on" (ohne force) bzw. "repeater off" - so muss
  // der force-Modus explizit per Companion-Geste reaktiviert werden.
  uint8_t client_repeat_force;
  // Duty-Cycle-Schwellen in % der regulatorischen 10% TX-Airtime-Grenze
  // (= 360 s in einem rollenden 1h-Fenster). Default 80 / 100.
  //   duty_soft_pct (80) -> bei Erreichen werden Forward/Repeats abgelehnt
  //   duty_hard_pct(100) -> bei Erreichen werden ALLE TX blockiert (auch
  //                         User-Chat) - schuetzt gegen unbeabsichtigtes
  //                         Spam-Verhalten z.B. eines Chat-Bots ueber USB.
  uint8_t duty_soft_pct;
  uint8_t duty_hard_pct;
  // Nightly-Bake-Scope (persistent). Separater Scope NUR fuer den
  // periodischen Flood-Advert ("nightly bake" + "advert flood"-Befehl).
  // Default-Scope (oben) bleibt fuer regulaere Sends; Bake darf bewusst
  // weiter gehen (z.B. default=#de-be lokal, bake=#de-bebb fuer groesseren
  // Outreach in Berlin/Brandenburg).
  char    bake_scope_name[31];
  uint8_t bake_scope_key[16];
  // Override-Scope (persistent ueber Reboots damit Power-Cycles waehrend
  // einer mehrtaegigen Reise nicht den Override killen).
  // override_expiry = unix-sec wann Override ablaeuft. 0 = inaktiv.
  // Hat Prioritaet 1 in chooseNightFloodScope (vor bake, default, geo).
  char     override_scope_name[31];
  uint8_t  override_scope_key[16];
  uint32_t override_expiry;
  // Persistent gespeicherte Trace-Kategorien-Selektion. Active flags
  // (_trace_flags im RAM) starten bei Reboot auf 0; per "trace on" wird
  // active = trace_flags_persistent wiederhergestellt. Damit ist eine
  // einmalige Auswahl ueber Reboots stabil ohne dass die Firmware
  // automatisch wieder zu loggen anfaengt.
  uint16_t trace_flags_persistent;
  // GPS Power-Management Konfiguration:
  //   gps_power_mode = 0 (cycle, Default) -> Power-Cycle aktiv,
  //                                          GPS schlaeft zwischen Adverts
  //   gps_power_mode = 1 (always-on)     -> GPS dauerhaft an, kein Cycling
  //   gps_lead_min  (1..14, Default 5)   -> Wake-Zeit vor Advert in Minuten.
  //                                         Sleep-Dauer = (15 - lead) min im
  //                                         hardcoded 15-min Motion-Check-Cycle.
  uint8_t gps_power_mode;
  uint8_t gps_lead_min;
  // Retransmit-Delay-Faktoren (analog simple_repeater / CommonCLI txdelay/
  // direct_txdelay). Faktor multipliziert mit Pkt-Airtime, +/- 5x RNG-Spread.
  // 0 wird in MyMesh::begin() als uninitialisiert gewertet und einmalig auf
  // den Companion-Default (0.5 / 0.2 — die alten hartcodierten Werte) gesetzt.
  float tx_delay_factor;
  float direct_tx_delay_factor;
  // Scope-Registry (Liste A) + Repeat-Policy (Mode + IN_REPEAT_LIST-Flag
  // pro Eintrag). scope_registry_count == 0 triggert in begin() einmalige
  // Pre-Population mit den hartcodierten Geo-Boxen (#bebb, #ostfriesland).
  uint8_t        scope_registry_count;
  uint8_t        repeat_scope_mode;          // REPEAT_SCOPE_MODE_*
  ScopeRegEntry  scope_registry[SCOPE_REG_SLOTS];
};