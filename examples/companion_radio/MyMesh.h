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

/* -------------------------------------------------------------------------------------- */

#define REQ_TYPE_GET_STATUS             0x01 // same as _GET_STATS
#define REQ_TYPE_KEEP_ALIVE             0x02
#define REQ_TYPE_GET_TELEMETRY_DATA     0x03

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
#define CR_MAX_REPEAT_PATH_LEN  16
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
  void savePrefs() { _store->savePrefs(_prefs, sensors.node_lat, sensors.node_lon); }

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

  // client-repeater + periodic advert helpers
  void applyRadioPolicy();   // calls radio_set_params() with freq/CR overrides
  void markHeardDirect(uint8_t hash);   // call only for zero-hop adverts
  bool isLocallyHeard(uint8_t hash) const;
  bool getEffectiveLatLon(double& lat, double& lon) const;
  bool chooseGeoFallbackScope(TransportKey& out_key) const;
  bool chooseNightFloodScope(TransportKey& out_key) const;
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

  TransportKey runtime_last_channel_scope;   // RAM-only; set on CMD_SEND_CHANNEL_TXT_MSG
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

  // RAM-only counters for STATS display (reset on reboot)
  uint32_t      _tx_advert_count;            // own adverts: periodic + nightly + manual
  uint32_t      _tx_digi_count;              // packets we accepted for forwarding
  uint32_t      _bt_connect_count;           // app/BT connect events (rising edges)
  bool          _last_serial_connected;      // edge-detector state
  uint32_t      _last_observed_rtc;          // for detecting external RTC corrections

public:
  uint32_t getTxAdvertCount()  const { return _tx_advert_count; }
  uint32_t getTxDigiCount()    const { return _tx_digi_count; }
  uint32_t getBtConnectCount() const { return _bt_connect_count; }
  uint32_t getMillisWraps()    const { return _millis_wraps; }   // >0 means uptime > ~49 days
  bool     isClientRepeatOn()  const { return _prefs.client_repeat != 0; }
};

extern MyMesh the_mesh;
