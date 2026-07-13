#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include "AbstractUITask.h"

/*------------ Frame Protocol --------------*/
#define FIRMWARE_VER_CODE 13

#ifndef FIRMWARE_BUILD_DATE
#define FIRMWARE_BUILD_DATE "6 Jun 2026"
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v1.16.0-DL9SAU"
#endif

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
#include <InternalFileSystem.h>
#elif defined(RP2040_PLATFORM)
#include <LittleFS.h>
#elif defined(ESP32)
#include <SPIFFS.h>
#endif

#include "DataStore.h"
#include "NodePrefs.h"

#include <RTClib.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/BaseSerialInterface.h>
#include <helpers/IdentityStore.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <target.h>

/* ---------------------------------- DL9SAU FEATURE GATES ---------------------------- */

// DL9SAU Wunschliste 85+ (2026-06-20): BLE-Power-Cycle State-Machine
// ist ESP32-only.
//   ESP32 (Heltec WT, etc.): NimBLE Software-Stack auf CPU, kein
//     Hardware-Idle-Modus -- Advertising kostet messbar Strom.
//     Cycler spart 80 mA bei aktivem App-Connect-Window (User-
//     Crossvergleich mit Meshtastic 2026-06-19).
//   NRF52 (T1000-E): SoftDevice hat Hardware-BLE-Idle in uA-Bereich.
//     Advertising-Stop spart ~ 0 mA messbar (User-Test 2026-06-19,
//     'BT on/off kaum messbarer Unterschied'). Cycler bringt keinen
//     Spareffekt, bedeutet aber Code-Komplexitaet + Wartungslast +
//     SoftDevice-Lifecycle-Risiko (siehe memory project_ble_powerbank_
//     diagnostics + project_nrf52_resetreas_softdevice_trap).
// Daher: Cycler-State-Machine, Timer und BleLog werden auf NRF52
// vollstaendig herausuebersetzt. Auf NRF52 bleibt BLE permanent an
// (SoftDevice-Default, Advertising-Stop via 'bluetooth off' bleibt
// als einmaliger Schalter erhalten -- ist in SerialBLEInterface
// schon ohne SD-Disable implementiert).
#if !defined(NRF52_PLATFORM)
  #define BLE_CYCLE_AVAILABLE
#endif

/* ---------------------------------- CONFIGURATION ------------------------------------- */

#ifndef LORA_FREQ
#define LORA_FREQ 915.0
#endif
#ifndef LORA_BW
#define LORA_BW 250
#endif
#ifndef LORA_SF
#define LORA_SF 10
#endif
#ifndef LORA_CR
#define LORA_CR 5
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER 20
#endif
#ifndef MAX_LORA_TX_POWER
#define MAX_LORA_TX_POWER LORA_TX_POWER
#endif

#ifndef MAX_CONTACTS
#define MAX_CONTACTS 100
#endif

// (OFFLINE_QUEUE_SIZE entfernt 2026-05-30 -- Phase A+B Bucket-Redesign,
//  Wunschliste 19. Limits sind jetzt BUCKET_LIMIT_* in der Klasse.)

#ifndef BLE_NAME_PREFIX
#define BLE_NAME_PREFIX "MeshCore-"
#endif

#include <helpers/BaseChatMesh.h>
#include <helpers/TransportKeyStore.h>

// Region table (community-maintained, scraped at build time by dl9sau_regions.py).
// Only available for builds that include that pre-build script (currently the
// Heltec Tracker variants); other builds get an empty stub.
#if __has_include("dl9sau_regions.h")
  #include "dl9sau_regions.h"
  #define DL9SAU_REGIONS_AVAILABLE 1
#else
  #define DL9SAU_REGION_COUNT 0
  struct dl9sau_region { const char* name; };
  static const struct dl9sau_region dl9sau_regions[1] = { { 0 } };
#endif

/* -------------------------------------------------------------------------------------- */

#define REQ_TYPE_GET_STATUS             0x01 // same as _GET_STATS
#define REQ_TYPE_KEEP_ALIVE             0x02
#define REQ_TYPE_GET_TELEMETRY_DATA     0x03
#define REQ_TYPE_GET_NEIGHBOURS         0x06
#define REQ_TYPE_GET_OWNER_INFO         0x07  // FIRMWARE_VER_LEVEL >= 2
// Wunschliste 52 (2026-06-10): Remote-Administration ueber RPC-Strings.
// Payload: [1]REQ_TYPE_ADMIN_CMD [4]sender_timestamp [N]command_text (ASCII)
// Reply:   [4]reply_timestamp [M]response_text (ASCII).
// Permission-Marker: ContactInfo.flags & CONTACT_FLAG_ADMIN_OK (gesetzt
// nach erfolgreichem ANON_REQ_TYPE_LOGIN).
#define REQ_TYPE_ADMIN_CMD              0x08

// Wunschliste 7 Phase 3: Anonymous-Request-Typen (Discovery-Queries).
// Werden in onAnonDataRecv anhand der effektiven Role gegated.
#define ANON_REQ_TYPE_REGIONS    0x01
#define ANON_REQ_TYPE_OWNER      0x02
#define ANON_REQ_TYPE_BASIC      0x03  // just remote clock + features
// Wunschliste 52: Login (ANON-Pfad, kein vorab-Contact noetig).
// Payload: [1]ANON_REQ_TYPE_LOGIN [4]sender_timestamp [N]password (ASCII,
//          unterminiert, max 31 byte).
#define ANON_REQ_TYPE_LOGIN      0x04

// Contact-Flags-Bit fuer 'darf REQ_TYPE_ADMIN_CMD ausfuehren'.
// Wird nach erfolgreichem ANON_REQ_TYPE_LOGIN gesetzt.
#define CONTACT_FLAG_ADMIN_OK    0x40    // gewaehlt damit kein Konflikt
                                          // mit etablierten Lower-Bits
                                          // (favourite/blocked/sharing).
// Zusatz-Marker: 'reduced (guest) permissions' -- gesetzt wenn Login
// via passwd_guest erfolgte. Bei guest werden write-Befehle gefiltert.
#define CONTACT_FLAG_GUEST_ONLY  0x80

// Wunschliste 6b: Loop-Detection-Modi (analog CommonCLI).
#define LOOP_DETECT_OFF          0
#define LOOP_DETECT_MINIMAL      1
#define LOOP_DETECT_MODERATE     2
#define LOOP_DETECT_STRICT       3

// Wunschliste 15: Runtime-Neighbour-Tabelle fuer REQ_TYPE_GET_NEIGHBOURS.
// RAM-only, LRU, gefuellt aus onAdvertRecv bei zero-hop heard (path_len==0).
// 'liberal' Filter: alle adv_types werden aufgenommen (Repeater/Chat/Sensor).
// Funktioniert ab Boot ohne RTC (heard_timestamp ist Unix-Sekunde wenn RTC
// gesetzt, sonst 0 -> seconds_ago wird relativ aus heard_millis berechnet).
#define MAX_RUNTIME_NEIGHBOURS   32
struct RuntimeNeighbour {
  uint8_t  pub_key[32];      // PUB_KEY_SIZE
  uint32_t advert_timestamp; // aus dem advert (sender clock)
  uint32_t heard_timestamp;  // RTC unix seconds (0 wenn RTC ungesetzt)
  uint32_t heard_millis;     // millis() zum Zeitpunkt des heard
  int8_t   snr;              // x 4 wie simple_repeater
  int8_t   rssi_dbm;         // RSSI in dBm; INT8_MIN = ungesetzt
  uint8_t  adv_type;         // ADV_TYPE_*
  // Scope-Annotation (User-Wunsch 2026-06-11: in neighbors-Output zeigen
  // welcher Repeater einen default-scope gesetzt hat).
  //   leerer string: advert war unscoped
  //   "?"          : scoped, aber transport_code unbekannt (nicht in
  //                   unserer region-Tabelle)
  //   "de-be" usw.: scope-Name (aufgeloest via lookupRegionByTransportCode)
  char     scope_name[16];
};

// Wire-Layout fuer REQ_TYPE_GET_STATUS Antwort (Wunschliste 7 Phase 4).
// 1:1 kompatibel zu simple_repeater::RepeaterStats damit die App den
// gleichen Parser verwenden kann.
struct RepeaterStats {
  uint16_t batt_milli_volts;
  uint16_t curr_tx_queue_len;
  int16_t  noise_floor;
  int16_t  last_rssi;
  uint32_t n_packets_recv;
  uint32_t n_packets_sent;
  uint32_t total_air_time_secs;
  uint32_t total_up_time_secs;
  uint32_t n_sent_flood, n_sent_direct;
  uint32_t n_recv_flood, n_recv_direct;
  uint16_t err_events;
  int16_t  last_snr;   // x 4
  uint16_t n_direct_dups, n_flood_dups;
  uint32_t total_rx_air_time_secs;
  uint32_t n_recv_errors;
};

struct AdvertPath {
  uint8_t pubkey_prefix[7];
  uint8_t path_len;
  char    name[32];
  uint32_t recv_timestamp;
  uint8_t path[MAX_PATH_SIZE];
};

// custom client-repeater feature (EU narrow band 869.618 MHz, scoped-only forwarding)
// HeardList only stores nodes heard zero-hop (path_hash_count == 0), 1-byte hash like
// the wire protocol — collisions are accepted, same as the rest of MeshCore.
#define CR_HEARD_TABLE_SIZE     64
#define CR_HEARD_MAX_AGE_SECS   (48UL * 3600UL)
#define CR_NARROW_FREQ_TRIGGER  869.000f
#define CR_NARROW_FREQ_ACTUAL   869.618f
#define CR_REPEATER_CR          5
#define CR_TX_POWER_REDUCTION_DB 6   // applied to auto-adverts and digipeat TX
#define CR_TX_POWER_FLOOR_DBM    10  // never reduce below this

// GPS power-saving: keep the GPS module powered only when a fresh fix is
// useful, i.e. just before the next scheduled advert. The Heltec Tracker
// (and similar boards) cycle the module via its own GPS_EN pin, separate
// from V_EXT (which feeds the display and cannot be cut). Position cache
// (sensors.node_lat/lon) keeps the last fix so adverts still carry a
// recent location even when the module is asleep.
#ifndef CR_GPS_LEAD_BEFORE_ADVERT_MS
#define CR_GPS_LEAD_BEFORE_ADVERT_MS  (5UL * 60UL * 1000UL)  // wake GPS 5 min before next advert
#endif
// Minimum on-time after waking GPS, to give it time to lock even if the
// advert timer is very close. Also prevents thrashing the GPS_EN pin.
#ifndef CR_GPS_MIN_AWAKE_MS
#define CR_GPS_MIN_AWAKE_MS           (60UL * 1000UL)        // keep awake at least 1 min once turned on
#endif
// Periodic motion-check wake, independent of the advert schedule. Even in
// static (1h-advert) mode we want to notice movement promptly so we can
// switch the advert cadence. 15 min keeps motion tracking responsive.
#ifndef CR_GPS_MOTION_CHECK_INTERVAL_MS
#define CR_GPS_MOTION_CHECK_INTERVAL_MS  (15UL * 60UL * 1000UL)
#endif
// When the advert does NOT carry position (advert_loc_policy == NONE), we
// still wake GPS occasionally for RTC time sync and last-known-position
// cache refresh. Longer interval — extra energy is negligible.
#ifndef CR_GPS_TIME_SYNC_INTERVAL_MS
#define CR_GPS_TIME_SYNC_INTERVAL_MS     (6UL * 3600UL * 1000UL)
#endif
// Reise-Wunsch 2026-06-08: Cold-Start-Cycle bevor !_gps_had_fix_ever.
// Manche GPS-Module brauchen bis 10 Min fuer einen ersten Almanac-Load.
// 10 min an + 10 min aus = 50% Stromverbrauch wenn der Cold-Start
// lange dauert. Vorher: GPS dauerhaft an = 100% bis erster Fix.
#ifndef CR_GPS_COLD_START_WAKE_MS
#define CR_GPS_COLD_START_WAKE_MS        (10UL * 60UL * 1000UL)
#endif
#ifndef CR_GPS_COLD_START_SLEEP_MS
#define CR_GPS_COLD_START_SLEEP_MS       (10UL * 60UL * 1000UL)
#endif

// periodic advert feature — dynamic interval
// 3h: no location in advert, or GPS enabled but no fix
// 1h: static position (GPS off, or GPS-fix without motion in the last window)
// 15min: GPS-fix and movement > CR_MOTION_RADIUS_M in CR_MOTION_WINDOW_MS
#define CR_ADVERT_INT_NO_LOC_MS    (3UL * 3600UL * 1000UL)
#define CR_ADVERT_INT_STATIC_MS    (1UL * 3600UL * 1000UL)
#define CR_ADVERT_INT_MOVING_MS    (15UL * 60UL * 1000UL)
#define CR_MOTION_WINDOW_MS        (10UL * 60UL * 1000UL)
#define CR_MOTION_RADIUS_M         370.0
// Tolerance for the boot-time comparison against the persisted position
// (sensors.node_lat/lon). Wider than CR_MOTION_RADIUS_M to absorb the
// jitter that a cheap GPS module produces on a fresh cold-start fix.
#define CR_BOOT_MOVE_TOLERANCE_M   120.0
// Hysterese-Schwelle für maybePushGeoRecommendation(): solange wir uns
// weniger als diesen Wert vom letzten Auswertungs-Anker entfernt haben,
// wird die Geo-Scope-Empfehlung NICHT neu berechnet. Bei 40 km/h (Auto)
// löst der Anker alle ~15-20 min eine Neu-Auswertung aus; ein Knoten,
// der genau auf einer Bundesland-Grenze parkt, bleibt ruhig.
#define CR_GEO_RECO_REEVAL_DIST_M  10000.0
// Trace-Flags: RAM-only Bitmask die selektiv Events als Companion-Channel-
// Messages pusht. Setzen via "trace <cat> <0..3>" Befehl.
//
// DL9SAU 2026-07-13 (Level-Umbau): Jede Kategorie hat jetzt ein 4-stufiges
// LEVEL statt nur an/aus -- 0=aus, 1=wichtig, 2=verbose, 3=debug (2 Bit pro
// Kategorie, gepackt in NodePrefs.trace_levels uint64). Eine Meldung wird
// ausgegeben, wenn Kategorie-Level >= Meldungs-Level ist (Default-Meldungs-
// level = 1, siehe traceCompanion vs traceCompanionL). Die #define-Werte
// unten sind weiterhin Einzel-Bits: sie dienen (a) als Kategorie-IDENTITAET
// (Bit-Index = __builtin_ctz(flag) = 2-Bit-Slot-Index in trace_levels) und
// (b) als abgeleitete Aktiv-Bitmaske _trace_flags (Bit gesetzt wenn Level>0
// und nicht pausiert) fuer die vielen bestehenden 'traceCompanion(FLAG,..)'
// und '_trace_flags & FLAG'-Checks (= Level >= 1).
//
// Persistenz: trace_levels (uint64) ueberlebt Reboot und wird beim Boot
// via recomputeTraceFlags() wieder aktiv (fruher: bewusst Reset-bei-Reboot;
// aufgegeben zugunsten 'delivery immer an'-Wunsch). Pausieren ohne Verlust
// der Auswahl via 'trace off' (_trace_paused, RAM-only).
#define TRACE_GPS       0x0001
#define TRACE_ADVERTS   0x0002
#define TRACE_REPEAT    0x0004
#define TRACE_SCOPE     0x0008
#define TRACE_MOTION    0x0010
#define TRACE_HEARD     0x0020
#define TRACE_RTC       0x0040
#define TRACE_CONNECT   0x0080
#define TRACE_FILTER    0x0100
#define TRACE_NIGHT     0x0200
#define TRACE_DUTY      0x0400
#define TRACE_MSGSTORE  0x0800   // Offline-Queue Bucket-Save zu Flash
#define TRACE_BT        0x1000   // BLE-Diagnose (heap + disconnect-counter)
#define TRACE_DISCOVER  0x2000   // discover regions ANON-RESP + leer-Diagnose
#define TRACE_DBG_ANON  0x4000   // Bug-5-Debug: ANON-TX/RX hex-dump (CLI vs App)
#define TRACE_COALESCE  0x8000   // Resend-Coalescing: Timestamp aus Cache korrigiert / Eintrag entfernt
#define TRACE_DELIVERY  0x10000  // Verbreitung eigener Sends: gehoerte Repeats (Channel) + DM-ACK
#define TRACE_GEO       0x20000  // Geo-Status-Meldung (frueher immer-an) als eigene Kategorie
#define TRACE_CAT_N     18       // Anzahl Kategorien (= hoechster Bit-Index + 1); passt in uint64 (max 32)
#define TRACE_ALL_MASK  0x3FFFF

