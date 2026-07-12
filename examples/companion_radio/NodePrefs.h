#pragma once
#include <cstdint> // For uint8_t, uint32_t

// Sentinel-Wert fuer flood_max_infra: "folge flood_max persistent".
// Wert 254 -- gross genug dass path_hash_count > eff_cap nie wahr wird
// (Pfade max 64 Hops), klein genug dass uint8_t-Storage trivial bleibt
// und kein Konflikt mit 0xFF (potentielles "uninitialisiert"-Pattern).
#define FLOOD_MAX_INFRA_FOLLOW   254

// Sentinel-Wert fuer channel_hops_cap / flood_max_unknown_chan:
// "off" = kein per-channel-Cap, es greift flood_max. 0 ist KEIN
// Sentinel sondern bedeutet explizit "nicht repeaten". Werte 1..flood_max
// sind explizite Caps. Same numeric als FLOOD_MAX_INFRA_FOLLOW; semantisch
// gleich (= "kein extra Cap"), zur Konsistenz.
#define CH_HOPS_OFF              254

#define TELEM_MODE_DENY            0
#define TELEM_MODE_ALLOW_FLAGS     1     // use contact.flags
#define TELEM_MODE_ALLOW_ALL       2

#define ADVERT_LOC_NONE       0
#define ADVERT_LOC_SHARE      1
// DL9SAU 2026-06-17 (Wunschliste 80): PREFS-Mode -- Advert traegt eine
// stabile "Heimat"-Position (Snapshot beim Boot bzw. set lat/lon), Live-GPS
// kann parallel fuer Zeit-Sync laufen ohne den Advert-Standort zu veraendern.
#define ADVERT_LOC_PREFS      2

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


// ---- Scope-Architektur-Pivot (Wunschliste 11) ---------------------------
// Ab diesem Punkt: Build-in-Tabelle (in Flash, dl9sau_geo_recommendations.cpp)
// wird zur Quelle der bekannten Regionen. Pro Build-in-Eintrag mit non-default
// Status liegt ein Sparse-Eintrag hier in NodePrefs (= scope_buildin_status[]).
// Selbst-erfundene Namen die NICHT in der Build-in-Tabelle stehen kommen in
// scope_extras[] als vollwertige Eintraege.
//
// Status-Byte-Layout:
//   bits 0-1: repeat_mode (00=auto, 01=on, 10=off, 11=reserved)
//   bit  2:   advert_geo_off    (0 = darf als Geo-Fallback fuer eigenen
//                                    scoped flood advert dienen,
//                                1 = aus dem Auto-Geo-Send ausgeschlossen)
//   bit  3:   disabled          (temporaer inaktiv, zeigt in 'scope list'
//                                mit D-Flag, reversibel)
//   bit  4:   user_deleted      (User hat Eintrag versteckt, fliegt aus
//                                'scope list' raus, nur in 'scope list all'
//                                noch sichtbar)
//   bits 5-7: reserved
//
// Default = 0x00 = auto / Geo-Send aktiv / nicht disabled / nicht deleted.
//
// Indexierung ueber 4-Byte FNV-1a-Hash des Region-Namens (NICHT ueber
// Position in der Build-in-Tabelle). Damit ueberlebt der Status auch
// Reorder/Insert/Remove in der Build-in-Tabelle. Nur Eintraege mit
// non-default Status werden gespeichert (sparse). Max-Anzahl Slots
// begrenzt durch SCOPE_BUILDIN_STATUS_MAX — sollte deutlich groesser
// sein als die typische Anzahl angepasster Eintraege.
#define SCOPE_BUILDIN_STATUS_MAX     32    // Sparse-Slots fuer customisierte Build-in-Eintraege
#define SCOPE_EXTRAS_SLOTS           16    // User-erfundene Namen

struct BuildinStatusEntry {
  uint8_t name_hash[4];       // FNV-1a 32-bit ueber den Region-Namen
  uint8_t status;             // SCOPE_STATUS_* Flags
  uint8_t _reserved[3];       // Padding fuer 4-Byte Alignment + future use
};                            // = 8 Byte pro Eintrag

