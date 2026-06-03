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
  // Schritt 12). Pakete mit path_hash_count >= scope_regional_hop_limit werden
  // nicht repeated. 0 wird beim Boot auf Default 3 gehoben.
  uint8_t        scope_regional_hop_limit;

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
  struct ChannelHopsEntry {
    uint32_t name_fnv1a;
    uint8_t  cap;
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
};