// Duty-Cycle-Schutz: regulatorische 10% TX-Airtime pro rollendem 1h-Fenster
// (EU SRD 869 narrow). Sliding-Window mit 60 Slots à 1 Minute (millis-basiert,
// GPS/RTC-unabhaengig). Threshold-Schwellen als %% von 360s in _prefs.
#define CR_DUTY_WINDOW_SLOTS    60UL
#define CR_DUTY_SLOT_MS         60000UL
#define CR_DUTY_HARD_BASE_MS    360000UL   // 10% von 1h = 360s = "100% Limit"

// auto_advert_enabled-Bitmask (siehe NodePrefs.h Kommentar)
#define AUTO_ADV_ZEROHOP        0x01
#define AUTO_ADV_NIGHTLY        0x02
#define AUTO_ADV_ALL            (AUTO_ADV_ZEROHOP | AUTO_ADV_NIGHTLY)
// 5 min after boot when GPS is off or has already obtained a fix; if GPS is
// enabled but still searching, wait up to 10 min so that the first advert can
// already carry a position. When the first fix arrives during the wait, the
// 10-min deadline is shortened to 5 min from boot.
#define CR_PERIODIC_ADVERT_BOOT_DELAY_MS      (5UL  * 60UL * 1000UL)
#define CR_PERIODIC_ADVERT_BOOT_DELAY_GPS_MS  (10UL * 60UL * 1000UL)
#ifndef LOCAL_TZ_OFFSET_SECS
#define LOCAL_TZ_OFFSET_SECS    3600   // CET (winter base); override per-build for other zones
#endif
// EU-DST automatisch handhaben (Last-Sunday-of-March 01:00 UTC -> +1h,
// Last-Sunday-of-October 01:00 UTC -> zurueck). Fuer andere Regionen
// im Build deaktivieren (-DLOCAL_TZ_DST_EU=0).
#ifndef LOCAL_TZ_DST_EU
#define LOCAL_TZ_DST_EU         1
#endif
#define CR_NIGHT_FLOOD_START_HOUR_LOCAL 23
#define CR_NIGHT_FLOOD_END_HOUR_LOCAL    5     // exclusive

// (override-expiry ist jetzt absolut in _prefs.override_expiry, kein
//  relative max-age mehr; Default-TTL bei 'scope override <name>' ohne
//  Suffix = 12h.)

// When sending a channel text message, truncate the sender name (prefixed
// as "name: msg" in the group payload) to this many whitespace-separated
// words. Long full names eat into the airtime/space available for the
// actual message body. Self-Adverts and direct contact display still use
// the full _prefs.node_name.
#ifndef CR_CHANNEL_SENDER_MAX_WORDS
#define CR_CHANNEL_SENDER_MAX_WORDS  2
#endif

struct HeardEntry {
  uint8_t  hash;         // 1-byte protocol hash (pub_key[0])
  uint32_t last_heard;   // RTC unix time; 0 = empty slot
};

class MyMesh : public BaseChatMesh, public DataStoreHost {
public:
  MyMesh(mesh::Radio &radio, mesh::RNG &rng, mesh::RTCClock &rtc, SimpleMeshTables &tables, DataStore& store, AbstractUITask* ui=NULL);

  void begin(bool has_display);
  void startInterface(BaseSerialInterface &serial);

  const char *getNodeName();
  NodePrefs *getNodePrefs();
  uint32_t getBLEPin();

  void loop();
  void handleCmdFrame(size_t len);
  bool advert();
  void enterCLIRescue();
  // DL9SAU 2026-07-12: Button-Long-Press-Gate. true = Shutdown erlaubt.
  // Bei Sperre (button_press_allow_shutdown==0) Serial-Log + $companion-Meldung.
  bool requestButtonShutdown();

  int  getRecentlyHeard(AdvertPath dest[], int max_num);

protected:
  float getAirtimeBudgetFactor() const override;
  int getInterferenceThreshold() const override;
  int getAGCResetInterval() const override;
  bool getCADEnabled() const override;
  int calcRxDelay(float score, uint32_t air_time) const override;
  uint32_t getRetransmitDelay(const mesh::Packet *packet) override;
  uint32_t getDirectRetransmitDelay(const mesh::Packet *packet) override;
  // Effektiver Faktor (Auto-Sentinel aufgeloest). Fuer Status-Anzeigen.
  float effectiveTxDelayFactor() const;
  float effectiveDirectTxDelayFactor() const;
  // Effektive Hop-Caps mit follow-Sentinel-Aufloesung (Wunschliste 39).
  uint8_t effectiveFloodMaxInfra() const;
  uint8_t effectiveFloodMaxReqResp() const;
  bool shouldReduceFloodRetransmit(const mesh::Packet* packet, uint8_t original_path_count) const override;
  uint8_t getExtraAckTransmitCount() const override;
  bool filterRecvFloodPacket(mesh::Packet* packet) override;
  bool allowPacketForward(const mesh::Packet* packet) override;

  void sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis);
  void sendFloodScoped(const ContactInfo& recipient, mesh::Packet* pkt, uint32_t delay_millis=0) override;
  void sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis=0) override;

  // Wunschliste 2026-07-02: Magic-Scope-Namen beim Senden. Wenn der
  // App-User im Scope-Feld eines Channels (oder als default_scope) einen
  // dieser Namen setzt, entfaltet die Firmware Sonderverhalten:
  //   #direct/#direkt  -> zero-hop, kein Scope
  //   #unscoped        -> klassischer flood ohne Scope-Code (path>0)
  //   #geo             -> chooseGeoFallbackScope (Fallback: #local
  //                       aus Build-in-Table analog chooseNightFloodScope)
  // Alle anderen Scopes werden normal als Transport-Code gesendet.
  // Erkennung ueber vorberechnete SHA(#name)-Keys.
  enum MagicScope : uint8_t {
    MS_NONE = 0,
    MS_DIRECT,      // #direct/#direkt
    MS_UNSCOPED,    // #unscoped
    MS_GEO,         // #geo
  };
  MagicScope detectMagicScope(const TransportKey& scope);
  void initMagicScopeKeys();
  // Channel-Name-Force fuer #local/#lokal: unabhaengig von Scope-Magic.
  // User-Test 2026-06-XX: bei Channel '#local' wurde User via 80km/2hops
  // gehoert weil er dabei einen weiten Scope gewaehlt hatte -- das
  // konterkariert 'local'. Firmware erzwingt Scope=#local bzw #lokal
  // ungeachtet App-Scope. Returns "local"/"lokal" oder NULL wenn kein Match.
  const char* detectForcedLocalChannel(const mesh::GroupChannel& channel);
  TransportKey _magic_direct_key;
  TransportKey _magic_direkt_key;
  TransportKey _magic_norepeat_key;
  TransportKey _magic_no_repeat_key;
  TransportKey _magic_unscoped_key;
  TransportKey _magic_geo_key;
  bool _magic_scope_keys_inited = false;

  // Wunschliste 2026-07-01: Path-Bytes als 'aa,bb,cc' formatieren.
  // Zeigt Adressierungsbreite (1/2/3-Byte Hop-IDs) sowie welche Repeater
  // konkret auf dem Weg waren -- diagnostisch wertvoll bei Rerouting.
  static void formatPathBytes(char* out, size_t out_size, const uint8_t* path, uint8_t path_len);

  // DL9SAU Wunschliste 82: zentraler TX-Choke-Point. Wenn _tx_blocked
  // gesetzt ist, wird das Paket sofort an den Pool zurueckgegeben statt
  // queueOutbound zu rufen. Alle send-Pfade (sendFlood/Direct/ZeroHop/
  // ACK + Repeat in Mesh.cpp) muenden hier ein -- Dispatcher::sendPacket
  // ist seit 2026-06-18 dafuer virtual.
  void sendPacket(mesh::Packet* packet, uint8_t priority, uint32_t delay_millis=0) override;

  void logRxRaw(float snr, float rssi, const uint8_t raw[], int len) override;
  bool isAutoAddEnabled() const override;
  bool shouldAutoAddContactType(uint8_t type) const override;
  bool shouldOverwriteWhenFull() const override;
  uint8_t getAutoAddMaxHops() const override;
  void onContactsFull() override;
  void onContactOverwrite(const uint8_t* pub_key) override;
  bool onContactPathRecv(ContactInfo& from, uint8_t* in_path, uint8_t in_path_len, uint8_t* out_path, uint8_t out_path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) override;
  void onDiscoveredContact(ContactInfo &contact, bool is_new, uint8_t path_len, const uint8_t* path) override;
  // SNR der Advert für die Quality-Klassifikation in onDiscoveredContact merken.
  void onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp, const uint8_t* app_data, size_t app_data_len) override;

  // Wunschliste 31: Advert-basierte RTC-Sync. Wird aus onAdvertRecv
  // aufgerufen wenn das Paket Zero-Hop ist und einen plausiblen
  // adv_type hat. Implementiert lazy/strict-Modi, Boot-Collection,
  // Plausibility, 24h-Cap, Single-Source-Heuristik mit Boot-Bypass.
  void maybeAdvertTimeSync(const mesh::Identity& id, uint32_t adv_timestamp, uint8_t adv_type);
  void timeSyncFinalizeLazyCollection();   // wird aus loop() / onAdvertRecv aufgerufen
  bool isGpsAuthoritative() const;

  // Wunschliste 32 v2: RAM-Cache der channel_hops Werte pro Slot.
  // Wird aus _prefs.channel_hops_list (name-hash-basiert, slot-stable)
  // aufgebaut. Per-Paket-Lookup in shouldRepeat() ist O(channels-matching-
  // ch_hash). Rebuild auf: boot (nach channels laden), CMD_SET_CHANNEL,
  // set ch.hops / ch.hops clear, setupCompanionChannel.
  uint8_t _channel_hops_cap_cache[MAX_GROUP_CHANNELS];
  void rebuildChannelHopsCache();
  // Hilfsfunktion: Eintrag in channel_hops_list per Name-Hash finden.
  // Returns index in der Liste (0..count-1) oder -1.
  int findChannelHopsEntry(uint32_t name_fnv1a) const;

  // Wunschliste 34/35: Runtime-effektiver Repeater-Zustand.
  // client_repeat ist der STORED user-wunsch. Effective ist abhaengig
  // vom profile (defensive vs normal) und im defensive-Fall auch von
  // freq+force. Bei freq-Wechsel auf non-strict ohne force: wish=on
  // bleibt im Storage, runtime suppressed transparent zu 'repeater off'.
  bool isRepeatingEffectivelyAllowed() const;
  // Cached Bool fuer per-Packet-Check (vermeidet pro Paket
  // isValidClientRepeatFreq + Branches). Wird aktualisiert via
  // recomputeRepeatingAllowed() bei jeder Aenderung von freq/bw/
  // client_repeat/force/profile.
  bool _repeating_allowed;
  // Recompute + ggf. Trace bei Transition. Call nach jedem stored-state-
  // Wechsel der die effektive Erlaubnis beeinflussen koennte.
  void recomputeRepeatingAllowed(const char* reason);
  void onContactPathUpdated(const ContactInfo &contact) override;
  ContactInfo* processAck(const uint8_t *data) override;
  void queueMessage(const ContactInfo &from, uint8_t txt_type, mesh::Packet *pkt, uint32_t sender_timestamp,
                    const uint8_t *extra, int extra_len, const char *text);

  void onMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                     const char *text) override;
  void onCommandDataRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                         const char *text) override;
  void onSignedMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                           const uint8_t *sender_prefix, const char *text) override;
  void onChannelMessageRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint32_t timestamp,
                            const char *text) override;
  void onChannelDataRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint16_t data_type,
                         const uint8_t *data, size_t data_len) override;

  uint8_t onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                           uint8_t len, uint8_t *reply) override;
  void onContactResponse(const ContactInfo &contact, const uint8_t *data, uint8_t len) override;

  // Wunschliste 7 Phase 3: ANON_REQ Discovery-Queries (OWNER/REGIONS/BASIC).
  // Override Mesh::onAnonDataRecv. Gating per effectiveAdvertRole().
  void onAnonDataRecv(mesh::Packet* packet, const uint8_t* secret,
                      const mesh::Identity& sender, uint8_t* data, size_t len) override;

  // Wunschliste 7 Phase 2: effektiver Advert-Role (Auflösung von 'auto').
  //   Returns ADV_TYPE_* (1=CHAT, 2=REPEATER, 3=ROOM, 4=SENSOR).
  // Wird sowohl von createSelfAdvert als auch von den Discovery-Query-
  // Handlern verwendet (single source of truth).
  uint8_t effectiveAdvertRole() const;
  // Shadows BaseChatMesh::createSelfAdvert(...). Verwendet
  // effectiveAdvertRole() statt hartcodiert ADV_TYPE_CHAT.
  mesh::Packet* createSelfAdvert(const char* name);
  mesh::Packet* createSelfAdvert(const char* name, double lat, double lon);

  // Wunschliste 6b: Loop-Detection. Zaehlt wie oft unser self_id-Hash
  // (in der angegebenen Hash-Size) im Path bereits vorkommt. Returns
  // true wenn n >= max_counters[hash_size]. Analog simple_repeater.
  bool isLooped(const mesh::Packet* packet, const uint8_t max_counters[]) const;

  // Wunschliste 15: zero-hop-Neighbour zur Tabelle hinzufuegen oder
  // updaten. LRU-Verdraengung wenn voll. Wird aus onAdvertRecv gerufen
  // wenn packet->path_len == 0 (= zero-hop, direkt gehoert).
  void putRuntimeNeighbour(const mesh::Identity& id, uint32_t advert_timestamp,
                           int8_t snr_q4, int8_t rssi_dbm,
                           uint8_t adv_type,
                           const char* scope_name);
  void onControlDataRecv(mesh::Packet *packet) override;
  void onRawDataRecv(mesh::Packet *packet) override;
  void onTraceRecv(mesh::Packet *packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                   const uint8_t *path_snrs, const uint8_t *path_hashes, uint8_t path_len) override;

  uint32_t calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const override;
  uint32_t calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const override;
  void onSendTimeout() override;

  // DataStoreHost methods
  bool onContactLoaded(const ContactInfo& contact) override { return addContact(contact); }
  bool getContactForSave(uint32_t idx, ContactInfo& contact) override { return getContactByIdx(idx, contact); }
  bool onChannelLoaded(uint8_t channel_idx, const ChannelDetails& ch) override { return setChannel(channel_idx, ch); }
  bool getChannelForSave(uint8_t channel_idx, ChannelDetails& ch) override { return getChannel(channel_idx, ch); }

  void clearPendingReqs() {
    pending_login = pending_status = pending_telemetry = pending_discovery = pending_req = 0;
  }