#define SCOPE_STATUS_REPEAT_MASK     0x03  // bits 0-1
#define SCOPE_STATUS_REPEAT_AUTO     0x00
#define SCOPE_STATUS_REPEAT_ON       0x01
#define SCOPE_STATUS_REPEAT_OFF      0x02
#define SCOPE_STATUS_ADVERT_OFF      0x04  // bit 2
#define SCOPE_STATUS_DISABLED        0x08  // bit 3
#define SCOPE_STATUS_USER_DELETED    0x10  // bit 4

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
  //
  // Build-Schalter REPEATER_DEFENSIVE_FORCE (Wunschliste 54):
  //   nicht definiert (Default in GitHub-Release): User-sichtbare
  //     Pfade (CLI 'repeater on force', App force-Flag, help, get all,
  //     backup-export, status-Anzeige) sind aus-ifdef't. Pref-Layout
  //     bleibt persistent kompatibel, der Wert wird aber ignoriert.
  //   definiert (-DREPEATER_DEFENSIVE_FORCE=1 in platformio.ini):
  //     komplettes Force-Feature aktiv.
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
  // 0  = uninit -> begin() setzt auf -1 (auto, Default fuer neue Geraete).
  // -1 = AUTO (Sentinel): bei txdelay 1.5 wenn repeater_profile==full,
  //      0.5 wenn defensive; bei direct_txdelay fix 0.2.
  // 0..2 = expliziter User-Wert.
  float tx_delay_factor;
  float direct_tx_delay_factor;
  uint8_t        repeat_scope_mode;          // REPEAT_SCOPE_MODE_*

  // Sparse-Status fuer Build-in-Eintraege mit non-default Settings.
  // Indexiert via FNV-1a-Hash des Region-Namens (NICHT Position) — damit
  // ueberlebt User-Konfig auch wenn die Build-in-Tabelle reorderet wird.
  uint8_t              scope_buildin_status_count;
  BuildinStatusEntry   scope_buildin_status[SCOPE_BUILDIN_STATUS_MAX];

  // User-Extras: Eintraege fuer Namen die NICHT in der Build-in-Tabelle
  // stehen (eigener Vereins-Scope, Kiez-Scope, voruebergehende Region).
  uint8_t        scope_extras_count;
  ScopeRegEntry  scope_extras[SCOPE_EXTRAS_SLOTS];

  // Hop-Cap fuer #region- / #regional-scoped Pakete (Wunschliste 11
  // Schritt 12). Pakete mit path_hash_count >= flood_max_scope_region werden
  // nicht repeated. 0 wird beim Boot auf Default 3 gehoben.
  uint8_t        flood_max_scope_region;

  // Allgemeine Hop-Obergrenze fuer Repeat. Pakete mit path_hash_count >=
  // flood_max werden nicht weitergeleitet. Analog zu CommonCLI/simple_
  // repeater 'flood.max'. 0 wird beim Boot auf Default 16 gehoben. Range
  // 1..64. CLI: set flood_max <N> (oder flood.max).
  uint8_t        flood_max;

  // Separater Hop-Cap fuer Infrastruktur-Adverts (adv_type != ADV_TYPE_CHAT,
  // also REPEATER/ROOM/SENSOR). Wenn > 0: Pakete dieses Typs werden ab
  // path_hash_count > flood_max_infra nicht mehr weitergeleitet --
  // typisch enger als flood_max um Infrastruktur-Adverts ortsbezogen zu
  // halten. 0 = deaktiviert (es greift flood_max).
  // Constraint: 0 oder 1..flood_max. Auto-Cap auf flood_max wenn dieser
  // unter den gesetzten Wert sinkt. CLI: set flood_max_infra <N>.
  // Begruendung (Wunschliste 24): User-Adverts (Chat) muessen weit
  // kommen damit Erstkontakt ohne externen Schluesseltausch moeglich
  // bleibt; Repeater/Sensor/Room-Adverts sind ortsbezogen.
  uint8_t        flood_max_infra;

  // Hop-Cap fuer REQ/RESP/ANON_REQ-Pakete (Telemetrie, Owner-Info, Login etc).
  // REQ/RESP machen aus jeder Konversation Doppel-Flood (REQ floodet hin,
  // PATH_RETURN mit RESP floodet zurueck) und dominieren empirisch den
  // Mesh-Traffic (analyzer.meshcorenetz.de). Eng cappen bringt am meisten.
  // Range 0 oder 1..flood_max_infra (falls gesetzt, sonst 1..flood_max).
  //   > 0  -> Pakete mit path_hash_count > flood_max_req_resp werden nicht
  //           mehr repeated.
  //   == 0 -> kaskadiert: es greift flood_max_infra (falls > 0), sonst
  //           flood_max.
  // Default 0 (= kaskade): out-of-the-box keine Verhaltensaenderung. Help
  // empfiehlt explizit 4..8 zu setzen -- 4 fuer eng (typische Stadt-
  // Reichweite), 8 ist sinnvoll wenn man Adverts auf 16 Hops weit gehen
  // laesst und Repeater darunter trotzdem flooded abfragen koennen will.
  // Auto-Cap wenn flood_max_infra oder flood_max unter den gesetzten Wert
  // sinken. CLI: set flood_max_req_resp <N>.
  //
  // flood_max_infra kann den Wert FLOOD_MAX_INFRA_FOLLOW (254) tragen --
  // "folgt flood_max persistent". Wird beim Boot nicht zurueck-gebumpt
  // und beim flood_max-Senken nicht ueberschrieben.
  // Sub-Typ-Differenzierung (Telemetrie vs. Owner-Info) ist beim Forwarding
  // protokoll-bedingt nicht moeglich: REQ-Sub-Typ ist im verschluesselten
  // Payload, dest_hash ist nur 1 Byte (kollidiert), ADV_TYPE des Empfaengers
  // nicht im Header.
  uint8_t        flood_max_req_resp;

  // Wunschliste 32 v2 (User-Feedback 2026-06-02): per-Channel Repeat-Cap
  // jetzt als Name-Hash-Liste statt Slot-Index-Array. Slot-Indices in
  // channels[] sind nicht stabil -- wenn Channel an einen anderen Slot
  // wandert (App-Sync, delete+add), zeigte der Slot-indizierte Cap auf
  // den falschen Channel.
  //
  // Storage: bis zu MAX_GROUP_CHANNELS Eintraege, je 5 Byte.
  // Lookup im RX-Hot-Path via RAM-Cache _channel_hops_cap_cache (slot-
  // indiziert, wird bei jeder Channel- oder ch.hops-Aenderung neu aus
  // dieser Liste aufgebaut).
  // FNV-1a 32-bit Hash ueber den Channel-Namen -- Kollisions-
  // Wahrscheinlichkeit fuer 40 Eintraege < 2e-9.
  // Cap-Werte:
  //   254 (CH_HOPS_OFF) = kein expliziter Cap (Entry sollte dann nicht
  //                       in der Liste stehen; defensiv akzeptiert)
  //   0                  = "nicht repeaten" (z.B. Public gegen Spam)
  //   1..flood_max       = expliziter Cap (Drop wenn
  //                        path_hash_count > N)
  // $companion-Slot wird in setupCompanionChannel() im Cache forced auf
  // 0 gesetzt -- NICHT in dieser Liste persistiert (Sicherheitsmassnahme).
  // CH_HOPS_FLAG_EXTERNAL: Entry referenziert einen NICHT-abonnierten
  // Hashtag-Channel. Hashtag-PSK ist deterministisch aus dem Namen
  // ableitbar (sha256), wir koennen also channel_hash[0] vorab
  // berechnen und beim RX-Filter ohne Slot direkt matchen. Use-Case:
  // '#bots' interessiert uns nicht im Chat, soll aber auch nicht
  // weitergeleitet werden -- 'set ch.hops #bots 0' ohne Subscribe.
  // Fuer Private-Channels nicht moeglich (PSK random, nicht
  // rekonstruierbar).
