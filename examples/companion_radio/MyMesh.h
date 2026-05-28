#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include "AbstractUITask.h"

/*------------ Frame Protocol --------------*/
#define FIRMWARE_VER_CODE 11

#ifndef FIRMWARE_BUILD_DATE
#define FIRMWARE_BUILD_DATE "25 May 2026"
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v1.15.0-GIT-DL9SAU"
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

#ifndef OFFLINE_QUEUE_SIZE
#define OFFLINE_QUEUE_SIZE 16
#endif

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

// Wunschliste 7 Phase 3: Anonymous-Request-Typen (Discovery-Queries).
// Werden in onAnonDataRecv anhand der effektiven Role gegated.
#define ANON_REQ_TYPE_REGIONS    0x01
#define ANON_REQ_TYPE_OWNER      0x02
#define ANON_REQ_TYPE_BASIC      0x03  // just remote clock + features

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
// Messages pusht. Setzen via "trace <cat> on/off" Befehl. Reset bei Reboot,
// damit man nicht versehentlich eine Trace-Kategorie auf-Dauer aktiv lässt.
#define TRACE_GPS      0x0001
#define TRACE_ADVERTS  0x0002
#define TRACE_REPEAT   0x0004
#define TRACE_SCOPE    0x0008
#define TRACE_MOTION   0x0010
#define TRACE_HEARD    0x0020
#define TRACE_RTC      0x0040
#define TRACE_CONNECT  0x0080
#define TRACE_FILTER   0x0100
#define TRACE_NIGHT    0x0200
#define TRACE_DUTY     0x0400
#define TRACE_ALL_MASK 0x07FF

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
#define LOCAL_TZ_OFFSET_SECS    3600   // CET; override per-build for other zones
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

  int  getRecentlyHeard(AdvertPath dest[], int max_num);