public:
  void savePrefs() {
    _store->savePrefs(_prefs, sensors.node_lat, sensors.node_lon);
  }

  // Wunschliste 53 (2026-06-14): main.cpp meldet beim Boot die Reset-
  // Reason (Plattform-spezifisch ausgelesen, einheitlich gemappt).
  void setLastResetReason(uint8_t kind) { _last_reset_reason = kind; }
  uint8_t getLastResetReason() const    { return _last_reset_reason; }
  // String-Mapping (mirrored main.cpp resetKindStr). Indirektion damit
  // CLI/stats-core direkt aufrufen koennen ohne main.cpp-Symbol-Dependency.
  const char* getLastResetReasonStr() const {
    switch (_last_reset_reason) {
      case 1: return "COLD";
      case 2: return "WARM";
      case 3: return "WDT";
      case 4: return "PANIC";
      case 5: return "BROWNOUT";
      default: return "UNKNOWN";
    }
  }
  uint32_t getLastSessionUptimeMs() const { return _last_session_uptime_ms; }

#if defined(NRF52_PLATFORM)
  // 2026-06-15 T1000-E Workaround: getter fuer main.cpp::petWatchdog().
  // Wenn true, hoert main.cpp auf zu petten -> WDT feuert -> Reset.
  // Sauberster Weg da NVIC_SystemReset() auf T1000-E nicht zuverlaessig
  // reseted (auch nicht via Adafruit reset_mcu Pattern).
  bool shouldStopPettingWdt() const { return _wdt_let_fire; }
  // armWdtReset(): wenn WDT noch nicht laeuft, mit 5s Timeout starten
  // (NRF52-WDT ist nach Start nicht stoppbar -- macht nichts, wir
  // resetten gleich). Dann flag setzen -> petWatchdog() stoppt -> WDT
  // feuert spaetestens nach 5s (oder ≤90s wenn unser App-WDT bereits
  // mit Default-Timeout laeuft). Universeller Reset-Mechanismus weil
  // NVIC_SystemReset auf T1000-E nicht greift.
  void armWdtReset();
#endif

  // Wunschliste 53 Phase 7 (2026-06-14): Boot-Log Public-API.
  // /boot_log.txt in LittleFS, Ring-Buffer 10 Eintraege, plain-text.
  // Eintrags-Schema: "<unixsec> <cause>[ uptime=<dur>]\n"
  //   - unixsec aus rtc_clock zur Schreibzeit (kommt aus rtc_persist
  //     +- 10min beim Boot, exakt nach App-Sync)
  //   - cause aus _last_reset_reason (COLD/WARM/WDT/PANIC/...)
  //   - uptime aus _last_session_uptime_ms (rtc_persist piggyback);
  //     bei COLD ohne, weil Power-Cycle das uptime-tracking resettet.
  // CLI: 'log read|clear', 'log dest <usb|channel> <on|off>',
  //      'log' allein -> Status.
  void          bootLogAppend();         // Boot-Time: Eintrag fuer
                                         //  jetzigen Boot
  // Pre-Reboot-Eintrag mit exakter Uptime. cause default "WARM(cli)"
  // fuer reboot-CLI, "LORA-DEAD" fuer Stage 9 SPI-Liveness-Trigger.
  void          bootLogWritePreReboot(const char* cause = "WARM(cli)");
  void          bootLogPrint();          // CLI 'log read'
  void          bootLogClear();          // CLI 'log clear'
  // DL9SAU 2026-06-20: Boot-Phase Check fuer shutdown_pending Pref.
  // Wenn Pref=1 UND USB nicht stable an -> board.powerOff (no return).
  // Public, weil aus main.cpp setup() aufgerufen (nach serial_interface.begin).
  void          applyShutdownPendingCheck();
  // DL9SAU 2026-07-12 (Power Weg A): HW-Comparator (LPCOMP+VBUS) mit der
  // effektiven Recovery-Schwelle armieren. Public -- aus main.cpp setup()
  // (nach the_mesh.begin) UND aus dem 'set batt_min_mv_boot'-Handler.
  void          configureBatteryWake();

#if ENV_INCLUDE_GPS == 1
  void applyGpsPrefs() {
    sensors.setSettingValue("gps", _prefs.gps_enabled ? "1" : "0");
    if (_prefs.gps_interval > 0) {
      char interval_str[12];  // Max: 24 hours = 86400 seconds (5 digits + null)
      sprintf(interval_str, "%u", _prefs.gps_interval);
      sensors.setSettingValue("gps_interval", interval_str);
    }
  }
#endif

  // To check if there is pending work
  bool hasPendingWork() const;
  // Wunschliste 86 Phase 1: ESP32-Light-Sleep darf NICHT laufen
  // waehrend der USB-Serial-CLI gerade einen Command bearbeitet
  // (sonst geht spaeter eintreffender Output verloren). Eigentlicher
  // Async-Async-Window auch beachtet via _serial_cli_async_expiry_ms.
  bool isSerialCliActive() const;