#define CH_HOPS_FLAG_EXTERNAL  0x01
  struct ChannelHopsEntry {
    uint32_t name_fnv1a;
    char     name[20];       // Channel-Name (mit '#' fuer hashtag),
                             // null-terminiert. Bei Slot-Eintraegen
                             // synced beim rebuild; bei External-
                             // Eintraegen vom User gesetzt. Display
                             // + Backup nutzen direkt diesen Wert.
    uint8_t  cap;
    uint8_t  channel_hash;   // gueltig wenn flags & EXTERNAL
    uint8_t  flags;          // CH_HOPS_FLAG_*
    uint8_t  _reserved;      // padding -> 28 Byte (4-aligned)
  };
  ChannelHopsEntry channel_hops_list[MAX_GROUP_CHANNELS];
  uint8_t          channel_hops_count;

  // Wunschliste 32: Repeat-Cap fuer Group-Messages auf Channels die
  // NICHT in unserer channels[]-Liste stehen (channel_hash[0] matched
  // keinen Eintrag). Default-Verhalten fuer Unbekannte.
  //   CH_HOPS_OFF (254) = kein Cap, es greift flood_max (Status quo)
  //   0                  = nicht repeaten
  //   1..flood_max       = expliziter Cap
  // Pre-Init via begin() VOR loadPrefs(), damit fresh-install /
  // Firmware-Upgrade-ohne-Save den 'off'-Default sehen statt 'nicht
  // repeaten' (memset 0). Persistierte User-Werte ueberschreiben das.
  uint8_t        flood_max_unknown_chan;

  // Wunschliste 31: RTC-Sync aus signiertem Advert.
  //   0 = off (kein Advert-Sync)
  //   1 = lazy (jeden zero-hop signierten Advert akzeptieren)
  //   2 = strict (nur konfigurierte Sources unten)
  // Default per Pre-Init in begin() = 1 (lazy). Wer das deaktivieren
  // will, setzt 'set time sync off' explizit.
  uint8_t        time_sync_mode;

  // Bis zu 3 Trust-Source-Pub-Key-Prefixes (je 3 Byte). 0-Prefix = Slot
  // ungenutzt. Greift nur bei mode==2 (strict).
  uint8_t        time_sync_sources[3][3];

  // Geo-vs-Default Send-Hierarchie fuer eigene Adverts (Wunschliste 13).
  //   0 = uninitialisiert (begin() migriert)
  //   1 = off:    Default-Scope gewinnt immer. Geo wird nie verwendet.
  //   2 = on:     Geo als Fallback wenn Default leer (heutiges Standard-
  //               Verhalten ohne scope_geo_prefers).
  //   3 = prefer: Geo gewinnt vor Default wenn ortliche Region != Default.
  //               (= altes scope_geo_prefers=1)
  // Migration aus altem scope_geo_prefers (war an gleichem Offset):
  //   alter Wert 0 -> 2 (on)
  //   alter Wert 1 -> 3 (prefer)
  // CLI: scope advert auto off|on|prefer
  uint8_t        scope_advert_auto;

  // Repeater-Profil (Wunschliste 8):
  //   0 = defensive (Default, aktuelles Verhalten: PATH nur lokal,
  //                  Repeats mit reduzierter Power + CR5)
  //   1 = normal    ("wie ein echter Repeater": ALLE PATH repeaten,
  //                  Repeats mit voller Power + konfigurierter CR)
  // Use-Case: voruebergehender Standortwechsel an einen guten Spot
  // (Hochhaus, Berg) fuer ein paar Stunden vollwertig repeaten, dann
  // wieder defensiv. Andere Filter (client_repeat, duty, flood_max,
  // scope-Allowlist) bleiben unveraendert.
  uint8_t        repeater_profile;

  // Globaler Auto-Schalter fuer Repeat (Wunschliste 13).
  //   0 = uninitialisiert (begin() initialisiert auf 2)
  //   1 = off: alle auto-rep-Eintraege werden ignoriert. Nur Pin und
  //            explizit OFF zaehlen. Use-Case: temporaer Geo-Bbox-mode
  //            stilllegen ohne Per-Eintrag-Config zu verlieren.
  //   2 = on (Default): auto-rep-Eintraege greifen wenn GPS in Bbox.
  // CLI: scope repeater auto on|off
  uint8_t        scope_repeater_auto;

  // Owner-Info (Wunschliste 7 Phase 1). Free-form Beschreibung der Node,
  // wird bei ANON_REQ_TYPE_OWNER und REQ_TYPE_GET_OWNER_INFO mitgesendet.
  // Layout analog CommonCLI/simple_repeater.
  char           owner_info[120];

  // Advert-Role (Wunschliste 7 Phase 2). Steuert sowohl den ADV_TYPE in
  // createSelfAdvert als auch die Discovery-Query-Antworten (anon/peer).
  //   0 = auto      -> effectiveAdvertRole() per Matrix (siehe MyMesh.cpp)
  //   1 = chat      -> ADV_TYPE_CHAT
  //   2 = repeater  -> ADV_TYPE_REPEATER
  //   3 = sensor    -> ADV_TYPE_SENSOR
  //   4 = room      -> ADV_TYPE_ROOM
  // CLI: advert role auto | fixed chat|repeater|sensor|room
  uint8_t        advert_role;

  // Loop-Detection-Modus (Wunschliste 6b). Analog CommonCLI/
  // simple_repeater. Pro Hash-Size ein maximaler Self-Occurrence-Count
  // im Pfad -- ab Schwelle wird das Paket verworfen.
  //   0 = off       (Default; greift nur im normal-Profile)
  //   1 = minimal   { 1B:4, 2B:2, 3B:1 }
  //   2 = moderate  { 1B:2, 2B:1, 3B:1 }
  //   3 = strict    { 1B:1, 2B:1, 3B:1 }
  // GATING: nur aktiv wenn repeater_profile == 1 (normal). Defensive
  // Profile leitet ohnehin nur an lokal-bekannte Endpoints weiter --
  // Schleifen koennen da nicht entstehen.
  // CLI: set loop_detect <mode> (oder loop.detect fuer CommonCLI-Stil)
  uint8_t        loop_detect;

  // ----------------------------------------------------------------
  // Offline-Message-Queue Konfiguration (Wunschliste 19 Phase C).
  // ----------------------------------------------------------------
  // Bit-Field pro Bucket: 1 = Flash-Persistenz aktiv, 0 = RAM-only.
  // Default 0 (alles RAM-only, privacy-konsistent).
  //   bit 0 = PUBLIC, bit 1 = HASHTAG, bit 2 = PRIVATE,
  //   bit 3 = DM,     bit 4 = COMPANION
  // CLI: messages flash <type> on|off
  uint8_t        msg_store_flash;

  // Per-Bucket Slot-Limit. 0 = type-spezifischer Default
  // (PUB=8, HT=8, PRIV=16, DM=16, COMP=16). Cap: 16 fuer alle ausser
  // DM (32). Indizes [PUB,HT,PRIV,DM,COMP].
  // CLI: messages limit <type> <N>
  uint8_t        msg_store_limit[5];

  // Logging-Senken-Steuerung (Wunschliste 21, User-Wunsch 2026-05-30).
  // ASYMMETRISCHE Bit-Semantik damit Default 0 = USB off + Channel on:
  //   bit 0 = USB-Serial Output ENABLED (1 = on, 0 = off). Default off.
  //           Begruendung: bei USB-Companion-Firmware wuerde Debug-Output
  //           die App-Frame-Stream zerschiessen. Da der User in dem Fall
  //           NICHT mehr ueber die App das Logging deaktivieren kann
  //           (App geht nicht), sicher per Default off. Symmetrisch fuer
  //           BLE-Builds, kein Build-spezifisches Verhalten noetig.
  //   bit 1 = $companion-Channel Output DISABLED (1 = off, 0 = on). Default on.
  //           Klassischer Trace-Pfad zum Lesen via App.
  // App-Debug-Frame (PUSH_CODE_DEBUG_LOG) bleibt unbeeinflusst.
  // CLI: logging usb|channel on|off ; 'logging' zeigt Status.
  uint8_t        log_flags;

  // Wunschliste 39 (2026-06-04): Hop-Cap fuer UNSCOPED Companion-Pakete
  // (ROUTE_TYPE_FLOOD ohne transport_codes):
  //   - ADVERT mit adv_type = ADV_TYPE_CHAT  (Companion-User)
  //   - TXT_MSG flood (DM ohne etablierten Path und ohne Default-Scope)
  // Erlaubt limited propagation damit User-Erstkontakt ueber Repeater
  // hinweg klappt -- ohne unbeschraenkten unscoped-Traffic.
  //   254 (CH_HOPS_OFF)  = follow flood_max_scope_region (Default)
  //   0                   = nicht repeaten (explizit off)
  //   1..flood_max        = expliziter Cap
  // CLI: set flood_max_unscoped_companions <follow|off|1..63>
  uint8_t        flood_max_unscoped_companions;
  // Reise-Wunsch 2026-06-08: Toggle fuer scope-Annotation an
  // Channel-Sender-Namen ('Name (#scope[, direct])') und DM-Vorab-
  // Frame ('[#scope, direct]'). Default on (Wunschliste 35 Verhalten).
  //   1 = on  -> annotation einfuegen (Default)
  //   0 = off -> Original-Text/Frame ohne scope-Info
  // CLI: 'messages append-scope-to-name on|off'
  // Wirkung: bei off kann die App den Sender-Namen wieder unverändert
  // gegen ihre DB matchen -> Pfad-Anzeige funktioniert wieder.
  uint8_t        messages_append_scope_to_name;
  // Reise-Wunsch 2026-06-09 (Wunschliste 46 Phase 1): Filter-System
  // fuer eingehende Pakete. Sender-Filter (drop) wirkt auf DM und
  // Channel-Sender-Namen. Text-Filter (drop) wirkt auf Channel-Text.
  // Pattern-Flags:
  //   bit 0 = anchor start (^foo)
  //   bit 1 = anchor end   (foo$)
  //   beide = exact match  (^foo$)
  //   keiner = substring   (*foo*)
  // Filter sind 'for-us' (only) -- App-Push wird unterdrueckt, Repeat
  // bleibt unbeeinflusst. Phase 1: nur drop-Listen, keine allow-
  // Listen, keine per-Channel-Ausnahmen, keine Pattern-Sprache.
  struct FilterEntry {
    char pattern[31];
    uint8_t flags;
  };
  FilterEntry    filter_sender_drop[16];
  uint8_t        filter_sender_drop_count;
  FilterEntry    filter_text_drop[16];
  uint8_t        filter_text_drop_count;
  // Wunschliste 46 Phase 3 (2026-06-10): keep-Listen (Positiv-Liste).
  // Match-Reihenfolge: channel-filter -> keep -> drop. Keep-Match gewinnt
  // (laesst Paket durch, ueberspringt drop). 'drop *' + 'keep <pat>'
  // ergibt einen Whitelist-Modus ohne separaten Strict/Lenient-Schalter.
  FilterEntry    filter_sender_keep[16];
  uint8_t        filter_sender_keep_count;
  FilterEntry    filter_text_keep[16];
  uint8_t        filter_text_keep_count;

  // Wunschliste 46 Phase 2 v2 (2026-06-10): channel-filter pro Pattern.
  // Pro Pattern eigene on/exempt-Maske. Bit_i gesetzt -> Channel i.
  // Beide 0 = global (alle Channels). on/exempt mutual exclusive
  // (in CLI durchgesetzt). Ersetzt die fruehere Global-pro-Typ-Mask.
  uint64_t       filter_sender_drop_chan_on[16];
  uint64_t       filter_sender_drop_chan_ex[16];
  uint64_t       filter_sender_keep_chan_on[16];
  uint64_t       filter_sender_keep_chan_ex[16];
  uint64_t       filter_text_drop_chan_on[16];
  uint64_t       filter_text_drop_chan_ex[16];
  uint64_t       filter_text_keep_chan_on[16];
  uint64_t       filter_text_keep_chan_ex[16];

  // Wunschliste 46 Phase 5 (2026-06-10): scope-Filter.
  // Eigene Achse: filter wirkt anhand scope-Tag im Wire-Header (also
  // unabhaengig von Sender-Name oder Text-Inhalt). User-Wunsch z.B.
  // 'filter scope drop add #europe,#de,unscoped on-channel Public'.
  // Reserved Pseudo-Token 'unscoped' (case-insens) -- Pakete ohne
  // scope-Tag. profile-Achse pro Entry: bit 0-1 (0=for-us / 1=repeat
  // / 2=complete; 3 reserviert). NICHT mit MeshCore-'scope'-Welt
  // (TransportKey) selbst verwechseln -- Filter referenziert sie nur.
  struct FilterScopeEntry {
    char scope_name[31];
    uint8_t flags;  // bit 0-1: profile, bit 2-7: reserved
  };
  FilterScopeEntry filter_scope_drop[16];
  uint8_t          filter_scope_drop_count;
  uint64_t         filter_scope_drop_chan_on[16];
  uint64_t         filter_scope_drop_chan_ex[16];
  FilterScopeEntry filter_scope_keep[16];
  uint8_t          filter_scope_keep_count;
  uint64_t         filter_scope_keep_chan_on[16];
  uint64_t         filter_scope_keep_chan_ex[16];

  // Wunschliste 46 Repeat-Achse (2026-06-10): unbekannte Channels.
  // Repeater sieht Pakete fuer Channels die er nicht selbst konfiguriert
  // hat (kein Klartext, aber Channel-Hash + scope-Tag sichtbar).
  // mode: 0 = yes (alle weiterleiten, heutiges Default-Verhalten)
  //       1 = scoped only (gescopte weiterleiten, unscoped droppen)
  //       2 = unscoped only (umgekehrt, selten)
  //       3 = no (gar nicht weiterleiten)
  uint8_t filter_unknown_channel_repeat;

  // Wunschliste 43 BLE-Power-Cycle (2026-06-10, refactored 2026-06-11):
  // Klare Trennung Profil vs On/Off-Zustand:
  //   bluetooth_profile  -- Bit-Mask, welcher Modus konfiguriert ist
  //                          0x01 = cycle
  //                          0x20 = always-on
  //                          (Default 0x01)
  //   bluetooth_active   -- aktueller Modus (Kopie eines profile-Bits)
  //                          0    = off (Chip wirklich aus)
  //                          0x01 = on im cycle-Modus
  //                          0x20 = on im always-on-Modus
  //                          (Default 0x01)
  // CLI:
  //   bluetooth off                  -> active = 0 (off persistent)
  //   bluetooth on                   -> active = profile (zurueck zum
  //                                       konfigurierten Modus)
  //   bluetooth power cycle          -> profile |= 0x01; if active != 0:
  //                                       active = profile
  //   bluetooth power always-on      -> profile |= 0x20; analog
  //   bluetooth tmp-off              -> runtime override (RAM-only,
  //                                       Pref bleibt unangetastet)
  // Legacy: vorheriges Layout hatte bluetooth_power_mode (0=cycle,
  // 1=always-on, 2=off). Migration in MyMesh::begin() POST-load:
  //   alte Werte {0,1,2} im profile-Byte werden auf Bit-Form
  //   gemappt; 0xFF im active = EOF/sentinel -> default uebernommen.
  uint8_t bluetooth_profile;
  uint8_t bluetooth_active;

  // Wunschliste 50 Phase 1 (2026-06-11): TZ-Override.
  // tz_mode = 0  -> auto-eu (regelbasiert DST, Default fuer EU-User)
  // tz_mode = 1  -> fixed offset (kein DST)
  // tz_mode = 2  -> utc (0, kein offset)
  // tz_offset_min: Offset in Minuten von UTC (-720..+840 = -12h..+14h).
  //                bei auto-eu: Basis-Offset (Winter), DST addiert 60min.
  //                bei fixed:  effektiver Offset.
  // Defaults: tz_mode=0 (auto-eu), tz_offset_min=60 (CET-Basis = +1h).
  // 0xFF im tz_mode = Sentinel (Pre-Init/EOF) -> Migration zu defaults.
  uint8_t tz_mode;
  int16_t tz_offset_min;

  // Wunschliste 46 Phase 4 (2026-06-11): Advert- und Pubkey-basierte Filter.
  //
  //   filter_advert_drop_name[]:    display-soft Match auf Sender-Klartextname
  //     (heard_list bleibt unberuehrt; nur App-Push + UI gedrosselt).
  //   filter_advert_drop_pubkey[]:  wire-hardblock im Advert-Recv-Pfad,
  //     kein heard_list-Eintrag, kein contact-add (pubkey-Prefix-Match
  //     auf die Identity im Advert).
  //   filter_sender_drop_pubkey[]:  post-decrypt-Block fuer DM/REQ/RESP an
  //     uns; matched die ECDH-MAC-verifizierte Sender-pubkey-Prefix. App-
  //     Push wird unterdrueckt, ACK wird nicht zurueckgesendet. (GRP_TXT/
  //     Advert/etc. tragen keine eindeutige Sender-pubkey -- dafuer
  //     bestehen die Name- und Wire-Filter.)
  //
  // Storage je Pubkey-Slot: 16 Byte Key-Prefix + 1 Byte len (1..16). 8 Slots
  // pro Filter -- (16+1)*8 = 136 Byte. Drei Filter zusammen ~408 Byte.
  struct FilterPubkeyEntry {
    uint8_t key[16];
    uint8_t len;  // gueltige Prefix-Laenge in Bytes (1..16)
  };
  FilterEntry        filter_advert_drop_name[16];
  uint8_t            filter_advert_drop_name_count;
  FilterPubkeyEntry  filter_advert_drop_pubkey[8];
  uint8_t            filter_advert_drop_pubkey_count;
  FilterPubkeyEntry  filter_sender_drop_pubkey[8];
  uint8_t            filter_sender_drop_pubkey_count;

  // Wunschliste 10 (2026-06-11): USB-Serial Plain-Text CLI Persistent-State.
  //   0 = off (Default fuer USB-frame-Builds: Frame-Protokoll bleibt aktiv)
  //   1 = on  (Default fuer BLE/WiFi-Builds: USB ist frei fuer User-CLI)
  // 0xFF im File = Sentinel (Legacy/EOF) -> Default je nach Build.
  // Runtime-Toggle 'serial-cli on-temp' liegt im RAM und wird beim Reboot
  // verworfen (revertiert zum Persist-Wert).
  uint8_t serial_cli_persist_on;

  // Wunschliste 58 Phase B (2026-06-13): CPU-Clock-Frequenz.
  // Erlaubte Werte (ESP32-S3): 240, 160, 80, 40, 20, 10 MHz.
  // 0 = Sentinel "Default benutzen" (ESP32: 240 MHz).
  // 0xFF im File = Legacy/Pre-Init -> wird zu 0 normalisiert.
  // Boot-Apply in main.cpp setup() NACH the_mesh.begin() (Prefs-load
  // first), aber bevor sehr Timing-kritische Operationen folgen.
  // User-Vergleich (LoRa-APRS-Firmware, 80 MHz, kein BLE): 155 mA.
  // Unsere Firmware 240 MHz mit BLE active: 186-192 mA. Delta plausibel
  // erklaerbar durch Clock + BLE-Overhead.
  uint8_t cpu_clock_mhz;

  // Wunschliste 58 Phase A (2026-06-13/14): Display Power-Control.
  //   0 = off                 -- nie auto-on (Channel-Msg etc);
  //                             Hardware-Button-Press weckt weiterhin.
  //   1 = on                  -- permanent an, kein Auto-Off-Timer.
  //   2 = on-at-new-messages  -- Default, aktuelles Verhalten (auto-on
  //                             bei Channel-Msg, dann Auto-Off-Timer).
  // 0xFF im File = Sentinel (EOF/Legacy) -> Default 2 (Verhaltens-
  // bruch-frei fuer Bestandsuser).
  uint8_t display_wake_mode;

  // Wunschliste 46 Phase 2 (2026-06-10): channel-filter -- pro Filter-Typ
  // einschraenken auf welchen Channels der Filter wirkt.
  // Bit-Mask: bit_i gesetzt -> filter wirkt auf channels[i].
  // on_channel_mask: nur diese Channels (falls != 0).
  // exempt_mask:     ueberall ausser diese (falls != 0).
  // Default beide = 0 = global (alle lokal konfigurierten Channels).
  // CLI-seitig mutual exclusive: Setzen einer Mask loescht die andere.
  // channel-filter gilt nur fuer Channel-Match -- DM (Sender-Filter)
  // ignoriert die Masks und wird stets gegen das Pattern getestet.
  // ACHTUNG nicht mit MeshCore-'scope' (TransportKey-Tags wie #de,
  // #regional) verwechseln -- das ist eine andere Achse (Wunschliste 5).
  uint64_t       filter_sender_drop_on_channel_mask;
  uint64_t       filter_sender_drop_exempt_mask;
  uint64_t       filter_text_drop_on_channel_mask;
  uint64_t       filter_text_drop_exempt_mask;
  // Wunschliste 45 (Reise 2026-06-09): LBT-Stub-Removal.
  // interference_threshold: RSSI-Margin (dB) ueber noise_floor.
  // 0 = LBT off (no listen-before-talk). Upstream-Doku-Default: 14.
  // Wessel Nieboers AGC-Reset-Fix (Feb 2026, RadioLibWrappers.cpp:77)
  // schuetzt vor stuck _noise_floor=-120 -- damit ist LBT im Companion
  // sicher aktivierbar.
  // agc_reset_interval: Sekunden / 4 (* 4000ms intern). 0 = disabled.
  // Periodischer AGC-Reset bei verrauschten/RX-uebersteuerten Standorten.
  // Default 0 -- User aktiviert nach Bedarf.
  uint8_t        interference_threshold;
  uint8_t        agc_reset_interval;
  // Reise-Wunsch 2026-06-09 (Wunschliste 52): Remote-CLI-Login per
  // Passwort. Admin-Passwort gibt vollen Zugriff auf alle Befehle,
  // Guest-Passwort gibt reduzierten Read-only-Set (optional).
  // Leer = jeweiliges Login deaktiviert. Anonyme Repeater-Queries
  // (ANON_REQ_TYPE_OWNER/REGIONS/BASIC) brauchen kein Passwort.
  // CLI: set passwd_admin <pw> / set passwd_guest <pw> / ...clear
  char           passwd_admin[32];
  char           passwd_guest[32];

  // Wunschliste 53 Phase 1+2 (2026-06-14): Hardware-Watchdog Pref.
  //   0 = off (Default, Validierungs-Phase nach Implementation)
  //   1 = on  (ESP32: TWDT 90s panic-reboot, NRF52: nrfx_wdt 90s).
  // Aenderung wirkt erst beim naechsten Neustart (im laufenden Betrieb
  // an/aus zu schalten ist zu riskant). 0xFF im File = EOF-Sentinel ->
  // Migration zu 0 (off).
  uint8_t        watchdog_mode;
  // DL9SAU 2026-06-17 (Wunschliste 75): Buzzer-Profile Bitmask.
  // Bit 0x01 DM, 0x02 CH_PUB (Public+Hashtag+$companion),
  // 0x04 CH_PRIV (random-private), 0x08 ACK, 0x10 APP_DISC_ONLY.
  // Default 0x0F = alle Events an, app_disc-only off (= current behavior).
  // 0xFF im File = EOF-Sentinel -> Migration zu 0x0F.
  // buzzer_quiet bleibt als Master-Mute -- wenn quiet=1 ist alles aus
  // ausser boot/shutdown.
  uint8_t        buzzer_profile;
  // DL9SAU 2026-06-17 (Wunschliste 81): GPS-Profile.
  //   0 = full          Default. Lat/lon UND time von Live-GPS.
  //   1 = position-only Live-GPS-Position, kein time-sync (RTC anders).
  //   2 = time-only     GPS nur fuer Zeit-Sync, lat/lon-Updates ignoriert
  //                     (Position aus _prefs.lat/lon, statischer Repeater).
  // 0xFF im File = EOF-Sentinel -> Migration zu 0 (full).
  uint8_t        gps_profile;
  // DL9SAU 2026-06-18 (Wunschliste 83): RX permanent ausschalten zum
  // Stromsparen. 0 = RX an (Default, Companion empfaengt normal).
  // 1 = RX im Sleep-Mode, nur kurzes Wake fuer Send + 5min RX-Window
  // danach (User-Communication-Fenster). EFFEKTIV nur bei
  // !client_repeat -- ein Repeater muss zwingend RX halten. Bei
  // 'repeater on' wird RX automatisch wieder aktiv ohne diesen
  // Pref-Wert zu aendern; bei 'repeater off' greift er wieder.
  // Boot-Window (bis zum ersten geplanten Advert -- 5 oder 10min)
  // bleibt RX an damit RTC-Sync via signierter Adverts moeglich ist.
  // 0xFF im File = EOF-Sentinel -> Migration zu 0 (RX an).
  uint8_t        rx_disabled;
  // DL9SAU 2026-06-18 (Wunschliste 81 Phase 3): GPS-Lead in Sekunden.
  // Erweitert gps_lead_min (uint8_t, 1..14 min) auf bis zu 90 Tage,
  // sodass time-only-Repeater mit langem Cycle moeglich sind
  // ('gps power lead 1d', '7d', ...).
  // Semantik je nach Wert:
  //   1..900s    (== 1..15min): pre-advert wake-lead, alter Cycle-Mode
  //                              (15-min-Cycle, advert-Lead). gps_lead_min
  //                              bleibt fuer Backward-Compat aktiv.
  //   > 900s:    Long-Cycle-Mode -- gps_lead_secs ist die volle Cycle-
  //              Laenge zwischen GPS-Wakes. wake-Dauer = bis time-sync
  //              done (oder Position-Fix bei full/position-only).
  // 0 = Sentinel "noch nicht gesetzt" -> migrate from gps_lead_min in
  //     begin() (gps_lead_min * 60). 0xFFFFFFFF im File = EOF/Legacy.
  // CLI 'gps power lead <N>[s|m|h|d]' -- Default-Suffix m.
  uint32_t       gps_lead_secs;
  // DL9SAU 2026-06-18 (Wunschliste 89/90/91): Battery + USB-Power.
  // batt_chemistry: 0=disabled (kein Schutz, keine %-Anzeige),
  //                 1=lion/lipo (1S, Cutoff 3000mV, Default-Schwelle 3200),
  //                 2=lifepo4   (1S, Cutoff 2500mV, Default-Schwelle 2700).
  // batt_min_mv: User-Cutoff-Schwelle in mV. 0 = Default je Chemie.
  //              Wenn unterschritten -> shutdown via state-machine
  //              (10s-Sampling-Verify ueber 30s gegen LoRa-TX-Spike).
  //              Bei isExternalPowered() ist Schutz DEAKTIVIERT
  //              (Lade-Wave verfaelscht Messung).
  // usb_loss_shutdown_min: nach USB-Verlust N Minuten -> shutdown.
  //              0 = disabled (Default). 1..240. Use-Case: Tracker
  //              im Auto, Zuendung aus -> nach N min sauberer Off.
  // 0xFF im File = EOF-Sentinel -> Migration in begin() zu defaults.
  uint8_t        batt_chemistry;
  uint16_t       batt_min_mv;
  // DL9SAU 2026-07-12: batt_min_mv_boot -- LPCOMP-Recovery-Schwelle (mV).
  //   Nach Low-Battery-Shutdown bleibt das Geraet aus, bis die Spannung
  //   diesen Wert ueberschreitet (Hardware-Comparator weckt = Auto-Boot).
  //   0 = Default je Chemie (~30%). Wird zur Nutzungszeit IMMER auf mind.
  //   ~12 Ladeprozent ueber batt_min_mv geklemmt (getEffectiveBootMinMv),
  //   egal was gesetzt ist -- sonst Boot-Reboot-Loop.
  uint16_t       batt_min_mv_boot;
  uint8_t        usb_loss_shutdown_min;
  // DL9SAU 2026-07-12: button_press_allow_shutdown -- 0 = Long-Press-Button
  //   loest KEINEN Shutdown aus (Schutz gegen versehentliches Aussperren,
  //   solange Button-Wake nicht funktioniert -- kein Ladekabel dabei = kein
  //   Wieder-An). 1 = erlaubt (Default). 0xFF/>1 im File -> Default 1.
  uint8_t        button_press_allow_shutdown;
  // DL9SAU 2026-06-20: usb_wake_action wurde nach erfolgreicher
  // shutdown_pending-Sentinel-Implementierung entfernt (Wunschliste
  // 90 Phase 2). Pref-Byte bleibt als 'reserved' im Layout damit
  // bestehende NodePrefs-Dateien nicht verschoben werden muessen.
  uint8_t        _reserved_usb_wake_action;
  // DL9SAU 2026-06-20: shutdown_pending Sentinel-Byte. Wird durch
  // setShutdownSentinel() auf 1 gesetzt + savePrefs() VOR powerOff().
  // Beim Boot prueft MyMesh::begin nach loadPrefs: wenn 1 UND USB
  // nicht da -> wieder powerOff. Robuster als GPREGRET (BOR loescht)
  // und File-Sentinel (LittleFS-Cache-Race). Sentinel 0xFF -> 0.
  uint8_t        shutdown_pending;
  // DL9SAU 2026-07-02: 'scope channel no-scope' (loest runtime-only
  // 'unscoped-channelmessages' ab). Steuert was passiert wenn eine
  // Channel-Msg ohne Scope raus geht (weder App-Scope noch Default
  // noch Geo). Persistent.
  //   0xFF (EOF) = uninit -> Migration in initPrefs auf Default (1)
  //   0          = uninit-alt (falls Byte zufaellig 0 gelanded) -> auf Default
  //   1          = direct (Default; zero-hop, kein Repeat)
  //   2          = flood (klassisch, unscoped Reichweite)
  uint8_t        channel_no_scope_behavior;
  // 2026-07-06 Upstream-Merge: Hardware CAD (Channel Activity Detection)
  // vor TX. SX1262 macht Preamble-Detection intern. Werte:
  //   0xFF (EOF) = uninit -> Migration auf Default (1 = ein)
  //   0          = aus
  //   1          = an (Default)
  // Cad ergaenzt int.thresh (RSSI-basiertes LBT) -- beide aktiv sinnvoll:
  // CAD faengt Meshcore-Preambles, int.thresh das allgemeine RF-Level.
  uint8_t        cad_enabled;
#ifdef ESP_PLATFORM
  // DL9SAU 2026-06-16: Reboot-into-OTA-Mode (Wunschliste-OTA).
  // 'start ota' setzt das Flag und triggert Reboot. setup() prueft beim
  // Boot, clear's das Flag, und skipt BLE-Init -> max. freier Heap fuer
  // AsyncElegantOTA's 53 KB Response. 0/EOF-Sentinel = kein OTA-Reboot.
  // NRF52 (T1000-E) hat eigenen DFU-Mechanismus (SoftDevice +
  // adafruit-nrf-util), nicht WiFi-basiert -- daher hier ifdef-fenced.
  uint8_t        ota_pending;
#endif
};