protected:
  float getAirtimeBudgetFactor() const override;
  int getInterferenceThreshold() const override;
  int calcRxDelay(float score, uint32_t air_time) const override;
  uint32_t getRetransmitDelay(const mesh::Packet *packet) override;
  uint32_t getDirectRetransmitDelay(const mesh::Packet *packet) override;
  bool shouldReduceFloodRetransmit(const mesh::Packet* packet, uint8_t original_path_count) const override;
  uint8_t getExtraAckTransmitCount() const override;
  bool filterRecvFloodPacket(mesh::Packet* packet) override;
  bool allowPacketForward(const mesh::Packet* packet) override;

  void sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis);
  void sendFloodScoped(const ContactInfo& recipient, mesh::Packet* pkt, uint32_t delay_millis=0) override;
  void sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis=0) override;

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
  // Pusht eine Trace-Message in den Companion-Channel — aber nur wenn das
  // entsprechende Flag in _trace_flags gesetzt ist. No-op sonst.
  void traceCompanion(uint16_t flag, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
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
  bool getEffectiveLatLon(double& lat, double& lon) const;
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

  // Dispatcher hooks: per-packet TX-param override (CR5 / reduced power) used
  // for repeated packets and our automatic adverts. Eigene Direct-Messages
  // bleiben mit konfigurierter CR und voller TX-Power.
  void applyPacketTxOverrides(const mesh::Packet* packet) override;
  void restorePacketTxDefaults() override;

  // helpers, short-cuts
  void saveChannels() { _store->saveChannels(this); }
  void saveContacts() { _store->saveContacts(this); }

  DataStore* _store;
  NodePrefs _prefs;
  uint32_t pending_login;
  uint32_t pending_status;
  uint32_t pending_telemetry, pending_discovery;   // pending _TELEMETRY_REQ
  uint32_t pending_req;   // pending _BINARY_REQ
  BaseSerialInterface *_serial;
  AbstractUITask* _ui;

  ContactsIterator _iter;
  uint32_t _iter_filter_since;
  uint32_t _most_recent_lastmod;
  uint32_t _active_ble_pin;
  bool _iter_started;
  bool _cli_rescue;
  char cli_command[80];
  uint8_t app_target_ver;
  uint8_t *sign_data;
  uint32_t sign_data_len;
  unsigned long dirty_contacts_expiry;

  TransportKey send_scope;

  uint8_t cmd_frame[MAX_FRAME_SIZE + 1];
  uint8_t out_frame[MAX_FRAME_SIZE + 1];
  CayenneLPP telemetry;

  struct Frame {
    uint8_t len;
    uint8_t buf[MAX_FRAME_SIZE];

    bool isChannelMsg() const;
  };
  int offline_queue_len;
  Frame offline_queue[OFFLINE_QUEUE_SIZE];

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
  bool          _gps_had_fix_ever;           // true once GPS reported a valid fix this session
  unsigned long _gps_woke_at_millis;         // when we last (re-)enabled GPS; 0 = currently off (or never managed)
  unsigned long _gps_off_at_millis;          // when we last switched GPS off; 0 = currently on (or never managed)
  bool          _gps_fix_seen_this_wake;     // a position fix arrived since last wakeup — OK to sleep again
  bool          _gps_user_override_until_advert;  // user toggled GPS on via app — keep on until next advert
  uint32_t      _last_millis_seen;           // for wrap detection of millis()
  uint32_t      _millis_wraps;               // how many times millis() has wrapped since boot

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
  // Bitmask aktiver Trace-Kategorien (siehe TRACE_*-Konstanten). RAM-only,
  // reset bei Reboot — verhindert dass eine Trace-Kategorie versehentlich
  // unbegrenzt die Offline-Queue mit Events flutet.
  uint16_t      _trace_flags;
  // Deferred reboot: wenn != 0, dann millis()-Zeitpunkt zu dem die loop()
  // den reboot ausloesen soll. Vermeidet das blockierende delay() im
  // CLI-Handler — sonst wuerde die loop() pausiert und der "Rebooting
  // now.."-Push waere nicht zur App ausgeliefert.
  unsigned long _pending_reboot_at;
  // Detail-Statistik-Counter (RAM-only, reset bei Reboot).
  // Indizes: ADV_TYPE_* (0..4) bzw. PAYLOAD_TYPE_* (0..0x0F).
  uint16_t      _heard_direct[5];        // zero-hop empfangene Adverts pro Node-Typ
  // Qualitaets-Klassifizierung der zero-hop empfangenen Adverts (SNR-Schwellen
  // in dB: gut >= 0, mittel >= -8, schlecht < -8). Dimensionen: [type][quality]
  uint16_t      _heard_quality[5][3];    // [ADV_TYPE_*][0=gut, 1=mittel, 2=schlecht]
  int8_t        _last_advert_snr_q4;     // SNR (q4) der zuletzt empfangenen Advert
  uint16_t      _rx_advert_total[5];     // ALLE empfangenen Adverts (egal Hop-Count) pro Node-Typ
  uint16_t      _rx_flood_by_ptype[16];  // Flood-Pakete als Forward-Kandidat (alle Pkt-Typen)
  uint16_t      _repeat_by_ptype[16];    // Pakete die WIR tatsaechlich durchgereicht haben
  uint16_t      _tx_total_by_ptype[16];  // ALLE TX (eigen + repeated); eigen = total - repeat
  uint32_t      _tx_repeat_airtime_ms;   // geschaetzte Airtime nur unserer Repeats
  // Duty-Cycle Sliding-Window (siehe CR_DUTY_* Konstanten).
  uint32_t      _duty_air_ms_per_minute[CR_DUTY_WINDOW_SLOTS];
  uint8_t       _duty_slot_idx;          // 0..59
  unsigned long _duty_slot_start_ms;     // millis() bei Slot-Start
  unsigned long _duty_last_total_ms;     // letzter Snapshot getTotalAirTime()
  uint32_t      _duty_blocked_count;     // gedroppte Pakete (Soft+Hard zusammen)

  // RAM-only counters for STATS display (reset on reboot)
  uint32_t      _tx_advert_count;            // own adverts: periodic + nightly + manual
  uint32_t      _tx_digi_count;              // packets we accepted for forwarding
  uint32_t      _bt_connect_count;           // app/BT connect events (rising edges)
  bool          _last_serial_connected;      // edge-detector state
  uint32_t      _last_observed_rtc;          // for detecting external RTC corrections

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
