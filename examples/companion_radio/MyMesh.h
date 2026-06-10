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
  uint8_t  adv_type;         // ADV_TYPE_*
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
// Messages pusht. Setzen via "trace <cat> on/off" Befehl. Reset bei Reboot,
// damit man nicht versehentlich eine Trace-Kategorie auf-Dauer aktiv lässt.
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
#define TRACE_ALL_MASK  0x1FFF

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

  int  getRecentlyHeard(AdvertPath dest[], int max_num);

protected:
  float getAirtimeBudgetFactor() const override;
  int getInterferenceThreshold() const override;
  int getAGCResetInterval() const override;
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
                           int8_t snr_q4, uint8_t adv_type);
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

  // To check if there is pending work
  bool hasPendingWork() const;

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
  struct DiscoverEntry {
    uint8_t pub_key[32];     // 32 byte voll oder 8 byte prefix + Rest 0
    uint8_t adv_type;        // ADV_TYPE_*
    int8_t  their_snr_q4;    // ihre SNR-Sicht auf unseren REQ (aus RESP-Payload)
    int8_t  our_snr_q4;      // unsere SNR-Sicht auf ihren RESP
    int8_t  our_rssi_dbm;    // unsere RSSI-Sicht (dBm, int8); their_rssi nicht im Protokoll
    bool    full_pubkey;     // true wenn 32 byte, false wenn 8 byte prefix
    uint32_t recv_at_rtc;    // Reise-Wunsch 2026-06-08: RTC bei Eintrags-Empfang
                              // fuer 15-min-Cache + LRU-Overwrite bei voller Liste
  };
  static const int MAX_DISCOVER_ENTRIES = 16;
  // ---- (a) Sender 'discover*' ---------------------------------------------
  DiscoverEntry _discover_entries[MAX_DISCOVER_ENTRIES];
  uint8_t       _discover_count;
  uint32_t      _discover_tag;
  bool          _discover_active;
  unsigned long _discover_expiry_ms;
  unsigned long _discover_next_allowed_ms; // rate-limit: 60s seit letztem REQ
  // RTC-Timestamp wann der letzte discover-Batch endete (>=1 Antwort).
  // Verwendet von 'neighbors' um discover-only-Eintraege (nicht in
  // Kontaktliste) bis 48h nachzulisten.
  uint32_t      _discover_last_at_rtc;
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
  struct PendingRegionsEntry {
    uint32_t tag;
    char     name[32];          // contact name oder leer
    uint8_t  pubkey[32];        // voller pubkey -- bei chain-mode brauchen
                                // wir den fuer den Legenden-Lookup
    bool     from_chain;        // true=chain-buffered, false=manual immediate-push
  };
  // Limit 16 fuer Paritaet mit MAX_DISCOVER_ENTRIES (alle drei Strukturen
  // -- CTL-Antworten, in-flight Tags, gebufferte ANON-RESPs -- sind im
  // Chain-Modus 1:1 verbunden).
  static const int MAX_PENDING_REGIONS = 16;
  PendingRegionsEntry _regions_pending[MAX_PENDING_REGIONS];
  uint8_t  _regions_pending_count;
  // Chain-mode Buffered-Responses (Aggregator)
  struct CompletedRegionsEntry {
    uint8_t  pubkey[32];
    int8_t   our_snr_q4;        // unsere SNR-Sicht der RESP
    char     csv[120];          // CSV der Regions vom Responder
  };
  static const int MAX_COMPLETED_REGIONS = 16;
  CompletedRegionsEntry _regions_completed[MAX_COMPLETED_REGIONS];
  uint8_t  _regions_completed_count;
  // Flag: 'discover regions' (no args) hat CTL-Discover ausgeloest und
  // erwartet pro REPEATER-RESP einen automatischen ANON_REQ_TYPE_REGIONS.
  bool          _discover_regions_chained;
  unsigned long _regions_chain_finalize_at;  // millis() Zeitpunkt fuer Aggregat
  bool sendRegionsQueryZeroHop(const uint8_t* pubkey32, const char* display_name);
  void finalizeRegionsChain();
  // Einheitlicher Legend-Entry fuer 'discover' (CTL-only) UND
  // 'discover regions' (Chain mit Region-CSV). entry liefert
  // SNR + role + name, csv_or_null fuegt ggf. Region-Info hinzu.
  // verbose=true: zeigt raw SNR/RSSI-Werte (fuer 'discover')
  // verbose=false: nur Quality-Klassifikation (fuer 'discover regions' chain)
  void printRepeaterLegendEntry(const DiscoverEntry& entry,
                                const CompletedRegionsEntry* csv_or_null,
                                bool verbose);
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
  char     _br_json[2560];
  uint16_t _br_json_len;
  int16_t  _br_brace_depth;
  bool     _br_in_string;
  bool     _br_escape_next;
  unsigned long _br_timeout_at;
  uint16_t _br_applied;
  uint16_t _br_skipped;
  uint16_t _br_errors;
  // True wenn restorerte Felder einen Reboot empfehlen (Radio-Params,
  // prv_key). Wird in der End-Statusmeldung gehinted, kein auto-action.
  bool     _br_reboot_recommended;
  // CLI-Einstieg
  void backupRestoreStart();
  // In loop() pollen
  void backupRestoreLoop();
  // Block-Parser nach komplettem JSON-Body
  void backupRestoreParseBlock();
  // Beenden / abbrechen
  void backupRestoreFinish(const char* reason);
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

  // Wunschliste 43 (2026-06-10): BLE-Power-Cycle State Machine.
  //   BLE_PWR_BOOT      = Boot-Grace (30 min, BLE an, wartet auf ersten Connect)
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
  // Deferred-disable analog _pending_reboot_at: bei 'bluetooth off'-CLI
  // bekommt die App noch 3s Zeit den OK-Frame ueber BLE zu empfangen
  // bevor wir den Chip abschalten.
  uint32_t    _pending_ble_off_at = 0;
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
  // entsprechende Flag in _trace_flags gesetzt ist. No-op sonst.
  void traceCompanion(uint16_t flag, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
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
  // Detail-Statistik-Counter (RAM-only, reset bei Reboot).
  // Indizes: ADV_TYPE_* (0..4) bzw. PAYLOAD_TYPE_* (0..0x0F).
  uint16_t      _heard_direct[5];        // zero-hop empfangene Adverts pro Node-Typ
  // Qualitaets-Klassifizierung der zero-hop empfangenen Adverts (SNR-Schwellen
  // in dB: gut >= 0, mittel >= -8, schlecht < -8). Dimensionen: [type][quality]
  uint16_t      _heard_quality[5][3];    // [ADV_TYPE_*][0=gut, 1=mittel, 2=schlecht]
  int8_t        _last_advert_snr_q4;     // SNR (q4) der zuletzt empfangenen Advert
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
                                    uint8_t direct_flag);

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