private:
  void writeOKFrame();
  void writeErrFrame(uint8_t err_code);
  void writeDisabledFrame();
  void writeContactRespFrame(uint8_t code, const ContactInfo &contact);
  void updateContactFromFrame(ContactInfo &contact, uint32_t& last_mod, const uint8_t *frame, int len);
  void addToOfflineQueue(const uint8_t frame[], int len);
  int getFromOfflineQueue(uint8_t frame[]);
  int getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) override { 
    return _store->getBlobByKey(key, key_len, dest_buf);
  }
  bool putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], int len) override {
    return _store->putBlobByKey(key, key_len, src_buf, len);
  }

  void checkCLIRescueCmd();
  void checkSerialInterface();
  bool isValidClientRepeatFreq(uint32_t f) const;
  bool signalFitsInIsmBand(uint32_t freq_khz, uint32_t bw_hz) const;

  // Koordinaten im nautischen DM-Format "DD-MM,M N/S DDD-MM,M E/W"
  // (Komma als Dezimal-Trenner, Grad-Breite 2 fuer Lat, 3 fuer Lon mit
  // fuehrenden Nullen). Anzeige-Kontext in allen User-sichtbaren
  // Ausgaben (status, gps, motion-trace, geo-scope, scope info bbox).
  // NICHT in 'get/set' -- dort bleiben Dezimalgrade (config backup).
  // Output benoetigt mindestens 18 Byte ("DD-MM,M N DDD-MM,M E\0").
  void formatLatLonDM(char* out, size_t out_size, double lat, double lon) const;
  // Copies _prefs.node_name into dest, truncated to the first
  // CR_CHANNEL_SENDER_MAX_WORDS whitespace-separated words.
  void copyShortSenderName(char* dest, size_t dest_size) const;
  // Region table support — name from dl9sau_regions[] that matches the
  // packet's transport_codes[0], or NULL if no entry matches (unknown
  // scope or unscoped packet).
  void initRegionKeys();
  const char* lookupRegionByTransportCode(const mesh::Packet* packet) const;
  // DL9SAU 2026-06-16: Berechnet Scope-Hash + befuellt scope_buf mit
  // dem displayfaehigen Scope-Label ("#name" / "#?" / "#*"). 2 Stellen
  // hatten denselben 15-Zeiler inline (Z 2074-2089 + 3086-3101).
  // scope_buf min 36 Byte.
  void computeScopeLabel(const mesh::Packet* pkt,
                         uint32_t& scope_h,
                         char* scope_buf, size_t scope_buf_sz) const;
  // Format a debug line, write it to Serial AND push it to the app as a
  // PUSH_CODE_DEBUG_LOG frame (so the user can inspect logs in the app's
  // Debug-Protokolle view when no USB-Serial is attached, e.g. mobile).
  void pushDebugLog(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
  // Re-evaluate geo-based scope recommendations for the given lat/lon and
  // push a debug-log line if the result changed since the last call.
  // No-op if either coordinate is exactly 0.0.
  void maybePushGeoRecommendation(double lat, double lon);

  // Companion-Channel (lokal, kein RF): legt beim Boot einmalig den Channel
  // "companion" an (oder findet ihn falls schon persistiert) und merkt sich
  // den Index in _companion_channel_idx.
  void setupCompanionChannel();
  // PSK-basierte Identifikation eines beliebigen Channel-Slots als
  // unseren lokalen $companion-Channel (Magic-PSK match). Robust gegen
  // App-Rename + App-Index-Drift.
  bool isCompanionChannel(uint8_t channel_idx);

  // ===== Wunschliste 27: CTL_TYPE_NODE_DISCOVER =============================
  // Zwei separate Konzepte mit eigener Namens-Konvention:
  //
  //   discover*      -- WIR senden REQ, sammeln RESPs. PRO-AKTIV.
  //                     Diagnose-CLI 'discover'. Im Client- und im
  //                     Repeater-Mode verfuegbar. "ich gehe finden".
  //
  //   discoverable*  -- WIR antworten auf eingehende REQs. PASSIV.
  //                     Nur aktiv im full-repeater-mode (profile=normal +
  //                     advert_role=repeater). "ich bin auffindbar".
  //
  // Beide nutzen dasselbe Wire-Protokoll, sind aber konzeptuell verschieden.
  //
  // Protokoll-Refresher (simple_repeater MyMesh.cpp:772-826):
  //   REQ-Payload:  [type|prefix_only][filter][tag×4][since×4] = 10 byte
  //   RESP-Payload: [type|adv_type][snr][tag×4][pub_key×32 oder ×8] = 14/38 byte
  // Wann unser aktueller Discover-Lauf gestartet ist (RTC). DiscoverEntries
  // mit recv_at_rtc < diesem Wert stammen aus dem 15-min-Cache, NICHT
  // aus einer frischen Antwort. Trennung im Output (User-Wunsch 2026-06-12:
  // "1 Antworten" suggerierte fresh, war aber Cache-Treffer).
  uint32_t _discover_round_started_rtc = 0;
  // ---- (a) Sender 'discover*' ---------------------------------------------
  // 2026-07-06 REFACTOR: _discover_entries[] + _discover_count entfernt.
  // Ersetzt durch _discovery[MAX_DISCOVERY] (siehe unten). Ein Slot pro
  // pubkey mit vollem CTL + Regions + Chain-Send State.
  uint32_t      _discover_tag;
  // Piggyback 2026-07-04: App-triggered Discovers mit-processen. Wenn die
  // App CMD_SEND_CONTROL_DATA mit CTL_TYPE_NODE_DISCOVER_REQ schickt,
  // extrahieren wir den Tag und matchen ihn zusaetzlich in
  // discoverHandleResp. Damit fuellt sich unser _discover_entries[] auch
  // wenn User die Discovery via App (nicht CLI) startet.
  uint32_t      _app_discover_tag;
  // 2026-07-05: Filter aus CTL-REQ merken damit Ausgabe zwischen
  // "discover repeater" / "discover sensor" / "discover all" unterscheiden
  // kann. 0 = nicht app-triggered oder unbekannt.
  uint8_t       _app_discover_filter = 0;
  // Regions-Modus wird per follow-up CMD_SEND_ANON_REQ (REGIONS-type)
  // erkannt -- App sendet nach den CTL-RESPs eine ANON-Query je REPEATER.
  bool          _app_discover_regions_mode = false;
  bool          _discover_active;

  // 2026-07-06 REFACTOR: ChainSendPending entfernt -- chain_send_at_ms
  // ist jetzt Feld in DiscoveryEntry.
  void manageChainSends();
  unsigned long _discover_expiry_ms;
  unsigned long _discover_next_allowed_ms; // rate-limit: 60s seit letztem REQ
  // RTC-Timestamp wann der letzte discover-Batch endete (>=1 Antwort).
  // Verwendet von 'neighbors' um discover-only-Eintraege (nicht in
  // Kontaktliste) bis 48h nachzulisten.
  uint32_t      _discover_last_at_rtc;

  // ====================================================================
  // 2026-07-06 REFACTOR: Unified DiscoveryEntry
  //
  // Ersetzt die 4 vorherigen Listen (_discover_entries, _regions_pending,
  // _regions_completed, _chain_send_pending) durch EINE Liste mit einem
  // Slot pro pubkey. Verhindert strukturell:
  //   - Doppelanfragen (Chain-Vorab + CTL-triggered) durch zentralen
  //     region_query_pending_tag + Dedup gegen region_answered_at_rtc.
  //   - Doppel-Slots durch findOrAddByPubkey (nie 2 Slots pro pubkey).
  //   - Duplikate im Print durch nur EINE Iteration.
  //   - Zombie-pendings durch state-machine mit klaren Uebergaengen.
  // TTL-Purge (15 min) in discoverStart -- Roll-Forward-Cache.
  // ====================================================================
  struct DiscoveryEntry {
    // ---- Identity ----
    uint8_t  pubkey[32];
    uint8_t  adv_type;               // ADV_TYPE_REPEATER/SENSOR/... (0=unknown)
    bool     full_pubkey;            // false = nur 8-Byte-Prefix
    char     name[32];               // Contact-Name (aus lookup)
    // ---- Global lifetime ----
    uint32_t last_seen_at_rtc;       // max(ctl_answered, region_answered,
                                     // chain_queried, ctl_queried) -- fuer TTL-Purge.
    // ---- CTL-Discovery-State (Repeater/Sensor meldete sich) ----
    uint32_t ctl_answered_at_rtc;    // 0 = nie via CTL-RESP gehoert
    int8_t   their_snr_q4;           // seine SNR-Sicht auf unseren CTL-REQ
    int8_t   our_snr_q4;             // unsere SNR-Sicht auf seine CTL-RESP
    int8_t   our_rssi_dbm;           // unsere RSSI-Sicht
    // ---- Regions-Anfrage State ----
    // WICHTIG: region_csv wird NUR bei einer eingehenden Antwort geaendert.
    // Kein Reset beim erneuten Query, kein Loeschen bei 'keine Antwort'.
    // So bleibt eine "gute" Antwort aus fruherem Discover erhalten wenn der
    // Repeater in einer spaeteren Runde nicht mehr geantwortet hat. TTL-
    // basiertes Purge (last_seen_at_rtc > 15 min) wirft alte Slots
    // vollstaendig raus. Gilt gleichermassen fuer 'discover repeater' (dort
    // wird gar keine REGIONS-Query abgesetzt -> csv bleibt vom letzten mal).
    uint32_t region_queried_at_rtc;  // 0 = nie gefragt (in dieser Session)
    uint32_t region_answered_at_rtc; // 0 = keine Antwort. >0 = update-Zeitstempel.
    uint32_t region_query_tag;       // Match-Tag fuer RESP; 0 = kein pending
    bool     region_from_chain;      // gefragt weil aus _neighbours[] bekannt
    bool     region_from_ctl_trigger;// gefragt weil per CTL-RESP entdeckt
    bool     region_csv_empty_deny;  // aktueller Repeater-Zustand: deny unscoped +
                                     // keine Regionen (leere RESP). Wird gesetzt
                                     // wenn leerer RESP kommt und geloescht bei
                                     // non-leerem RESP. Bei 'keine Antwort'
                                     // bleibt der letzte Zustand erhalten.
    char     region_csv[120];        // letzte erhaltene Regions-CSV oder ""
    // ---- Chain-Send-Queue (delayed send fuer Staffelung) ----
    uint32_t chain_send_at_ms;       // 0 = nicht in queue; >0 = pending send
  };
  static const int MAX_DISCOVERY = 16;
  DiscoveryEntry _discovery[MAX_DISCOVERY];
  uint8_t        _discovery_count;
  // Hilfs-API:
  DiscoveryEntry* discoveryFindByPubkey(const uint8_t* pk32);
  DiscoveryEntry* discoveryFindOrAdd(const uint8_t* pk32);
  void discoveryPurgeStale(uint32_t ttl_secs);
  // Ist der Slot in aktueller Runde schon beantwortet (Regions)?
  // Wird als Dedup-Check verwendet BEVOR eine neue Anfrage rausgeht.
  bool discoveryHasFreshRegionAnswer(const DiscoveryEntry& e,
                                     uint32_t round_started_rtc) const;
  // Neuer Print-Helper -- direkt auf DiscoveryEntry. suppress_suffix
  // fuer 'discover repeater' ohne Regions-Kontext.
  void printDiscoveryLegendEntry(const DiscoveryEntry& e, bool verbose,
                                 bool suppress_suffix = false);

  void discoverStart(uint8_t filter, bool prefix_only);
  void discoverFinishAndPrint();
  void discoverHandleResp(mesh::Packet* packet);
  void discoverLoop();    // in main loop() -- prueft Expiry

  // ---- (b) Responder 'discoverable*' --------------------------------------
  // Rate-Limit auf Responder-Seite: max 4 RESPs pro 2-min Window
  // (analog simple_repeater discover_limiter(4, 120)).
  unsigned long _discoverable_window_start_ms;
  uint8_t       _discoverable_count_window;
  void discoverableHandleReq(mesh::Packet* packet);

  // ---- (c) ANON-Regions-Discovery: bei ANON_REQ_TYPE_REGIONS-CMD von der
  //          App den Typ merken, beim Response ggf. CSV in $companion pushen.
  //          Auch als CLI 'discover regions' (Chain) und 'discover regions
  //          <contact-prefix>' (direkt).
  uint8_t _last_anon_req_type;
  // Multi-Tag-Tracking fuer parallele eigene REGIONS-Queries (CLI- und
  // Chain-getriggert). Notwendig weil pending_req nur 1 outstanding tag
  // halten kann, der Chain aber N parallele REQs verschickt.
  // 2026-07-06 REFACTOR: PendingRegionsEntry + CompletedRegionsEntry
  // entfernt. State ist jetzt Feld in DiscoveryEntry (region_query_tag,
  // region_answered_at_rtc, region_csv, region_csv_empty_deny).
  // Chain-Counter entfernt -- Zaehlung via _discovery-Iteration.
  // Flag: 'discover regions' (no args) hat CTL-Discover ausgeloest und
  // erwartet pro REPEATER-RESP einen automatischen ANON_REQ_TYPE_REGIONS.
  bool          _discover_regions_chained;
  unsigned long _regions_chain_finalize_at;  // millis() Zeitpunkt fuer Aggregat
  // Wunschliste 27 + Anon-Erweiterung 2026-06-11: parametrierbar.
  // req_type = ANON_REQ_TYPE_REGIONS (Default) | _OWNER | _BASIC.
  bool sendAnonQueryZeroHop(const uint8_t* pubkey32, const char* display_name,
                            uint8_t req_type);
  // Legacy-Convenience -- ruft sendAnonQueryZeroHop mit REGIONS.
  bool sendRegionsQueryZeroHop(const uint8_t* pubkey32, const char* display_name);
  // Upsert Regions-Antwort in Slot _discovery. csv_len==0 -> empty_deny.
  void upsertCompletedRegion(const uint8_t* pubkey32, const uint8_t* csv_data,
                             size_t csv_len);
  void finalizeRegionsChain();
  // Wunschliste 28: backup save -- schreibt zwei JSON-Bloecke nach
  // USB-Serial (DL9SAU prefs + node main).
  void backupSaveToSerial();
  // Wunschliste 28 Phase B: Backup-Restore State-Machine.
  // 'backup restore' versetzt die Firmware in Read-Modus auf USB-Serial.
  // Sie liest paste-bytes, erkennt Marker-Linien ('--- BACKUP X BEGIN ---'),
  // sammelt JSON-Body bis brace-balance, parsiert, wendet bekannte Felder
  // an. Ctrl-D (0x04) terminiert sauber, 60 s Timeout abort.
  enum {
    BR_IDLE = 0,
    BR_WAIT_MARKER,   // suche naechste BEGIN-Marker-Linie
    BR_READING_JSON,  // sammle JSON-Body
  };
  uint8_t  _br_state;
  uint8_t  _br_block_type;          // 0=none, 1=dl9sau, 2=main
  char     _br_line[96];
  uint16_t _br_line_len;
  // User-Bug 2026-06-15: 2560 reichte nicht fuer NODE MAIN Block mit
  // ~9 filter_scope_drop-Eintraegen. Bump auf 6KB schluckt das aktuelle
  // Worst-Case + Reserve fuer wachsende Filter-Liste, RAM-Footprint
  // bleibt vertretbar (~3.5KB mehr in BSS).
  char     _br_json[6144];
  uint16_t _br_json_len;
  int16_t  _br_brace_depth;
  bool     _br_in_string;
  bool     _br_escape_next;
  unsigned long _br_timeout_at;
  uint16_t _br_applied;
  uint16_t _br_skipped;
  uint16_t _br_errors;
  // User-Wunsch 2026-06-15: bei errors > 0 muss die App-sichtbare
  // Summary sagen WELCHE keys/Issues fehlschlugen. _br_error_log
  // sammelt komma-separierte Tokens (capped). brRecordError() hängt an.
  char     _br_error_log[120];
  // True wenn restorerte Felder einen Reboot empfehlen (Radio-Params,
  // prv_key). Wird in der End-Statusmeldung gehinted, kein auto-action.
  bool     _br_reboot_recommended;
  // User-Hinweis 2026-06-16: Pre-Clear war direkt zu saveChannels(),
  // bei Korruption permanent verloren. Jetzt deferred -- _br_pre_clear_dirty
  // = true wenn channels[] geclearted aber noch nicht persistiert.
  // saveChannels() erst nach erfolgreichem Block-Apply (brace_depth=0
  // erreicht). Bei Timeout/Abort: Reboot bringt alte Channels via
  // loadChannels() zurueck (channels[] in RAM wurde aber clearted --
  // bis Reboot sind sie temporaer weg).
  bool     _br_pre_clear_dirty;
  // User-Hinweis 2026-06-16: Snapshot von _prefs damit bei Korruption
  // (Paste-Byte-Loss + zufaelliges schliessendes } in den verlorenen
  // Bytes) der teil-applied State rueckgaengig gemacht werden kann.
  // Snapshot bei BEGIN-Marker fuer block_type 1+2 (DL9SAU PREFS / NODE
  // MAIN). Commit (clear taken-flag) bei END-Marker. Restore bei
  // timeout/abort wenn taken-flag noch true.
  NodePrefs _br_prefs_snapshot;
  bool      _br_prefs_snapshot_taken;
  // CLI-Einstieg
  void backupRestoreStart();
  // In loop() pollen
  void backupRestoreLoop();
  // Block-Parser nach komplettem JSON-Body
  void backupRestoreParseBlock();
  // Beenden / abbrechen
  void backupRestoreFinish(const char* reason);
  // 2026-06-15: Error-Logger fuer App-sichtbare Diagnose. Haengt token
  // an _br_error_log an (komma-separiert, capped). NULL/leer = ignore.
  void brRecordError(const char* token);
  // Field-Dispatcher (block_type 1 = DL9SAU, 2 = MAIN)
  void brApplyField(uint8_t block_type, const char* key,
                    const char* val_start, size_t val_len, char val_type);
  void brApplyMeta(const char* val_start, size_t val_len);
  // String/Hex/Array Extraktoren mit JSON-Escape-Dekodierung
  void brExtractString(const char* val_start, size_t val_len,
                       char* dest, size_t dest_max);
  void brExtractHex(const char* val_start, size_t val_len,
                    uint8_t* dest, size_t dest_max);
  void brExtractUint8Array(const char* val_start, size_t val_len,
                           uint8_t* dest, size_t dest_count);
  // Pusht einen Text als synthetische incoming-channel-message für den
  // Companion-Channel. Sendername = board.getManufacturerName(). Wenn die App
  // nicht connected ist landet die Nachricht in der Offline-Queue (16 Slots,
  // älteste Channel-Msg fliegt raus bei Overflow) und wird beim nächsten BLE-
  // Connect via PUSH_CODE_MSG_WAITING ausgeliefert. No-op wenn der Companion-
  // Channel nicht angelegt werden konnte (_companion_channel_idx == 0xFF).
  void pushCompanionMessage(const char* text);
  // Parst und führt einen vom User über den Companion-Channel gesendeten
  // Befehl aus. Antwort wird via pushCompanionMessage() zurückgegeben.
  void handleCompanionCommand(const char* cmd);

  // Wunschliste 10 (2026-06-11): USB-Serial Plain-Text CLI.
  // Auf BLE-Builds ist die USB-Serial-Schiene fuer User-Use frei (App
  // connectet via BLE). Plain-Text-Terminal: User tippt Befehle wie
  // im Companion-Channel; pushCompanionMessage-Output wird waehrend
  // der Dispatch-Phase nach Serial.println umgeleitet (statt BLE-Frame).
  // Aktivierung: erstes Enter macht Prompt sichtbar. Exit: Ctrl-D.
  // Trace-Output kann mit 'logging usb off' unterdrueckt werden falls
  // er die Anzeige stoert.
  // _serial_cli_active: TRUE waehrend pushCompanionMessage-Output zu
  // Serial umgeleitet wird (waehrend Dispatch eines per Serial-CLI
  // eingegangenen Befehls).
  bool     _serial_cli_active = false;
  // Wunschliste 10 (2026-06-14): wenn ein CLI-Dispatch eine ASYNC-Antwort
  // produziert (z.B. 'discover regions' kommt nach bis zu 60s zurueck mit
  // Progressive-Output), ist _serial_cli_active beim Eintreffen schon false
  // -- pushCompanionMessage wuerde an BLE/companion routen statt an Serial.
  // _serial_cli_async_expiry_ms verlaengert das Serial-Routing fuer die
  // Dauer des async-Window. 0 = kein async-window aktiv. Aufrufer (z.B.
  // discoverStart) setzt es bei Start.
  unsigned long _serial_cli_async_expiry_ms = 0;
  // _serial_cli_temp_on: Runtime-Override 'serial-cli on-temp'. Reboot
  // verwirft -- Persistenz liegt in _prefs.serial_cli_persist_on.
  bool     _serial_cli_temp_on = false;
  // Prompt-Pending: erkennt wenn BR-Restore oder CLI-Rescue Serial
  // uebernommen hat; emittiert "> " erst wenn sie wieder fertig sind.
  bool     _serial_cli_prompt_pending = false;
  char     _serial_cli_buf[200];
  uint16_t _serial_cli_pos = 0;
  void serialCliLoop();
  // Effektiver CLI-on-Status: persist || temp.
  bool serialCliEffectiveOn() const {
    return (_prefs.serial_cli_persist_on != 0) || _serial_cli_temp_on;
  }

  // Wortlaengen-Prefix Dispatch fuer CLI-Tokens (User-Wunsch 2026-06-11:
  // 'fi sen dr nam remove foo' = 'filter sender drop name remove foo').
  // Skippt fuehrendes Whitespace, ruft match_choice, gibt bei
  // Mehrdeutig/Fehlend/Unbekannt eine Companion-Message aus und liefert
  // -1 zurueck. Bei Erfolg: schiebt p past matched token + ws, liefert
  // den Index. Konsolidiert die ehemals dupliziterten strncmp+laengen-
  // checks der Sub-Sub-Befehle (User-Konsolidierungs-Wunsch 2026-06-11).
  int dispatchToken(const char*& p,
                    const struct CompanionChoice* choices, int nchoices,
                    const char* expected_label);

  // Wunschliste 43 (2026-06-10): BLE-Power-Cycle State Machine.
  //   BLE_PWR_BOOT      = Boot-Grace (10 min, BLE an, wartet auf ersten Connect)
  //   BLE_PWR_AWAKE     = App connected ODER nach Connect-Hot-Start (5 min)
  //   BLE_PWR_HOT_START = 5 min nach Disconnect, BLE noch an
  //   BLE_PWR_SLEEP     = Cycle: 180s aus
  //   BLE_PWR_WAIT      = Cycle: 30s an, wartet ob jemand connectet
  //   BLE_PWR_TMP_OFF   = runtime off (Pref nicht persistiert), Wake nur Button
  //   BLE_PWR_OFF       = Pref-persistent off
  enum BlePwrState : uint8_t {
    BLE_PWR_BOOT = 0,
    BLE_PWR_AWAKE,
    BLE_PWR_HOT_START,
    BLE_PWR_SLEEP,
    BLE_PWR_WAIT,
    BLE_PWR_TMP_OFF,
    BLE_PWR_OFF,
  };
  BlePwrState _ble_pwr_state = BLE_PWR_BOOT;
  uint32_t    _ble_pwr_state_until = 0;   // millis() Ziel fuer Phase-Ende
  bool        _ble_was_connected = false; // edge-detection
  // Disconnect-Recency: 10min nach BT-Disconnect verkuerzter Cycle (30/30
  // statt 60/30), weil Reconnect-Wahrscheinlichkeit direkt nach App-Use
  // hoch ist (User-Wunsch 2026-06-11). 0 = inaktiv / abgelaufen.
  uint32_t    _ble_disconnect_at = 0;
  // Ring-Buffer letzte 8 State-Transitions. Wird in 'bluetooth'-Status
  // mit ausgegeben damit User-Diagnose moeglich ist ohne live-BLE-
  // Verbindung haben zu muessen (User-Hinweis 2026-06-11: 'BLE-Debug
  // ueber BLE ist absurd wenn man gerade BLE abschaltet').
  // Bis Wunschliste 51 (USB-BREAK-CLI) ist das die einzige Diagnose-
  // Quelle ueber langere Zeitraueme.
  struct BleLogEntry {
    uint32_t   t_ms;        // millis() bei Transition
    BlePwrState from_state;
    BlePwrState to_state;
    char       reason[12];  // kurzer Reason-Tag
  };
  static const uint8_t BLE_LOG_SIZE = 8;
  BleLogEntry _ble_log[BLE_LOG_SIZE] = {};
  uint8_t     _ble_log_head = 0;
  uint8_t     _ble_log_count = 0;
  void logBleTransition(BlePwrState to_state, const char* reason);
  // DL9SAU 2026-06-16: 16 Stellen hatten 'logBleTransition(X, reason);
  // _ble_pwr_state = X;'-Paar inline. Helper macht beides atomic.
  inline void bleSetState(BlePwrState to_state, const char* reason) {
    logBleTransition(to_state, reason);
    _ble_pwr_state = to_state;
  }
  // Deferred-disable analog _pending_reboot_at: bei 'bluetooth off'-CLI
  // bekommt die App noch 5s Zeit den OK-Frame ueber BLE zu empfangen
  // bevor wir den Chip abschalten. Plus periodisch tickle nach unten
  // damit App-Sync nicht verpasst wird.
  uint32_t    _pending_ble_off_at = 0;
  uint32_t    _pending_ble_off_next_tickle_at = 0;
  void manageBlePower();
  void setBleEnabled(bool en);
  // Wake-on-LoRa Hook: bei eingehender DM oder admin-cmd in den
  // Cycle-Phasen (SLEEP/WAIT) erweckt BLE fuer einen HOT_START-
  // Window (5 min). Wirkt nicht auf AWAKE/BOOT (eh an), nicht
  // auf TMP_OFF/OFF (User-Wunsch zu schlafen wird respektiert).
  void bleWakeOnLora(const char* reason);
  // Hardware-Button Toggle Hook -- UITask-Menue ruft das beim
  // Long-Press auf der Bluetooth-Seite. Nicht persistent.
  void bleManualToggleFromMenu();

  // Wunschliste 52 (2026-06-10): Remote-Admin Client-Pfad.
  // pending_admin_pubkey[4]: Match-Suffix fuer onContactResponse, analog
  // pending_login/status. Wenn != 0: kommende Response wird als Admin-CMD
  // Antwort behandelt und in $companion gepushed.
  uint8_t  pending_admin_pubkey[4] = {0,0,0,0};
  uint32_t pending_admin_tag = 0;
  bool     pending_admin_login = false;   // login vs cmd unterscheiden

  // Wunschliste 52 (2026-06-10): Remote-Admin Capture.
  // Wenn _admin_capture_active gesetzt, redirected pushCompanionMessage()
  // in _admin_reply_buf statt in die App-Push-Queue. So koennen wir den
  // bestehenden handleCompanionCommand-Pfad fuer REQ_TYPE_ADMIN_CMD
  // wiederverwenden. _admin_reply_buf zeigt auf einen vom Caller
  // gestellten Reply-Buffer (typisch 155 byte).
  char*  _admin_reply_buf = NULL;
  size_t _admin_reply_max = 0;
  size_t _admin_reply_used = 0;
  bool   _admin_capture_active = false;
  bool   _admin_capture_truncated = false;
  // Guest-Mode: read-only Befehle erlaubt. Lese-Whitelist als Liste
  // von Prefix-Tokens; wird beim REQ_TYPE_ADMIN_CMD-Handler konsultiert.
  bool isAdminCmdAllowedForGuest(const char* cmd) const;
  // Pusht eine Trace-Message in den Companion-Channel — aber nur wenn das
  // entsprechende Flag in _trace_flags gesetzt ist (= Kategorie-Level >= 1
  // und nicht pausiert). No-op sonst. Meldungs-Level implizit 1 ("wichtig").
  void traceCompanion(uint32_t flag, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
  // Kategorie-KLASSE (2-Bit-Slot in _prefs.trace_levels): 1=wichtig, 2=verbose,
  // 3=debug, 0=aus. flag = Einzel-Bit (TRACE_*); Slot-Index = __builtin_ctz(flag).
  uint8_t traceLevelOf(uint32_t flag) const;   // 0..3, Klasse (ignoriert pause + globalen Schnitt)
  void    setTraceLevel(uint32_t flag, uint8_t lvl);  // setzt Klasse + recomputeTraceFlags()
  // Globaler View-Level (Schnitt) 1..3: es werden nur Kategorien mit Klasse <= N
  // aktiv. Gespeichert in trace_levels Bits 62-63 (0=ungesetzt -> Default 3).
  uint8_t traceViewLevel() const;
  void    setTraceViewLevel(uint8_t n);
  // Rechnet _trace_flags (aktive Bitmaske: 0<Klasse<=View-Level, nicht pausiert)
  // + _prefs.trace_flags_persistent (Low-16-Spiegel) aus _prefs.trace_levels.
  void    recomputeTraceFlags();
  // Baut einen Status-Suffix für TRACE_GPS Messages: pos, alt, valid-Flags,
  // RTC-Zeit. Wird an [gps] wake/sleep/first-fix angehaengt damit der User
  // GPS-Zustandsuebergaenge mit Position/Hoehe/Zeit korrelieren kann.
  void appendGpsTraceStatus(char* out, size_t out_size);
  // Duty-Cycle Helper. updateDutyWindow muss in loop() laufen damit
  // get/Reached-Funktionen aktuelle Werte liefern.
  void updateDutyWindow();
  unsigned long getTxAirLastHour() const;
  unsigned long getDutySoftLimitMs() const;
  unsigned long getDutyHardLimitMs() const;
  bool dutySoftReached() const;
  bool dutyHardReached() const;

  // client-repeater + periodic advert helpers
  void applyRadioPolicy();   // calls radio_set_params() with freq/CR overrides
  void markHeardDirect(uint8_t hash);   // call only for zero-hop adverts
  bool isLocallyHeard(uint8_t hash) const;
  // Wunschliste 46 Phase 1 (Reise 2026-06-09): Filter-System.
  // pattern + flags (bit0=anchor start, bit1=anchor end, both=exact,
  // none=substring). Returnt true wenn s das Pattern matched.
  static bool filterPatternMatch(const NodePrefs::FilterEntry& e, const char* s);
  // 2026-07-11: rohes Text-Pattern-Match (^/$ inline geparst) fuer CLI-Suchen
  // in contacts/neighbors/path show. Selbe Engine wie filterPatternMatch.
  static bool textMatchesPattern(const char* pattern, const char* text);
  // Wunschliste 46 Phase 4 (2026-06-11): Pubkey-Prefix-Match.
  // Returns true wenn key gegen einen der ersten cnt Eintraege matched.
  static bool pubkeyFilterMatch(const uint8_t* key,
                                const NodePrefs::FilterPubkeyEntry* arr,
                                uint8_t cnt);
  // Beide Helper: true wenn DROP greift (= mind. ein Filter matched).
  // Wunschliste 46 Phase 2: channel_idx = -1 fuer DM (Skopus ignoriert),
  // sonst Index in channels[].
  bool filterSenderDropMatch(const char* sender_name, int channel_idx = -1) const;
  bool filterTextDropMatch(const char* text, int channel_idx = -1) const;
  // Wunschliste 46 Phase 5: scope-Filter. scope_name = NULL fuer unscoped.
  // for_repeat: true im Repeat-Pfad, false im Display-Pfad.
  bool filterScopeMatch(const char* scope_name, int channel_idx,
                        bool for_repeat) const;
  bool getEffectiveLatLon(double& lat, double& lon) const;
  // Profile-aware Bbox-Quelle (Reise-Fix 2026-06-08):
  //   profile=normal    -> nur fixed location (sensors.node_lat/lon)
  //   profile=defensive -> wie getEffectiveLatLon (GPS-Live + fixed-fallback)
  bool getRepeaterBboxLatLon(double& lat, double& lon) const;
  // Helper: Bbox-Membership neu evaluieren via getRepeaterBboxLatLon.
  // Wenn keine Quelle verfuegbar (profile=defensive + gps an + nie Fix),
  // wird die Bbox komplett geleert. Wird gerufen nach Profile-Wechsel,
  // gps on/off-Toggle, fixed-location-Aenderung.
  void reevaluateRepeaterBbox();
  bool chooseGeoFallbackScope(TransportKey& out_key) const;
  bool chooseNightFloodScope(TransportKey& out_key) const;
  // "Default-oder-Geo" Helper fuer Sende-Pfade. Wenn
  // _prefs.scope_advert_auto=prefer UND die ortliche Geo-Region eine andere
  // ist als das Default-Scope, gewinnt Geo. Sonst Default. Returns
  // true wenn out_key non-null geschrieben wurde.
  bool resolveDefaultOrGeo(TransportKey& out_key) const;
  // Scope-Helper.
  // normalize: strippt fuehrendes '#', lowercase, lehnt leer/"##"/zu-lang ab.
  bool normalizeScopeName(const char* in, char* out, size_t out_size) const;
  void computeScopeHash(const char* name, uint8_t out_hash[4]) const;
  // Repeat-Policy: liefert true wenn das Paket geforwarded werden darf
  // (entweder Mode=ALL oder Mode=ALLOWLIST und transport_code matcht
  // einen Eintrag mit IN_REPEAT_LIST-Flag). Aufrufer hat bereits
  // hasTransportCodes()-Check gemacht.
  bool scopeAllowedForRepeat(const mesh::Packet* packet) const;

  // Updated _buildin_in_bbox[] und _extras_in_bbox[] basierend auf der
  // angegebenen Position. Wird vom Motion-Tracking gerufen wenn der
  // 370m-Motion-Anker frisch gesetzt wird.
  void evaluateScopeBboxes(double lat, double lon);

  // ----- Scope-Architektur-Pivot Helpers (Wunschliste 11 Schritt 4) -----
  // Cross-Storage Lookup-Identifier. SCOPE_BUILDIN: idx ist Position in
  // dl9sau_regions-Tabelle. SCOPE_EXTRAS: idx ist Position in
  // _prefs.scope_extras[]. SCOPE_NONE: nichts gefunden.
  enum ScopeStorage { SCOPE_NONE = 0, SCOPE_BUILDIN, SCOPE_EXTRAS };
  struct ScopeRef {
    ScopeStorage storage;
    int          idx;
  };

  // Sucht den Eintrag in beiden Storages. Build-in zuerst.
  ScopeRef findScopeByName(const char* name) const;

  // Effektiver Status-Byte fuer einen Build-in-Eintrag (idx = Position
  // in dl9sau_regions-Tabelle). Sucht in scope_buildin_status[] nach
  // dem Name-Hash; wenn nicht gefunden, returns 0 (Default).
  uint8_t getBuildinStatus(int buildin_idx) const;

  // Setzt Status fuer einen Build-in-Eintrag. status==0 entfernt einen
  // ggf. vorhandenen Sparse-Slot (Storage-Defragmentierung). status!=0
  // legt einen neuen Slot an wenn keiner existiert. Returns false wenn
  // alle Slots belegt sind (max SCOPE_BUILDIN_STATUS_MAX).
  bool    setBuildinStatus(int buildin_idx, uint8_t status);

  // Effektiver Status fuer einen ScopeRef. Bei BUILDIN: via
  // getBuildinStatus(). Bei EXTRAS: aus scope_extras[idx].flags
  // (gleiches Bit-Layout wie Build-in-Status; alte SCOPE_FLAG_*-Bits
  // sind in dieser Phase noch in extras.flags drin und werden in
  // Schritt 5+ migriert).
  uint8_t getScopeStatus(const ScopeRef& ref) const;
  bool    setScopeStatus(const ScopeRef& ref, uint8_t status);

  // TransportKey fuer einen ScopeRef. BUILDIN: aus _buildin_keys[]-
  // Cache. EXTRAS: aus scope_extras[idx].key. Returns NULL bei
  // SCOPE_NONE oder out-of-range.
  const uint8_t* getScopeKey(const ScopeRef& ref) const;

  // Bbox fuer einen ScopeRef. Returns true wenn Bbox vorhanden ist.
  bool getScopeBbox(const ScopeRef& ref,
                    double* lat_min, double* lat_max,
                    double* lon_min, double* lon_max) const;
  void scheduleNextNightFlood();
  void doPeriodicZeroHopAdvert();
  void doNightFloodAdvert();
  void updateMotionTracking();
  unsigned long computeNextAdvertIntervalMs() const;
  void manageGpsPower();
  // DL9SAU Wunschliste 83: RX-Sleep-Manager. Loop-tick. Bestimmt aus
  // Pref + Repeater-Modus + Boot/Wake-Windows ob RX an oder im Sleep
  // sein soll, und ruft entsprechend radio_driver.setRxSuspended().
  void manageRxPower();
  // Wunschliste 89/90/91: Battery-Schutz + USB-Loss-Timer Tick.
  void manageBatteryAndUsb();
  // DL9SAU 2026-07-12: geklemmte Boot/Recovery-Schwelle (mV) fuer LPCOMP-Wake.
  uint16_t getEffectiveBootMinMv() const;

  // Dispatcher hooks: per-packet TX-param override (CR5 / reduced power) used
  // for repeated packets and our automatic adverts. Eigene Direct-Messages
  // bleiben mit konfigurierter CR und voller TX-Power.
  void applyPacketTxOverrides(const mesh::Packet* packet) override;
  void restorePacketTxDefaults() override;

  // helpers, short-cuts
  void saveChannels() { _store->saveChannels(this); }
  void saveContacts();

  DataStore* _store;
  NodePrefs _prefs;
  uint32_t pending_login;
  uint32_t pending_status;
  uint32_t pending_telemetry, pending_discovery;   // pending _TELEMETRY_REQ
  uint32_t pending_req;   // pending _BINARY_REQ
  // 2026-07-06: CLI 'ping' State. 0 = kein pending.
  uint32_t _cli_ping_tag = 0;
  unsigned long _cli_ping_started_ms = 0;
  unsigned long _cli_ping_expiry_ms  = 0;
  uint8_t  _cli_ping_target_pubkey[32] = {0};
  char     _cli_ping_target_name[32]   = {0};
  uint8_t  _cli_ping_target_hex_len = 3;  // Bytes fuer pkx-Ausgabe (variabel bei raw-hex)
  uint8_t  _cli_ping_hash_size = 1;       // hop-hash-Breite; Folge-Pings (-c N) nutzen sie
  // 2026-07-07: Ping -c N -i M Stats.
  uint16_t _cli_ping_count_target = 0;      // 0 = single, >0 = -c N mode
  uint16_t _cli_ping_count_done = 0;
  uint16_t _cli_ping_count_lost = 0;
  unsigned long _cli_ping_interval_ms = 0;
  unsigned long _cli_ping_next_at_ms = 0;
  // Running stats: sum, sum_sq, min, max je field.
  double   _cli_ping_rtt_sum = 0, _cli_ping_rtt_sq = 0;
  double   _cli_ping_rtt_min = 0, _cli_ping_rtt_max = 0;
  double   _cli_ping_hin_sum = 0, _cli_ping_hin_sq = 0;
  double   _cli_ping_hin_min = 0, _cli_ping_hin_max = 0;
  double   _cli_ping_rueck_sum = 0, _cli_ping_rueck_sq = 0;
  double   _cli_ping_rueck_min = 0, _cli_ping_rueck_max = 0;
  double   _cli_ping_rssi_sum = 0, _cli_ping_rssi_sq = 0;
  double   _cli_ping_rssi_min = 0, _cli_ping_rssi_max = 0;
  // 2026-07-07: CLI Last-Cmd Recall via '!!' (unix shell style).
  char _cli_last_cmd[128] = {0};
  // 2026-07-07: Einheitliche Regions-Result-Ausgabe (4 Sites konsolidiert).
  void pushRegionsResult(const uint8_t* pubkey32, const char* name,
                         const uint8_t* csv, size_t csv_len);
  // 2026-07-07: TRACE-Ping an gespeichertes Ziel (fuer -c N Auto-Repeat).
  bool sendCliPingToStoredTarget();
  void resetCliPingStats();
  void updateCliPingStats(double rtt_s, double hin, double rueck, int rssi);
  void printCliPingStats();
  // 2026-07-07: CLI 'tracepath' State.
  uint32_t _cli_trace_tag = 0;
  unsigned long _cli_trace_started_ms = 0;
  unsigned long _cli_trace_expiry_ms  = 0;
  uint8_t  _cli_trace_target_pubkey[32] = {0};
  char     _cli_trace_target_name[32]   = {0};
  uint8_t  _cli_trace_hash_size = 1;
  uint8_t  _cli_trace_forward_hops = 0;   // Anzahl Hops im Hin-Weg (ohne Ziel)
  uint8_t  _cli_trace_target_hex_len = 3; // Bytes fuer pkx-Ausgabe (variabel bei raw-hex)
  // 2026-07-07: App-Piggyback fuer Ping/Trace (analog Discover-Piggyback).
  uint32_t _app_ping_tag = 0;
  unsigned long _app_ping_started_ms = 0;
  uint8_t  _app_ping_target_pubkey[32] = {0};
  uint32_t _app_trace_tag = 0;
  unsigned long _app_trace_started_ms = 0;
  uint8_t  _app_trace_target_pubkey[32] = {0};
  uint8_t  _app_trace_hex_len = 3;  // Bytes fuer pkx-Ausgabe (= app_hash_size)
  // 2026-07-08: Resend-Coalescing (aus stash@{3} reaktiviert 2026-07-09 zur
  // Crash-Reproduktion mit scharfem Crash-Faenger). Channel: gleicher Text im
  // gleichen Channel innerhalb Fenster -> Original-Timestamp -> identischer
  // Hash -> Empfaenger dedupt. DM: gleicher Timestamp + attempt++ (anderer
  // Hash propagiert, Empfaenger-App dedupt nach (Sender, Timestamp)).
  struct ResendCoalesce {
    uint32_t      key_hash;       // Channel: fnv1a32(secret); DM: fnv1a32(pubkey6)
    uint32_t      text_hash;      // fnv1a32 des final gesendeten Texts
    uint32_t      orig_timestamp; // Timestamp des ERSTEN Sends
    unsigned long anchor_millis;  // millis() des letzten Sends (erneuert)
    uint8_t       max_attempt;    // hoechster genutzter attempt (nur DM)
    bool          is_dm;
    bool          used;
    // 2026-07-10 Phase 2: Confirm-Handle. Channel = calcShortHash des gesendeten
    // Pakets (Echo-Match in filterRecvFloodPacket); DM = expected_ack-Code
    // (Match in processAck). 0 = noch keiner gesetzt.
    uint32_t      confirm_key;
  };
  static const int RESEND_COALESCE_N = 16;
  ResendCoalesce _resend_coalesce[RESEND_COALESCE_N] = {};
  // Gibt den zu nutzenden Timestamp zurueck. Bei DM (attempt != nullptr)
  // wird *attempt bei einem Coalesce-Treffer hochgezaehlt.
  uint32_t resendCoalesce(bool is_dm, uint32_t key_hash, uint32_t text_hash,
                          uint32_t app_timestamp, uint8_t* attempt);
  // Phase 2: Confirm-Handle nachtragen (nach dem Send, wenn pkt-hash/ack da).
  void noteCoalesceConfirm(bool is_dm, uint32_t key_hash, uint32_t text_hash,
                           uint32_t confirm_key);
  // Phase 2: Eintrag loeschen wenn Echo (Channel) bzw. ACK (DM) bestaetigt --
  // damit ein spaeterer identischer Send NICHT faelschlich rueckdatiert wird.
  bool dropCoalesceByConfirm(bool is_dm, uint32_t confirm_key);
  BaseSerialInterface *_serial;
  AbstractUITask* _ui;

  ContactsIterator _iter;
  uint32_t _iter_filter_since;
  uint32_t _most_recent_lastmod;
  uint32_t _active_ble_pin;
  bool _iter_started;
  bool _cli_rescue;
  bool send_unscoped;   // force un-scoped flood (instead of using send_scope)
  char cli_command[80];
  uint8_t app_target_ver;
  uint8_t *sign_data;
  uint32_t sign_data_len;
  unsigned long dirty_contacts_expiry;

  TransportKey send_scope;

  uint8_t cmd_frame[MAX_FRAME_SIZE + 1];
  uint8_t out_frame[MAX_FRAME_SIZE + 1];
  CayenneLPP telemetry;

  // Offline-Queue (Messages die ankommen waehrend App nicht connected ist).
  // 4 typisierte Buckets statt einer einzigen 16er-Queue (Test-Bericht
  // 2026-05-29 Bug: bei Voll mit DMs wurde die neue DM gedroppt statt die
  // aelteste DM zu verdraengen). Jetzt: per-Bucket FIFO, kein cross-bucket
  // Drop. Global monoton steigende seq_no pro Frame -> chronologische
  // Reihenfolge beim Pop wird ueber alle Buckets hinweg erhalten.
  struct Frame {
    uint8_t  len;
    uint32_t seq_no;            // 0 = leerer Slot, sonst global monoton steigend
    uint8_t  buf[MAX_FRAME_SIZE];

    bool isChannelMsg() const;
  };
  enum MsgBucket : uint8_t {
    BUCKET_PUBLIC    = 0,       // PSK matches PUBLIC_GROUP_PSK
    BUCKET_HASHTAG   = 1,       // channel.name[0] == '#'
    BUCKET_PRIVATE   = 2,       // alles andere (User-Channels) ohne $companion
    BUCKET_DM        = 3,       // RESP_CODE_CONTACT_MSG_RECV / _V3
    BUCKET_COMPANION = 4,       // $companion-Channel (Trace, CLI-Output)
    BUCKET_COUNT     = 5
  };
  // Bucket-Kapazitaeten (Array-Groessen). Phase C macht das Slot-Limit
  // pro Bucket per NodePrefs+CLI konfigurierbar; die Arrays sind hier
  // auf MAX vorallokiert. Slot mit Index >= runtime-limit bleibt
  // ungenutzt. $companion eigener Bucket: Trace/CLI-Output wuerde sonst
  // den privaten Bucket ueberfluten und User-Channel-Nachrichten
  // verdraengen.
  // Caps angepasst nach Test-Bericht 2026-05-30: User-Wunsch HASHTAG/
  // PRIVATE auf 24 anheben (mehr Puffer fuer Gruppen-Chats), DM von
  // 32 auf 24 reduziert (DMs sind seltener). PUBLIC bewusst klein (8)
  // -- nur ein well-known PSK-Channel, geringer Traffic.
  static constexpr int BUCKET_CAP_PUBLIC    = 8;
  static constexpr int BUCKET_CAP_HASHTAG   = 24;
  static constexpr int BUCKET_CAP_PRIVATE   = 24;
  static constexpr int BUCKET_CAP_DM        = 24;
  static constexpr int BUCKET_CAP_COMPANION = 16;
  // Defaults wenn NodePrefs.msg_store_limit[b] == 0.
  static constexpr int BUCKET_DEFAULT_PUBLIC    = 8;
  static constexpr int BUCKET_DEFAULT_HASHTAG   = 16;
  static constexpr int BUCKET_DEFAULT_PRIVATE   = 16;
  static constexpr int BUCKET_DEFAULT_DM        = 16;
  static constexpr int BUCKET_DEFAULT_COMPANION = 16;
  Frame    bucket_public   [BUCKET_CAP_PUBLIC];
  Frame    bucket_hashtag  [BUCKET_CAP_HASHTAG];
  Frame    bucket_private  [BUCKET_CAP_PRIVATE];
  Frame    bucket_dm       [BUCKET_CAP_DM];
  Frame    bucket_companion[BUCKET_CAP_COMPANION];
  // Runtime-Limit pro Bucket (resolved aus NodePrefs.msg_store_limit + Default).
  int      getBucketLimit(MsgBucket b) const;
  // Flash-Flag pro Bucket (lookup in NodePrefs.msg_store_flash bit-field).
  bool     getBucketFlash(MsgBucket b) const;
  // Bucket vollstaendig auf Flash schreiben (Datei: /msgs/b<n>.dat). Wird
  // nach jedem add gerufen wenn der Flash-Flag fuer diesen Bucket gesetzt
  // ist. Format: per-Eintrag { seq_no:4 LE, len:1, frame:len }. Nur Slots
  // mit seq_no != 0 werden geschrieben.
  void     saveBucketToFlash(MsgBucket b);
  // Pfad fuer die persistente Datei eines Buckets.
  const char* msgBucketPath(MsgBucket b) const;
  // Effektiver Lokal-TZ-Offset zu UTC fuer einen gegebenen UTC-Zeitpunkt.
  // = LOCAL_TZ_OFFSET_SECS + (EU-DST aktiv ? 3600 : 0).
  // Sommerzeit-Detektion: last-Sunday-March 01:00 UTC bis last-Sunday-
  // October 01:00 UTC. CEST = UTC+2 in Sommer, CET = UTC+1 in Winter.
  int32_t localTzOffsetSecs(uint32_t utc) const;
  // Alle Buckets mit aktivem Flash-Flag von Flash einlesen. Wird einmalig
  // beim Boot nach setupCompanionChannel() gerufen.
  void     loadBucketsFromFlash();
  // Bucket leeren -- RAM-Slots auf 0 setzen + ggf. Flash-Datei loeschen.
  // Wird von 'messages clear' CLI verwendet.
  void     clearBucket(MsgBucket b);
  // Helper: User-facing Bucket-Name fuer CLI ('public', 'hashtag', etc.).
  const char* bucketName(MsgBucket b) const;
  // Helper: User-Eingabe ('public'/'pub', 'dm', ...) -> Bucket-Enum.
  // Returns -1 bei unbekanntem Namen, -2 bei mehrdeutigem Prefix.
  int      bucketByName(const char* s) const;
  uint32_t _msg_seq_next;       // naechste seq_no fuer eingehenden Frame
  // Helper: tableau-pointer + capacity je Bucket.
  void     getBucket(MsgBucket b, Frame*& out_arr, int& out_cap);
  int      offlineQueueTotal() const;     // Summe aller Buckets (UI-Indikator)
  MsgBucket classifyFrame(const uint8_t* frame, int len);

  struct AckTableEntry {
    unsigned long msg_sent;
    uint32_t ack;
    ContactInfo* contact;
  };
  #define EXPECTED_ACK_TABLE_SIZE 8
  AckTableEntry expected_ack_table[EXPECTED_ACK_TABLE_SIZE]; // circular table
  int next_ack_idx;

  #define ADVERT_PATH_TABLE_SIZE   16
  AdvertPath advert_paths[ADVERT_PATH_TABLE_SIZE]; // circular table

  HeardEntry heard_list[CR_HEARD_TABLE_SIZE];
  uint8_t    heard_next_idx;    // FIFO eviction index

  unsigned long next_periodic_advert_at;     // millis()
  uint32_t      next_night_flood_unix;       // RTC unix; 0 means not scheduled

  // motion tracking for dynamic advert interval
  double        _pos_anchor_lat;
  double        _pos_anchor_lon;
  unsigned long _pos_anchor_millis;          // 0 means no anchor yet
  bool          _is_moving;                  // result of last window evaluation
  double        _boot_lat;                   // persisted position at boot, before GPS overwrites it
  double        _boot_lon;
  bool          _boot_pos_known;             // true if loadPrefs gave us a non-zero position
  // DL9SAU 2026-06-17 (Wunschliste 80): "Heimat"-Position fuer ADVERT_LOC_PREFS.
  // Initialisiert in begin() aus sensors.node_lat/lon (= geladener Pref-Wert);
  // bei 'set lat/lon' und CMD_SET_RADIO_PARAMS aktualisiert. Live-GPS-Fix
  // ueberschreibt _adv_prefs_* nicht -- so kann GPS fuer Zeit-Sync laufen,
  // waehrend der Advert eine stabile fixe Position traegt.
  double        _adv_prefs_lat;
  double        _adv_prefs_lon;
  bool          _gps_had_fix_ever;           // true once GPS reported a valid fix this session
  unsigned long _gps_woke_at_millis;         // when we last (re-)enabled GPS; 0 = currently off (or never managed)
  unsigned long _gps_off_at_millis;          // when we last switched GPS off; 0 = currently on (or never managed)
  bool          _gps_fix_seen_this_wake;     // a position fix arrived since last wakeup — OK to sleep again
  // DL9SAU 2026-06-17: Zeitstempel des letzten Live-GPS-Fix.
  //  0 = noch nie gesehen. millis()-basiert -> bei 49-Tage-Wrap
  //  ggf. Sentinel oder uint64 noetig, fuer Diagnose reicht aber 32-bit.
  unsigned long _gps_last_fix_at_millis;
  bool          _gps_user_override_until_advert;  // user toggled GPS on via app — keep on until next advert
  uint32_t      _last_millis_seen;           // for wrap detection of millis()
  uint32_t      _millis_wraps;               // how many times millis() has wrapped since boot

  // DL9SAU 2026-06-18 (Wunschliste 82): TX-Block fuer Wartung.
  // _tx_blocked     -- harter Schalter; alle sendPacket-Aufrufe werden
  //                    direkt verworfen (Packet freigegeben statt queued).
  //                    NICHT persistent -- nach Reboot wieder enabled,
  //                    sonst koennte der User sich aussperren.
  // _tx_blocked_until_millis -- 0 = kein Timer (hartes disable); sonst
  //                    Auto-Reenable wenn millis() den Wert erreicht.
  //                    'tx suspend <N>' setzt Timer; 'tx disable' = 0.
  bool          _tx_blocked;
  unsigned long _tx_blocked_until_millis;

  // DL9SAU 2026-06-18 (Wunschliste 83): RX-Sleep-State (RAM, abgeleitet
  // aus _prefs.rx_disabled + Repeater-Modus + Boot-Window + Wake-Window).
  // _rx_wake_until_millis  -- 0 = kein post-TX Window aktiv; sonst Millis
  //                          bis zu denen RX nach TX wach bleibt (5min).
  // _rx_currently_suspended -- aktueller Hardware-Status, damit
  //                          manageRxPower() keine Redundant-Calls macht.
  unsigned long _rx_wake_until_millis;
  bool          _rx_currently_suspended;

  // DL9SAU 2026-06-18 (Wunschliste 89/90/91): Battery + USB-Power State.
  // _usb_lost_at_millis: 0 wenn USB an, sonst millis() der ersten USB-
  //                      off-Erkennung. Hysterese 10s gegen Glitches.
  // _batt_last_sample_millis: 10-min Sample-Anker. 0 = erste Messung pending.
  // _batt_low_burst_until_millis: wenn != 0, im 10s-Sampling-Modus bis
  //                      zu diesem millis. Bei drei Samples unter Schwelle
  //                      -> shutdown via board.powerOff().
  // _batt_low_consecutive: Counter im Burst-Mode (3 = shutdown).
  // _batt_last_mv / _batt_last_pct: letzte ermittelte Werte fuer Status.
  unsigned long _usb_lost_at_millis;
  // _usb_ever_seen (Wunschliste 90 Bug-Fix 2026-06-20): runtime-only
  // Edge-Detection. usb_loss_shutdown_min schaltet sich erst scharf
  // wenn USB einmal im laufenden Betrieb gesteckt war. Verhindert
  // sofort-shutdown nach Akku-only-Boot (Button-Press nach shutdown).
  bool          _usb_ever_seen;
  unsigned long _batt_last_sample_millis;
  unsigned long _batt_low_burst_until_millis;
  uint8_t       _batt_low_consecutive;
  uint16_t      _batt_last_mv;
  uint8_t       _batt_last_pct;

  // Pre-computed TransportKeys for every region in dl9sau_regions[]. Built
  // once in begin() via SHA-256 over "#name". Lookup at packet receive time
  // costs one HMAC per entry (~10 us on ESP32-S3) — at ~200 entries that's
  // ~2 ms per channel-message lookup, acceptable.
#if DL9SAU_REGIONS_AVAILABLE
  TransportKey  _region_keys[DL9SAU_REGION_COUNT];
  bool          _region_keys_ready;
#endif

  // Cache of the last geo-scope recommendation we logged, so we don't spam
  // the debug log on every motion-tracking tick.
  char          _last_geo_reco[200];
  // Hysterese-Anker: Position bei letzter Berechnung. Solange wir uns weniger
  // als CR_GEO_RECO_REEVAL_DIST_M von dort entfernt haben, sparen wir uns die
  // Neu-Auswertung. Bei kontinuierlicher Bewegung (z.B. 40 km/h Auto) löst der
  // Anker alle ~15-20 min eine Neu-Berechnung aus; bei stillstand oder
  // Grenz-Pendeln (zwischen zwei Bundesländern parken) bleibt alles ruhig.
  double        _geo_reco_anchor_lat;
  double        _geo_reco_anchor_lon;

  // Index des lokalen "companion"-Channels in channels[]. Nachrichten an
  // diesen Channel werden NICHT über LoRa gesendet, sondern als Befehle an
  // die Firmware geparst. Output (z.B. Geo-Scope-Empfehlung) erscheint als
  // synthetic incoming-channel-message — App sieht es als normalen Chat.
  // 0xFF = nicht initialisiert / kein freier Slot.
  uint8_t       _companion_channel_idx;

  // DL9SAU Wunschliste 88 (2026-06-20) Phase 1: at / cron CLI-Skelett.
  // RAM-only. Persistenz (cron auf Flash) ist Phase 2 nach Reise.
  //
  // AtEntry: einmaliger Job, RAM-only, kein Reboot-Survival.
  //   - flags bit 0 = relative (millis-basiert), sonst absolute (RTC-basiert)
  static const uint8_t AT_MAX_ENTRIES   = 4;
  static const uint8_t CRON_MAX_ENTRIES = 8;
  static const uint8_t CRON_CMD_LEN     = 48;
  struct AtEntry {
    uint8_t flags;            // bit 0 = relative
    uint8_t in_use;           // 0 = slot frei
    union {
      uint32_t trigger_millis;  // bei relative: millis()-Zielwert
      struct {
        uint16_t hour_min;      // h*60+m
        uint8_t  target_day;    // 0=heute, 1=morgen
      } abs;                    // bei absolute: RTC-basiert
    } when;
    char cmd[CRON_CMD_LEN];
  };
  AtEntry _at_entries[AT_MAX_ENTRIES] = {};

  // CronEntry: wiederholt, RAM-only Phase 1 (Phase 2: persistent).
  //   - flags bit 0 = enabled
  //   - flags bit 1 = weekday-based (nutzt weekday_mask + hour_min)
  //   - flags bit 2 = interval-based (nutzt interval_secs)
  struct CronEntry {
    uint8_t  flags;
    uint8_t  in_use;          // 0 = slot frei
    uint8_t  weekday_mask;    // bit 0=Mo .. bit 6=So
    uint16_t hour_min;        // h*60+m fuer weekday-based
    uint32_t interval_secs;   // fuer interval-based
    uint32_t last_run_secs;   // Unix
    char     cmd[CRON_CMD_LEN];
  };
  CronEntry _cron_entries[CRON_MAX_ENTRIES] = {};
  // DL9SAU Wunschliste 88 Phase 2c (2026-06-20): rebootfest pausieren.
  // 'cron suspend' / 'cron resume'. Eintraege bleiben erhalten, nur
  // manageCronAt skippt die Trigger. at-Jobs bleiben aktiv.
  bool _cron_suspended = false;

  // Letzter Minute-Tick fuer manageCronAt-Polling.
  uint32_t _cron_at_last_check_ms = 0;
  // Tag-Origin fuer ScheduledCmd-Output. Gesetzt waehrend execute,
  // gelesen von pushCompanionMessage fuer Serial-Fallback-Prefix.
  // nullptr = nicht scheduled, normaler Pfad.
  const char* _scheduled_origin_tag = nullptr;
  // Boot-Grace: cron-Jobs erst nach 10min uptime (siehe Wunschliste 88).
  // At-Jobs duerfen sofort, sind explizite User-Setzung.
  static const uint32_t CRON_BOOT_GRACE_MS = 10UL * 60 * 1000;
  // DL9SAU 2026-06-20: GPREGRET-Shutdown-Sentinel. NRF52-only.
  // Setzt 0xAB in GPREGRET vor powerOff -- Boot-Check in main.cpp
  // sieht das und blockt Phantom-Wakes (BOR durch USB-Pull-Glitch
  // hat keine RESETREAS-Bits).
  void setShutdownSentinel();
  void manageCronAt();
  void executeScheduledCmd(const char* cmd, const char* origin_tag);
  bool parseRelativeMinutes(const char* s, uint32_t* out_minutes);
  bool parseAbsoluteHourMin(const char* s, uint16_t* out_hour_min);
  bool parseIntervalSecs(const char* s, uint32_t* out_secs);
  bool parseWeekdayMask(const char* s, uint8_t* out_mask);
  int  findFreeAtSlot();
  int  findFreeCronSlot();
  void emitAtList();
  void emitCronList();
  // DL9SAU Wunschliste 88 Phase 2 (2026-06-20): Cron-Persistenz.
  // File "/cron.dat": magic-byte (0xC0) + version (0x01) + count +
  // N x CronEntry (ohne in_use/last_run_secs -- last_run startet
  // beim Boot wieder bei 0). At-Jobs sind RAM-only by design.
  void saveCronToFile();
  void loadCronFromFile();
  // Abgeleitete Aktiv-Bitmaske (Bit gesetzt wenn Kategorie-Level>0 UND nicht
  // pausiert). RAM-only, via recomputeTraceFlags() aus _prefs.trace_levels
  // berechnet -- beim Boot (Restore der persistenten Level) und bei jeder
  // Level-/Pause-Aenderung. uint32: Platz fuer bis zu 32 Kategorien (18 in
  // Benutzung, TRACE_DELIVERY/TRACE_GEO > 0xFFFF). Alle bestehenden
  // '_trace_flags & FLAG'-Checks bedeuten damit weiterhin "Level >= 1".
  uint32_t      _trace_flags;
  // DL9SAU 2026-07-13: 'trace off' pausiert ALLE Traces ohne die gespeicherte
  // Level-Auswahl (trace_levels) zu verlieren. RAM-only, Default false (Boot =
  // nicht pausiert -> gespeicherte Level sind aktiv).
  bool          _trace_paused;
  // Modus fuer TRACE_HEARD: false (Default) = nur NEUE Direct-Nodes loggen
  // (HeardList-Insertion); true = jeder Direct-Empfang loggen auch wenn
  // der Node schon bekannt ist. CLI: 'trace heard on new|all'.
  // Runtime-only (kein NodePrefs-Feld) -- nach Reboot wieder false.
  bool          _trace_heard_all;

  // Wunschliste 25 (Floodless unscoped channels, User-Wunsch 2026-05-30):
  // wenn ein Channel-Send ohne Scope rausgeht (kein send_scope, kein Default,
  // kein Geo-Fallback), wird er statt geflood ALS ZERO-HOP direct gesendet
  // -- begrenzt Reichweite auf direkt-empfangbare Nachbarn statt Europa-
  // weiten Flood. Defaullt TRUE.
  // Runtime-only (nicht persistent). CLI: 'unscoped-channelmessages direct|flood'.
  bool          _unscoped_channel_direct;
  // Deferred reboot: wenn != 0, dann millis()-Zeitpunkt zu dem die loop()
  // den reboot ausloesen soll. Vermeidet das blockierende delay() im
  // CLI-Handler — sonst wuerde die loop() pausiert und der "Rebooting
  // now.."-Push waere nicht zur App ausgeliefert.
  unsigned long _pending_reboot_at;
  // DL9SAU Wunschliste 94 (2026-06-19): deferred shutdown (Pseudo-
  // Powerloss mit Buzzer-Sound). Sentinel 0 = nichts pending. Wie
  // _pending_reboot_at, aber Endzustand ist powerOff statt reboot.
  unsigned long _pending_shutdown_at;
#if defined(NRF52_PLATFORM)
  // 2026-06-15: deferred DFU-Mode-Entry (NRF52 only -- ESP32 hat keinen
  // SoftDevice-Bootloader, dort spart das #ifdef RAM). _pending_dfu_at = 0
  // sentinel 'nichts pending'. _pending_dfu_serial = true -> enterSerialDfu,
  // sonst enterUf2Dfu (Default UF2 = drag-and-drop USB-Mass-Storage).
  unsigned long _pending_dfu_at    = 0;
  bool          _pending_dfu_serial = false;
  // 2026-06-15 T1000-E-Workaround: NVIC_SystemReset() feuert auf T1000-E
  // nicht zuverlaessig (auch nicht via Adafruit reset_mcu Pattern). WDT
  // ist die einzige verlaessliche Reset-Quelle. Wenn watchdog_mode == 1,
  // setzen reboot/dfu CLI dieses Flag -> petWatchdog() in main.cpp pet
  // nicht mehr -> WDT feuert in <=90s -> Hardware-Reset. Fuer DFU wird
  // vorher GPREGRET-Magic gesetzt -- bleibt ueber WDT-Reset erhalten.
  bool          _wdt_let_fire = false;
#endif
  // Detail-Statistik-Counter (RAM-only, reset bei Reboot).
  // Indizes: ADV_TYPE_* (0..4) bzw. PAYLOAD_TYPE_* (0..0x0F).
  uint16_t      _heard_direct[5];        // zero-hop empfangene Adverts pro Node-Typ
  // Qualitaets-Klassifizierung der zero-hop empfangenen Adverts (SNR-Schwellen
  // in dB: gut >= 0, mittel >= -8, schlecht < -8). Dimensionen: [type][quality]
  uint16_t      _heard_quality[5][3];    // [ADV_TYPE_*][0=gut, 1=mittel, 2=schlecht]
  int8_t        _last_advert_snr_q4;     // SNR (q4) der zuletzt empfangenen Advert
  int8_t        _last_advert_rssi_dbm;   // RSSI (dBm) der zuletzt empfangenen Advert

  // RTC-Persistierung (User-Bug 2026-06-14: nach App-Sync werden Messages
  // aus den Buckets popt + Save-on-Pop entfernt sie aus Flash. Bei Reboot
  // ohne neue Adverts/Messages faellt RTC-Bootstrap zurueck auf alte
  // contact.lastmod -- der per BLE-Time-Sync gesetzte Wert ist verloren.)
  // Wir schreiben den aktuellen RTC-Stand in eine kleine Flash-Datei
  // (9 byte) wenn er sich um >= RTC_PERSIST_MIN_DELTA gegenueber dem
  // zuletzt gespeicherten Wert weiterentwickelt hat. Beim Boot wird der
  // Wert geladen und als zusaetzliche Bootstrap-Quelle verwendet (neuer
  // gewinnt).
  uint32_t      _rtc_persist_last_saved = 0;        // zuletzt persistierter RTC-Wert
  uint32_t      _rtc_persist_check_ms = 0;          // millis() der letzten Loop-Pruefung
  uint32_t      _rtc_persist_last_write_ms = 0;     // millis() des letzten echten Flash-Writes
  // Wunschliste 53 Phase 2026-06-14: Piggyback der Session-Uptime in
  // rtc_persist. Wird in loadRtcPersist() befuellt (0 wenn Alt-Format).
  // Vom Boot-Log gelesen fuer "Letzte Session lief ~Xh"-Anzeige.
  uint32_t      _last_session_uptime_ms = 0;
  // Wunschliste 53 Phase 2026-06-14: Reset-Reason vom main.cpp gemeldet.
  // Enum-Mapping siehe main.cpp WdtResetKind. Wird fuer stats-core
  // 'last_reset'-Zeile und Boot-Log gelesen.
  uint8_t       _last_reset_reason = 0;  // 0 = UNKNOWN
  // Wunschliste 53 Stage 9 (2026-06-14): LoRa SPI-Liveness-Probe.
  // Periodisches getCurrentRSSI() (live SPI-Read von SX126x getRSSI(false))
  // checkt ob Chip antwortet. MISO-stuck-high -> RSSI -127.5 dBm, MISO-
  // stuck-low -> 0 dBm; beides out-of-range. 3 Strikes hintereinander
  // (= 90s) -> Reboot mit Boot-Log Cause "LORA-DEAD".
  // Nur aktiv wenn _prefs.watchdog_mode == 1, nur wenn !isReceivingPacket
  // (kein TX/RX in flight um Chip nicht zu stoeren).
  uint32_t      _radio_health_check_ms = 0;
  uint8_t       _radio_dead_strikes    = 0;
  static const uint32_t RADIO_HEALTH_INTERVAL_MS = 30UL * 1000;
  static const uint8_t  RADIO_HEALTH_MAX_STRIKES = 3;
  // RTC-Progress-Grenze: kleinere Bumps zaehlen als 'kein Fortschritt'.
  static const uint32_t RTC_PERSIST_MIN_DELTA = 60; // sec
  // Min-Abstand zwischen tatsaechlichen Flash-Writes -- schuetzt vor
  // bug-haftem Adverter der alle 3min adverten wuerde (User-Sorge
  // 2026-06-14). 10 min -> max ~144 Writes/Tag, plus 30-min Periodic.
  static const uint32_t RTC_PERSIST_MIN_WRITE_INTERVAL_MS = 10UL*60UL*1000UL;
  // User-Bug 2026-06-14: Reboot-Gruende koennen Bugs sein, tiefentladener
  // Akku, oder Solar-Unterversorgung. In den ersten 90s nach Boot keine
  // Persist-Writes -- sonst koennte ein moeglicherweise korrupter RTC-Wert
  // beim sofortigen Boot+GPS-Fix schon den File-Stempel ueberschreiben.
  // Nach 90s sind Sync-Quellen (GPS, lazy-advert, strict-advert,
  // CMD_SET_DEVICE_TIME) plausibel etabliert; ab dann normale Write-Logik.
  static const uint32_t RTC_PERSIST_BOOT_DELAY_MS = 90UL*1000UL;  // 90 sec
  // User-Bug 2026-06-14: nach Flash 8.5min laufen lassen + Reboot zeigte
  // RTC-Rueckwaerts-Sprung weil periodic check erst bei 30min greift.
  // Auf 10min runter -- damit erster Save schon bei Boot+10min liegt;
  // Reboot-Risiko-Fenster halbiert sich gegenueber 30min-Setup.
  // Write-Interval-Guard (RTC_PERSIST_MIN_WRITE_INTERVAL_MS) verhindert
  // weiterhin Haeufungen -- echte Writes maximal alle 10min.
  static const uint32_t RTC_PERSIST_CHECK_INTERVAL_MS = 10UL*60UL*1000UL;  // 10 min
  // force=true: Save erlauben auch wenn rtc <= last_saved (z.B. App-
  // CMD_SET_DEVICE_TIME bei Rueckwaerts-Korrektur, App ist immer
  // authoritativ -- User-Spec 2026-06-14).
  void          saveRtcPersist(uint32_t rtc, bool force = false);
  uint32_t      loadRtcPersist();
  // Wunschliste 53 Phase 7 (2026-06-14): private Helper fuer Boot-Log
  // (Lade-Routine fuer Ring-Buffer-Read in append/print). Public-Methoden
  // bootLogAppend / bootLogWritePreReboot / bootLogPrint / bootLogClear
  // im public-Block (siehe oben bei savePrefs).
  int           bootLogLoad(char dst[][96], int max_entries);
  static const int BOOT_LOG_MAX_ENTRIES = 10;
  int8_t        _last_advert_scoped;     // 1=scoped (transport_codes), 0=unscoped
  int8_t        _last_advert_route_direct; // 1=DIRECT-typed (sendZeroHop), 0=FLOOD-typed
  uint16_t      _rx_advert_total[5];     // ALLE empfangenen Adverts (egal Hop-Count) pro Node-Typ
  // Wunschliste 26 C: Scoped/Unscoped × Adv-Typ Aufschluesselung.
  // Indizes: [ADV_TYPE_*][0=unscoped, 1=scoped]. Increment parallel zu
  // _rx_advert_total bzw _heard_direct; existierende Displays unveraendert.
  uint16_t      _rx_advert_by_scope[5][2];
  uint16_t      _heard_direct_by_scope[5][2];
  // Wunschliste 26 D (2026-05-31): DIRECT-typed Adverts (= sendZeroHop)
  // pro Adv-Typ. Komplement zur FLOOD-typed Sicht in _rx_flood_by_ptype.
  // Non-Advert DIRECT-typed Pakete werden bewusst NICHT gezaehlt
  // (User-Entscheidung: nicht relevant).
  uint16_t      _rx_direct_advert_by_role[5];

  // Wunschliste 26 B (2026-05-31): rx-us Echo-Tracking.
  // Wir merken uns 32 Hashes selbst-initiierter Sendungen und 128 Hashes
  // repeateter Sendungen (4-Byte truncated MAX_HASH). Auf jedem empfangenen
  // Flood-Paket (filterRecvFloodPacket -- laeuft VOR hasSeen-Dedup) wird
  // gegen beide Sets gematched; Match -> entsprechender rx_us-Counter
  // erhoeht. Aussagekraft: 'wie haeufig werden eigene Pakete weiter ins
  // Netz hineingetragen' -- niedrig = wir sind isoliert, hoch = wir
  // werden propagiert.
  //
  // Sizing-Rationale: ein vollausgenutzter Repeater (10% Airtime) kann ca.
  // 720 Pakete/h repeaten (0.5s avg Sendung). 128 Slots decken so ~10 min
  // Echo-Window ab. Self-initiated ist deutlich seltener (Adverts/User-
  // Chat) -- 32 Slots decken viele Stunden ab.
  uint32_t      _self_initiated_hashes[32];
  uint8_t       _self_initiated_head;       // naechster Schreib-Index (ringbuffer)
  uint32_t      _self_repeated_hashes[128];
  uint8_t       _self_repeated_head;
  uint16_t      _rx_us_self_initiated_count;  // Echo-Counter
  uint16_t      _rx_us_repeated_count;
  // Hash-Helper (4-Byte truncated MAX_HASH_SIZE=8).
  uint32_t calcShortHash(const mesh::Packet* packet) const;
  void     markSelfInitiated(const mesh::Packet* packet);
  // Gibt ein gehoertes Echo eines EIGENEN Sends direkt als [deliv]-Zeile aus
  // (Repeater = letzter Path-Eintrag in eigener path-hash-size; Channel-Name
  // aus payload[0]-Hash via channels-Liste). Kein State ausser dem self-hash-Ring.
  void     deliveryTraceEcho(mesh::Packet* packet);
  void     markSelfRepeated(const mesh::Packet* packet);
  // Lookup-Resultat: 0=kein Match, 1=self-initiated, 2=repeated.
  uint8_t  matchSelfHash(uint32_t h) const;
  // Wunschliste 26 D (2026-05-31): zusaetzliche path_len-Achse.
  // [ptype][0]=heard-direct (path_len==0), [ptype][1]=repeated (path_len>0).
  // Summe ist die alte _rx_flood_by_ptype-Semantik.
  uint16_t      _rx_flood_by_ptype[16][2];
  uint16_t      _repeat_by_ptype[16];    // Pakete die WIR tatsaechlich durchgereicht haben
  uint16_t      _tx_total_by_ptype[16];  // ALLE TX (eigen + repeated); eigen = total - repeat
  // Self-initiated FLOOD-Tx pro ptype. Subset von '_tx_total_by_ptype - _repeat_by_ptype'.
  // Direct = self_total - self_flood. Erlaubt Stats-Anzeige "tx own flood vs direct"
  // pro ptype (z.B. um zu sehen ob der nightly-flood-Beacon-Advert raus ging).
  uint16_t      _tx_self_flood_by_ptype[16];
  uint32_t      _tx_repeat_airtime_ms;   // geschaetzte Airtime nur unserer Repeats
  // Duty-Cycle Sliding-Window (siehe CR_DUTY_* Konstanten).
  uint32_t      _duty_air_ms_per_minute[CR_DUTY_WINDOW_SLOTS];
  uint8_t       _duty_slot_idx;          // 0..59
  unsigned long _duty_slot_start_ms;     // millis() bei Slot-Start
  unsigned long _duty_last_total_ms;     // letzter Snapshot getTotalAirTime()
  uint32_t      _duty_blocked_count;     // gedroppte Pakete (Soft+Hard zusammen)

  // ---- Wunschliste 31: Advert-basierte RTC-Sync (RAM-State) ----
  uint32_t      _time_sync_last_at_rtc;  // RTC-Wert beim letzten angewendeten Sync (24h-Cap)
  uint32_t      _time_sync_strict_last_ts[3]; // Replay: pro Strict-Source letzter akzept. timestamp
  uint8_t       _time_sync_last_pubkey[3];   // 3-Byte Prefix der zuletzt genutzten Quelle (Display)
  bool          _time_sync_done_since_boot;  // erster Sync nach Boot vollzogen (Boot-Drift-Bypass)
  // Reise-Wunsch 2026-06-08: letzter adv-sync-CANDIDATE merken (egal ob
  // applied oder skipped), fuer Diagnose im clock-Befehl. Wird bei JEDEM
  // maybeAdvertTimeSync-Aufruf gesetzt der die Plausibility passiert.
  uint8_t       _last_adv_sync_pubkey[3];    // Pubkey-Prefix des letzten Versuchs
  uint32_t      _last_adv_sync_ts;           // Sender-timestamp im Versuch
  uint32_t      _last_adv_sync_at_rtc;       // RTC-Zeit zum Zeitpunkt des Versuchs
  int32_t       _last_adv_sync_delta;        // ts - rtc beim Versuch
  uint8_t       _last_adv_sync_outcome;      // 0=none yet, 1=applied, 2=skipped-24h-cap,
                                              // 3=skipped-replay, 4=skipped-too-small,
                                              // 5=skipped-too-big-1src

  // ---- Wunschliste 35: Channel-Message Sender-Annotation (Once-per-Tuple) ----
  // Bei Channel-Messages anhaengen einer Annotation '(#scope[, direct])' an
  // den Sender-Anzeigenamen. Aber nur EINMAL pro (Name, Scope, Direct)-Tuple
  // -- danach plain 'Name: text'. Vermeidet dass der App-Reply-Button mit
  // dem augmentierten Namen '@[Name (#scope, direct)]' den App-Sound-Trigger
  // verfehlt (App matched exact-name).
  // Storage: 16 Eintraege global, FNV-1a 32-bit Hash ueber Name + Scope.
  // Sentinels fuer scope_fnv1a:
  //   0xFFFFFFFE = '#?' (scoped, aber unbekannte region)
  //   0xFFFFFFFF = '#*' (unscoped)
  //   andere     = FNV-1a vom Scope-Namen
  struct ChannelSenderSeen {
    uint32_t channel_hash; // 0 = DM (kein Channel), sonst 4-Byte Channel-Hash
    uint32_t name_fnv1a;
    uint32_t scope_fnv1a;
    uint32_t path_fnv1a;   // DM: FNV ueber pkt->path (0 wenn path_len==0).
                           // Channel: immer 0 (Path wird bei Channel nicht getrackt).
                           // Path-Wechsel bei DM -> neuer Tuple -> neuer []-Frame.
    uint8_t  scope_key[16]; // Task 63: Reply-Scope-Cache. Bei Channel-Msg
                           // gespeichert wenn Sender bekannten Scope-Namen
                           // hatte (aus Region-Table). Null (all zero) wenn
                           // Sender unscoped ('#*') oder unbekannter Scope
                           // ('#?') war. Wird bei Reply-Detection genutzt.
    uint8_t  direct_flag;  // 0 = via repeats, 1 = direkt gehoert (path_len==0)
  };
  static const int CHANNEL_SENDER_SEEN_MAX = 16;
  ChannelSenderSeen _channel_sender_seen[CHANNEL_SENDER_SEEN_MAX];
  uint8_t           _channel_sender_seen_count;  // Anzahl belegter Slots (<= MAX)
  uint8_t           _channel_sender_seen_next;   // naechster Slot fuer LRU-Wrap

  // FNV-1a 32-bit Hash-Helper.
  static uint32_t fnv1a32(const char* data, size_t len);
  static uint32_t fnv1a32_cstr(const char* s);
  // Liefert true wenn das Tuple in der seen-Liste war (also schon annotiert).
  // Andernfalls: ein neuer Eintrag wird angelegt und false zurueckgegeben
  // (caller fuegt die Annotation diesmal hinzu).
  // channel_hash=0 fuer DMs (kein Channel-Bezug), sonst 4-Byte Channel-Hash
  // (per-Channel-Separation: derselbe Sender in zwei Channels = zwei Tupel).
  bool channelSenderSeenLookupOrAdd(uint32_t channel_hash,
                                    uint32_t name_fnv1a, uint32_t scope_fnv1a,
                                    uint32_t path_fnv1a,
                                    const uint8_t scope_key[16],
                                    uint8_t direct_flag);
  // Task 63: Reply-Scope-Cache lookup. Suche einen (channel_hash, name_h)
  // Slot; wenn gefunden und scope_key != null, kopiere Key nach out_key.
  // Returns true wenn Match mit gueltigem Key (Reply-Scope aktivierbar).
  // out_scope_fnv1a (optional): scope_fnv1a des gefundenen Slots (0 wenn nicht
  // gefunden). Sentinels: 0xFFFFFFFF = #* (unscoped), 0xFFFFFFFE = #? (unbekannt)
  // -- erlaubt der Reply-Seite '#*' zu erkennen obwohl der Key null ist.
  bool channelSenderScopeLookup(uint32_t channel_hash, uint32_t name_fnv1a,
                                uint8_t out_key[16],
                                uint32_t* out_scope_fnv1a = nullptr);

  // Lazy-Mode Boot-Collection-Phase: bis zu 5 Kandidaten ueber 3 Min
  // sammeln, dann Cluster-Auswertung. RAM-only, einmal pro Boot.
  struct TimeSyncCandidate {
    uint32_t timestamp;
    uint8_t  pub_key3[3];
    uint8_t  adv_type;
  };
  TimeSyncCandidate _time_sync_lazy_cands[5];
  uint8_t       _time_sync_lazy_count;
  unsigned long _time_sync_lazy_started_ms; // 0 = noch nicht gestartet
  bool          _time_sync_lazy_done;       // Collection abgeschlossen

  // RAM-only counters for STATS display (reset on reboot)
  uint32_t      _tx_advert_count;            // own adverts: periodic + nightly + manual
  uint32_t      _tx_digi_count;              // packets we accepted for forwarding
  uint32_t      _bt_connect_count;           // app/BT connect events (rising edges)
  bool          _last_serial_connected;      // edge-detector state
  uint32_t      _last_observed_rtc;          // for detecting external RTC corrections
  // ENTFERNBAR (Wunschliste 40, BLE-Powerbank-Diagnose): wenn das
  // Disconnect-Storm-Problem dauerhaft Geschichte ist, koennen die folgenden
  // 4 Variablen + zugehoeriger Loop-Block in MyMesh::loop() + 'bleinfo' CLI
  // + 'trace bt' Kategorie ersatzlos geloescht werden. Permanenter Fix
  // (FRAME_QUEUE_SIZE Bump) bleibt davon unberuehrt.
  unsigned long _next_heap_log_at;            // millis() fuer naechsten heap-log
  uint32_t      _last_logged_disconnect_count; // delta-detect fuer disconnect-trace
  uint32_t      _session_min_heap;            // min-heap ueber Session
  unsigned long _last_ble_diag_log_at;        // 30sec Cooldown gegen Feedback-Loop

  // Key-Cache fuer Build-in-Region-Eintraege (Wunschliste 11 Schritt 4).
  // Wird in begin() einmalig befuellt: TransportKey pro Build-in-Name aus
  // dl9sau_geo_recommendations-Tabelle (SHA-256("#name")). Size ist obere
  // Schranke fuer die Build-in-Tabellen-Groesse — aktuell ~25 Eintraege,
  // capacity bis 64.
  static constexpr int SCOPE_BUILDIN_KEY_CACHE_MAX = 64;
  TransportKey  _buildin_keys[SCOPE_BUILDIN_KEY_CACHE_MAX];
  int           _buildin_keys_count;          // tatsaechliche Anzahl belegter Slots

  // Bbox-Tracking RAM-Arrays (Wunschliste 11 Schritt 5). Pro Eintrag:
  // ist die aktuelle Position innerhalb der Bbox? Wird von
  // evaluateScopeBboxes() gesetzt; von scopeAllowedForRepeat() gelesen
  // wenn repeat_mode == AUTO. Default false (out-of-bbox / no-bbox).
  bool          _buildin_in_bbox[SCOPE_BUILDIN_KEY_CACHE_MAX];
  bool          _extras_in_bbox[SCOPE_EXTRAS_SLOTS];

  // Wunschliste 15: zero-hop Runtime-Neighbour-Tabelle.
  RuntimeNeighbour _neighbours[MAX_RUNTIME_NEIGHBOURS];
  uint8_t          _neighbours_count;

public:
  uint32_t getTxAdvertCount()  const { return _tx_advert_count; }
  uint32_t getTxDigiCount()    const { return _tx_digi_count; }
  uint32_t getBtConnectCount() const { return _bt_connect_count; }
  // Reset aller RAM-Statistik-Counter (Companion-CLI 'clear stats')
  void clearStats();
  uint32_t getMillisWraps()    const { return _millis_wraps; }   // >0 means uptime > ~49 days
  bool     isClientRepeatOn()  const { return _prefs.client_repeat != 0; }
};

extern MyMesh the_mesh;
