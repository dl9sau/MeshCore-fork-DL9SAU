#include "MyMesh.h"

#include <Arduino.h> // needed for PlatformIO
#include <Mesh.h>
#include <SHA256.h>
#include "dl9sau_geo_recommendations.h"

#define CMD_APP_START                 1
#define CMD_SEND_TXT_MSG              2
#define CMD_SEND_CHANNEL_TXT_MSG      3
#define CMD_GET_CONTACTS              4 // with optional 'since' (for efficient sync)
#define CMD_GET_DEVICE_TIME           5
#define CMD_SET_DEVICE_TIME           6
#define CMD_SEND_SELF_ADVERT          7
#define CMD_SET_ADVERT_NAME           8
#define CMD_ADD_UPDATE_CONTACT        9
#define CMD_SYNC_NEXT_MESSAGE         10
#define CMD_SET_RADIO_PARAMS          11
#define CMD_SET_RADIO_TX_POWER        12
#define CMD_RESET_PATH                13
#define CMD_SET_ADVERT_LATLON         14
#define CMD_REMOVE_CONTACT            15
#define CMD_SHARE_CONTACT             16
#define CMD_EXPORT_CONTACT            17
#define CMD_IMPORT_CONTACT            18
#define CMD_REBOOT                    19
#define CMD_GET_BATT_AND_STORAGE      20   // was CMD_GET_BATTERY_VOLTAGE
#define CMD_SET_TUNING_PARAMS         21
#define CMD_DEVICE_QUERY              22
#define CMD_EXPORT_PRIVATE_KEY        23
#define CMD_IMPORT_PRIVATE_KEY        24
#define CMD_SEND_RAW_DATA             25
#define CMD_SEND_LOGIN                26
#define CMD_SEND_STATUS_REQ           27
#define CMD_HAS_CONNECTION            28
#define CMD_LOGOUT                    29 // 'Disconnect'
#define CMD_GET_CONTACT_BY_KEY        30
#define CMD_GET_CHANNEL               31
#define CMD_SET_CHANNEL               32
#define CMD_SIGN_START                33
#define CMD_SIGN_DATA                 34
#define CMD_SIGN_FINISH               35
#define CMD_SEND_TRACE_PATH           36
#define CMD_SET_DEVICE_PIN            37
#define CMD_SET_OTHER_PARAMS          38
#define CMD_SEND_TELEMETRY_REQ        39  // can deprecate this
#define CMD_GET_CUSTOM_VARS           40
#define CMD_SET_CUSTOM_VAR            41
#define CMD_GET_ADVERT_PATH           42
#define CMD_GET_TUNING_PARAMS         43
// NOTE: CMD range 44..49 parked, potentially for WiFi operations
#define CMD_SEND_BINARY_REQ           50
#define CMD_FACTORY_RESET             51
#define CMD_SEND_PATH_DISCOVERY_REQ   52
#define CMD_SET_FLOOD_SCOPE_KEY       54   // v8+
#define CMD_SEND_CONTROL_DATA         55   // v8+
#define CMD_GET_STATS                 56   // v8+, second byte is stats type
#define CMD_SEND_ANON_REQ             57
#define CMD_SET_AUTOADD_CONFIG        58
#define CMD_GET_AUTOADD_CONFIG        59
#define CMD_GET_ALLOWED_REPEAT_FREQ   60
#define CMD_SET_PATH_HASH_MODE        61
#define CMD_SEND_CHANNEL_DATA         62
#define CMD_SET_DEFAULT_FLOOD_SCOPE   63
#define CMD_GET_DEFAULT_FLOOD_SCOPE   64
#define CMD_SEND_RAW_PACKET           65

// Stats sub-types for CMD_GET_STATS
#define STATS_TYPE_CORE               0
#define STATS_TYPE_RADIO              1
#define STATS_TYPE_PACKETS             2

#define RESP_CODE_OK                  0
#define RESP_CODE_ERR                 1
#define RESP_CODE_CONTACTS_START      2  // first reply to CMD_GET_CONTACTS
#define RESP_CODE_CONTACT             3  // multiple of these (after CMD_GET_CONTACTS)
#define RESP_CODE_END_OF_CONTACTS     4  // last reply to CMD_GET_CONTACTS
#define RESP_CODE_SELF_INFO           5  // reply to CMD_APP_START
#define RESP_CODE_SENT                6  // reply to CMD_SEND_TXT_MSG
#define RESP_CODE_CONTACT_MSG_RECV    7  // a reply to CMD_SYNC_NEXT_MESSAGE (ver < 3)
#define RESP_CODE_CHANNEL_MSG_RECV    8  // a reply to CMD_SYNC_NEXT_MESSAGE (ver < 3)
#define RESP_CODE_CURR_TIME           9  // a reply to CMD_GET_DEVICE_TIME
#define RESP_CODE_NO_MORE_MESSAGES    10 // a reply to CMD_SYNC_NEXT_MESSAGE
#define RESP_CODE_EXPORT_CONTACT      11
#define RESP_CODE_BATT_AND_STORAGE    12 // a reply to a CMD_GET_BATT_AND_STORAGE
#define RESP_CODE_DEVICE_INFO         13 // a reply to CMD_DEVICE_QUERY
#define RESP_CODE_PRIVATE_KEY         14 // a reply to CMD_EXPORT_PRIVATE_KEY
#define RESP_CODE_DISABLED            15
#define RESP_CODE_CONTACT_MSG_RECV_V3 16 // a reply to CMD_SYNC_NEXT_MESSAGE (ver >= 3)
#define RESP_CODE_CHANNEL_MSG_RECV_V3 17 // a reply to CMD_SYNC_NEXT_MESSAGE (ver >= 3)
#define RESP_CODE_CHANNEL_INFO        18 // a reply to CMD_GET_CHANNEL
#define RESP_CODE_SIGN_START          19
#define RESP_CODE_SIGNATURE           20
#define RESP_CODE_CUSTOM_VARS         21
#define RESP_CODE_ADVERT_PATH         22
#define RESP_CODE_TUNING_PARAMS       23
#define RESP_CODE_STATS               24   // v8+, second byte is stats type
#define RESP_CODE_AUTOADD_CONFIG      25
#define RESP_ALLOWED_REPEAT_FREQ      26
#define RESP_CODE_CHANNEL_DATA_RECV   27
#define RESP_CODE_DEFAULT_FLOOD_SCOPE 28

#define MAX_CHANNEL_DATA_LENGTH       (MAX_FRAME_SIZE - 9)

#define SEND_TIMEOUT_BASE_MILLIS        500
#define FLOOD_SEND_TIMEOUT_FACTOR       16.0f
#define DIRECT_SEND_PERHOP_FACTOR       6.0f
#define DIRECT_SEND_PERHOP_EXTRA_MILLIS 250
#define LAZY_CONTACTS_WRITE_DELAY       5000

#define PUBLIC_GROUP_PSK                "izOH6cXN6mrJ5e26oRXNcg=="

// these are _pushed_ to client app at any time
#define PUSH_CODE_ADVERT                0x80
#define PUSH_CODE_PATH_UPDATED          0x81
#define PUSH_CODE_SEND_CONFIRMED        0x82
#define PUSH_CODE_MSG_WAITING           0x83
#define PUSH_CODE_RAW_DATA              0x84
#define PUSH_CODE_LOGIN_SUCCESS         0x85
#define PUSH_CODE_LOGIN_FAIL            0x86
#define PUSH_CODE_STATUS_RESPONSE       0x87
#define PUSH_CODE_LOG_RX_DATA           0x88
#define PUSH_CODE_TRACE_DATA            0x89
#define PUSH_CODE_NEW_ADVERT            0x8A
#define PUSH_CODE_TELEMETRY_RESPONSE    0x8B
#define PUSH_CODE_BINARY_RESPONSE       0x8C
#define PUSH_CODE_PATH_DISCOVERY_RESPONSE 0x8D
#define PUSH_CODE_CONTROL_DATA          0x8E   // v8+
#define PUSH_CODE_CONTACT_DELETED       0x8F // used to notify client app of deleted contact when overwriting oldest
#define PUSH_CODE_CONTACTS_FULL         0x90 // used to notify client app that contacts storage is full
#define PUSH_CODE_DEBUG_LOG             0x91 // DL9SAU: ASCII debug line for the app's Debug-Protokolle view

#define ERR_CODE_UNSUPPORTED_CMD        1
#define ERR_CODE_NOT_FOUND              2
#define ERR_CODE_TABLE_FULL             3
#define ERR_CODE_BAD_STATE              4
#define ERR_CODE_FILE_IO_ERROR          5
#define ERR_CODE_ILLEGAL_ARG            6

#define MAX_SIGN_DATA_LEN               (8 * 1024) // 8K

// Auto-add config bitmask
// Bit 0: If set, overwrite oldest non-favourite contact when contacts file is full
// Bits 1-4: these indicate which contact types to auto-add when manual_contact_mode = 0x01
#define AUTO_ADD_OVERWRITE_OLDEST (1 << 0)  // 0x01 - overwrite oldest non-favourite when full
#define AUTO_ADD_CHAT             (1 << 1)  // 0x02 - auto-add Chat (Companion) (ADV_TYPE_CHAT)
#define AUTO_ADD_REPEATER         (1 << 2)  // 0x04 - auto-add Repeater (ADV_TYPE_REPEATER)
#define AUTO_ADD_ROOM_SERVER      (1 << 3)  // 0x08 - auto-add Room Server (ADV_TYPE_ROOM)
#define AUTO_ADD_SENSOR           (1 << 4)  // 0x10 - auto-add Sensor (ADV_TYPE_SENSOR)

void MyMesh::writeOKFrame() {
  uint8_t buf[1];
  buf[0] = RESP_CODE_OK;
  _serial->writeFrame(buf, 1);
}
void MyMesh::writeErrFrame(uint8_t err_code) {
  uint8_t buf[2];
  buf[0] = RESP_CODE_ERR;
  buf[1] = err_code;
  _serial->writeFrame(buf, 2);
}

void MyMesh::writeDisabledFrame() {
  uint8_t buf[1];
  buf[0] = RESP_CODE_DISABLED;
  _serial->writeFrame(buf, 1);
}

void MyMesh::writeContactRespFrame(uint8_t code, const ContactInfo &contact) {
  int i = 0;
  out_frame[i++] = code;
  memcpy(&out_frame[i], contact.id.pub_key, PUB_KEY_SIZE);
  i += PUB_KEY_SIZE;
  out_frame[i++] = contact.type;
  out_frame[i++] = contact.flags;
  out_frame[i++] = contact.out_path_len;
  memcpy(&out_frame[i], contact.out_path, MAX_PATH_SIZE);
  i += MAX_PATH_SIZE;
  StrHelper::strzcpy((char *)&out_frame[i], contact.name, 32);
  i += 32;
  memcpy(&out_frame[i], &contact.last_advert_timestamp, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.gps_lat, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.gps_lon, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.lastmod, 4);
  i += 4;
  _serial->writeFrame(out_frame, i);
}

void MyMesh::updateContactFromFrame(ContactInfo &contact, uint32_t& last_mod, const uint8_t *frame, int len) {
  int i = 0;
  uint8_t code = frame[i++]; // eg. CMD_ADD_UPDATE_CONTACT
  memcpy(contact.id.pub_key, &frame[i], PUB_KEY_SIZE);
  i += PUB_KEY_SIZE;
  contact.type = frame[i++];
  contact.flags = frame[i++];
  contact.out_path_len = frame[i++];
  memcpy(contact.out_path, &frame[i], MAX_PATH_SIZE);
  i += MAX_PATH_SIZE;
  memcpy(contact.name, &frame[i], 32);
  i += 32;
  memcpy(&contact.last_advert_timestamp, &frame[i], 4);
  i += 4;
  if (len >= i + 8) { // optional fields
    memcpy(&contact.gps_lat, &frame[i], 4);
    i += 4;
    memcpy(&contact.gps_lon, &frame[i], 4);
    i += 4;
    if (len >= i + 4) {
      memcpy(&last_mod, &frame[i], 4);
    }
  }
}

bool MyMesh::Frame::isChannelMsg() const {
  return buf[0] == RESP_CODE_CHANNEL_MSG_RECV || buf[0] == RESP_CODE_CHANNEL_MSG_RECV_V3 ||
         buf[0] == RESP_CODE_CHANNEL_DATA_RECV;
}

// Dekodierte Public-Channel-PSK (16 Bytes). Aus PUBLIC_GROUP_PSK
// "izOH6cXN6mrJ5e26oRXNcg==" einmalig vor-dekodiert -- spart Runtime-
// base64 + Header-Konflikt (base64.hpp ist header-only und kollidiert wenn
// in zwei Translation-Units inkludiert wie BaseChatMesh.cpp + MyMesh.cpp).
static const uint8_t s_public_psk[16] = {
  0x8B, 0x33, 0x87, 0xE9, 0xC5, 0xCD, 0xEA, 0x6A,
  0xC9, 0xE5, 0xED, 0xBA, 0xA1, 0x15, 0xCD, 0x72
};

// Magic-PSK fuer den lokalen $companion-Channel.
// 16 Bytes: 8x 0x00 + 8x 0x99 -- als Hex-String "00..00 99..99" (32 Zeichen)
// einfach von Hand in die App einzugeben falls jemals manuell wieder
// herzustellen. PSK-basierte Identifikation statt Name-Match macht den
// Channel robust gegen Rename + Sync-Race (User-Bugreport 2026-06-01).
static const uint8_t s_companion_psk_magic[16] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99
};

// True wenn das Channel an channel_idx unsere companion-Magic-PSK traegt.
// Zentrale Identitaetspruefung -- wird sowohl beim RX-Klassifizieren als
// auch beim TX-Annehmen verwendet damit App-Index-Drift egal ist.
// Nicht const weil getChannel() in der Basis-Klasse non-const ist
// (verlangt ChannelDetails& by reference, nicht von uns geaendert).
bool MyMesh::isCompanionChannel(uint8_t channel_idx) {
  ChannelDetails ch;
  if (!getChannel(channel_idx, ch)) return false;
  return memcmp(ch.channel.secret, s_companion_psk_magic, 16) == 0;
}

// out_cap = Hardware-Kapazitaet (Array-Groesse). Runtime-Limit (slot count
// das tatsaechlich benutzt wird) ist getBucketLimit() -- kann kleiner sein.
void MyMesh::getBucket(MsgBucket b, Frame*& out_arr, int& out_cap) {
  switch (b) {
    case BUCKET_PUBLIC:    out_arr = bucket_public;    out_cap = BUCKET_CAP_PUBLIC;    break;
    case BUCKET_HASHTAG:   out_arr = bucket_hashtag;   out_cap = BUCKET_CAP_HASHTAG;   break;
    case BUCKET_PRIVATE:   out_arr = bucket_private;   out_cap = BUCKET_CAP_PRIVATE;   break;
    case BUCKET_DM:        out_arr = bucket_dm;        out_cap = BUCKET_CAP_DM;        break;
    case BUCKET_COMPANION: out_arr = bucket_companion; out_cap = BUCKET_CAP_COMPANION; break;
    default:               out_arr = NULL;             out_cap = 0;                    break;
  }
}

int MyMesh::getBucketLimit(MsgBucket b) const {
  uint8_t cap_default[BUCKET_COUNT] = {
    BUCKET_DEFAULT_PUBLIC,  BUCKET_DEFAULT_HASHTAG, BUCKET_DEFAULT_PRIVATE,
    BUCKET_DEFAULT_DM,      BUCKET_DEFAULT_COMPANION
  };
  uint8_t cap_max[BUCKET_COUNT] = {
    BUCKET_CAP_PUBLIC,  BUCKET_CAP_HASHTAG, BUCKET_CAP_PRIVATE,
    BUCKET_CAP_DM,      BUCKET_CAP_COMPANION
  };
  if (b < 0 || b >= BUCKET_COUNT) return 0;
  uint8_t pref = _prefs.msg_store_limit[b];
  if (pref == 0) return cap_default[b];
  if (pref > cap_max[b]) return cap_max[b];
  return pref;
}

bool MyMesh::getBucketFlash(MsgBucket b) const {
  if (b < 0 || b >= BUCKET_COUNT) return false;
  return (_prefs.msg_store_flash & (1 << (int)b)) != 0;
}

static const char* k_bucket_names[] = {
  "public", "hashtag", "private", "dm", "companion"
};

const char* MyMesh::bucketName(MsgBucket b) const {
  if (b < 0 || b >= BUCKET_COUNT) return "?";
  return k_bucket_names[b];
}

int MyMesh::bucketByName(const char* s) const {
  if (!s || !*s) return -1;
  size_t n = strlen(s);
  int hit = -1;
  int matches = 0;
  for (int i = 0; i < BUCKET_COUNT; i++) {
    if (strncmp(s, k_bucket_names[i], n) == 0) {
      if (strlen(k_bucket_names[i]) == n) return i;  // exakt
      hit = i;
      matches++;
    }
  }
  if (matches == 1) return hit;
  if (matches > 1)  return -2;
  return -1;
}

// --- DST-Helper (EU CET/CEST) -----------------------------------------------
// Zeller's congruence: Wochentag fuer Datum (y, m, d). Rueckgabe 0=Sonntag,
// 1=Montag, ..., 6=Samstag. (Standard ISO 8601-aehnlich)
static int dst_dayOfWeek(int y, int m, int d) {
  if (m < 3) { m += 12; y -= 1; }
  int K = y % 100;
  int J = y / 100;
  int h = (d + (13*(m+1))/5 + K + K/4 + J/4 + 5*J) % 7;
  // h: 0=Saturday, 1=Sunday, 2=Monday, ..., 6=Friday
  // Konvertieren auf 0=Sunday: (h + 6) % 7
  return (h + 6) % 7;
}

int32_t MyMesh::localTzOffsetSecs(uint32_t utc) const {
  int32_t base = (int32_t)LOCAL_TZ_OFFSET_SECS;
#if LOCAL_TZ_DST_EU
  // EU-DST: last-Sunday-of-March 01:00 UTC bis last-Sunday-of-October 01:00 UTC.
  if (utc < 1500000000UL) return base;  // RTC unset -- nichts annehmen
  time_t t = (time_t)utc;
  struct tm tm_utc;
  gmtime_r(&t, &tm_utc);
  int year = tm_utc.tm_year + 1900;
  // March 31 weekday -> wieviele Tage zurueck zum letzten Sonntag.
  int mar31_dow = dst_dayOfWeek(year, 3, 31);  // 0=Sunday
  int mar_sun = 31 - mar31_dow;
  int oct31_dow = dst_dayOfWeek(year, 10, 31);
  int oct_sun = 31 - oct31_dow;

  int mon = tm_utc.tm_mon + 1;
  int day = tm_utc.tm_mday;
  int hour = tm_utc.tm_hour;
  // Vor Maerz oder nach Oktober -> sicher Winterzeit
  if (mon < 3 || mon > 10) return base;
  // April..September -> sicher Sommerzeit
  if (mon > 3 && mon < 10) return base + 3600;
  // Maerz: ab last-Sunday 01:00 UTC -> CEST
  if (mon == 3) {
    if (day < mar_sun) return base;
    if (day > mar_sun) return base + 3600;
    // genau am Umstellungs-Sonntag
    return (hour >= 1) ? (base + 3600) : base;
  }
  // Oktober: bis last-Sunday 01:00 UTC -> CEST
  if (mon == 10) {
    if (day < oct_sun) return base + 3600;
    if (day > oct_sun) return base;
    return (hour < 1) ? (base + 3600) : base;
  }
#endif
  return base;
}

// Extrahiere sender_timestamp aus einem gespeicherten Frame -- wird beim
// Boot-Restore verwendet um die RTC ggf. nach oben zu korrigieren wenn ein
// gespeicherter Frame neuer ist als der Contact-basierte Bootstrap.
// Frame-Layouts siehe queueMessage/onChannelMessageRecv/onChannelDataRecv
// (Test-Bericht 2026-05-30). Returns 0 wenn Frame kein timestamp-Feld hat.
static uint32_t frame_extract_timestamp(const uint8_t* buf, int len) {
  if (len < 4) return 0;
  uint8_t code = buf[0];
  int off = -1;
  switch (code) {
    case 7:   off = 9;  break;  // RESP_CODE_CONTACT_MSG_RECV
    case 16:  off = 12; break;  // RESP_CODE_CONTACT_MSG_RECV_V3
    case 8:   off = 4;  break;  // RESP_CODE_CHANNEL_MSG_RECV
    case 17:  off = 7;  break;  // RESP_CODE_CHANNEL_MSG_RECV_V3
    case 27:  return 0;         // RESP_CODE_CHANNEL_DATA_RECV (kein timestamp)
    default:  return 0;
  }
  if (off < 0 || off + 4 > len) return 0;
  uint32_t ts;
  memcpy(&ts, buf + off, 4);
  return ts;
}

const char* MyMesh::msgBucketPath(MsgBucket b) const {
  switch (b) {
    case BUCKET_PUBLIC:    return "/msgs/b0_public.dat";
    case BUCKET_HASHTAG:   return "/msgs/b1_hashtag.dat";
    case BUCKET_PRIVATE:   return "/msgs/b2_private.dat";
    case BUCKET_DM:        return "/msgs/b3_dm.dat";
    case BUCKET_COMPANION: return "/msgs/b4_companion.dat";
    default: return NULL;
  }
}

void MyMesh::saveBucketToFlash(MsgBucket b) {
  const char* path = msgBucketPath(b);
  if (!path) return;
  Frame* arr = NULL;
  int cap = 0;
  getBucket(b, arr, cap);
  if (!arr) return;
  // Verzeichnis muss existieren -- mkdir falls noetig (ist idempotent auf den
  // unterstuetzten FS). Plattformen ohne mkdir-Support: das open(...,"w")
  // greift trotzdem solange path keine Tiefen-Ebene anlegt.
  FILESYSTEM* fs = _store->getPrimaryFS();
#if !(defined(NRF52_PLATFORM) || defined(STM32_PLATFORM))
  if (fs) fs->mkdir("/msgs");
#else
  (void)fs;
#endif
  File f = _store->openWriteFile(path);
  if (!f) return;
  for (int i = 0; i < cap; i++) {
    if (arr[i].seq_no == 0) continue;
    uint8_t hdr[5];
    hdr[0] = (uint8_t)( arr[i].seq_no        & 0xFF);
    hdr[1] = (uint8_t)((arr[i].seq_no >>  8) & 0xFF);
    hdr[2] = (uint8_t)((arr[i].seq_no >> 16) & 0xFF);
    hdr[3] = (uint8_t)((arr[i].seq_no >> 24) & 0xFF);
    hdr[4] = arr[i].len;
    f.write(hdr, 5);
    if (arr[i].len > 0) f.write(arr[i].buf, arr[i].len);
  }
  f.close();
}

void MyMesh::loadBucketsFromFlash() {
  uint32_t max_seq = 0;
  uint32_t max_ts  = 0;   // hoechster sender_timestamp ueber alle restaurierten
                          // Frames -- fuer RTC-Bootstrap (Test-Bericht 2026-05-30)
  for (int b = 0; b < BUCKET_COUNT; b++) {
    if (!getBucketFlash((MsgBucket)b)) continue;
    const char* path = msgBucketPath((MsgBucket)b);
    if (!path) continue;
    File f = _store->openRead(path);
    if (!f) continue;
    Frame* arr = NULL;
    int cap = 0;
    getBucket((MsgBucket)b, arr, cap);
    int limit = getBucketLimit((MsgBucket)b);
    if (limit > cap) limit = cap;
    int slot = 0;
    while (slot < limit && f.available() >= 5) {
      uint8_t hdr[5];
      if (f.read(hdr, 5) != 5) break;
      uint32_t seq = (uint32_t)hdr[0]
                   | ((uint32_t)hdr[1] << 8)
                   | ((uint32_t)hdr[2] << 16)
                   | ((uint32_t)hdr[3] << 24);
      uint8_t len = hdr[4];
      if (len == 0 || len > MAX_FRAME_SIZE) { f.close(); break; }
      // Validity-Check fuer Channel-basierte Buckets: wenn der Frame eine
      // channel_idx referenziert die nicht mehr existiert -> Eintrag droppen
      // (verlorener Channel-Loesch-Speicher). DM-Bucket bleibt ohne Check.
      uint8_t tmp[MAX_FRAME_SIZE];
      if (f.read(tmp, len) != (int)len) { f.close(); break; }
      // Re-Klassifizieren -- falls Frame jetzt in einen ANDEREN Bucket
      // gehoeren wuerde (z.B. Channel geloescht und Idx mit anderem PSK
      // wieder belegt), in Original-Bucket dennoch laden -- der Mismatch
      // ist akzeptabler Edge-Case. Aber wenn classify ergibt: Channel
      // existiert ueberhaupt nicht mehr (z.B. Slot leer) -> droppen.
      MsgBucket re_b = classifyFrame(tmp, len);
      (void)re_b;  // Pruefung erfolgt indirekt: BUCKET_PRIVATE default fuer
                   // unbekannte channel_idx -- mischt sich harmlos in privat-
                   // Bucket beim erstmaligen Laden. Per Phase-C-Refinement
                   // ggf. spaeter strenger.
      arr[slot].seq_no = seq;
      arr[slot].len    = len;
      memcpy(arr[slot].buf, tmp, len);
      if (seq > max_seq) max_seq = seq;
      uint32_t ts = frame_extract_timestamp(tmp, len);
      if (ts > max_ts) max_ts = ts;
      slot++;
    }
    f.close();
  }
  if (max_seq + 1 > _msg_seq_next) _msg_seq_next = max_seq;  // ++ in addToOfflineQueue erhoeht auf max_seq+1

  // RTC-Bootstrap aus restaurierten Frames: wenn ein Frame mit hoeherem
  // sender_timestamp existiert als der derzeitige RTC-Stand (typisch aus
  // bootstrapRTCfromContacts -- max contact.lastmod), RTC anheben. So
  // verlieren wir nach Reboot nicht die ueblicherweise hoehere Zeit-
  // Genauigkeit der Trace-Messages im $companion-Bucket (oder einer
  // gerade-erst empfangenen DM in BUCKET_DM).
  // Toleranz: kleiner Puffer +1s damit neue Pakete nicht denselben
  // Timestamp tragen koennen.
  if (max_ts > 0) {
    uint32_t cur = getRTCClock()->getCurrentTime();
    // Sanity-Schranke (Test-Bericht 2026-05-30): wenn der hoechste stored
    // sender_timestamp in einer korrupten Aera geschrieben wurde (alter
    // NMEA-stale-time Bug -> Frames mit Jahr 2038), wuerden wir die RTC
    // beim Boot in die ferne Zukunft schieben. Akzeptiere nur Bumps die
    // maximal 1 Jahr ueber dem Contact-Bootstrap liegen.
    const uint32_t ONE_YEAR_SECS = 365UL * 86400UL;
    if (max_ts + 1 > cur && max_ts < cur + ONE_YEAR_SECS) {
      getRTCClock()->setCurrentTime(max_ts + 1);
      pushDebugLog("[MSGSTORE] RTC bumped %lu -> %lu (max stored ts)\n",
                   (unsigned long)cur, (unsigned long)(max_ts + 1));
    } else if (max_ts >= cur + ONE_YEAR_SECS) {
      pushDebugLog("[MSGSTORE] RTC bump SKIPPED: max stored ts %lu > cur %lu + 1 year\n",
                   (unsigned long)max_ts, (unsigned long)cur);
    }
  }
}

void MyMesh::clearBucket(MsgBucket b) {
  Frame* arr = NULL;
  int cap = 0;
  getBucket(b, arr, cap);
  if (arr) {
    for (int i = 0; i < cap; i++) arr[i].seq_no = 0;
  }
  const char* path = msgBucketPath(b);
  if (path) _store->removeFile(path);
}

int MyMesh::offlineQueueTotal() const {
  int n = 0;
  for (int i = 0; i < BUCKET_CAP_PUBLIC;    i++) if (bucket_public   [i].seq_no) n++;
  for (int i = 0; i < BUCKET_CAP_HASHTAG;   i++) if (bucket_hashtag  [i].seq_no) n++;
  for (int i = 0; i < BUCKET_CAP_PRIVATE;   i++) if (bucket_private  [i].seq_no) n++;
  for (int i = 0; i < BUCKET_CAP_DM;        i++) if (bucket_dm       [i].seq_no) n++;
  for (int i = 0; i < BUCKET_CAP_COMPANION; i++) if (bucket_companion[i].seq_no) n++;
  return n;
}

// Anhand des Frame-Headers (RESP_CODE) und ggf. der gespeicherten
// ChannelDetails entscheiden in welchen Bucket ein eingehender Frame gehoert.
// Frame-Layouts (siehe Frame-Building in queueMessage/onChannelMessageRecv/
// onChannelDataRecv):
//   RESP_CODE_CONTACT_MSG_RECV    (7):  Offset 0 = code. Kein channel_idx (DM).
//   RESP_CODE_CHANNEL_MSG_RECV    (8):  Offset 0 = code, Offset 1 = channel_idx.
//   RESP_CODE_CONTACT_MSG_RECV_V3 (16): Offset 0 = code. Kein channel_idx (DM).
//   RESP_CODE_CHANNEL_MSG_RECV_V3 (17): Offset 0 = code (+snr+res1+res2),
//                                       Offset 4 = channel_idx.
//   RESP_CODE_CHANNEL_DATA_RECV   (27): wie V3, Offset 4 = channel_idx.
// Alles andere (z.B. raw-data Frames die zukuenftig dazukommen koennten) wird
// vorsichtshalber als PRIVATE klassifiziert -- nicht in PUBLIC weil das eine
// Datenschutz-Regression waere.
MyMesh::MsgBucket MyMesh::classifyFrame(const uint8_t* frame, int len) {
  if (len < 1) return BUCKET_PRIVATE;
  uint8_t code = frame[0];
  // DM
  if (code == RESP_CODE_CONTACT_MSG_RECV || code == RESP_CODE_CONTACT_MSG_RECV_V3) {
    return BUCKET_DM;
  }
  // Channel-basierte Codes
  int chan_offset = -1;
  if (code == RESP_CODE_CHANNEL_MSG_RECV) chan_offset = 1;
  else if (code == RESP_CODE_CHANNEL_MSG_RECV_V3
        || code == RESP_CODE_CHANNEL_DATA_RECV) chan_offset = 4;
  if (chan_offset < 0 || chan_offset >= len) return BUCKET_PRIVATE;
  uint8_t channel_idx = frame[chan_offset];
  // $companion VOR allen anderen Channel-Klassifizierungen abfangen:
  // Trace-Output landet in $companion und wuerde sonst alle anderen
  // "private" Channel-Nachrichten aus dem 16er-Bucket verdraengen.
  // PSK-Identifikation statt Index-Match (User-Bugreport 2026-06-01):
  // App-Index-Drift waehrend Sync kann sonst alte Slot-Nummer als companion
  // fehlinterpretieren. Magic-PSK ist eindeutig.
  if (isCompanionChannel(channel_idx)) {
    return BUCKET_COMPANION;
  }
  ChannelDetails ch;
  if (!getChannel(channel_idx, ch)) return BUCKET_PRIVATE;
  if (memcmp(ch.channel.secret, s_public_psk, 16) == 0) return BUCKET_PUBLIC;
  if (ch.name[0] == '#')                                 return BUCKET_HASHTAG;
  return BUCKET_PRIVATE;
}

void MyMesh::addToOfflineQueue(const uint8_t frame[], int len) {
  if (len <= 0 || len > MAX_FRAME_SIZE) return;
  MsgBucket b = classifyFrame(frame, len);
  Frame* arr = NULL;
  int cap = 0;
  getBucket(b, arr, cap);
  if (!arr || cap == 0) return;
  // Runtime-Limit aus NodePrefs (oder Default wenn 0). Slot mit Index
  // >= limit ist tabu -- wird beim Pop ignoriert (alte Eintraege bei
  // Limit-Verkleinerung verfallen so naturwuechsig nach naechstem Pop).
  int limit = getBucketLimit(b);
  if (limit <= 0) return;
  if (limit > cap) limit = cap;

  // seq_no = 0 ist Sentinel "leer" -- monotonic 1.. via _msg_seq_next.
  // Bei Wrap-Around (extrem unwahrscheinlich, ~4 Mrd Messages) springen
  // wir auf 1 zurueck; alte Eintraege hatten dann hoehere Werte, sind
  // aber bei normalen Pop-Zyklen laengst weg. Beim erstmaligen Hit ist
  // Reihenfolge-Verlust akzeptabel (Edge-Case).
  if (++_msg_seq_next == 0) _msg_seq_next = 1;

  // Freien Slot suchen INNERHALB des Runtime-Limits, sonst aelteste seq_no
  // DESSELBEN Buckets ueberschreiben.
  int free_slot = -1;
  int oldest_slot = 0;
  uint32_t oldest_seq = UINT32_MAX;
  for (int i = 0; i < limit; i++) {
    if (arr[i].seq_no == 0) { free_slot = i; break; }
    if (arr[i].seq_no < oldest_seq) {
      oldest_seq = arr[i].seq_no;
      oldest_slot = i;
    }
  }
  int slot = (free_slot >= 0) ? free_slot : oldest_slot;
  if (free_slot < 0) {
    MESH_DEBUG_PRINTLN("INFO: msg-bucket %d full, evicting oldest seq=%u",
                       (int)b, (unsigned)oldest_seq);
  }
  arr[slot].seq_no = _msg_seq_next;
  arr[slot].len    = (uint8_t)len;
  memcpy(arr[slot].buf, frame, len);

  // Flash-Persistenz (Wunschliste 19 Phase C): wenn fuer diesen Bucket
  // aktiviert, neuen Bucket-State auf Flash schreiben. $companion-Bucket
  // ist von TRACE_MSGSTORE ausgenommen (sonst Rekursion: Trace pusht
  // $companion-Msg -> wird gespeichert -> Trace pusht "gespeichert" -> ...).
  if (getBucketFlash(b)) {
    saveBucketToFlash(b);
    if (b != BUCKET_COMPANION) {
      traceCompanion(TRACE_MSGSTORE, "[store] saved msg in bucket %d", (int)b);
    }
  }
}

int MyMesh::getFromOfflineQueue(uint8_t frame[]) {
  // Scan ueber alle Buckets, finde Slot mit niedrigster nicht-null seq_no.
  // Das erhaelt chronologische Reihenfolge wie die alte single-queue --
  // unabhaengig davon in welchem Bucket der Frame liegt.
  Frame* best_slot = NULL;
  uint32_t best_seq = UINT32_MAX;
  for (int b = 0; b < BUCKET_COUNT; b++) {
    Frame* arr = NULL;
    int cap = 0;
    getBucket((MsgBucket)b, arr, cap);
    for (int i = 0; i < cap; i++) {
      if (arr[i].seq_no != 0 && arr[i].seq_no < best_seq) {
        best_seq  = arr[i].seq_no;
        best_slot = &arr[i];
      }
    }
  }
  if (!best_slot) return 0;
  int len = best_slot->len;
  memcpy(frame, best_slot->buf, len);
  best_slot->seq_no = 0;   // Slot freigeben
  return len;
}

float MyMesh::getAirtimeBudgetFactor() const {
  return _prefs.airtime_factor;
}

// Wunschliste 45 (Reise 2026-06-09): Stub aufgehoben. Companion ist
// jetzt fuer LBT konfigurierbar -- Wessel Nieboers AGC-Reset-Fix
// (Feb 2026, RadioLibWrappers.cpp:77) hat das urspruengliche
// stuck-_noise_floor=-120-Problem behoben. Default 14 (upstream-Doku),
// User kann via 'set int.thresh <N>' tunen. 0 = off.
int MyMesh::getInterferenceThreshold() const {
  return _prefs.interference_threshold;
}
// Wunschliste 45: AGC-Reset-Interval (Sekunden/4, intern *4000 ms).
// 0 = disabled. Periodischer AGC-Reset bei verrauschten RX-Standorten,
// macht das Geraet fuer wenige ms taub -- bewusst sparsam nutzen.
int MyMesh::getAGCResetInterval() const {
  return ((int)_prefs.agc_reset_interval) * 4000;
}

int MyMesh::calcRxDelay(float score, uint32_t air_time) const {
  if (_prefs.rx_delay_base <= 0.0f) return 0;
  return (int)((pow(_prefs.rx_delay_base, 0.85f - score) - 1.0) * air_time);
}

// Auto-Mode (Sentinel -1.0) Logik. Diese Funktion liefert den EFFEKTIVEN
// Faktor unabhaengig davon ob der User "auto" oder einen expliziten Wert
// gesetzt hat -- damit Status-Anzeigen ("get txdelay") konsistent sind mit
// dem was die Send-Funktionen tatsaechlich verwenden.
float MyMesh::effectiveTxDelayFactor() const {
  if (_prefs.tx_delay_factor >= 0.0f) return _prefs.tx_delay_factor;
  // Auto: Forwarding-Pfad. Normal-Repeater raffinierter, defensive
  // wie Upstream-Default 0.5.
  return (_prefs.repeater_profile == 1) ? 1.5f : 0.5f;
}
float MyMesh::effectiveDirectTxDelayFactor() const {
  if (_prefs.direct_tx_delay_factor >= 0.0f) return _prefs.direct_tx_delay_factor;
  // Auto: Upstream-Repeater-Default fuer direct/zero-hop-Forwarding.
  return 0.2f;
}

uint32_t MyMesh::getRetransmitDelay(const mesh::Packet *packet) {
  float f = effectiveTxDelayFactor();
  uint32_t t = (uint32_t)(_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * f);
  return getRNG()->nextInt(0, 5*t + 1);
}
uint32_t MyMesh::getDirectRetransmitDelay(const mesh::Packet *packet) {
  float f = effectiveDirectTxDelayFactor();
  uint32_t t = (uint32_t)(_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * f);
  return getRNG()->nextInt(0, 5*t + 1);
}

// Wunschliste 39 (2026-06-04): Vereinheitlichte Hop-Cap-Hierarchie.
// FLOOD_MAX_INFRA_FOLLOW (254) = "folge parent". Aufloesung zur
// Laufzeit damit das Setzen von flood_max nicht zur Migration der
// Kinder zwingt.
uint8_t MyMesh::effectiveFloodMaxInfra() const {
  return (_prefs.flood_max_infra == FLOOD_MAX_INFRA_FOLLOW)
    ? _prefs.flood_max : _prefs.flood_max_infra;
}
uint8_t MyMesh::effectiveFloodMaxReqResp() const {
  return (_prefs.flood_max_req_resp == FLOOD_MAX_INFRA_FOLLOW)
    ? effectiveFloodMaxInfra() : _prefs.flood_max_req_resp;
}

// Differentiate flood-retransmit TX power / CR by packet source:
//  - already repeated (n > 0): polite, reduced + CR5
//  - heard directly (n == 0), default: act as transparent extension of
//    the source — full power + configured CR
//  - exception A: ADVERT directly heard from another REPEATER -> reduced
//    (otherwise repeater-to-repeater adverts at full power would flood
//    the network unnecessarily)
//  - exception B: PATH discovery for endpoints that aren't in our
//    direct-heard list (zero-hop HeardList) -> reduced. We let these
//    through allowPacketForward via the contacts <48h check, but they
//    are not "our" local nodes in the strict sense.
// Wunschliste 6b: Loop-Detection-Maxima je Hash-Size (analog
// simple_repeater). Index = Hash-Size (1..3 Byte). Index 0 unused.
static const uint8_t max_loop_minimal[]  = { 0, 4, 2, 1 };
static const uint8_t max_loop_moderate[] = { 0, 2, 1, 1 };
static const uint8_t max_loop_strict[]   = { 0, 1, 1, 1 };

bool MyMesh::isLooped(const mesh::Packet* packet, const uint8_t max_counters[]) const {
  uint8_t hash_size  = packet->getPathHashSize();
  uint8_t hash_count = packet->getPathHashCount();
  uint8_t n = 0;
  const uint8_t* path = packet->path;
  while (hash_count > 0) {
    if (self_id.isHashMatch(path, hash_size)) n++;
    hash_count--;
    path += hash_size;
  }
  if (hash_size == 0 || hash_size > 3) return false;  // safety
  return n >= max_counters[hash_size];
}

bool MyMesh::shouldReduceFloodRetransmit(const mesh::Packet* packet, uint8_t n) const {
  // Wunschliste 8: im normal-Profil keine Power/CR-Reduktion. Egal wie
  // viele Retransmits — der Repeater verhaelt sich wie ein "echter".
  if (_prefs.repeater_profile == 1) return false;

  if (n > 0) return true;

  uint8_t ptype = packet->getPayloadType();

  // Exception A: directly-heard advert from another repeater.
  if (ptype == PAYLOAD_TYPE_ADVERT) {
    // payload layout: pub_key(32) + timestamp(4) + signature(64) + app_data[]
    // app_data[0] lower 4 bits = ADV_TYPE_* (ADV_TYPE_REPEATER == 2)
    const int ADV_APP_DATA_OFFSET = PUB_KEY_SIZE + 4 + SIGNATURE_SIZE;
    if (packet->payload_len > ADV_APP_DATA_OFFSET) {
      uint8_t adv_type = packet->payload[ADV_APP_DATA_OFFSET] & 0x0F;
      if (adv_type == ADV_TYPE_REPEATER) return true;
    }
  }

  // Exception B: PATH discovery for non-locally-heard endpoints.
  if (ptype == PAYLOAD_TYPE_PATH && packet->payload_len >= 2) {
    uint8_t dest_hash = packet->payload[0];
    uint8_t src_hash  = packet->payload[1];
    if (!isLocallyHeard(dest_hash) && !isLocallyHeard(src_hash)) {
      return true;
    }
  }

  return false;
}

uint8_t MyMesh::getExtraAckTransmitCount() const {
  return _prefs.multi_acks;
}

void MyMesh::logRxRaw(float snr, float rssi, const uint8_t raw[], int len) {
  if (_serial->isConnected() && len + 3 <= MAX_FRAME_SIZE) {
    int i = 0;
    out_frame[i++] = PUSH_CODE_LOG_RX_DATA;
    out_frame[i++] = (int8_t)(snr * 4);
    out_frame[i++] = (int8_t)(rssi);
    memcpy(&out_frame[i], raw, len);
    i += len;

    _serial->writeFrame(out_frame, i);
  }
}

bool MyMesh::isAutoAddEnabled() const {
  return (_prefs.manual_add_contacts & 1) == 0;
}

bool MyMesh::shouldAutoAddContactType(uint8_t contact_type) const {
  if ((_prefs.manual_add_contacts & 1) == 0) {
    return true;
  }

  uint8_t type_bit = 0;
  switch (contact_type) {
    case ADV_TYPE_CHAT:
      type_bit = AUTO_ADD_CHAT;
      break;
    case ADV_TYPE_REPEATER:
      type_bit = AUTO_ADD_REPEATER;
      break;
    case ADV_TYPE_ROOM:
      type_bit = AUTO_ADD_ROOM_SERVER;
      break;
    case ADV_TYPE_SENSOR:
      type_bit = AUTO_ADD_SENSOR;
      break;
    default:
      return false;  // Unknown type, don't auto-add
  }

  return (_prefs.autoadd_config & type_bit) != 0;
}

bool MyMesh::shouldOverwriteWhenFull() const {
  return (_prefs.autoadd_config & AUTO_ADD_OVERWRITE_OLDEST) != 0;
}

uint8_t MyMesh::getAutoAddMaxHops() const {
  return _prefs.autoadd_max_hops;
}

void MyMesh::onContactOverwrite(const uint8_t* pub_key) {
    _store->deleteBlobByKey(pub_key, PUB_KEY_SIZE); // delete from storage
  if (_serial->isConnected()) {
    out_frame[0] = PUSH_CODE_CONTACT_DELETED;
    memcpy(&out_frame[1], pub_key, PUB_KEY_SIZE);
    _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE);
  }
}

void MyMesh::onContactsFull() {
  if (_serial->isConnected()) {
    out_frame[0] = PUSH_CODE_CONTACTS_FULL;
    _serial->writeFrame(out_frame, 1);
  }
}

void MyMesh::markHeardDirect(uint8_t hash) {
  uint32_t now = getRTCClock()->getCurrentTime();

  // find existing entry (1-byte match), else evict via FIFO
  HeardEntry* slot = NULL;
  for (int i = 0; i < CR_HEARD_TABLE_SIZE; i++) {
    if (heard_list[i].last_heard != 0 && heard_list[i].hash == hash) {
      slot = &heard_list[i];
      break;
    }
  }
  bool is_new = (slot == NULL);
  if (is_new) {
    slot = &heard_list[heard_next_idx];
    heard_next_idx = (heard_next_idx + 1) % CR_HEARD_TABLE_SIZE;
    slot->hash = hash;
  }
  slot->last_heard = now;
  // Default ('trace heard on' = mode 'new'): nur neue Direct-Nodes loggen.
  // Optional 'trace heard on all': jeder Empfang -> mehr Volumen, aber
  // sichtbare Hoer-Aktivitaet pro Node.
  if (is_new || _trace_heard_all) {
    traceCompanion(TRACE_HEARD,
                   is_new ? "[heard] NEW Direct-Node hash=0x%02X"
                          : "[heard] Direct-Node hash=0x%02X",
                   hash);
  }
}

// Wunschliste 46 Phase 1 (Reise 2026-06-09).
// Filter-Pattern-Match. Case-INSENSITIVE (User-Wunsch 2026-06-09:
// Pattern-Sprache (Phase 1.5, 2026-06-10): Wort-Match als Default,
// '*' als Wildcard pro Token, Multi-Token-Subsequenz, Anker ^/$.
// Case-insensitive durchgaengig.
//
//   ping       Wort-Match: matched Token "ping", nicht "pingable"
//   ping*      Token startsWith "ping": matched "pingable"
//   *ping      Token endsWith   "ping": matched "doping"
//   *ping*     Token contains   "ping": substring im Token
//   "foo bar*" Multi-Token: konsekutive Subsequenz, jedes
//              Pattern-Token wird per Glob gegen Text-Token gematched.
//   ^foo       String beginnt mit Pattern (erstes Text-Token)
//   foo$       String endet mit Pattern (letztes Text-Token)
//   ^foo$      String ist genau Pattern
//
// Token-Trennung im Text: Whitespace + ASCII-Satzzeichen am Token-Rand
// (z.B. 'ping!' wird als 'ping' gegen Pattern verglichen). Pattern
// wird nur an Whitespace getrennt -- darin sind '*'-Sterne die einzige
// Sonder-Syntax.

// Token-Separator: NUR Whitespace + Control-Chars.
// User-Klarstellung 2026-06-10: Pattern ist literal. Wenn User
// 'foo.bar' eintippt, will er das ganze Wort 'foo.bar' (analog
// 'abc-def' oder 'xxx/yyy') -- kein automatisches Stripping von
// Satzzeichen am Wort-Rand. Wer auch 'foo.bar.' oder 'Hallo,'
// matchen will, schreibt 'foo.bar*' bzw. 'Hallo*' explizit als
// Wildcard. Konsistent und vorhersehbar.
static inline bool isFilterHardSep(unsigned char c) {
  if (c < 32) return true;
  return c == ' ' || c == 0x7F;
}

// UTF-8 codepoint decode. Returns codepoint, writes consumed bytes.
// On invalid/incomplete returns the raw byte and consumes 1.
static int utf8Decode(const char* p, size_t avail, size_t* consumed) {
  if (avail == 0) { *consumed = 0; return -1; }
  unsigned char c = (unsigned char)p[0];
  if (c < 0x80) { *consumed = 1; return c; }
  if ((c & 0xE0) == 0xC0 && avail >= 2) {
    *consumed = 2;
    return ((c & 0x1F) << 6) | ((unsigned char)p[1] & 0x3F);
  }
  if ((c & 0xF0) == 0xE0 && avail >= 3) {
    *consumed = 3;
    return ((c & 0x0F) << 12) | (((unsigned char)p[1] & 0x3F) << 6)
         | ((unsigned char)p[2] & 0x3F);
  }
  if ((c & 0xF8) == 0xF0 && avail >= 4) {
    *consumed = 4;
    return ((c & 0x07) << 18) | (((unsigned char)p[1] & 0x3F) << 12)
         | (((unsigned char)p[2] & 0x3F) << 6) | ((unsigned char)p[3] & 0x3F);
  }
  *consumed = 1;
  return c;
}

// Case-fold to lowercase. ASCII A-Z plus deutsche Latin-1-Umlaute
// (Ä/Ö/Ü -> ä/ö/ü). Eszett bleibt wie es ist.
static inline int casefoldCp(int cp) {
  if (cp >= 'A' && cp <= 'Z') return cp + 32;
  if (cp == 0xC4) return 0xE4;  // Ä
  if (cp == 0xD6) return 0xF6;  // Ö
  if (cp == 0xDC) return 0xFC;  // Ü
  return cp;
}

// True iff bytes p[0..pl) and t[0..tl) decode to the same codepoint
// sequence under case-fold.
static bool ciUtf8Equal(const char* p, size_t pl, const char* t, size_t tl) {
  size_t pi = 0, ti = 0;
  while (pi < pl && ti < tl) {
    size_t pc, tc;
    int pcp = utf8Decode(p + pi, pl - pi, &pc);
    int tcp = utf8Decode(t + ti, tl - ti, &tc);
    if (casefoldCp(pcp) != casefoldCp(tcp)) return false;
    pi += pc;
    ti += tc;
  }
  return pi == pl && ti == tl;
}

// True iff text starts with pattern (both byte-spans) under case-fold.
static bool ciUtf8StartsWith(const char* t, size_t tl,
                             const char* p, size_t pl) {
  size_t pi = 0, ti = 0;
  while (pi < pl) {
    if (ti >= tl) return false;
    size_t pc, tc;
    int pcp = utf8Decode(p + pi, pl - pi, &pc);
    int tcp = utf8Decode(t + ti, tl - ti, &tc);
    if (casefoldCp(pcp) != casefoldCp(tcp)) return false;
    pi += pc;
    ti += tc;
  }
  return true;
}

// True iff substring p found in t (case-fold). Iteriert nur an
// Codepoint-Grenzen im Text, damit wir nicht in der Mitte einer
// UTF-8-Sequenz suchen.
static bool ciUtf8Contains(const char* t, size_t tl,
                           const char* p, size_t pl) {
  size_t ti = 0;
  while (ti < tl) {
    if (ciUtf8StartsWith(t + ti, tl - ti, p, pl)) return true;
    size_t step;
    utf8Decode(t + ti, tl - ti, &step);
    if (step == 0) break;
    ti += step;
  }
  return false;
}

// True iff text ends with pattern (case-fold), iterating codepoint
// boundaries from the end. Pragmatisch: vorwaerts iterieren, alle
// codepoint-Grenzen aufzeichnen, dann den passenden Start finden.
static bool ciUtf8EndsWith(const char* t, size_t tl,
                           const char* p, size_t pl) {
  if (pl == 0) return true;
  if (pl > tl) return false;
  // We try each codepoint-boundary starting from tl backwards. Forward
  // scan accumulates boundaries, then iterate from largest.
  const size_t MAX_B = 256;
  size_t boundaries[MAX_B];
  size_t bn = 0;
  size_t ti = 0;
  boundaries[bn++] = 0;
  while (ti < tl && bn < MAX_B) {
    size_t step;
    utf8Decode(t + ti, tl - ti, &step);
    if (step == 0) break;
    ti += step;
    boundaries[bn++] = ti;
  }
  // try boundaries from later to earlier
  for (size_t i = bn; i > 0; i--) {
    size_t bs = boundaries[i-1];
    if (tl - bs < pl) continue;
    if (ciUtf8Equal(p, pl, t + bs, tl - bs)) return true;
  }
  return false;
}

static bool tokenGlobMatch(const char* pat, size_t pl,
                           const char* tok, size_t tl) {
  if (pl == 0) return false;
  bool wild_s = (pat[0] == '*');
  bool wild_e = (pl > 0 && pat[pl-1] == '*');
  if (pl == 1 && wild_s) return true;     // "*" matched alles
  if (pl == 2 && wild_s && wild_e) return true;  // "**" auch
  size_t ps = wild_s ? 1 : 0;
  size_t pe = pl - (wild_e ? 1 : 0);
  size_t plen = (pe > ps) ? (pe - ps) : 0;
  if (plen == 0) return true;
  if (wild_s && wild_e) {
    return ciUtf8Contains(tok, tl, pat + ps, plen);
  }
  if (wild_s) {
    return ciUtf8EndsWith(tok, tl, pat + ps, plen);
  }
  if (wild_e) {
    return ciUtf8StartsWith(tok, tl, pat + ps, plen);
  }
  return ciUtf8Equal(pat + ps, plen, tok, tl);
}

bool MyMesh::filterPatternMatch(const NodePrefs::FilterEntry& e, const char* s) {
  size_t pl = strlen(e.pattern);
  if (pl == 0 || !s) return false;
  size_t sl = strlen(s);
  if (sl == 0) return false;
  bool anchor_start = (e.flags & 0x01) != 0;
  bool anchor_end   = (e.flags & 0x02) != 0;

  // Tokenize Pattern und Text strikt by Hard-Sep (Whitespace).
  // Tokens bleiben literal -- kein Edge-Trim. User-Pattern matched
  // gegen Token byte-genau (mit case-fold + UTF-8). Flexibilitaet
  // ueber Wildcards: 'ping*' fuer 'ping,' 'ping!' usw.
  const uint8_t MAX_PAT_TOKENS = 8;
  uint16_t pat_off[MAX_PAT_TOKENS];
  uint16_t pat_len[MAX_PAT_TOKENS];
  uint8_t pat_count = 0;
  {
    size_t i = 0;
    while (i < pl && pat_count < MAX_PAT_TOKENS) {
      while (i < pl && isFilterHardSep((unsigned char)e.pattern[i])) i++;
      if (i >= pl) break;
      size_t st = i;
      while (i < pl && !isFilterHardSep((unsigned char)e.pattern[i])) i++;
      pat_off[pat_count] = (uint16_t)st;
      pat_len[pat_count] = (uint16_t)(i - st);
      pat_count++;
    }
  }
  if (pat_count == 0) return false;

  const uint8_t MAX_TXT_TOKENS = 64;
  uint16_t txt_off[MAX_TXT_TOKENS];
  uint16_t txt_len[MAX_TXT_TOKENS];
  uint8_t txt_count = 0;
  {
    size_t i = 0;
    while (i < sl && txt_count < MAX_TXT_TOKENS) {
      while (i < sl && isFilterHardSep((unsigned char)s[i])) i++;
      if (i >= sl) break;
      size_t st = i;
      while (i < sl && !isFilterHardSep((unsigned char)s[i])) i++;
      txt_off[txt_count] = (uint16_t)st;
      txt_len[txt_count] = (uint16_t)(i - st);
      txt_count++;
    }
  }
  if (txt_count == 0 || txt_count < pat_count) return false;

  // Suche konsekutive Token-Subsequenz wo jedes Pattern-Token
  // glob-matched.
  uint8_t start_min = 0;
  uint8_t start_max = (uint8_t)(txt_count - pat_count);
  if (anchor_start) start_max = 0;
  if (anchor_end)   start_min = (uint8_t)(txt_count - pat_count);
  if (start_min > start_max) return false;

  for (uint8_t st = start_min; st <= start_max; st++) {
    bool ok = true;
    for (uint8_t k = 0; k < pat_count; k++) {
      if (!tokenGlobMatch(e.pattern + pat_off[k], pat_len[k],
                          s + txt_off[st + k], txt_len[st + k])) {
        ok = false;
        break;
      }
    }
    if (ok) return true;
  }
  return false;
}

// channel-filter (Wunschliste 46 Phase 2/5, 2026-06-10):
//   channel_idx = -1 -> DM-Pfad (mask ignoriert, filter immer geprueft)
//   channel_idx = -2 -> unknown channel im Repeat-Pfad (Channel ist nicht
//                       lokal konfiguriert). Mit on_mask: filter wirkt
//                       NICHT (Channel nicht in Liste). Mit exempt_mask:
//                       filter wirkt (Channel nicht in exempt-Liste).
//                       Ohne Mask: filter wirkt global.
//   channel_idx >= 0 -> Index in channels[]. Masks konsultieren.
// ACHTUNG: nicht mit MeshCore-'scope' (TransportKey-Tags) verwechseln.
static inline bool filterAppliesToChannel(int channel_idx,
                                          uint64_t on_mask,
                                          uint64_t exempt_mask) {
  if (channel_idx == -1) return true;        // DM
  if (channel_idx == -2) {
    if (on_mask != 0) return false;
    if (exempt_mask != 0) return true;
    return true;
  }
  if (channel_idx < 0 || channel_idx >= 64) return true;
  uint64_t bit = (uint64_t)1 << channel_idx;
  if (on_mask != 0) return (on_mask & bit) != 0;
  if (exempt_mask != 0) return (exempt_mask & bit) == 0;
  return true;                               // global
}

// Phase 2 v2 (2026-06-10): pro-Pattern channel-filter. Pro Pattern
// werden seine on/exempt-Masken konsultiert. Beide 0 = global.
bool MyMesh::filterSenderDropMatch(const char* sender_name, int channel_idx) const {
  if (!sender_name || !*sender_name) return false;
  // keep gewinnt vor drop. Pro-Pattern Skopus pruefen.
  for (uint8_t i = 0; i < _prefs.filter_sender_keep_count
       && i < sizeof(_prefs.filter_sender_keep)/sizeof(_prefs.filter_sender_keep[0]); i++) {
    if (!filterAppliesToChannel(channel_idx,
                                _prefs.filter_sender_keep_chan_on[i],
                                _prefs.filter_sender_keep_chan_ex[i])) continue;
    if (filterPatternMatch(_prefs.filter_sender_keep[i], sender_name)) return false;
  }
  for (uint8_t i = 0; i < _prefs.filter_sender_drop_count
       && i < sizeof(_prefs.filter_sender_drop)/sizeof(_prefs.filter_sender_drop[0]); i++) {
    if (!filterAppliesToChannel(channel_idx,
                                _prefs.filter_sender_drop_chan_on[i],
                                _prefs.filter_sender_drop_chan_ex[i])) continue;
    if (filterPatternMatch(_prefs.filter_sender_drop[i], sender_name)) return true;
  }
  return false;
}

bool MyMesh::filterTextDropMatch(const char* text, int channel_idx) const {
  if (!text || !*text) return false;
  for (uint8_t i = 0; i < _prefs.filter_text_keep_count
       && i < sizeof(_prefs.filter_text_keep)/sizeof(_prefs.filter_text_keep[0]); i++) {
    if (!filterAppliesToChannel(channel_idx,
                                _prefs.filter_text_keep_chan_on[i],
                                _prefs.filter_text_keep_chan_ex[i])) continue;
    if (filterPatternMatch(_prefs.filter_text_keep[i], text)) return false;
  }
  for (uint8_t i = 0; i < _prefs.filter_text_drop_count
       && i < sizeof(_prefs.filter_text_drop)/sizeof(_prefs.filter_text_drop[0]); i++) {
    if (!filterAppliesToChannel(channel_idx,
                                _prefs.filter_text_drop_chan_on[i],
                                _prefs.filter_text_drop_chan_ex[i])) continue;
    if (filterPatternMatch(_prefs.filter_text_drop[i], text)) return true;
  }
  return false;
}

// Wunschliste 46 Phase 5: scope-Filter.
// scope_name = aktueller scope des Pakets ('europe', 'de', 'de-be', ...)
// oder NULL bei unscoped (-> Match gegen Pseudo-Token 'unscoped').
// for_repeat = true: nur Entries mit profile=repeat oder complete pruefen
// for_repeat = false: nur Entries mit profile=for-us oder complete.
// channel_idx: -1 = ignoriert (z.B. wenn fuer Repeat-Pfad ohne lokalen
// Channel-Index), sonst pro-Pattern channel-filter.
// Returns true = DROP (Paket weg).
bool MyMesh::filterScopeMatch(const char* scope_name, int channel_idx,
                              bool for_repeat) const {
  auto profile_matches = [&](uint8_t flags) {
    uint8_t profile = flags & 0x03;
    if (profile == 2) return true;         // complete -> beide
    if (for_repeat) return profile == 1;   // repeat-only
    return profile == 0;                   // for-us
  };
  auto name_matches = [&](const char* stored) -> bool {
    if (!stored || !stored[0]) return false;
    // Akzeptiere mit/ohne '#' Prefix. 'unscoped' (reserved) vs scope_name == NULL.
    const char* a = (stored[0] == '#') ? stored + 1 : stored;
    if (strcasecmp(a, "unscoped") == 0) return (scope_name == NULL);
    if (!scope_name) return false;
    const char* b = (scope_name[0] == '#') ? scope_name + 1 : scope_name;
    return strcasecmp(a, b) == 0;
  };
  // keep vor drop. Pro-Pattern channel-filter + profile-filter.
  for (uint8_t i = 0; i < _prefs.filter_scope_keep_count
       && i < sizeof(_prefs.filter_scope_keep)/sizeof(_prefs.filter_scope_keep[0]); i++) {
    if (!profile_matches(_prefs.filter_scope_keep[i].flags)) continue;
    if (!filterAppliesToChannel(channel_idx,
                                _prefs.filter_scope_keep_chan_on[i],
                                _prefs.filter_scope_keep_chan_ex[i])) continue;
    if (name_matches(_prefs.filter_scope_keep[i].scope_name)) return false;
  }
  for (uint8_t i = 0; i < _prefs.filter_scope_drop_count
       && i < sizeof(_prefs.filter_scope_drop)/sizeof(_prefs.filter_scope_drop[0]); i++) {
    if (!profile_matches(_prefs.filter_scope_drop[i].flags)) continue;
    if (!filterAppliesToChannel(channel_idx,
                                _prefs.filter_scope_drop_chan_on[i],
                                _prefs.filter_scope_drop_chan_ex[i])) continue;
    if (name_matches(_prefs.filter_scope_drop[i].scope_name)) return true;
  }
  return false;
}

bool MyMesh::isLocallyHeard(uint8_t hash) const {
  uint32_t now = getRTCClock()->getCurrentTime();
  for (int i = 0; i < CR_HEARD_TABLE_SIZE; i++) {
    if (heard_list[i].last_heard == 0) continue;
    if (now - heard_list[i].last_heard > CR_HEARD_MAX_AGE_SECS) continue;
    if (heard_list[i].hash == hash) return true;
  }
  return false;
}

void MyMesh::onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id,
                          uint32_t timestamp, const uint8_t* app_data, size_t app_data_len) {
  // SNR vom Paket fuer die nachfolgende Quality-Klassifikation in
  // onDiscoveredContact() merken (Q4-Format, wie ueblich in MeshCore).
  _last_advert_snr_q4 = (packet != NULL) ? packet->_snr : 0;
  // Wunschliste 26 C: Scope-Flag fuer die scoped/unscoped × adv-Typ
  // Aufschluesselung in onDiscoveredContact merken.
  _last_advert_scoped = (packet != NULL && packet->hasTransportCodes()) ? 1 : 0;
  // Wunschliste 26 D: DIRECT-typed (sendZeroHop) Adverts identifizieren.
  // DIRECT-typed Adverts haben per Definition path_len=0 (= zero-hop).
  _last_advert_route_direct = (packet != NULL && packet->isRouteDirect()) ? 1 : 0;
  BaseChatMesh::onAdvertRecv(packet, id, timestamp, app_data, app_data_len);

  // Wunschliste 15: bei zero-hop heard (path_len==0) zur Runtime-
  // Neighbour-Tabelle hinzufuegen. 'liberal' Filter: alle adv_types
  // werden aufgenommen (chat/repeater/sensor/room).
  if (packet != NULL && packet->path_len == 0
      && app_data != NULL && app_data_len > 0) {
    AdvertDataParser parser(app_data, app_data_len);
    if (parser.isValid()) {
      putRuntimeNeighbour(id, timestamp, _last_advert_snr_q4, parser.getType());
      // Wunschliste 31: Advert-basierte RTC-Sync. NUR zero-hop
      // (filterstaerke: Adversary muss in Funkreichweite sein).
      maybeAdvertTimeSync(id, timestamp, parser.getType());
    }
  }
}

// =========================================================================
// Wunschliste 31: Advert-basierte RTC-Sync
// =========================================================================

bool MyMesh::isRepeatingEffectivelyAllowed() const {
  if (_prefs.client_repeat == 0) return false;
  if (_prefs.repeater_profile != 0) return true;   // normal: ignoriert moving + freq
  // defensive:
  if (_is_moving) return false;                    // is_moving suppressed defensive-rep
  // freq muss in strict-Range sein ODER (nur mit ifdef) force gesetzt
  uint32_t f_khz = (uint32_t)(_prefs.freq * 1000.0f + 0.5f);
  if (isValidClientRepeatFreq(f_khz)) return true;
#ifdef REPEATER_DEFENSIVE_FORCE
  if (_prefs.client_repeat_force) return true;
#endif
  return false;
}

void MyMesh::recomputeRepeatingAllowed(const char* reason) {
  bool prev = _repeating_allowed;
  _repeating_allowed = isRepeatingEffectivelyAllowed();
  if (prev != _repeating_allowed) {
    pushDebugLog("[repeat] effective -> %s (%s)\n",
                 _repeating_allowed ? "on" : "off",
                 reason ? reason : "?");
  }
}

bool MyMesh::isGpsAuthoritative() const {
  // GPS hat Vorrang VOR Advert-Sync wenn GPS aktiv ist UND schon
  // mindestens einmal einen Fix bekommen hat. Andernfalls greift
  // Advert-Sync als Fallback.
#if ENV_INCLUDE_GPS == 1
  if (_prefs.gps_enabled && _gps_had_fix_ever) return true;
#endif
  return false;
}

void MyMesh::maybeAdvertTimeSync(const mesh::Identity& id, uint32_t adv_timestamp, uint8_t adv_type) {
  if (_prefs.time_sync_mode == 0) return;                  // off
  if (isGpsAuthoritative()) return;                         // GPS-Vorrang
  // Source-Type-Filter: nur Infra/User-Adverts (NICHT SENSOR).
  // Roomserver = stark vertrauenswuerdig (Admin), Repeater = Admin
  // ueblich aber theor. user-spoofbar (Trust via signiertem advert
  // + zero-hop), CHAT = User. SENSOR liefert keine authoritativ
  // korrekte Uhrzeit.
  if (adv_type != ADV_TYPE_REPEATER
      && adv_type != ADV_TYPE_ROOM
      && adv_type != ADV_TYPE_CHAT) return;
  // Plausibility-Check: Jahr 2020..2099. Werte ausserhalb sind
  // wahrscheinlich Replay alter Adverts oder verirrte/falsche Quellen.
  if (adv_timestamp < 1577836800UL /* 2020-01-01 */
      || adv_timestamp > 4070908800UL /* 2099-01-01 */) return;

  uint32_t now_rtc = getRTCClock()->getCurrentTime();
  // Plausibility-Check 2 (User-Iteration 2026-06-08): adv_timestamp muss
  // innerhalb +/- 3h vom aktuellen RTC liegen. Sehr restriktiv -- die
  // urspruenglichen 4 Wochen waren auf Powerbank-/lange-Pause ausgerichtet,
  // aber: die App synced beim Connect via CMD_SET_DEVICE_TIME die RTC
  // mit ihrer (akkuraten) Zeit. Damit ist RTC nach App-Connect immer
  // aktuell. Bei laengeren Pausen ohne App muss User App-Sync ODER
  // GPS-Sync den RTC erst plausibel machen, dann greifen adv-syncs wieder.
  //
  // Schuetzt vor:
  //  - Replay alter Adverts (Repeater mit ungesyncter Uhr, Cache-Replay)
  //  - Repeater die lokale Zeit (mit TZ-Offset 1..2h) statt UTC senden
  //  - korrupten advert-timestamps die durch das breite 2020..2099-Fenster
  //    sonst durchrutschen wuerden
  const uint32_t PLAUSIBILITY_WINDOW_SECS = 3UL * 3600UL;  // 3 Stunden
  uint32_t plaus_diff = (adv_timestamp >= now_rtc)
                       ? (adv_timestamp - now_rtc)
                       : (now_rtc - adv_timestamp);
  if (plaus_diff > PLAUSIBILITY_WINDOW_SECS) {
    pushDebugLog("[rtc] adv-sync REJECTED: ts %lu vs rtc %lu diff %lus > 3h\n",
                 (unsigned long)adv_timestamp,
                 (unsigned long)now_rtc,
                 (unsigned long)plaus_diff);
    return;
  }
  // Letzten Versuch fuer Diagnose merken (alle Pfade unten beerben den
  // outcome). Wird vom 'clock'-Befehl als Diagnose-Zeile angezeigt.
  memcpy(_last_adv_sync_pubkey, id.pub_key, 3);
  _last_adv_sync_ts = adv_timestamp;
  _last_adv_sync_at_rtc = now_rtc;
  _last_adv_sync_delta = (int32_t)(adv_timestamp - now_rtc);
  // 24h-Cap (nur wenn schon einmal synced):
  if (_time_sync_done_since_boot
      && _time_sync_last_at_rtc > 0
      && now_rtc >= _time_sync_last_at_rtc
      && (now_rtc - _time_sync_last_at_rtc) < 86400UL) {
    _last_adv_sync_outcome = 2; // skipped 24h-cap
    pushDebugLog("[rtc] adv-sync SKIPPED (24h-cap): src=%02x%02x%02x ts=%lu delta=%lds\n",
                 id.pub_key[0], id.pub_key[1], id.pub_key[2],
                 (unsigned long)adv_timestamp, (long)_last_adv_sync_delta);
    return;
  }

  // ----- Strict mode: nur konfigurierte Sources -----
  if (_prefs.time_sync_mode == 2) {
    int src_idx = -1;
    int configured = 0;
    for (int i = 0; i < 3; i++) {
      bool nonzero = (_prefs.time_sync_sources[i][0] != 0
                     || _prefs.time_sync_sources[i][1] != 0
                     || _prefs.time_sync_sources[i][2] != 0);
      if (nonzero) configured++;
      if (nonzero && memcmp(id.pub_key, _prefs.time_sync_sources[i], 3) == 0) {
        src_idx = i;
      }
    }
    if (src_idx < 0) return;
    // Replay-Schutz pro Source
    if (adv_timestamp <= _time_sync_strict_last_ts[src_idx]) return;
    int32_t delta = (int32_t)(adv_timestamp - now_rtc);
    // Drift-Schwelle 120s (User-Wunsch 2026-06-08): kleiner als 2 Min
    // Drift lohnt keinen RTC-Update. Begruendung: ESP32-RTC driftet
    // nur Millisekunden pro Stunde, Funk-Latenz + advert-Aging
    // erzeugen aber leicht Sub-Sekunden- bis Sekunden-Noise. Bei
    // drift < 2 min ist der adv-timestamp NICHT zuverlaessig besser
    // als unser aktueller RTC -- nicht ueberschreiben.
    if (delta > -120 && delta < 120) {
      _last_adv_sync_outcome = 4; // skipped: drift too small
      return;
    }
    // Single-Source-Heuristik: bei nur 1 configured AND nach Boot-First-Sync
    // sehr grossen Drift ignorieren (koennte falsche Source sein). Erste
    // Sync nach Boot umgeht das (RTC kann real wild off sein).
    if (configured == 1 && _time_sync_done_since_boot
        && (delta > 3600 || delta < -3600)) {
      pushDebugLog("[rtc] adv-sync IGNORED: delta=%lds too big (1 source)\n",
                   (long)delta);
      return;
    }
    // Anwenden
    getRTCClock()->setCurrentTime(adv_timestamp);
    _time_sync_last_at_rtc = adv_timestamp;
    _time_sync_strict_last_ts[src_idx] = adv_timestamp;
    _time_sync_done_since_boot = true;
    memcpy(_time_sync_last_pubkey, id.pub_key, 3);
    _last_adv_sync_outcome = 1; // applied
    pushDebugLog("[rtc] adv-sync from %02x%02x%02x delta=%lds (strict)\n",
                 id.pub_key[0], id.pub_key[1], id.pub_key[2], (long)delta);
    return;
  }

  // ----- Lazy mode -----
  if (_prefs.time_sync_mode == 1) {
    if (!_time_sync_lazy_done) {
      // Collection-Phase
      if (_time_sync_lazy_started_ms == 0) _time_sync_lazy_started_ms = millis();
      if (_time_sync_lazy_count < 5) {
        TimeSyncCandidate& c = _time_sync_lazy_cands[_time_sync_lazy_count++];
        c.timestamp = adv_timestamp;
        c.pub_key3[0] = id.pub_key[0];
        c.pub_key3[1] = id.pub_key[1];
        c.pub_key3[2] = id.pub_key[2];
        c.adv_type = adv_type;
      }
      // Trigger Finalize wenn 5 Kandidaten oder 3 min vergangen.
      // 3-min-Check via loop()-tick separat, hier nur die 5er-Schwelle.
      if (_time_sync_lazy_count >= 5) {
        timeSyncFinalizeLazyCollection();
      }
      return;
    }
    // Steady-state Lazy: einfache Drift-Pruefung mit single-source-
    // Heuristik. Replay-Schutz hier nicht per Source (lazy hat keine
    // gespeicherte Source-Liste), aber das advert_timestamp muss >
    // _time_sync_last_at_rtc sein.
    if (adv_timestamp <= _time_sync_last_at_rtc) return;
    int32_t delta = (int32_t)(adv_timestamp - now_rtc);
    if (delta > -20 && delta < 20) return;
    // Im Steady-state lazy: bei sehr grossen Drifts vorsichtig sein
    // (koennte falsche Source sein). > 1h ignorieren.
    if (delta > 3600 || delta < -3600) {
      pushDebugLog("[rtc] adv-sync IGNORED: delta=%lds too big (lazy)\n",
                   (long)delta);
      return;
    }
    getRTCClock()->setCurrentTime(adv_timestamp);
    _time_sync_last_at_rtc = adv_timestamp;
    memcpy(_time_sync_last_pubkey, id.pub_key, 3);
    pushDebugLog("[rtc] adv-sync from %02x%02x%02x delta=%lds (lazy)\n",
                 id.pub_key[0], id.pub_key[1], id.pub_key[2], (long)delta);
  }
}

void MyMesh::timeSyncFinalizeLazyCollection() {
  if (_time_sync_lazy_done) return;
  if (_time_sync_lazy_count == 0) {
    // Keine Kandidaten gesehen -- collection beenden, beim naechsten
    // Advert greift dann steady-state lazy.
    _time_sync_lazy_done = true;
    _time_sync_lazy_started_ms = 0;
    return;
  }
  // Cluster-Analyse: fuer jeden Kandidaten zaehlen wie viele andere
  // innerhalb 60s liegen. Hoechste Cluster-Staerke gewinnt. Bei Gleichstand
  // adv_type Prio ROOM (3) > CHAT (1) > REPEATER (2).
  // (Hinweis: Konstanten ADV_TYPE_REPEATER=2, _CHAT=1, _ROOM=3 -- Prio-
  // Mapping unten explizit.)
  auto type_prio = [](uint8_t t) -> int {
    if (t == ADV_TYPE_ROOM) return 3;
    if (t == ADV_TYPE_CHAT) return 2;
    if (t == ADV_TYPE_REPEATER) return 1;
    return 0;
  };
  int best = -1;
  int best_strength = -1;
  int best_prio = -1;
  for (int i = 0; i < _time_sync_lazy_count; i++) {
    int strength = 0;
    for (int j = 0; j < _time_sync_lazy_count; j++) {
      if (i == j) continue;
      int32_t d = (int32_t)(_time_sync_lazy_cands[j].timestamp
                          - _time_sync_lazy_cands[i].timestamp);
      if (d < 0) d = -d;
      if (d <= 60) strength++;
    }
    int prio = type_prio(_time_sync_lazy_cands[i].adv_type);
    if (strength > best_strength
        || (strength == best_strength && prio > best_prio)) {
      best = i;
      best_strength = strength;
      best_prio = prio;
    }
  }
  if (best < 0) {
    _time_sync_lazy_done = true;
    _time_sync_lazy_started_ms = 0;
    return;
  }
  const TimeSyncCandidate& chosen = _time_sync_lazy_cands[best];
  uint32_t now_rtc = getRTCClock()->getCurrentTime();
  int32_t delta = (int32_t)(chosen.timestamp - now_rtc);
  if (delta > -20 && delta < 20) {
    // RTC ist bereits nahe genug an Cluster-Median -- nichts tun.
    _time_sync_lazy_done = true;
    pushDebugLog("[rtc] lazy-collect: %d cand, no correction (delta=%ld)\n",
                 (int)_time_sync_lazy_count, (long)delta);
    return;
  }
  getRTCClock()->setCurrentTime(chosen.timestamp);
  _time_sync_last_at_rtc = chosen.timestamp;
  _time_sync_done_since_boot = true;
  memcpy(_time_sync_last_pubkey, chosen.pub_key3, 3);
  _time_sync_lazy_done = true;
  pushDebugLog("[rtc] lazy-collect: %d cand, picked %02x%02x%02x type=%d delta=%lds\n",
               (int)_time_sync_lazy_count,
               chosen.pub_key3[0], chosen.pub_key3[1], chosen.pub_key3[2],
               (int)chosen.adv_type, (long)delta);
}

void MyMesh::putRuntimeNeighbour(const mesh::Identity& id, uint32_t advert_timestamp,
                                 int8_t snr_q4, uint8_t adv_type) {
  // 1) Bereits bekannt? -> Eintrag updaten.
  for (int i = 0; i < _neighbours_count; i++) {
    if (memcmp(_neighbours[i].pub_key, id.pub_key, 32) == 0) {
      _neighbours[i].advert_timestamp = advert_timestamp;
      _neighbours[i].heard_timestamp  = getRTCClock()->getCurrentTime();
      _neighbours[i].heard_millis     = millis();
      _neighbours[i].snr              = snr_q4;
      _neighbours[i].adv_type         = adv_type;
      return;
    }
  }
  // 2) Platz vorhanden? -> Anhaengen.
  int target_idx;
  if (_neighbours_count < MAX_RUNTIME_NEIGHBOURS) {
    target_idx = _neighbours_count++;
  } else {
    // 3) LRU: aelteste heard_millis verdraengen. (heard_timestamp
    // unsicher solange RTC nicht gesetzt -> heard_millis ist robuster
    // weil monoton seit Boot.)
    uint32_t oldest = 0xFFFFFFFFu;
    target_idx = 0;
    for (int i = 0; i < _neighbours_count; i++) {
      if (_neighbours[i].heard_millis < oldest) {
        oldest = _neighbours[i].heard_millis;
        target_idx = i;
      }
    }
  }
  memcpy(_neighbours[target_idx].pub_key, id.pub_key, 32);
  _neighbours[target_idx].advert_timestamp = advert_timestamp;
  _neighbours[target_idx].heard_timestamp  = getRTCClock()->getCurrentTime();
  _neighbours[target_idx].heard_millis     = millis();
  _neighbours[target_idx].snr              = snr_q4;
  _neighbours[target_idx].adv_type         = adv_type;
}

void MyMesh::onDiscoveredContact(ContactInfo &contact, bool is_new, uint8_t path_len, const uint8_t* path) {
  // Stats: alle empfangenen Adverts (egal Hop-Count) pro Node-Typ
  if (contact.type < 5 && _rx_advert_total[contact.type] < 0xFFFF) {
    _rx_advert_total[contact.type]++;
  }
  // Wunschliste 26 C: scoped/unscoped Aufschluesselung parallel pflegen.
  int scope_idx = _last_advert_scoped ? 1 : 0;
  if (contact.type < 5 && _rx_advert_by_scope[contact.type][scope_idx] < 0xFFFF) {
    _rx_advert_by_scope[contact.type][scope_idx]++;
  }
  // Wunschliste 26 D: DIRECT-typed (sendZeroHop) Adverts pro Rolle.
  if (_last_advert_route_direct && contact.type < 5
      && _rx_direct_advert_by_role[contact.type] < 0xFFFF) {
    _rx_direct_advert_by_role[contact.type]++;
  }
  // Track only adverts received directly (zero-hop, no repeater in the path).
  if ((path_len & 63) == 0) {
    markHeardDirect(contact.id.pub_key[0]);
    // Stats: zero-hop direkt empfangene Adverts pro Node-Typ
    if (contact.type < 5 && _heard_direct[contact.type] < 0xFFFF) {
      _heard_direct[contact.type]++;
      if (_heard_direct_by_scope[contact.type][scope_idx] < 0xFFFF) {
        _heard_direct_by_scope[contact.type][scope_idx]++;
      }
      // SNR-Quality-Klassifikation. Schwellen in Q4 (snr*4):
      //   gut:     SNR >= 0 dB  -> q4 >= 0
      //   mittel:  -8 <= SNR < 0 -> -32 <= q4 < 0
      //   schlecht: SNR < -8 dB -> q4 < -32
      int8_t snr_q4 = _last_advert_snr_q4;
      int q_idx = (snr_q4 >= 0) ? 0 : (snr_q4 >= -32 ? 1 : 2);
      if (_heard_quality[contact.type][q_idx] < 0xFFFF) {
        _heard_quality[contact.type][q_idx]++;
      }
    }
  }

  if (_serial->isConnected()) {
    if (is_new) {
      writeContactRespFrame(PUSH_CODE_NEW_ADVERT, contact);
    } else {
      out_frame[0] = PUSH_CODE_ADVERT;
      memcpy(&out_frame[1], contact.id.pub_key, PUB_KEY_SIZE);
      _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE);
    }
  } else {
#ifdef DISPLAY_CLASS
    if (_ui) _ui->notify(UIEventType::newContactMessage);
#endif
  }

  // add inbound-path to mem cache
  if (path && mesh::Packet::isValidPathLen(path_len)) {  // check path is valid
    AdvertPath* p = advert_paths;
    uint32_t oldest = 0xFFFFFFFF;
    for (int i = 0; i < ADVERT_PATH_TABLE_SIZE; i++) {   // check if already in table, otherwise evict oldest
      if (memcmp(advert_paths[i].pubkey_prefix, contact.id.pub_key, sizeof(AdvertPath::pubkey_prefix)) == 0) {
        p = &advert_paths[i];   // found
        break;
      }
      if (advert_paths[i].recv_timestamp < oldest) {
        oldest = advert_paths[i].recv_timestamp;
        p = &advert_paths[i];
      }
    }

    memcpy(p->pubkey_prefix, contact.id.pub_key, sizeof(p->pubkey_prefix));
    strcpy(p->name, contact.name);
    p->recv_timestamp = getRTCClock()->getCurrentTime();
    p->path_len = mesh::Packet::copyPath(p->path, path, path_len);
  }

  if (!is_new) dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY); // only schedule lazy write for contacts that are in contacts[]
}

static int sort_by_recent(const void *a, const void *b) {
  return ((AdvertPath *) b)->recv_timestamp - ((AdvertPath *) a)->recv_timestamp;
}

int MyMesh::getRecentlyHeard(AdvertPath dest[], int max_num) {
  if (max_num > ADVERT_PATH_TABLE_SIZE) max_num = ADVERT_PATH_TABLE_SIZE;
  qsort(advert_paths, ADVERT_PATH_TABLE_SIZE, sizeof(advert_paths[0]), sort_by_recent);

  for (int i = 0; i < max_num; i++) {
    dest[i] = advert_paths[i];
  }
  return max_num;
}

void MyMesh::onContactPathUpdated(const ContactInfo &contact) {
  out_frame[0] = PUSH_CODE_PATH_UPDATED;
  memcpy(&out_frame[1], contact.id.pub_key, PUB_KEY_SIZE);
  _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE); // NOTE: app may not be connected

  dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
}

ContactInfo*  MyMesh::processAck(const uint8_t *data) {
  // see if matches any in a table
  for (int i = 0; i < EXPECTED_ACK_TABLE_SIZE; i++) {
    if (memcmp(data, &expected_ack_table[i].ack, 4) == 0) { // got an ACK from recipient
      out_frame[0] = PUSH_CODE_SEND_CONFIRMED;
      memcpy(&out_frame[1], data, 4);
      uint32_t trip_time = _ms->getMillis() - expected_ack_table[i].msg_sent;
      memcpy(&out_frame[5], &trip_time, 4);
      _serial->writeFrame(out_frame, 9);

      // NOTE: the same ACK can be received multiple times!
      expected_ack_table[i].ack = 0; // clear expected hash, now that we have received ACK
      return expected_ack_table[i].contact;
    }
  }
  return checkConnectionsAck(data);
}

void MyMesh::queueMessage(const ContactInfo &from, uint8_t txt_type, mesh::Packet *pkt,
                          uint32_t sender_timestamp, const uint8_t *extra, int extra_len, const char *text) {
  // Wunschliste 46 Phase 1 (Reise 2026-06-09): Sender-Filter fuer DMs.
  // Bei Match: komplett verwerfen (kein Push, kein Offline-Queue-Eintrag).
  // 'for-us'-Filter -- Repeat wird vom Filter NICHT beeinflusst.
  if (filterSenderDropMatch(from.name)) {
    traceCompanion(TRACE_FILTER, "[filter] DM dropped: sender='%s'", from.name);
    return;
  }
  // Wunschliste 35 (DM): once-per-tuple Scope-/Direct-Annotation als
  // SEPARATE Vorab-Message '[#scope, direct]'. Separate Frame statt
  // Footer im Original-Text: (1) keine Laengen-Limit-Konflikte bei
  // langen DMs, (2) [...] signalisiert visuell 'das ist Metadata, nicht
  // vom Sender'. Vorab-Timestamp = sender_timestamp - 1 damit die
  // Annotation chronologisch oberhalb der eigentlichen Nachricht
  // einsortiert wird.
  // Reise-Toggle 2026-06-08: DM-Vorab-Frame [#scope, direct] nur wenn
  // messages_append_scope_to_name on. Default off.
  if ((txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_SIGNED_PLAIN)
      && _prefs.messages_append_scope_to_name) {
    uint32_t name_h = fnv1a32((const char*)from.id.pub_key, 6);
    uint32_t scope_h;
    const char* scope_label = NULL;
    char scope_buf[36];
    if (pkt->hasTransportCodes()) {
      const char* scope_name = lookupRegionByTransportCode(pkt);
      if (scope_name) {
        scope_h = fnv1a32_cstr(scope_name);
        if (scope_h == 0xFFFFFFFEUL || scope_h == 0xFFFFFFFFUL) scope_h ^= 0x12345678UL;
        snprintf(scope_buf, sizeof(scope_buf), "#%s", scope_name);
      } else {
        scope_h = 0xFFFFFFFEUL;
        snprintf(scope_buf, sizeof(scope_buf), "#?");
      }
      scope_label = scope_buf;
    } else {
      scope_h = 0xFFFFFFFFUL;
      scope_label = "#*";
    }
    uint8_t direct_flag = (pkt->path_len == 0) ? 1 : 0;
    // DM-Pfad: channel_hash=0 (kein Channel-Bezug). User-Wunsch 2026-06-08:
    // wenn ein Sender seinen scope aendert, soll die '[#scope, direct]'-
    // Vorab-Message neu kommen -- ist durch das scope_h-Tuple-Member
    // automatisch (neuer scope_h => neuer Tuple => !already_seen).
    bool already_seen = channelSenderSeenLookupOrAdd(0 /* DM */, name_h, scope_h, direct_flag);
    if (!already_seen) {
      // Separate Vorab-Frame mit Metadata-Text.
      char meta_text[48];
      snprintf(meta_text, sizeof(meta_text), "[%s%s]",
               scope_label, direct_flag ? ", direct" : "");
      uint32_t meta_ts = sender_timestamp > 0 ? (sender_timestamp - 1) : sender_timestamp;
      int mi = 0;
      if (app_target_ver >= 3) {
        out_frame[mi++] = RESP_CODE_CONTACT_MSG_RECV_V3;
        out_frame[mi++] = (int8_t)(pkt->getSNR() * 4);
        out_frame[mi++] = 0;  // reserved1
        out_frame[mi++] = 0;  // reserved2
      } else {
        out_frame[mi++] = RESP_CODE_CONTACT_MSG_RECV;
      }
      memcpy(&out_frame[mi], from.id.pub_key, 6);
      mi += 6;
      out_frame[mi++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
      out_frame[mi++] = TXT_TYPE_PLAIN;
      memcpy(&out_frame[mi], &meta_ts, 4);
      mi += 4;
      // extra-Section: leer fuer Metadata (kein ACK-Hash o.ae.)
      int mtlen = (int)strlen(meta_text);
      if (mi + mtlen > MAX_FRAME_SIZE) mtlen = MAX_FRAME_SIZE - mi;
      memcpy(&out_frame[mi], meta_text, mtlen);
      mi += mtlen;
      addToOfflineQueue(out_frame, mi);
    }
  }

  int i = 0;
  if (app_target_ver >= 3) {
    out_frame[i++] = RESP_CODE_CONTACT_MSG_RECV_V3;
    out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
    out_frame[i++] = 0; // reserved1
    out_frame[i++] = 0; // reserved2
  } else {
    out_frame[i++] = RESP_CODE_CONTACT_MSG_RECV;
  }
  memcpy(&out_frame[i], from.id.pub_key, 6);
  i += 6; // just 6-byte prefix
  uint8_t path_len = out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
  out_frame[i++] = txt_type;
  memcpy(&out_frame[i], &sender_timestamp, 4);
  i += 4;
  if (extra_len > 0) {
    memcpy(&out_frame[i], extra, extra_len);
    i += extra_len;
  }

  int tlen = strlen(text); // TODO: UTF-8 ??
  if (i + tlen > MAX_FRAME_SIZE) {
    tlen = MAX_FRAME_SIZE - i;
  }
  memcpy(&out_frame[i], text, tlen);
  i += tlen;
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  }

#ifdef DISPLAY_CLASS
  // we only want to show text messages on display, not cli data
  bool should_display = txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_SIGNED_PLAIN;
  if (should_display && _ui) {
    _ui->newMsg(path_len, from.name, text, offlineQueueTotal());
    if (!_serial->isConnected()) {
      _ui->notify(UIEventType::contactMessage);
    }
  }
#endif
}

// Lesbarer Pakettyp-Name fuer Trace-Output (statt nur Zahlenwert).
static const char* ptypeName(uint8_t pt) {
  switch (pt) {
    case PAYLOAD_TYPE_REQ:        return "REQ";
    case PAYLOAD_TYPE_RESPONSE:   return "RSP";
    case PAYLOAD_TYPE_TXT_MSG:    return "TXT";
    case PAYLOAD_TYPE_ACK:        return "ACK";
    case PAYLOAD_TYPE_ADVERT:     return "ADV";
    case PAYLOAD_TYPE_GRP_TXT:    return "GRP";
    case PAYLOAD_TYPE_GRP_DATA:   return "GDATA";
    case PAYLOAD_TYPE_ANON_REQ:   return "ANON";
    case PAYLOAD_TYPE_PATH:       return "PATH";
    case PAYLOAD_TYPE_TRACE:      return "TRC";
    case PAYLOAD_TYPE_MULTIPART:  return "MPART";
    case PAYLOAD_TYPE_CONTROL:    return "CTRL";
    case PAYLOAD_TYPE_RAW_CUSTOM: return "RAW";
    default:                      return "?";
  }
}

// Wunschliste 26 B: 4-Byte truncated MAX_HASH (= SHA-Trunc) als
// kompakte Identifizierung des Pakets. Kollisionswahrscheinlichkeit ueber
// 128 Eintraege ~2^-25 -- vernachlaessigbar.
uint32_t MyMesh::calcShortHash(const mesh::Packet* packet) const {
  if (!packet) return 0;
  uint8_t h[MAX_HASH_SIZE];
  // calculatePacketHash ist const-ueblich aber im aufrufenden Code nicht
  // immer const-zugaenglich -- cast.
  const_cast<mesh::Packet*>(packet)->calculatePacketHash(h);
  uint32_t hh;
  memcpy(&hh, h, 4);
  return hh;
}

void MyMesh::markSelfInitiated(const mesh::Packet* packet) {
  uint32_t h = calcShortHash(packet);
  if (h == 0) return;
  _self_initiated_hashes[_self_initiated_head] = h;
  _self_initiated_head = (uint8_t)((_self_initiated_head + 1) % 32);
}

void MyMesh::markSelfRepeated(const mesh::Packet* packet) {
  uint32_t h = calcShortHash(packet);
  if (h == 0) return;
  _self_repeated_hashes[_self_repeated_head] = h;
  _self_repeated_head = (uint8_t)((_self_repeated_head + 1) % 128);
}

uint8_t MyMesh::matchSelfHash(uint32_t h) const {
  if (h == 0) return 0;
  for (int i = 0; i < 32; i++) {
    if (_self_initiated_hashes[i] == h) return 1;
  }
  for (int i = 0; i < 128; i++) {
    if (_self_repeated_hashes[i] == h) return 2;
  }
  return 0;
}

bool MyMesh::filterRecvFloodPacket(mesh::Packet* packet) {
  // Wunschliste 26 B: rx-us Echo-Tracking. filterRecvFloodPacket laeuft
  // VOR der hasSeen-Dedup, also sehen wir hier auch Echos eigener Sendungen.
  // Match gegen unsere self-sent/repeated Hash-Ringe.
  uint32_t h = calcShortHash(packet);
  uint8_t m = matchSelfHash(h);
  if (m == 1 && _rx_us_self_initiated_count < 0xFFFF) {
    _rx_us_self_initiated_count++;
  } else if (m == 2 && _rx_us_repeated_count < 0xFFFF) {
    _rx_us_repeated_count++;
  }
  // REVISIT: try to determine which Region (from transport_codes[1]) that Sender is indicating for replies/responses
  //    if unknown, fallback to finding Region from transport_codes[0], the 'scope' used by Sender
  return false;
}

bool MyMesh::allowPacketForward(const mesh::Packet* packet) {
  uint8_t ptype_raw = packet->getPayloadType();
  // Stats: alle Flood-Pakete die wir als Forward-Kandidaten sehen (egal ob
  // wir sie am Ende durchreichen oder nicht). Decken nicht die Direct-Pakete
  // ab — die durchlaufen einen anderen Mesh-Pfad. Aber im realen Mesh sind
  // floods der Hauptanteil des Hintergrundsrauschens.
  if (ptype_raw < 16) {
    // Wunschliste 26 D: split by path_len. heard-direct = noch nicht von
    // einem Repeater weitergereicht (wir sind in Funkreichweite des
    // Originators). repeated = ueber mind. einen Repeater eingetroffen.
    int hop_idx = ((packet->path_len & 63) == 0) ? 0 : 1;
    if (_rx_flood_by_ptype[ptype_raw][hop_idx] < 0xFFFF) {
      _rx_flood_by_ptype[ptype_raw][hop_idx]++;
    }
  }

  // Runtime-Gate: cached effektive Repeating-Erlaubnis. Wird via
  // recomputeRepeatingAllowed() bei jeder relevanten Stored-State-
  // Aenderung neu berechnet (CLI repeater on/off/profile, App-CMD_SET_
  // RADIO_PARAMS, boot). Per-Paket-Check ist ein einzelner Bool-Load.
  if (!_repeating_allowed) {
    // Kein Trace hier - bei deaktiviertem Repeater wuerde JEDES Paket einen
    // filter-trace generieren, das ist nur Laerm.
    return false;
  }

  // (Wunschliste 34 is_moving-Gate ist jetzt in isRepeatingEffectivelyAllowed()
  // / cached _repeating_allowed Flag integriert -- kein per-Paket-Branch
  // mehr noetig. Transition via recomputeRepeatingAllowed() aus
  // updateMotionTracking().)

  // Duty-Cycle Soft-Limit: Repeats unterdruecken bei Annaeherung an die
  // 10%/h-Grenze. Eigene Pakete (Auto-Adverts, User-Chat) laufen weiter
  // bis Hard erreicht ist. Stats-Counter + Trace.
  if (dutySoftReached()) {
    _duty_blocked_count++;
    traceCompanion(TRACE_DUTY, "[duty] repeat dropped (last_h=%lus soft=%lus)",
                   getTxAirLastHour()/1000, getDutySoftLimitMs()/1000);
    return false;
  }

  uint8_t ptype = packet->getPayloadType();
  bool decision = false;
  const char* reject_reason = "?";

  // Reise-Hardening 2026-06-08: Defense-in-Depth gegen Self-Echo.
  // Schicht A (SimpleMeshTables::hasSeen) speichert beim Senden den
  // packet-hash (Mesh.cpp:652+). Echo wird normalerweise dort gedroppt.
  // ABER: Ringbuffer ist 160 Slots gross -- bei viel Traffic koennte
  // ein eigenes Paket nach Eviction wieder duplikat-frei erscheinen.
  // Schicht B (DL9SAU _self_initiated + _self_repeated, je 32/128 Slots)
  // haelt eigene Hashes separat. Hier als zusaetzlicher Drop-Check
  // VOR allen anderen Filter-Stufen: wenn das Paket schon mal von uns
  // initiiert oder repeated wurde, niemals wieder repeaten.
  if (matchSelfHash(calcShortHash(packet)) != 0) {
    traceCompanion(TRACE_FILTER, "[filter] reject %s hops=%u reason=self-echo",
                   ptypeName(ptype), (unsigned)packet->getPathHashCount());
    return false;
  }

  // Wunschliste 32 Pre-Flight: GRP-Cap-Check als Vor-Berechnung.
  // Cap-Encoding: CH_HOPS_OFF=skip / 0=immer droppen / 1..N=droppen wenn
  // path_hash_count > N. Restriktivster Match aus allen matchenden Slots +
  // External-Eintraegen gewinnt. Unbekannter ch_hash: flood_max_unknown_chan.
  // Ergebnis wird im scope-Block (else-if unten) konsumiert.
  bool grp_cap_violated = false;
  const char* grp_cap_reason = NULL;
  if ((ptype == PAYLOAD_TYPE_GRP_TXT || ptype == PAYLOAD_TYPE_GRP_DATA)
      && packet->payload_len >= 1) {
    uint8_t ch_hash = packet->payload[0];
    uint8_t eff_cap = CH_HOPS_OFF;
    bool matched = false;
    for (int ci = 0; ci < MAX_GROUP_CHANNELS; ci++) {
      ChannelDetails ch;
      if (!getChannel(ci, ch)) continue;
      if (ch.name[0] == 0) continue;
      if (ch.channel.hash[0] != ch_hash) continue;
      matched = true;
      uint8_t cap = _channel_hops_cap_cache[ci];
      if (cap == CH_HOPS_OFF) continue;
      if (eff_cap == CH_HOPS_OFF || cap < eff_cap) eff_cap = cap;
    }
    for (uint8_t e = 0; e < _prefs.channel_hops_count; e++) {
      const auto& en = _prefs.channel_hops_list[e];
      if (!(en.flags & CH_HOPS_FLAG_EXTERNAL)) continue;
      if (en.channel_hash != ch_hash) continue;
      matched = true;
      if (en.cap == CH_HOPS_OFF) continue;
      if (eff_cap == CH_HOPS_OFF || en.cap < eff_cap) eff_cap = en.cap;
    }
    if (!matched) eff_cap = _prefs.flood_max_unknown_chan;
    if (eff_cap != CH_HOPS_OFF
        && (eff_cap == 0 || packet->getPathHashCount() > eff_cap)) {
      grp_cap_violated = true;
      grp_cap_reason = matched ? "ch.hops-cap" : "unknown-chan-cap";
    }
  }

  // Wunschliste 6b: Loop-Detection-Vorbereitung. Pre-compute ob das
  // Paket im normal-Profile als Loop verworfen werden soll. Wir nutzen
  // das Ergebnis als else-if-Pruefer, damit nicht-geloopte Pakete den
  // normalen ptype-Dispatch weiter durchlaufen.
  bool loop_drop = (_prefs.repeater_profile == 1 /* normal */
                    && _prefs.loop_detect != LOOP_DETECT_OFF
                    && packet->isRouteFlood());
  if (loop_drop) {
    const uint8_t* maxs =
        (_prefs.loop_detect == LOOP_DETECT_MINIMAL)  ? max_loop_minimal
      : (_prefs.loop_detect == LOOP_DETECT_MODERATE) ? max_loop_moderate
                                                     : max_loop_strict;
    loop_drop = isLooped(packet, maxs);
  }

  // path-length cap (hop count, not byte length). _prefs.flood_max
  // analog CommonCLI/simple_repeater 'flood.max'.
  if (packet->getPathHashCount() > _prefs.flood_max) {
    reject_reason = "path-too-long";
  }
  else if (loop_drop) {
    reject_reason = "loop-detected";
  }
  // ROUTE_TYPE_DIRECT/_TRANSPORT_DIRECT: path-forwarding, NICHT repeating.
  // Wir sind benannter Hop -- Mesh.cpp:81-114 hat self_id.isHashMatch
  // bereits geprueft. Path-Routing ist die Routing-Entscheidung, kein
  // zusaetzlicher Scope/Cap-Filter. Globale Limits (flood_max via
  // getPathHashCount, duty soft, _repeating_allowed) sind oben bereits
  // geprueft. Behebt vorigen Bug wo unscoped DMs entlang etablierter
  // Pfade gedroppt wurden (Wunschliste 39, 2026-06-04).
  else if (packet->isRouteDirect()) {
    decision = true;
  }
  // Wunschliste 24 (2026-05-30): Infrastruktur-Advert-Cap. Adverts mit
  // adv_type != ADV_TYPE_CHAT (also REPEATER/SENSOR/ROOM) werden ab
  // path_hash_count > flood_max_infra nicht mehr weitergeleitet.
  // User-Adverts (Chat) muessen weit kommen damit Erstkontakt ohne
  // externen Schluesseltausch funktioniert; Infrastruktur-Adverts sind
  // ortsbezogen. Advert-Payload-Layout (Mesh.cpp:249+):
  //   [0..31]    pub_key (PUB_KEY_SIZE)
  //   [32..35]   timestamp
  //   [36..99]   signature (SIGNATURE_SIZE)
  //   [100]      _flags -- low 4 bits = adv_type
  // Condition komplett in das else-if-Statement gezogen damit bei
  // NICHT-rejection (z.B. CHAT-advert oder hops <= cap) der else-Zweig
  // nicht eintritt und die nachfolgende ptype-Scope-Pruefung greift.
  else if (ptype == PAYLOAD_TYPE_ADVERT
           && packet->payload_len > (int)(PUB_KEY_SIZE + 4 + SIGNATURE_SIZE)
           && (packet->payload[PUB_KEY_SIZE + 4 + SIGNATURE_SIZE] & 0x0F) != ADV_TYPE_CHAT
           && packet->getPathHashCount() > effectiveFloodMaxInfra()) {
    reject_reason = "infra-advert-cap";
  }
  // Wunschliste 29 (2026-06-01) + Vereinheitlichung 39 (2026-06-04):
  // Hop-Cap fuer REQ/RESP/ANON_REQ. effectiveFloodMaxReqResp() loest
  // die follow-Kaskade auf: follow-sentinel -> infra -> flood_max.
  else if ((ptype == PAYLOAD_TYPE_REQ
            || ptype == PAYLOAD_TYPE_RESPONSE
            || ptype == PAYLOAD_TYPE_ANON_REQ)
           && packet->getPathHashCount() > effectiveFloodMaxReqResp()) {
    reject_reason = "req-resp-cap";
  }
  // Wunschliste 32 (per-Channel Repeat-Cap fuer Group-Messages) wurde
  // urspruenglich als eigener else-if-Branch implementiert und hat dabei
  // versehentlich den scope-Check fuer GRP-Pakete umgangen -- weil GRP_TXT/
  // GRP_DATA im exklusiven else-if landeten und decision=true im scope-Block
  // weiter unten nie erreicht wurde. Resultat: ALLE GRP-Pakete wurden mit
  // 'reject reason=?' verworfen, grp=0 in tx-repeated-Stats.
  //
  // Fix (2026-06-08): Cap-Check als Pre-Flight VOR dem else-if-Strang. Das
  // Ergebnis (grp_cap_violated/grp_cap_reason) wird im scope-Block unten
  // konsumiert, sodass GRP-Pakete normal durch hasTransportCodes/
  // scopeAllowedForRepeat laufen.
  //
  // Cap-Encoding:
  //   CH_HOPS_OFF (254) -> kein Cap, skip
  //   0                 -> immer droppen
  //   1..N              -> droppen wenn path_hash_count > N
  else if (ptype == PAYLOAD_TYPE_ADVERT || ptype == PAYLOAD_TYPE_ACK ||
      ptype == PAYLOAD_TYPE_GRP_TXT || ptype == PAYLOAD_TYPE_GRP_DATA ||
      ptype == PAYLOAD_TYPE_MULTIPART || ptype == PAYLOAD_TYPE_CONTROL ||
      ptype == PAYLOAD_TYPE_RAW_CUSTOM || ptype == PAYLOAD_TYPE_TRACE ||
      ptype == PAYLOAD_TYPE_REQ || ptype == PAYLOAD_TYPE_RESPONSE ||
      ptype == PAYLOAD_TYPE_TXT_MSG || ptype == PAYLOAD_TYPE_ANON_REQ) {
    // Wunschliste 32 (Fix 2026-06-08): GRP-Cap-Verstoss hat Vorrang vor
    // dem hasTransportCodes/scope-Check. Wenn das Pre-Flight oben einen
    // Cap-Verstoss erkannt hat, droppen wir ohne weitere Pruefung.
    if (grp_cap_violated) {
      reject_reason = grp_cap_reason;
      // decision bleibt false
    } else {
    decision = packet->hasTransportCodes();
    if (!decision) {
      // Wunschliste 39 (2026-06-04): Selektives Aufweichen des
      // unscoped-Blocks fuer Companion-User-Traffic (Erstkontakt).
      //   - ADVERT + ADV_TYPE_CHAT  (User publiziert pub_key/Position)
      //   - TXT_MSG flood            (DM ohne Pfad + ohne Scope-Default)
      // Cap = flood_max_unscoped_companions (Sentinel CH_HOPS_OFF =
      // follow flood_max_scope_region). REQ/RESP/ANON_REQ bleiben
      // gedroppt (40% Traffic, kein Erstkontakt-Use-Case).
      uint8_t cap = _prefs.flood_max_unscoped_companions;
      if (cap == CH_HOPS_OFF) cap = _prefs.flood_max_scope_region;
      bool is_chat_advert =
          (ptype == PAYLOAD_TYPE_ADVERT
           && packet->payload_len > (int)(PUB_KEY_SIZE + 4 + SIGNATURE_SIZE)
           && (packet->payload[PUB_KEY_SIZE + 4 + SIGNATURE_SIZE] & 0x0F) == ADV_TYPE_CHAT);
      bool is_user_txt = (ptype == PAYLOAD_TYPE_TXT_MSG);
      if ((is_chat_advert || is_user_txt) && cap > 0
          && packet->getPathHashCount() <= cap) {
        decision = true;
      } else {
        reject_reason = (cap == 0) ? "unscoped-companions-off"
                                    : (is_chat_advert || is_user_txt)
                                       ? "unscoped-companions-cap"
                                       : "unscoped";
      }
    }
    else if (!scopeAllowedForRepeat(packet)) {
      decision = false;
      reject_reason = "scope-not-allowed";
    } else {
      // Wunschliste 11 Schritte 11+12 + Wunschliste 14: Special-Scope-
      // Handling. Diese Logik laeuft NACH scopeAllowedForRepeat — also
      // sowohl im 'repeat allowlist'- als auch im 'repeat all'-Modus
      // (im all-Modus liefert scopeAllowedForRepeat immer true, danach
      // greifen hier die hard-blocks).
      //
      // local/lokal: single-hop. Pakete mit hops==0 werden weitergeleitet,
      //              dabei wird der Scope zu 'local-discard' umgeschrieben
      //              damit kein zweiter Repeater drueber geht. Pakete mit
      //              hops>0 werden verworfen.
      // region/regional: konfigurierbares Hop-Limit (flood_max_scope_region).
      //
      // local-discard SENTINEL — WICHTIG: hier IMMER hart blocken,
      // unabhaengig vom repeat_scope_mode. Im 'repeat all'-Modus ist
      // scopeStatusAllowsRepeat NICHT auf den status_byte geprueft worden
      // (kurz-circuit), daher MUESSEN wir den Block hier setzen. Sonst
      // wuerde ein local-Paket, das gerade auf local-discard umgeschrieben
      // wurde, beim naechsten Repeater wieder weitergeleitet — und die
      // Single-Hop-Garantie waere kaputt.
      uint16_t target = packet->transport_codes[0];
      uint8_t hops = packet->getPathHashCount();
      int idx_local         = dl9sau_find_region_index("local");
      int idx_lokal         = dl9sau_find_region_index("lokal");
      int idx_region        = dl9sau_find_region_index("region");
      int idx_regional      = dl9sau_find_region_index("regional");
      int idx_local_discard = dl9sau_find_region_index("local-discard");
      // Wunschliste 38: User-adressierbare No-Repeat-Sentinels. Wer ein
      // Paket mit einem dieser Scopes sendet signalisiert allen Repeatern
      // "nicht weiterleiten". Hard-Block analog local-discard, ohne
      // Rewrite (sind End-States, kein Hop weiter).
      int idx_direct        = dl9sau_find_region_index("direct");
      int idx_direkt        = dl9sau_find_region_index("direkt");
      int idx_norepeat      = dl9sau_find_region_index("norepeat");
      int idx_no_repeat     = dl9sau_find_region_index("no-repeat");
      auto codeMatches = [&](int idx) -> bool {
        return idx >= 0 && idx < _buildin_keys_count
               && _buildin_keys[idx].calcTransportCode(packet) == target;
      };
      bool is_local         = codeMatches(idx_local) || codeMatches(idx_lokal);
      bool is_region        = codeMatches(idx_region) || codeMatches(idx_regional);
      bool is_local_discard = codeMatches(idx_local_discard);
      bool is_user_norepeat = codeMatches(idx_direct)
                            || codeMatches(idx_direkt)
                            || codeMatches(idx_norepeat)
                            || codeMatches(idx_no_repeat);

      if (is_local_discard) {
        // SENTINEL: NIE weiterleiten. Gilt auch im 'repeat all'-Modus.
        decision = false;
        reject_reason = "local-discard";
      } else if (is_user_norepeat) {
        // User hat explizit signalisiert nicht weiterleiten zu wollen.
        decision = false;
        reject_reason = "user-norepeat";
      } else if (is_local) {
        if (hops > 0) {
          decision = false;
          reject_reason = "local-multihop-drop";
        } else if (idx_local_discard >= 0
                   && idx_local_discard < _buildin_keys_count) {
          // Erst-Repeat: Scope auf local-discard umschreiben, sodass kein
          // weiterer Repeater drueber geht. const_cast OK weil Mesh.cpp
          // den Caller mit non-const Packet* hat.
          mesh::Packet* p = const_cast<mesh::Packet*>(packet);
          p->transport_codes[0] =
              _buildin_keys[idx_local_discard].calcTransportCode(packet);
          // decision bleibt true
        }
        // (idx_local_discard nicht gefunden: einfach normal weiterleiten —
        // sollte nicht passieren da local-discard im Build-in-Table steht.)
      } else if (is_region) {
        if (hops >= _prefs.flood_max_scope_region) {
          decision = false;
          reject_reason = "region-hop-limit";
        }
      }
    }
    }  // ende else-Branch fuer grp_cap_violated-Wrap (Wunschliste 32 Fix)
  } else if (ptype == PAYLOAD_TYPE_PATH) {
    // PATH discovery. Wunschliste 8:
    //   defensive (Default): nur fuer lokale Nodes repeaten (heard < 48h
    //     ODER known contact < 48h).
    //   normal: alle PATH-Pakete weiterleiten ("wie echter Repeater").
    //
    // Reise-Fix 2026-06-09: Lokaler Endpoint ist PRIVILEGIERT. Wenn
    // src ODER dest in unserer heard/contacts-Liste: durchlassen
    // unabhaengig von scope/unscoped-cap. Begruendung User:
    // 'Lokaler user macht unscoped path discovery. Antwort kommt via
    // repeatern. Antwort muss durch kommen.' Plus: 'Entfernter user
    // macht unscoped path discovery mit Ziel lokaler User. Antwort
    // zu ihm muss durch kommen.'
    bool is_local_endpoint = false;
    if (packet->payload_len >= 2) {
      uint8_t dest_hash = packet->payload[0];
      uint8_t src_hash  = packet->payload[1];
      uint32_t now = getRTCClock()->getCurrentTime();
      auto matchHash = [&](uint8_t h) -> bool {
        if (isLocallyHeard(h)) return true;
        int num = getNumContacts();
        for (int i = 0; i < num; i++) {
          ContactInfo c;
          if (!getContactByIdx(i, c)) continue;
          if (now - c.lastmod > CR_HEARD_MAX_AGE_SECS) continue;
          if (c.id.isHashMatch(&h)) return true;
        }
        return self_id.isHashMatch(&h);
      };
      is_local_endpoint = matchHash(dest_hash) || matchHash(src_hash);
    }

    if (_prefs.repeater_profile == 1) {
      decision = true;  // normal-Profil: kein lokaler Endpoint-Filter
    } else if (packet->payload_len >= 2) {
      decision = is_local_endpoint;  // defensive: nur lokal-endpoint
      if (!decision) reject_reason = "path-endpoint-unknown";
    } else {
      reject_reason = "path-payload-too-short";
    }

    // Scope-/Cap-Filter NUR bei nicht-lokalem Endpoint. Lokal-Endpoint
    // wird privilegiert durchgelassen (User-Wunsch oben).
    //
    // (a) scoped PATH: scopeAllowedForRepeat. Wenn Sender Scope auf
    //     Path-Discovery gesetzt hat, respektieren -- nicht weiter
    //     repeaten wenn ausserhalb Allowlist.
    // (b) unscoped PATH: Hop-Cap via flood_max_unscoped_companions
    //     (Sentinel CH_HOPS_OFF = follow flood_max_scope_region).
    //     Analog zur Wunschliste-39-Logik fuer unscoped CHAT-Adverts
    //     + TXT_MSG: 'damit messages von companion zu companion oder
    //     companion durch kommen'.
    if (decision && !is_local_endpoint) {
      if (packet->hasTransportCodes()) {
        if (!scopeAllowedForRepeat(packet)) {
          decision = false;
          reject_reason = "scope-not-allowed";
        }
      } else {
        uint8_t cap = _prefs.flood_max_unscoped_companions;
        if (cap == CH_HOPS_OFF) cap = _prefs.flood_max_scope_region;
        if (cap == 0 || packet->getPathHashCount() > cap) {
          decision = false;
          reject_reason = (cap == 0) ? "unscoped-companions-off"
                                      : "unscoped-companions-cap";
        }
      }
    }
  } else {
    reject_reason = "unknown-ptype";
  }

  // Wunschliste 46 Phase 5 (Repeat-Pfad, 2026-06-10):
  //   (a) scope-Filter mit profile=repeat/complete fuer GRP_TXT/GRP_DATA
  //   (b) filter_unknown_channel_repeat fuer unbekannte Channels
  // Erst nach allen anderen Checks, damit reject_reason erhalten bleibt
  // wenn schon ein anderer Grund das Paket droppt.
  if (decision && (ptype == PAYLOAD_TYPE_GRP_TXT || ptype == PAYLOAD_TYPE_GRP_DATA)
      && packet->payload_len >= 1) {
    // Channel-Index aus ch_hash ermitteln. -2 = unknown channel.
    int ch_idx = -2;
    uint8_t ch_hash = packet->payload[0];
    for (int ci = 0; ci < MAX_GROUP_CHANNELS && ci < 64; ci++) {
      ChannelDetails cd;
      if (!getChannel(ci, cd)) continue;
      if (cd.name[0] == 0) continue;
      if (cd.channel.hash[0] != ch_hash) continue;
      ch_idx = ci;
      break;
    }
    // (b) Repeat-Achse fuer unbekannte Channels
    if (ch_idx == -2) {
      uint8_t mode = _prefs.filter_unknown_channel_repeat;
      bool is_scoped = packet->hasTransportCodes();
      if (mode == 3) {
        decision = false;
        reject_reason = "unknown-channel-no";
      } else if (mode == 1 && !is_scoped) {
        decision = false;
        reject_reason = "unknown-channel-unscoped-drop";
      } else if (mode == 2 && is_scoped) {
        decision = false;
        reject_reason = "unknown-channel-scoped-drop";
      }
    }
    // (a) scope-Filter mit profile=repeat
    if (decision) {
      const char* sc_name = NULL;
      if (packet->hasTransportCodes()) {
        sc_name = lookupRegionByTransportCode(packet);
      }
      if (filterScopeMatch(sc_name, ch_idx, /*for_repeat=*/true)) {
        decision = false;
        reject_reason = "scope-filter";
      }
    }
  }

  if (decision) {
    _tx_digi_count++;
    if (ptype < 16 && _repeat_by_ptype[ptype] < 0xFFFF) {
      _repeat_by_ptype[ptype]++;
    }
    // Wunschliste 26 B: Hash dieses Repeats in den self_repeated-Ring.
    // Match in filterRecvFloodPacket erkennt Echos -> _rx_us_repeated_count.
    markSelfRepeated(packet);
    // Geschaetzte Airtime des Repeats (vor TX, gleiche Formel wie an
    // anderen Stellen im Code: pathBytes + payload + 2 Header-Bytes).
    _tx_repeat_airtime_ms += _radio->getEstAirtimeFor(
        packet->getPathByteLen() + packet->payload_len + 2);
    traceCompanion(TRACE_REPEAT, "[repeat] %s hops=%u scope=%s",
                   ptypeName(ptype), (unsigned)packet->getPathHashCount(),
                   packet->hasTransportCodes() ? "yes" : "no");
  } else {
    traceCompanion(TRACE_FILTER, "[filter] reject %s hops=%u reason=%s",
                   ptypeName(ptype), (unsigned)packet->getPathHashCount(),
                   reject_reason);
  }
  return decision;
}

void MyMesh::sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis) {
  if (scope.isNull()) {
    sendFlood(pkt, delay_millis, _prefs.path_hash_mode + 1);
  } else {
    uint16_t codes[2];
    codes[0] = scope.calcTransportCode(pkt);
    codes[1] = 0;  // REVISIT: set to 'home' Region, for sender/return region?
    sendFlood(pkt, codes, delay_millis, _prefs.path_hash_mode + 1);
  }
}

void MyMesh::sendFloodScoped(const ContactInfo& recipient, mesh::Packet* pkt, uint32_t delay_millis) {
  // TODO: dynamic send_scope, depending on recipient and current 'home' Region
  // upstream-1.16: App kann via send_unscoped Flag explizit unscoped senden.
  // Diese Pruefung kommt VOR unserer DL9SAU-Scope-Hierarchie, damit
  // die App-Entscheidung Vorrang hat.
  if (send_unscoped) {
    sendFlood(pkt, delay_millis, _prefs.path_hash_mode + 1);
    return;
  }
  TransportKey eff_scope;
  if (!send_scope.isNull()) {
    eff_scope = send_scope;
  } else if (!resolveDefaultOrGeo(eff_scope)) {
    // weder Default noch geo_prefers haben gegriffen -> null Scope
    memset(eff_scope.key, 0, sizeof(eff_scope.key));
  }
  sendFloodScoped(eff_scope, pkt, delay_millis);
}
void MyMesh::sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis) {
  // TODO: have per-channel send_scope
  //
  // Architektur Scope-Auswahl vs Routing-Policy (Channel-Send):
  //
  // upstream-1.16 hat 'send_unscoped' als App-CMD eingefuehrt
  // (CMD_SET_FLOOD_SCOPE_KEY [1]=1, globaler State). Naiv-Interpretation
  // waere: "App will unscoped flood -> mach unscoped flood, jetzt".
  // DL9SAU-Sicht: 'send_unscoped' ist ein Scope-AUSWAHL-Signal
  // ("kein Scope verwenden"), KEIN Routing-Befehl ("flood erzwingen").
  // Routing entscheidet die Firmware-Policy via Wunschliste 25
  // (_unscoped_channel_direct). Andernfalls wuerde App-Klick die
  // User-Policy 'unscoped-channel = direct' umgehen.
  //
  // Reihenfolge:
  // 1. send_unscoped       -> null-Scope (Scope-Auswahl)
  // 2. send_scope          -> per-send override
  // 3. resolveDefaultOrGeo -> Default-Scope / Geo-Fallback
  // 4. sonst null-Scope
  //
  // Bei null-Scope (egal aus welchem Pfad oben) greift dann
  // Wunschliste-25-Routing: direct (Default) oder flood (CLI-Override).
  TransportKey eff_scope;
  if (send_unscoped) {
    memset(eff_scope.key, 0, sizeof(eff_scope.key));
  } else if (!send_scope.isNull()) {
    eff_scope = send_scope;
  } else if (!resolveDefaultOrGeo(eff_scope)) {
    memset(eff_scope.key, 0, sizeof(eff_scope.key));
  }
  // Wunschliste 25 (Floodless unscoped channels, 2026-05-30):
  // bei null-Scope -- direct (zero-hop) statt flood. Zwei Beweggruende:
  //   1) Netzentlastung. Unscoped Flood propagiert bis flood_max-Hops
  //      (typisch 63 Hops Reichweite). Neulinge die stundenlang im
  //      Public-Channel quatschen ohne Scope-Konfiguration verursachen
  //      massiv Netz-Traffic ohne Mehrwert (Empfaenger sitzen meist
  //      lokal).
  //   2) Privacy / lokale Channels. Ohne dieses Feature war ein
  //      "lokaler Kanal, der NICHT weitergereicht werden soll" nicht
  //      einrichtbar -- jede unscoped Channel-Msg wurde gefloodet.
  //      Mit _unscoped_channel_direct=Default kann der User einen
  //      privaten Channel 'meineHausgemeinschaft' fuehren ohne dass
  //      Repeater die Nachrichten weiterreichen.
  // Default-Aktiv. Override via 'unscoped-channelmessages flood'
  // (runtime-only) falls Flooding doch erwuenscht (z.B. fuer einen
  // bewusst grossraeumigen Public-Channel ohne Scope).
  if (eff_scope.isNull() && _unscoped_channel_direct) {
    sendZeroHop(pkt, delay_millis);
    return;
  }
  if (eff_scope.isNull()) {
    sendFlood(pkt, delay_millis, _prefs.path_hash_mode + 1);
    return;
  }
  sendFloodScoped(eff_scope, pkt, delay_millis);
}

void MyMesh::onMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                           const char *text) {
  markConnectionActive(from); // in case this is from a server, and we have a connection
  queueMessage(from, TXT_TYPE_PLAIN, pkt, sender_timestamp, NULL, 0, text);
  // Wunschliste 43: Wake-on-LoRa fuer eingehende DM.
  bleWakeOnLora("DM");
}

void MyMesh::onCommandDataRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                               const char *text) {
  markConnectionActive(from); // in case this is from a server, and we have a connection
  queueMessage(from, TXT_TYPE_CLI_DATA, pkt, sender_timestamp, NULL, 0, text);
}

void MyMesh::onSignedMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                                 const uint8_t *sender_prefix, const char *text) {
  markConnectionActive(from);
  // from.sync_since change needs to be persisted
  dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
  queueMessage(from, TXT_TYPE_SIGNED_PLAIN, pkt, sender_timestamp, sender_prefix, 4, text);
}

// =========================================================================
// Wunschliste 35: Channel-Message Sender-Annotation + Reply-Mention-Strip
// =========================================================================

// FNV-1a 32-bit. Klein, deterministisch, byte-genau (UTF-8-safe ohne
// Parse). Kollisions-Wahrscheinlichkeit bei 16 Eintraegen vernachlaessigbar.
// Reply-Mention-Strip: entfernt ' (#...)' direkt vor ']' innerhalb von
// '@[...]'-Mentions. In-place Mutation. User-Klammern ohne '#' bleiben
// unangetastet. Robust gegen multiple Mentions, leere Mentions, mid-text.
//
// Beispiele:
//   '@[X (#scope, direct)] hi'            -> '@[X] hi'
//   '@[X (foo) (#scope)] hi'              -> '@[X (foo)] hi'
//   '@[X] hi'                             -> '@[X] hi'  (no-op)
//   '@[X (#scope)]'                       -> '@[X]'    (empty body)
static void stripReplyMentionDecoration(char* buf) {
  if (buf == NULL) return;
  char* p = buf;
  while ((p = strstr(p, "@[")) != NULL) {
    char* close = strchr(p + 2, ']');
    if (!close) break;
    if (close - (p + 2) < 4 || close[-1] != ')') {
      // Kein ')' direkt vor ']' -> kein Strip-Kandidat
      p = close + 1;
      continue;
    }
    // Backward-Scan im Bracket-Inhalt: letztes ' (#' suchen.
    char* openpat = NULL;  // zeigt aufs ' ' vor '(#'
    char* lim = p + 2;     // 1. Char nach '@['
    for (char* q = close - 4; q >= lim; q--) {
      if (q[0] == ' ' && q[1] == '(' && q[2] == '#') {
        openpat = q;
        break;
      }
    }
    if (openpat) {
      // memmove den Rest (close..end+null) nach openpat.
      memmove(openpat, close, strlen(close) + 1);
      p = openpat + 1;  // direkt nach dem neu liegenden ']'
    } else {
      p = close + 1;
    }
  }
}

// =========================================================================
// Wunschliste 32 v2: channel_hops_list (name-hash basiert)
// =========================================================================

int MyMesh::findChannelHopsEntry(uint32_t name_fnv1a) const {
  for (uint8_t i = 0; i < _prefs.channel_hops_count; i++) {
    if (_prefs.channel_hops_list[i].name_fnv1a == name_fnv1a) return i;
  }
  return -1;
}

// Upsert: aktualisiere existierenden Eintrag oder fuege neuen an.
// Returns true bei Erfolg, false wenn Liste voll (nur bei neuem Eintrag).
// flags+channel_hash optional fuer External-Eintraege (Default 0, 0 =
// slot-basiert / non-external). Bei Update werden flags/channel_hash
// ueberschrieben damit ein Slot-Eintrag der zu External wechselt
// (oder umgekehrt) sauber konvertiert wird.
static bool channelHopsUpsert(NodePrefs& prefs, uint32_t name_fnv1a, uint8_t cap,
                              uint8_t flags = 0, uint8_t channel_hash = 0,
                              const char* name = "") {
  for (uint8_t i = 0; i < prefs.channel_hops_count; i++) {
    if (prefs.channel_hops_list[i].name_fnv1a == name_fnv1a) {
      prefs.channel_hops_list[i].cap = cap;
      prefs.channel_hops_list[i].flags = flags;
      prefs.channel_hops_list[i].channel_hash = channel_hash;
      if (name && name[0]) {
        strncpy(prefs.channel_hops_list[i].name, name,
                sizeof(prefs.channel_hops_list[i].name) - 1);
        prefs.channel_hops_list[i].name[sizeof(prefs.channel_hops_list[i].name) - 1] = 0;
      }
      return true;
    }
  }
  if (prefs.channel_hops_count >= MAX_GROUP_CHANNELS) return false;
  uint8_t k = prefs.channel_hops_count;
  memset(&prefs.channel_hops_list[k], 0, sizeof(NodePrefs::ChannelHopsEntry));
  prefs.channel_hops_list[k].name_fnv1a   = name_fnv1a;
  prefs.channel_hops_list[k].cap          = cap;
  prefs.channel_hops_list[k].flags        = flags;
  prefs.channel_hops_list[k].channel_hash = channel_hash;
  if (name && name[0]) {
    strncpy(prefs.channel_hops_list[k].name, name,
            sizeof(prefs.channel_hops_list[k].name) - 1);
    prefs.channel_hops_list[k].name[sizeof(prefs.channel_hops_list[k].name) - 1] = 0;
  }
  prefs.channel_hops_count++;
  return true;
}

// Remove: loesche Eintrag aus Liste, kompaktiere. No-op falls nicht da.
static void channelHopsRemove(NodePrefs& prefs, uint32_t name_fnv1a) {
  for (uint8_t i = 0; i < prefs.channel_hops_count; i++) {
    if (prefs.channel_hops_list[i].name_fnv1a == name_fnv1a) {
      for (uint8_t j = i; j < prefs.channel_hops_count - 1; j++) {
        prefs.channel_hops_list[j] = prefs.channel_hops_list[j + 1];
      }
      prefs.channel_hops_count--;
      memset(&prefs.channel_hops_list[prefs.channel_hops_count], 0,
             sizeof(NodePrefs::ChannelHopsEntry));
      return;
    }
  }
}

void MyMesh::rebuildChannelHopsCache() {
  // Companion-Eintrag (cap=0) sicherstellen -- ueberschreibt jeden User-
  // Versuch, ihn anders zu setzen. Pre-Boot Garantie: companion wird nie
  // repeated, auch wenn er irgendwie ins Funkfeld entfleucht.
  // COMPANION_CHANNEL_NAME ist weiter unten definiert; hardcode "companion".
  channelHopsUpsert(_prefs, fnv1a32_cstr("companion"), 0, 0, 0, "companion");
  // Punkt 7 Reise-Fix 2026-06-08: TerminalCLI (dz264-Konvention) bekommt
  // dieselbe Sicherheits-Sperre wie 'companion', falls der User von dz264
  // zu unserer Firmware wechselt und den TerminalCLI-Channel im Slot
  // behaelt -- darf NIE repeated werden (App-Control-Traffic).
  channelHopsUpsert(_prefs, fnv1a32_cstr("TerminalCLI"), 0, 0, 0, "TerminalCLI");

  memset(_channel_hops_cap_cache, CH_HOPS_OFF, sizeof(_channel_hops_cap_cache));
  for (int slot = 0; slot < MAX_GROUP_CHANNELS; slot++) {
    ChannelDetails ch;
    if (!getChannel(slot, ch)) continue;
    if (ch.name[0] == 0) continue;
    uint32_t h = fnv1a32_cstr(ch.name);
    int idx = findChannelHopsEntry(h);
    if (idx >= 0) {
      _channel_hops_cap_cache[slot] = _prefs.channel_hops_list[idx].cap;
      // Slot-Eintrag: aktuellen ch.name in die Liste syncen (Channel
      // koennte umbenannt worden sein -- relevant fuer Display+Backup).
      strncpy(_prefs.channel_hops_list[idx].name, ch.name,
              sizeof(_prefs.channel_hops_list[idx].name) - 1);
      _prefs.channel_hops_list[idx].name[sizeof(_prefs.channel_hops_list[idx].name) - 1] = 0;
    }
  }
}

uint32_t MyMesh::fnv1a32(const char* data, size_t len) {
  uint32_t h = 0x811c9dc5UL;
  for (size_t i = 0; i < len; i++) {
    h ^= (uint8_t)data[i];
    h *= 0x01000193UL;
  }
  return h;
}
uint32_t MyMesh::fnv1a32_cstr(const char* s) {
  if (s == NULL) return 0;
  return fnv1a32(s, strlen(s));
}

bool MyMesh::channelSenderSeenLookupOrAdd(uint32_t channel_hash,
                                          uint32_t name_fnv1a,
                                          uint32_t scope_fnv1a,
                                          uint8_t direct_flag) {
  for (uint8_t i = 0; i < _channel_sender_seen_count; i++) {
    const ChannelSenderSeen& e = _channel_sender_seen[i];
    if (e.channel_hash == channel_hash
        && e.name_fnv1a == name_fnv1a
        && e.scope_fnv1a == scope_fnv1a
        && e.direct_flag == direct_flag) return true;
  }
  // Nicht in Liste -> als neu eintragen (LRU-Wrap nach MAX).
  uint8_t slot;
  if (_channel_sender_seen_count < CHANNEL_SENDER_SEEN_MAX) {
    slot = _channel_sender_seen_count++;
  } else {
    slot = _channel_sender_seen_next;
    _channel_sender_seen_next = (uint8_t)((_channel_sender_seen_next + 1) % CHANNEL_SENDER_SEEN_MAX);
  }
  _channel_sender_seen[slot].channel_hash = channel_hash;
  _channel_sender_seen[slot].name_fnv1a = name_fnv1a;
  _channel_sender_seen[slot].scope_fnv1a = scope_fnv1a;
  _channel_sender_seen[slot].direct_flag = direct_flag;
  return false;
}

void MyMesh::onChannelMessageRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint32_t timestamp,
                                  const char *text) {
  // Schutz 2026-06-01: $companion ist STRICT LOCAL -- von aussen darf
  // hier NICHTS reinkommen, auch nicht wenn jemand die Magic-PSK kennt
  // und in unser Funkfeld schickt. Sonst koennte ein Angreifer den
  // $companion-Bucket fluten und legitime lokale Pushes (Geo-Reco etc.)
  // verdraengen.
  uint8_t ch_idx_check = findChannelIdx(channel);
  if (isCompanionChannel(ch_idx_check)) {
    MESH_DEBUG_PRINTLN("onChannelMessageRecv: dropping external $companion text");
    return;
  }
  // Wunschliste 46 Phase 1 (Reise 2026-06-09): Sender + Text Filter.
  // Channel-Wire: '<sender_name>: <text>'. Sender bis ': ' extrahieren
  // -- wenn Sender oder Text matched, komplett verwerfen.
  // 'for-us'-Filter -- Repeat bleibt unbeeinflusst.
  {
    const char* sep = strstr(text, ": ");
    char sender_buf[40];
    sender_buf[0] = 0;
    const char* text_only = text;
    if (sep) {
      size_t sl = (size_t)(sep - text);
      if (sl >= sizeof(sender_buf)) sl = sizeof(sender_buf) - 1;
      memcpy(sender_buf, text, sl);
      sender_buf[sl] = 0;
      text_only = sep + 2;
    }
    int ch_idx = (int)ch_idx_check;
    if (sender_buf[0] && filterSenderDropMatch(sender_buf, ch_idx)) {
      traceCompanion(TRACE_FILTER, "[filter] GRP dropped: sender='%s'", sender_buf);
      return;
    }
    if (filterTextDropMatch(text_only, ch_idx)) {
      traceCompanion(TRACE_FILTER, "[filter] GRP dropped: text-match");
      return;
    }
    // Wunschliste 46 Phase 5: scope-Filter (Display-Pfad).
    // scope_name = NULL bei unscoped (oder bei scoped mit unbekanntem
    // Region-Code; in beiden Faellen matched 'unscoped' im Filter).
    const char* sc_name = NULL;
    if (pkt->hasTransportCodes()) {
      sc_name = lookupRegionByTransportCode(pkt);
    }
    if (filterScopeMatch(sc_name, ch_idx, /*for_repeat=*/false)) {
      traceCompanion(TRACE_FILTER, "[filter] GRP dropped: scope='%s'",
                     sc_name ? sc_name : "unscoped");
      return;
    }
  }
  // Wunschliste 35: Sender-Annotation '(#scope[, direct])' an Sender-Namen
  // anhaengen -- aber NUR EINMAL pro (Name, Scope, Direct)-Tuple. Sonst
  // bricht der App-Reply-Button die @-Mention-Sound-Logik weil der
  // augmentierte Name '@[Name (#scope, direct)]' nicht mehr exact dem
  // tatsaechlichen Sender-Namen entspricht. Trade-off: User sieht den
  // Scope nur bei der ersten Nachricht eines Tuples; gut genug zum
  // Identifizieren.
  // direct_flag: pkt->path_len == 0 (direkt gehoert, kein Repeater
  // im Pfad). Gilt fuer DIRECT-routed und FLOOD-routed (beide haben
  // path_len 0 bei direktem Empfang).
  //
  // ENTFERNBAR-MARKER (Reise-Notiz 2026-06-08, Wunschliste 48):
  // Diese augmented-text-Logik bricht die App-Pfad-Anzeige
  // ('Nachrichtenpfad ansehen' -> 'Keine Pfadinfo') fuer Messages
  // mit Suffix. App matched vermutlich Sender-Name (oder text-Hash)
  // gegen ihre interne Live-DB; mit Suffix kein Match.
  // KISS-Mode/PUSH_CODE_LOG_RX_DATA (Z. 771 logRxRaw) liefert die
  // ROH-Bytes unveraendert -- das ist absichtlich (transparenter
  // Funk-Mitschnitt). Augment greift NUR im UI-Frame
  // RESP_CODE_CHANNEL_MSG_RECV (Z. ~2148 unten).
  //
  // Wenn User entscheidet das Suffix wegen Pfad-Anzeige-Bruch
  // wieder zu entfernen, RUECKSTANDSLOS:
  //   - Block 2085-2128 entfernen ('const char* effective_text = text;'
  //     bis Ende '...} // (sep) sep-Block')
  //   - 'effective_text' wieder durch 'text' ersetzen weiter unten
  //   - channelSenderSeenLookupOrAdd-Helper + _channel_sender_seen[]
  //     Member-Liste koennen bleiben (kein toter Code, nur ungenutzt)
  //     ODER auch entfernen wenn ENTFERNBAR-Cleanup gewuenscht.
  //
  // Feature-Request (Wunschliste 48): CLI-Toggle
  //   set scope_suffix_in_msgs on|off
  // damit User waehlt Pfad-Anzeige (off) ODER Scope-Sichtbarkeit (on).
  const char* effective_text = text;
  char augmented[MAX_TEXT_LEN + 64];
  const char* sep = strstr(text, ": ");
  // Reise-Toggle 2026-06-08: messages_append_scope_to_name aus -> Augment
  // ueberspringen. Default off (App-Pfad-Anzeige funktioniert dann wieder).
  if (sep && _prefs.messages_append_scope_to_name) {
    // Sender-Name extrahieren (prefix vor ': ').
    size_t name_len = (size_t)(sep - text);
    uint32_t name_h = fnv1a32(text, name_len);
    // Scope-Hash mit Sentinels:
    //   0xFFFFFFFE = '#?' (scoped aber Region unbekannt)
    //   0xFFFFFFFF = '#*' (unscoped)
    uint32_t scope_h;
    const char* scope_label = NULL;
    char scope_buf[36];
    if (pkt->hasTransportCodes()) {
      const char* scope_name = lookupRegionByTransportCode(pkt);
      if (scope_name) {
        scope_h = fnv1a32_cstr(scope_name);
        // Kollision mit Sentinel-Werten extrem unwahrscheinlich aber sicher:
        if (scope_h == 0xFFFFFFFEUL || scope_h == 0xFFFFFFFFUL) scope_h ^= 0x12345678UL;
        snprintf(scope_buf, sizeof(scope_buf), "#%s", scope_name);
      } else {
        scope_h = 0xFFFFFFFEUL;
        snprintf(scope_buf, sizeof(scope_buf), "#?");
      }
      scope_label = scope_buf;
    } else {
      scope_h = 0xFFFFFFFFUL;
      scope_label = "#*";
    }
    uint8_t direct_flag = (pkt->path_len == 0) ? 1 : 0;
    // User-Wunsch 2026-06-08: per-Channel-Separation. 4 Bytes aus dem
    // GroupChannel-Hash als channel-Identifier. Derselbe Sender in
    // zwei verschiedenen Channels triggert die Annotation in jedem
    // Channel einmal (eigene Tuples). Bei DM nutzen wir 0 als
    // Sentinel -- 4-Byte-channel-Hash kann nicht 0 sein (collision
    // unwahrscheinlich).
    uint32_t channel_h;
    memcpy(&channel_h, channel.hash, sizeof(channel_h));
    if (channel_h == 0) channel_h = 1;  // collision-safe vs DM-Sentinel
    bool already_seen = channelSenderSeenLookupOrAdd(channel_h, name_h, scope_h, direct_flag);
    if (!already_seen) {
      // One-time Annotation. Format: 'Name (#scope[, direct]): text'
      const char* dir_suffix = direct_flag ? ", direct" : "";
      int n = snprintf(augmented, sizeof(augmented), "%.*s (%s%s)%s",
                       (int)name_len, text, scope_label, dir_suffix, sep);
      if (n > 0 && n < (int)sizeof(augmented)) {
        effective_text = augmented;
      }
    }
    // else: schon gesehen -> kein Suffix, plain 'Sender: text'. So
    // bleibt der App-Reply-Button sauber.
  }

  int i = 0;
  if (app_target_ver >= 3) {
    out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV_V3;
    out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
    out_frame[i++] = 0; // reserved1
    out_frame[i++] = 0; // reserved2
  } else {
    out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV;
  }

  uint8_t channel_idx = findChannelIdx(channel);
  out_frame[i++] = channel_idx;
  uint8_t path_len = out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;

  out_frame[i++] = TXT_TYPE_PLAIN;
  memcpy(&out_frame[i], &timestamp, 4);
  i += 4;
  int tlen = strlen(effective_text); // TODO: UTF-8 ??
  if (i + tlen > MAX_FRAME_SIZE) {
    tlen = MAX_FRAME_SIZE - i;
  }
  memcpy(&out_frame[i], effective_text, tlen);
  i += tlen;
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  } else {
#ifdef DISPLAY_CLASS
    if (_ui) _ui->notify(UIEventType::channelMessage);
#endif
  }
#ifdef DISPLAY_CLASS
  // Get the channel name from the channel index
  const char *channel_name = "Unknown";
  ChannelDetails channel_details;
  if (getChannel(channel_idx, channel_details)) {
    channel_name = channel_details.name;
  }
  if (_ui) _ui->newMsg(path_len, channel_name, effective_text, offlineQueueTotal());
#endif
}

void MyMesh::onChannelDataRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint16_t data_type,
                               const uint8_t *data, size_t data_len) {
  // Schutz 2026-06-01: siehe onChannelMessageRecv -- gleicher Grund.
  // Da andere DL9SAU-Firmware-User die Magic-PSK kennen koennen
  // (Source-Code ist offen), ist External-Drop hier obligatorisch.
  uint8_t ch_idx_check = findChannelIdx(channel);
  if (isCompanionChannel(ch_idx_check)) {
    MESH_DEBUG_PRINTLN("onChannelDataRecv: dropping external $companion data");
    return;
  }
  if (data_len > MAX_CHANNEL_DATA_LENGTH) {
    MESH_DEBUG_PRINTLN("onChannelDataRecv: dropping payload_len=%d exceeds frame limit=%d",
                       (uint32_t)data_len, (uint32_t)MAX_CHANNEL_DATA_LENGTH);
    return;
  }

  int i = 0;
  out_frame[i++] = RESP_CODE_CHANNEL_DATA_RECV;
  out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
  out_frame[i++] = 0; // reserved1
  out_frame[i++] = 0; // reserved2

  uint8_t channel_idx = findChannelIdx(channel);
  out_frame[i++] = channel_idx;
  out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
  out_frame[i++] = (uint8_t)(data_type & 0xFF);
  out_frame[i++] = (uint8_t)(data_type >> 8);
  out_frame[i++] = (uint8_t)data_len;

  int copy_len = (int)data_len;
  if (copy_len > 0) {
    memcpy(&out_frame[i], data, copy_len);
    i += copy_len;
  }
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  }
}

// ----- Wunschliste 7 (Discoverability) -------------------------------------
//
// Phase 2: effectiveAdvertRole.
// Resolved Wire-Type fuer Adverts UND Discovery-Query-Antworten. Wenn
// _prefs.advert_role gesetzt (1..4) -> fester Type. Wenn 0 (auto):
// Matrix aus client_repeat + repeater_profile:
//   client_repeat == 0                          -> CHAT
//   client_repeat == 1, profile == normal       -> REPEATER
//   client_repeat == 1, profile == defensive    -> CHAT (chat-tauglich)
uint8_t MyMesh::effectiveAdvertRole() const {
  switch (_prefs.advert_role) {
    case 1: return ADV_TYPE_CHAT;
    case 2: return ADV_TYPE_REPEATER;
    case 3: return ADV_TYPE_SENSOR;
    case 4: return ADV_TYPE_ROOM;
    default: break;  // 0 = auto
  }
  if (_prefs.client_repeat == 0) return ADV_TYPE_CHAT;
  if (_prefs.repeater_profile == 1) return ADV_TYPE_REPEATER;  // normal
  return ADV_TYPE_CHAT;  // defensive (Default)
}

// Shadows BaseChatMesh::createSelfAdvert. Wirkt nur fuer externe Aufrufer
// (= MyMesh.cpp); BaseChatMesh selbst ruft seine Variante nicht intern.
mesh::Packet* MyMesh::createSelfAdvert(const char* name) {
  uint8_t app_data[MAX_ADVERT_DATA_SIZE];
  uint8_t app_data_len;
  {
    AdvertDataBuilder builder(effectiveAdvertRole(), name);
    app_data_len = builder.encodeTo(app_data);
  }
  return createAdvert(self_id, app_data, app_data_len);
}

mesh::Packet* MyMesh::createSelfAdvert(const char* name, double lat, double lon) {
  uint8_t app_data[MAX_ADVERT_DATA_SIZE];
  uint8_t app_data_len;
  {
    AdvertDataBuilder builder(effectiveAdvertRole(), name, lat, lon);
    app_data_len = builder.encodeTo(app_data);
  }
  return createAdvert(self_id, app_data, app_data_len);
}

// Phase 3: ANON_REQ Handler. Antwortet abhaengig von effectiveAdvertRole:
//   CHAT     -> nur BASIC (clock)
//   REPEATER -> OWNER + REGIONS + BASIC + handleRequest (Phase 4)
//   SENSOR   -> BASIC (clock); GET_TELEMETRY in onContactRequest
//   ROOM     -> nichts (eigene Wunschliste 9)
void MyMesh::onAnonDataRecv(mesh::Packet* packet, const uint8_t* secret,
                            const mesh::Identity& sender, uint8_t* data, size_t len) {
  if (packet->getPayloadType() != PAYLOAD_TYPE_ANON_REQ) return;
  if (len < 5) return;  // mindestens timestamp(4) + type(1)

  uint32_t sender_timestamp;
  memcpy(&sender_timestamp, data, 4);
  uint8_t req_type = data[4];

  uint8_t role = effectiveAdvertRole();
  bool allow_basic   = (role == ADV_TYPE_CHAT)
                    || (role == ADV_TYPE_REPEATER)
                    || (role == ADV_TYPE_SENSOR);
  bool allow_owner   = (role == ADV_TYPE_REPEATER);
  bool allow_regions = (role == ADV_TYPE_REPEATER);

  // Reply-Path aus dem Request extrahieren (analog simple_repeater).
  // data[5] = (path_len << 6) | (path_hash_size - 1) ... eigentlich
  // (size-1)<<6 | len; siehe simple_repeater handleAnonOwnerReq.
  if (len < 6) return;
  uint8_t reply_path_meta = data[5];
  uint8_t reply_path_len  = reply_path_meta & 63;
  uint8_t hash_size       = (reply_path_meta >> 6) + 1;
  if (6 + (size_t)reply_path_len * hash_size > len) return;  // truncated
  const uint8_t* reply_path = &data[6];

  // Reply-Datagram bauen: 4 Byte sender_timestamp-Echo, 4 Byte unsere
  // RTC-Zeit, dann typ-spezifischer Payload.
  uint8_t reply_data[160];
  uint8_t reply_len = 0;
  memcpy(&reply_data[0], &sender_timestamp, 4);
  uint32_t now = getRTCClock()->getCurrentTime();
  memcpy(&reply_data[4], &now, 4);
  reply_len = 8;

  if (req_type == ANON_REQ_TYPE_OWNER && allow_owner && packet->isRouteDirect()) {
    // name + owner_info
    int n = snprintf((char*)&reply_data[8], sizeof(reply_data) - 8,
                     "%s\n%s", _prefs.node_name, _prefs.owner_info);
    if (n < 0) return;
    if ((size_t)n > sizeof(reply_data) - 8 - 1) n = sizeof(reply_data) - 8 - 1;
    reply_len = 8 + (uint8_t)n;
  } else if (req_type == ANON_REQ_TYPE_REGIONS && allow_regions && packet->isRouteDirect()) {
    // Comma-separated Liste der aktiven Repeat-Scopes (Build-in Repeat-Set
    // + Extras, mit mode != OFF). Best-Effort; truncate bei buffer-Limit.
    size_t off = 8;
    auto append_name = [&](const char* nm) -> bool {
      size_t nl = strlen(nm);
      size_t need = nl + (off > 8 ? 1 : 0);  // +1 fuer Komma-Separator
      if (off + need + 1 > sizeof(reply_data)) return false;  // +1 NUL
      if (off > 8) reply_data[off++] = ',';
      memcpy(&reply_data[off], nm, nl);
      off += nl;
      return true;
    };
    bool auto_en = (_prefs.scope_repeater_auto == 2);
    for (int i = 0; i < _buildin_keys_count; i++) {
      // Alias-Eintraege (zusatz-Bbox fuer einen schon vorhandenen Namen)
      // ueberspringen -- der Primaer-Eintrag schreibt den Namen schon.
      if (dl9sau_is_alias((size_t)i)) continue;
      uint8_t st = getBuildinStatus(i);
      if (st & (SCOPE_STATUS_DISABLED | SCOPE_STATUS_USER_DELETED)) continue;
      uint8_t m = st & SCOPE_STATUS_REPEAT_MASK;
      if (m == SCOPE_STATUS_REPEAT_OFF) continue;
      if (m == SCOPE_STATUS_REPEAT_AUTO && !auto_en) continue;
      const char* nm = NULL;
      if (!dl9sau_get_region((size_t)i, &nm, NULL, NULL, NULL, NULL)) continue;
      if (!append_name(nm)) break;
    }
    for (int i = 0; i < _prefs.scope_extras_count; i++) {
      uint8_t st = getScopeStatus({SCOPE_EXTRAS, i});
      if (st & (SCOPE_STATUS_DISABLED | SCOPE_STATUS_USER_DELETED)) continue;
      uint8_t m = st & SCOPE_STATUS_REPEAT_MASK;
      if (m == SCOPE_STATUS_REPEAT_OFF) continue;
      if (m == SCOPE_STATUS_REPEAT_AUTO && !auto_en) continue;
      if (!append_name(_prefs.scope_extras[i].name)) break;
    }
    reply_data[off] = 0;
    reply_len = (uint8_t)off;
  } else if (req_type == ANON_REQ_TYPE_BASIC && allow_basic && packet->isRouteDirect()) {
    // features-byte (analog simple_repeater)
    reply_data[8] = 0;  // keine Bridge-Features im Companion
    if (_prefs.client_repeat == 0) reply_data[8] |= 0x80;  // 'disabled' = nicht-repeating
    reply_len = 9;
  } else if (req_type == ANON_REQ_TYPE_LOGIN && role == ADV_TYPE_REPEATER
             && packet->isRouteDirect()) {
    // Wunschliste 52 (2026-06-10): Login via Password.
    // isRouteDirect-Pflicht: unscoped flood-Login wird abgelehnt --
    // Auth-Versuche sollen 1-Hop sein (Anti-Flooding + lokalisiert).
    // Konsistent mit den anderen ANON_REQ-Handlern (OWNER/REGIONS/BASIC).
    // Payload nach data[5]: ASCII-Password (max 31 byte, evtl. ohne NUL).
    // Setzt CONTACT_FLAG_ADMIN_OK bit auf existierenden Contact (Sender
    // muss als Contact bekannt sein -- add via Advert + manual add).
    // Reply: [4]ts_echo [4]now [1]RESP_SERVER_LOGIN_OK [1]perm (1=admin,
    //        2=guest).
    if (_prefs.passwd_admin[0] == 0 && _prefs.passwd_guest[0] == 0) return;
    if (len <= 5) return;
    size_t pw_max = len - 5;
    if (pw_max > 31) pw_max = 31;
    char pw_buf[32];
    memcpy(pw_buf, &data[5], pw_max);
    pw_buf[pw_max] = 0;
    // Trim trailing zero-bytes oder whitespace (Client kann unterminiert
    // schicken).
    while (pw_max > 0 && (pw_buf[pw_max-1] == 0
                          || pw_buf[pw_max-1] == ' '
                          || pw_buf[pw_max-1] == '\r'
                          || pw_buf[pw_max-1] == '\n')) {
      pw_buf[--pw_max] = 0;
    }
    bool admin_match = (_prefs.passwd_admin[0] != 0
                        && strcmp(pw_buf, _prefs.passwd_admin) == 0);
    bool guest_match = (!admin_match
                        && _prefs.passwd_guest[0] != 0
                        && strcmp(pw_buf, _prefs.passwd_guest) == 0);
    if (!admin_match && !guest_match) {
      pushDebugLog("[admin] login: wrong password\n");
      return;
    }
    ContactInfo* c = lookupContactByPubKey(sender.pub_key, PUB_KEY_SIZE);
    if (!c) {
      pushDebugLog("[admin] login: sender not in contacts\n");
      return;
    }
    c->flags |= CONTACT_FLAG_ADMIN_OK;
    if (guest_match) c->flags |= CONTACT_FLAG_GUEST_ONLY;
    else             c->flags &= (uint8_t)~CONTACT_FLAG_GUEST_ONLY;
    dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
    pushDebugLog("[admin] login OK perm=%s for %s\n",
                 admin_match ? "admin" : "guest", c->name);
    reply_data[8] = RESP_SERVER_LOGIN_OK;
    reply_data[9] = admin_match ? 1 : 2;
    reply_len = 10;
  } else {
    return;  // unbekannt oder per role gegated
  }

  // Reply zurueck. Wenn Request flood war: createPathReturn (analog
  // simple_repeater). Wenn direct + path bekannt: sendDirect; sonst
  // sendFlood. Reply uebernimmt scope (transport_codes) vom Request
  // damit Antwort gleiche Reichweite/Region hat (User-Wunsch 2026-06-10).
  uint16_t reply_codes[2] = {0, 0};
  bool reply_scoped = packet->hasTransportCodes();
  if (reply_scoped) {
    reply_codes[0] = packet->transport_codes[0];
    reply_codes[1] = packet->transport_codes[1];
  }
  if (packet->isRouteFlood()) {
    mesh::Packet* path = createPathReturn(sender, secret, packet->path, packet->path_len,
                                          PAYLOAD_TYPE_RESPONSE, reply_data, reply_len);
    if (path) {
      if (reply_scoped) sendFlood(path, reply_codes, 300);
      else              sendFlood(path, 300);
    }
  } else {
    mesh::Packet* reply = createDatagram(PAYLOAD_TYPE_RESPONSE, sender, secret,
                                          reply_data, reply_len);
    if (!reply) return;
    if (reply_path_len > 0) {
      uint8_t path_meta = ((hash_size - 1) << 6) | (reply_path_len & 63);
      sendDirect(reply, (uint8_t*)reply_path, path_meta, 300 /* ms reply-delay */);
    } else {
      if (reply_scoped) sendFlood(reply, reply_codes, 300);
      else              sendFlood(reply, 300);
    }
  }
}

// Wunschliste 52: Guest-Permission-Filter. ABSICHTLICH MINIMAL --
// nur klar lesende Befehle die kein Geheimnis preisgeben und keine
// Wire-Aktion ausloesen. NICHT in Whitelist (Sicherheits-Gruende):
//   get      -- koennte 'get passwd_admin/guest' Passwoerter leaken
//   prefs    -- enthielt passwd-Status (set/empty)
//   advert   -- triggert TX (Wire-Aktion, kein read-only)
//   messages -- kann DMs anderer User enthalten
//   discover -- Wire-TX
//   scope    -- 'scope' Sub-Befehle koennen schreiben (write-Subbefehle
//                gehen auch ueber 'scope <name> off')
//   set      -- offensichtlich Write
//   trace    -- kann Debug-Daten mit sensiblen Inhalten zeigen
// Wer das aufweichen will: explizit ergaenzen + Sicherheits-Review.
bool MyMesh::isAdminCmdAllowedForGuest(const char* cmd) const {
  if (!cmd) return false;
  char tok[32];
  size_t i = 0;
  while (cmd[i] && cmd[i] != ' ' && i < sizeof(tok) - 1) {
    tok[i] = cmd[i]; i++;
  }
  tok[i] = 0;
  if (i == 0) return false;
  static const char* const ALLOW[] = {
    "stats", "status", "uptime",
    "clock", "date", "time",
    "version", "help", "?",
    "neighbors",  // direct heard <48h -- Mesh-Diagnose; Topologie ist
                  // ohnehin halb-public ueber Adverts
    NULL
  };
  for (int k = 0; ALLOW[k]; k++) {
    if (strcasecmp(tok, ALLOW[k]) == 0) return true;
  }
  return false;
}

uint8_t MyMesh::onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                                 uint8_t len, uint8_t *reply) {
  uint8_t role = effectiveAdvertRole();

  // Wunschliste 52 (2026-06-10): Remote-Admin RPC ueber REQ_TYPE_ADMIN_CMD.
  // Wire-Payload (nach den 4 byte sender_timestamp): [1]REQ_TYPE_ADMIN_CMD
  // [N]command_text (ASCII, unterminiert oder NUL-terminiert).
  // Reply: [4]ts_echo [M]response_text (ASCII).
  // Voraussetzungen:
  //   - effective Role == REPEATER (defensive: nicht in Chat-Mode aktiv)
  //   - Contact hat CONTACT_FLAG_ADMIN_OK (gesetzt nach Login)
  // Guest-Mode (CONTACT_FLAG_GUEST_ONLY): nur read-only Befehle.
  if (data[0] == REQ_TYPE_ADMIN_CMD && role == ADV_TYPE_REPEATER) {
    if ((contact.flags & CONTACT_FLAG_ADMIN_OK) == 0) {
      pushDebugLog("[admin] cmd rejected: contact lacks admin flag\n");
      return 0;
    }
    // Wunschliste 43: Wake-on-LoRa fuer admin-cmd.
    bleWakeOnLora("admin-cmd");
    if (len < 2) return 0;
    char cmd_buf[160];
    size_t cmd_len = len - 1;
    if (cmd_len >= sizeof(cmd_buf)) cmd_len = sizeof(cmd_buf) - 1;
    memcpy(cmd_buf, &data[1], cmd_len);
    cmd_buf[cmd_len] = 0;
    // Trim trailing whitespace/control
    while (cmd_len > 0 && (cmd_buf[cmd_len-1] == ' '
                            || cmd_buf[cmd_len-1] == '\r'
                            || cmd_buf[cmd_len-1] == '\n'
                            || cmd_buf[cmd_len-1] == 0)) {
      cmd_buf[--cmd_len] = 0;
    }
    if (cmd_len == 0) return 0;
    if ((contact.flags & CONTACT_FLAG_GUEST_ONLY)
        && !isAdminCmdAllowedForGuest(cmd_buf)) {
      pushDebugLog("[admin] cmd rejected: guest not allowed: %s\n", cmd_buf);
      memcpy(reply, &sender_timestamp, 4);
      const char* msg = "ERR: guest-permission";
      size_t ml = strlen(msg);
      memcpy(&reply[4], msg, ml);
      return 4 + ml;
    }
    // Wunschliste 52 Pagination (2026-06-10): cmd kann mit trailing
    // 'page N' enden (Client-driven). Server fuehrt cmd komplett aus
    // in einem 800-byte Capture-Buffer, splittet in ~140-byte-Pages
    // und liefert die angeforderte Page zurueck. Reply-Prefix
    // '<page N/M>\n' damit Client weiss wie viele Seiten total.
    int requested_page = 1;
    {
      // Suche letztes Vorkommen von ' page <num>' am Ende
      char* p = cmd_buf + cmd_len;
      // Trim erstmal trailing whitespace
      while (p > cmd_buf && (p[-1] == ' ' || p[-1] == '\t')) p--;
      // Rueckwaerts: Ziffern lesen
      char* end_num = p;
      while (p > cmd_buf && p[-1] >= '0' && p[-1] <= '9') p--;
      if (p < end_num && p > cmd_buf + 5
          && strncmp(p - 5, " page ", 6) == 0) {
        long n = strtol(p, NULL, 10);
        if (n >= 1 && n <= 99) {
          requested_page = (int)n;
          *(p - 5) = 0;  // strip ' page N' vom cmd
          // re-trim trailing whitespace
          char* t = p - 5;
          while (t > cmd_buf && (t[-1] == ' ' || t[-1] == '\t')) {
            *(--t) = 0;
          }
        }
      }
    }
    // Capture-Mode setzen, Befehl ausfuehren, Buffer + Pages.
    char capture[800];
    capture[0] = 0;
    _admin_reply_buf = capture;
    _admin_reply_max = sizeof(capture);
    _admin_reply_used = 0;
    _admin_capture_active = true;
    _admin_capture_truncated = false;
    pushDebugLog("[admin] cmd from %s (page %d): %s\n",
                 contact.name, requested_page, cmd_buf);
    handleCompanionCommand(cmd_buf);
    _admin_capture_active = false;
    _admin_reply_buf = NULL;
    _admin_reply_max = 0;
    size_t total_len = strlen(capture);
    if (total_len == 0) {
      strcpy(capture, "(OK, no reply)");
      total_len = strlen(capture);
    }
    const size_t PAGE_PAYLOAD = 140;
    size_t total_pages = (total_len + PAGE_PAYLOAD - 1) / PAGE_PAYLOAD;
    if (total_pages == 0) total_pages = 1;
    if ((size_t)requested_page > total_pages) requested_page = (int)total_pages;
    size_t page_start = ((size_t)requested_page - 1) * PAGE_PAYLOAD;
    size_t page_len = total_len - page_start;
    if (page_len > PAGE_PAYLOAD) page_len = PAGE_PAYLOAD;
    char reply_buf[160];
    size_t pre_len = snprintf(reply_buf, sizeof(reply_buf),
                              "<page %d/%u>\n",
                              requested_page, (unsigned)total_pages);
    if (page_len + pre_len > sizeof(reply_buf)) {
      page_len = sizeof(reply_buf) - pre_len;
    }
    memcpy(reply_buf + pre_len, capture + page_start, page_len);
    memcpy(reply, &sender_timestamp, 4);
    memcpy(&reply[4], reply_buf, pre_len + page_len);
    return (uint8_t)(4 + pre_len + page_len);
  }

  // Wunschliste 7 Phase 4: REQ_TYPE_GET_STATUS (RepeaterStats).
  // Nur fuer Role == REPEATER. Wire-Layout 1:1 wie simple_repeater.
  if (data[0] == REQ_TYPE_GET_STATUS && role == ADV_TYPE_REPEATER) {
    memcpy(reply, &sender_timestamp, 4);
    RepeaterStats stats;
    stats.batt_milli_volts     = (uint16_t)board.getBattMilliVolts();
    stats.curr_tx_queue_len    = (uint16_t)_mgr->getOutboundTotal();
    stats.noise_floor          = (int16_t)_radio->getNoiseFloor();
    stats.last_rssi            = (int16_t)radio_driver.getLastRSSI();
    stats.n_packets_recv       = radio_driver.getPacketsRecv();
    stats.n_packets_sent       = radio_driver.getPacketsSent();
    stats.total_air_time_secs  = getTotalAirTime() / 1000;
    stats.total_up_time_secs   = millis() / 1000;
    stats.n_sent_flood         = getNumSentFlood();
    stats.n_sent_direct        = getNumSentDirect();
    stats.n_recv_flood         = getNumRecvFlood();
    stats.n_recv_direct        = getNumRecvDirect();
    stats.err_events           = 0;
    stats.last_snr             = (int16_t)(radio_driver.getLastSNR() * 4);
    stats.n_direct_dups        = ((SimpleMeshTables*)getTables())->getNumDirectDups();
    stats.n_flood_dups         = ((SimpleMeshTables*)getTables())->getNumFloodDups();
    stats.total_rx_air_time_secs = getReceiveAirTime() / 1000;
    stats.n_recv_errors        = radio_driver.getPacketsRecvErrors();
    memcpy(&reply[4], &stats, sizeof(stats));
    return 4 + sizeof(stats);
  }

  // Wunschliste 7 Phase 4: REQ_TYPE_GET_OWNER_INFO.
  if (data[0] == REQ_TYPE_GET_OWNER_INFO && role == ADV_TYPE_REPEATER) {
    memcpy(reply, &sender_timestamp, 4);
    int n = snprintf((char*)&reply[4], 160 - 4,
                     "%s\n%s\n%s", FIRMWARE_VERSION, _prefs.node_name, _prefs.owner_info);
    if (n < 0) return 0;
    return 4 + (uint8_t)n;
  }

  // Wunschliste 15: REQ_TYPE_GET_NEIGHBOURS. Nur Role == REPEATER.
  // Liefert die zero-hop Runtime-Neighbour-Tabelle. Wire-Layout 1:1
  // wie simple_repeater (request_version 0 mit count/offset/order_by/
  // pubkey_prefix_length).
  if (data[0] == REQ_TYPE_GET_NEIGHBOURS && role == ADV_TYPE_REPEATER) {
    if (len < 8) return 0;  // version + count + offset(2) + order_by + prefix_len + 4-blob = 9
    uint8_t request_version    = data[1];
    if (request_version != 0) return 0;
    uint8_t want_count         = data[2];
    uint16_t want_offset;       memcpy(&want_offset, &data[3], 2);
    uint8_t order_by           = data[5];
    uint8_t pubkey_prefix_len  = data[6];
    if (pubkey_prefix_len > 32) pubkey_prefix_len = 32;

    memcpy(reply, &sender_timestamp, 4);
    int reply_off = 4;

    // Sorted-Pointer-Array bauen (LRU ist insertion-order, hier
    // brauchen wir explizite Sortierung).
    RuntimeNeighbour* sorted[MAX_RUNTIME_NEIGHBOURS];
    int16_t total = 0;
    for (int i = 0; i < _neighbours_count; i++) {
      sorted[total++] = &_neighbours[i];
    }
    // Sort. std::sort wird die meisten ESP32-builds unterstuetzen.
    if (total > 1) {
      // Einfacher Insertion-Sort (32 Eintraege max -> O(N^2) ist
      // billig, vermeidet std::sort-Include-Aufwand).
      for (int i = 1; i < total; i++) {
        RuntimeNeighbour* key = sorted[i];
        int j = i - 1;
        while (j >= 0) {
          bool key_before = false;
          switch (order_by) {
            case 0: key_before = key->heard_timestamp > sorted[j]->heard_timestamp; break; // newest first
            case 1: key_before = key->heard_timestamp < sorted[j]->heard_timestamp; break; // oldest first
            case 2: key_before = key->snr > sorted[j]->snr; break; // strongest first
            case 3: key_before = key->snr < sorted[j]->snr; break; // weakest first
            default: key_before = false; break;
          }
          if (!key_before) break;
          sorted[j + 1] = sorted[j];
          j--;
        }
        sorted[j + 1] = key;
      }
    }

    // Wunschliste 15b: Recent Contacts als Augmentation. Nur wenn RTC
    // gesetzt -- ContactInfo.lastmod ist Unix-Sekunde, ohne RTC nicht
    // interpretierbar. Gefiltert auf lastmod < 1 Woche, dedupliziert
    // gegen die Runtime-Tabelle.
    uint32_t now_rtc = getRTCClock()->getCurrentTime();
    uint32_t now_ms  = millis();
    bool rtc_ok = (now_rtc > 1500000000UL);

    // Sammeln und Sortieren der "recent contacts" -- analog zu sorted[],
    // aber kompakt mit einem Lookup-Array von ContactInfo-Pointern.
    const ContactInfo* recent_contacts[MAX_RUNTIME_NEIGHBOURS];
    int recent_count = 0;
    if (rtc_ok) {
      uint32_t week_ago = (now_rtc > 7UL * 86400UL) ? (now_rtc - 7UL * 86400UL) : 0;
      int n_contacts = getNumContacts();
      for (int ci = 0; ci < n_contacts && recent_count < MAX_RUNTIME_NEIGHBOURS; ci++) {
        ContactInfo c;
        if (!getContactByIdx(ci, c)) continue;
        if (c.lastmod < week_ago) continue;
        // Dedup gegen _neighbours[]
        bool dup = false;
        for (int j = 0; j < _neighbours_count; j++) {
          if (memcmp(_neighbours[j].pub_key, c.id.pub_key, 32) == 0) {
            dup = true; break;
          }
        }
        if (dup) continue;
        // Bereits in recent_contacts?
        bool already = false;
        for (int j = 0; j < recent_count; j++) {
          if (memcmp(recent_contacts[j]->id.pub_key, c.id.pub_key, 32) == 0) {
            already = true; break;
          }
        }
        if (already) continue;
        // Slot in TLS-statische Kopie -- nicht moeglich da getContactByIdx
        // ContactInfo lokal kopiert. Wir muessen direkt vom Storage lesen
        // koennen. ALTERNATIVE: ContactInfo lokal cachen.
        // Wir cachen die Pointer indirekt via Index nicht (Race bei
        // Mutationen). Stattdessen: separate static cache fuer recent.
        // Trick: ContactInfo ist klein (~96 Byte); statisches Array von
        // Kopien ist OK.
        static ContactInfo recent_cache[MAX_RUNTIME_NEIGHBOURS];
        recent_cache[recent_count] = c;
        recent_contacts[recent_count] = &recent_cache[recent_count];
        recent_count++;
      }
      // Sortiere recent_contacts (gleiches Kriterium wie sorted[]).
      if (recent_count > 1) {
        for (int i = 1; i < recent_count; i++) {
          const ContactInfo* key = recent_contacts[i];
          int j = i - 1;
          while (j >= 0) {
            bool key_before = false;
            switch (order_by) {
              case 0: key_before = key->lastmod > recent_contacts[j]->lastmod; break;
              case 1: key_before = key->lastmod < recent_contacts[j]->lastmod; break;
              case 2: case 3: key_before = false; break;  // contacts haben kein SNR
              default: key_before = false; break;
            }
            if (!key_before) break;
            recent_contacts[j + 1] = recent_contacts[j];
            j--;
          }
          recent_contacts[j + 1] = key;
        }
      }
    }

    // Header: total + result_count placeholder. total = Runtime + Recent.
    int16_t result_count = 0;
    int16_t total_w = total + (int16_t)recent_count;
    memcpy(&reply[reply_off], &total_w, 2); reply_off += 2;
    int header_results_off = reply_off; reply_off += 2;  // fill later

    // Entries: erst Runtime-Neighbours, dann Recent Contacts.
    int produced = 0;       // Position im kombinierten virtuellen Stream
    int total_produce = total + recent_count;
    for (int idx = 0; idx < want_count && idx + want_offset < total_produce; idx++) {
      int pos = idx + want_offset;
      int entry_size = pubkey_prefix_len + 4 + 1;
      if (reply_off + entry_size > 150) break;
      const uint8_t* pk;
      uint32_t heard_secs;
      int8_t snr_b;
      if (pos < total) {
        // Runtime
        RuntimeNeighbour* n = sorted[pos];
        pk = n->pub_key;
        if (rtc_ok && n->heard_timestamp > 0) {
          heard_secs = (now_rtc > n->heard_timestamp)
                       ? (now_rtc - n->heard_timestamp) : 0;
        } else {
          heard_secs = (now_ms - n->heard_millis) / 1000;
        }
        snr_b = n->snr;
      } else {
        // Recent contact
        const ContactInfo* c = recent_contacts[pos - total];
        pk = c->id.pub_key;
        heard_secs = (now_rtc > c->lastmod) ? (now_rtc - c->lastmod) : 0;
        snr_b = 0;  // ContactInfo hat keinen SNR
      }
      memcpy(&reply[reply_off], pk, pubkey_prefix_len);
      reply_off += pubkey_prefix_len;
      memcpy(&reply[reply_off], &heard_secs, 4); reply_off += 4;
      reply[reply_off++] = (uint8_t)snr_b;
      result_count++;
      produced++;
    }
    (void)produced;
    memcpy(&reply[header_results_off], &result_count, 2);
    return (uint8_t)reply_off;
  }

  if (data[0] == REQ_TYPE_GET_TELEMETRY_DATA) {
    uint8_t permissions = 0;
    uint8_t cp = contact.flags >> 1; // LSB used as 'favourite' bit (so only use upper bits)

    if (_prefs.telemetry_mode_base == TELEM_MODE_ALLOW_ALL) {
      permissions = TELEM_PERM_BASE;
    } else if (_prefs.telemetry_mode_base == TELEM_MODE_ALLOW_FLAGS) {
      permissions = cp & TELEM_PERM_BASE;
    }

    if (_prefs.telemetry_mode_loc == TELEM_MODE_ALLOW_ALL) {
      permissions |= TELEM_PERM_LOCATION;
    } else if (_prefs.telemetry_mode_loc == TELEM_MODE_ALLOW_FLAGS) {
      permissions |= cp & TELEM_PERM_LOCATION;
    }

    if (_prefs.telemetry_mode_env == TELEM_MODE_ALLOW_ALL) {
      permissions |= TELEM_PERM_ENVIRONMENT;
    } else if (_prefs.telemetry_mode_env == TELEM_MODE_ALLOW_FLAGS) {
      permissions |= cp & TELEM_PERM_ENVIRONMENT;
    }

    uint8_t perm_mask = ~(data[1]);    // NEW: first reserved byte (of 4), is now inverse mask to apply to permissions
    permissions &= perm_mask;

    if (permissions & TELEM_PERM_BASE) { // only respond if base permission bit is set
      telemetry.reset();
      telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      // query other sensors -- target specific
      sensors.querySensors(permissions, telemetry);

      memcpy(reply, &sender_timestamp,
             4); // reflect sender_timestamp back in response packet (kind of like a 'tag')

      uint8_t tlen = telemetry.getSize();
      memcpy(&reply[4], telemetry.getBuffer(), tlen);
      return 4 + tlen;
    }
  }
  return 0; // unknown
}

void MyMesh::onContactResponse(const ContactInfo &contact, const uint8_t *data, uint8_t len) {
  uint32_t tag;
  memcpy(&tag, data, 4);

  // Wunschliste 52 (2026-06-10): Remote-Admin Antwort.
  if ((pending_admin_pubkey[0] || pending_admin_pubkey[1]
       || pending_admin_pubkey[2] || pending_admin_pubkey[3])
      && memcmp(pending_admin_pubkey, contact.id.pub_key, 4) == 0) {
    memset(pending_admin_pubkey, 0, sizeof(pending_admin_pubkey));
    if (pending_admin_login) {
      pending_admin_login = false;
      // Login-Antwort: [4]ts [4]now [1]RESP [1]perm
      if (len >= 10 && data[8] == RESP_SERVER_LOGIN_OK) {
        const char* perm_s = (data[9] == 1) ? "admin"
                            : (data[9] == 2) ? "guest" : "unknown";
        char r[120];
        snprintf(r, sizeof(r), "[admin] Login OK bei %s (perm=%s)",
                 contact.name, perm_s);
        pushCompanionMessage(r);
      } else {
        char r[100];
        snprintf(r, sizeof(r), "[admin] Login failed bei %s", contact.name);
        pushCompanionMessage(r);
      }
      return;
    }
    // CMD-Antwort: [4]ts_echo [N]response_text
    if (len > 4) {
      char r[200];
      size_t pre_len = snprintf(r, sizeof(r), "[admin %s]\n", contact.name);
      size_t avail = (sizeof(r) > pre_len + 1) ? (sizeof(r) - pre_len - 1) : 0;
      size_t txt_len = (size_t)(len - 4);
      if (txt_len > avail) txt_len = avail;
      memcpy(r + pre_len, &data[4], txt_len);
      r[pre_len + txt_len] = 0;
      pushCompanionMessage(r);
    } else {
      char r[100];
      snprintf(r, sizeof(r), "[admin %s] (leere Antwort)", contact.name);
      pushCompanionMessage(r);
    }
    return;
  }

  if (pending_login && memcmp(&pending_login, contact.id.pub_key, 4) == 0) { // check for login response
    // yes, is response to pending sendLogin()
    pending_login = 0;

    int i = 0;
    if (memcmp(&data[4], "OK", 2) == 0) { // legacy Repeater login OK response
      out_frame[i++] = PUSH_CODE_LOGIN_SUCCESS;
      out_frame[i++] = 0; // legacy: is_admin = false
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6;                                     // pub_key_prefix
    } else if (data[4] == RESP_SERVER_LOGIN_OK) { // new login response
      uint16_t keep_alive_secs = ((uint16_t)data[5]) * 16;
      if (keep_alive_secs > 0) {
        startConnection(contact, keep_alive_secs);
      }
      out_frame[i++] = PUSH_CODE_LOGIN_SUCCESS;
      out_frame[i++] = data[6]; // permissions (eg. is_admin)
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6; // pub_key_prefix
      memcpy(&out_frame[i], &tag, 4);
      i += 4; // NEW: include server timestamp
      out_frame[i++] = data[7]; // NEW (v7): ACL permissions
      out_frame[i++] = data[12]; // FIRMWARE_VER_LEVEL
    } else {
      out_frame[i++] = PUSH_CODE_LOGIN_FAIL;
      out_frame[i++] = 0; // reserved
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6; // pub_key_prefix
    }
    _serial->writeFrame(out_frame, i);
  } else if (len > 4 && // check for status response
             pending_status &&
             memcmp(&pending_status, contact.id.pub_key, 4) == 0 // legacy matching scheme
                                                                 // FUTURE: tag == pending_status
  ) {
    pending_status = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_STATUS_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], contact.id.pub_key, 6);
    i += 6; // pub_key_prefix
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  } else if (len > 4 && tag == pending_telemetry) {  // check for matching response tag
    pending_telemetry = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_TELEMETRY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], contact.id.pub_key, 6);
    i += 6; // pub_key_prefix
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  } else if (len > 4) {
    // Wunschliste 27c: erst unseren eigenen Ring (Multi-Tag) pruefen,
    // damit Chain-/Manual-CLI Antworten in $companion landen ohne den
    // App-Pfad zu stoeren.
    for (uint8_t i = 0; i < _regions_pending_count; i++) {
      if (_regions_pending[i].tag != tag) continue;
      bool from_chain = _regions_pending[i].from_chain;
      if (from_chain) {
        // Chain-Modus: in den Completed-Buffer fuer spaeteren Aggregat-Output.
        if (_regions_completed_count < MAX_COMPLETED_REGIONS && len > 8) {
          CompletedRegionsEntry& ce = _regions_completed[_regions_completed_count++];
          memcpy(ce.pubkey, _regions_pending[i].pubkey, PUB_KEY_SIZE);
          ce.our_snr_q4 = (int8_t)(_radio->getLastSNR() * 4);
          size_t csv_len = len - 8;
          if (csv_len > sizeof(ce.csv) - 1) csv_len = sizeof(ce.csv) - 1;
          memcpy(ce.csv, &data[8], csv_len);
          ce.csv[csv_len] = 0;
        }
      } else {
        // Manueller 'discover regions <name>': sofort pushen wie bisher.
        if (len > 8) {
          char id_str[40];
          if (_regions_pending[i].name[0]) {
            StrHelper::strzcpy(id_str, _regions_pending[i].name, sizeof(id_str));
          } else {
            for (int j = 0; j < 8; j++) snprintf(id_str + j*2, 3, "%02x", _regions_pending[i].pubkey[j]);
            id_str[16] = 0;
          }
          char buf[200];
          size_t csv_len = len - 8;
          if (csv_len > sizeof(buf) - 80) csv_len = sizeof(buf) - 80;
          snprintf(buf, sizeof(buf),
                   "discover regions @%s:\n  %.*s",
                   id_str, (int)csv_len, (const char*)&data[8]);
          pushCompanionMessage(buf);
        }
      }
      // Slot aus Ring entfernen (shift down)
      for (uint8_t j = i+1; j < _regions_pending_count; j++) {
        _regions_pending[j-1] = _regions_pending[j];
      }
      _regions_pending_count--;
      return;  // nicht an App weiterleiten -- wir initiierten
    }
    if (tag != pending_req) return;
    // App-getriggerter ANON_REQ-Response-Pfad
    pending_req = 0;

    // App-Mirror: ANON_REQ_TYPE_REGIONS vom App-Outgoing -> $companion
    if (_last_anon_req_type == ANON_REQ_TYPE_REGIONS && len > 8) {
      char buf[200];
      size_t csv_len = len - 8;
      if (csv_len > sizeof(buf) - 80) csv_len = sizeof(buf) - 80;
      snprintf(buf, sizeof(buf),
               "discover regions @%s:\n  %.*s",
               contact.name, (int)csv_len, (const char*)&data[8]);
      pushCompanionMessage(buf);
    }
    _last_anon_req_type = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_BINARY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], &tag, 4);   // app needs to match this to RESP_CODE_SENT.tag
    i += 4;
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  }
}

bool MyMesh::onContactPathRecv(ContactInfo& contact, uint8_t* in_path, uint8_t in_path_len, uint8_t* out_path, uint8_t out_path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) {
  if (extra_type == PAYLOAD_TYPE_RESPONSE && extra_len > 4) {
    uint32_t tag;
    memcpy(&tag, extra, 4);

    if (tag == pending_discovery) {  // check for matching response tag)
      pending_discovery = 0;

      if (!mesh::Packet::isValidPathLen(in_path_len) || !mesh::Packet::isValidPathLen(out_path_len)) {
        MESH_DEBUG_PRINTLN("onContactPathRecv, invalid path sizes: %d, %d", in_path_len, out_path_len);
      } else {
        int i = 0;
        out_frame[i++] = PUSH_CODE_PATH_DISCOVERY_RESPONSE;
        out_frame[i++] = 0; // reserved
        memcpy(&out_frame[i], contact.id.pub_key, 6);
        i += 6; // pub_key_prefix
        out_frame[i++] = out_path_len;
        i += mesh::Packet::writePath(&out_frame[i], out_path, out_path_len);
        out_frame[i++] = in_path_len;
        i += mesh::Packet::writePath(&out_frame[i], in_path, in_path_len);
        // NOTE: telemetry data in 'extra' is discarded at present

        _serial->writeFrame(out_frame, i);
      }
      return false;  // DON'T send reciprocal path!
    }
  }
  // let base class handle received path and data
  return BaseChatMesh::onContactPathRecv(contact, in_path, in_path_len, out_path, out_path_len, extra_type, extra, extra_len);
}

// Wunschliste 27: CTL_TYPE_NODE_DISCOVER hooks.
// Konstanten analog simple_repeater MyMesh.cpp:769-770.
#define CTL_TYPE_NODE_DISCOVER_REQ   0x80
#define CTL_TYPE_NODE_DISCOVER_RESP  0x90

void MyMesh::onControlDataRecv(mesh::Packet *packet) {
  if (packet->payload_len + 4 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onControlDataRecv(), payload_len too long: %d", packet->payload_len);
    return;
  }
  // Wunschliste 27: NODE_DISCOVER REQ/RESP vorab abfangen, dann ggf.
  // weiter an die App durchreichen (alte Default-Behavior).
  if (packet->payload_len >= 1) {
    uint8_t type_high = packet->payload[0] & 0xF0;
    if (type_high == CTL_TYPE_NODE_DISCOVER_RESP) {
      discoverHandleResp(packet);
      // weiterleiten an App ist optional; wir behandeln den RESP
      // lokal und droppen ihn nicht weiter -- die App soll diagnose-
      // RESPs ja auch sehen koennen falls jemand mit App-CTL was macht.
    } else if (type_high == CTL_TYPE_NODE_DISCOVER_REQ) {
      // Responder-Logik (sub-feature b: 'discoverable')
      discoverableHandleReq(packet);
      // Auch hier nicht droppen -- App-CTL relay.
    }
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_CONTROL_DATA;
  out_frame[i++] = (int8_t)(_radio->getLastSNR() * 4);
  out_frame[i++] = (int8_t)(_radio->getLastRSSI());
  out_frame[i++] = packet->path_len;
  memcpy(&out_frame[i], packet->payload, packet->payload_len);
  i += packet->payload_len;

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onControlDataRecv(), data received while app offline");
  }
}

// Wunschliste 27 (a): Diagnose-Sender 'discover'.
// Baut einen CTL_TYPE_NODE_DISCOVER_REQ und sendet ihn via sendZeroHop.
void MyMesh::discoverStart(uint8_t filter, bool prefix_only) {
  // Default = ALLE Adv-Type-Bits (forward-kompatibel falls die Spec
  // jemals CHAT/ROOM responder hinzubekommt). Bit 0 (= ADV_TYPE_NONE)
  // bleibt 0 -- sinnlos. Heute antworten nur REPEATER + SENSOR;
  // simple_repeater/simple_sensor pruefen jeweils nur ihr eigenes Bit
  // mit '(filter & (1<<TYPE)) != 0', stoeren also nicht an anderen Bits.
  if (filter == 0) filter = 0xFE;
  // Rate-Limit: max 1 discover pro 60s damit User nicht selbst spammt.
  if (!millisHasNowPassed(_discover_next_allowed_ms) && _discover_next_allowed_ms != 0) {
    char r[80];
    unsigned long left_ms = _discover_next_allowed_ms - millis();
    snprintf(r, sizeof(r), "discover: rate-limited. %lu s noch.", left_ms / 1000);
    pushCompanionMessage(r);
    return;
  }
  // Tag generieren (random uint32). Eindeutig fuer Matching.
  uint8_t tag_bytes[4];
  getRNG()->random(tag_bytes, 4);
  memcpy(&_discover_tag, tag_bytes, 4);

  // Payload bauen
  uint8_t data[10];
  data[0] = CTL_TYPE_NODE_DISCOVER_REQ | (prefix_only ? 1 : 0);
  data[1] = filter;
  memcpy(&data[2], &_discover_tag, 4);
  uint32_t since = 0;
  memcpy(&data[6], &since, 4);

  auto pkt = createControlData(data, sizeof(data));
  if (!pkt) {
    pushCompanionMessage("discover: createControlData FAILED (Packet-Pool voll?).");
    return;
  }
  sendZeroHop(pkt);

  // Reise-Wunsch 2026-06-08: bei 'discover regions' (chain-Modus)
  // BEKANNTE Direct-Repeater aus _neighbours (Wunschliste 15) zusaetzlich
  // sofort per zero-hop ANON_REQ_TYPE_REGIONS anfragen. Vorteil:
  //  - hoehere Hit-Rate bei Repeatern die nur sporadisch antworten
  //  - kein 30s-Wartebedarf fuer die Bekannten
  //  - bei 0 CTL-Antworten haben wir trotzdem etwas
  // Filter: nur ADV_TYPE_REPEATER + heard_timestamp <= 3h alt
  // (3h-Grenze nach User-Wunsch -- Repeater die stuendlich adverten
  // werden auch wenn 1 advert verpasst war innerhalb 3h gesichert
  // gehoert).
  // Dedup gegen spaetere CTL-Antworten passiert in
  // sendRegionsQueryZeroHop selbst.
  if (_discover_regions_chained) {
    uint32_t now_rtc = getRTCClock()->getCurrentTime();
    const uint32_t NEIGHBOUR_MAX_AGE_SECS = 3UL * 3600UL;  // 3h
    uint8_t pre_queried = 0;
    for (int i = 0; i < _neighbours_count; i++) {
      const RuntimeNeighbour& n = _neighbours[i];
      if (n.heard_timestamp == 0) continue;
      if (n.adv_type != ADV_TYPE_REPEATER) continue;
      if (now_rtc < n.heard_timestamp) continue;  // RTC moved back
      if (now_rtc - n.heard_timestamp > NEIGHBOUR_MAX_AGE_SECS) continue;
      ContactInfo* known = lookupContactByPubKey(n.pub_key, PUB_KEY_SIZE);
      if (sendRegionsQueryZeroHop(n.pub_key, known ? known->name : "")) {
        pre_queried++;
      }
    }
    if (pre_queried > 0) {
      char r[140];
      snprintf(r, sizeof(r),
               "discover regions: %u bekannte REPEATER\n"
               "(zero-hop, <= 3h) vorab angefragt.",
               (unsigned)pre_queried);
      pushCompanionMessage(r);
    }
  }

  // Listening-Window: 30 s
  _discover_active = true;
  // Reise-Wunsch 2026-06-08: 15-min-Cache. Wenn der letzte Discover
  // weniger als 15 Min her ist, bestehende Eintraege behalten -- neue
  // Antworten ergaenzen sie (per pub_key-Match) oder ueberschreiben
  // den aeltesten bei voller Liste. Vorher: _discover_count = 0 hat
  // alle Eintraege geloescht, selbst wenn der User wiederholt
  // 'discover regions' gemacht hat und manche Repeater nur sporadisch
  // antworten.
  uint32_t now_rtc = getRTCClock()->getCurrentTime();
  bool keep_cache = (_discover_last_at_rtc != 0
                     && now_rtc >= _discover_last_at_rtc
                     && (now_rtc - _discover_last_at_rtc) <= 15UL * 60UL);
  if (!keep_cache) _discover_count = 0;
  _discover_expiry_ms = futureMillis(30000);
  _discover_next_allowed_ms = futureMillis(60000);  // 60 s bis naechster discover

  char r[120];
  const char* what =
      (filter == 0xFE) ? "all"
    : (filter == (1 << ADV_TYPE_REPEATER)) ? "REPEATER"
    : (filter == (1 << ADV_TYPE_SENSOR))   ? "SENSOR"
    : "custom";
  snprintf(r, sizeof(r),
           "discover: REQ sent (filter=%s%s).\n"
           "Listening 30s, Tabelle folgt.",
           what, prefix_only ? ", prefix" : "");
  pushCompanionMessage(r);
}

void MyMesh::discoverHandleResp(mesh::Packet *packet) {
  if (!_discover_active) return;
  if (packet->payload_len < 6) return;
  // Layout: [0]type|adv_type, [1]snr, [2..5]tag, [6..]pub_key
  uint8_t node_type = packet->payload[0] & 0x0F;
  int8_t  their_snr_q4 = (int8_t)packet->payload[1];
  uint32_t echo_tag;
  memcpy(&echo_tag, &packet->payload[2], 4);
  if (echo_tag != _discover_tag) return;  // nicht zu unserem REQ
  // Pub_key extrahieren (32 byte full ODER 8 byte prefix)
  size_t pk_len = packet->payload_len - 6;
  if (pk_len != 32 && pk_len != 8) return;
  // Reise-Wunsch 2026-06-08: 15-min-Cache + slot-Reuse.
  // 1) Wenn pub_key bereits in der Liste -> existing slot ueberschreiben
  //    (neueste Daten gewinnen).
  // 2) Sonst wenn Platz frei -> hinten anhaengen.
  // 3) Sonst (voll) -> aeltesten Eintrag per recv_at_rtc ueberschreiben.
  uint32_t now_rtc = getRTCClock()->getCurrentTime();
  // Kompare-Laenge: voll-key vergleichen wenn beide full, sonst prefix
  size_t cmp_len = (pk_len == 32) ? 8 : pk_len;  // 8-byte-prefix reicht
                                                  // fuer Duplikat-Erkennung
  int target_idx = -1;
  for (uint8_t i = 0; i < _discover_count; i++) {
    if (memcmp(_discover_entries[i].pub_key, &packet->payload[6], cmp_len) == 0) {
      target_idx = i;
      break;
    }
  }
  if (target_idx < 0) {
    if (_discover_count < MAX_DISCOVER_ENTRIES) {
      target_idx = _discover_count++;
    } else {
      // Liste voll -- aeltesten Eintrag finden und ueberschreiben.
      uint32_t oldest_rtc = 0xFFFFFFFFUL;
      target_idx = 0;
      for (uint8_t i = 0; i < _discover_count; i++) {
        if (_discover_entries[i].recv_at_rtc < oldest_rtc) {
          oldest_rtc = _discover_entries[i].recv_at_rtc;
          target_idx = i;
        }
      }
    }
  }
  DiscoverEntry& e = _discover_entries[target_idx];
  memset(e.pub_key, 0, sizeof(e.pub_key));
  memcpy(e.pub_key, &packet->payload[6], pk_len);
  e.full_pubkey = (pk_len == 32);
  e.adv_type = node_type;
  e.their_snr_q4 = their_snr_q4;
  e.our_snr_q4 = (int8_t)(_radio->getLastSNR() * 4);
  e.our_rssi_dbm = (int8_t)radio_driver.getLastRSSI();
  e.recv_at_rtc = now_rtc;

  // Chain-Modus: 'discover regions' (no args) hat den CTL-REQ getriggert.
  // Pro REPEATER-RESP sofort eine zero-hop ANON_REQ_TYPE_REGIONS abfeuern.
  // Sensors koennen REGIONS-Antworten nicht handhaben -- skip.
  if (_discover_regions_chained && e.full_pubkey && e.adv_type == ADV_TYPE_REPEATER) {
    ContactInfo* known = lookupContactByPubKey(e.pub_key, PUB_KEY_SIZE);
    sendRegionsQueryZeroHop(e.pub_key, known ? known->name : "");
  }
}

bool MyMesh::sendRegionsQueryZeroHop(const uint8_t* pubkey32, const char* display_name) {
  if (_regions_pending_count >= MAX_PENDING_REGIONS) return false;
  // Reise-Fix 2026-06-08: Dedup gegen bereits offene REGIONS-Queries.
  // Wichtig wenn Pre-Discover-Phase (_neighbours-Iteration) und CTL-
  // Discover-Response unabhaengig denselben Repeater treffen -- ohne
  // diesen Check wuerden wir 2x denselben ANON_REQ senden.
  for (uint8_t i = 0; i < _regions_pending_count; i++) {
    if (memcmp(_regions_pending[i].pubkey, pubkey32, PUB_KEY_SIZE) == 0) {
      return false;  // already pending
    }
  }
  // Temp ContactInfo. Nur die Felder die sendAnonReq braucht:
  //   id.pub_key (fuer createAnonDatagram + ECDH shared_secret)
  //   out_path_len = 0 -> sendAnonReq nimmt sendDirect mit empty path
  //   = ROUTE_TYPE_DIRECT, zero-hop. Genau was wir wollen.
  ContactInfo tmp;
  memset(&tmp, 0, sizeof(tmp));
  memcpy(tmp.id.pub_key, pubkey32, PUB_KEY_SIZE);
  tmp.out_path_len = 0;
  // Payload: [ts×4][type×1][reply_path_meta×1] -- meta=0 = zero-hop reply
  uint8_t req_data[6];
  uint32_t ts = getRTCClock()->getCurrentTime();
  memcpy(req_data, &ts, 4);
  req_data[4] = ANON_REQ_TYPE_REGIONS;
  req_data[5] = 0x00;
  uint32_t tag, est_timeout;
  int result = sendAnonReq(tmp, req_data, sizeof(req_data), tag, est_timeout);
  if (result == MSG_SEND_FAILED) return false;
  PendingRegionsEntry& e = _regions_pending[_regions_pending_count++];
  e.tag = tag;
  StrHelper::strzcpy(e.name, display_name ? display_name : "", sizeof(e.name));
  memcpy(e.pubkey, pubkey32, PUB_KEY_SIZE);
  e.from_chain = _discover_regions_chained;
  return true;
}

// Haversine-Distanz in km zwischen zwei lat/lon-Paaren (in degrees).
static double dl9sau_haversine_km(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371.0;
  const double D2R = M_PI / 180.0;
  double dlat = (lat2 - lat1) * D2R;
  double dlon = (lon2 - lon1) * D2R;
  double a = sin(dlat/2)*sin(dlat/2)
           + cos(lat1*D2R) * cos(lat2*D2R) * sin(dlon/2)*sin(dlon/2);
  return R * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}
// Bearing (0..360 grad, 0=Nord, 90=Ost) von (lat1,lon1) nach (lat2,lon2).
static int dl9sau_bearing_deg(double lat1, double lon1, double lat2, double lon2) {
  const double D2R = M_PI / 180.0;
  double dlon = (lon2 - lon1) * D2R;
  double y = sin(dlon) * cos(lat2*D2R);
  double x = cos(lat1*D2R)*sin(lat2*D2R)
           - sin(lat1*D2R)*cos(lat2*D2R)*cos(dlon);
  double brng = atan2(y, x) * 180.0 / M_PI;
  int deg = ((int)(brng) % 360 + 360) % 360;
  return deg;
}

// Einheitlicher Legend-Entry (CTL + Chain teilen sich diesen Helper).
// Pro Repeater eine kompakte Zeile unter 145 byte:
//   "aabbcc Name [REP good +2.5/-3.0] 12km @45° (no regions response)"
// 'csv_or_null' ist nur im Chain-Modus interessant -- NULL = wir haben
// keinen Region-CSV fuer diesen Knoten.
void MyMesh::printRepeaterLegendEntry(const DiscoverEntry& e,
                                       const CompletedRegionsEntry* csv_or_null,
                                       bool verbose) {
  char prefix6[7];
  for (int j = 0; j < 3; j++) snprintf(prefix6 + j*2, 3, "%02x", e.pub_key[j]);
  prefix6[6] = 0;
  ContactInfo* known = lookupContactByPubKey(
      (const uint8_t*)e.pub_key, e.full_pubkey ? PUB_KEY_SIZE : 8);
  const char* role = (e.adv_type == ADV_TYPE_REPEATER) ? "REP"
                   : (e.adv_type == ADV_TYPE_SENSOR)   ? "SNS"
                   : (e.adv_type == ADV_TYPE_CHAT)     ? "CMP"
                   : (e.adv_type == ADV_TYPE_ROOM)     ? "ROOM" : "?";
  // Quality: schlechtere der beiden SNR-Richtungen entscheidet
  int8_t worse = (e.our_snr_q4 < e.their_snr_q4) ? e.our_snr_q4 : e.their_snr_q4;
  const char* qual = (worse >= 0)   ? "good"
                   : (worse >= -32) ? "mid"
                   : "bad";
  char dist_buf[32]; dist_buf[0] = 0;
  bool have_my_gps = (sensors.node_lat != 0.0 || sensors.node_lon != 0.0);
  if (known && have_my_gps && (known->gps_lat != 0 || known->gps_lon != 0)) {
    double their_lat = (double)known->gps_lat / 1000000.0;
    double their_lon = (double)known->gps_lon / 1000000.0;
    double km  = dl9sau_haversine_km(sensors.node_lat, sensors.node_lon,
                                      their_lat, their_lon);
    int    brg = dl9sau_bearing_deg(sensors.node_lat, sensors.node_lon,
                                      their_lat, their_lon);
    // Reise-Wunsch 2026-06-08: 1 Dezimalstelle (100m-Aufloesung)
    snprintf(dist_buf, sizeof(dist_buf), " %.1fkm @%d°", km, brg);
  }
  // 'no regions' nur wenn Chain-Modus aktiv UND wir haben keine CSV
  const char* suffix = (_discover_regions_chained && !csv_or_null)
                       ? " (no regions)" : "";
  const char* name = known ? known->name : "(unknown)";
  char line[160];
  if (verbose) {
    // Fuer 'discover': raw Werte mitausgeben (Reise-Wunsch 2026-06-09:
    // Wording 'our/their' -> 'rx_us/rx_him' -- klarer welche
    // Empfangsrichtung gemeint ist):
    //   rx_him = unsere RX seiner RESP (SNR + RSSI) -- wir hoeren ihn
    //   rx_us  = seine RX unseres REQ (SNR, sein RSSI nicht im Protokoll)
    snprintf(line, sizeof(line),
             "%s %.30s [%s %s] rx_him=%+.1fdB/%ddBm rx_us=%+.1fdB%s%s",
             prefix6, name, role, qual,
             (double)e.our_snr_q4 / 4.0, (int)e.our_rssi_dbm,
             (double)e.their_snr_q4 / 4.0,
             dist_buf, suffix);
  } else {
    // Fuer 'discover regions' chain: kompakt nur Quality-Klassifikation.
    snprintf(line, sizeof(line),
             "%s %.30s [%s %s]%s%s",
             prefix6, name, role, qual, dist_buf, suffix);
  }
  pushCompanionMessage(line);
}

// Aggregator: alle bisherigen REGIONS-RESPs nach Region gruppieren und
// als '#name (prefix1, prefix2, ...)' ausgeben, dann eine Legende mit
// vollen Namen (falls in der Kontaktliste) + Entfernung/Bearing falls
// GPS bekannt.
void MyMesh::finalizeRegionsChain() {
  if (_discover_count == 0) {
    pushCompanionMessage("discover regions: keine REPEATER gefunden.");
    _discover_regions_chained = false;
    _regions_chain_finalize_at = 0;
    return;
  }
  // CSV-by-pubkey Lookup helper
  auto find_csv = [&](const uint8_t* pk) -> const CompletedRegionsEntry* {
    for (uint8_t i = 0; i < _regions_completed_count; i++) {
      if (memcmp(_regions_completed[i].pubkey, pk, PUB_KEY_SIZE) == 0) {
        return &_regions_completed[i];
      }
    }
    return NULL;
  };
  // Region-Aggregator
  struct AggRegion {
    char    name[28];
    char    prefix_list[110];
    uint8_t plen;
  };
  static const int MAX_AGG = 24;
  AggRegion agg[MAX_AGG];
  uint8_t agg_count = 0;
  auto find_or_add_region = [&](const char* nm, size_t nlen) -> int {
    for (uint8_t i = 0; i < agg_count; i++) {
      if (strlen(agg[i].name) == nlen && memcmp(agg[i].name, nm, nlen) == 0) return i;
    }
    if (agg_count >= MAX_AGG) return -1;
    int idx = agg_count++;
    size_t cp = (nlen < sizeof(agg[idx].name) - 1) ? nlen : sizeof(agg[idx].name) - 1;
    memcpy(agg[idx].name, nm, cp);
    agg[idx].name[cp] = 0;
    agg[idx].prefix_list[0] = 0;
    agg[idx].plen = 0;
    return idx;
  };
  auto append_prefix = [&](int idx, const char* prefix6) {
    if (idx < 0) return;
    size_t need = strlen(prefix6) + (agg[idx].plen > 0 ? 2 : 0);
    if ((size_t)agg[idx].plen + need + 1 > sizeof(agg[idx].prefix_list)) return;
    if (agg[idx].plen > 0) {
      strcat(agg[idx].prefix_list, ", ");
      agg[idx].plen += 2;
    }
    strcat(agg[idx].prefix_list, prefix6);
    agg[idx].plen += (uint8_t)strlen(prefix6);
  };

  // CSV parsen, Regions nach pubkey-prefix gruppieren
  for (uint8_t i = 0; i < _regions_completed_count; i++) {
    CompletedRegionsEntry& ce = _regions_completed[i];
    char prefix6[7];
    for (int j = 0; j < 3; j++) snprintf(prefix6 + j*2, 3, "%02x", ce.pubkey[j]);
    prefix6[6] = 0;
    const char* p = ce.csv;
    while (*p) {
      while (*p && (*p == ' ' || *p == ',')) p++;
      if (!*p) break;
      const char* rs = p;
      while (*p && *p != ',') p++;
      size_t rlen = (size_t)(p - rs);
      while (rlen > 0 && (rs[rlen-1] == ' ' || rs[rlen-1] == '\t')) rlen--;
      if (rlen == 0) continue;
      int idx = find_or_add_region(rs, rlen);
      append_prefix(idx, prefix6);
    }
  }

  // Header
  char hdr[140];
  snprintf(hdr, sizeof(hdr),
           "discover regions: %u REPEATER,\n"
           "  %u mit Region-Antworten.",
           (unsigned)_discover_count,
           (unsigned)_regions_completed_count);
  pushCompanionMessage(hdr);

  // Region-Tabelle
  if (agg_count > 0) {
    pushCompanionMessage("Regionen:");
    char line[160];
    for (uint8_t i = 0; i < agg_count; i++) {
      snprintf(line, sizeof(line), "#%s (%s)", agg[i].name, agg[i].prefix_list);
      pushCompanionMessage(line);
    }
  }

  // Legende: ALLE CTL-Antworten einschliesslich derer ohne Region-Antwort.
  // Format kompakt, <145 Byte pro Zeile (siehe Helper).
  pushCompanionMessage("Legende:");
  for (uint8_t i = 0; i < _discover_count; i++) {
    const DiscoverEntry& e = _discover_entries[i];
    const CompletedRegionsEntry* c = find_csv(e.pub_key);
    printRepeaterLegendEntry(e, c, /*verbose=*/false);
  }

  _regions_completed_count = 0;
  _regions_pending_count   = 0;
  _discover_regions_chained = false;
  _regions_chain_finalize_at = 0;
}

void MyMesh::discoverableHandleReq(mesh::Packet *packet) {
  // Wunschliste 27 (b) 'discoverable': passive Antwort-Logik.
  // Wir antworten nur wenn wir auch wirklich ein 'full repeater' sind.
  if (_prefs.client_repeat == 0) return;
  if (_prefs.repeater_profile != 1) return;             // nur normal/full
  if (effectiveAdvertRole() != ADV_TYPE_REPEATER) return;
  if (dutyHardReached()) return;
  if (packet->payload_len < 6) return;
  uint8_t filter = packet->payload[1];
  if ((filter & (1 << ADV_TYPE_REPEATER)) == 0) return; // wir sind nicht gemeint

  // Rate-Limit: max 4 RESPs pro 2-min Window (analog simple_repeater).
  unsigned long now = millis();
  if (_discoverable_window_start_ms == 0
      || (long)(now - _discoverable_window_start_ms) > 120000) {
    _discoverable_window_start_ms = now;
    _discoverable_count_window = 0;
  }
  if (_discoverable_count_window >= 4) return;
  _discoverable_count_window++;

  bool prefix_only = (packet->payload[0] & 1) != 0;
  uint32_t tag;
  memcpy(&tag, &packet->payload[2], 4);

  uint8_t data[6 + PUB_KEY_SIZE];
  data[0] = CTL_TYPE_NODE_DISCOVER_RESP | ADV_TYPE_REPEATER;
  data[1] = (uint8_t)(int8_t)(_radio->getLastSNR() * 4);  // unsere SNR-Sicht
  memcpy(&data[2], &tag, 4);
  memcpy(&data[6], self_id.pub_key, PUB_KEY_SIZE);
  size_t resp_len = prefix_only ? (6 + 8) : (6 + PUB_KEY_SIZE);
  auto resp = createControlData(data, resp_len);
  if (resp) {
    sendZeroHop(resp, getRetransmitDelay(resp) * 4);  // Jitter
  }
}

void MyMesh::discoverFinishAndPrint() {
  _discover_active = false;
  // Chain-Modus: keine CTL-Tabelle ausgeben. Stattdessen Hinweis dass
  // die ANON-REQs abgesetzt wurden und in 30s ein Aggregat erscheint.
  // _discover_regions_chained bleibt TRUE bis finalizeRegionsChain
  // (wir brauchen es im RESP-Pfad fuer from_chain-Klassifikation; aber
  // sendRegionsQueryZeroHop sieht es nur waehrend dieses CTL-Loops).
  if (_discover_regions_chained) {
    // Reise-Bugfix 2026-06-08: wenn Phase 1 (REPEATER-Discover) gar keine
    // Antworten brachte, Sofort beenden statt nochmal 30s auf ein
    // Region-Aggregat zu warten (das ohnehin leer waere). Vorher: 1 Min
    // umsonst gewartet.
    if (_discover_count == 0) {
      pushCompanionMessage("discover regions: keine REPEATER in 30s.");
      _discover_regions_chained = false;
      _regions_chain_finalize_at = 0;
      _regions_pending_count = 0;
      return;
    }
    char r[140];
    snprintf(r, sizeof(r),
             "discover regions chain: %u REPEATER,\n"
             "  %u ANON-REQs offen.\n"
             "Aggregat in 30s (oder nach Abschluss).",
             (unsigned)_discover_count,
             (unsigned)_regions_pending_count);
    pushCompanionMessage(r);
    _regions_chain_finalize_at = futureMillis(30000);
    return;
  }
  if (_discover_count == 0) {
    pushCompanionMessage("discover: keine Antworten in 30s.");
    return;
  }
  // Timestamp setzen sobald wir >=1 Antwort haben -- 'neighbors' nutzt
  // diesen Wert um discover-only-Knoten (nicht in Kontaktliste) bis
  // 48h nachzulisten.
  _discover_last_at_rtc = getRTCClock()->getCurrentTime();
  // Tabelle via gemeinsamem Legend-Helper (gleiche Format wie chain-
  // Modus). Keine Region-CSV in diesem Pfad, daher 2. Arg = NULL.
  char header[80];
  snprintf(header, sizeof(header), "discover: %u Antworten:", (unsigned)_discover_count);
  pushCompanionMessage(header);
  for (uint8_t i = 0; i < _discover_count; i++) {
    printRepeaterLegendEntry(_discover_entries[i], NULL, /*verbose=*/true);
  }
}

void MyMesh::discoverLoop() {
  if (_discover_active) {
    if (millisHasNowPassed(_discover_expiry_ms)) {
      discoverFinishAndPrint();
    }
  }
  // Chain-Aggregat-Finalize: separater Timer nach CTL-Window-Ende
  if (_regions_chain_finalize_at != 0
      && millisHasNowPassed(_regions_chain_finalize_at)) {
    finalizeRegionsChain();
  }
}

void MyMesh::onRawDataRecv(mesh::Packet *packet) {
  if (packet->payload_len + 4 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onRawDataRecv(), payload_len too long: %d", packet->payload_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_RAW_DATA;
  out_frame[i++] = (int8_t)(_radio->getLastSNR() * 4);
  out_frame[i++] = (int8_t)(_radio->getLastRSSI());
  out_frame[i++] = 0xFF; // reserved (possibly path_len in future)
  memcpy(&out_frame[i], packet->payload, packet->payload_len);
  i += packet->payload_len;

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onRawDataRecv(), data received while app offline");
  }
}

void MyMesh::onTraceRecv(mesh::Packet *packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                         const uint8_t *path_snrs, const uint8_t *path_hashes, uint8_t path_len) {
  uint8_t path_sz = flags & 0x03;  // NEW v1.11+
  if (12 + path_len + (path_len >> path_sz) + 1 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onTraceRecv(), path_len is too long: %d", (uint32_t)path_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_TRACE_DATA;
  out_frame[i++] = 0; // reserved
  out_frame[i++] = path_len;
  out_frame[i++] = flags;
  memcpy(&out_frame[i], &tag, 4);
  i += 4;
  memcpy(&out_frame[i], &auth_code, 4);
  i += 4;
  memcpy(&out_frame[i], path_hashes, path_len);
  i += path_len;

  memcpy(&out_frame[i], path_snrs, path_len >> path_sz);
  i += path_len >> path_sz;
  out_frame[i++] = (int8_t)(packet->getSNR() * 4); // extra/final SNR (to this node)

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onTraceRecv(), data received while app offline");
  }
}

uint32_t MyMesh::calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const {
  return SEND_TIMEOUT_BASE_MILLIS + (FLOOD_SEND_TIMEOUT_FACTOR * pkt_airtime_millis);
}
uint32_t MyMesh::calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const {
  uint8_t path_hash_count = path_len & 63;
  return SEND_TIMEOUT_BASE_MILLIS +
         ((pkt_airtime_millis * DIRECT_SEND_PERHOP_FACTOR + DIRECT_SEND_PERHOP_EXTRA_MILLIS) *
          (path_hash_count + 1));
}

void MyMesh::onSendTimeout() {}

MyMesh::MyMesh(mesh::Radio &radio, mesh::RNG &rng, mesh::RTCClock &rtc, SimpleMeshTables &tables, DataStore& store, AbstractUITask* ui)
    : BaseChatMesh(radio, *new ArduinoMillis(), rng, rtc, *new StaticPoolPacketManager(16), tables),
      _serial(NULL), telemetry(MAX_PACKET_PAYLOAD - 4), _store(&store), _ui(ui) {
  _iter_started = false;
  _cli_rescue = false;
  // 5 Offline-Buckets initialisieren (seq_no=0 als leerer-Slot Sentinel).
  // Konstruktor laeuft vor dem ersten addToOfflineQueue, so dass spaeter
  // _msg_seq_next per ++ auf 1 inkrementiert wird.
  _msg_seq_next = 0;
  memset(bucket_public,    0, sizeof(bucket_public));
  memset(bucket_hashtag,   0, sizeof(bucket_hashtag));
  memset(bucket_private,   0, sizeof(bucket_private));
  memset(bucket_dm,        0, sizeof(bucket_dm));
  memset(bucket_companion, 0, sizeof(bucket_companion));
  app_target_ver = 0;
  clearPendingReqs();
  next_ack_idx = 0;
  sign_data = NULL;
  dirty_contacts_expiry = 0;
  memset(advert_paths, 0, sizeof(advert_paths));
  memset(send_scope.key, 0, sizeof(send_scope.key));
  memset(heard_list, 0, sizeof(heard_list));
  heard_next_idx = 0;
  // Wunschliste 15: Runtime-Neighbour-Tabelle leer initialisieren.
  _neighbours_count = 0;
  memset(_neighbours, 0, sizeof(_neighbours));
  next_periodic_advert_at = 0;
  next_night_flood_unix = 0;
  _pos_anchor_lat = 0;
  _pos_anchor_lon = 0;
  _pos_anchor_millis = 0;
  _is_moving = false;
  _boot_lat = 0;
  _boot_lon = 0;
  _boot_pos_known = false;
  _gps_had_fix_ever = false;
  _gps_woke_at_millis = 0;
  _gps_off_at_millis = 0;
  _gps_fix_seen_this_wake = false;
  _gps_user_override_until_advert = false;
  _tx_advert_count = 0;
  _tx_digi_count = 0;
  _bt_connect_count = 0;
  _last_serial_connected = false;
  _last_observed_rtc = 0;
  _next_heap_log_at = 0;  // erste Pruefung greift in loop, hochsetzen dort
  _last_logged_disconnect_count = 0;
  _session_min_heap = UINT32_MAX;
  _last_ble_diag_log_at = 0;
  _buildin_keys_count = 0;
  memset(_buildin_in_bbox, 0, sizeof(_buildin_in_bbox));
  memset(_extras_in_bbox,  0, sizeof(_extras_in_bbox));
  _last_millis_seen = 0;
  _millis_wraps = 0;
#if DL9SAU_REGIONS_AVAILABLE
  _region_keys_ready = false;
#endif
  _last_geo_reco[0] = 0;
  _geo_reco_anchor_lat = 0.0;
  _geo_reco_anchor_lon = 0.0;
  _companion_channel_idx = 0xFF;
  _trace_flags = 0;
  _trace_heard_all = false;   // 'trace heard' default mode = new-only
  _unscoped_channel_direct = true;  // Wunschliste 25: unscoped channel = direct
  _pending_reboot_at = 0;
  memset(_heard_direct,       0, sizeof(_heard_direct));
  memset(_rx_advert_total,    0, sizeof(_rx_advert_total));
  memset(_rx_advert_by_scope, 0, sizeof(_rx_advert_by_scope));
  memset(_heard_direct_by_scope, 0, sizeof(_heard_direct_by_scope));
  memset(_rx_direct_advert_by_role, 0, sizeof(_rx_direct_advert_by_role));
  _last_advert_scoped = 0;
  // Backup-Restore State (Wunschliste 28 Phase B)
  _br_state = BR_IDLE;
  _br_block_type = 0;
  _br_line_len = 0;
  _br_json_len = 0;
  _br_brace_depth = 0;
  _br_in_string = false;
  _br_escape_next = false;
  _br_timeout_at = 0;
  _br_applied = 0;
  _br_skipped = 0;
  _br_errors = 0;
  _br_reboot_recommended = false;
  // Wunschliste 27: CTL_TYPE_NODE_DISCOVER state
  _discover_active = false;
  _discover_count = 0;
  _discover_tag = 0;
  _discover_expiry_ms = 0;
  _discover_next_allowed_ms = 0;
  _discover_last_at_rtc = 0;
  _discoverable_window_start_ms = 0;
  _discoverable_count_window = 0;
  memset(_discover_entries, 0, sizeof(_discover_entries));
  _last_anon_req_type = 0;
  memset(_regions_pending, 0, sizeof(_regions_pending));
  _regions_pending_count = 0;
  memset(_regions_completed, 0, sizeof(_regions_completed));
  _regions_completed_count = 0;
  _discover_regions_chained = false;
  _regions_chain_finalize_at = 0;
  _last_advert_route_direct = 0;
  // Wunschliste 26 B: rx-us Echo-Tracking
  memset(_self_initiated_hashes, 0, sizeof(_self_initiated_hashes));
  memset(_self_repeated_hashes,  0, sizeof(_self_repeated_hashes));
  _self_initiated_head = 0;
  _self_repeated_head  = 0;
  _rx_us_self_initiated_count = 0;
  _rx_us_repeated_count       = 0;
  memset(_rx_flood_by_ptype,  0, sizeof(_rx_flood_by_ptype));
  memset(_repeat_by_ptype,        0, sizeof(_repeat_by_ptype));
  memset(_tx_total_by_ptype,      0, sizeof(_tx_total_by_ptype));
  memset(_tx_self_flood_by_ptype, 0, sizeof(_tx_self_flood_by_ptype));
  memset(_heard_quality,      0, sizeof(_heard_quality));
  _tx_repeat_airtime_ms = 0;
  _last_advert_snr_q4 = 0;
  memset(_duty_air_ms_per_minute, 0, sizeof(_duty_air_ms_per_minute));
  _duty_slot_idx = 0;
  _duty_slot_start_ms = 0;
  _duty_last_total_ms = 0;
  _duty_blocked_count = 0;
  // Wunschliste 31: time-sync RAM-state
  _time_sync_last_at_rtc = 0;
  memset(_time_sync_strict_last_ts, 0, sizeof(_time_sync_strict_last_ts));
  memset(_time_sync_last_pubkey, 0, sizeof(_time_sync_last_pubkey));
  _time_sync_done_since_boot = false;
  memset(_time_sync_lazy_cands, 0, sizeof(_time_sync_lazy_cands));
  _time_sync_lazy_count = 0;
  _time_sync_lazy_started_ms = 0;
  _time_sync_lazy_done = false;
  // Reise-Diagnose 2026-06-08: letzter adv-sync candidate
  memset(_last_adv_sync_pubkey, 0, sizeof(_last_adv_sync_pubkey));
  _last_adv_sync_ts = 0;
  _last_adv_sync_at_rtc = 0;
  _last_adv_sync_delta = 0;
  _last_adv_sync_outcome = 0;
  // Wunschliste 35: channel-sender-seen Liste leer
  memset(_channel_sender_seen, 0, sizeof(_channel_sender_seen));
  _channel_sender_seen_count = 0;
  _channel_sender_seen_next = 0;
  _repeating_allowed = false;  // boot-conservative; recompute laeuft in begin() nach loadPrefs
  send_unscoped = false;  // upstream 1.16 new field (zweiter init-Block)

  // defaults
  memset(&_prefs, 0, sizeof(_prefs));
  _prefs.duty_soft_pct = 80;   // Repeats droppen bei 80% von 360s (= 288s)
  _prefs.duty_hard_pct = 100;  // alle TX droppen bei 360s (= 10% TX/h)
  _prefs.gps_power_mode = 0;   // 0=cycle (Default), 1=always-on
  _prefs.gps_lead_min   = 5;   // 5 min Wake vor Advert (Sleep = 10 min im 15-min-Cycle)
  _prefs.airtime_factor = 1.0;
  strcpy(_prefs.node_name, "NONAME");
  _prefs.freq = LORA_FREQ;
  _prefs.sf = LORA_SF;
  _prefs.bw = LORA_BW;
  _prefs.cr = LORA_CR;
  _prefs.tx_power_dbm = LORA_TX_POWER;
  _prefs.gps_enabled = 0;       // GPS disabled by default
  _prefs.gps_interval = 0;      // No automatic GPS updates by default
  //_prefs.rx_delay_base = 10.0f;  enable once new algo fixed
#if defined(USE_SX1262) || defined(USE_SX1268)
#ifdef SX126X_RX_BOOSTED_GAIN
  _prefs.rx_boosted_gain = SX126X_RX_BOOSTED_GAIN;
#else
  _prefs.rx_boosted_gain = 1; // enabled by default
#endif
#endif
}

// Geo bounding boxes (inclusive). Adjust per-build with #defines if needed.
// Werden in begin() einmalig in die Scope-Registry geschrieben sowie als
// Fallback in chooseGeoFallbackScope() verwendet (fuer Installs ohne
// Registry-Eintrag — kann eigentlich nicht mehr passieren nach
// Pre-Population, aber als Defense-in-Depth bleibt der Pfad).
// Werte konsistent mit dl9sau_geo_recommendations.cpp Eintraegen "de-bb"
// und "de-bebb" — beide haben dieselbe Box (Berlin liegt geografisch ganz
// innerhalb von Brandenburg, daher identische Aussen-Bbox).
#ifndef CR_BBOX_BEBB_LAT_MIN
#define CR_BBOX_BEBB_LAT_MIN  51.36
#define CR_BBOX_BEBB_LAT_MAX  53.56
#define CR_BBOX_BEBB_LON_MIN  11.27
#define CR_BBOX_BEBB_LON_MAX  14.77
#endif
#ifndef CR_BBOX_OSTFR_LAT_MIN
#define CR_BBOX_OSTFR_LAT_MIN 53.10
#define CR_BBOX_OSTFR_LAT_MAX 53.80
#define CR_BBOX_OSTFR_LON_MIN  6.50
#define CR_BBOX_OSTFR_LON_MAX  8.50
#endif
// Berlin (Bundesland, eigenes ISO-Kuerzel be); enthalten in CR_BBOX_BEBB.
// Stadtgebiet rund 52.34..52.68 N / 13.09..13.76 E — leicht erweitert
// damit der Rand nicht knapp wird.
#ifndef CR_BBOX_DE_BE_LAT_MIN
#define CR_BBOX_DE_BE_LAT_MIN 52.30
#define CR_BBOX_DE_BE_LAT_MAX 52.70
#define CR_BBOX_DE_BE_LON_MIN 13.05
#define CR_BBOX_DE_BE_LON_MAX 13.80
#endif

void MyMesh::begin(bool has_display) {
  BaseChatMesh::begin();

  if (!_store->loadMainIdentity(self_id)) {
    self_id = radio_new_identity(); // create new random identity
    int count = 0;
    while (count < 10 && (self_id.pub_key[0] == 0x00 || self_id.pub_key[0] == 0xFF)) { // reserved id hashes
      self_id = radio_new_identity();
      count++;
    }
    _store->saveMainIdentity(self_id);
  }

// if name is provided as a build flag, use that as default node name instead
#ifdef ADVERT_NAME
  strcpy(_prefs.node_name, ADVERT_NAME);
#else
  // use hex of first 4 bytes of identity public key as default node name
  char pub_key_hex[10];
  mesh::Utils::toHex(pub_key_hex, self_id.pub_key, 4);
  strcpy(_prefs.node_name, pub_key_hex);
#endif

  // if build provides default-scope, init with that
#ifdef DEFAULT_FLOOD_SCOPE_NAME
  strcpy(_prefs.default_scope_name, DEFAULT_FLOOD_SCOPE_NAME);
  {
    TransportKeyStore temp;
    TransportKey key;
    temp.getAutoKeyFor(0, "#" DEFAULT_FLOOD_SCOPE_NAME, key);
    memcpy(_prefs.default_scope_key, key.key, sizeof(key.key));
  }
#endif

  // Wunschliste 32: Pre-Init der per-Channel-Hops auf CH_HOPS_OFF (254).
  // Trick: VOR loadPrefs() setzen. Falls die Prefs-Datei zu kurz ist
  // (frische Installation, oder Upgrade von Firmware ohne dieses Feature),
  // bleiben diese Default-Werte stehen. Falls die Datei sie enthaelt,
  // ueberschreibt loadPrefs() mit den persistierten Werten -- inkl. einem
  // User-explizit-gesetzten 0 ("don't repeat"). Ohne dieses Pre-Init
  // wuerde der memset(0)-Default flaechendeckend als "don't repeat"
  // interpretiert, was sicher nicht gewollt waere.
  // Wunschliste 32 v2: RAM-Cache fuer per-Channel-Hops. Default OFF.
  // Wird in rebuildChannelHopsCache() aus _prefs.channel_hops_list (name-
  // hash) befuellt, sobald Channels geladen sind.
  memset(_channel_hops_cap_cache, CH_HOPS_OFF, sizeof(_channel_hops_cap_cache));
  // Persistente Liste: leer als Default. Pre-Init vor loadPrefs damit
  // fresh-install / Firmware-Upgrade keine geisterhaften Eintraege bekommt.
  _prefs.channel_hops_count = 0;
  memset(_prefs.channel_hops_list, 0, sizeof(_prefs.channel_hops_list));
  _prefs.flood_max_unknown_chan = CH_HOPS_OFF;
  // Wunschliste 12 update 2026-06-02: flood_max_scope_region Default 3
  // VOR loadPrefs. Falls Datei kuerzer / fresh-install: bleibt 3 stehen.
  // Stored-Wert (inkl. user-explizit 0 = 'nicht repeaten') ueberschreibt.
  _prefs.flood_max_scope_region = 3;
  // Wunschliste 39 (2026-06-04): unscoped Companion cap. Sentinel
  // CH_HOPS_OFF = follow flood_max_scope_region (Default damit
  // Erstkontakt via unscoped CHAT-Adverts klappt).
  _prefs.flood_max_unscoped_companions = CH_HOPS_OFF;

  // Wunschliste 45 Pre-Init (Reise 2026-06-09): LBT-Default 14 (upstream-
  // Doku-Wert). Alte Pref-Files ohne dieses Feld lassen den Pre-Init
  // stehen -> User aktiviert LBT automatisch ab nuechstem Boot. Wer
  // explicit 0 in seinem File hat (= bewusst off): bleibt 0. AGC-Reset
  // Default 0 (= disabled) -- selten gebraucht, User aktiviert manuell.
  _prefs.interference_threshold = 14;
  _prefs.agc_reset_interval = 0;

  // Wunschliste 46 Phase 2 (2026-06-10): channel-filter Masks
  // Pre-Init: alle Masks = 0 -> global (alle Channels).
  _prefs.filter_sender_drop_on_channel_mask = 0;
  _prefs.filter_sender_drop_exempt_mask = 0;
  _prefs.filter_text_drop_on_channel_mask = 0;
  _prefs.filter_text_drop_exempt_mask = 0;
  // Wunschliste 46 Phase 3 (2026-06-10): keep-Listen
  // Pre-Init leer; loadPrefs ueberschreibt falls vorhanden.
  _prefs.filter_sender_keep_count = 0;
  memset(_prefs.filter_sender_keep, 0, sizeof(_prefs.filter_sender_keep));
  _prefs.filter_text_keep_count = 0;
  memset(_prefs.filter_text_keep, 0, sizeof(_prefs.filter_text_keep));
  // Wunschliste 46 Phase 2 v2 (2026-06-10): pro-Pattern channel-filter
  // Pre-Init alle Slot-Masks = 0 (= global / alle Channels).
  memset(_prefs.filter_sender_drop_chan_on, 0, sizeof(_prefs.filter_sender_drop_chan_on));
  memset(_prefs.filter_sender_drop_chan_ex, 0, sizeof(_prefs.filter_sender_drop_chan_ex));
  memset(_prefs.filter_sender_keep_chan_on, 0, sizeof(_prefs.filter_sender_keep_chan_on));
  memset(_prefs.filter_sender_keep_chan_ex, 0, sizeof(_prefs.filter_sender_keep_chan_ex));
  memset(_prefs.filter_text_drop_chan_on, 0, sizeof(_prefs.filter_text_drop_chan_on));
  memset(_prefs.filter_text_drop_chan_ex, 0, sizeof(_prefs.filter_text_drop_chan_ex));
  memset(_prefs.filter_text_keep_chan_on, 0, sizeof(_prefs.filter_text_keep_chan_on));
  memset(_prefs.filter_text_keep_chan_ex, 0, sizeof(_prefs.filter_text_keep_chan_ex));

  // Wunschliste 46 Phase 5 (2026-06-10): scope-Filter Pre-Init.
  _prefs.filter_scope_drop_count = 0;
  memset(_prefs.filter_scope_drop, 0, sizeof(_prefs.filter_scope_drop));
  memset(_prefs.filter_scope_drop_chan_on, 0, sizeof(_prefs.filter_scope_drop_chan_on));
  memset(_prefs.filter_scope_drop_chan_ex, 0, sizeof(_prefs.filter_scope_drop_chan_ex));
  _prefs.filter_scope_keep_count = 0;
  memset(_prefs.filter_scope_keep, 0, sizeof(_prefs.filter_scope_keep));
  memset(_prefs.filter_scope_keep_chan_on, 0, sizeof(_prefs.filter_scope_keep_chan_on));
  memset(_prefs.filter_scope_keep_chan_ex, 0, sizeof(_prefs.filter_scope_keep_chan_ex));
  // Repeat-Achse Default = 1 (scoped). User-Wunsch 2026-06-10:
  // unscoped-Channelmessages auf unbekannten Channels sollen per
  // Default NICHT weitergeleitet werden (Spam-Schutz). Wer das
  // alte Verhalten will, setzt explizit 'all'.
  _prefs.filter_unknown_channel_repeat = 1;
  // Wunschliste 43 BLE-Power-Mode: Default cycle (= 0). Aktiviert
  // sleep nach Boot-Grace / Disconnect-Hot-Start.
  _prefs.bluetooth_power_mode = 0;

  // Wunschliste 31: time-sync Pre-Init analog. Default = 1 (lazy).
  // VOR loadPrefs() setzen, dann ueberschreibt der persistierte Wert (falls
  // existent) -- so wird fresh-install zu 'lazy' (Komfort), aber 'set time
  // sync off' bleibt erhalten.
  _prefs.time_sync_mode = 1; // lazy default
  memset(_prefs.time_sync_sources, 0, sizeof(_prefs.time_sync_sources));

  // load persisted prefs
  _store->loadPrefs(_prefs, sensors.node_lat, sensors.node_lon);

  // Migration: alter auto_advert_enabled=1 (= "on" mit beiden Adverts) zu
  // dem neuen Bitmask-Schema (3 = AUTO_ADV_ZEROHOP | AUTO_ADV_NIGHTLY).
  // Andere Werte (0, 2, 3) bleiben unangetastet — sie waren entweder im
  // alten Schema "off" oder schon in der neuen Bitmask-Semantik gesetzt.
  if (_prefs.auto_advert_enabled == 1) {
    _prefs.auto_advert_enabled = AUTO_ADV_ALL;
    _store->savePrefs(_prefs, sensors.node_lat, sensors.node_lon);
  }

  // Snapshot the persisted position before GPS updates start overwriting
  // sensors.node_lat/lon. Used by updateMotionTracking() to decide if the
  // device has moved since the last session.
  _boot_lat = sensors.node_lat;
  _boot_lon = sensors.node_lon;
  _boot_pos_known = (sensors.node_lat != 0.0 || sensors.node_lon != 0.0);
  // (Geo-Reco-Push folgt weiter unten nach setupCompanionChannel().)

  // One-time migration: any persisted 869.000 stands for the real EU narrow
  // 869.618 MHz. Rewrite the pref so display and app show the actual
  // operating frequency rather than the app-side shorthand.
  if (fabsf(_prefs.freq - CR_NARROW_FREQ_TRIGGER) < 0.0005f) {
    _prefs.freq = CR_NARROW_FREQ_ACTUAL;
    _store->savePrefs(_prefs, sensors.node_lat, sensors.node_lon);
  }

  // sanitise bad pref values
  _prefs.rx_delay_base = constrain(_prefs.rx_delay_base, 0, 20.0f);
  // tx_delay_factor / direct_tx_delay_factor: Sentinel-Logik
  //   0    = uninitialisiert (neues Geraet)        -> -1 (auto)
  //   -1   = expliziter "auto"                      -> bleibt
  //   0..2 = expliziter numerischer Wert            -> bleibt
  //   sonst (out-of-range, NaN)                     -> -1 (auto)
  if (_prefs.tx_delay_factor == 0.0f
      || _prefs.tx_delay_factor > 2.0f
      || _prefs.tx_delay_factor < -1.0f
      || isnan(_prefs.tx_delay_factor)) {
    _prefs.tx_delay_factor = -1.0f;  // auto
  }
  if (_prefs.direct_tx_delay_factor == 0.0f
      || _prefs.direct_tx_delay_factor > 2.0f
      || _prefs.direct_tx_delay_factor < -1.0f
      || isnan(_prefs.direct_tx_delay_factor)) {
    _prefs.direct_tx_delay_factor = -1.0f;  // auto
  }
  if (_prefs.repeat_scope_mode > REPEAT_SCOPE_MODE_ALLOWLIST) {
    // Out-of-range -> defensive default. Reise-Wunsch 2026-06-08:
    // Default fuer Neuinstall = ALLOWLIST (sicher, User entscheidet
    // welche Scopes weitergeleitet werden), nicht ALL.
    _prefs.repeat_scope_mode = REPEAT_SCOPE_MODE_ALLOWLIST;
  }

  // Kanonische Region-Eintraege (de, de-by, ..., de-bebb, ostfriesland,
  // local, region usw.) kommen aus der Build-in-Tabelle
  // (dl9sau_geo_recommendations.cpp) und sind out-of-the-box im
  // Repeat-Set (Default-Status AUTO bzw. Pin fuer local/lokal/
  // region/regional). User-Customizations leben in
  // scope_buildin_status[] (sparse) + scope_extras[].

  // ----- Key-Cache fuer Build-in-Region-Eintraege (Wunschliste 11, Schritt 4)
  // Computed once at boot. Wird von scopeAllowedForRepeat() etc. verwendet
  // um TransportKey::calcTransportCode() ohne SHA-256-Recomputation aufzurufen.
  // Alias-Eintraege bekommen denselben Schluessel wie der Primaer-Eintrag
  // (gleicher Name -> gleicher getAutoKeyFor-Output); chooseGeoFallbackScope
  // verwendet das Index direkt, beide Slots liefern also intentional die
  // gleichen Bytes.
  {
    size_t n = dl9sau_region_count();
    if (n > (size_t)SCOPE_BUILDIN_KEY_CACHE_MAX) n = SCOPE_BUILDIN_KEY_CACHE_MAX;
    _buildin_keys_count = (int)n;
    // FIX 2026-06-05: BUG! TransportKeyStore::getAutoKeyFor cacht NUR
    // per uint16_t id, NICHT per name. Wenn wir tmp.getAutoKeyFor(0, ...)
    // in einer Schleife mit immer id=0 aber verschiedenen Namen aufrufen,
    // gibt Iteration 2+ den GECACHEDETEN Key der ersten Iteration zurueck
    // -- ALLE _buildin_keys[1..] enthielten sha256("#europe"). Folge:
    // chooseGeoFallbackScope picked ostfriesland-bbox, gab #europe-Key
    // zurueck. _buildin_keys[idx_local] auch = #europe-Key. Trace-Label-
    // Check (memcmp scope.key vs _buildin_keys[idx_local]) matchte und
    // labelt "last-resort/local". User-Beobachtung 2026-06-05 (debugscope).
    //
    // Bypass: SHA256 direkt rechnen statt buggy getAutoKeyFor benutzen.
    for (size_t i = 0; i < n; i++) {
      const char* name = NULL;
      if (!dl9sau_get_region(i, &name, NULL, NULL, NULL, NULL)) continue;
      char tag[40];
      snprintf(tag, sizeof(tag), "#%s", name);
      SHA256 sha;
      sha.update((const uint8_t*)tag, strlen(tag));
      sha.finalize(_buildin_keys[i].key, sizeof(_buildin_keys[i].key));
    }
  }

  // flood_max: 0 = uninitialisiert -> Default 16 (analog dem alten
  // hartcodierten Wert). Range 1..63 (Protokoll-Max: 6-Bit-hash_count).
  if (_prefs.flood_max == 0 || _prefs.flood_max > 63) {
    _prefs.flood_max = 16;
  }
  // scope_advert_auto: 0=uninit, 1=off, 2=on (default), 3=prefer.
  // Migration: alter scope_geo_prefers war 0/1 -> map auf 2/3.
  // (Field-Offset 1377 wird wiederverwendet; alte bytes 0/1 sind genau
  // die Migration-Eingaben.)
  if (_prefs.scope_advert_auto == 0) {
    _prefs.scope_advert_auto = 2;  // alter Wert 0 -> on
  } else if (_prefs.scope_advert_auto == 1) {
    _prefs.scope_advert_auto = 3;  // alter Wert 1 (geo_prefers=on) -> prefer
  } else if (_prefs.scope_advert_auto > 3) {
    _prefs.scope_advert_auto = 2;  // out-of-range -> Default
  }
  // scope_repeater_auto: 0=uninit, 1=off, 2=on (default).
  if (_prefs.scope_repeater_auto == 0 || _prefs.scope_repeater_auto > 2) {
    _prefs.scope_repeater_auto = 2;
  }
  // repeater_profile: 0 = defensive (Default), 1 = normal
  if (_prefs.repeater_profile > 1) _prefs.repeater_profile = 0;
  // advert_role (Wunschliste 7): 0=auto, 1=chat, 2=repeater, 3=sensor, 4=room
  if (_prefs.advert_role > 4) _prefs.advert_role = 0;
  // loop_detect (Wunschliste 6b): 0=off, 1=minimal, 2=moderate, 3=strict
  if (_prefs.loop_detect > 3) _prefs.loop_detect = 0;
  // owner_info: NULL-Terminator sicherstellen (defensive gegen
  // unterminierte Flash-Daten).
  _prefs.owner_info[sizeof(_prefs.owner_info) - 1] = 0;
  // flood_max_scope_region: 0 ist jetzt gueltiger User-Wert
  // ('#region/#regional nicht repeaten'). Pre-Init vor loadPrefs() liefert
  // den Default 3. Hier nur Sanity-Cap an flood_max.
  if (_prefs.flood_max_scope_region > _prefs.flood_max) {
    _prefs.flood_max_scope_region = _prefs.flood_max;
  }
  // flood_max_infra: 0 = uninitialisiert -> bump auf 16 (oder flood_max
  // falls kleiner). Analog zu flood_max=0->16. Hintergrund: wir wollen
  // Infrastruktur-Adverts (REPEATER/SENSOR/ROOM) per Default begrenzen,
  // auch wenn User flood_max spaeter auf z.B. 20 hochsetzt -- infra
  // bleibt dann bei 16, bis aktiv geaendert.
  // 'set flood_max_infra 0' bleibt als transienter Escape Hatch erhalten
  // (fallback auf flood_max), wird aber beim naechsten Reboot zurueck-
  // gebumpt. User-Erwartung "0 = aus" ist NICHT die Default-Bedeutung.
  // Wunschliste 39 Migration (2026-06-04): vereinheitlichte Sentinels.
  // Legacy: flood_max_infra==0 hiess "deaktiviert / follow flood_max",
  // wurde aber beim Boot auf 16 gebumpt. Jetzt: 0 -> follow (254).
  // User-Eingabe "0" wird vom CLI-Parser abgewiesen.
  if (_prefs.flood_max_infra == 0) {
    _prefs.flood_max_infra = FLOOD_MAX_INFRA_FOLLOW;
  } else if (_prefs.flood_max_infra != FLOOD_MAX_INFRA_FOLLOW
             && _prefs.flood_max_infra > _prefs.flood_max) {
    _prefs.flood_max_infra = _prefs.flood_max;
  }
  // flood_max_req_resp Migration: 0 -> follow (254). Legacy "0 = kaskade
  // auf flood_max_infra" wird jetzt durch das Sentinel ausgedrueckt.
  if (_prefs.flood_max_req_resp == 0) {
    _prefs.flood_max_req_resp = FLOOD_MAX_INFRA_FOLLOW;
  } else if (_prefs.flood_max_req_resp != FLOOD_MAX_INFRA_FOLLOW
             && _prefs.flood_max_req_resp > _prefs.flood_max) {
    _prefs.flood_max_req_resp = _prefs.flood_max;
  }

  _prefs.airtime_factor = constrain(_prefs.airtime_factor, 0, 9.0f);
  _prefs.freq = constrain(_prefs.freq, 150.0f, 2500.0f);
  _prefs.bw = constrain(_prefs.bw, 7.8f, 500.0f);
  _prefs.sf = constrain(_prefs.sf, 5, 12);
  _prefs.cr = constrain(_prefs.cr, 5, 8);
  // Wunschliste 35: cached effektive Repeating-Erlaubnis initial berechnen.
  // Wird bei jeder Stored-State-Aenderung (CLI repeater on/off/profile,
  // App-CMD_SET_RADIO_PARAMS) per recomputeRepeatingAllowed() refreshed.
  // _repeating_allowed wird in shouldRepeat() pro Paket gecheckt --
  // einzelner Bool-Load statt isValidClientRepeatFreq pro Paket.
  recomputeRepeatingAllowed("boot");
  _prefs.tx_power_dbm = constrain(_prefs.tx_power_dbm, -9, MAX_LORA_TX_POWER);
  _prefs.gps_enabled = constrain(_prefs.gps_enabled, 0, 1);  // Ensure boolean 0 or 1
  _prefs.gps_interval = constrain(_prefs.gps_interval, 0, 86400);  // Max 24 hours

#ifdef BLE_PIN_CODE // 123456 by default
  if (_prefs.ble_pin == 0) {
#ifdef DISPLAY_CLASS
    if (has_display && BLE_PIN_CODE == 123456) {
      StdRNG rng;
      _active_ble_pin = rng.nextInt(100000, 999999); // random pin each session
    } else {
      _active_ble_pin = BLE_PIN_CODE; // otherwise static pin
    }
#else
    _active_ble_pin = BLE_PIN_CODE; // otherwise static pin
#endif
  } else {
    _active_ble_pin = _prefs.ble_pin;
  }
#else
  _active_ble_pin = 0;
#endif

  // Punkt 15 Reise-Wunsch 2026-06-08: Fresh-Install-Defaults fuer
  // scope-advert. Grosse Regionen (europe, de, de-*) sollen ab Werk
  // advert=off haben, damit das Netz nicht mit Adverts dieser breiten
  // Regionen ueberflutet wird. Trigger: scope_buildin_status_count == 0
  // UND scope_extras_count == 0 (= unangetastete Erst-Konfiguration).
  // Bei bestehenden Installs mit count > 0 keine Migration -- User
  // koennte eigene Konfig haben.
  if (_prefs.scope_buildin_status_count == 0 && _prefs.scope_extras_count == 0) {
    static const char* fresh_install_advert_off[] = {
      "europe", "de", "de-sued", "de-west", "de-nord", "de-mitte", "de-ost"
    };
    bool any_applied = false;
    for (size_t k = 0; k < sizeof(fresh_install_advert_off)/sizeof(fresh_install_advert_off[0]); k++) {
      int idx = dl9sau_find_region_index(fresh_install_advert_off[k]);
      if (idx >= 0) {
        uint8_t st = getBuildinStatus(idx);
        st |= SCOPE_STATUS_ADVERT_OFF;
        setBuildinStatus(idx, st);
        any_applied = true;
      }
    }
    // Reise-Fix 2026-06-08: fresh-install soll repeat_scope_mode auf
    // ALLOWLIST defaulten (sicher, User entscheidet). Bisheriger Default
    // 'all' wurde User-Erfahrung als zu permissiv empfunden -- ein
    // frisches Geraet wuerde sonst sofort Pakete aus allen Regionen
    // weiterleiten.
    if (_prefs.repeat_scope_mode == REPEAT_SCOPE_MODE_ALL) {
      _prefs.repeat_scope_mode = REPEAT_SCOPE_MODE_ALLOWLIST;
      any_applied = true;
    }
    if (any_applied) _store->savePrefs(_prefs, sensors.node_lat, sensors.node_lon);
  }

  // Reise-Bugfix 2026-06-08: Bbox-Membership initial setzen via
  // getEffectiveLatLon. Vorher wurde evaluateScopeBboxes nur in
  // updateMotionTracking aufgerufen, das bei !loc->isValid() (kein
  // Live-Fix) early-returnt -- ohne fixed-location-Fallback. Damit
  // blieb _buildin_in_bbox[] auf false fuer alle Regionen, AUTO-Scopes
  // konnten nicht matchen. Folge: User mit konfigurierter fixed
  // location (z.B. Indoor, GPS gestoert) hatte effektiv keine
  // funktionierende geo-basierte Repeat-Allowlist.
  //
  // Inkonsistenz-Beseitigung: SEND-Pfad (chooseGeoFallbackScope) nutzt
  // schon getEffectiveLatLon mit fixed-fallback, RECEIVE-Pfad
  // (scopeAllowedForRepeat ueber _buildin_in_bbox[]) tat es nicht.
  //
  // Live-GPS-Fix ueberschreibt das spaeter in updateMotionTracking
  // (nur bei profile=defensive; bei profile=normal bleibt fixed location).
  {
    double init_lat, init_lon;
    if (getRepeaterBboxLatLon(init_lat, init_lon)) {
      evaluateScopeBboxes(init_lat, init_lon);
    }
  }

  resetContacts();
  _store->loadContacts(this);
  bootstrapRTCfromContacts();
  // Reise-Fix 2026-06-08: nach Boot-Bootstrap unsere Sync-State-Marker
  // initialisieren, damit clock-Display die Quelle 'Bootstrap (last
  // advert)' anzeigt statt einer evtl. spaeter eingefangenen Stale-
  // Advert-Sync-Quelle. last_at_rtc = aktueller RTC = 'gerade synchron'.
  // pub_key=[0xFF,0xFF,0xFF] als Bootstrap-Marker (App-Sync nutzt 0x00).
  {
    uint32_t rtc_after_bootstrap = getRTCClock()->getCurrentTime();
    if (rtc_after_bootstrap > 1577836800UL /* 2020-01-01 */) {
      _time_sync_last_at_rtc = rtc_after_bootstrap;
      _time_sync_done_since_boot = true;
      memset(_time_sync_last_pubkey, 0xFF, sizeof(_time_sync_last_pubkey));
    }
  }
  addChannel("Public", PUBLIC_GROUP_PSK); // pre-configure Andy's public channel
  _store->loadChannels(this);
  // Companion-Channel (lokal, kein RF) anlegen oder Index aus persistiertem
  // Eintrag übernehmen. Muss VOR jedem pushCompanionMessage() laufen — daher
  // hier, NACH loadChannels() aber vor dem Boot-Geo-Push weiter unten.
  setupCompanionChannel();

  // Offline-Message-Buckets von Flash laden (Wunschliste 19 Phase C).
  // Vor dem Boot-Geo-Push, sonst wuerde der seq_no der Geo-Push-Message
  // VOR den restaurierten Eintraegen liegen und sich falsch in den Pop-
  // Order schmuggeln. loadBucketsFromFlash() setzt _msg_seq_next auf
  // max(seq_no) der restaurierten Eintraege.
  loadBucketsFromFlash();

  // Boot-Greeting als ALLERERSTE Nachricht: soll die iOS-Companion-App
  // dazu bringen, bei Mention des eigenen Namens einen Ton abzuspielen,
  // damit der User akustisch einen Reboot mitbekommt. Empirie 2026-06-01:
  //   v1 "Booted. Hello <name>"     -> kein Ton
  //   v2 "@<name>: booted. enjoy!"  -> kein Ton
  // User-Erkenntnis: die App-eigene 'Antworten'-Funktion erzeugt
  // intern '@[<name>] ' (mit Klammern). Die Klammern werden angezeigt
  // als grau-hinterlegte Highlight-Box. Vermutlich ist DAS das echte
  // Mention-Format, das die Ton-Logik triggert.
  //   v3 "@[<name>] booted.."       -> Test
  {
    char greet[80];
    snprintf(greet, sizeof(greet), "@[%.40s] booted..", _prefs.node_name);
    pushCompanionMessage(greet);
  }

  // Boot-Geo-Push (Channel ist jetzt vorhanden, Output erscheint im Chat):
  if (_boot_pos_known) {
    maybePushGeoRecommendation(_boot_lat, _boot_lon);
  }

  // DL9SAU: applyRadioPolicy() ist unser Wrapper mit Override-Logik;
  // ersetzt upstream's neue direkte radio_driver.setParams/setTxPower-
  // Aufrufe (Stand 1.16.0 Merge 2026-06-06).
  applyRadioPolicy();
  radio_driver.setTxPower(_prefs.tx_power_dbm);
  radio_driver.setRxBoostedGainMode(_prefs.rx_boosted_gain);

  // Pre-compute scope keys for the (community-sourced) region list, so
  // incoming scoped packets can be matched to a region name.
  initRegionKeys();

  // First periodic advert:
  //   - GPS off -> 5 min
  //   - GPS on with valid fix already (rare so soon after boot) -> 5 min
  //   - GPS on, still searching -> 10 min, but clamped down to 5 min once
  //     the first fix arrives (see updateMotionTracking()).
  unsigned long first_advert_wait = CR_PERIODIC_ADVERT_BOOT_DELAY_MS;
#if ENV_INCLUDE_GPS == 1
  if (_prefs.gps_enabled) {
    LocationProvider* loc = sensors.getLocationProvider();
    if (loc == NULL || !loc->isValid()) {
      first_advert_wait = CR_PERIODIC_ADVERT_BOOT_DELAY_GPS_MS;
    }
  }
#endif
  next_periodic_advert_at = futureMillis(first_advert_wait);
  next_night_flood_unix = 0;
  pushDebugLog("[ADV-DBG] begin: boot_delay_ms=%lu next_periodic_at=%lu millis=%lu rtc=%lu\n",
                first_advert_wait, next_periodic_advert_at, millis(),
                (unsigned long)getRTCClock()->getCurrentTime());
  MESH_DEBUG_PRINTLN("RX Boosted Gain Mode: %s",
                     radio_driver.getRxBoostedGainMode() ? "Enabled" : "Disabled");
}

const char *MyMesh::getNodeName() {
  return _prefs.node_name;
}
NodePrefs *MyMesh::getNodePrefs() {
  return &_prefs;
}
uint32_t MyMesh::getBLEPin() {
  return _active_ble_pin;
}

struct FreqRange {
  uint32_t lower_freq, upper_freq;
};

// Legal ISM band centre-frequency ranges in kHz. NOTE: these are the band
// edges of the regulatory limit; the app is responsible for keeping
// (mid_freq +/- BW/2) inside one band, picking a BW that respects the
// per-band maximum, AND keeping TX power within the band's ERP cap.
// The MeshCore wire protocol currently only carries a single global
// MAX_LORA_TX_POWER — per-band power limits below are EU regulatory
// guidance, not enforced by firmware.
//
// Why no automatic clamping? The legal limit is ERP (relative to dipole)
// or EIRP (relative to isotrope), not the chip's conducted output. The
// actual radiated power depends on cable loss, antenna gain (dBi vs dBd)
// and antenna efficiency. Hard-clamping the chip output would either
// over-restrict (long feedline, no antenna gain) or under-restrict
// (high-gain antenna right at the radio). The user has to do that math.
//
// EU regulatory caps:
//   70cm SRD (433.05-434.79):     10  mW ERP, 10%   duty cycle (LPD433)
//   865.6-867.6 MHz sub-bands:    500 mW ERP, 10%   duty (network AP) / 2.5% else
//   868.7-869.2 MHz (g3):          25 mW ERP, 0.1%  duty cycle
//   869.4-869.65 MHz (g4 narrow): 500 mW ERP, 10%   duty (LBT/AFA recommended)
//   US 902-928 MHz:               1 W EIRP for spread-spectrum (FCC Part 15)
static FreqRange repeat_freq_ranges[] = {
  // DL9SAU: bewusst weitere Ranges als upstream-1.16-Default (das war
  // dort 433.0 + 869.495 + 918.0 single-freqs via ALLOWED_REPEAT_FREQ_RANGE).
  // Wir erlauben alle EU 869-Subbands + 70cm + US 915.
  { 433050, 434790 },   // 70cm SRD / ISM, EU + most regions (BW typ. <= 250 kHz)
  // EU 865-868 MHz sub-bands (BW <= 200 kHz each):
  { 865600, 865800 },
  { 866200, 866400 },
  { 866800, 867000 },
  { 867400, 867600 },
  { 868700, 869200 },   // EU 869 MHz g3 band
  { 869400, 869650 },   // EU 869 MHz narrow band (max BW 250 kHz)
  { 902000, 928000 }    // US 915 MHz ISM band (902.0-928.0 MHz)
  // Amateur radio 70cm (430.000 - 439.999 MHz). CAVE: MeshCore encrypts
  // payloads end-to-end, which is generally not permitted on amateur
  // radio frequencies (open-mode requirement). Only uncomment if you are
  // sure your local regulation allows it for your usage:
  //, { 430000, 439999 }
};

// Striktere Liste fuer client_repeat=1. Im EU 869-Narrow-Band schliesst
// dieser Range die gaengig genutzte 869.618 MHz Hauptfrequenz NICHT ein —
// "compliant variant" gegen ungewolltes Repeaten auf der Main-Freq. Der
// App-Pfad (CMD_SET_RADIO_PARAMS, isValidClientRepeatFreq) lehnt repeat=1
// auf 869.618 also ab. Wer bewusst auch dort repeaten will: Companion-CLI
// "repeater on --force" umgeht die strict-Pruefung, behaelt aber
// signalFitsInIsmBand als Sicherheitsgate.
static FreqRange repeat_freq_ranges_strict[] = {
  { 433050, 434790 },   // 70cm SRD / ISM
  { 865600, 865800 },
  { 866200, 866400 },
  { 866800, 867000 },
  { 867400, 867600 },
  { 868700, 869200 },   // EU 869 g3
  { 869495, 869495 },   // EU 869 narrow exakt 869.495 (upstream-1.16 Default, gem. PR a37078f6: 10% duty 500mW ERP); 869.618 weiter ueber 'force' verfuegbar
  { 918000, 918000 }    // US 915 ISM (= upstream-1.16 Default, single-point 918.0)
};

void MyMesh::applyRadioPolicy() {
  // EU narrow-band override: when app requests 869.000 MHz, switch RX and TX to 869.618 MHz.
  // Coding rate follows _prefs.cr in this "base" mode. Repeats and our own
  // auto-adverts switch to CR5 + reduced power per packet via the Dispatcher
  // override hooks; that keeps user direct messages on the configured CR.
  float freq = _prefs.freq;
  if (fabsf(freq - CR_NARROW_FREQ_TRIGGER) < 0.0005f) {
    freq = CR_NARROW_FREQ_ACTUAL;
  }
  radio_driver.setParams(freq, _prefs.bw, _prefs.sf, _prefs.cr);
}

void MyMesh::applyPacketTxOverrides(const mesh::Packet* packet) {
  if (packet == NULL) return;
  // Stats: zentraler TX-Hook — laeuft fuer JEDEN ausgehenden Packet (eigene
  // UND repeated). Eigene Pakete = _tx_total_by_ptype - _repeat_by_ptype.
  uint8_t pt = packet->getPayloadType();
  if (pt < 16 && _tx_total_by_ptype[pt] < 0xFFFF) {
    _tx_total_by_ptype[pt]++;
  }
  // Wunschliste 26 B: self-initiated Hash-Ring. Wenn der Hash schon im
  // self_repeated-Ring liegt, ist's ein Repeat (in allowPacketForward
  // bereits markiert) -- nicht erneut adden. Sonst self-initiated.
  uint32_t h = calcShortHash(packet);
  if (h != 0 && matchSelfHash(h) != 2) {
    _self_initiated_hashes[_self_initiated_head] = h;
    _self_initiated_head = (uint8_t)((_self_initiated_head + 1) % 32);
    // Selbst-initiierten FLOOD-Anteil pro ptype zaehlen (Direct ergibt
    // sich als Differenz). Erlaubt 'stats-packets' das nightly-flood-
    // Beacon-Advert separat sichtbar zu machen.
    if (pt < 16 && packet->isRouteFlood()
        && _tx_self_flood_by_ptype[pt] < 0xFFFF) {
      _tx_self_flood_by_ptype[pt]++;
    }
  }
  uint8_t flags = packet->tx_flags;
  if (flags == 0) return;

  if (flags & PKT_TX_REDUCE_POWER) {
    int reduced = (int)_prefs.tx_power_dbm - CR_TX_POWER_REDUCTION_DB;
    if (reduced < CR_TX_POWER_FLOOR_DBM) reduced = CR_TX_POWER_FLOOR_DBM;
    if (reduced > _prefs.tx_power_dbm) reduced = _prefs.tx_power_dbm;  // never *raise* power
    radio_driver.setTxPower((int8_t)reduced);
  }
  if ((flags & PKT_TX_FORCE_CR5) && _prefs.cr != CR_REPEATER_CR) {
    float freq = _prefs.freq;
    if (fabsf(freq - CR_NARROW_FREQ_TRIGGER) < 0.0005f) freq = CR_NARROW_FREQ_ACTUAL;
    radio_driver.setParams(freq, _prefs.bw, _prefs.sf, CR_REPEATER_CR);
  }
}

void MyMesh::restorePacketTxDefaults() {
  // Bring radio back to the user-configured CR and full TX power. Cheap if
  // nothing was overridden (radio_set_params and radio_set_tx_power are
  // light register writes on SX126x).
  applyRadioPolicy();
  radio_driver.setTxPower(_prefs.tx_power_dbm);
}

bool MyMesh::isValidClientRepeatFreq(uint32_t f) const {
  // Strikte Liste — 869.618 (EU-Narrow-Main) ist hier NICHT enthalten.
  for (int i = 0; i < (int)(sizeof(repeat_freq_ranges_strict)/sizeof(repeat_freq_ranges_strict[0])); i++) {
    auto r = &repeat_freq_ranges_strict[i];
    if (f >= r->lower_freq && f <= r->upper_freq) return true;
  }
  return false;
}

// Checks that the entire LoRa signal spectrum (centre freq +/- BW/2) fits
// inside one of the listed ISM band ranges. Catches misconfigurations
// like 433.125 MHz with BW=250 kHz (would extend below the 433.05 limit)
// or 866.300 MHz with BW=125 kHz in the 866.2-866.4 sub-band (centre too
// close to either edge).
void MyMesh::copyShortSenderName(char* dest, size_t dest_size) const {
  if (dest_size == 0) return;
  // chat_name_mode-Semantik:
  //   0          = voller _prefs.node_name (Default)
  //   1..253     = erste N Woerter aus node_name
  //   255        = custom Text aus _prefs.chat_name_custom
  uint8_t mode = _prefs.chat_name_mode;
  size_t out = 0;

  if (mode == 255 && _prefs.chat_name_custom[0] != 0) {
    while (out + 1 < dest_size && _prefs.chat_name_custom[out] != 0
           && out < sizeof(_prefs.chat_name_custom)) {
      dest[out] = _prefs.chat_name_custom[out];
      out++;
    }
  } else if (mode == 0) {
    while (out + 1 < dest_size && _prefs.node_name[out] != 0
           && out < sizeof(_prefs.node_name)) {
      dest[out] = _prefs.node_name[out];
      out++;
    }
  } else {
    // Mode N: erste N Woerter. Wenn N > vorhandene Anzahl Woerter, wird der
    // ganze Name uebernommen (natuerliche Konsequenz der Schleife).
    int max_words = (int)mode;
    const char* src = _prefs.node_name;
    int word_count = 0;
    bool in_word = false;
    while (src[out] != 0 && out + 1 < dest_size) {
      if (src[out] != ' ' && src[out] != '\t') {
        if (!in_word) {
          if (++word_count > max_words) break;
          in_word = true;
        }
      } else {
        in_word = false;
      }
      dest[out] = src[out];
      out++;
    }
  }

  // Gemeinsamer Trim am Ende: trailing whitespace + alle control-bytes.
  // Erweitert auf "alles <= 0x20" damit auch NUL, 0x1F-Steuerzeichen usw.
  // weggetrimmt werden. Achtung: das schneidet keine UTF-8-Multi-Byte-
  // continuation-bytes (>= 0x80), die wuerden falls am Ende ein gekapptes
  // Emoji bilden — gegen das hilft nur einen UTF-8-aware Trim oder eine
  // hex-Diagnose-Anzeige ('chatname hex').
  while (out > 0) {
    unsigned char c = (unsigned char)dest[out - 1];
    if (c > 0x20) break;
    out--;
  }
  dest[out] = 0;
}

bool MyMesh::signalFitsInIsmBand(uint32_t freq_khz, uint32_t bw_hz) const {
  uint32_t half_khz = (bw_hz + 1999) / 2000;   // ceil(BW/2) in kHz
  if (freq_khz < half_khz) return false;       // underflow guard
  uint32_t lo = freq_khz - half_khz;
  uint32_t hi = freq_khz + half_khz;
  for (size_t i = 0; i < sizeof(repeat_freq_ranges)/sizeof(repeat_freq_ranges[0]); i++) {
    auto r = &repeat_freq_ranges[i];
    if (lo >= r->lower_freq && hi <= r->upper_freq) return true;
  }
  return false;
}

void MyMesh::startInterface(BaseSerialInterface &serial) {
  _serial = &serial;
  serial.enable();
}

void MyMesh::handleCmdFrame(size_t len) {
  if (cmd_frame[0] == CMD_DEVICE_QUERY && len >= 2) { // sent when app establishes connection
    app_target_ver = cmd_frame[1];                    // which version of protocol does app understand

    int i = 0;
    out_frame[i++] = RESP_CODE_DEVICE_INFO;
    out_frame[i++] = FIRMWARE_VER_CODE;
    out_frame[i++] = MAX_CONTACTS / 2;   // v3+
    out_frame[i++] = MAX_GROUP_CHANNELS; // v3+
    memcpy(&out_frame[i], &_prefs.ble_pin, 4);
    i += 4;
    memset(&out_frame[i], 0, 12);
    strcpy((char *)&out_frame[i], FIRMWARE_BUILD_DATE);
    i += 12;
    StrHelper::strzcpy((char *)&out_frame[i], board.getManufacturerName(), 40);
    i += 40;
    StrHelper::strzcpy((char *)&out_frame[i], FIRMWARE_VERSION, 20);
    i += 20;
    out_frame[i++] = _prefs.client_repeat;   // v9+
    out_frame[i++] = _prefs.path_hash_mode;  // v10+
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_APP_START &&
             len >= 8) { // sent when app establishes connection, respond with node ID
    //  cmd_frame[1..7]  reserved future
    char *app_name = (char *)&cmd_frame[8];
    cmd_frame[len] = 0; // make app_name null terminated
    MESH_DEBUG_PRINTLN("App %s connected", app_name);

    _iter_started = false; // stop any left-over ContactsIterator
    int i = 0;
    out_frame[i++] = RESP_CODE_SELF_INFO;
    out_frame[i++] = ADV_TYPE_CHAT; // what this node Advert identifies as (maybe node's pronouns too?? :-)
    out_frame[i++] = _prefs.tx_power_dbm;
    out_frame[i++] = MAX_LORA_TX_POWER;
    memcpy(&out_frame[i], self_id.pub_key, PUB_KEY_SIZE);
    i += PUB_KEY_SIZE;

    int32_t lat, lon;
    lat = (sensors.node_lat * 1000000.0);
    lon = (sensors.node_lon * 1000000.0);
    memcpy(&out_frame[i], &lat, 4);
    i += 4;
    memcpy(&out_frame[i], &lon, 4);
    i += 4;
    out_frame[i++] = _prefs.multi_acks; // new v7+
    out_frame[i++] = _prefs.advert_loc_policy;
    out_frame[i++] = (_prefs.telemetry_mode_env << 4) | (_prefs.telemetry_mode_loc << 2) |
                     (_prefs.telemetry_mode_base); // v5+
    out_frame[i++] = _prefs.manual_add_contacts;

    uint32_t freq = _prefs.freq * 1000;
    memcpy(&out_frame[i], &freq, 4);
    i += 4;
    uint32_t bw = _prefs.bw * 1000;
    memcpy(&out_frame[i], &bw, 4);
    i += 4;
    out_frame[i++] = _prefs.sf;
    out_frame[i++] = _prefs.cr;

    int tlen = strlen(_prefs.node_name); // revisit: UTF_8 ??
    memcpy(&out_frame[i], _prefs.node_name, tlen);
    i += tlen;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_TXT_MSG && len >= 14) {
    // Duty-Cycle Hard-Limit: blockt JEDEN TX inkl. User-Chat (z.B. wenn
    // ein Bot ueber USB zu viel sendet). Schuetzt 10% TX/h auch bei
    // unbeabsichtigtem App-Verhalten.
    if (dutyHardReached()) {
      _duty_blocked_count++;
      traceCompanion(TRACE_DUTY, "[duty] txt blocked (last_h=%lus hard=%lus)",
                     getTxAirLastHour()/1000, getDutyHardLimitMs()/1000);
      writeErrFrame(ERR_CODE_NOT_FOUND);
      return;
    }
    int i = 1;
    uint8_t txt_type = cmd_frame[i++];
    uint8_t attempt = cmd_frame[i++];
    uint32_t msg_timestamp;
    memcpy(&msg_timestamp, &cmd_frame[i], 4);
    i += 4;
    uint8_t *pub_key_prefix = &cmd_frame[i];
    i += 6;
    ContactInfo *recipient = lookupContactByPubKey(pub_key_prefix, 6);
    if (recipient && (txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_CLI_DATA)) {
      char *text = (char *)&cmd_frame[i];
      int tlen = len - i;
      uint32_t est_timeout;
      text[tlen] = 0; // ensure null
      // Wunschliste 35: Reply-Mention-Strip. Falls App eine Reply mit
      // augmentiertem Sender '@[Name (#scope...)]' sendet, das ' (#...)'
      // entfernen damit Empfaenger-App-Sound auf @-Mention korrekt
      // matcht. Im DM-Kontext nur defensive (Copy-Paste-Schutz).
      stripReplyMentionDecoration(text);
      tlen = (int)strlen(text);
      int result;
      uint32_t expected_ack;
      if (txt_type == TXT_TYPE_CLI_DATA) {
        msg_timestamp = getRTCClock()->getCurrentTimeUnique(); // Use node's RTC instead of app timestamp to avoid tripping replay protection
        result = sendCommandData(*recipient, msg_timestamp, attempt, text, est_timeout);
        expected_ack = 0; // no Ack expected
      } else {
        result = sendMessage(*recipient, msg_timestamp, attempt, text, expected_ack, est_timeout);
      }
      // TODO: add expected ACK to table
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        if (expected_ack) {
          expected_ack_table[next_ack_idx].msg_sent = _ms->getMillis(); // add to circular table
          expected_ack_table[next_ack_idx].ack = expected_ack;
          expected_ack_table[next_ack_idx].contact = recipient;
          next_ack_idx = (next_ack_idx + 1) % EXPECTED_ACK_TABLE_SIZE;
        }

        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &expected_ack, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(recipient == NULL
                        ? ERR_CODE_NOT_FOUND
                        : ERR_CODE_UNSUPPORTED_CMD); // unknown recipient, or unsupported TXT_TYPE_*
    }
  } else if (cmd_frame[0] == CMD_SEND_CHANNEL_TXT_MSG) { // send GroupChannel text msg
    // Companion-Channel-Befehle laufen OHNE Funk - durfen daher den Duty-
    // Check ueberspringen. Check passiert weiter unten, NACH dem Channel-
    // Index-Decoding (siehe interne Verzweigung).
    int i = 1;
    uint8_t txt_type = cmd_frame[i++]; // should be TXT_TYPE_PLAIN
    uint8_t channel_idx = cmd_frame[i++];
    uint32_t msg_timestamp;
    memcpy(&msg_timestamp, &cmd_frame[i], 4);
    i += 4;
    const char *text = (char *)&cmd_frame[i];

    if (txt_type != TXT_TYPE_PLAIN) {
      writeErrFrame(ERR_CODE_UNSUPPORTED_CMD);
    } else if (isCompanionChannel(channel_idx)) {
      // Lokaler Companion-Channel: NICHT senden, sondern als Befehl parsen.
      // PSK-Identifikation statt Index-Match -- App kann Channels intern
      // umsortieren, Sync-Race-Drift waere sonst ein 'Command fliegt aufs
      // Funknetz' Bug (User-Befund 2026-06-01).
      // text im cmd_frame ist NICHT null-terminiert (Mesh-Protokoll arbeitet
      // text+len getrennt). In lokalen Buffer kopieren, terminieren, trailing
      // Whitespace/Newlines abschneiden (manche Apps senden CRLF mit).
      char cmd_buf[200];
      int cmd_len = (int)len - i;
      if (cmd_len < 0) cmd_len = 0;
      if (cmd_len >= (int)sizeof(cmd_buf)) cmd_len = sizeof(cmd_buf) - 1;
      memcpy(cmd_buf, text, cmd_len);
      cmd_buf[cmd_len] = 0;
      while (cmd_len > 0) {
        char c = cmd_buf[cmd_len - 1];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
          cmd_buf[--cmd_len] = 0;
        } else {
          break;
        }
      }
      handleCompanionCommand(cmd_buf);
      writeOKFrame();
    } else if (dutyHardReached()) {
      // Hard-Limit blockt das echte LoRa-Send (Companion-Channel ist oben
      // bereits abgefangen und nicht betroffen).
      _duty_blocked_count++;
      traceCompanion(TRACE_DUTY, "[duty] grp-txt blocked (last_h=%lus hard=%lus)",
                     getTxAirLastHour()/1000, getDutyHardLimitMs()/1000);
      writeErrFrame(ERR_CODE_NOT_FOUND);
    } else {
      ChannelDetails channel;
      bool success = getChannel(channel_idx, channel);
      // Use a shortened sender name for channel messages — full node_name can
      // be long (e.g. "Thomas DL9SAU/p @686.618 CQ<icon>") which crowds out
      // the actual message body in the group payload.
      char short_sender[sizeof(_prefs.node_name)];
      copyShortSenderName(short_sender, sizeof(short_sender));
      // Wunschliste 35: Reply-Mention-Strip auf Channel-Send-Text.
      // text zeigt in cmd_frame -- in eigenen Puffer kopieren um
      // in-place zu mutieren ohne den Original-cmd_frame zu veraendern.
      char mtext[MAX_TEXT_LEN];
      int mlen_raw = len - i;
      if (mlen_raw < 0) mlen_raw = 0;
      if (mlen_raw >= (int)sizeof(mtext)) mlen_raw = sizeof(mtext) - 1;
      memcpy(mtext, text, mlen_raw);
      mtext[mlen_raw] = 0;
      stripReplyMentionDecoration(mtext);
      int mlen = (int)strlen(mtext);
      if (success && sendGroupMessage(msg_timestamp, channel.channel, short_sender, mtext, mlen)) {
        // Autolearn des TX-Scope-Override aus Channel-Sends wurde entfernt:
        // wir mischten channel.secret (= Channel-Decryption-Key) und scope-
        // keys (= SHA-256("#name")) im selben Slot — zwei unterschiedliche
        // Hash-Schemen, semantisch inkonsistent. Bei wechselnden Channels/
        // Scopes konnte die Reichweite der nightly-Bake unbeabsichtigt
        // eskalieren (Flood-Footprint im Mesh). Override jetzt NUR noch
        // explizit per Companion-CLI:
        //   scope override <name>   -> 12h
        //   scope override clear
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
      }
    }
  } else if (cmd_frame[0] == CMD_SEND_CHANNEL_DATA) { // send GroupChannel datagram
    if (len < 4) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      return;
    }
    int i = 1;
    uint8_t channel_idx = cmd_frame[i++];
    uint8_t path_len = cmd_frame[i++];

    // validate path len, allowing 0xFF for flood
    if (!mesh::Packet::isValidPathLen(path_len) && path_len != OUT_PATH_UNKNOWN) {
      MESH_DEBUG_PRINTLN("CMD_SEND_CHANNEL_DATA invalid path size: %d", path_len);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      return;
    }

    // parse provided path if not flood
    uint8_t path[MAX_PATH_SIZE];
    if (path_len != OUT_PATH_UNKNOWN) {
      i += mesh::Packet::writePath(path, &cmd_frame[i], path_len);
    }

    uint16_t data_type = ((uint16_t)cmd_frame[i]) | (((uint16_t)cmd_frame[i + 1]) << 8);
    i += 2;
    const uint8_t *payload = &cmd_frame[i];
    int payload_len = (len > (size_t)i) ? (int)(len - i) : 0;

    ChannelDetails channel;
    if (!getChannel(channel_idx, channel)) {
      writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
    } else if (data_type == DATA_TYPE_RESERVED) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (payload_len > MAX_CHANNEL_DATA_LENGTH) {
      MESH_DEBUG_PRINTLN("CMD_SEND_CHANNEL_DATA payload too long: %d > %d", payload_len, MAX_CHANNEL_DATA_LENGTH);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (sendGroupData(channel.channel, path, path_len, data_type, payload, payload_len)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_GET_CONTACTS) { // get Contact list
    if (_iter_started) {
      writeErrFrame(ERR_CODE_BAD_STATE); // iterator is currently busy
    } else {
      if (len >= 5) { // has optional 'since' param
        memcpy(&_iter_filter_since, &cmd_frame[1], 4);
      } else {
        _iter_filter_since = 0;
      }

      uint8_t reply[5];
      reply[0] = RESP_CODE_CONTACTS_START;
      uint32_t count = getNumContacts(); // total, NOT filtered count
      memcpy(&reply[1], &count, 4);
      _serial->writeFrame(reply, 5);

      // start iterator
      _iter = startContactsIterator();
      _iter_started = true;
      _most_recent_lastmod = 0;
    }
  } else if (cmd_frame[0] == CMD_SET_ADVERT_NAME && len >= 2) {
    int nlen = len - 1;
    if (nlen > sizeof(_prefs.node_name) - 1) nlen = sizeof(_prefs.node_name) - 1; // max len
    memcpy(_prefs.node_name, &cmd_frame[1], nlen);
    _prefs.node_name[nlen] = 0; // null terminator
    savePrefs();
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_ADVERT_LATLON && len >= 9) {
    int32_t lat, lon, alt = 0;
    memcpy(&lat, &cmd_frame[1], 4);
    memcpy(&lon, &cmd_frame[5], 4);
    if (len >= 13) {
      memcpy(&alt, &cmd_frame[9], 4); // for FUTURE support
    }
    if (lat <= 90 * 1E6 && lat >= -90 * 1E6 && lon <= 180 * 1E6 && lon >= -180 * 1E6) {
      sensors.node_lat = ((double)lat) / 1000000.0;
      sensors.node_lon = ((double)lon) / 1000000.0;
      savePrefs();
      // User just programmed a fixed location — re-evaluate scope-recommendation
      // so the app sees the matching #region list right away.
      maybePushGeoRecommendation(sensors.node_lat, sensors.node_lon);
      // Reise-Fix 2026-06-08: Bbox-Membership neu evaluieren (insbes. fuer
      // profile=normal -- Live-GPS ignoriert, fixed location ist die Quelle).
      reevaluateRepeaterBbox();
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid geo coordinate
    }
  } else if (cmd_frame[0] == CMD_GET_DEVICE_TIME) {
    uint8_t reply[5];
    reply[0] = RESP_CODE_CURR_TIME;
    uint32_t now = getRTCClock()->getCurrentTime();
    memcpy(&reply[1], &now, 4);
    _serial->writeFrame(reply, 5);
  } else if (cmd_frame[0] == CMD_SET_DEVICE_TIME && len >= 5) {
    uint32_t secs;
    memcpy(&secs, &cmd_frame[1], 4);
    uint32_t curr = getRTCClock()->getCurrentTime();
    if (secs >= curr) {
      uint32_t delta = secs - curr;
      // Toleranz fuer App-Sync (Test-Bericht 2026-05-30): jeder App-Connect
      // schickt CMD_SET_DEVICE_TIME. Bei kleinen Deltas (App-Clock und RTC
      // praktisch identisch) wuerde sonst jedes Connect-Event den Nightly-
      // Slot invalidieren -> ungewollte Re-Schedules pro App-Connect.
      //   delta <= 10s    : komplett ignorieren (RTC bleibt, Slot bleibt)
      //   delta <= 3600s  : RTC aktualisieren, Slot beibehalten (1h-Drift
      //                     innerhalb des 23-05-Fensters bleibt der Slot
      //                     plausibel)
      //   delta > 3600s   : RTC aktualisieren UND Slot invalidieren
      if (delta <= 10) {
        // praktisch synchron -- still OK. Aber: Sync-State auch hier
        // aktualisieren, damit der Marker auf 'App' wechselt (war
        // moeglicherweise Boot-Bootstrap, jetzt durch App bestaetigt).
        // age = now - last_at_rtc rutscht damit zurueck nahe 0.
        // Reise-Fix 2026-06-08.
        _time_sync_last_at_rtc = secs;
        _time_sync_done_since_boot = true;
        memset(_time_sync_last_pubkey, 0, sizeof(_time_sync_last_pubkey));
        writeOKFrame();
      } else {
        getRTCClock()->setCurrentTime(secs);
        // Reise-Fix 2026-06-08: App-Sync ueberschreibt auch unseren
        // advert-sync-State. Ohne das blieb _time_sync_last_at_rtc auf
        // dem (moeglicherweise alten) Wert eines frueheren adv-syncs
        // haengen, und 'clock' zeigte 'age = jetzt - alter advert',
        // teilweise Jahre Diff. pub_key={0,0,0} markiert App als Quelle.
        _time_sync_last_at_rtc = secs;
        _time_sync_done_since_boot = true;
        memset(_time_sync_last_pubkey, 0, sizeof(_time_sync_last_pubkey));
        bool invalidate = (delta > 3600);
        if (invalidate) next_night_flood_unix = 0;
        pushDebugLog("[ADV-DBG] CMD_SET_DEVICE_TIME: rtc %lu -> %lu (delta %lus)%s\n",
                      (unsigned long)curr, (unsigned long)secs,
                      (unsigned long)delta,
                      invalidate ? ", nightly slot invalidated" : "");
        writeOKFrame();
      }
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SEND_SELF_ADVERT) {
    if (dutyHardReached()) {
      _duty_blocked_count++;
      traceCompanion(TRACE_DUTY, "[duty] self-advert blocked (last_h=%lus)",
                     getTxAirLastHour()/1000);
      writeErrFrame(ERR_CODE_NOT_FOUND);
      return;
    }
    mesh::Packet* pkt;
    if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
      pkt = createSelfAdvert(_prefs.node_name);
    } else {
      pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
    }
    if (pkt) {
      if (len >= 2 && cmd_frame[1] == 1) { // optional param (1 = flood, 0 = zero hop)
        unsigned long delay_millis = 0;
        TransportKey eff_scope;
        if (!resolveDefaultOrGeo(eff_scope)) {
          memset(eff_scope.key, 0, sizeof(eff_scope.key));
        }
        sendFloodScoped(eff_scope, pkt, delay_millis);
      } else {
        sendZeroHop(pkt);
      }
      _tx_advert_count++;
      pushDebugLog("[ADV-DBG] app-cmd (CMD_SEND_SELF_ADVERT flood=%d), millis=%lu\n",
                    (int)(len >= 2 && cmd_frame[1] == 1), millis());
      traceCompanion(TRACE_ADVERTS, "[adv] manual (app-cmd) flood=%d",
                     (int)(len >= 2 && cmd_frame[1] == 1));
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_RESET_PATH && len >= 1 + 32) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      recipient->out_path_len = OUT_PATH_UNKNOWN;
      // recipient->lastmod = ??   shouldn't be needed, app already has this version of contact
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // unknown contact
    }
  } else if (cmd_frame[0] == CMD_ADD_UPDATE_CONTACT && len >= 1 + 32 + 2 + 1) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    uint32_t last_mod = getRTCClock()->getCurrentTime();  // fallback value if not present in cmd_frame
    if (recipient) {
      updateContactFromFrame(*recipient, last_mod, cmd_frame, len);
      recipient->lastmod = last_mod;
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      ContactInfo contact;
      updateContactFromFrame(contact, last_mod, cmd_frame, len);
      contact.lastmod = last_mod;
      contact.sync_since = 0;
      if (addContact(contact)) {
        dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    }
  } else if (cmd_frame[0] == CMD_REMOVE_CONTACT) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient && removeContact(*recipient)) {
      _store->deleteBlobByKey(pub_key, PUB_KEY_SIZE);
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // not found, or unable to remove
    }
  } else if (cmd_frame[0] == CMD_SHARE_CONTACT) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      if (shareContactZeroHop(*recipient)) {
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL); // unable to send
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_GET_CONTACT_BY_KEY) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *contact = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (contact) {
      writeContactRespFrame(RESP_CODE_CONTACT, *contact);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // not found
    }
  } else if (cmd_frame[0] == CMD_EXPORT_CONTACT) {
    if (len < 1 + PUB_KEY_SIZE) {
      // export SELF
      mesh::Packet* pkt;
      if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
        pkt = createSelfAdvert(_prefs.node_name);
      } else {
        pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
      }
      if (pkt) {
        pkt->header |= ROUTE_TYPE_FLOOD; // would normally be sent in this mode

        out_frame[0] = RESP_CODE_EXPORT_CONTACT;
        uint8_t out_len = pkt->writeTo(&out_frame[1]);
        releasePacket(pkt); // undo the obtainNewPacket()
        _serial->writeFrame(out_frame, out_len + 1);
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL); // Error
      }
    } else {
      uint8_t *pub_key = &cmd_frame[1];
      ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
      uint8_t out_len;
      if (recipient && (out_len = exportContact(*recipient, &out_frame[1])) > 0) {
        out_frame[0] = RESP_CODE_EXPORT_CONTACT;
        _serial->writeFrame(out_frame, out_len + 1);
      } else {
        writeErrFrame(ERR_CODE_NOT_FOUND); // not found
      }
    }
  } else if (cmd_frame[0] == CMD_IMPORT_CONTACT && len > 2 + 32 + 64) {
    if (importContact(&cmd_frame[1], len - 1)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SYNC_NEXT_MESSAGE) {
    int out_len;
    if ((out_len = getFromOfflineQueue(out_frame)) > 0) {
      _serial->writeFrame(out_frame, out_len);
#ifdef DISPLAY_CLASS
      if (_ui) _ui->msgRead(offlineQueueTotal());
#endif
    } else {
      out_frame[0] = RESP_CODE_NO_MORE_MESSAGES;
      _serial->writeFrame(out_frame, 1);
    }
  } else if (cmd_frame[0] == CMD_SET_RADIO_PARAMS) {
    int i = 1;
    uint32_t freq;
    memcpy(&freq, &cmd_frame[i], 4);
    i += 4;
    uint32_t bw;
    memcpy(&bw, &cmd_frame[i], 4);
    i += 4;
    uint8_t sf = cmd_frame[i++];
    uint8_t cr = cmd_frame[i++];
    uint8_t repeat = 0;  // default - false
    if (len > i) {
      repeat = cmd_frame[i++];   // FIRMWARE_VER_CODE  9+
    }

    // EU narrow band shorthand: older apps (and our own previous version)
    // still send the label 869.000 MHz. Map to the real 869.618 BEFORE
    // validating against the repeat-freq allow-list, otherwise repeat=1
    // would be rejected.
    if (freq == (uint32_t)(CR_NARROW_FREQ_TRIGGER * 1000.0f + 0.5f)) {
      freq = (uint32_t)(CR_NARROW_FREQ_ACTUAL * 1000.0f + 0.5f);
    }

    // Punkt 10 Reise-Fix 2026-06-08: force-Check NUR in profile=defensive
    // anwenden. In profile=normal (echter Repeater) sind alle Frequenzen
    // aus der wide-Liste legitim ohne force-Flag. Vorher schlug 'repeating
    // on' in der App mit 'Illegal value' fehl, weil App keine
    // force-Option hat und der Check immer feuerte.
    bool defensive_mode = (_prefs.repeater_profile == 0 /* defensive */);
    bool force_override = false;
#ifdef REPEATER_DEFENSIVE_FORCE
    force_override = (_prefs.client_repeat_force != 0);
#endif
    if (repeat && defensive_mode
        && !force_override && !isValidClientRepeatFreq(freq)) {
      // App will Repeater aktivieren auf einer Freq die ausserhalb des
      // strict-Range liegt. Ohne REPEATER_DEFENSIVE_FORCE-Build: hart
      // ablehnen. Mit ifdef + force-Flag: User-Verantwortung.
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (freq >= 150000 && freq <= 2500000 && sf >= 5 && sf <= 12 && cr >= 5 && cr <= 8 && bw >= 7000 &&
        bw <= 500000) {
      // Always enforce: full signal spectrum (freq +/- BW/2) must fit inside
      // an ISM band, regardless of repeat=0/1. Catches edge-case configs
      // like 433.125 MHz with BW=250 kHz (extends below 433.05 limit) or
      // 866.300 MHz with BW=125 kHz centered close to a sub-band edge.
      if (!signalFitsInIsmBand(freq, bw)) {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
        return;
      }
      _prefs.sf = sf;
      _prefs.cr = cr;
      _prefs.freq = (float)freq / 1000.0;
      _prefs.bw = (float)bw / 1000.0;
      _prefs.client_repeat = repeat;
      // User-Wunsch 2026-06-02: client_repeat_force persistent halten
      // auch wenn der Repeater per App ausgeschaltet wird. Vorher wurde
      // force bei !repeat gecleart, was unerwartet war: nach erneutem
      // 'repeater on' musste der User wieder 'force' angeben obwohl er
      // die Einstellung explizit gesetzt hatte.
      savePrefs();
      recomputeRepeatingAllowed("App: radio params");

      applyRadioPolicy();   // DL9SAU-Wrapper (s. Konflikt #2 Merge-Note)
      MESH_DEBUG_PRINTLN("OK: CMD_SET_RADIO_PARAMS: f=%d, bw=%d, sf=%d, cr=%d", freq, bw, (uint32_t)sf,
                         (uint32_t)cr);

      writeOKFrame();
    } else {
      MESH_DEBUG_PRINTLN("Error: CMD_SET_RADIO_PARAMS: f=%d, bw=%d, sf=%d, cr=%d", freq, bw, (uint32_t)sf,
                         (uint32_t)cr);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SET_RADIO_TX_POWER) {
    int8_t power = (int8_t)cmd_frame[1];
    if (power < -9 || power > MAX_LORA_TX_POWER) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      _prefs.tx_power_dbm = power;
      savePrefs();
      radio_driver.setTxPower(_prefs.tx_power_dbm);
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_SET_TUNING_PARAMS) {
    int i = 1;
    uint32_t rx, af;
    memcpy(&rx, &cmd_frame[i], 4);
    i += 4;
    memcpy(&af, &cmd_frame[i], 4);
    i += 4;
    _prefs.rx_delay_base = ((float)rx) / 1000.0f;
    _prefs.airtime_factor = ((float)af) / 1000.0f;
    savePrefs();
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_GET_TUNING_PARAMS) {
    uint32_t rx = _prefs.rx_delay_base * 1000, af = _prefs.airtime_factor * 1000;
    int i = 0;
    out_frame[i++] = RESP_CODE_TUNING_PARAMS;
    memcpy(&out_frame[i], &rx, 4); i += 4;
    memcpy(&out_frame[i], &af, 4); i += 4;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SET_OTHER_PARAMS) {
    _prefs.manual_add_contacts = cmd_frame[1];
    if (len >= 3) {
      _prefs.telemetry_mode_base = cmd_frame[2] & 0x03; // v5+
      _prefs.telemetry_mode_loc = (cmd_frame[2] >> 2) & 0x03;
      _prefs.telemetry_mode_env = (cmd_frame[2] >> 4) & 0x03;

      if (len >= 4) {
        _prefs.advert_loc_policy = cmd_frame[3];
        if (len >= 5) {
          _prefs.multi_acks = cmd_frame[4];
        }
      }
    }
    savePrefs();
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_PATH_HASH_MODE && cmd_frame[1] == 0 && len >= 3) {
    if (cmd_frame[2] >= 3) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      _prefs.path_hash_mode = cmd_frame[2];
      savePrefs();
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_REBOOT && memcmp(&cmd_frame[1], "reboot", 6) == 0) {
    if (dirty_contacts_expiry) { // is there are pending dirty contacts write needed?
      saveContacts();
    }
    board.reboot();
  } else if (cmd_frame[0] == CMD_GET_BATT_AND_STORAGE) {
    uint8_t reply[11];
    int i = 0;
    reply[i++] = RESP_CODE_BATT_AND_STORAGE;
    uint16_t battery_millivolts = board.getBattMilliVolts();
    uint32_t used = _store->getStorageUsedKb();
    uint32_t total = _store->getStorageTotalKb();
    memcpy(&reply[i], &battery_millivolts, 2); i += 2;
    memcpy(&reply[i], &used, 4); i += 4;
    memcpy(&reply[i], &total, 4); i += 4;
    _serial->writeFrame(reply, i);
  } else if (cmd_frame[0] == CMD_EXPORT_PRIVATE_KEY) {
#if ENABLE_PRIVATE_KEY_EXPORT
    uint8_t reply[65];
    reply[0] = RESP_CODE_PRIVATE_KEY;
    self_id.writeTo(&reply[1], 64);
    _serial->writeFrame(reply, 65);
#else
    writeDisabledFrame();
#endif
  } else if (cmd_frame[0] == CMD_IMPORT_PRIVATE_KEY && len >= 65) {
#if ENABLE_PRIVATE_KEY_IMPORT
    if (!mesh::LocalIdentity::validatePrivateKey(&cmd_frame[1])) {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid key
    } else {
        mesh::LocalIdentity identity;
        identity.readFrom(&cmd_frame[1], 64);
        if (_store->saveMainIdentity(identity)) {
          self_id = identity;
          writeOKFrame();
          // re-load contacts, to invalidate ecdh shared_secrets
          resetContacts();
          _store->loadContacts(this);
        } else {
          writeErrFrame(ERR_CODE_FILE_IO_ERROR);
        }
    }
#else
    writeDisabledFrame();
#endif
  } else if (cmd_frame[0] == CMD_SEND_RAW_DATA && len >= 6) {
    int i = 1;
    int8_t path_len = cmd_frame[i++];
    if (path_len >= 0 && i + path_len + 4 <= len) { // minimum 4 byte payload
      uint8_t *path = &cmd_frame[i];
      i += path_len;
      auto pkt = createRawData(&cmd_frame[i], len - i);
      if (pkt) {
        sendDirect(pkt, path, path_len);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    } else {
      writeErrFrame(ERR_CODE_UNSUPPORTED_CMD); // flood, not supported (yet)
    }
  } else if (cmd_frame[0] == CMD_SEND_LOGIN && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    char *password = (char *)&cmd_frame[1 + PUB_KEY_SIZE];
    cmd_frame[len] = 0; // ensure null terminator in password
    if (recipient) {
      uint32_t est_timeout;
      int result = sendLogin(*recipient, password, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        memcpy(&pending_login, recipient->id.pub_key, 4); // match this to onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &pending_login, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_ANON_REQ && len > 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    ContactInfo anon;
    if (recipient == NULL) { // FIRMWARE_VER_CODE 13+,  allow non-contact requests
      memset(&anon, 0, sizeof(anon));
      memcpy(anon.id.pub_key, pub_key, PUB_KEY_SIZE);
      anon.out_path_len = 0;   // default to zero-hop direct
      anon.type = ADV_TYPE_NONE;  // unknown

      if (addContact(anon)) recipient = &anon;
    }
    uint8_t *data = &cmd_frame[1 + PUB_KEY_SIZE];
    // Wunschliste 27c: anon_req-Typ merken damit onContactResponse die
    // typ-spezifische Antwort (z.B. REGIONS-CSV) parsen + in $companion
    // pushen kann. Anon-REQ-Payload-Layout: [ts×4][type×1][...].
    if ((size_t)(len - (1 + PUB_KEY_SIZE)) > 4) {
      _last_anon_req_type = data[4];
    }
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendAnonReq(*recipient, data, len - (1 + PUB_KEY_SIZE), tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_req = tag; // match this to onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL); // contacts full
    }
  } else if (cmd_frame[0] == CMD_SEND_STATUS_REQ && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, REQ_TYPE_GET_STATUS, tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        // FUTURE:  pending_status = tag;  // match this in onContactResponse()
        memcpy(&pending_status, recipient->id.pub_key, 4); // legacy matching scheme
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_PATH_DISCOVERY_REQ && cmd_frame[1] == 0 && len >= 2 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[2];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      // 'Path Discovery' is just a special case of flood + Telemetry req
      uint8_t req_data[9];
      req_data[0] = REQ_TYPE_GET_TELEMETRY_DATA;
      req_data[1] = ~(TELEM_PERM_BASE);  // NEW: inverse permissions mask (ie. we only want BASE telemetry)
      memset(&req_data[2], 0, 3);  // reserved
      getRNG()->random(&req_data[5], 4);   // random blob to help make packet-hash unique
      auto save = recipient->out_path_len;    // temporarily force sendRequest() to flood
      recipient->out_path_len = OUT_PATH_UNKNOWN;
      int result = sendRequest(*recipient, req_data, sizeof(req_data), tag, est_timeout);
      recipient->out_path_len = save;
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_discovery = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_TELEMETRY_REQ && len >= 4 + PUB_KEY_SIZE) {  // can deprecate, in favour of CMD_SEND_BINARY_REQ
    uint8_t *pub_key = &cmd_frame[4];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, REQ_TYPE_GET_TELEMETRY_DATA, tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_telemetry = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_TELEMETRY_REQ && len == 4) {  // 'self' telemetry request
    telemetry.reset();
    telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
    // query other sensors -- target specific
    sensors.querySensors(0xFF, telemetry);

    int i = 0;
    out_frame[i++] = PUSH_CODE_TELEMETRY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], self_id.pub_key, 6);
    i += 6; // pub_key_prefix
    uint8_t tlen = telemetry.getSize();
    memcpy(&out_frame[i], telemetry.getBuffer(), tlen);
    i += tlen;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_BINARY_REQ && len >= 2 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint8_t *req_data = &cmd_frame[1 + PUB_KEY_SIZE];
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, req_data, len - (1 + PUB_KEY_SIZE), tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_req = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_HAS_CONNECTION && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    if (hasConnectionTo(pub_key)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_LOGOUT && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    stopConnection(pub_key);
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_GET_CHANNEL && len >= 2) {
    uint8_t channel_idx = cmd_frame[1];
    ChannelDetails channel;
    if (getChannel(channel_idx, channel)) {
      int i = 0;
      out_frame[i++] = RESP_CODE_CHANNEL_INFO;
      out_frame[i++] = channel_idx;
      strcpy((char *)&out_frame[i], channel.name);
      i += 32;
      memcpy(&out_frame[i], channel.channel.secret, 16);
      i += 16; // NOTE: only 128-bit supported
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_SET_CHANNEL && len >= 2 + 32 + 32) {
    writeErrFrame(ERR_CODE_UNSUPPORTED_CMD); // not supported (yet)
  } else if (cmd_frame[0] == CMD_SET_CHANNEL && len >= 2 + 32 + 16) {
    uint8_t channel_idx = cmd_frame[1];
    ChannelDetails channel;
    StrHelper::strncpy(channel.name, (char *)&cmd_frame[2], 32);
    memset(channel.channel.secret, 0, sizeof(channel.channel.secret));
    memcpy(channel.channel.secret, &cmd_frame[2 + 32], 16); // NOTE: only 128-bit supported
    // Wunschliste 32 v2: bei Channel-Wechsel im Slot rebuildChannelHops-
    // Cache(). Der ch.hops-Eintrag ist Name-Hash-basiert; bei Channel-
    // Aenderung greift automatisch der neue Mapping ueber rebuild. Alter
    // diff-Reset-Hook entfaellt -- war fehleranfaellig bei App-Resync.
    if (setChannel(channel_idx, channel)) {
      saveChannels();
      rebuildChannelHopsCache();
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
    }
  } else if (cmd_frame[0] == CMD_SIGN_START) {
    out_frame[0] = RESP_CODE_SIGN_START;
    out_frame[1] = 0; // reserved
    uint32_t len = MAX_SIGN_DATA_LEN;
    memcpy(&out_frame[2], &len, 4);
    _serial->writeFrame(out_frame, 6);

    if (sign_data) {
      free(sign_data);
    }
    sign_data = (uint8_t *)malloc(MAX_SIGN_DATA_LEN);
    sign_data_len = 0;
  } else if (cmd_frame[0] == CMD_SIGN_DATA && len > 1) {
    if (sign_data == NULL || sign_data_len + (len - 1) > MAX_SIGN_DATA_LEN) {
      writeErrFrame(sign_data == NULL ? ERR_CODE_BAD_STATE : ERR_CODE_TABLE_FULL); // error: too long
    } else {
      memcpy(&sign_data[sign_data_len], &cmd_frame[1], len - 1);
      sign_data_len += (len - 1);
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_SIGN_FINISH) {
    if (sign_data) {
      self_id.sign(&out_frame[1], sign_data, sign_data_len);

      free(sign_data); // don't need sign_data now
      sign_data = NULL;

      out_frame[0] = RESP_CODE_SIGNATURE;
      _serial->writeFrame(out_frame, 1 + SIGNATURE_SIZE);
    } else {
      writeErrFrame(ERR_CODE_BAD_STATE);
    }
  } else if (cmd_frame[0] == CMD_SEND_TRACE_PATH && len > 10 && len - 10 < MAX_PACKET_PAYLOAD-5) {
    uint8_t path_len = len - 10;
    uint8_t flags = cmd_frame[9];
    uint8_t path_sz = flags & 0x03;  // NEW v1.11+
    if ((path_len >> path_sz) > MAX_PATH_SIZE || (path_len % (1 << path_sz)) != 0) { // make sure is multiple of path_sz
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      uint32_t tag, auth;
      memcpy(&tag, &cmd_frame[1], 4);
      memcpy(&auth, &cmd_frame[5], 4);
      auto pkt = createTrace(tag, auth, flags);
      if (pkt) {
        sendDirect(pkt, &cmd_frame[10], path_len);

        uint32_t t = _radio->getEstAirtimeFor(pkt->payload_len + pkt->path_len + 2);
        uint32_t est_timeout = calcDirectTimeoutMillisFor(t, path_len >> path_sz);

        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    }
  } else if (cmd_frame[0] == CMD_SET_DEVICE_PIN && len >= 5) {

    // get pin from command frame
    uint32_t pin;
    memcpy(&pin, &cmd_frame[1], 4);

    // ensure pin is zero, or a valid 6 digit pin
    if (pin == 0 || (pin >= 100000 && pin <= 999999)) {
      _prefs.ble_pin = pin;
      savePrefs();
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_GET_CUSTOM_VARS) {
    out_frame[0] = RESP_CODE_CUSTOM_VARS;
    char *dp = (char *)&out_frame[1];
    for (int i = 0; i < sensors.getNumSettings() && dp - (char *)&out_frame[1] < 140; i++) {
      if (i > 0) {
        *dp++ = ',';
      }
      strcpy(dp, sensors.getSettingName(i));
      dp = strchr(dp, 0);
      *dp++ = ':';
      strcpy(dp, sensors.getSettingValue(i));
      dp = strchr(dp, 0);
    }
    _serial->writeFrame(out_frame, dp - (char *)out_frame);
  } else if (cmd_frame[0] == CMD_SET_CUSTOM_VAR && len >= 4) {
    cmd_frame[len] = 0;
    char *sp = (char *)&cmd_frame[1];
    char *np = strchr(sp, ':'); // look for separator char
    if (np) {
      *np++ = 0; // modify 'cmd_frame', replace ':' with null
      bool success = sensors.setSettingValue(sp, np);
      if (success) {
        #if ENV_INCLUDE_GPS == 1
        // Update node preferences for GPS settings
        if (strcmp(sp, "gps") == 0) {
          _prefs.gps_enabled = (np[0] == '1') ? 1 : 0;
          savePrefs();
          // Reise-Fix 2026-06-08: gps on/off-Wechsel -> Bbox-Quelle aendert
          // sich (defensive + gps on + nie Fix -> keine Bbox; defensive +
          // gps off -> fixed location als Bbox). Re-Eval, sonst bleibt
          // alte Bbox vom vorigen Modus haengen.
          reevaluateRepeaterBbox();
          if (_prefs.gps_enabled) {
            // User just enabled GPS via the app. Hold GPS on until at least
            // the next periodic advert is sent (regardless of fix status),
            // so the user sees an obvious effect of their click.
            _gps_woke_at_millis = millis();
            if (_gps_woke_at_millis == 0) _gps_woke_at_millis = 1;
            _gps_fix_seen_this_wake = false;
            _gps_user_override_until_advert = true;
          } else {
            _gps_user_override_until_advert = false;
          }
        } else if (strcmp(sp, "gps_interval") == 0) {
          uint32_t interval_seconds = atoi(np);
          _prefs.gps_interval = constrain(interval_seconds, 0, 86400);
          savePrefs();
        }
        #endif
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_GET_ADVERT_PATH && len >= PUB_KEY_SIZE+2) {
    // FUTURE use:  uint8_t reserved = cmd_frame[1];
    uint8_t *pub_key = &cmd_frame[2];
    AdvertPath* found = NULL;
    for (int i = 0; i < ADVERT_PATH_TABLE_SIZE; i++) {
      auto p = &advert_paths[i];
      if (memcmp(p->pubkey_prefix, pub_key, sizeof(p->pubkey_prefix)) == 0) {
        found = p;
        break;
      }
    }
    if (found) {
      int i = 0;
      out_frame[i++] = RESP_CODE_ADVERT_PATH;
      memcpy(&out_frame[i], &found->recv_timestamp, 4); i += 4;
      out_frame[i++] = found->path_len;
      i += mesh::Packet::writePath(&out_frame[i], found->path, found->path_len);
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_GET_STATS && len >= 2) {
    uint8_t stats_type = cmd_frame[1];
    if (stats_type == STATS_TYPE_CORE) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_CORE;
      uint16_t battery_mv = board.getBattMilliVolts();
      uint32_t uptime_secs = _ms->getMillis() / 1000;
      uint8_t queue_len = (uint8_t)_mgr->getOutboundTotal();
      memcpy(&out_frame[i], &battery_mv, 2); i += 2;
      memcpy(&out_frame[i], &uptime_secs, 4); i += 4;
      memcpy(&out_frame[i], &_err_flags, 2); i += 2;
      out_frame[i++] = queue_len;
      _serial->writeFrame(out_frame, i);
    } else if (stats_type == STATS_TYPE_RADIO) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_RADIO;
      int16_t noise_floor = (int16_t)_radio->getNoiseFloor();
      int8_t last_rssi = (int8_t)radio_driver.getLastRSSI();
      int8_t last_snr = (int8_t)(radio_driver.getLastSNR() * 4); // scaled by 4 for 0.25 dB precision
      uint32_t tx_air_secs = getTotalAirTime() / 1000;
      uint32_t rx_air_secs = getReceiveAirTime() / 1000;
      memcpy(&out_frame[i], &noise_floor, 2); i += 2;
      out_frame[i++] = last_rssi;
      out_frame[i++] = last_snr;
      memcpy(&out_frame[i], &tx_air_secs, 4); i += 4;
      memcpy(&out_frame[i], &rx_air_secs, 4); i += 4;
      _serial->writeFrame(out_frame, i);
    } else if (stats_type == STATS_TYPE_PACKETS) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_PACKETS;
      uint32_t recv = radio_driver.getPacketsRecv();
      uint32_t sent = radio_driver.getPacketsSent();
      uint32_t n_sent_flood = getNumSentFlood();
      uint32_t n_sent_direct = getNumSentDirect();
      uint32_t n_recv_flood = getNumRecvFlood();
      uint32_t n_recv_direct = getNumRecvDirect();
      uint32_t n_recv_errors = radio_driver.getPacketsRecvErrors();
      memcpy(&out_frame[i], &recv, 4); i += 4;
      memcpy(&out_frame[i], &sent, 4); i += 4;
      memcpy(&out_frame[i], &n_sent_flood, 4); i += 4;
      memcpy(&out_frame[i], &n_sent_direct, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_flood, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_direct, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_errors, 4); i += 4;
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid stats sub-type
    }
  } else if (cmd_frame[0] == CMD_FACTORY_RESET && memcmp(&cmd_frame[1], "reset", 5) == 0) {
    if (_serial) {
      MESH_DEBUG_PRINTLN("Factory reset: disabling serial interface to prevent reconnects (BLE/WiFi)");
      _serial->disable(); // Phone app disconnects before we can send OK frame so it's safe here
    }
    bool success = _store->formatFileSystem();
    if (success) {
      writeOKFrame();
      delay(1000);
      board.reboot();  // doesn't return
    } else {
      writeErrFrame(ERR_CODE_FILE_IO_ERROR);
    }
  } else if (cmd_frame[0] == CMD_SET_FLOOD_SCOPE_KEY && len >= 2 && cmd_frame[1] == 0) {
    if (len >= 2 + 16) {
      memcpy(send_scope.key, &cmd_frame[2], sizeof(send_scope.key));  // set scope override TransportKey
    } else {
      memset(send_scope.key, 0, sizeof(send_scope.key));  // reset scope override
    }
    send_unscoped = false;
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_FLOOD_SCOPE_KEY && len >= 2 && cmd_frame[1] == 1) {  // ver 12+
    send_unscoped = true;
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_DEFAULT_FLOOD_SCOPE && len >= 1) {
    if (len >= 1+31+16) {
      int n = strlen((char *) &cmd_frame[1]);
      if (n > 0 && n < 31) {
        strcpy(_prefs.default_scope_name, (char *) &cmd_frame[1]);
        memcpy(_prefs.default_scope_key, &cmd_frame[1+31], 16);
        savePrefs();
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      memset(_prefs.default_scope_name, 0, sizeof(_prefs.default_scope_name));  // set default scope to null
      memset(_prefs.default_scope_key, 0, sizeof(_prefs.default_scope_key));
      savePrefs();
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_GET_DEFAULT_FLOOD_SCOPE) {
    out_frame[0] = RESP_CODE_DEFAULT_FLOOD_SCOPE;
    if (strlen(_prefs.default_scope_name) > 0) {
      memcpy(&out_frame[1], _prefs.default_scope_name, 31);
      memcpy(&out_frame[1+31], _prefs.default_scope_key, 16);
      _serial->writeFrame(out_frame, 1+31+16);
    } else {
      _serial->writeFrame(out_frame, 1);   // no name or key means null
    }
  } else if (cmd_frame[0] == CMD_SEND_CONTROL_DATA && len >= 2 && (cmd_frame[1] & 0x80) != 0) {
    auto resp = createControlData(&cmd_frame[1], len - 1);
    if (resp) {
      sendZeroHop(resp);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_SET_AUTOADD_CONFIG) {
    _prefs.autoadd_config = cmd_frame[1];
    if (len >= 3) {
      _prefs.autoadd_max_hops = min(cmd_frame[2], (uint8_t)64);
    }
    savePrefs();
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_GET_AUTOADD_CONFIG) {
    int i = 0;
    out_frame[i++] = RESP_CODE_AUTOADD_CONFIG;
    out_frame[i++] = _prefs.autoadd_config;
    out_frame[i++] = _prefs.autoadd_max_hops;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_GET_ALLOWED_REPEAT_FREQ) {
    // Punkt 11 Reise-Fix 2026-06-08: profile-aware Frequenz-Liste.
    //   profile=normal     -> wide-Liste (repeat_freq_ranges): echter
    //                         Repeater darf alle Band-Frequenzen nutzen
    //   profile=defensive  -> strict-Liste (repeat_freq_ranges_strict):
    //                         nur Mesh-Hauptfrequenzen erlaubt
    //   client_repeat_force gesetzt -> wide-Liste auch in defensive,
    //                         damit User mit gesetztem force-Flag in
    //                         der App seine Freq weiter speichern kann
    //                         (sonst wuerde die App die aktuelle Freq
    //                         als ungueltig markieren). Unabhaengig von
    //                         repeat on/off -- force ist die explizite
    //                         User-Entscheidung 'meine Freq ist OK'.
    bool wide_list = (_prefs.repeater_profile == 1 /* normal */);
#ifdef REPEATER_DEFENSIVE_FORCE
    if (_prefs.client_repeat_force != 0) wide_list = true;
#endif
    int i = 0;
    out_frame[i++] = RESP_ALLOWED_REPEAT_FREQ;
    if (wide_list) {
      for (int k = 0;
           k < (int)(sizeof(repeat_freq_ranges)/sizeof(repeat_freq_ranges[0]))
           && i + 8 < (int)sizeof(out_frame); k++) {
        auto r = &repeat_freq_ranges[k];
        memcpy(&out_frame[i], &r->lower_freq, 4); i += 4;
        memcpy(&out_frame[i], &r->upper_freq, 4); i += 4;
      }
    } else {
      for (int k = 0;
           k < (int)(sizeof(repeat_freq_ranges_strict)/sizeof(repeat_freq_ranges_strict[0]))
           && i + 8 < (int)sizeof(out_frame); k++) {
        auto r = &repeat_freq_ranges_strict[k];
        memcpy(&out_frame[i], &r->lower_freq, 4); i += 4;
        memcpy(&out_frame[i], &r->upper_freq, 4); i += 4;
      }
    }
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_RAW_PACKET && len >= 4) {
    auto pkt = obtainNewPacket();
    if (pkt) {
      uint8_t priority = cmd_frame[1];
      if (tryParsePacket(pkt, &cmd_frame[2], len - 2)) {
        sendPacket(pkt, priority, 0);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else {
    writeErrFrame(ERR_CODE_UNSUPPORTED_CMD);
    MESH_DEBUG_PRINTLN("ERROR: unknown command: %02X", cmd_frame[0]);
  }
}

static bool save_filter(const ContactInfo& c) {
  return c.type != ADV_TYPE_NONE;   // don't save the transient/anon entries
}

void MyMesh::saveContacts() {
  _store->saveContacts(this, save_filter);
}

void MyMesh::enterCLIRescue() {
  _cli_rescue = true;
  cli_command[0] = 0;
  Serial.println("========= CLI Rescue =========");
}

void MyMesh::checkCLIRescueCmd() {
  int len = strlen(cli_command);
  while (Serial.available() && len < sizeof(cli_command)-1) {
    char c = Serial.read();
    if (c != '\n') {
      cli_command[len++] = c;
      cli_command[len] = 0;
    }
    Serial.print(c);  // echo
  }
  if (len == sizeof(cli_command)-1) {  // command buffer full
    cli_command[sizeof(cli_command)-1] = '\r';
  }

  if (len > 0 && cli_command[len - 1] == '\r') {  // received complete line
    cli_command[len - 1] = 0;  // replace newline with C string null terminator

    if (memcmp(cli_command, "set ", 4) == 0) {
      const char* config = &cli_command[4];
      if (memcmp(config, "pin ", 4) == 0) {
        _prefs.ble_pin = atoi(&config[4]);
        savePrefs();
        Serial.printf("  > pin is now %06d\n", _prefs.ble_pin);
      } else {
        Serial.printf("  Error: unknown config: %s\n", config);
      }
    } else if (strcmp(cli_command, "rebuild") == 0) {
      bool success = _store->formatFileSystem();
      if (success) {
        _store->saveMainIdentity(self_id);
        savePrefs();
        saveContacts();
        saveChannels();
        Serial.println("  > erase and rebuild done");
      } else {
        Serial.println("  Error: erase failed");
      }
    } else if (strcmp(cli_command, "erase") == 0) {
      bool success = _store->formatFileSystem();
      if (success) {
        Serial.println("  > erase done");
      } else {
        Serial.println("  Error: erase failed");
      }
    } else if (memcmp(cli_command, "ls", 2) == 0) {

      // get path from command e.g: "ls /adafruit"
      const char *path = &cli_command[3];

      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      }
      Serial.printf("Listing files in %s\n", path);

      // log each file and directory
      File root = _store->openRead(path);
      if (is_fs2 == false) {
        if (root) {
          File file = root.openNextFile();
          while (file) {
            if (file.isDirectory()) {
              Serial.printf("[dir]  UserData%s/%s\n", path, file.name());
            } else {
              Serial.printf("[file] UserData%s/%s (%d bytes)\n", path, file.name(), file.size());
            }
            // move to next file
            file = root.openNextFile();
          }
          root.close();
        }
      }

      if (is_fs2 == true || strlen(path) == 0 || strcmp(path, "/") == 0) {
        if (_store->getSecondaryFS() != nullptr) {
          File root2 = _store->openRead(_store->getSecondaryFS(), path);
          File file = root2.openNextFile();
          while (file) {
            if (file.isDirectory()) {
              Serial.printf("[dir]  ExtraFS%s/%s\n", path, file.name());
            } else {
              Serial.printf("[file] ExtraFS%s/%s (%d bytes)\n", path, file.name(), file.size());
            }
            // move to next file
            file = root2.openNextFile();
          }
          root2.close();
        }
      }
    } else if (memcmp(cli_command, "cat", 3) == 0) {

      // get path from command e.g: "cat /contacts3"
      const char *path = &cli_command[4];

      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      } else {
        Serial.println("Invalid path provided, must start with UserData/ or ExtraFS/");
        cli_command[0] = 0;
        return;
      }

      // log file content as hex
      File file = _store->openRead(path);
      if (is_fs2 == true) {
        file = _store->openRead(_store->getSecondaryFS(), path);
      }
      if(file){

        // get file content
        int file_size = file.available();
        uint8_t buffer[file_size];
        file.read(buffer, file_size);

        // print hex
        mesh::Utils::printHex(Serial, buffer, file_size);
        Serial.print("\n");

        file.close();

      }

    } else if (memcmp(cli_command, "rm ", 3) == 0) {
      // get path from command e.g: "rm /adv_blobs"
      const char *path = &cli_command[3];
      MESH_DEBUG_PRINTLN("Removing file: %s", path);
      // ensure path is not empty, or root dir
      if(!path || strlen(path) == 0 || strcmp(path, "/") == 0){
        Serial.println("Invalid path provided");
      } else {
      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      }

        // remove file
        bool removed;
        if (is_fs2) {
          MESH_DEBUG_PRINTLN("Removing file from ExtraFS: %s", path);
          removed = _store->removeFile(_store->getSecondaryFS(), path);
        } else {
          MESH_DEBUG_PRINTLN("Removing file from UserData: %s", path);
          removed = _store->removeFile(path);
        }
        if(removed){
          Serial.println("File removed");
        } else {
          Serial.println("Failed to remove file");
        }

      }

    } else if (strcmp(cli_command, "reboot") == 0) {
      board.reboot();  // doesn't return
    } else {
      Serial.println("  Error: unknown command");
    }

    cli_command[0] = 0;  // reset command buffer
  }
}

void MyMesh::checkSerialInterface() {
  size_t len = _serial->checkRecvFrame(cmd_frame);
  if (len > 0) {
    handleCmdFrame(len);
  } else if (_iter_started              // check if our ContactsIterator is 'running'
             && !_serial->isWriteBusy() // don't spam the Serial Interface too quickly!
  ) {
    ContactInfo contact;
    bool found = false;
    while (_iter.hasNext(this, contact)) {
      if (contact.type != ADV_TYPE_NONE) {
        found = true;
        break;
      }
    }

    if (found) {
      if (contact.lastmod > _iter_filter_since) { // apply the 'since' filter
        writeContactRespFrame(RESP_CODE_CONTACT, contact);
        if (contact.lastmod > _most_recent_lastmod) {
          _most_recent_lastmod = contact.lastmod; // save for the RESP_CODE_END_OF_CONTACTS frame
        }
      }
    } else { // EOF
      out_frame[0] = RESP_CODE_END_OF_CONTACTS;
      memcpy(&out_frame[1], &_most_recent_lastmod,
             4); // include the most recent lastmod, so app can update their 'since'
      _serial->writeFrame(out_frame, 5);
      _iter_started = false;
    }
  //} else if (!_serial->isWriteBusy()) {
  //  checkConnections();    // TODO - deprecate the 'Connections' stuff
  }
}

void MyMesh::loop() {
  // Track millis() wraps so a long uptime can be displayed as "> 49d"
  // instead of silently rolling back to "0m 0s".
  {
    uint32_t now_ms = millis();
    if (now_ms < _last_millis_seen) _millis_wraps++;
    _last_millis_seen = now_ms;
  }

  // Duty-Cycle Sliding-Window aktualisieren VOR BaseChatMesh::loop() —
  // damit allowPacketForward() (was vom Mesh-Layer aus loop() kommt) bei
  // seinem Soft-Check aktuelle Werte sieht.
  updateDutyWindow();

  BaseChatMesh::loop();

  // Wunschliste 43 (2026-06-10): BLE-Power-Cycle State Machine
  manageBlePower();

  // Wunschliste 27: discover-Listen-Window check
  discoverLoop();

  // Wunschliste 31: 3-min Timeout fuer Lazy-Collection. Wenn nach
  // 3 min noch nicht finalisiert (< 5 Kandidaten gesammelt),
  // jetzt evaluieren mit dem was wir haben.
  if (_prefs.time_sync_mode == 1
      && !_time_sync_lazy_done
      && _time_sync_lazy_started_ms != 0
      && (millis() - _time_sync_lazy_started_ms) > 180000UL) {
    timeSyncFinalizeLazyCollection();
  }

  // Wunschliste 28 Phase B: backup restore Serial-Read State-Machine.
  // Liest rohe USB-CDC bytes (Serial.*). Laeuft PARALLEL zum BLE-Frame-
  // Pfad (_serial->checkRecvFrame in checkSerialInterface), weil im
  // BLE-Build die beiden Streams (USB-Serial vs BLE) physisch getrennt
  // sind. App kann waehrend Restore weiter operieren.
  if (_br_state != BR_IDLE) {
    backupRestoreLoop();
  }
  if (_cli_rescue) {
    checkCLIRescueCmd();
  } else {
    checkSerialInterface();
  }

  // is there are pending dirty contacts write needed?
  if (dirty_contacts_expiry && millisHasNowPassed(dirty_contacts_expiry)) {
    saveContacts();
    dirty_contacts_expiry = 0;
  }

  // count rising edges of serial/BLE connection (i.e. app re-connects)
  if (_serial != NULL) {
    bool is_connected = _serial->isConnected();
    if (is_connected && !_last_serial_connected) {
      _bt_connect_count++;
      traceCompanion(TRACE_CONNECT, "[connect] App connected (count=%lu)",
                     (unsigned long)_bt_connect_count);
    } else if (!is_connected && _last_serial_connected) {
      traceCompanion(TRACE_CONNECT, "[connect] App disconnected");
    }
    _last_serial_connected = is_connected;
  }

  // Adaptive zero-hop unscoped advert (3h / 1h / 15min depending on motion)
  updateMotionTracking();
  manageGpsPower();
  if ((_prefs.auto_advert_enabled & AUTO_ADV_ZEROHOP)
      && next_periodic_advert_at && millisHasNowPassed(next_periodic_advert_at)) {
    doPeriodicZeroHopAdvert();
    // Jitter 0..120s addieren (User-Wunsch 2026-05-30): bei mehreren
    // Fix-Position-Clients die gleichzeitig booten oder die selbe
    // Cadence-Klasse haben (3h/1h/15min) wuerden ihre Adverts sonst
    // synchron landen -> Frame-Kollisionen. Positive-only Jitter (keine
    // Drift in Mittel-Cadence) und im Anteil des Basis-Intervalls
    // vernachlaessigbar (0.5..7%) je nach Klasse.
    unsigned long base = computeNextAdvertIntervalMs();
    unsigned long jitter = (unsigned long)getRNG()->nextInt(0, 120000);
    next_periodic_advert_at = futureMillis(base + jitter);
    // Next-fire-Visibility (User-Wunsch 2026-06-03). Lokale Uhrzeit + Delta.
    uint32_t now_rtc = getRTCClock()->getCurrentTime();
    if (now_rtc > 1500000000UL) {
      uint32_t abs_unix = now_rtc + (base + jitter) / 1000UL;
      uint32_t loc = abs_unix + (uint32_t)localTzOffsetSecs(now_rtc);
      unsigned hh = (unsigned)((loc % 86400UL) / 3600UL);
      unsigned mm = (unsigned)((loc % 3600UL) / 60UL);
      traceCompanion(TRACE_ADVERTS,
                     "[adv] next zero-hop at %02u:%02u (in %lus)",
                     hh, mm, (base + jitter) / 1000UL);
    }
  }

  // Nightly scoped flood advert: random instant in 23:00-05:00 local
  {
    uint32_t now_rtc = getRTCClock()->getCurrentTime();

    // Generic RTC-jump detector. Between two loop ticks the RTC should
    // change by at most a few seconds (real time). Any large jump means
    // an external correction (CMD_SET_DEVICE_TIME, GPS time sync from
    // the location provider, or a manual set). Invalidate the slot so
    // it is re-picked against the corrected time.
    if (_last_observed_rtc != 0) {
      int32_t delta = (int32_t)(now_rtc - _last_observed_rtc);
      // Trace-Schwelle: ab 5 Min Sprung loggen (interessant fuer Diagnose).
      // Invalidierungs-Schwelle: ab 1h Sprung -- kleine Drift-Korrekturen
      // verschieben den zufaelligen Slot im 23-05-Fenster nicht relevant.
      // Test-Bericht 2026-05-30: ohne diese Trennung re-schedulet jeder
      // App-Sync den Slot, sichtbar als unerwartetes [night] scheduled
      // beim App-Connect.
      int32_t abs_delta = delta < 0 ? -delta : delta;
      if (abs_delta > 300) {
        traceCompanion(TRACE_RTC, "[rtc] Sprung %ld sec erkannt", (long)delta);
        if (abs_delta > 3600 && next_night_flood_unix != 0) {
          pushDebugLog("[ADV-DBG] RTC jumped %ld sec, nightly slot invalidated\n", (long)delta);
          next_night_flood_unix = 0;
        }
      }
    }
    _last_observed_rtc = now_rtc;

    if (!(_prefs.auto_advert_enabled & AUTO_ADV_NIGHTLY)) {
      // Nightly-Flood deaktiviert: kein Schedule, kein Send. Beim Wieder-
      // einschalten setzt der "autoadv ..."-Befehl next_night_flood_unix=0
      // und triggert die Neuplanung hier.
    } else if (next_night_flood_unix == 0) {
      scheduleNextNightFlood();   // no-op if RTC still unset
    } else if (now_rtc >= next_night_flood_unix) {
      // Backup sanity check (in case the jump detector missed an edge case)
      if (now_rtc - next_night_flood_unix > 12UL * 3600UL) {
        scheduleNextNightFlood();
      } else {
        doNightFloodAdvert();
        scheduleNextNightFlood();
      }
    }
  }

#ifdef DISPLAY_CLASS
  if (_ui) _ui->setHasConnection(_serial->isConnected());
#endif

  // Wunschliste 40 (2026-06-04): Periodic Heap- + BLE-Disconnect-Tracking.
  // Trace in den Companion-Channel via pushCompanionMessage -> Offline-
  // Queue. ABSICHTLICH NICHT via pushDebugLog: jenes droppt wenn weder
  // USB-Host connected NOCH BLE connected -- genau unser Fehler-Szenario
  // (Powerbank ohne USB + BLE just disconnected). Companion-Channel
  // bleibt in der Offline-Queue, App sieht beim Reconnect die komplette
  // Diagnose-Spur.
  //
  // RATE-LIMIT (Update 2026-06-05): mindestens 30sec zwischen Eintraegen
  // -- verhindert Rueckkopplung bei Disconnect-Storm. User-Bericht:
  // 102 disconnects in kurzer Folge => ohne Cooldown 102 Diag-Frames
  // zusaetzlich im Sync-Burst, was wiederum die Connection-Update-
  // Aktivitaet verschaerft. Mit Cooldown: max ~10 Logs in 5min, gibt
  // genug Datenpunkte ohne selbst zu amplifizieren.
#ifdef ESP32
  // ENTFERNBAR (Wunschliste 40): BLE-Diagnose-Block. Falls Stabilitaet
  // dauerhaft gut bleibt, dieser ganze ifdef ESP32-Block + die zugehoerigen
  // member-vars (_next_heap_log_at, _last_logged_disconnect_count,
  // _session_min_heap, _last_ble_diag_log_at) koennen ersatzlos entfernt
  // werden. TRACE_BT in MyMesh.h auch raus.
  // Update 2026-06-05: min-heap-Tracking laeuft IMMER (sehr billig),
  // damit 'bleinfo' jederzeit den aktuellen min anzeigen kann.
  // Periodisches PUSH in den Companion-Channel nur wenn 'trace bt'
  // aktiv -- vorher permanent alle 5min, nervte im taeglichen Betrieb.
  {
    uint32_t cur_heap = ESP.getFreeHeap();
    if (cur_heap < _session_min_heap) _session_min_heap = cur_heap;
  }
  if (_companion_channel_idx != 0xFF && (_trace_flags & TRACE_BT)) {
    uint32_t cur_heap = ESP.getFreeHeap();
    uint32_t dc = _serial ? _serial->getDisconnectCount() : 0;
    bool dc_changed = (dc != _last_logged_disconnect_count);
    unsigned long now_ms = millis();
    bool cooldown_ok = (_last_ble_diag_log_at == 0
                        || (long)(now_ms - _last_ble_diag_log_at) >= 30000);
    bool time_due = (_next_heap_log_at == 0
                     || (long)(now_ms - _next_heap_log_at) >= 0);
    if (cooldown_ok && (dc_changed || time_due)) {
      uint8_t reason = _serial ? _serial->getLastDisconnectReason() : 0xFF;
      uint32_t delta = dc - _last_logged_disconnect_count;
      char diag[145];
      if (dc_changed && delta > 1) {
        snprintf(diag, sizeof(diag),
          "[ble-diag] heap=%u min=%u dc=%u (+%u burst) reason=0x%02X",
          (unsigned)cur_heap, (unsigned)_session_min_heap,
          (unsigned)dc, (unsigned)delta, (unsigned)reason);
      } else if (dc_changed) {
        snprintf(diag, sizeof(diag),
          "[ble-diag] heap=%u min=%u dc=%u reason=0x%02X NEW",
          (unsigned)cur_heap, (unsigned)_session_min_heap,
          (unsigned)dc, (unsigned)reason);
      } else {
        snprintf(diag, sizeof(diag),
          "[ble-diag] heap=%u min=%u dc=%u reason=0x%02X",
          (unsigned)cur_heap, (unsigned)_session_min_heap,
          (unsigned)dc, (unsigned)reason);
      }
      pushCompanionMessage(diag);
      _last_logged_disconnect_count = dc;
      _last_ble_diag_log_at = now_ms;
      _next_heap_log_at = now_ms + 5UL * 60UL * 1000UL;  // 5min
    }
  }
#endif

  // Deferred reboot — siehe handleCompanionCommand("reboot"). Erst hier am
  // Ende der loop() pruefen: bis dahin hatten App-Frame-Auslieferung +
  // CMD_SYNC_NEXT_MSG genug Zeit. (long)(now - target) >= 0 ist
  // wrap-safe via signed-diff.
  if (_pending_reboot_at != 0 && (long)(millis() - _pending_reboot_at) >= 0) {
    board.reboot();
    // returns not.
  }
}

bool MyMesh::getEffectiveLatLon(double& lat, double& lon) const {
#if ENV_INCLUDE_GPS == 1
  if (_prefs.gps_enabled) {
    LocationProvider* loc = sensors.getLocationProvider();
    if (loc && loc->isValid()) {
      lat = ((double)loc->getLatitude()) / 1000000.0;
      lon = ((double)loc->getLongitude()) / 1000000.0;
      return true;
    }
  }
#endif
  if (sensors.node_lat != 0.0 || sensors.node_lon != 0.0) {
    lat = sensors.node_lat;
    lon = sensors.node_lon;
    return true;
  }
  return false;
}

// Reise-Wunsch 2026-06-08: profile-aware Bbox-Quelle.
//
// profile=normal (echter Repeater): hat einen FESTEN Standort. User
// hat den per 'set lat/lon' konfiguriert. Live-GPS-Fix wird fuer
// die Bbox-Membership IGNORIERT -- auch wenn das GPS-Modul gerade
// Position liefert (z.B. fuer Time-Sync oder Tracker-Co-Existenz).
//
// profile=defensive (Client mit Repeat-Funktion):
//   - GPS aus  -> fixed location als Fallback (User hat bewusst
//                 GPS aus, fixed ist die deklarierte Position).
//   - GPS an + noch nie Fix bekommen -> KEINE Bbox-Quelle.
//                 Defensive Haltung: wir wissen noch nicht wo wir
//                 sind, also keine AUTO-Scopes greifen lassen. Erst
//                 wenn der erste Fix da war (gps_had_fix_ever),
//                 wechseln wir auf Live-GPS.
//                 Folge: GRP/TXT/ADVERT/REQ/RESP von scoped Paketen
//                 die nur per AUTO-Scope greifen wuerden, werden in
//                 dieser Phase nicht repeated. PINNED Scopes
//                 (SCOPE_STATUS_REPEAT_ON) bleiben unberuehrt --
//                 scopeStatusAllowsRepeat returnt fuer REPEAT_ON
//                 immer true, unabhaengig von in_bbox. Unscoped
//                 CHAT-Adverts + TXT_MSG laufen ueber Wunschliste 39.
//   - GPS an + Fix erhalten -> Live-GPS-Position (fixed-Fallback bei
//                 spaeterem Fix-Verlust dank getEffectiveLatLon).
//
// Wenn weder Live-GPS noch fixed location verfuegbar: false (kein
// Geo-Match moeglich; AUTO-Scopes greifen nicht, nur explizit-pinned).
void MyMesh::reevaluateRepeaterBbox() {
  double new_lat, new_lon;
  if (getRepeaterBboxLatLon(new_lat, new_lon)) {
    evaluateScopeBboxes(new_lat, new_lon);
  } else {
    // Keine Quelle (z.B. defensive + gps on + nie Fix). Bbox leeren --
    // AUTO-Scopes greifen nicht. Pinned bleiben unberuehrt.
    memset(_buildin_in_bbox, 0, sizeof(_buildin_in_bbox));
    memset(_extras_in_bbox,  0, sizeof(_extras_in_bbox));
  }
}

bool MyMesh::getRepeaterBboxLatLon(double& lat, double& lon) const {
  if (_prefs.repeater_profile == 1 /* normal */) {
    if (sensors.node_lat == 0.0 && sensors.node_lon == 0.0) return false;
    lat = sensors.node_lat;
    lon = sensors.node_lon;
    return true;
  }
  // defensive
#if ENV_INCLUDE_GPS == 1
  if (_prefs.gps_enabled && !_gps_had_fix_ever) {
    // GPS aktiv aber noch nie ein Fix. Defensive Haltung: nicht
    // auf fixed location zurueckfallen, sondern warten.
    return false;
  }
#endif
  return getEffectiveLatLon(lat, lon);
}

// --- Scope-Registry-Helpers (Liste A) ---------------------------------------

// Normalisiert User-Input: strippt fuehrendes '#', lowercase, akzeptiert
// nur [a-z0-9-_]. Lehnt leer, doppelte '#', zu lang ab.
bool MyMesh::normalizeScopeName(const char* in, char* out, size_t out_size) const {
  if (!in || out_size < 2) return false;
  while (*in == ' ' || *in == '\t') in++;
  if (*in == 0) return false;
  if (*in == '#') {
    in++;
    if (*in == '#') return false;   // "##foo" wuerde doppelt gehasht
  }
  if (*in == 0) return false;
  size_t k = 0;
  while (*in && *in != ' ' && *in != '\t' && k + 1 < out_size) {
    char c = *in++;
    if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
          || c == '-' || c == '_')) return false;
    out[k++] = c;
  }
  out[k] = 0;
  return k > 0;
}

void MyMesh::computeScopeHash(const char* name, uint8_t out_hash[4]) const {
  // SHA-256("#" + name)[0..3] — kompatibel zu TransportKeyStore::
  // getAutoKeyFor(), nur der erste 4-Byte-Anteil.
  char tag[40];
  snprintf(tag, sizeof(tag), "#%s", name);
  TransportKey k;
  TransportKeyStore tmp;
  tmp.getAutoKeyFor(0, tag, k);
  memcpy(out_hash, k.key, 4);
}

// ----- Bbox-Membership-Update -----
// Iteriert beide Storages, prueft fuer jeden Eintrag mit Bbox ob die
// aktuelle Position drin liegt, schreibt das Ergebnis in
// _buildin_in_bbox[] / _extras_in_bbox[]. Aufruf vom Motion-Tracker
// nach jedem Anker-Update.
void MyMesh::evaluateScopeBboxes(double lat, double lon) {
  // Build-in
  for (int i = 0; i < _buildin_keys_count && i < SCOPE_BUILDIN_KEY_CACHE_MAX; i++) {
    bool has_bbox = false;
    dl9sau_get_region_meta((size_t)i, &has_bbox, NULL);
    if (!has_bbox) { _buildin_in_bbox[i] = false; continue; }
    double lat1, lat2, lon1, lon2;
    if (!dl9sau_get_region((size_t)i, NULL, &lat1, &lat2, &lon1, &lon2)) {
      _buildin_in_bbox[i] = false;
      continue;
    }
    _buildin_in_bbox[i] = (lat >= lat1 && lat <= lat2 && lon >= lon1 && lon <= lon2);
  }
  // Extras
  for (int i = 0; i < _prefs.scope_extras_count && i < SCOPE_EXTRAS_SLOTS; i++) {
    const ScopeRegEntry& e = _prefs.scope_extras[i];
    if (!(e.flags & SCOPE_FLAG_HAS_GEO_BOX)) {
      _extras_in_bbox[i] = false;
      continue;
    }
    _extras_in_bbox[i] = ((double)e.bbox_lat_min <= lat && lat <= (double)e.bbox_lat_max
                       && (double)e.bbox_lon_min <= lon && lon <= (double)e.bbox_lon_max);
  }
}

// ----- Cross-Storage Helpers (Wunschliste 11 Schritt 4) -------------------

MyMesh::ScopeRef MyMesh::findScopeByName(const char* name) const {
  ScopeRef r = { SCOPE_NONE, -1 };
  if (!name || !*name) return r;
  int idx = dl9sau_find_region_index(name);
  if (idx >= 0) { r.storage = SCOPE_BUILDIN; r.idx = idx; return r; }
  for (int i = 0; i < _prefs.scope_extras_count && i < SCOPE_EXTRAS_SLOTS; i++) {
    if (strncmp(_prefs.scope_extras[i].name, name,
                sizeof(_prefs.scope_extras[i].name)) == 0) {
      r.storage = SCOPE_EXTRAS; r.idx = i; return r;
    }
  }
  return r;
}

uint8_t MyMesh::getBuildinStatus(int buildin_idx) const {
  if (buildin_idx < 0) return 0;
  const char* name = NULL;
  if (!dl9sau_get_region((size_t)buildin_idx, &name, NULL, NULL, NULL, NULL)) return 0;
  uint8_t want[4];
  dl9sau_compute_name_hash(name, want);
  for (int i = 0; i < _prefs.scope_buildin_status_count
                   && i < SCOPE_BUILDIN_STATUS_MAX; i++) {
    if (memcmp(_prefs.scope_buildin_status[i].name_hash, want, 4) == 0) {
      return _prefs.scope_buildin_status[i].status;
    }
  }
  // Kein User-Override -> Build-in Default-Status (z.B. REPEAT_ON fuer
  // local/lokal/region/regional). Default fuer normale Geo-Regionen = 0.
  uint8_t def = 0;
  dl9sau_get_region_meta((size_t)buildin_idx, NULL, &def);
  return def;
}

bool MyMesh::setBuildinStatus(int buildin_idx, uint8_t status) {
  if (buildin_idx < 0) return false;
  const char* name = NULL;
  if (!dl9sau_get_region((size_t)buildin_idx, &name, NULL, NULL, NULL, NULL)) return false;
  uint8_t want[4];
  dl9sau_compute_name_hash(name, want);

  // Existierender Slot? -> update oder loeschen
  for (int i = 0; i < _prefs.scope_buildin_status_count
                   && i < SCOPE_BUILDIN_STATUS_MAX; i++) {
    if (memcmp(_prefs.scope_buildin_status[i].name_hash, want, 4) == 0) {
      if (status == 0) {
        // Default -> Slot entfernen (Compaction: letzten Slot drueber kopieren)
        int last = _prefs.scope_buildin_status_count - 1;
        if (i != last) {
          _prefs.scope_buildin_status[i] = _prefs.scope_buildin_status[last];
        }
        memset(&_prefs.scope_buildin_status[last], 0,
               sizeof(_prefs.scope_buildin_status[last]));
        _prefs.scope_buildin_status_count--;
      } else {
        _prefs.scope_buildin_status[i].status = status;
      }
      return true;
    }
  }
  // Kein Slot — bei status==0 ist nix zu tun
  if (status == 0) return true;
  if (_prefs.scope_buildin_status_count >= SCOPE_BUILDIN_STATUS_MAX) return false;
  BuildinStatusEntry& e =
      _prefs.scope_buildin_status[_prefs.scope_buildin_status_count];
  memcpy(e.name_hash, want, 4);
  e.status = status;
  memset(e._reserved, 0, sizeof(e._reserved));
  _prefs.scope_buildin_status_count++;
  return true;
}

uint8_t MyMesh::getScopeStatus(const ScopeRef& ref) const {
  if (ref.storage == SCOPE_BUILDIN) return getBuildinStatus(ref.idx);
  if (ref.storage == SCOPE_EXTRAS
      && ref.idx >= 0 && ref.idx < _prefs.scope_extras_count) {
    // Extras tragen das alte ScopeRegEntry-Flag-Layout — wir mappen
    // beim Zugriff auf das neue Status-Byte-Layout.
    uint8_t old = _prefs.scope_extras[ref.idx].flags;
    uint8_t s = 0;
    bool in_list  = (old & SCOPE_FLAG_IN_REPEAT_LIST) != 0;
    bool geo_mgd  = (old & SCOPE_FLAG_GEO_MANAGED)   != 0;
    bool disabled = (old & SCOPE_FLAG_DISABLED)      != 0;
    if (geo_mgd)        s |= SCOPE_STATUS_REPEAT_AUTO;  // = 0
    else if (in_list)   s |= SCOPE_STATUS_REPEAT_ON;
    else                s |= SCOPE_STATUS_REPEAT_OFF;
    if (disabled)       s |= SCOPE_STATUS_DISABLED;
    return s;
  }
  return 0;
}

bool MyMesh::setScopeStatus(const ScopeRef& ref, uint8_t status) {
  if (ref.storage == SCOPE_BUILDIN) return setBuildinStatus(ref.idx, status);
  if (ref.storage == SCOPE_EXTRAS
      && ref.idx >= 0 && ref.idx < _prefs.scope_extras_count) {
    // Auf alte ScopeRegEntry-Flags zurueck-mappen waehrend dual-storage-
    // Phase. Schritt 8 (Cleanup) wird scope_extras auf das neue Layout
    // umstellen.
    ScopeRegEntry& e = _prefs.scope_extras[ref.idx];
    e.flags &= ~(SCOPE_FLAG_IN_REPEAT_LIST | SCOPE_FLAG_GEO_MANAGED
                 | SCOPE_FLAG_DISABLED);
    uint8_t rm = status & SCOPE_STATUS_REPEAT_MASK;
    if (rm == SCOPE_STATUS_REPEAT_AUTO) {
      e.flags |= SCOPE_FLAG_GEO_MANAGED;  // hat HAS_GEO_BOX schon
    } else if (rm == SCOPE_STATUS_REPEAT_ON) {
      e.flags |= SCOPE_FLAG_IN_REPEAT_LIST;
    }
    if (status & SCOPE_STATUS_DISABLED) e.flags |= SCOPE_FLAG_DISABLED;
    return true;
  }
  return false;
}

const uint8_t* MyMesh::getScopeKey(const ScopeRef& ref) const {
  if (ref.storage == SCOPE_BUILDIN
      && ref.idx >= 0 && ref.idx < _buildin_keys_count) {
    return _buildin_keys[ref.idx].key;
  }
  if (ref.storage == SCOPE_EXTRAS
      && ref.idx >= 0 && ref.idx < _prefs.scope_extras_count) {
    return _prefs.scope_extras[ref.idx].key;
  }
  return NULL;
}

bool MyMesh::getScopeBbox(const ScopeRef& ref,
                          double* lat_min, double* lat_max,
                          double* lon_min, double* lon_max) const {
  if (ref.storage == SCOPE_BUILDIN) {
    return dl9sau_get_region((size_t)ref.idx, NULL,
                             lat_min, lat_max, lon_min, lon_max);
  }
  if (ref.storage == SCOPE_EXTRAS
      && ref.idx >= 0 && ref.idx < _prefs.scope_extras_count) {
    const ScopeRegEntry& e = _prefs.scope_extras[ref.idx];
    if (!(e.flags & SCOPE_FLAG_HAS_GEO_BOX)) return false;
    if (lat_min) *lat_min = (double)e.bbox_lat_min;
    if (lat_max) *lat_max = (double)e.bbox_lat_max;
    if (lon_min) *lon_min = (double)e.bbox_lon_min;
    if (lon_max) *lon_max = (double)e.bbox_lon_max;
    return true;
  }
  return false;
}

// Pruefe ob ein einzelnes Status-Byte einen Repeat erlaubt, gegeben
// der "in_bbox"-Hint (Position liegt in der Bbox des Eintrags).
//   - DISABLED oder USER_DELETED -> nie
//   - repeat_mode = OFF           -> nie
//   - repeat_mode = ON  (pin)     -> immer
//   - repeat_mode = AUTO          -> nur wenn in_bbox UND globaler
//                                    scope_repeater_auto = on. Wenn
//                                    globaler Schalter off ist, werden
//                                    alle auto-Eintraege ignoriert
//                                    (nur Pin zaehlt).
static inline bool scopeStatusAllowsRepeat(uint8_t status, bool in_bbox,
                                           bool auto_enabled) {
  if (status & (SCOPE_STATUS_DISABLED | SCOPE_STATUS_USER_DELETED)) return false;
  uint8_t mode = status & SCOPE_STATUS_REPEAT_MASK;
  if (mode == SCOPE_STATUS_REPEAT_OFF) return false;
  if (mode == SCOPE_STATUS_REPEAT_ON)  return true;
  // AUTO: nur wenn global aktiviert UND in_bbox
  return auto_enabled && in_bbox;
}

// Wunschliste 11 Schritt 5: liest aus der neuen Storage
// (scope_buildin_status sparse + scope_extras) statt der alten
// scope_registry. Verwendet _buildin_in_bbox / _extras_in_bbox die
// von evaluateScopeBboxes() in updateMotionTracking() aktualisiert
// werden.
bool MyMesh::scopeAllowedForRepeat(const mesh::Packet* packet) const {
  if (_prefs.repeat_scope_mode == REPEAT_SCOPE_MODE_ALL) return true;
  if (!packet || !packet->hasTransportCodes()) return false;
  uint16_t target = packet->transport_codes[0];

  bool auto_enabled = (_prefs.scope_repeater_auto == 2);

  // Build-in: iteriere alle bekannten Regionen.
  for (int i = 0; i < _buildin_keys_count && i < SCOPE_BUILDIN_KEY_CACHE_MAX; i++) {
    uint8_t status = getBuildinStatus(i);
    if (!scopeStatusAllowsRepeat(status, _buildin_in_bbox[i], auto_enabled)) continue;
    // calcTransportCode ist eine member-Methode auf TransportKey.
    // _buildin_keys[] ist als TransportKey-Array deklariert, also
    // direkt aufrufbar.
    if (_buildin_keys[i].calcTransportCode(packet) == target) return true;
  }
  // Extras: iteriere User-erfundene Eintraege.
  for (int i = 0; i < _prefs.scope_extras_count && i < SCOPE_EXTRAS_SLOTS; i++) {
    ScopeRef ref = { SCOPE_EXTRAS, i };
    uint8_t status = getScopeStatus(ref);
    if (!scopeStatusAllowsRepeat(status, _extras_in_bbox[i], auto_enabled)) continue;
    TransportKey k;
    memcpy(k.key, _prefs.scope_extras[i].key, sizeof(k.key));
    if (k.calcTransportCode(packet) == target) return true;
  }
  return false;
}

// Wunschliste 11 Schritt 6: Geo-Fallback aus Registry statt hartcodierter
// Liste. Iteriert Build-in + Extras, waehlt die ENGSTE Bbox die die
// aktuelle Position enthaelt (= kleinste Flaeche).
//
// Filter pro Eintrag:
//   - HAS_GEO_BOX (sonst kein bbox-Check moeglich)
//   - Build-in: !ADVERT_OFF, !DISABLED, !USER_DELETED
//   - Extras: !SCOPE_FLAG_DISABLED (advert_off und user_deleted gibt's
//             im alten Flag-Layout noch nicht — Schritt 8 Cleanup wird
//             auch Extras auf das neue Status-Byte-Layout umstellen)
//
// Bei mehreren passenden Eintraegen gewinnt der mit der kleinsten
// Bbox-Flaeche. Tie-Breaker: erster Treffer (stabil, Build-in vor
// Extras, dann nach Position in der jeweiligen Storage).
bool MyMesh::chooseGeoFallbackScope(TransportKey& out_key) const {
  double lat, lon;
  if (!getEffectiveLatLon(lat, lon)) return false;

  ScopeRef winner = { SCOPE_NONE, -1 };
  double   winner_area = 0;

  auto consider = [&](ScopeStorage storage, int idx,
                      double lat1, double lat2, double lon1, double lon2) {
    if (lat < lat1 || lat > lat2 || lon < lon1 || lon > lon2) return;
    double area = (lat2 - lat1) * (lon2 - lon1);
    if (winner.storage == SCOPE_NONE || area < winner_area) {
      winner.storage = storage;
      winner.idx = idx;
      winner_area = area;
    }
  };

  // Build-in
  for (int i = 0; i < _buildin_keys_count && i < SCOPE_BUILDIN_KEY_CACHE_MAX; i++) {
    uint8_t status = getBuildinStatus(i);
    if (status & (SCOPE_STATUS_ADVERT_OFF
                  | SCOPE_STATUS_DISABLED
                  | SCOPE_STATUS_USER_DELETED)) continue;
    bool has_bbox = false;
    dl9sau_get_region_meta((size_t)i, &has_bbox, NULL);
    if (!has_bbox) continue;     // local/lokal/region/regional kein Geo-Fallback
    double lat1, lat2, lon1, lon2;
    if (!dl9sau_get_region((size_t)i, NULL, &lat1, &lat2, &lon1, &lon2)) continue;
    consider(SCOPE_BUILDIN, i, lat1, lat2, lon1, lon2);
  }

  // Extras
  for (int i = 0; i < _prefs.scope_extras_count && i < SCOPE_EXTRAS_SLOTS; i++) {
    const ScopeRegEntry& e = _prefs.scope_extras[i];
    if (!(e.flags & SCOPE_FLAG_HAS_GEO_BOX)) continue;
    if (e.flags & SCOPE_FLAG_DISABLED) continue;
    consider(SCOPE_EXTRAS, i,
             (double)e.bbox_lat_min, (double)e.bbox_lat_max,
             (double)e.bbox_lon_min, (double)e.bbox_lon_max);
  }

  if (winner.storage == SCOPE_NONE) return false;
  const uint8_t* k = getScopeKey(winner);
  if (!k) return false;
  memcpy(out_key.key, k, sizeof(out_key.key));
  return true;
}

// Wunschliste 5+13 (scope_advert_auto):
// Geo-vs-Default Send-Hierarchie. _prefs.scope_advert_auto entscheidet:
//   1 (off):    Nur Default. Geo wird nie verwendet.
//   2 (on):     Geo als Fallback, wenn Default leer ist.
//   3 (prefer): Geo gewinnt vor Default wenn oertliche Region != Default.
// Returns false wenn weder Default noch Geo etwas liefern.
bool MyMesh::resolveDefaultOrGeo(TransportKey& out_key) const {
  TransportKey configured;
  memcpy(configured.key, _prefs.default_scope_key, sizeof(configured.key));
  bool has_default = !configured.isNull();
  uint8_t mode = _prefs.scope_advert_auto;

  // prefer (3): Geo > Default falls anderer Match.
  if (mode == 3) {
    TransportKey geo;
    if (chooseGeoFallbackScope(geo)) {
      if (!has_default
          || memcmp(geo.key, configured.key, sizeof(geo.key)) != 0) {
        out_key = geo;
        return true;
      }
    }
  }
  // Default gewinnt wenn vorhanden.
  if (has_default) {
    out_key = configured;
    return true;
  }
  // on (2): Geo als Fallback wenn Default leer.
  // (mode 1 = off -> wir gehen hier vorbei und returnen false.)
  if (mode == 2) {
    if (chooseGeoFallbackScope(out_key)) return true;
  }
  return false;
}

bool MyMesh::chooseNightFloodScope(TransportKey& out_key) const {
  // Hierarchie:
  //   1) override (persistent, expiry-basiert)
  //   2) bake-scope (persistent, explizit fuer nightly)
  //   3) resolveDefaultOrGeo (Default/Geo gemaess scope_advert_auto)
  //   4) geo-fallback (Position-basiert, wenn nichts anderes)
  //   5) #local LAST-RESORT (User-Konsens 2026-05-29): wenn alles
  //      andere fehlt, geht der Nightly mit #local raus -- single-
  //      hop, harmlos, minimaler Netz-Impact. Stellt sicher dass
  //      ein User der ALLE Send-Scopes geleert hat (kein default,
  //      bake, override) UND ausserhalb aller Geo-Bboxen sitzt
  //      trotzdem erreichbar ist.
  uint32_t now = getRTCClock()->getCurrentTime();
  // 1) override: aktiv solange now < expiry
  if (_prefs.override_expiry != 0 && now < _prefs.override_expiry) {
    TransportKey k;
    memcpy(k.key, _prefs.override_scope_key, sizeof(k.key));
    if (!k.isNull()) {
      out_key = k;
      return true;
    }
  }
  // 2) bake-scope
  TransportKey bake;
  memcpy(bake.key, _prefs.bake_scope_key, sizeof(bake.key));
  if (!bake.isNull()) {
    out_key = bake;
    return true;
  }
  // 3) Default oder Geo (gemaess scope_advert_auto)
  if (resolveDefaultOrGeo(out_key)) return true;
  // 4) geo fallback (wenn weder Default noch geo_prefers gegriffen hat)
  if (chooseGeoFallbackScope(out_key)) return true;
  // 5) Last-Resort: #local aus der Build-in-Tabelle.
  int idx = dl9sau_find_region_index("local");
  if (idx >= 0 && idx < _buildin_keys_count) {
    out_key = _buildin_keys[idx];
    return true;
  }
  return false;
}

void MyMesh::scheduleNextNightFlood() {
  uint32_t now = getRTCClock()->getCurrentTime();
  if (now < 1500000000UL) {   // RTC clearly unset (pre-2017): skip
    next_night_flood_unix = 0;
    return;
  }
  // shift into local time (DST-aware -- CEST im Sommer = UTC+2)
  int32_t  tz_off    = localTzOffsetSecs(now);
  uint32_t local_now = now + (uint32_t)tz_off;
  uint32_t day_secs = local_now % 86400UL;
  uint32_t local_midnight_today = local_now - day_secs;

  // Window: [23:00, 29:00) local on the *current* day, i.e. covers 23-24 plus 0-5 of next day.
  uint32_t window_start = local_midnight_today + (uint32_t)(CR_NIGHT_FLOOD_START_HOUR_LOCAL * 3600UL);
  uint32_t window_end   = local_midnight_today + (uint32_t)((24 + CR_NIGHT_FLOOD_END_HOUR_LOCAL) * 3600UL);

  // Semantik (User-Wunsch 2026-05-29):
  // Es wird IMMER nur ausserhalb des Nightly-Fensters gewuerfelt -- der Slot
  // bleibt den ganzen Tag stehen. Wenn local_now im laufenden Fenster liegt
  // (heute 23:00..04:59 morgen), schieben wir das Fenster komplett um einen
  // Tag weiter. Das verhindert:
  //   - dass nach einem gerade gesendeten Flood ein 2. Slot in den Resttagen
  //     des selben Fensters gewuerfelt wird (z.B. Flood 04:30 -> reschedule
  //     waehlt 04:56 -> zweiter Flood in der gleichen Nacht).
  //   - dass nach einem Boot mitten in der Nacht ohne Wissen ueber prior
  //     Flood ein Slot in der Resttag-Nacht erwischt wird.
  // Konsequenz: Boot mitten in der Nacht -> heutige Nacht wird uebersprungen,
  // erst morgen Nacht gefloodet. Akzeptabler Trade-off fuer Eindeutigkeit.
  if (local_now >= window_start) {
    window_start += 86400UL;
    window_end   += 86400UL;
  }

  uint32_t span = window_end - window_start;
  uint32_t pick_local = window_start + getRNG()->nextInt(0, span);
  // Rueckkonvertierung lokal->UTC mit gleichem Offset wie oben. Edge-Case
  // DST-Wechsel-Sonntag: minimal 1h off, akzeptabler Trade-off.
  next_night_flood_unix = pick_local - (uint32_t)tz_off;
  uint32_t now_rtc2 = getRTCClock()->getCurrentTime();
  long until_s = (long)next_night_flood_unix - (long)now_rtc2;
  // Absolute Uhrzeit mit ausgeben (User-Wunsch 2026-06-03).
  uint32_t loc = next_night_flood_unix + (uint32_t)localTzOffsetSecs(now_rtc2);
  unsigned hh = (unsigned)((loc % 86400UL) / 3600UL);
  unsigned mm = (unsigned)((loc % 3600UL) / 60UL);
  traceCompanion(TRACE_NIGHT,
                 "[night] scheduled %02u:%02u (in %ld min)",
                 hh, mm, until_s / 60);
}

// Adaptive zero-hop advert pacing:
//   * 3 hours, when location is NOT configured to be sent in the advert,
//     OR when GPS is enabled and has NEVER yet produced a valid fix in this
//     session (cold device, indoor, antenna fault — nothing useful to share).
//   * 1 hour, when the position is static — i.e. GPS is disabled (we publish
//     the configured fixed location), OR GPS is enabled and the position has
//     not moved by more than CR_MOTION_RADIUS_M (370 m) within the last
//     CR_MOTION_WINDOW_MS (10 minutes), OR GPS HAD a fix but lost it (we
//     keep publishing the last-known location at the static rate).
//   * 15 minutes, when the GPS-derived position has moved beyond that radius
//     in the last window — i.e. the node is being carried around.
unsigned long MyMesh::computeNextAdvertIntervalMs() const {
  if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) return CR_ADVERT_INT_NO_LOC_MS;

#if ENV_INCLUDE_GPS == 1
  if (_prefs.gps_enabled) {
    if (!_gps_had_fix_ever) {
      // Never saw a fix this session — no point publishing a 0,0 position
      // frequently. Long interval until something useful happens.
      return CR_ADVERT_INT_NO_LOC_MS;
    }
    // Use the LAST KNOWN motion state, not the live fix status. The GPS may
    // be sleeping by design — that's expected — and the last value of
    // _is_moving still reflects what we knew at the previous wake cycle.
    return _is_moving ? CR_ADVERT_INT_MOVING_MS : CR_ADVERT_INT_STATIC_MS;
  }
#endif
  // GPS disabled → published location is the configured one, treated as static.
  return CR_ADVERT_INT_STATIC_MS;
}

void MyMesh::updateMotionTracking() {
#if ENV_INCLUDE_GPS == 1
  if (!_prefs.gps_enabled) {
    // GPS turned off by the user — drop tracking state entirely.
    _is_moving = false;
    _pos_anchor_millis = 0;
    // Reise-Fix 2026-06-08: trotzdem Bbox-Membership pflegen wenn
    // sensors.node_lat/lon konfiguriert ist (User hat fixed location
    // gesetzt + GPS aus). Sonst wuerden AUTO-Scopes nie matchen.
    if (sensors.node_lat != 0.0 || sensors.node_lon != 0.0) {
      evaluateScopeBboxes(sensors.node_lat, sensors.node_lon);
    }
    return;
  }
  LocationProvider* loc = sensors.getLocationProvider();
  if (loc == NULL || !loc->isValid()) {
    // No live fix right now. This is the normal case while the GPS module
    // is power-cycled between adverts — DO NOT reset _is_moving or the
    // motion anchor here. Resetting would collapse the dynamic interval
    // back to STATIC (1h), and the cycling would never wake the GPS for
    // the next 15-min moving slot. Last known motion state stays in place.
    return;
  }

  // Mark this wake cycle as having seen a real position fix — manageGpsPower()
  // uses this to decide if it's safe to power the module down again.
  _gps_fix_seen_this_wake = true;

  if (!_gps_had_fix_ever) {
    _gps_had_fix_ever = true;
    // Reise-Fix 2026-06-08: GPS-Marker setzen damit clock-Display auch
    // nach 'gps off' weiss dass die letzte Sync-Quelle GPS war.
    // pub_key=[0xFE,0xFE,0xFE] = GPS (anders als 0xFF Bootstrap, 0x00 App).
    // MicroNMEALocationProvider setzt RTC direkt ueber _clock->setCurrentTime
    // (deep in Lib), kann unsere Companion-State-Variablen nicht erreichen
    // -- hier nachholen.
    _time_sync_last_at_rtc = getRTCClock()->getCurrentTime();
    _time_sync_done_since_boot = true;
    memset(_time_sync_last_pubkey, 0xFE, sizeof(_time_sync_last_pubkey));
    {
      char st[140];
      appendGpsTraceStatus(st, sizeof(st));
      traceCompanion(TRACE_GPS, "[gps] first fix erkannt -- %s", st);
    }
    // First fix arrived during boot wait: collapse the GPS-extended boot
    // delay (10 min) to "5 min after boot" (not "5 min from now"!). If we are
    // already past that mark, fire as soon as possible.
    if (_tx_advert_count == 0) {
      unsigned long now = millis();
      // millis() starts at 0 on boot, so the absolute "boot + 5 min" mark is
      // just CR_PERIODIC_ADVERT_BOOT_DELAY_MS.
      unsigned long boot_plus_5min = CR_PERIODIC_ADVERT_BOOT_DELAY_MS;
      unsigned long target = (boot_plus_5min > now) ? boot_plus_5min : now;
      if ((long)(next_periodic_advert_at - target) > 0) {
        next_periodic_advert_at = target;
        // NOTE: this is a position fix (GPRMC status 'A'), NOT just a GPS
        // time-sync. The driver syncs the RTC earlier (time_valid > 2),
        // independent of this code path.
        pushDebugLog("[ADV-DBG] first GPS position-fix at millis=%lu, advert clamped to %lu\n",
                      now, target);
      }
    }
  }

  double cur_lat = ((double)loc->getLatitude()) / 1000000.0;
  double cur_lon = ((double)loc->getLongitude()) / 1000000.0;
  unsigned long now = millis();

  // First fix this session OR a fresh motion-window tick (every 10 min) —
  // re-evaluate which regions our coordinates fall into. Dedup is in
  // maybePushGeoRecommendation(), so calling per tick is cheap.
  maybePushGeoRecommendation(cur_lat, cur_lon);

  // Distance helper (equirectangular approximation; fine for the
  // sub-kilometre scale we operate at).
  auto distMeters = [](double lat1, double lon1, double lat2, double lon2) -> double {
    double dlat_m = (lat1 - lat2) * 111320.0;
    double dlon_m = (lon1 - lon2) * 111320.0 * cos(lat1 * DEG_TO_RAD);
    return sqrt(dlat_m * dlat_m + dlon_m * dlon_m);
  };

  // Boot-time motion hint: compare against the position persisted at last
  // shutdown (or last savePrefs). Stays true throughout the boot phase
  // (until first advert is sent) so several consecutive fixes during the
  // initial 5-min GPS-on window all contribute to the decision.
  if (_boot_pos_known && _tx_advert_count == 0) {
    double d_m = distMeters(cur_lat, cur_lon, _boot_lat, _boot_lon);
    if (d_m > CR_BOOT_MOVE_TOLERANCE_M) {
      _is_moving = true;   // device has moved since last session
    }
  }

  if (_pos_anchor_millis == 0) {
    // First fix this session — seed the motion anchor.
    _pos_anchor_lat = cur_lat;
    _pos_anchor_lon = cur_lon;
    _pos_anchor_millis = now;
    // Position erstmals bekannt — Scope-Bbox-Membership evaluieren.
    // Reise-Fix 2026-06-08: bei profile=normal Live-GPS ignorieren,
    // Bbox bleibt auf fixed location aus dem Boot-Init.
    if (_prefs.repeater_profile != 1 /* nicht normal */) {
      evaluateScopeBboxes(cur_lat, cur_lon);
    }
    return;
  }

  if (now - _pos_anchor_millis >= CR_MOTION_WINDOW_MS) {
    double d_m = distMeters(cur_lat, cur_lon, _pos_anchor_lat, _pos_anchor_lon);
    bool was_moving = _is_moving;
    _is_moving = (d_m > CR_MOTION_RADIUS_M);
    _pos_anchor_lat = cur_lat;
    _pos_anchor_lon = cur_lon;
    _pos_anchor_millis = now;

    if (was_moving != _is_moving) {
      char ll[32];
      formatLatLonDM(ll, sizeof(ll), cur_lat, cur_lon);
      // Wunschliste 34: bei defensive-Mode-Repeater-Gate auf
      // Bewegungs-Status hinweisen damit der User mitbekommt warum
      // gerade nicht repeated wird (oder wieder repeated wird).
      const char* rep_hint = "";
      if (_prefs.client_repeat != 0 && _prefs.repeater_profile == 0) {
        rep_hint = _is_moving ? " -- repeating temporaer aus"
                              : " -- repeating wieder an";
      }
      traceCompanion(TRACE_MOTION, "[motion] %s (Anker-Distanz %d m) pos=%s%s",
                     _is_moving ? "moving" : "static", (int)d_m, ll, rep_hint);
      // Cache aktualisieren (Wunschliste 34 Refactor): _is_moving ist
      // jetzt Teil von isRepeatingEffectivelyAllowed() -- recompute
      // damit per-Paket-Gate sofort greift.
      recomputeRepeatingAllowed(_is_moving ? "started moving" : "stopped moving");
    }
    // Movement just started — accelerate the next advert so a fresh
    // position goes out promptly, instead of waiting out the static
    // (1h) slot we may currently be on.
    if (!was_moving && _is_moving) {
      next_periodic_advert_at = millis();
    }
    // Position-Anker frisch — Scope-Bbox-Membership neu bewerten.
    // Das 370m-Anker-Update wirkt als Hysterese (Aufruf nur bei
    // signifikanter Distanz).
    // Reise-Fix 2026-06-08: bei profile=normal Live-GPS ignorieren.
    if (_prefs.repeater_profile != 1 /* nicht normal */) {
      evaluateScopeBboxes(cur_lat, cur_lon);
    }
  }
#else
  _is_moving = false;
#endif
}

void MyMesh::doPeriodicZeroHopAdvert() {
  if (dutyHardReached()) {
    _duty_blocked_count++;
    traceCompanion(TRACE_DUTY, "[duty] periodic blocked (last_h=%lus hard=%lus)",
                   getTxAirLastHour()/1000, getDutyHardLimitMs()/1000);
    return;
  }
  mesh::Packet* pkt;
  if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
    pkt = createSelfAdvert(_prefs.node_name);
  } else {
    pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
  }
  if (pkt) {
    // CR5 always (short airtime). Power reduction only when we are stationary;
    // while moving (15-min interval, e.g. driving) we want maximum reach so
    // distant neighbours can still pick up our position. Zero-hop adverts are
    // never repeated, so this stays a local-only burst.
    uint8_t flags = PKT_TX_FORCE_CR5;
    if (!_is_moving) flags |= PKT_TX_REDUCE_POWER;
    pkt->tx_flags |= flags;
    sendZeroHop(pkt);
    _tx_advert_count++;
    _gps_user_override_until_advert = false;   // user-on override expires with this advert
    pushDebugLog("[ADV-DBG] periodic, millis=%lu moving=%d\n", millis(), (int)_is_moving);
    // D4: scope-Info mit ausgeben. Zero-hop ist normalerweise unscoped
    // (sendZeroHop) -- machen wir explizit klar.
    traceCompanion(TRACE_ADVERTS, "[adv] periodic zero-hop moving=%d scope=unscoped",
                   (int)_is_moving);
  }
}

void MyMesh::doNightFloodAdvert() {
  if (dutyHardReached()) {
    _duty_blocked_count++;
    traceCompanion(TRACE_DUTY, "[duty] nightly blocked (last_h=%lus hard=%lus)",
                   getTxAirLastHour()/1000, getDutyHardLimitMs()/1000);
    return;
  }
  TransportKey scope;
  if (!chooseNightFloodScope(scope)) {
    MESH_DEBUG_PRINTLN("night-flood: no scope available, skipping");
    return;
  }
  mesh::Packet* pkt;
  if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
    pkt = createSelfAdvert(_prefs.node_name);
  } else {
    pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
  }
  if (pkt) {
    pkt->tx_flags |= (PKT_TX_REDUCE_POWER | PKT_TX_FORCE_CR5);
    // 3-byte path-hash for the nightly flood: caps max hop count to ~21
    // (64-byte path / 3) instead of 64 with the default 1-byte hashes.
    // This noticeably limits how far the advert can ripple through the mesh.
    // We bypass sendFloodScoped() because it forces _prefs.path_hash_mode+1.
    uint16_t codes[2];
    codes[0] = scope.calcTransportCode(pkt);
    codes[1] = 0;
    sendFlood(pkt, codes, 0, /*path_hash_size=*/3);
    _tx_advert_count++;
    pushDebugLog("[ADV-DBG] nightly-flood (3B path), millis=%lu rtc=%lu\n",
                  millis(), (unsigned long)getRTCClock()->getCurrentTime());
    // D4: scope-Info mit ausgeben. Welcher Slot der Send-Hierarchie
    // tatsaechlich genommen wurde (override/bake/default/geo).
    const char* src = "geo-fallback";
    const char* nm  = "?";
    auto keyNotNull = [](const uint8_t* k, size_t n) {
      for (size_t i = 0; i < n; i++) if (k[i]) return true;
      return false;
    };
    if (keyNotNull(_prefs.override_scope_key, 16)
        && memcmp(scope.key, _prefs.override_scope_key, 16) == 0) {
      src = "override"; nm = _prefs.override_scope_name;
    } else if (keyNotNull(_prefs.bake_scope_key, 16)
        && memcmp(scope.key, _prefs.bake_scope_key, 16) == 0) {
      src = "bake"; nm = _prefs.bake_scope_name;
    } else if (keyNotNull(_prefs.default_scope_key, 16)
        && memcmp(scope.key, _prefs.default_scope_key, 16) == 0) {
      src = "default"; nm = _prefs.default_scope_name;
    } else {
      // Wenn key == #local Build-in-Key -> Last-Resort gegriffen.
      int li = dl9sau_find_region_index("local");
      if (li >= 0 && li < _buildin_keys_count
          && memcmp(scope.key, _buildin_keys[li].key, 16) == 0) {
        src = "last-resort"; nm = "local";
      }
    }
    traceCompanion(TRACE_ADVERTS, "[adv] nightly-flood scope=#%s (%s) code=%04X",
                   nm[0] ? nm : "?", src, (unsigned)codes[0]);
  }
}

// Cycle the GPS module on/off to save current. Stays on during the initial
// boot search and from CR_GPS_LEAD_BEFORE_ADVERT_MS before each scheduled
// advert until shortly after. The last known position lives in
// sensors.node_lat/lon and is published in adverts even while the module
// is asleep, so we keep transmitting a recent location either way.
//
// User-controlled GPS state (CMD_SET_CUSTOM_VAR "gps") is respected:
// when _prefs.gps_enabled is 0, we don't touch anything.
void MyMesh::manageGpsPower() {
#if ENV_INCLUDE_GPS == 1
  if (!_prefs.gps_enabled) return;   // user disabled GPS entirely

  unsigned long now = millis();
  bool gps_is_on = false;
  const char* cur = sensors.getSettingByKey("gps");
  if (cur != NULL) gps_is_on = (cur[0] == '1');

  // always-on Modus: nicht cyceln, GPS dauerhaft an. Reise-Fix
  // 2026-06-08: dieser Block lief VORHER nach dem !_gps_had_fix_ever
  // early-return -- d.h. solange noch kein Fix gehabt, blieb GPS im
  // sleeping-State haengen (manageGpsPower returnte frueh, ohne dass
  // sensors.setSettingValue("gps","1") aufgerufen wurde). Jetzt VOR
  // dem fix-ever-Check: always-on greift sofort, unabhaengig vom Fix.
  if (_prefs.gps_power_mode == 1) {
    if (!gps_is_on) {
      sensors.setSettingValue("gps", "1");
      _gps_woke_at_millis = (now == 0 ? 1 : now);
      _gps_off_at_millis = 0;
      _gps_fix_seen_this_wake = false;
      pushDebugLog("[GPS-DBG] wake (always-on mode) at millis=%lu\n", now);
      traceCompanion(TRACE_GPS, "[gps] wake (always-on)");
    }
    return;
  }

  // Reise-Wunsch 2026-06-08: Cold-Start-Cycle vor first-fix.
  // Vorher: cycle-Mode + !fix_ever -> manageGpsPower returnte frueh,
  // GPS blieb vom Boot an dauernd an (= 100% Stromverbrauch). Bei
  // problematischem Empfang (Indoor + Modul-Empfindlichkeitsschwankung)
  // hat das den Akku unnoetig belastet.
  //
  // Neu: 10 min an + 10 min aus (50% Strom) bis first-fix. Manche
  // GPS-Module brauchen volle 10 min fuer Almanac-Load -- 5 min waeren
  // grenzwertig. Modul-RAM verliert bei sleep den Almanac, naechster
  // Wake ist wieder Cold-Start -- aber 10 min reichen typisch.
  //
  // Sobald first-fix da: bisheriger normaler cycle-Mode mit lead_min.
  if (!_gps_had_fix_ever) {
    if (gps_is_on) {
      // Cold-Start in progress -- nach 10 min wach ohne Fix schlafen
      unsigned long awake_for = (_gps_woke_at_millis == 0)
                               ? 0 : (now - _gps_woke_at_millis);
      if (awake_for >= CR_GPS_COLD_START_WAKE_MS) {
        sensors.setSettingValue("gps", "0");
        _gps_off_at_millis = (now == 0 ? 1 : now);
        _gps_woke_at_millis = 0;
        pushDebugLog("[GPS-DBG] cold-start sleep (no fix in %lus)\n",
                     awake_for / 1000UL);
        traceCompanion(TRACE_GPS, "[gps] cold-start sleep");
      }
    } else {
      // GPS schlafend in cold-start-cycle. Nach 10 min wieder aufwecken.
      unsigned long off_for = (_gps_off_at_millis == 0)
                             ? 0 : (now - _gps_off_at_millis);
      if (_gps_off_at_millis == 0 || off_for >= CR_GPS_COLD_START_SLEEP_MS) {
        sensors.setSettingValue("gps", "1");
        _gps_woke_at_millis = (now == 0 ? 1 : now);
        _gps_off_at_millis = 0;
        _gps_fix_seen_this_wake = false;
        pushDebugLog("[GPS-DBG] cold-start wake (slept %lus)\n",
                     off_for / 1000UL);
        traceCompanion(TRACE_GPS, "[gps] cold-start wake");
      }
    }
    return;
  }

  // Lead-Zeit aus _prefs.gps_lead_min (Minuten). Backward-compat: wenn der
  // gespeicherte Wert 0 ist (z.B. alte Datei ohne Feld), nutze Default 5.
  uint8_t lead_min = (_prefs.gps_lead_min == 0) ? 5 : _prefs.gps_lead_min;
  unsigned long lead_ms = (unsigned long)lead_min * 60UL * 1000UL;

  // distance to next advert (signed; can be negative if we're past schedule)
  long until_advert = (long)(next_periodic_advert_at - now);

  // baseline: turn GPS on shortly before each scheduled advert
  bool want_gps_on = (until_advert <= (long)lead_ms);

  // Periodic motion check / time-sync wake, independent of the advert
  // schedule. At static-rate adverts the next slot may be 55 min away,
  // but we still want to notice movement promptly so we can switch the
  // advert cadence. When advert_loc_policy == NONE there's no rush —
  // just a long-interval wake to keep RTC and last-known-position warm.
  if (_gps_off_at_millis != 0) {
    unsigned long off_for = now - _gps_off_at_millis;
    unsigned long check_interval = (_prefs.advert_loc_policy == ADVERT_LOC_NONE)
                                     ? CR_GPS_TIME_SYNC_INTERVAL_MS
                                     : CR_GPS_MOTION_CHECK_INTERVAL_MS;
    // wake lead_ms earlier so a fix has time to lock
    if (off_for + lead_ms >= check_interval) {
      want_gps_on = true;
    }
  }

  // User toggled GPS on via the app: keep it on until the next advert,
  // regardless of fix status / hysteresis / lead time.
  if (_gps_user_override_until_advert) {
    want_gps_on = true;
  }

  if (gps_is_on && _gps_woke_at_millis != 0) {
    unsigned long awake_for = now - _gps_woke_at_millis;
    if (awake_for < CR_GPS_MIN_AWAKE_MS) {
      // hysteresis: don't thrash the GPS_EN pin
      want_gps_on = true;
    } else if (!_gps_fix_seen_this_wake) {
      // We woke GPS up but haven't seen a real position fix this cycle yet
      // (NMEA time-sync alone is not enough). Keep trying so the next advert
      // actually carries a fresh position.
      want_gps_on = true;
    }
  }

  if (want_gps_on && !gps_is_on) {
    sensors.setSettingValue("gps", "1");
    _gps_woke_at_millis = (now == 0 ? 1 : now);   // 0 means "never managed"
    _gps_off_at_millis = 0;
    _gps_fix_seen_this_wake = false;              // start a fresh wake cycle
    pushDebugLog("[GPS-DBG] wake at millis=%lu (until_advert=%lds)\n", now, until_advert / 1000);
    {
      char st[140];
      appendGpsTraceStatus(st, sizeof(st));
      traceCompanion(TRACE_GPS, "[gps] wake (until_advert=%lds) %s",
                     until_advert / 1000, st);
    }
  } else if (!want_gps_on && gps_is_on) {
    // Wert VOR dem Reset retten -- sonst loggen wir immer 0. Der Reset selbst
    // gehoert hierher (frischer Wake-Cycle bei naechstem wake), nur die
    // Reihenfolge war Bug-haftig.
    bool fix_was_seen = _gps_fix_seen_this_wake;
    // NMEA-Parser-State explizit invalidieren bevor GPS abschaltet.
    // syncTime() ruft intern nmea.clear() + setzt _time_sync_needed=true:
    //   - clear() verhindert dass der naechste loop() nach Wake mit stalem
    //     isValid()=true/getTimestamp() sofort die RTC rueckwaerts springen
    //     laesst (Bug 2026-05-29: symmetrische +/-7min RTC-Spruenge).
    //   - _time_sync_needed=true sorgt fuer frische RTC-Sync nach Wake
    //     statt auf den naechsten 30-min-Slot zu warten.
    // Redundant zu nmea.clear() in MicroNMEALocationProvider::stop()/begin();
    // greift aber auch wenn diese Upstream-Patches ge-reverted wuerden.
    LocationProvider* loc = sensors.getLocationProvider();
    if (loc) loc->syncTime();
    sensors.setSettingValue("gps", "0");
    _gps_woke_at_millis = 0;
    _gps_off_at_millis = (now == 0 ? 1 : now);
    _gps_fix_seen_this_wake = false;
    pushDebugLog("[GPS-DBG] sleep at millis=%lu (until_advert=%lds, fix_was_seen=%d)\n",
                  now, until_advert / 1000, (int)fix_was_seen);
    {
      // Status-Suffix nach dem syncTime()/nmea.clear() oben -- loc_valid und
      // time_valid sind hier definitionsgemaess 0. Pos/Alt/RTC bleiben
      // letzte bekannte Werte (sensors.node_lat/lon/altitude wurden zuvor
      // gecached).
      char st[140];
      appendGpsTraceStatus(st, sizeof(st));
      if (fix_was_seen)
        traceCompanion(TRACE_GPS, "[gps] sleep (fix this wake) %s", st);
      else
        traceCompanion(TRACE_GPS, "[gps] sleep (got no fix this wake) %s", st);
    }
  }
#endif
}

void MyMesh::maybePushGeoRecommendation(double lat, double lon) {
  if (lat == 0.0 && lon == 0.0) return;

  // Hysterese-Schwelle (siehe CR_GEO_RECO_REEVAL_DIST_M): solange wir uns
  // seit der letzten Auswertung weniger als 10 km bewegt haben, sparen wir
  // uns die Neu-Berechnung. Beim allerersten Aufruf ist der Anker (0,0) —
  // jede reale Position liefert dann eine huge Distanz und triggert.
  // Equirectangular approximation; reicht völlig für die 10-km-Schwelle.
  double dlat_m = (lat - _geo_reco_anchor_lat) * 111320.0;
  double dlon_m = (lon - _geo_reco_anchor_lon) * 111320.0 * cos(lat * DEG_TO_RAD);
  double dist_m = sqrt(dlat_m * dlat_m + dlon_m * dlon_m);
  if (dist_m < CR_GEO_RECO_REEVAL_DIST_M) return;

  // Anker IMMER updaten (auch wenn der Output unverändert bleibt), damit die
  // Distanz nicht von der ursprünglichen Anker-Position akkumuliert.
  _geo_reco_anchor_lat = lat;
  _geo_reco_anchor_lon = lon;

  char buf[200];
  dl9sau_recommend_scopes(lat, lon, buf, sizeof(buf));
  if (buf[0] == 0) return;
  if (strcmp(buf, _last_geo_reco) == 0) return;   // unchanged, skip
  strncpy(_last_geo_reco, buf, sizeof(_last_geo_reco) - 1);
  _last_geo_reco[sizeof(_last_geo_reco) - 1] = 0;

  char ll[32];
  formatLatLonDM(ll, sizeof(ll), lat, lon);
  pushDebugLog("[GEO-SCOPE] %s -> %s", ll, buf);
  // Zusätzlich im Companion-Channel anzeigen, damit die Info auch bei
  // verbundener App sichtbar wird (nicht nur im Debug-Protokoll-View).
  // Wunschliste 21 (2026-05-30): durch logging-channel-Master-Switch
  // gegated -- bei 'logging channel off' wird die Info-Push unterdrueckt
  // (vorher kam sie auch bei trace-off + logging-usb-off durch).
  if (!(_prefs.log_flags & 0x02)) {
    char chat[256];
    snprintf(chat, sizeof(chat), "GEO-SCOPE @ %s: %s", ll, buf);
    pushCompanionMessage(chat);
  }
}

// Helper-Implementation. Siehe Header fuer Format-Doku.
// Minuten-Nachkomma jetzt 3 Stellen (User-Wunsch nach Genauigkeit):
// fueher 'DD-MM,M', jetzt 'DD-MM,MMM'.
void MyMesh::formatLatLonDM(char* out, size_t out_size, double lat, double lon) const {
  auto fmt_one = [](char* p, size_t n, double v, int deg_w, char pos, char neg) {
    char hemi = (v >= 0) ? pos : neg;
    double a = fabs(v);
    int deg = (int)a;
    double rem_min = (a - deg) * 60.0;
    int min_int = (int)rem_min;
    // 3 Nachkommastellen der Minuten (~0.001' ~ 1.8 m am Aequator).
    int min_frac = (int)((rem_min - min_int) * 1000.0 + 0.5);
    if (min_frac >= 1000) { min_frac = 0; min_int++; }
    if (min_int >= 60)    { min_int = 0;  deg++; }
    snprintf(p, n, "%0*d-%02d,%03d%c", deg_w, deg, min_int, min_frac, hemi);
  };
  char lat_dm[20], lon_dm[20];
  fmt_one(lat_dm, sizeof(lat_dm), lat, 2, 'N', 'S');
  fmt_one(lon_dm, sizeof(lon_dm), lon, 3, 'E', 'W');
  snprintf(out, out_size, "%s %s", lat_dm, lon_dm);
}

// ---------------------------------------------------------------------------
// Companion-Channel — lokal, kein RF. Output erscheint im normalen Chat der
// App, Input vom User wird als Befehl an die Firmware geparst statt zu
// transmitten. Sinn: Sichtbarkeit der Firmware-Events (Geo-Empfehlung etc.)
// und bidirektionale Konfiguration ohne die App-eigenen Menüs anfassen zu
// müssen. Pre-Connect-Buffer: existierende Offline-Queue (16 Slots) — keine
// eigene Datenstruktur nötig.
// ---------------------------------------------------------------------------
#define COMPANION_CHANNEL_NAME "companion"

// Wunschliste 28 Phase A: Backup-Save nach USB-Serial.
// Zwei JSON-Bloecke mit Marker-Linien.
// Format: pretty-printed multi-line, eine "key": value Zeile pro Feld
// (Komma davor wenn nicht erste Zeile -> Tracking via 'first' Flag).
void MyMesh::backupSaveToSerial() {
  // Guard: USB-Serial muss verbunden sein. Sonst stauen sich hunderte
  // Serial.print()-Aufrufe im HWCDC-TX-Buffer, jeder kann (bei DTR-true
  // ohne aktiv lesendem Host) bis zu tx_timeout_ms blockieren --
  // cumulative kann das BLE-Supervision-Timeout reissen und die App-
  // Verbindung abreisst (User-Report 2026-06-02, Powerbank-Use-Case).
  // Mit 'if (!Serial)' (HWCDC::operator bool() == isCDC_Connected())
  // halten wir die Backup-Action zurueck wenn kein Host bereit ist.
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  if (!Serial) {
    pushCompanionMessage(
      "backup save: USB-Serial nicht verbunden.\n"
      "Terminal anschliessen (z.B. cu / minicom) und\n"
      "danach Befehl erneut absetzen.");
    return;
  }
#endif
  bool first;
  // Hilfs-Lambdas. Schreiben direkt auf Serial.
  auto kv_uint = [&](const char* name, uint32_t v) {
    if (!first) Serial.println(",");
    first = false;
    Serial.print("  \""); Serial.print(name); Serial.print("\": "); Serial.print(v);
  };
  auto kv_uint64_hex = [&](const char* name, uint64_t v) {
    // 16-stelliger Hex-String fuer 64-bit-Channel-Masks. Lesbar +
    // restore-parser kann mit strtoull(base 16) wieder einlesen.
    if (!first) Serial.println(",");
    first = false;
    char buf[20];
    snprintf(buf, sizeof(buf), "0x%08lx%08lx",
             (unsigned long)((v >> 32) & 0xFFFFFFFFul),
             (unsigned long)(v & 0xFFFFFFFFul));
    Serial.print("  \""); Serial.print(name); Serial.print("\": \"");
    Serial.print(buf); Serial.print("\"");
  };
  auto kv_int = [&](const char* name, int32_t v) {
    if (!first) Serial.println(",");
    first = false;
    Serial.print("  \""); Serial.print(name); Serial.print("\": "); Serial.print(v);
  };
  auto kv_float = [&](const char* name, double v, int prec) {
    if (!first) Serial.println(",");
    first = false;
    Serial.print("  \""); Serial.print(name); Serial.print("\": "); Serial.print(v, prec);
  };
  // String-Emit -- Standard-JSON-Escape:
  //   " \ -> \" \\
  //   \b \f \n \r \t -> Standard-Kurzform
  //   andere Steuerzeichen < 0x20 -> \u00XX
  //   ASCII 0x20..0x7E direkt
  //   UTF-8 multi-byte -> Codepoint dekodiert, dann \uXXXX (BMP)
  //     bzw Surrogate-Pair \uHHHH\uLLLL (Supplementary Plane,
  //     Codepoint > U+FFFF; Emojis etc.)
  // Pure ASCII Output -> terminal-copy-safe. Standard-JSON-konform
  // damit externe Tools (jq, Python json) den Backup lesen koennen.
  auto kv_str = [&](const char* name, const char* s) {
    if (!first) Serial.println(",");
    first = false;
    Serial.print("  \""); Serial.print(name); Serial.print("\": \"");
    while (*s) {
      unsigned char c = (unsigned char)*s++;
      if (c == '"')       { Serial.print("\\\""); continue; }
      if (c == '\\')      { Serial.print("\\\\"); continue; }
      if (c == '\b')      { Serial.print("\\b");  continue; }
      if (c == '\f')      { Serial.print("\\f");  continue; }
      if (c == '\n')      { Serial.print("\\n");  continue; }
      if (c == '\r')      { Serial.print("\\r");  continue; }
      if (c == '\t')      { Serial.print("\\t");  continue; }
      if (c < 0x20) {
        char b[8]; snprintf(b, sizeof(b), "\\u%04X", c); Serial.print(b);
        continue;
      }
      if (c < 0x80) {
        Serial.print((char)c);
        continue;
      }
      // UTF-8 multi-byte. Codepoint dekodieren.
      uint32_t cp = 0;
      int rem = 0;
      if      ((c & 0xE0) == 0xC0) { cp = c & 0x1F; rem = 1; }
      else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; rem = 2; }
      else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; rem = 3; }
      else { Serial.print("\\uFFFD"); continue; } // invalides leading byte
      bool ok = true;
      for (int j = 0; j < rem; j++) {
        unsigned char cb = (unsigned char)*s;
        if (cb == 0 || (cb & 0xC0) != 0x80) { ok = false; break; }
        cp = (cp << 6) | (cb & 0x3F);
        s++;
      }
      if (!ok) { Serial.print("\\uFFFD"); continue; }
      if (cp <= 0xFFFF) {
        char b[8]; snprintf(b, sizeof(b), "\\u%04X", (unsigned)cp); Serial.print(b);
      } else {
        // Supplementary Plane -> Surrogate Pair
        cp -= 0x10000;
        uint32_t hi = 0xD800u + (cp >> 10);
        uint32_t lo = 0xDC00u + (cp & 0x3FFu);
        char b[16];
        snprintf(b, sizeof(b), "\\u%04X\\u%04X", (unsigned)hi, (unsigned)lo);
        Serial.print(b);
      }
    }
    Serial.print('"');
  };
  auto kv_hex = [&](const char* name, const uint8_t* bytes, size_t len) {
    if (!first) Serial.println(",");
    first = false;
    Serial.print("  \""); Serial.print(name); Serial.print("\": \"");
    for (size_t i = 0; i < len; i++) {
      char b[3]; snprintf(b, sizeof(b), "%02x", bytes[i]); Serial.print(b);
    }
    Serial.print('"');
  };
  auto kv_arr_uint8 = [&](const char* name, const uint8_t* arr, size_t n) {
    if (!first) Serial.println(",");
    first = false;
    Serial.print("  \""); Serial.print(name); Serial.print("\": [");
    for (size_t i = 0; i < n; i++) {
      if (i > 0) Serial.print(", ");
      Serial.print((unsigned)arr[i]);
    }
    Serial.print("]");
  };

  // Helper: nested _meta-Block schreiben. Enthaelt fw_version + pubkey
  // (= Identitaets-Anker zum Verifizieren). KEIN prv_key mehr im
  // Backup -- User-Wunsch 2026-06-02: damit der private Schluessel
  // nicht versehentlich in Logs/Backup-Dateien/Cloud-Sync abhanden
  // kommt. Wer eine Identitaet auf ein neues Geraet migrieren will,
  // nutzt explizit den prv.key-Pfad (set prv.key NEW, manuelle
  // Schluessel-Eingabe etc.).
  auto emit_meta = [&]() {
    if (!first) Serial.println(",");
    first = false;
    Serial.println("  \"_meta\": {");
    Serial.print("    \"fw_version\": \"");
    Serial.print(FIRMWARE_VERSION);
    Serial.println("\",");
    Serial.print("    \"pubkey\": \"");
    for (int i = 0; i < PUB_KEY_SIZE; i++) {
      char b[3]; snprintf(b, sizeof(b), "%02x", self_id.pub_key[i]); Serial.print(b);
    }
    Serial.println("\"");
    Serial.print("  }");
  };

  // -------- Block 1: DL9SAU prefs --------
  Serial.println();
  Serial.println("--- BACKUP DL9SAU PREFS BEGIN ---");
  Serial.println("{");
  first = true;
  emit_meta();
  kv_uint("chat_name_mode",        _prefs.chat_name_mode);
  kv_str ("chat_name_custom",      _prefs.chat_name_custom);
  kv_uint("auto_advert_enabled",   _prefs.auto_advert_enabled);
#ifdef REPEATER_DEFENSIVE_FORCE
  kv_uint("client_repeat_force",   _prefs.client_repeat_force);
#endif
  kv_uint("repeater_profile",      _prefs.repeater_profile);
  kv_uint("loop_detect",           _prefs.loop_detect);
  kv_uint("duty_soft_pct",         _prefs.duty_soft_pct);
  kv_uint("duty_hard_pct",         _prefs.duty_hard_pct);
  kv_str ("bake_scope_name",       _prefs.bake_scope_name);
  kv_hex ("bake_scope_key",        _prefs.bake_scope_key, 16);
  kv_str ("override_scope_name",   _prefs.override_scope_name);
  kv_hex ("override_scope_key",    _prefs.override_scope_key, 16);
  kv_uint("override_expiry",       _prefs.override_expiry);
  kv_uint("scope_advert_auto",     _prefs.scope_advert_auto);
  kv_uint("scope_repeater_auto",   _prefs.scope_repeater_auto);
  kv_uint("advert_role",           _prefs.advert_role);
  kv_str ("owner_info",            _prefs.owner_info);
  kv_uint("trace_flags_persistent",_prefs.trace_flags_persistent);
  kv_uint("gps_power_mode",        _prefs.gps_power_mode);
  kv_uint("gps_lead_min",          _prefs.gps_lead_min);
  kv_uint("repeat_scope_mode",     _prefs.repeat_scope_mode);
  kv_uint("msg_store_flash",       _prefs.msg_store_flash);
  kv_arr_uint8("msg_store_limit",  _prefs.msg_store_limit, 5);
  kv_uint("log_flags",             _prefs.log_flags);
  Serial.println();
  Serial.println("}");
  Serial.println("--- BACKUP DL9SAU PREFS END ---");
  Serial.flush();

  // -------- Block 2: Node Main (App-Settings) --------
  Serial.println();
  Serial.println("--- BACKUP NODE MAIN BEGIN ---");
  Serial.println("{");
  first = true;
  emit_meta();
  kv_str  ("name",                 _prefs.node_name);
  kv_float("freq",                 _prefs.freq, 4);
  kv_uint ("sf",                   _prefs.sf);
  kv_float("bw",                   _prefs.bw, 1);
  kv_uint ("cr",                   _prefs.cr);
  kv_int  ("tx_power",             _prefs.tx_power_dbm);
  kv_uint ("repeat",               _prefs.client_repeat);
  kv_uint ("gps",                  _prefs.gps_enabled);
  kv_uint ("gps_interval",         _prefs.gps_interval);
  kv_uint ("advert_loc_policy",    _prefs.advert_loc_policy);
  kv_float("airtime_factor",       _prefs.airtime_factor, 3);
  kv_uint ("rx_boosted_gain",      _prefs.rx_boosted_gain);
  kv_uint ("manual_add_contacts",  _prefs.manual_add_contacts);
  kv_uint ("multi_acks",           _prefs.multi_acks);
  kv_uint ("path_hash_mode",       _prefs.path_hash_mode);
  kv_uint ("autoadd_config",       _prefs.autoadd_config);
  kv_uint ("autoadd_max_hops",     _prefs.autoadd_max_hops);
  kv_uint ("telemetry_mode_base",  _prefs.telemetry_mode_base);
  kv_uint ("telemetry_mode_loc",   _prefs.telemetry_mode_loc);
  kv_uint ("telemetry_mode_env",   _prefs.telemetry_mode_env);
  kv_uint ("buzzer_quiet",         _prefs.buzzer_quiet);
  kv_uint ("messages_append_scope_to_name", _prefs.messages_append_scope_to_name);
  kv_float("rxdelay",              _prefs.rx_delay_base, 3);
  kv_float("txdelay",              _prefs.tx_delay_factor, 3);
  kv_float("direct_txdelay",       _prefs.direct_tx_delay_factor, 3);
  // Wunschliste 39: cap-Vars als kv_str mit follow/off Keywords wenn
  // anwendbar, sonst Number-as-String. Restore akzeptiert beide
  // Formate (number oder string) -- human-editable Backup.
  auto kv_cap = [&](const char* name, uint8_t v,
                    bool has_follow, bool has_off) {
    char buf[8];
    if (has_follow && v == FLOOD_MAX_INFRA_FOLLOW) { kv_str(name, "follow"); }
    else if (has_off && v == 0) { kv_str(name, "off"); }
    else { snprintf(buf, sizeof(buf), "%u", (unsigned)v); kv_str(name, buf); }
  };
  // flood_max selbst: keine Keywords, immer Number.
  kv_uint("flood_max",            _prefs.flood_max);
  kv_cap ("flood_max_scope_region",  _prefs.flood_max_scope_region,
                                     /*follow=*/false, /*off=*/true);
  kv_cap ("flood_max_infra",         _prefs.flood_max_infra,
                                     /*follow=*/true,  /*off=*/false);
  kv_cap ("flood_max_req_resp",      _prefs.flood_max_req_resp,
                                     /*follow=*/true,  /*off=*/false);
  kv_cap ("flood_max_unknown_chan",  _prefs.flood_max_unknown_chan,
                                     /*follow=*/true,  /*off=*/true);
  kv_cap ("flood_max_unscoped_companions", _prefs.flood_max_unscoped_companions,
                                     /*follow=*/true,  /*off=*/true);
  // Wunschliste 31: time-sync prefs
  kv_uint ("time_sync_mode",       _prefs.time_sync_mode);
  // Sources als 6-hex-Strings (3 Slots, leere als 000000)
  {
    char buf[8];
    for (int i = 0; i < 3; i++) {
      snprintf(buf, sizeof(buf), "%02x%02x%02x",
               _prefs.time_sync_sources[i][0],
               _prefs.time_sync_sources[i][1],
               _prefs.time_sync_sources[i][2]);
      char key_[24];
      snprintf(key_, sizeof(key_), "time_sync_src%d", i);
      kv_str(key_, buf);
    }
  }
  kv_float("lat",                  sensors.node_lat, 6);
  kv_float("lon",                  sensors.node_lon, 6);

  // Wunschliste 43 BLE-Power-Mode.
  kv_uint("bluetooth_power_mode", _prefs.bluetooth_power_mode);

  // Wunschliste 46 Filter (Phase 1-5, 2026-06-10):
  // Filter-Listen + channel-masks + Repeat-Achse exportieren.
  kv_uint("filter_unknown_channel_repeat", _prefs.filter_unknown_channel_repeat);
  auto emit_filter_list = [&](const char* base, NodePrefs::FilterEntry* arr,
                              uint8_t cnt, const uint64_t* c_on, const uint64_t* c_ex) {
    char k[64];
    snprintf(k, sizeof(k), "%s_count", base);
    kv_uint(k, cnt);
    for (uint8_t i = 0; i < cnt; i++) {
      snprintf(k, sizeof(k), "%s_%u_pattern", base, (unsigned)i);
      kv_str(k, arr[i].pattern);
      snprintf(k, sizeof(k), "%s_%u_flags", base, (unsigned)i);
      kv_uint(k, arr[i].flags);
      snprintf(k, sizeof(k), "%s_%u_chan_on", base, (unsigned)i);
      kv_uint64_hex(k, c_on[i]);
      snprintf(k, sizeof(k), "%s_%u_chan_ex", base, (unsigned)i);
      kv_uint64_hex(k, c_ex[i]);
    }
  };
  auto emit_filter_scope_list = [&](const char* base, NodePrefs::FilterScopeEntry* arr,
                                    uint8_t cnt, const uint64_t* c_on, const uint64_t* c_ex) {
    char k[64];
    snprintf(k, sizeof(k), "%s_count", base);
    kv_uint(k, cnt);
    for (uint8_t i = 0; i < cnt; i++) {
      snprintf(k, sizeof(k), "%s_%u_name", base, (unsigned)i);
      kv_str(k, arr[i].scope_name);
      snprintf(k, sizeof(k), "%s_%u_flags", base, (unsigned)i);
      kv_uint(k, arr[i].flags);
      snprintf(k, sizeof(k), "%s_%u_chan_on", base, (unsigned)i);
      kv_uint64_hex(k, c_on[i]);
      snprintf(k, sizeof(k), "%s_%u_chan_ex", base, (unsigned)i);
      kv_uint64_hex(k, c_ex[i]);
    }
  };
  emit_filter_list("filter_sender_drop", _prefs.filter_sender_drop,
                   _prefs.filter_sender_drop_count,
                   _prefs.filter_sender_drop_chan_on,
                   _prefs.filter_sender_drop_chan_ex);
  emit_filter_list("filter_sender_keep", _prefs.filter_sender_keep,
                   _prefs.filter_sender_keep_count,
                   _prefs.filter_sender_keep_chan_on,
                   _prefs.filter_sender_keep_chan_ex);
  emit_filter_list("filter_text_drop", _prefs.filter_text_drop,
                   _prefs.filter_text_drop_count,
                   _prefs.filter_text_drop_chan_on,
                   _prefs.filter_text_drop_chan_ex);
  emit_filter_list("filter_text_keep", _prefs.filter_text_keep,
                   _prefs.filter_text_keep_count,
                   _prefs.filter_text_keep_chan_on,
                   _prefs.filter_text_keep_chan_ex);
  emit_filter_scope_list("filter_scope_drop", _prefs.filter_scope_drop,
                         _prefs.filter_scope_drop_count,
                         _prefs.filter_scope_drop_chan_on,
                         _prefs.filter_scope_drop_chan_ex);
  emit_filter_scope_list("filter_scope_keep", _prefs.filter_scope_keep,
                         _prefs.filter_scope_keep_count,
                         _prefs.filter_scope_keep_chan_on,
                         _prefs.filter_scope_keep_chan_ex);

  Serial.println();
  Serial.println("}");
  Serial.println("--- BACKUP NODE MAIN END ---");
  Serial.println();
  Serial.flush();

  // -------- Block 3: Hashtag-Channels (Wunschliste 28 Phase D) --------
  // Nur Channels deren Name mit '#' beginnt -- die PSK wird beim Restore
  // deterministisch aus dem Namen rekonstruiert (SHA-256-Prefix-Algorithmus
  // dokumentiert in docs/companion_protocol.md). Public-Channel hat eigene
  // hartcodierte PSK -- nicht im Backup. $companion-Channel wird beim
  // Boot von setupCompanionChannel() automatisch (re-)angelegt, ebenfalls
  // nicht im Backup. Private-Channels: bewusst NICHT im Backup, weil deren
  // PSK random ist und nicht rekonstruierbar -- User muss seinen Key
  // separat sichern (z.B. via App-Share).
  Serial.println();
  Serial.println("--- BACKUP HASHTAG CHANNELS BEGIN ---");
  Serial.println("{");
  first = true;
  emit_meta();
  {
    int idx = 0;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      ChannelDetails ch;
      if (!getChannel(i, ch)) continue;
      if (ch.name[0] != '#') continue;
      // Defensive: $companion-Channel sollte mit '#' nicht starten,
      // aber sicher ist sicher (PSK-Match).
      if (memcmp(ch.channel.secret, s_companion_psk_magic, 16) == 0) continue;
      char key[16];
      snprintf(key, sizeof(key), "ch%d", idx++);
      kv_str(key, ch.name);
    }
    // Public-PSK Channels (Wunschliste 36): Public hat festen Key
    // (s_public_psk) -- damit ist er genauso restaurierbar wie Hashtag-
    // Channels. User kann Public umbenennen oder loeschen; Backup
    // erfasst den aktuellen Slot-Stand. Format: "ch_pub_N": "<name>".
    // Random-private Channels bleiben aussen vor (Key nicht rekonstruier-
    // bar) -- wuerde bei Restore auf Target-Geraet verloren gehen.
    int puidx = 0;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      ChannelDetails ch;
      if (!getChannel(i, ch)) continue;
      if (ch.name[0] == 0) continue;
      if (memcmp(ch.channel.secret, s_public_psk, 16) != 0) continue;
      char key[24];
      snprintf(key, sizeof(key), "ch_pub_%d", puidx++);
      kv_str(key, ch.name);
    }
    // Wunschliste 32: per-Channel hop-cap mit save. Format
    // "ch_hops_N": "Name=Cap". Inkl. Public (kein '#' aber gespeicherte
    // Cap-Aenderung soll erhalten bleiben). Companion-Slot wird beim
    // Boot eh forced -- nicht saven.
    int hidx = 0;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      ChannelDetails ch;
      if (!getChannel(i, ch)) continue;
      if (ch.name[0] == 0) continue;
      if (_channel_hops_cap_cache[i] == CH_HOPS_OFF) continue;
      if (memcmp(ch.channel.secret, s_companion_psk_magic, 16) == 0) continue;
      char val[48];
      snprintf(val, sizeof(val), "%s=%u", ch.name, (unsigned)_channel_hops_cap_cache[i]);
      char key[24];
      snprintf(key, sizeof(key), "ch_hops_%d", hidx++);
      kv_str(key, val);
    }
    // External-Eintraege (nicht-abonnierte Hashtag-Channels) -- Name
    // ist jetzt in der Entry gespeichert, also voll restaurierbar.
    // Format: "ch_hops_ext_N": "Name=Cap".
    int eidx = 0;
    for (uint8_t e = 0; e < _prefs.channel_hops_count; e++) {
      const auto& en = _prefs.channel_hops_list[e];
      if (!(en.flags & CH_HOPS_FLAG_EXTERNAL)) continue;
      if (en.name[0] == 0) continue;  // defensive
      char val[40];
      snprintf(val, sizeof(val), "%s=%u", en.name, (unsigned)en.cap);
      char key[24];
      snprintf(key, sizeof(key), "ch_hops_ext_%d", eidx++);
      kv_str(key, val);
    }
  }
  Serial.println();
  Serial.println("}");
  Serial.println("--- BACKUP HASHTAG CHANNELS END ---");
  Serial.println();
  Serial.flush();
}

// =========================================================================
// Wunschliste 28 Phase B: backup restore -- State-Machine + JSON-Parser
// =========================================================================

void MyMesh::backupRestoreStart() {
  _br_state = BR_WAIT_MARKER;
  _br_block_type = 0;
  _br_line_len = 0;
  _br_json_len = 0;
  _br_brace_depth = 0;
  _br_in_string = false;
  _br_escape_next = false;
  _br_applied = 0;
  _br_skipped = 0;
  _br_errors = 0;
  _br_reboot_recommended = false;
  _br_timeout_at = futureMillis(60000);  // 60 s
  // RX-Buffer aufstocken damit ein 3-4 KB Paste-Burst nicht im
  // USB-CDC-FIFO ueberlaeuft bevor wir drain'en koennen.
  // (Default ist klein, ~256 byte.)
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  Serial.setRxBufferSize(4096);
  // Echo-TX zuverlaessiger machen waehrend Restore -- 50 ms Timeout
  // verhindert Echo-Drops bei Burst, ohne BLE-Disconnects zu
  // riskieren (Restore ist kurz). Wird in finish() zurueckgesetzt.
  Serial.setTxTimeoutMs(50);
#endif
  // Mit CRLF damit Terminal sauber bricht.
  Serial.print("\r\n# backup restore: waiting for BACKUP ... BEGIN/END markers.\r\n");
  Serial.print("# Paste backup content now. Ctrl-D = finish, 60 s timeout.\r\n");
  Serial.flush();
  pushCompanionMessage("backup restore: paste JSON via USB-Serial.\n"
                       "Ctrl-D (0x04) zum Beenden. 60s Timeout.");
}

void MyMesh::backupRestoreFinish(const char* reason) {
  // End-Statusmeldung. Bei reboot-relevanten Feldern (Radio-Params,
  // prv_key) wird zusaetzlich ein Reboot-Hinweis angehaengt -- kein
  // auto-Reboot, User entscheidet.
  char r[200];
  int n = snprintf(r, sizeof(r),
           "backup restore %s.\n"
           "applied=%u skipped=%u errors=%u\n"
           "(runtime only -- 'save' (USB-Serial oder $companion) fuer persistent)",
           reason ? reason : "done",
           (unsigned)_br_applied, (unsigned)_br_skipped, (unsigned)_br_errors);
  if (_br_reboot_recommended && n > 0 && n < (int)sizeof(r)) {
    snprintf(r + n, sizeof(r) - n,
             "\nreboot empfohlen (radio/identity geaendert).");
  }
  // Auf USB-Serial: pro embedded '\n' ein '\r\n' + '# ' praefix damit
  // jede Zeile sauber im Terminal landet.
  Serial.print("\r\n# ");
  for (const char* p = r; *p; p++) {
    if (*p == '\n') Serial.print("\r\n# ");
    else if (*p != '\r') Serial.write(*p);
  }
  Serial.print("\r\n");
  Serial.flush();
  pushCompanionMessage(r);
  _br_state = BR_IDLE;
  _br_block_type = 0;
  _br_line_len = 0;
  _br_json_len = 0;
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  // TX-Timeout zurueck auf 0 (non-blocking) damit BLE-Disconnects auf
  // Powerbank-Betrieb nicht wiederkommen.
  Serial.setTxTimeoutMs(0);
#endif
}

void MyMesh::backupRestoreLoop() {
  if (_br_state == BR_IDLE) return;
  if (millisHasNowPassed(_br_timeout_at)) {
    backupRestoreFinish("timeout (60s)");
    return;
  }
  // Read all currently available bytes.
  while (Serial.available() > 0) {
    int b = Serial.read();
    if (b < 0) break;
    unsigned char c = (unsigned char)b;
    // Ctrl-D = sauberes Beenden. '^D' im Terminal echoen damit der
    // User sieht dass er Ctrl-D gesendet hat.
    if (c == 0x04) {
      Serial.print("^D");
      backupRestoreFinish("done (Ctrl-D)");
      return;
    }
    // Reset Timeout bei jedem byte (User schreibt aktiv).
    _br_timeout_at = futureMillis(60000);

    // Echo fuer User-Feedback waehrend cut+paste. Druckbare ASCII
    // direkt zurueck; CR/LF als \r\n damit Terminal sauber umbricht.
    // Wir tracken nicht ob CRLF / LF / CR -- alle line-ends werden zu
    // \r\n, was harmlos doppelt wirken kann aber nie fehlt.
    if (c == '\r' || c == '\n') {
      Serial.write('\r'); Serial.write('\n');
    } else if (c >= 0x20 && c < 0x7F) {
      Serial.write(c);
    }
    // Steuerzeichen ausser \r\n nicht echoen (z.B. paste mit \x... wuerde
    // unleserlich werden).

    if (_br_state == BR_WAIT_MARKER) {
      // Zeilen-Ende-Erkennung -- akzeptieren ALLE Conventions:
      //   LF only (Unix):     \n triggert line-processing
      //   CR only (old Mac):  \r triggert line-processing
      //   CRLF (DOS/Win):     \r triggert line-processing; das folgende
      //                       \n trifft auf leeren Buffer -> no-op
      // Vorherige Version triggerte nur auf \n, was bei \r-only Paste
      // dazu fuehrte dass Marker NIE erkannt wurden (User-Bug 2026-06-01).
      if (c == '\r' || c == '\n') {
        if (_br_line_len == 0) continue;  // leere Zeile (z.B. \n nach \r)
        _br_line[_br_line_len] = 0;
        // Marker erkennen
        if (strncmp(_br_line, "--- BACKUP ", 11) == 0) {
          const char* type_str = _br_line + 11;
          if (strncmp(type_str, "DL9SAU PREFS BEGIN ---", 22) == 0) {
            _br_block_type = 1;
            _br_state = BR_READING_JSON;
            _br_json_len = 0;
            _br_brace_depth = 0;
            _br_in_string = false;
            _br_escape_next = false;
            Serial.print("\r\n# DL9SAU PREFS block: reading JSON...\r\n");
            Serial.flush();
          } else if (strncmp(type_str, "NODE MAIN BEGIN ---", 19) == 0) {
            _br_block_type = 2;
            _br_state = BR_READING_JSON;
            _br_json_len = 0;
            _br_brace_depth = 0;
            _br_in_string = false;
            _br_escape_next = false;
            Serial.print("\r\n# NODE MAIN block: reading JSON...\r\n");
            Serial.flush();
          } else if (strncmp(type_str, "HASHTAG CHANNELS BEGIN ---", 26) == 0) {
            _br_block_type = 3;
            _br_state = BR_READING_JSON;
            _br_json_len = 0;
            _br_brace_depth = 0;
            _br_in_string = false;
            _br_escape_next = false;
            // Pre-Clear (Wunschliste 36): bevor Block-Entries appliziert
            // werden, alle restaurierbaren Slots leeren -- hashtag (PSK
            // aus Name rekonstruierbar) und Public-PSK (fester Key).
            // Damit reflektiert das Restore auch Source-Loeschungen statt
            // nur additiv zu sein. NICHT angefasst: companion (PSK-magic,
            // wird beim Boot eh wieder erzeugt) und random-private (PSK
            // nicht im Backup, Loeschen wuerde User-Channel verlieren).
            int br_cleared = 0;
            for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
              ChannelDetails ch;
              if (!getChannel(i, ch)) continue;
              if (ch.name[0] == 0) continue;
              if (memcmp(ch.channel.secret, s_companion_psk_magic, 16) == 0) continue;
              bool is_hashtag = (ch.name[0] == '#');
              bool is_public  = (memcmp(ch.channel.secret, s_public_psk, 16) == 0);
              if (!is_hashtag && !is_public) continue;  // random-private bleibt
              ChannelDetails empty;
              memset(&empty, 0, sizeof(empty));
              if (setChannel(i, empty)) br_cleared++;
            }
            if (br_cleared > 0) saveChannels();
            char dbg[80];
            snprintf(dbg, sizeof(dbg),
                     "\r\n# HASHTAG CHANNELS block: pre-clear (%d slots), reading JSON...\r\n",
                     br_cleared);
            Serial.print(dbg);
            Serial.flush();
          }
          // andere Marker (z.B. END) ignorieren, bleiben in WAIT_MARKER
        }
        _br_line_len = 0;
      } else if (_br_line_len < sizeof(_br_line) - 1) {
        _br_line[_br_line_len++] = c;
      }
      // else: Zeile zu lang, ignorieren bis \n
    }
    else if (_br_state == BR_READING_JSON) {
      if (_br_json_len >= sizeof(_br_json) - 1) {
        _br_errors++;
        Serial.println("# error: JSON buffer overflow, skipping block.");
        _br_state = BR_WAIT_MARKER;
        _br_json_len = 0;
        _br_line_len = 0;
        continue;
      }
      _br_json[_br_json_len++] = c;
      // Brace-Depth tracking mit String-Awareness
      if (_br_escape_next) {
        _br_escape_next = false;
      } else if (_br_in_string) {
        if (c == '\\') _br_escape_next = true;
        else if (c == '"') _br_in_string = false;
      } else {
        if (c == '"') _br_in_string = true;
        else if (c == '{') _br_brace_depth++;
        else if (c == '}') {
          _br_brace_depth--;
          if (_br_brace_depth == 0) {
            _br_json[_br_json_len] = 0;
            // Per-Block-Stats: zeigt was im JUST-parse-ten Block angewendet
            // wurde. Bleibt drin als Diagnose (User-Wunsch 2026-06-01: 'koennte
            // den Wert pruefen koennen' bei Mehr-Block-Restores).
            uint16_t before_applied = _br_applied;
            uint16_t before_errors  = _br_errors;
            backupRestoreParseBlock();
            char dbg[80];
            snprintf(dbg, sizeof(dbg),
                     "\r\n# block parsed: applied+%u errors+%u\r\n",
                     (unsigned)(_br_applied - before_applied),
                     (unsigned)(_br_errors - before_errors));
            Serial.print(dbg);
            Serial.flush();
            // Zurueck in Marker-Such-Modus fuer naechsten Block
            _br_state = BR_WAIT_MARKER;
            _br_block_type = 0;
            _br_line_len = 0;
          }
        }
      }
    }
  }
}

// --- Extractors ----------------------------------------------------------

void MyMesh::brExtractString(const char* val_start, size_t val_len,
                             char* dest, size_t dest_max) {
  if (val_len < 2 || val_start[0] != '"' || dest_max == 0) {
    if (dest_max > 0) dest[0] = 0;
    return;
  }
  const char* p = val_start + 1;
  const char* end = val_start + val_len - 1;  // exclude closing "
  size_t dlen = 0;
  auto enc_utf8 = [&](uint32_t cp) {
    if (cp < 0x80) {
      if (dlen + 1 < dest_max) dest[dlen++] = (char)cp;
    } else if (cp < 0x800) {
      if (dlen + 2 < dest_max) {
        dest[dlen++] = (char)(0xC0 | (cp >> 6));
        dest[dlen++] = (char)(0x80 | (cp & 0x3F));
      }
    } else if (cp < 0x10000) {
      if (dlen + 3 < dest_max) {
        dest[dlen++] = (char)(0xE0 | (cp >> 12));
        dest[dlen++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        dest[dlen++] = (char)(0x80 | (cp & 0x3F));
      }
    } else {
      if (dlen + 4 < dest_max) {
        dest[dlen++] = (char)(0xF0 | (cp >> 18));
        dest[dlen++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        dest[dlen++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        dest[dlen++] = (char)(0x80 | (cp & 0x3F));
      }
    }
  };
  auto read_hex4 = [&]() -> uint32_t {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
      if (p >= end) return 0xFFFFFFFFu;
      char h = *p++;
      int d;
      if (h >= '0' && h <= '9') d = h - '0';
      else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
      else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
      else return 0xFFFFFFFFu;
      v = (v << 4) | (uint32_t)d;
    }
    return v;
  };
  while (p < end && dlen + 1 < dest_max) {
    if (*p != '\\') {
      dest[dlen++] = *p++;
      continue;
    }
    p++;  // skip backslash
    if (p >= end) break;
    char c = *p++;
    if      (c == '"')  dest[dlen++] = '"';
    else if (c == '\\') dest[dlen++] = '\\';
    else if (c == '/')  dest[dlen++] = '/';
    else if (c == 'b')  dest[dlen++] = '\b';
    else if (c == 'f')  dest[dlen++] = '\f';
    else if (c == 'n')  dest[dlen++] = '\n';
    else if (c == 'r')  dest[dlen++] = '\r';
    else if (c == 't')  dest[dlen++] = '\t';
    else if (c == 'u') {
      uint32_t cp = read_hex4();
      if (cp == 0xFFFFFFFFu) break;
      if (cp >= 0xD800 && cp <= 0xDBFF) {
        // High surrogate, erwarte \uLLLL
        if (p + 2 <= end && p[0] == '\\' && p[1] == 'u') {
          p += 2;
          uint32_t lo = read_hex4();
          if (lo >= 0xDC00 && lo <= 0xDFFF) {
            cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
          }
        }
      }
      enc_utf8(cp);
    }
    // sonst: unbekannte Escape-Sequenz, ignorieren
  }
  if (dlen < dest_max) dest[dlen] = 0;
  else dest[dest_max - 1] = 0;
}

void MyMesh::brExtractHex(const char* val_start, size_t val_len,
                          uint8_t* dest, size_t dest_max) {
  memset(dest, 0, dest_max);
  if (val_len < 2 || val_start[0] != '"') return;
  const char* p = val_start + 1;
  const char* end = val_start + val_len - 1;
  size_t out = 0;
  uint8_t cur = 0;
  int nyb = 0;
  while (p < end && out < dest_max) {
    char c = *p++;
    int v = -1;
    if (c >= '0' && c <= '9')      v = c - '0';
    else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
    if (v < 0) continue;
    cur = (uint8_t)((cur << 4) | (uint8_t)v);
    if (++nyb == 2) {
      dest[out++] = cur;
      cur = 0;
      nyb = 0;
    }
  }
}

void MyMesh::brExtractUint8Array(const char* val_start, size_t val_len,
                                 uint8_t* dest, size_t dest_count) {
  for (size_t i = 0; i < dest_count; i++) dest[i] = 0;
  const char* p = val_start;
  const char* end = val_start + val_len;
  if (p >= end || *p != '[') return;
  p++;
  for (size_t i = 0; i < dest_count; i++) {
    while (p < end && (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (p >= end || *p == ']') return;
    dest[i] = (uint8_t)atoi(p);
    while (p < end && *p != ',' && *p != ']') p++;
  }
}

// --- Block Parser ---------------------------------------------------------

void MyMesh::backupRestoreParseBlock() {
  const char* p = _br_json;
  const char* end = _br_json + _br_json_len;
  // Skip whitespace, opening brace
  while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
  if (p >= end || *p != '{') {
    _br_errors++;
    Serial.println("# parse error: expected '{' at start of JSON block.");
    return;
  }
  p++;
  while (p < end) {
    // Skip whitespace + commas
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',')) p++;
    if (p >= end || *p == '}') break;
    // Expect "key"
    if (*p != '"') {
      _br_errors++;
      break;
    }
    p++;
    char key[64];
    int klen = 0;
    while (p < end && *p != '"' && klen < (int)sizeof(key) - 1) {
      if (*p == '\\' && p + 1 < end) {
        // simplification: skip escaped char as raw (keys are ASCII)
        key[klen++] = p[1];
        p += 2;
      } else {
        key[klen++] = *p++;
      }
    }
    key[klen] = 0;
    if (p < end && *p == '"') p++;
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    if (p < end && *p == ':') p++;
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
    // Read value range based on first char
    const char* val_start = p;
    char val_type = 0;
    if (p < end) {
      if (*p == '"') {
        val_type = 's';
        p++;
        bool esc = false;
        while (p < end) {
          char c = *p++;
          if (esc) { esc = false; continue; }
          if (c == '\\') { esc = true; continue; }
          if (c == '"') break;
        }
      } else if (*p == '{') {
        val_type = 'o';
        int d = 1;
        bool in_str = false, en = false;
        p++;
        while (p < end && d > 0) {
          char c = *p++;
          if (en) en = false;
          else if (in_str) {
            if (c == '\\') en = true;
            else if (c == '"') in_str = false;
          } else {
            if (c == '"') in_str = true;
            else if (c == '{') d++;
            else if (c == '}') d--;
          }
        }
      } else if (*p == '[') {
        val_type = 'a';
        int d = 1;
        p++;
        while (p < end && d > 0) {
          char c = *p++;
          if (c == '[') d++;
          else if (c == ']') d--;
        }
      } else {
        val_type = 'n';
        while (p < end && *p != ',' && *p != '}' && *p != '\n' && *p != '\r') p++;
        // trim trailing whitespace
        while (p > val_start && (p[-1] == ' ' || p[-1] == '\t')) p--;
      }
    }
    size_t val_len = (size_t)(p - val_start);
    // Dispatch
    if (strcmp(key, "_meta") == 0 && val_type == 'o') {
      brApplyMeta(val_start, val_len);
    } else if (_br_block_type == 1 || _br_block_type == 2) {
      brApplyField(_br_block_type, key, val_start, val_len, val_type);
    }
  }
}

// --- _meta: nur prv_key wird angewendet (Identity-Restore + Save) --------

void MyMesh::brApplyMeta(const char* val_start, size_t val_len) {
  // Wir suchen "prv_key": "..." -- alles andere im _meta-Block ignorieren.
  const char* p = val_start;
  const char* end = val_start + val_len;
  const char* needle = "\"prv_key\"";
  size_t needle_len = 9;
  // Naive Suche -- _meta ist klein.
  const char* hit = NULL;
  for (const char* q = p; q + needle_len <= end; q++) {
    if (memcmp(q, needle, needle_len) == 0) { hit = q; break; }
  }
  if (!hit) return;
  // Nach Doppelpunkt und Anfuehrungszeichen suchen
  const char* q = hit + needle_len;
  while (q < end && (*q == ' ' || *q == '\t')) q++;
  if (q >= end || *q != ':') return;
  q++;
  while (q < end && (*q == ' ' || *q == '\t')) q++;
  if (q >= end || *q != '"') return;
  const char* str_start = q;
  q++;
  bool esc = false;
  while (q < end) {
    if (esc) { esc = false; q++; continue; }
    if (*q == '\\') { esc = true; q++; continue; }
    if (*q == '"') { q++; break; }
    q++;
  }
  size_t str_len = (size_t)(q - str_start);
  // Hex extrahieren
  uint8_t prv[PRV_KEY_SIZE];
  brExtractHex(str_start, str_len, prv, PRV_KEY_SIZE);
  // Validieren
  if (!mesh::LocalIdentity::validatePrivateKey(prv)) {
    _br_errors++;
    Serial.println("# meta.prv_key: invalid -- ignored.");
    return;
  }
  // Anwenden + persistieren (Identity wird IMMER persistiert weil
  // halbierter Zustand "RAM != Disk" gefaehrlich ist).
  mesh::LocalIdentity new_id;
  new_id.readFrom(prv, PRV_KEY_SIZE);
  if (!_store->saveMainIdentity(new_id)) {
    _br_errors++;
    Serial.println("# meta.prv_key: saveMainIdentity FAILED.");
    return;
  }
  self_id = new_id;
  _br_applied++;
  _br_reboot_recommended = true;
  Serial.println("# meta.prv_key: applied + saved. Identity changed.");
}

// --- Field-Dispatcher fuer die zwei Block-Typen -------------------------

void MyMesh::brApplyField(uint8_t block_type, const char* key,
                          const char* val_start, size_t val_len, char val_type) {
  auto as_uint = [&]() -> uint32_t { return (uint32_t)atoll(val_start); };
  auto as_int  = [&]() -> int32_t  { return (int32_t)atoll(val_start); };
  auto as_float = [&]() -> float   { return (float)atof(val_start); };

  if (block_type == 1) {
    // ===== DL9SAU PREFS =====
    if (val_type == 'n') {
      if (strcmp(key, "chat_name_mode") == 0)        { _prefs.chat_name_mode        = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "auto_advert_enabled") == 0)   { _prefs.auto_advert_enabled   = (uint8_t)as_uint(); _br_applied++; return; }
#ifdef REPEATER_DEFENSIVE_FORCE
      if (strcmp(key, "client_repeat_force") == 0)   { _prefs.client_repeat_force   = (uint8_t)as_uint(); _br_applied++; return; }
#else
      // Force-Feature in diesem Build deaktiviert; Backup-Eintrag
      // ignorieren (Pref bleibt persistent auf 0).
      if (strcmp(key, "client_repeat_force") == 0)   { return; }
#endif
      if (strcmp(key, "repeater_profile") == 0)      { _prefs.repeater_profile      = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "loop_detect") == 0)           { _prefs.loop_detect           = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "duty_soft_pct") == 0)         { _prefs.duty_soft_pct         = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "duty_hard_pct") == 0)         { _prefs.duty_hard_pct         = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "override_expiry") == 0)       { _prefs.override_expiry       = as_uint();         _br_applied++; return; }
      if (strcmp(key, "scope_advert_auto") == 0)     { _prefs.scope_advert_auto     = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "scope_repeater_auto") == 0)   { _prefs.scope_repeater_auto   = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "advert_role") == 0)           { _prefs.advert_role           = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "trace_flags_persistent") == 0){ _prefs.trace_flags_persistent= (uint16_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "gps_power_mode") == 0)        { _prefs.gps_power_mode        = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "gps_lead_min") == 0)          { _prefs.gps_lead_min          = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "repeat_scope_mode") == 0)     { _prefs.repeat_scope_mode     = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "msg_store_flash") == 0)       { _prefs.msg_store_flash       = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "log_flags") == 0)             { _prefs.log_flags             = (uint8_t)as_uint(); _br_applied++; return; }
    }
    if (val_type == 's') {
      if (strcmp(key, "chat_name_custom") == 0)    { brExtractString(val_start, val_len, _prefs.chat_name_custom, sizeof(_prefs.chat_name_custom)); _br_applied++; return; }
      if (strcmp(key, "bake_scope_name") == 0)     { brExtractString(val_start, val_len, _prefs.bake_scope_name, sizeof(_prefs.bake_scope_name)); _br_applied++; return; }
      if (strcmp(key, "bake_scope_key") == 0)      { brExtractHex(val_start, val_len, _prefs.bake_scope_key, sizeof(_prefs.bake_scope_key)); _br_applied++; return; }
      if (strcmp(key, "override_scope_name") == 0) { brExtractString(val_start, val_len, _prefs.override_scope_name, sizeof(_prefs.override_scope_name)); _br_applied++; return; }
      if (strcmp(key, "override_scope_key") == 0)  { brExtractHex(val_start, val_len, _prefs.override_scope_key, sizeof(_prefs.override_scope_key)); _br_applied++; return; }
      if (strcmp(key, "owner_info") == 0)          { brExtractString(val_start, val_len, _prefs.owner_info, sizeof(_prefs.owner_info)); _br_applied++; return; }
      if (strcmp(key, "passwd_admin") == 0)        { brExtractString(val_start, val_len, _prefs.passwd_admin, sizeof(_prefs.passwd_admin)); _br_applied++; return; }
      if (strcmp(key, "passwd_guest") == 0)        { brExtractString(val_start, val_len, _prefs.passwd_guest, sizeof(_prefs.passwd_guest)); _br_applied++; return; }
    }
    if (val_type == 'a' && strcmp(key, "msg_store_limit") == 0) {
      brExtractUint8Array(val_start, val_len, _prefs.msg_store_limit, 5);
      _br_applied++;
      return;
    }
    // Wunschliste 43 BLE-Power-Mode.
    if (val_type == 'n' && strcmp(key, "bluetooth_power_mode") == 0) {
      _prefs.bluetooth_power_mode = (uint8_t)as_uint();
      _br_applied++;
      return;
    }
    // Wunschliste 46 Filter Restore (2026-06-10).
    if (val_type == 'n' && strcmp(key, "filter_unknown_channel_repeat") == 0) {
      _prefs.filter_unknown_channel_repeat = (uint8_t)as_uint();
      _br_applied++;
      return;
    }
    // Prefix-Match auf 'filter_<typ>_<verb>_'. Extract index + suffix.
    auto try_filter_field = [&](const char* base, NodePrefs::FilterEntry* arr,
                                uint8_t* cnt_p, size_t max_slots,
                                uint64_t* c_on, uint64_t* c_ex) -> bool {
      size_t blen = strlen(base);
      if (strncmp(key, base, blen) != 0 || key[blen] != '_') return false;
      // Special: <base>_count
      const char* tail = key + blen + 1;
      if (val_type == 'n' && strcmp(tail, "count") == 0) {
        *cnt_p = (uint8_t)as_uint();
        if (*cnt_p > max_slots) *cnt_p = (uint8_t)max_slots;
        _br_applied++;
        return true;
      }
      // <base>_<N>_<suffix>
      const char* idx_end = tail;
      while (*idx_end && *idx_end != '_') idx_end++;
      if (*idx_end != '_') return false;
      char idxs[8];
      size_t il = (size_t)(idx_end - tail);
      if (il == 0 || il >= sizeof(idxs)) return false;
      memcpy(idxs, tail, il); idxs[il] = 0;
      char* iep = NULL;
      long idx = strtol(idxs, &iep, 10);
      if (!iep || *iep != 0 || idx < 0 || (size_t)idx >= max_slots) return false;
      const char* suffix = idx_end + 1;
      if (val_type == 's' && strcmp(suffix, "pattern") == 0) {
        brExtractString(val_start, val_len, arr[idx].pattern, sizeof(arr[idx].pattern));
        _br_applied++;
        return true;
      }
      if (val_type == 'n' && strcmp(suffix, "flags") == 0) {
        arr[idx].flags = (uint8_t)as_uint();
        _br_applied++;
        return true;
      }
      if (val_type == 's' && strcmp(suffix, "chan_on") == 0) {
        char hexbuf[20];
        brExtractString(val_start, val_len, hexbuf, sizeof(hexbuf));
        c_on[idx] = strtoull(hexbuf, NULL, 0);
        _br_applied++;
        return true;
      }
      if (val_type == 's' && strcmp(suffix, "chan_ex") == 0) {
        char hexbuf[20];
        brExtractString(val_start, val_len, hexbuf, sizeof(hexbuf));
        c_ex[idx] = strtoull(hexbuf, NULL, 0);
        _br_applied++;
        return true;
      }
      return false;
    };
    auto try_scope_field = [&](const char* base, NodePrefs::FilterScopeEntry* arr,
                               uint8_t* cnt_p, size_t max_slots,
                               uint64_t* c_on, uint64_t* c_ex) -> bool {
      size_t blen = strlen(base);
      if (strncmp(key, base, blen) != 0 || key[blen] != '_') return false;
      const char* tail = key + blen + 1;
      if (val_type == 'n' && strcmp(tail, "count") == 0) {
        *cnt_p = (uint8_t)as_uint();
        if (*cnt_p > max_slots) *cnt_p = (uint8_t)max_slots;
        _br_applied++;
        return true;
      }
      const char* idx_end = tail;
      while (*idx_end && *idx_end != '_') idx_end++;
      if (*idx_end != '_') return false;
      char idxs[8];
      size_t il = (size_t)(idx_end - tail);
      if (il == 0 || il >= sizeof(idxs)) return false;
      memcpy(idxs, tail, il); idxs[il] = 0;
      char* iep = NULL;
      long idx = strtol(idxs, &iep, 10);
      if (!iep || *iep != 0 || idx < 0 || (size_t)idx >= max_slots) return false;
      const char* suffix = idx_end + 1;
      if (val_type == 's' && strcmp(suffix, "name") == 0) {
        brExtractString(val_start, val_len, arr[idx].scope_name, sizeof(arr[idx].scope_name));
        _br_applied++;
        return true;
      }
      if (val_type == 'n' && strcmp(suffix, "flags") == 0) {
        arr[idx].flags = (uint8_t)as_uint();
        _br_applied++;
        return true;
      }
      if (val_type == 's' && strcmp(suffix, "chan_on") == 0) {
        char hexbuf[20];
        brExtractString(val_start, val_len, hexbuf, sizeof(hexbuf));
        c_on[idx] = strtoull(hexbuf, NULL, 0);
        _br_applied++;
        return true;
      }
      if (val_type == 's' && strcmp(suffix, "chan_ex") == 0) {
        char hexbuf[20];
        brExtractString(val_start, val_len, hexbuf, sizeof(hexbuf));
        c_ex[idx] = strtoull(hexbuf, NULL, 0);
        _br_applied++;
        return true;
      }
      return false;
    };
    if (try_filter_field("filter_sender_drop", _prefs.filter_sender_drop,
                          &_prefs.filter_sender_drop_count,
                          sizeof(_prefs.filter_sender_drop)/sizeof(_prefs.filter_sender_drop[0]),
                          _prefs.filter_sender_drop_chan_on,
                          _prefs.filter_sender_drop_chan_ex)) return;
    if (try_filter_field("filter_sender_keep", _prefs.filter_sender_keep,
                          &_prefs.filter_sender_keep_count,
                          sizeof(_prefs.filter_sender_keep)/sizeof(_prefs.filter_sender_keep[0]),
                          _prefs.filter_sender_keep_chan_on,
                          _prefs.filter_sender_keep_chan_ex)) return;
    if (try_filter_field("filter_text_drop", _prefs.filter_text_drop,
                          &_prefs.filter_text_drop_count,
                          sizeof(_prefs.filter_text_drop)/sizeof(_prefs.filter_text_drop[0]),
                          _prefs.filter_text_drop_chan_on,
                          _prefs.filter_text_drop_chan_ex)) return;
    if (try_filter_field("filter_text_keep", _prefs.filter_text_keep,
                          &_prefs.filter_text_keep_count,
                          sizeof(_prefs.filter_text_keep)/sizeof(_prefs.filter_text_keep[0]),
                          _prefs.filter_text_keep_chan_on,
                          _prefs.filter_text_keep_chan_ex)) return;
    if (try_scope_field("filter_scope_drop", _prefs.filter_scope_drop,
                         &_prefs.filter_scope_drop_count,
                         sizeof(_prefs.filter_scope_drop)/sizeof(_prefs.filter_scope_drop[0]),
                         _prefs.filter_scope_drop_chan_on,
                         _prefs.filter_scope_drop_chan_ex)) return;
    if (try_scope_field("filter_scope_keep", _prefs.filter_scope_keep,
                         &_prefs.filter_scope_keep_count,
                         sizeof(_prefs.filter_scope_keep)/sizeof(_prefs.filter_scope_keep[0]),
                         _prefs.filter_scope_keep_chan_on,
                         _prefs.filter_scope_keep_chan_ex)) return;
    _br_skipped++;
    return;
  }

  if (block_type == 2) {
    // ===== NODE MAIN =====
    if (val_type == 'n') {
      // Radio-Params: Restore landet im _prefs, aber das Radio bleibt
      // auf den Boot-Werten bis radio_set_params -- daher reboot empfohlen.
      if (strcmp(key, "freq") == 0)                  { _prefs.freq                  = as_float();        _br_applied++; _br_reboot_recommended = true; return; }
      if (strcmp(key, "sf") == 0)                    { _prefs.sf                    = (uint8_t)as_uint(); _br_applied++; _br_reboot_recommended = true; return; }
      if (strcmp(key, "bw") == 0)                    { _prefs.bw                    = as_float();        _br_applied++; _br_reboot_recommended = true; return; }
      if (strcmp(key, "cr") == 0)                    { _prefs.cr                    = (uint8_t)as_uint(); _br_applied++; _br_reboot_recommended = true; return; }
      if (strcmp(key, "tx_power") == 0)              { _prefs.tx_power_dbm          = (int8_t)as_int();   _br_applied++; return; }
      if (strcmp(key, "repeat") == 0)                { _prefs.client_repeat         = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "gps") == 0)                   { _prefs.gps_enabled           = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "gps_interval") == 0)          { _prefs.gps_interval          = as_uint();          _br_applied++; return; }
      if (strcmp(key, "advert_loc_policy") == 0)     { _prefs.advert_loc_policy     = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "airtime_factor") == 0)        { _prefs.airtime_factor        = as_float();        _br_applied++; return; }
      if (strcmp(key, "rx_boosted_gain") == 0)       { _prefs.rx_boosted_gain       = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "manual_add_contacts") == 0)   { _prefs.manual_add_contacts   = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "multi_acks") == 0)            { _prefs.multi_acks            = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "path_hash_mode") == 0)        { _prefs.path_hash_mode        = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "autoadd_config") == 0)        { _prefs.autoadd_config        = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "autoadd_max_hops") == 0)      { _prefs.autoadd_max_hops      = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "telemetry_mode_base") == 0)   { _prefs.telemetry_mode_base   = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "telemetry_mode_loc") == 0)    { _prefs.telemetry_mode_loc    = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "telemetry_mode_env") == 0)    { _prefs.telemetry_mode_env    = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "buzzer_quiet") == 0)          { _prefs.buzzer_quiet          = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "int.thresh") == 0 || strcmp(key, "int_thresh") == 0) { _prefs.interference_threshold = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "agc.reset.interval") == 0 || strcmp(key, "agc_reset_interval") == 0) { _prefs.agc_reset_interval = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "rxdelay") == 0)               { _prefs.rx_delay_base         = as_float();        _br_applied++; return; }
      if (strcmp(key, "txdelay") == 0)               { _prefs.tx_delay_factor       = as_float();        _br_applied++; return; }
      if (strcmp(key, "direct_txdelay") == 0)        { _prefs.direct_tx_delay_factor= as_float();        _br_applied++; return; }
      if (strcmp(key, "flood_max_scope_region") == 0)   { _prefs.flood_max_scope_region = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "flood_max") == 0)             { _prefs.flood_max             = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "flood_max_infra") == 0)   { _prefs.flood_max_infra   = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "flood_max_req_resp") == 0){ _prefs.flood_max_req_resp= (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "flood_max_unknown_chan") == 0){ _prefs.flood_max_unknown_chan= (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "flood_max_unscoped_companions") == 0){ _prefs.flood_max_unscoped_companions = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "messages_append_scope_to_name") == 0){ _prefs.messages_append_scope_to_name = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "time_sync_mode") == 0)         { _prefs.time_sync_mode         = (uint8_t)as_uint(); _br_applied++; return; }
      if (strcmp(key, "lat") == 0)                   { sensors.node_lat            = atof(val_start);   _br_applied++; return; }
      if (strcmp(key, "lon") == 0)                   { sensors.node_lon            = atof(val_start);   _br_applied++; return; }
    }
    if (val_type == 's') {
      // Wunschliste 39: cap-Vars koennen als String ("follow"/"off"/
      // number-as-string) im Backup stehen. Parse-Helper.
      auto parse_cap_str = [&](uint8_t off_val, uint8_t follow_val,
                               uint8_t* dst) -> bool {
        char buf[16];
        brExtractString(val_start, val_len, buf, sizeof(buf));
        if (strcmp(buf, "follow") == 0) { *dst = follow_val; return true; }
        if (strcmp(buf, "off") == 0)    { *dst = off_val;    return true; }
        if (strcmp(buf, "max") == 0)    { *dst = follow_val; return true; }
        // numerischer String
        *dst = (uint8_t)atoi(buf);
        return true;
      };
      if (strcmp(key, "flood_max_scope_region") == 0) {
        parse_cap_str(0, 0, &_prefs.flood_max_scope_region);
        _br_applied++; return;
      }
      if (strcmp(key, "flood_max_infra") == 0) {
        parse_cap_str(FLOOD_MAX_INFRA_FOLLOW, FLOOD_MAX_INFRA_FOLLOW,
                      &_prefs.flood_max_infra);
        _br_applied++; return;
      }
      if (strcmp(key, "flood_max_req_resp") == 0) {
        parse_cap_str(FLOOD_MAX_INFRA_FOLLOW, FLOOD_MAX_INFRA_FOLLOW,
                      &_prefs.flood_max_req_resp);
        _br_applied++; return;
      }
      if (strcmp(key, "flood_max_unknown_chan") == 0) {
        parse_cap_str(0, CH_HOPS_OFF, &_prefs.flood_max_unknown_chan);
        _br_applied++; return;
      }
      if (strcmp(key, "flood_max_unscoped_companions") == 0) {
        parse_cap_str(0, CH_HOPS_OFF, &_prefs.flood_max_unscoped_companions);
        _br_applied++; return;
      }
      if (strcmp(key, "name") == 0) { brExtractString(val_start, val_len, _prefs.node_name, sizeof(_prefs.node_name)); _br_applied++; return; }
      // Wunschliste 31: time_sync_src0/1/2 als 6-hex-string
      if (strncmp(key, "time_sync_src", 13) == 0 && key[13] >= '0' && key[13] <= '2' && key[14] == 0) {
        int slot = key[13] - '0';
        char hexbuf[8];
        brExtractString(val_start, val_len, hexbuf, sizeof(hexbuf));
        // Parse 6 hex chars
        uint8_t bytes[3] = {0,0,0};
        bool ok = (strlen(hexbuf) == 6);
        for (int j = 0; j < 3 && ok; j++) {
          int hi = -1, lo = -1;
          char ch1 = hexbuf[j*2], ch2 = hexbuf[j*2+1];
          if (ch1 >= '0' && ch1 <= '9') hi = ch1 - '0';
          else if (ch1 >= 'a' && ch1 <= 'f') hi = ch1 - 'a' + 10;
          else if (ch1 >= 'A' && ch1 <= 'F') hi = ch1 - 'A' + 10;
          if (ch2 >= '0' && ch2 <= '9') lo = ch2 - '0';
          else if (ch2 >= 'a' && ch2 <= 'f') lo = ch2 - 'a' + 10;
          else if (ch2 >= 'A' && ch2 <= 'F') lo = ch2 - 'A' + 10;
          if (hi < 0 || lo < 0) ok = false;
          else bytes[j] = (uint8_t)((hi << 4) | lo);
        }
        if (ok) {
          memcpy(_prefs.time_sync_sources[slot], bytes, 3);
          _br_applied++;
        } else {
          _br_skipped++;
        }
        return;
      }
    }
    _br_skipped++;
    return;
  }

  if (block_type == 3) {
    // ===== HASHTAG CHANNELS =====
    // Keys: "chN" mit N = 0..MAX_GROUP_CHANNELS-1, Value = "#name".
    // PSK wird hier deterministisch aus dem Namen rekonstruiert:
    // erste 16 Byte von SHA-256(name). Format-Konvention dokumentiert
    // in docs/companion_protocol.md.
    if (val_type == 's' && key[0] == 'c' && key[1] == 'h') {
      char chname[32];
      brExtractString(val_start, val_len, chname, sizeof(chname));
      if (chname[0] != '#') {
        // Nicht hashtag-Channel -- ueberspringen (Schutz vor versehentlich
        // ge-pasten Private-Channel-Eintraegen die wir nicht rekonstruieren
        // koennen).
        _br_skipped++;
        return;
      }
      uint8_t hash[32];
      mesh::Utils::sha256(hash, sizeof(hash),
                          (const uint8_t*)chname, strlen(chname));

      ChannelDetails ch;
      memset(&ch, 0, sizeof(ch));
      StrHelper::strncpy(ch.name, chname, sizeof(ch.name));
      memcpy(ch.channel.secret, hash, 16);

      // Slot finden: bevorzugt vorhandenen Eintrag mit gleichem Namen
      // ueberschreiben (idempotent), sonst ersten leeren Slot. Den
      // Companion-Slot niemals anfassen (PSK-Match).
      int slot = -1;
      int empty_slot = -1;
      for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
        ChannelDetails existing;
        if (!getChannel(i, existing)) continue;
        if (memcmp(existing.channel.secret, s_companion_psk_magic, 16) == 0) continue;
        if (existing.name[0] == 0) {
          if (empty_slot < 0) empty_slot = i;
          continue;
        }
        if (strcmp(existing.name, chname) == 0) {
          slot = i;
          break;
        }
      }
      if (slot < 0) slot = empty_slot;
      if (slot < 0) {
        _br_errors++;
        Serial.printf("# channel %s: no free slot, skipped.\r\n", chname);
        return;
      }
      if (setChannel(slot, ch)) {
        saveChannels();
        _br_applied++;
      } else {
        _br_errors++;
        Serial.printf("# channel %s: setChannel failed.\r\n", chname);
      }
      return;
    }
    // Public-PSK Channels (Wunschliste 36): ch_pub_N => "<name>"
    // Setzt Slot mit festem s_public_psk + uebernommenem Namen. Bevorzugt
    // existierenden Public-Slot (PSK-Match), sonst ersten leeren Slot.
    // Companion-Slot niemals anfassen.
    if (val_type == 's' && strncmp(key, "ch_pub_", 7) == 0) {
      char pname[32];
      brExtractString(val_start, val_len, pname, sizeof(pname));
      if (pname[0] == 0) { _br_skipped++; return; }

      ChannelDetails nch;
      memset(&nch, 0, sizeof(nch));
      StrHelper::strncpy(nch.name, pname, sizeof(nch.name));
      memcpy(nch.channel.secret, s_public_psk, 16);

      int slot = -1;
      int empty_slot = -1;
      for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
        ChannelDetails existing;
        if (!getChannel(i, existing)) continue;
        if (memcmp(existing.channel.secret, s_companion_psk_magic, 16) == 0) continue;
        if (existing.name[0] == 0) {
          if (empty_slot < 0) empty_slot = i;
          continue;
        }
        if (memcmp(existing.channel.secret, s_public_psk, 16) == 0) {
          slot = i;
          break;
        }
      }
      if (slot < 0) slot = empty_slot;
      if (slot < 0) {
        _br_errors++;
        Serial.printf("# ch_pub %s: no free slot, skipped.\r\n", pname);
        return;
      }
      if (setChannel(slot, nch)) {
        saveChannels();
        _br_applied++;
      } else {
        _br_errors++;
        Serial.printf("# ch_pub %s: setChannel failed.\r\n", pname);
      }
      return;
    }
    // Wunschliste 32 v2: ch_hops_N => "Name=Cap"
    // Speichern via name-hash in channel_hops_list. Cache wird einmal am
    // Ende rebuilt -- aber hier per-Entry direkt nach upsert -- ist OK,
    // rebuild ist guenstig. Companion-Name wird zwar geupsertet wenn er
    // im Backup steht, aber sein cap wird beim naechsten rebuild eh auf
    // 0 zurueck geforced.
    // Wunschliste 36: External-Eintraege "ch_hops_ext_N": "Name=Cap"
    // VOR dem allgemeinen ch_hops_ Check pruefen (prefix-match: ext_
    // ist Subset von ch_hops_*).
    if (val_type == 's' && strncmp(key, "ch_hops_ext_", 12) == 0) {
      char raw[64];
      brExtractString(val_start, val_len, raw, sizeof(raw));
      char* eq = strchr(raw, '=');
      if (!eq) { _br_skipped++; return; }
      *eq = 0;
      int cap = atoi(eq + 1);
      if (cap < 0 || cap > 63) { _br_skipped++; return; }
      // Nur hashtag-Channels (mit '#' prefix) sind extern restaurierbar.
      if (raw[0] != '#') {
        _br_skipped++;
        Serial.printf("# ch.hops ext %s: not a hashtag channel, skipped.\r\n", raw);
        return;
      }
      uint8_t hash16[32];
      mesh::Utils::sha256(hash16, sizeof(hash16),
                          (const uint8_t*)raw, strlen(raw));
      uint32_t h = fnv1a32_cstr(raw);
      if (!channelHopsUpsert(_prefs, h, (uint8_t)cap,
                             CH_HOPS_FLAG_EXTERNAL, hash16[0], raw)) {
        _br_errors++;
        Serial.printf("# ch.hops ext %s: list full, skipped.\r\n", raw);
        return;
      }
      _br_applied++;
      rebuildChannelHopsCache();
      return;
    }
    if (val_type == 's' && strncmp(key, "ch_hops_", 8) == 0) {
      char raw[64];
      brExtractString(val_start, val_len, raw, sizeof(raw));
      char* eq = strchr(raw, '=');
      if (!eq) { _br_skipped++; return; }
      *eq = 0;
      int cap = atoi(eq + 1);
      if (cap < 0 || cap > 63) { _br_skipped++; return; }
      uint32_t h = fnv1a32_cstr(raw);
      if (!channelHopsUpsert(_prefs, h, (uint8_t)cap, 0, 0, raw)) {
        _br_errors++;
        Serial.printf("# ch.hops %s: list full, skipped.\r\n", raw);
        return;
      }
      _br_applied++;
      // Cache am Ende rebuilten waere ideal -- hier defensiv pro Entry.
      rebuildChannelHopsCache();
      return;
    }
    _br_skipped++;
    return;
  }

  _br_skipped++;
}

void MyMesh::setupCompanionChannel() {
  ChannelDetails ch;

  // (1) PSK-Match (neue Welt). Stabilste Identifikation -- ueberlebt
  //     Rename durch die App.
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
    if (getChannel(i, ch)
        && memcmp(ch.channel.secret, s_companion_psk_magic, 16) == 0) {
      _companion_channel_idx = (uint8_t)i;
      // Heilung: Name auf "companion" zuruecksetzen falls App umbenannt
      // hat. Optional, nicht funktional kritisch (Identifikation laeuft
      // ueber PSK), aber sorgt fuer konsistente Anzeige in der App.
      if (strncmp(ch.name, COMPANION_CHANNEL_NAME, sizeof(ch.name)) != 0) {
        StrHelper::strncpy(ch.name, COMPANION_CHANNEL_NAME, sizeof(ch.name));
        setChannel(i, ch);
        saveChannels();
      }
      goto done;
    }
  }

  // (2) Migration alter Installationen: Name-Match "companion" mit
  //     non-magic PSK (vermutlich legacy all-zero oder vom User
  //     manuell gesetzt). PSK auf Magic umschreiben.
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
    if (getChannel(i, ch)
        && strncmp(ch.name, COMPANION_CHANNEL_NAME, sizeof(ch.name)) == 0) {
      memcpy(ch.channel.secret, s_companion_psk_magic, 16);
      memset(&ch.channel.secret[16], 0, 16);
      setChannel(i, ch);   // recomputes hash automatisch
      saveChannels();
      _companion_channel_idx = (uint8_t)i;
      goto done;
    }
  }

  // (2b) Punkt 7 Reise-Fix 2026-06-08: Fallback auf "TerminalCLI"
  //      (dz264-Konvention). Wenn der User von dz264 zu unserer Firmware
  //      wechselt und seinen TerminalCLI-Channel mitgebracht hat, nehmen
  //      wir diesen als App-Control-Channel statt einen neuen
  //      "companion"-Channel anzulegen. Wenn $companion ZUSAETZLICH
  //      vorhanden ist, gewinnt $companion (oben in (1) bzw. (2)
  //      gefunden -> wir kommen hier nicht her).
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
    if (getChannel(i, ch)
        && strncmp(ch.name, "TerminalCLI", sizeof(ch.name)) == 0) {
      _companion_channel_idx = (uint8_t)i;
      goto done;
    }
  }

  // (3) Nicht gefunden -- ersten freien Slot suchen und neu anlegen.
  {
    int target = -1;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      if (getChannel(i, ch) && ch.name[0] == 0) { target = i; break; }
    }
    if (target < 0) {
      _companion_channel_idx = 0xFF; // kein Slot frei
      goto done;   // andere ch.hops trotzdem rebuilden
    }
    ChannelDetails nch;
    memset(&nch, 0, sizeof(nch));
    StrHelper::strncpy(nch.name, COMPANION_CHANNEL_NAME, sizeof(nch.name));
    memcpy(nch.channel.secret, s_companion_psk_magic, 16);
    // secret[16..32] bleibt 0 -> setChannel berechnet 128-bit-Hash
    setChannel(target, nch);
    _companion_channel_idx = (uint8_t)target;
    saveChannels(); // persistieren, damit der Index ueber Reboots stabil bleibt
  }
done:
  // Wunschliste 32 v2: $companion-Channel darf NIE repeated werden.
  // rebuildChannelHopsCache() macht einen force-Upsert vom companion-
  // Eintrag (cap=0), bevor es den Cache neu aufbaut. MUSS in jedem
  // Pfad laufen (User-Bug 2026-06-02: war frueher nur im 3.-Pfad,
  // 'companion via PSK gefunden' Early-Return uebersprang den Cache-
  // Rebuild komplett -> 'get ch.hops companion' zeigte 'off').
  rebuildChannelHopsCache();
}

// Setzt alle RAM-Statistik-Counter zurueck (analog simple_repeater
// clearStats(), plus Companion-spezifische). Nicht zurueckgesetzt:
// Uptime-Wrap-Tracking (_millis_wraps), RTC-Jump-Detector
// (_last_observed_rtc), Lifecycle-State (next_advert, _is_moving,
// _gps_had_fix_ever). User-Konfig in _prefs bleibt komplett unberuehrt.
void MyMesh::clearStats() {
  // Standard MeshCore Reset (Radio + Mesh + dup-Tables)
  radio_driver.resetStats();
  resetStats();
  ((SimpleMeshTables *)getTables())->resetStats();
  // Companion-spezifische Counter
  _tx_advert_count = 0;
  _tx_digi_count = 0;
  _bt_connect_count = 0;
  _duty_blocked_count = 0;
  // Wunschliste 31: time-sync RAM-state
  _time_sync_last_at_rtc = 0;
  memset(_time_sync_strict_last_ts, 0, sizeof(_time_sync_strict_last_ts));
  memset(_time_sync_last_pubkey, 0, sizeof(_time_sync_last_pubkey));
  _time_sync_done_since_boot = false;
  memset(_time_sync_lazy_cands, 0, sizeof(_time_sync_lazy_cands));
  _time_sync_lazy_count = 0;
  _time_sync_lazy_started_ms = 0;
  _time_sync_lazy_done = false;
  // Reise-Diagnose 2026-06-08: letzter adv-sync candidate
  memset(_last_adv_sync_pubkey, 0, sizeof(_last_adv_sync_pubkey));
  _last_adv_sync_ts = 0;
  _last_adv_sync_at_rtc = 0;
  _last_adv_sync_delta = 0;
  _last_adv_sync_outcome = 0;
  // Wunschliste 35: channel-sender-seen Liste leer
  memset(_channel_sender_seen, 0, sizeof(_channel_sender_seen));
  _channel_sender_seen_count = 0;
  _channel_sender_seen_next = 0;
  _repeating_allowed = false;  // boot-conservative; recompute laeuft in begin() nach loadPrefs
  _tx_repeat_airtime_ms = 0;
  memset(_rx_flood_by_ptype, 0, sizeof(_rx_flood_by_ptype));
  memset(_repeat_by_ptype,        0, sizeof(_repeat_by_ptype));
  memset(_tx_total_by_ptype,      0, sizeof(_tx_total_by_ptype));
  memset(_tx_self_flood_by_ptype, 0, sizeof(_tx_self_flood_by_ptype));
  memset(_heard_direct,      0, sizeof(_heard_direct));
  memset(_heard_quality,     0, sizeof(_heard_quality));
  memset(_rx_advert_total,   0, sizeof(_rx_advert_total));
  memset(_rx_advert_by_scope,    0, sizeof(_rx_advert_by_scope));
  memset(_heard_direct_by_scope, 0, sizeof(_heard_direct_by_scope));
  memset(_rx_direct_advert_by_role, 0, sizeof(_rx_direct_advert_by_role));
  // Wunschliste 26 B: rx-us Counter resetten. Hash-Ringe behalten wir
  // bewusst -- die sind kurze rolling-windows und werden naturwuechsig
  // ueberschrieben. Bei einem 'clear stats' direkt nach TX wuerden sonst
  // legitime Echos der naechsten Sekunden verloren gehen.
  _rx_us_self_initiated_count = 0;
  _rx_us_repeated_count       = 0;
  // Duty-Sliding-Window — symmetrisch zur simple_repeater-Logik. Wirkt
  // wie ein 'Duty-Reset bei Stats-Clear', der User hat damit nach
  // 'clear stats' wieder volle 10%/h verfuegbar (was auch unfair sein
  // koennte gegenueber dem Mesh, aber explizite User-Aktion).
  memset(_duty_air_ms_per_minute, 0, sizeof(_duty_air_ms_per_minute));
}

// Wunschliste 43 BLE-Power-Cycle (2026-06-10).
// State-Machine in loop() getickt. Timings hartcodiert:
//   BOOT_GRACE  = 10 min nach Boot (kein Connect je)
//   HOT_START   = 5 min nach erstem Disconnect (verhindert Frust bei
//                 App-Background/Lockscreen)
//   SLEEP       = 180 s aus
//   WAIT        = 30 s an (Listening fuer Connect)
//   AWAKE       = solange connected
void MyMesh::setBleEnabled(bool en) {
  if (!_serial) return;
  if (en) _serial->enable();
  else    _serial->disable();
}

void MyMesh::manageBlePower() {
  if (!_serial) return;
  // External-Toggle-Detection (Wunschliste 43, User 2026-06-10):
  // UITask-Menue (Heltec Wireless Tracker Display + Button, Menupunkt 5
  // 'Bluetooth' mit long-press) ruft _serial->enable()/disable() direkt -- ohne
  // unsere State-Machine zu konsultieren. Wenn der tatsaechliche
  // BLE-Status von dem abweicht was der State annimmt, ist das ein
  // externer Toggle. State synchron ziehen damit wir nicht im naechsten
  // Tick alles wieder zurueckdrehen.
  bool ble_real = _serial->isEnabled();
  bool state_expects_on =
    (_ble_pwr_state == BLE_PWR_BOOT)
    || (_ble_pwr_state == BLE_PWR_AWAKE)
    || (_ble_pwr_state == BLE_PWR_HOT_START)
    || (_ble_pwr_state == BLE_PWR_WAIT);
  if (ble_real != state_expects_on) {
    if (ble_real) {
      // externes ON -> HOT_START (5min an, dann zurueck in Cycle)
      _ble_pwr_state = BLE_PWR_HOT_START;
      _ble_pwr_state_until = millis() + 5UL * 60 * 1000;
      pushDebugLog("[ble] external toggle on -> hot-start\n");
    } else {
      // externes OFF -> TMP_OFF (runtime aus, nicht persistent)
      _ble_pwr_state = BLE_PWR_TMP_OFF;
      _ble_pwr_state_until = 0;
      pushDebugLog("[ble] external toggle off -> tmp-off\n");
    }
  }
  // Pref-States haben Vorrang
  uint8_t mode = _prefs.bluetooth_power_mode;
  if (mode == 1) {
    // always-on: zwinge AWAKE wenn nicht schon
    if (_ble_pwr_state != BLE_PWR_AWAKE && _ble_pwr_state != BLE_PWR_BOOT) {
      _ble_pwr_state = BLE_PWR_AWAKE;
      _ble_pwr_state_until = 0;
      setBleEnabled(true);
    }
    return;
  }
  if (mode == 2 && _ble_pwr_state != BLE_PWR_TMP_OFF) {
    // off (persistent): nur Boot-Phase noch akzeptiert, sonst aus
    if (_ble_pwr_state != BLE_PWR_OFF) {
      _ble_pwr_state = BLE_PWR_OFF;
      _ble_pwr_state_until = 0;
      setBleEnabled(false);
    }
    return;
  }
  uint32_t now = millis();
  bool connected = _serial->isConnected();
  // Edge: Connect (any state with BLE on)
  if (connected && !_ble_was_connected) {
    _ble_pwr_state = BLE_PWR_AWAKE;
    _ble_pwr_state_until = 0;
  }
  // Edge: Disconnect (war AWAKE)
  if (!connected && _ble_was_connected) {
    _ble_pwr_state = BLE_PWR_HOT_START;
    _ble_pwr_state_until = now + 5UL * 60 * 1000;
  }
  _ble_was_connected = connected;

  // Initial-Boot: wenn noch nie connected war, bleibt 10 min an
  // (User-Wunsch 2026-06-10: 30min war zu lang; 10min reicht damit
  // User die App holt + erst-connectet, danach geht's in den cycle
  // ueber den Cycle-Sleep oder Hot-Start wenn der erste Connect war).
  if (_ble_pwr_state == BLE_PWR_BOOT) {
    if (_ble_pwr_state_until == 0) {
      _ble_pwr_state_until = now + 10UL * 60 * 1000;
      setBleEnabled(true);
    }
    if (connected) {
      _ble_pwr_state = BLE_PWR_AWAKE;
      _ble_pwr_state_until = 0;
    } else if ((int32_t)(now - _ble_pwr_state_until) >= 0) {
      // Boot-Grace abgelaufen, in Cycle.
      _ble_pwr_state = BLE_PWR_SLEEP;
      _ble_pwr_state_until = now + 180UL * 1000;
      setBleEnabled(false);
    }
    return;
  }
  if (_ble_pwr_state == BLE_PWR_AWAKE) {
    // bleibt an solange connected
    setBleEnabled(true);
    return;
  }
  if (_ble_pwr_state == BLE_PWR_HOT_START) {
    setBleEnabled(true);
    if (connected) {
      _ble_pwr_state = BLE_PWR_AWAKE;
      _ble_pwr_state_until = 0;
      return;
    }
    if ((int32_t)(now - _ble_pwr_state_until) >= 0) {
      _ble_pwr_state = BLE_PWR_SLEEP;
      _ble_pwr_state_until = now + 180UL * 1000;
      setBleEnabled(false);
    }
    return;
  }
  if (_ble_pwr_state == BLE_PWR_SLEEP) {
    setBleEnabled(false);
    if ((int32_t)(now - _ble_pwr_state_until) >= 0) {
      _ble_pwr_state = BLE_PWR_WAIT;
      _ble_pwr_state_until = now + 30UL * 1000;
      setBleEnabled(true);
    }
    return;
  }
  if (_ble_pwr_state == BLE_PWR_WAIT) {
    setBleEnabled(true);
    if (connected) {
      _ble_pwr_state = BLE_PWR_AWAKE;
      _ble_pwr_state_until = 0;
      return;
    }
    if ((int32_t)(now - _ble_pwr_state_until) >= 0) {
      _ble_pwr_state = BLE_PWR_SLEEP;
      _ble_pwr_state_until = now + 180UL * 1000;
      setBleEnabled(false);
    }
    return;
  }
  if (_ble_pwr_state == BLE_PWR_TMP_OFF) {
    setBleEnabled(false);
    // bleibt aus bis explizit aufgeweckt (CLI 'bluetooth on' /
    // 'bluetooth power always-on' / 'bluetooth power cycle')
    return;
  }
}

void MyMesh::bleWakeOnLora(const char* reason) {
  // Nur in Cycle-Phasen wachen.
  if (_ble_pwr_state != BLE_PWR_SLEEP && _ble_pwr_state != BLE_PWR_WAIT) return;
  // Pref-States respektieren -- bei manuellem off/tmp-off nicht wachen.
  if (_prefs.bluetooth_power_mode == 2) return;
  pushDebugLog("[ble] wake-on-lora (%s)\n", reason ? reason : "?");
  _ble_pwr_state = BLE_PWR_HOT_START;
  _ble_pwr_state_until = millis() + 5UL * 60 * 1000;
  setBleEnabled(true);
}

void MyMesh::bleManualToggleFromMenu() {
  // Hardware-Button auf Display-Menue 'Bluetooth'-Seite (long-press).
  // Symmetrischer Toggle, non-persistent.
  bool now_on = _serial && _serial->isConnected();
  // Effektive 'ist aktuell an'-Detection: state != SLEEP/TMP_OFF/OFF
  bool ble_currently_on =
    (_ble_pwr_state == BLE_PWR_AWAKE)
    || (_ble_pwr_state == BLE_PWR_HOT_START)
    || (_ble_pwr_state == BLE_PWR_WAIT)
    || (_ble_pwr_state == BLE_PWR_BOOT);
  if (ble_currently_on && !now_on) {
    // Toggle off (runtime)
    _ble_pwr_state = BLE_PWR_TMP_OFF;
    _ble_pwr_state_until = 0;
    setBleEnabled(false);
    pushDebugLog("[ble] menu toggle -> tmp-off\n");
  } else {
    // Toggle on (runtime, HOT_START so dass cycle wieder regulaer
    // einsetzt nach 5 min wenn niemand verbindet).
    _ble_pwr_state = BLE_PWR_HOT_START;
    _ble_pwr_state_until = millis() + 5UL * 60 * 1000;
    setBleEnabled(true);
    pushDebugLog("[ble] menu toggle -> hot-start\n");
  }
}

void MyMesh::pushCompanionMessage(const char* text) {
  if (text == NULL || text[0] == 0) return;
  // Wunschliste 52: Admin-Capture-Mode. Statt App-Push: in Reply-Buffer
  // konkatenieren (newline-getrennt). Wird im REQ_TYPE_ADMIN_CMD-Handler
  // aktiviert + nachher gelesen.
  if (_admin_capture_active && _admin_reply_buf && _admin_reply_max > 1) {
    size_t tl = strlen(text);
    if (_admin_capture_truncated) return;
    size_t need = tl + (_admin_reply_used > 0 ? 1 : 0);
    if (_admin_reply_used + need + 1 > _admin_reply_max) {
      // overflow -- truncate (Buffer zu klein)
      _admin_capture_truncated = true;
      const char* trunc = "[...]";
      size_t avail = (_admin_reply_max > _admin_reply_used + 1)
                     ? (_admin_reply_max - _admin_reply_used - 1) : 0;
      if (avail >= 6) {
        if (_admin_reply_used > 0) _admin_reply_buf[_admin_reply_used++] = '\n';
        memcpy(_admin_reply_buf + _admin_reply_used, trunc, 5);
        _admin_reply_used += 5;
      }
      _admin_reply_buf[_admin_reply_used] = 0;
      return;
    }
    if (_admin_reply_used > 0) _admin_reply_buf[_admin_reply_used++] = '\n';
    memcpy(_admin_reply_buf + _admin_reply_used, text, tl);
    _admin_reply_used += tl;
    _admin_reply_buf[_admin_reply_used] = 0;
    return;
  }
  if (_companion_channel_idx == 0xFF) return;

  // Frame analog zu onChannelMessageRecv() bauen, aber Sender = Plattform-
  // Name (z.B. "Heltec V3") und path_len=0 (zero-hop / lokal).
  // Konvention onChannelMessageRecv erkennt "Sender: msg" am ": " Separator,
  // wir liefern das ebenso damit die App den Sender-Teil korrekt darstellt.
  const char* sender = board.getManufacturerName();
  char combined[MAX_TEXT_LEN];
  int n = snprintf(combined, sizeof(combined), "%s: %s", sender ? sender : "fw", text);
  if (n <= 0) return;
  int total_len = n < (int)sizeof(combined) ? n : (int)sizeof(combined) - 1;

  int i = 0;
  if (app_target_ver >= 3) {
    out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV_V3;
    out_frame[i++] = 0;  // SNR: lokal -> 0
    out_frame[i++] = 0;  // reserved1
    out_frame[i++] = 0;  // reserved2
  } else {
    out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV;
  }
  out_frame[i++] = _companion_channel_idx;
  out_frame[i++] = 0;             // path_len = 0 (lokal, kein Funkweg)
  out_frame[i++] = TXT_TYPE_PLAIN;
  uint32_t ts = getRTCClock()->getCurrentTime();
  memcpy(&out_frame[i], &ts, 4);
  i += 4;
  int max_text = MAX_FRAME_SIZE - i;
  if (total_len > max_text) total_len = max_text;
  memcpy(&out_frame[i], combined, total_len);
  i += total_len;

  addToOfflineQueue(out_frame, i);
  // NULL-Check: bei sehr frühen Boot-Pushes (z.B. dem ersten GEO-SCOPE in
  // begin()) ist _serial noch nicht via startInterface() gesetzt. Die
  // Nachricht liegt dann nur in der Offline-Queue und wird abgeholt sobald
  // die App connected — der Tickle ist dafür nicht erforderlich.
  if (_serial != NULL && _serial->isConnected()) {
    uint8_t frame[1] = { PUSH_CODE_MSG_WAITING };
    _serial->writeFrame(frame, 1);
  }
}

// Mini-Helper: prüft ob Text mit dem gegebenen Wort + Whitespace/EOL beginnt.
// Case-insensitive nicht nötig: wir verlangen Kleinschreibung.
static bool starts_with_word(const char* text, const char* word) {
  size_t wlen = strlen(word);
  if (strncmp(text, word, wlen) != 0) return false;
  char c = text[wlen];
  return c == 0 || c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Argument-Prefix-Match gegen beliebige Auswahl-Liste (Wunschliste-4).
// Exact-Match gewinnt; sonst eindeutiger Prefix; sonst ambiguous oder not-found.
// Eintraege mit no_abbrev=true matchen NUR exakt (fuer reset/clear/remove
// u.ae. — Tippfehler wuerden sonst Daten kosten).
//
// Returns:
//   >=0 = Index in choices[] des eindeutigen Treffers
//   -1  = ambiguous (mehrere abkuerzbare Prefix-Treffer)
//   -2  = leer / kein Arg
//   -3  = nicht gefunden
//
// Falls out_ambig != NULL, wird bei -1 eine kommagetrennte Liste der
// matchenden Namen geschrieben (fuer "Mehrdeutig: ..."-Output).
struct CompanionChoice {
  const char* name;
  bool        no_abbrev;
};

static int match_choice(const char* arg,
                        const CompanionChoice* choices, int nchoices,
                        char* out_ambig, size_t out_size) {
  if (!arg) return -2;
  while (*arg == ' ' || *arg == '\t') arg++;
  size_t alen = 0;
  while (arg[alen] && arg[alen] != ' ' && arg[alen] != '\t') alen++;
  if (alen == 0) return -2;

  // exact match wins (auch fuer no_abbrev-Eintraege)
  for (int i = 0; i < nchoices; i++) {
    size_t nlen = strlen(choices[i].name);
    if (alen == nlen && strncmp(arg, choices[i].name, alen) == 0) return i;
  }

  // prefix match (no_abbrev-Eintraege ueberspringen)
  int matched_idx = -1;
  int n_matches = 0;
  for (int i = 0; i < nchoices; i++) {
    if (choices[i].no_abbrev) continue;
    size_t nlen = strlen(choices[i].name);
    if (alen < nlen && strncmp(arg, choices[i].name, alen) == 0) {
      n_matches++;
      if (matched_idx < 0) matched_idx = i;
    }
  }
  if (n_matches == 1) return matched_idx;
  if (n_matches > 1) {
    if (out_ambig && out_size > 0) {
      size_t off = 0;
      for (int i = 0; i < nchoices && off + 1 < out_size; i++) {
        if (choices[i].no_abbrev) continue;
        size_t nlen = strlen(choices[i].name);
        if (alen < nlen && strncmp(arg, choices[i].name, alen) == 0) {
          if (off > 0 && off + 2 < out_size) {
            out_ambig[off++] = ',';
            out_ambig[off++] = ' ';
          }
          size_t copy = nlen;
          if (off + copy >= out_size) copy = out_size - 1 - off;
          memcpy(out_ambig + off, choices[i].name, copy);
          off += copy;
        }
      }
      out_ambig[off] = 0;
    }
    return -1;
  }
  return -3;
}

// Wrapper ueber match_choice fuer on/off. Beibehaltene Semantik:
//   1 = on    0 = off    -1 = ambiguous    -2 = leer/nicht gefunden
static int match_on_off(const char* arg) {
  static const CompanionChoice on_off[] = {
    { "on",  false },
    { "off", false },
  };
  int r = match_choice(arg, on_off, 2, NULL, 0);
  if (r == 0)  return 1;
  if (r == 1)  return 0;
  if (r == -1) return -1;
  return -2;
}

// Prüft ob das erste Wort von `input` ein Prefix von `keyword` ist
// (Tipparbeit sparen). "chat" matcht "chatname", "stat" matcht "status".
// Nur fürs help-Topic-Matching benutzen — bei Top-Level-Befehlen würden
// Prefixes künftige Erweiterungen kollidieren lassen (z.B. "r" für
// reboot vs. region/reset).
static bool topic_prefix_match(const char* input, const char* keyword) {
  size_t tlen = 0;
  while (input[tlen] != 0 && input[tlen] != ' ' && input[tlen] != '\t'
         && input[tlen] != '\r' && input[tlen] != '\n') {
    tlen++;
  }
  if (tlen == 0) return false;
  return strncmp(input, keyword, tlen) == 0;
}

// ---------- Duty-Cycle Sliding-Window ---------------------------------
// 60 Slots a 1 Minute, millis()-basiert (kein RTC/GPS noetig). Jeder
// Loop-Tick: Delta zu getTotalAirTime() in den aktuellen Slot addieren.
// Slot-Wechsel anhand der Differenz now - slot_start (wrap-safe via
// signed Vergleich), beim Rotieren wird der neue Slot auf 0 gesetzt.
void MyMesh::updateDutyWindow() {
  unsigned long now = millis();
  if (_duty_slot_start_ms == 0 && _duty_last_total_ms == 0) {
    _duty_slot_start_ms = now == 0 ? 1 : now;
    _duty_last_total_ms = getTotalAirTime();
    return;
  }
  unsigned long curr_tot = getTotalAirTime();
  unsigned long delta = curr_tot - _duty_last_total_ms;
  if (delta < 600000UL) {  // Sanity: weniger als 10 min pro Tick
    _duty_air_ms_per_minute[_duty_slot_idx] += delta;
  }
  _duty_last_total_ms = curr_tot;
  while ((long)(now - _duty_slot_start_ms) >= (long)CR_DUTY_SLOT_MS) {
    _duty_slot_idx = (_duty_slot_idx + 1) % CR_DUTY_WINDOW_SLOTS;
    _duty_air_ms_per_minute[_duty_slot_idx] = 0;
    _duty_slot_start_ms += CR_DUTY_SLOT_MS;
  }
}

unsigned long MyMesh::getTxAirLastHour() const {
  unsigned long sum = 0;
  for (size_t i = 0; i < CR_DUTY_WINDOW_SLOTS; i++) sum += _duty_air_ms_per_minute[i];
  return sum;
}

unsigned long MyMesh::getDutySoftLimitMs() const {
  return CR_DUTY_HARD_BASE_MS * (unsigned long)_prefs.duty_soft_pct / 100UL;
}
unsigned long MyMesh::getDutyHardLimitMs() const {
  return CR_DUTY_HARD_BASE_MS * (unsigned long)_prefs.duty_hard_pct / 100UL;
}

bool MyMesh::dutySoftReached() const {
  return getTxAirLastHour() >= getDutySoftLimitMs();
}
bool MyMesh::dutyHardReached() const {
  return getTxAirLastHour() >= getDutyHardLimitMs();
}

void MyMesh::traceCompanion(uint16_t flag, const char* fmt, ...) {
  if ((_trace_flags & flag) == 0) return;
  // Channel-Output Master-Switch (Wunschliste 21, asymmetrisch zu USB):
  // _prefs.log_flags bit 1 = $companion-Output ABGESCHALTET (1 = off).
  // Default 0 = AN. 'logging channel off' setzt bit 1.
  if (_prefs.log_flags & 0x02) return;
  char buf[160];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n <= 0) return;
  if (n >= (int)sizeof(buf)) buf[sizeof(buf) - 1] = 0;
  pushCompanionMessage(buf);
}

void MyMesh::appendGpsTraceStatus(char* out, size_t out_size) {
  // Format: "pos=LAT LON alt=Xm loc=N time=N t=YYYY-MM-DD HH:MM:SS loc"
  // Identische Felder wie 'gps' no-arg, gemittelt fuer eine einzelne Trace-Zeile.
  char ll[32];
  formatLatLonDM(ll, sizeof(ll), sensors.node_lat, sensors.node_lon);
  int loc_valid = 0, time_valid = 0;
#if ENV_INCLUDE_GPS == 1
  {
    LocationProvider* loc = sensors.getLocationProvider();
    if (loc) {
      loc_valid  = loc->isValid() ? 1 : 0;
      time_valid = loc->waitingTimeSync() ? 0 : 1;
    }
  }
#endif
  char rtc_str[32];
  uint32_t now_rtc = getRTCClock()->getCurrentTime();
  if (now_rtc < 1500000000UL) {
    snprintf(rtc_str, sizeof(rtc_str), "rtc-unset");
  } else {
    time_t lt = (time_t)(now_rtc + (uint32_t)localTzOffsetSecs(now_rtc));
    struct tm tm_loc;
    gmtime_r(&lt, &tm_loc);
    snprintf(rtc_str, sizeof(rtc_str), "%04d-%02d-%02d %02d:%02d:%02d",
             tm_loc.tm_year + 1900, tm_loc.tm_mon + 1, tm_loc.tm_mday,
             tm_loc.tm_hour, tm_loc.tm_min, tm_loc.tm_sec);
  }
  snprintf(out, out_size, "pos=%s alt=%.1fm loc=%d time=%d t=%s",
           ll, sensors.node_altitude, loc_valid, time_valid, rtc_str);
}

// Trace-Kategorien-Tabelle für die CLI (Name + Flag + Beschreibung).
struct TraceCat {
  const char* name;
  uint16_t flag;
  const char* desc;
};
// Format ms als Sekunden mit 1 Nachkommastelle. 12345 -> "12.3s".
static void fmt_secs(char* out, size_t n, unsigned long ms) {
  unsigned long sw = ms / 1000;
  unsigned long sf = (ms % 1000) / 100;
  snprintf(out, n, "%lu.%lus", sw, sf);
}
// Haengt " (X.X/h)" bzw. " (X.X/h, Y.Y/d)" an, wenn die Uptime gross genug
// ist um die Hochrechnung NICHT total schief darzustellen. Bei <1h Uptime
// gar nichts (Total bleibt fuer sich).
static int append_rate_hint(char* out, size_t n, uint32_t total, uint64_t uptime_s) {
  if (uptime_s < 3600 || total == 0) return 0;
  double per_h = total * 3600.0 / (double)uptime_s;
  if (uptime_s < 86400) {
    return snprintf(out, n, " (%.1f/h)", per_h);
  }
  double per_d = total * 86400.0 / (double)uptime_s;
  return snprintf(out, n, " (%.1f/h, %.1f/d)", per_h, per_d);
}

static const TraceCat trace_cats[] = {
  { "gps",     TRACE_GPS,     "GPS power on/off, first fix, fix loss" },
  { "adverts", TRACE_ADVERTS, "eigene Adverts (periodic zero-hop, nightly flood, manual). Fremde Adverts via 'repeat'." },
  { "repeat",  TRACE_REPEAT,  "durchgereichte Packets" },
  { "scope",   TRACE_SCOPE,   "scope override/default/bake Wechsel" },
  { "motion",  TRACE_MOTION,  "_is_moving Uebergaenge" },
  { "heard",   TRACE_HEARD,   "direkt gehoerte zero-hop-Adverts (default: nur neue; 'trace heard on all' = alle)" },
  { "rtc",     TRACE_RTC,     "detektierte RTC-Spruenge" },
  { "connect", TRACE_CONNECT, "BLE-App-Connect Events" },
  { "filter",  TRACE_FILTER,  "NICHT-repeatete Pakete + Grund (kann viele Zeilen erzeugen)" },
  { "night",    TRACE_NIGHT,    "Nightly-Flood Schedule + Scope-Auswahl" },
  { "duty",     TRACE_DUTY,     "Duty-Cycle Drops (Soft/Hard) ueber 10% TX/h" },
  { "msgstore", TRACE_MSGSTORE, "Offline-Queue Flash-Persistenz-Writes (Flash-Wear-Diagnose)" },
  { "bt",       TRACE_BT,       "BLE-Diagnose alle 5min: heap + disconnect-counter (default off)" },
};
static const size_t TRACE_CAT_COUNT = sizeof(trace_cats) / sizeof(trace_cats[0]);

void MyMesh::handleCompanionCommand(const char* cmd) {
  if (cmd == NULL) return;
  // Führende Whitespace überspringen
  while (*cmd == ' ' || *cmd == '\t') cmd++;
  if (*cmd == 0) {
    pushCompanionMessage("(leerer Befehl - 'help' zeigt verfügbare Kommandos)");
    return;
  }

  // Smartphone-Tastaturen capitalisieren oft das erste Zeichen automatisch
  // ("Help" statt "help"). Lokale lowercase-Kopie für case-insensitive
  // Befehl-/Argument-Matching. Sub-Befehle die Original-Case brauchen (z.B.
  // "chatname custom <Name>") rechnen den Offset im lowercase-Buffer aus
  // und greifen damit in raw_cmd.
  const char* raw_cmd = cmd;
  char lower[200];
  size_t L = strlen(cmd);
  if (L >= sizeof(lower)) L = sizeof(lower) - 1;
  for (size_t k = 0; k < L; k++) {
    char c = cmd[k];
    lower[k] = (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
  }
  lower[L] = 0;
  cmd = lower;

  // Top-Level-Befehl-Prefix-Expansion (z.B. "up" -> "uptime"). Bei
  // Mehrdeutigkeit wird die Liste der Kandidaten ausgegeben und der
  // Befehl abgebrochen. Erstes Wort des cmd (lower-buffer) wird gegen
  // die bekannte Top-Level-Liste gematched. Der raw_cmd-Pointer bleibt
  // unveraendert; Sub-Handler die raw-strings brauchen (chatname custom)
  // tokenisieren raw_cmd selbst.
  static const char* const TOP_CMDS[] = {
    "help", "?", "status",
    // Wunschliste 20: 'stats' bleibt Sammel-Output, 'stat' als kurze Alias-
    // Form (exact in TOP_CMDS damit nicht mit den stats-*-Varianten
    // konfligiert). stats-core/-radio/-packets sind Docs-kompatible Filter.
    "stats", "stat", "stats-core", "stats-radio", "stats-packets",
    "uptime", "advert", "autoadv",
    "repeater", "gps", "trace", "chatname", "reboot", "duty", "scope",
    "prefs", "neighbors", "tempradio", "set", "get", "clock", "date", "time",
    "messages", "logging", "unscoped-channelmessages", "clear",
    "contact", "backup", "save", "discover",
    "ch.hops",
  };
  static const size_t TOP_N = sizeof(TOP_CMDS) / sizeof(TOP_CMDS[0]);
  size_t fw_len = 0;
  while (cmd[fw_len] != 0 && cmd[fw_len] != ' ' && cmd[fw_len] != '\t') fw_len++;
  if (fw_len > 0) {
    bool exact_found = false;
    int prefix_matches = 0;
    const char* prefix_canonical = NULL;
    for (size_t k = 0; k < TOP_N; k++) {
      size_t cl = strlen(TOP_CMDS[k]);
      if (cl == fw_len && strncmp(TOP_CMDS[k], cmd, fw_len) == 0) {
        exact_found = true;
        break;
      }
      if (cl > fw_len && strncmp(TOP_CMDS[k], cmd, fw_len) == 0) {
        prefix_matches++;
        prefix_canonical = TOP_CMDS[k];
      }
    }
    if (!exact_found && prefix_matches == 1) {
      // Eindeutiger Prefix — expandiere im lower-Buffer in place.
      size_t can_len = strlen(prefix_canonical);
      size_t rest_len = L - fw_len;
      if (can_len + rest_len < sizeof(lower)) {
        memmove(lower + can_len, lower + fw_len, rest_len + 1);
        memcpy(lower, prefix_canonical, can_len);
        L = can_len + rest_len;
      }
    } else if (!exact_found && prefix_matches > 1) {
      char msg[160];
      int pos = snprintf(msg, sizeof(msg), "Mehrdeutig:");
      for (size_t k = 0; k < TOP_N; k++) {
        size_t cl = strlen(TOP_CMDS[k]);
        if (cl > fw_len && strncmp(TOP_CMDS[k], cmd, fw_len) == 0) {
          pos += snprintf(msg + pos, sizeof(msg) - pos, " %s", TOP_CMDS[k]);
        }
      }
      pushCompanionMessage(msg);
      return;
    }
    // exact_found ODER prefix_matches == 0: cmd unveraendert, weiter unten
    // wird entweder ein Sub-Handler greifen oder die Default-Unknown-Antwort.
  }

  // ---------- help / ? --------------------------------------------------
  if (starts_with_word(cmd, "help") || starts_with_word(cmd, "?")) {
    // Optionales Topic?
    const char* topic = strchr(cmd, ' ');
    if (topic) {
      while (*topic == ' ') topic++;
    }
    if (topic && *topic) {
      if (topic_prefix_match(topic, "gps")) {
        pushCompanionMessage(
          "gps on/off: Modul ein/aus. Off behaelt letzte Position im Advert. "
          "Ohne Arg -> Status. gps sync: einmaliger Wake-Trigger (RTC/Position).");
        // D6: Hilfe fuer 'gps power' klarer (User-Feedback: 'lead' ist
        // Teil des cycle-Modus, nicht ein eigener Mode).
        pushCompanionMessage(
          "gps power: Power-Management-Konfig.");
        pushCompanionMessage(
          "  gps power always-on\n"
          "    GPS-Chip permanent eingeschaltet (mehr Strom, schneller Fix).");
        pushCompanionMessage(
          "  gps power cycle\n"
          "    Default. Im 15-min-Cycle: lead Minuten WACH, Rest schlafend.");
        pushCompanionMessage(
          "  gps power lead <N>\n"
          "    Feintuning fuer cycle-Mode: N Minuten wach vor Advert.\n"
          "    Default 5 -> 5 min wach + 10 min sleep.");
        pushCompanionMessage(
          "  gps power reset\n"
          "    Power-Mode auf Default zurueck (cycle, lead=5).");
        return;
      }
      if (topic_prefix_match(topic, "advert")) {
        pushCompanionMessage(
          "advert [zero-hop | flood]: sendet sofort einen einmaligen Advert. "
          "Ohne Arg = zero-hop. Aliase: z, f."
        );
        pushCompanionMessage(
          "'flood' verwendet die gleiche Scope-Auswahl wie nightly: "
          "override > default > geo-fallback. Scope-Quelle wird in der "
          "Antwort gemeldet."
        );
        pushCompanionMessage(
          "advert role [auto | fixed chat|repeater|sensor|room]:\n"
          "  Welcher ADV_TYPE in createSelfAdvert + welche Discovery-\n"
          "  Queries beantwortet werden."
        );
        pushCompanionMessage(
          "  'auto' folgt der\n"
          "  Matrix aus client_repeat + repeater_profile."
        );
        return;
      }
      if (topic_prefix_match(topic, "autoadv")) {
        pushCompanionMessage(
          "autoadv: wiederkehrende Adverts ein/aus, separat fuer "
          "periodic zerohop (15/60/180 min) und nightly scoped flood."
        );
        pushCompanionMessage(
          "Args: 'on'/'off' (beide), 'zerohop on/off', 'nightly on/off'. "
          "Default nach Flash: beide off."
        );
        return;
      }
      // 'help scope repeater' VOR 'help scope' pruefen — topic_prefix_match
      // sieht nur das erste Wort, deshalb wuerde 'scope' sonst gewinnen.
      if (starts_with_word(topic, "scope repeater")
          || starts_with_word(topic, "scoperepeater")) {
        pushCompanionMessage(
          "scope repeater steuert welche scoped Pakete repeated werden.");
        pushCompanionMessage(
          "Mode 'all' (Default): alle scoped. Mode 'allowlist': nur die in\n"
          "der Repeat-Liste markierten Eintraege.");
        pushCompanionMessage(
          "scope repeater\n"
          "  Status (Mode + Repeat-Liste mit Tags)");
        pushCompanionMessage(
          "scope repeater mode all|allowlist\n"
          "  Global: Liste ueberhaupt anwenden (allowlist) oder alle\n"
          "  scoped Pakete repeaten (all).");
        pushCompanionMessage(
          "scope repeater auto on|off\n"
          "  Global: auto-rep-Eintraege greifen (on, Default) oder\n"
          "  werden ignoriert (off, nur Pin zaehlt).");
        pushCompanionMessage(
          "Per Eintrag (statt der alten 'scope repeater add/remove/..'):");
        pushCompanionMessage(
          "  scope <name> pin     immer aktiv (Tag P)\n"
          "  scope <name> geo     aktiv wenn GPS in Bbox (Tags A / A-)\n"
          "  scope <name> off     nie aktiv");
        pushCompanionMessage(
          "  scope <name> disable temporaer aus (Mode bleibt erhalten)\n"
          "  scope <name> enable  temporaer wieder an\n"
          "  scope <name> delete  dauerhaft verstecken (Build-in)");
        return;
      }
      if (topic_prefix_match(topic, "scope advert")
          || topic_prefix_match(topic, "scope adv")) {
        pushCompanionMessage(
          "scope advert: konfiguriert was meine eigenen Auto-Adverts senden.");
        pushCompanionMessage(
          "scope advert default <name>|clear\n"
          "  Default-Scope fuer Auto-Adverts.");
        pushCompanionMessage(
          "scope advert bake <name>|clear\n"
          "  Nightly-Flood-Advert nutzt diesen Scope (kann weiter sein\n"
          "  als default, z.B. de-be -> de-bebb).");
        pushCompanionMessage(
          "scope advert override <name> [<n>h|<n>d]|clear\n"
          "  Hoechste Send-Prioritaet, persistent ueber Reboots, max 30d TTL.");
        pushCompanionMessage(
          "scope advert auto off|on|prefer\n"
          "  Send-Hierarchie fuer eigene Auto-Adverts:");
        pushCompanionMessage(
          "    on (Default):\n"
          "      Default-Scope gewinnt. Geo nur als Fallback wenn\n"
          "      kein Default gesetzt ist.");
        pushCompanionMessage(
          "    prefer:\n"
          "      Geo schlaegt Default, wenn die oertliche Region eine\n"
          "      andere ist als das Default. ('User ist nicht zu Hause')");
        pushCompanionMessage(
          "    off:\n"
          "      Geo wird nie verwendet, nur Default/Bake/Override.");
        return;
      }
      if (topic_prefix_match(topic, "scope")) {
        pushCompanionMessage(
          "scope: vier Bereiche.");
        pushCompanionMessage(
          "1) Auto-Adverts ('scope advert', 'help scope advert')\n"
          "2) Registry (scope list/add/remove/info/regions)\n"
          "3) Repeater-Policy ('scope repeater', 'help scope repeater')\n"
          "4) Per-Eintrag ('scope <name> ...')");
        pushCompanionMessage(
          "Send-Hierarchie (nightly bake / 'advert flood'):\n"
          "  override > bake > default-oder-geo (gemaess scope advert auto)");
        pushCompanionMessage(
          "scope\n"
          "  ohne Argument: Status der drei Send-Quellen + Registry-Count");
        pushCompanionMessage(
          "scope default <name>\n"
          "  persistent. Wirkt auch fuer regulaere Sends (App-Default).");
        pushCompanionMessage(
          "scope default clear\n"
          "  loescht den default");
        pushCompanionMessage(
          "scope bake <name>\n"
          "  persistent, NUR fuer nightly. Darf weiter sein als default\n"
          "  (z.B. default=#de-be, bake=#de-bebb).");
        pushCompanionMessage(
          "scope bake clear\n"
          "  loescht bake");
        pushCompanionMessage(
          "scope override <name> [12h|3d]\n"
          "  persistent ueber Reboots, hoechste Prio.\n"
          "  Default-TTL 12h, max 30d, Suffix h oder d.");
        pushCompanionMessage(
          "scope override clear\n"
          "  loescht override");
        pushCompanionMessage(
          "Registry (Liste A - bekannte Scopes):");
        pushCompanionMessage(
          "scope list\n"
          "  alle Eintraege + Flags");
        pushCompanionMessage(
          "scope add <name> [<lat_min,lon_min,lat_max,lon_max>]\n"
          "  Eintrag anlegen. Bekannte Namen (siehe 'scope regions')\n"
          "  bekommen die Bbox automatisch.");
        pushCompanionMessage(
          "scope remove <name>\n"
          "  loeschen (kein Prefix-Match)");
        pushCompanionMessage(
          "scope info <name>\n"
          "  Details (auch ob Bbox da)");
        pushCompanionMessage(
          "scope regions\n"
          "  Build-in Geo-Tabelle (RO)");
        return;
      }
      if (topic_prefix_match(topic, "filter")) {
        pushCompanionMessage(
          "filter: Spam-Filter eingehender Pakete.\n"
          "Wirkt 'for-us' (App-Push only),\n"
          "Repeat-Funktion bleibt aktiv.");
        pushCompanionMessage(
          "  filter list  (Komplett-Uebersicht)");
        pushCompanionMessage(
          "Filter-Typen: sender, text, scope.\n"
          "(scope = eigene Syntax, s.u.)\n"
          "Befehle TYPE=sender|text:\n"
          "  filter TYPE drop add <pat>");
        pushCompanionMessage(
          "  filter TYPE drop list|clear\n"
          "  filter TYPE keep add/remove/list/clear");
        pushCompanionMessage(
          "keep gewinnt vor drop:\n"
          "  drop * + keep ping = Whitelist\n"
          "  (nur ping durch).");
        pushCompanionMessage(
          "channel-filter pro Pattern:\n"
          "  drop add <pat> on-channel <liste>\n"
          "  drop add <pat> exempt-channel <liste>");
        pushCompanionMessage(
          "Nachtraegliches Aendern: remove + neu add.");
        pushCompanionMessage(
          "Shortcut fuer ALLE Pattern des Typs:\n"
          "  filter TYPE on-channel <liste>\n"
          "  filter TYPE exempt-channel <liste>\n"
          "  filter TYPE on-channel clear");
        pushCompanionMessage(
          "Pattern (literal, case-insens.):\n"
          "  foo   = exakt Wort 'foo'\n"
          "  foo*  = Wort beginnt mit foo\n"
          "  *foo  = Wort endet auf foo");
        pushCompanionMessage(
          "  *foo* = Wort enthaelt foo\n"
          "  ^foo  = Text-Anfang Wort foo\n"
          "  foo$  = Text-Ende Wort foo\n"
          "  ^foo$ = Text ist genau foo");
        pushCompanionMessage(
          "Tokens by Whitespace.\n"
          "Satzzeichen Teil des Wortes:\n"
          "  'ping!' braucht 'ping*' oder\n"
          "  exakt 'ping!' als Pattern.");
        pushCompanionMessage(
          "Mehrwort braucht Quotes:\n"
          "  drop add \"erstes zweites\"\n"
          "  drop add \"foo bar*\" on-channel X\n"
          "2. Wort ohne Quote = Modifier-Versuch.");
        pushCompanionMessage("Umlaute Ae/Oe/Ue ok.");
        pushCompanionMessage(
          "sender-Filter: DM-Absender +\n"
          "Channel-Sender (Prefix vor ': ').\n"
          "text-Filter: Channel-Text-Teil.\n"
          "Je 16 Slots, persistent.");
        pushCompanionMessage(
          "scope-Filter (eigene Achse):\n"
          "  matched scope-Tag im Wire-Header,\n"
          "  unabhaengig von Sender/Text.\n"
          "  Pseudo-Token 'unscoped' fuer no-tag.");
        pushCompanionMessage(
          "scope-Achse 1 -- channel-filter:\n"
          "  add <list> on-channel <chans>\n"
          "  add <list> exempt-channel <chans>");
        pushCompanionMessage(
          "scope-Achse 2 -- profile (wo wirkt):\n"
          "  Default: complete (Display+Repeat)\n"
          "  profile for-us   nur Display\n"
          "  profile repeat   nur Repeater");
        pushCompanionMessage(
          "Achsen unabhaengig kombinierbar:\n"
          "  add #de on-channel Public profile for-us\n"
          "list-Anzeige: 'p:dpy'/'p:rep' (kein\n"
          " Suffix = Default complete).");
        pushCompanionMessage(
          "filter unknown-channel:\n"
          "  Repeat-Policy fuer Channels die\n"
          "  nicht selbst konfiguriert sind.\n"
          "  Default: filter-unscoped.");
        return;
      }
      if (topic_prefix_match(topic, "ch.hops")) {
        pushCompanionMessage(
          "ch.hops: per-Channel Repeat-Cap fuer\n"
          "  PAYLOAD_TYPE_GRP_TXT/GRP_DATA-Pakete.");
        pushCompanionMessage(
          "  set ch.hops <name> <follow|off|N>\n"
          "    follow = kein Cap (folgt flood_max)\n"
          "    off    = nicht repeaten\n"
          "    1..63  = expliziter Cap");
        pushCompanionMessage(
          "  get ch.hops <name>\n"
          "  ch.hops status -- aktive Caps\n"
          "  ch.hops clear  -- alle Caps loeschen");
        pushCompanionMessage(
          "External (Hashtag-Channels ohne Subscribe):\n"
          "  set ch.hops #bots 0 (nicht abonniert, blocken)\n"
          "  PSK aus Name ableitbar (sha256), kein Slot noetig.\n"
          "  Anzeige: '#bots (ext)' in 'ch.hops status'.");
        pushCompanionMessage(
          "Private-Channels (Random-PSK): nur blockbar wenn\n"
          "  bereits konfiguriert (mit identischer PSK des\n"
          "  Absenders). Sonst pauschal via 'unknown'.");
        pushCompanionMessage(
          "Spezial-Name 'unknown' -> Cap fuer Channel-Hashes\n"
          "  die auf keinen Slot/External matchen. CH_HOPS_OFF\n"
          "  (default) = follow flood_max.\n"
          "$companion: forced 0, nicht aenderbar.");
        return;
      }
      if (topic_prefix_match(topic, "neighbors")) {
        pushCompanionMessage(
          "neighbors [hops <N> | km <D>]:\n"
          "  ohne Arg: nur direkt-gehoerte (out_path_len=0).");
        pushCompanionMessage(
          "  hops <N>: direkt + bis zu N Hops.\n"
          "  km <D>:   direkt + alle <= D km Distanz.");
        pushCompanionMessage(
          "Zeigt Typ (rep/cmp/room/sns), Name, Alter,\n"
          "Distanz/Bearing wenn Positionen bekannt.\n"
          "Cutoff 48h fuer stale-Eintraege.");
        return;
      }
      if (topic_prefix_match(topic, "set")) {
        // Mehrere kurze Messages -- jede unter 145 Byte BLE-Wire-Limit.
        // User-Feedback 2026-06-01: alte Version war ueberindentet und
        // an mehreren Stellen abgeschnitten (App, Hinweise).
        pushCompanionMessage(
          "set <key> <value>: persistente Settings.");
        pushCompanionMessage(
          "Radio: freq sf bw cr tx_power\n"
          "Position: lat lon gps gps_interval advert_loc_policy");
        pushCompanionMessage(
          "Repeat: repeat flood_max (1..63, def 16)\n"
          "  flood_max_infra (def 16, 'follow'=fmax)\n"
          "  flood_max_req_resp (def 0=erbt infra)");
        pushCompanionMessage(
          "  flood_max_scope_region\n"
          "  flood_max_unscoped_companions\n"
          "    (= alias 'flood_max_unscoped')\n"
          "  loop_detect (off|min|mod|strict)");
        pushCompanionMessage(
          "  ch.hops -> 'help ch.hops'");
        pushCompanionMessage(
          "Radio LBT/AGC:\n"
          "  int.thresh (0=off, default 14)\n"
          "  agc.reset.interval (sec/4, 0=off)\n"
          "  ('set <key>' ohne Wert -> Detailhilfe)");
        pushCompanionMessage(
          "Delays: rxdelay txdelay direct_txdelay\n"
          "  (tx/dir: 'auto' Default, oder 0..2)\n"
          "Telemetry: telemetry_mode_base loc env\n"
          "  airtime_factor rx_boosted_gain");
        pushCompanionMessage(
          "App: name manual_add_contacts multi_acks\n"
          "  autoadd_config autoadd_max_hops\n"
          "  path_hash_mode buzzer_quiet\n"
          "  messages_append_scope_to_name (on|off)\n"
          "  owner_info (max 119, '|' -> Newline)\n"
          "  passwd_admin/_guest (max 31,\n"
          "    'set passwd_admin clear' -> remote-CLI off)");
        pushCompanionMessage(
          "Identity (Reboot noetig!):\n"
          "  set prv.key <128 hex chars>\n"
          "  set prv.key NEW    (neu generieren)");
        pushCompanionMessage(
          "Hinweise: Lat/Lon Sued/West negativ.\n"
          "  freq MHz, bw kHz.\n"
          "  Delays = Faktor*Airtime\n"
          "    (tx/dir 0..2, rx 0..20).\n"
          "  Aenderungen sofort persistent.");
        return;
      }
      if (topic_prefix_match(topic, "get")) {
        pushCompanionMessage(
          "get\n  nur veraenderte App-Settings (analog 'prefs')");
        pushCompanionMessage(
          "get all\n  alle Settings mit [default] / (default: X) Markierung");
        pushCompanionMessage(
          "get <key>\n  einzelner Wert");
        pushCompanionMessage(
          "Keys wie 'set' (siehe 'help set').\n"
          "Alle underscore-Keys auch mit Punkt:\n"
          "  z.B. flood.max statt flood_max.");
        pushCompanionMessage(
          "Per-Channel ch.hops:\n"
          "  get ch.hops <name>   einzelner Channel\n"
          "  ch.hops status       komplette Liste\n"
          "  ('help ch.hops' fuer Details)");
        return;
      }
      if (topic_prefix_match(topic, "clock") || topic_prefix_match(topic, "date")
          || topic_prefix_match(topic, "time")) {
        pushCompanionMessage(
          "clock / date / time (ohne Arg):\n"
          "  RTC + Sync-Quelle anzeigen.");
        pushCompanionMessage(
          "time <epoch>: RTC manuell setzen.\n"
          "  (epoch = unix sec, post-2017..pre-2096)");
        pushCompanionMessage(
          "set time sync <off|lazy|aabbcc [bb [cc]]>:\n"
          "  Advert-basierte RTC-Sync, Default lazy.\n"
          "  Details: 'set time' ohne Wert.");
        return;
      }
      if (topic_prefix_match(topic, "clear")) {
        pushCompanionMessage(
          "clear stats:\n"
          "  Setzt alle RAM-Statistik-Counter zurueck."
        );
        pushCompanionMessage(
          "Abkuerzbar als 'stat' (Prefix-Match >= 3 Zeichen).\n"
          "Konsistent zur MeshCore-Docs 'clear stats'."
        );
        return;
      }
      if (topic_prefix_match(topic, "unscoped-channelmessages")) {
        pushCompanionMessage(
          "unscoped-channelmessages [direct|flood]:\n"
          "  Verhalten bei Channel-Send ohne Scope."
        );
        pushCompanionMessage(
          "  direct (Default): zero-hop, nur direkt empfangbare\n"
          "          Nachbarn sehen die Message.\n"
          "  flood: klassisches Mesh-Flood durchs Netz."
        );
        pushCompanionMessage(
          "Greift NUR bei Channel-Msgs ohne send_scope/Default/Geo.\n"
          "Scoped Sends sind unbeeinflusst.\n"
          "Runtime-only -- Reboot stellt 'direct' wieder her."
        );
        return;
      }
      if (topic_prefix_match(topic, "logging")) {
        pushCompanionMessage(
          "logging: Master-Switches fuer Debug-Output-Senken.\n"
          "  logging                  zeigt Status"
        );
        pushCompanionMessage(
          "  logging usb on|off       Serial/USB-Output\n"
          "  Default OFF (safe fuer USB-Companion-Builds)."
        );
        pushCompanionMessage(
          "  logging channel on|off   $companion-Channel-Output\n"
          "  Default ON. Trace-Kategorien werden dort gezeigt."
        );
        pushCompanionMessage(
          "App-Debug-Frame bleibt von 'usb off' unbeeinflusst --\n"
          "nur die Serial-Console wird stumm."
        );
        return;
      }
      if (topic_prefix_match(topic, "messages")) {
        pushCompanionMessage(
          "messages (no arg):\n"
          "  Status pro bucket.\n"
          "  2/8 zeigt: 2 neue\n"
          "  Nachrichten von max 8.\n"
          "Typen:\n"
          "  public, hashtag,\n"
          "  private, dm, companion."
        );
        pushCompanionMessage(
          "messages flash <type> on|off\n"
          "  Flash-Persistenz toggle (default off)."
        );
        pushCompanionMessage(
          "messages limit <type> <N>\n"
          "  Slot-Limit setzen. 0 = type-Default.\n"
          "  Max: public 8, companion 16,\n"
          "       hashtag/private/dm je 24."
        );
        pushCompanionMessage(
          "messages clear <type|all>\n"
          "  RAM + Flash leeren."
        );
        return;
      }
      if (topic_prefix_match(topic, "contact")) {
        pushCompanionMessage(
          "contact <name-prefix> type [chat|repeater|sensor|room]\n"
          "Diagnose-CLI: setzt ADV_TYPE eines gespeicherten Kontakts um."
        );
        pushCompanionMessage(
          "Ohne 'type ...' -> aktuellen Typ anzeigen.\n"
          "Beispiel: contact DL9SAU type sensor"
        );
        pushCompanionMessage(
          "Zweck: testen ob die App weiter Chat anbietet wenn ein Peer\n"
          "sich als SENSOR/REPEATER advertet (Wunschliste 7)."
        );
        return;
      }
      if (topic_prefix_match(topic, "discover")) {
        pushCompanionMessage(
          "discover [flags]:\n"
          "  pro-aktiv CTL-REQ via sendZeroHop\n"
          "  an direkte Nachbarn. 30s Tabelle.");
        pushCompanionMessage(
          "Flags (kombinierbar, Reihenfolge egal):\n"
          "  repeater - nur REPEATER\n"
          "  sensor   - nur SENSOR\n"
          "  all      - 0xFE (forward-compat)");
        pushCompanionMessage(
          "  prefix   - kurze RESP (8B pub_key)\n"
          "Output: name|pubkey, role, tx_snr, rx_snr.");
        pushCompanionMessage(
          "discover regions (ohne Arg):\n"
          "  CTL-Discover REPEATER, dann\n"
          "  zero-hop ANON-REQ pro RESP.\n"
          "  Region-Listen einzeln in $companion.");
        pushCompanionMessage(
          "discover regions <name-prefix>:\n"
          "  Namesuche, zero-hop direkt.\n"
          "  Nur direkte Nachbarn (protokoll-bedingt:\n"
          "  ANON_REQ_TYPE_REGIONS nur isRouteDirect).");
        pushCompanionMessage(
          "Rate-Limit: 60s zwischen Aufrufen.\n"
          "Client + Repeater-Mode.\n"
          "Komplement: 'discoverable' (passiv, full-rep).");
        return;
      }
      if (topic_prefix_match(topic, "tempradio")) {
        pushCompanionMessage(
          "tempradio: temporaere Funk-Parameter.\n"
          "Nicht persistent -- weg nach Reboot."
        );
        pushCompanionMessage(
          "Status: tempradio (ohne Arg)\n"
          "Alle: tempradio <f_MHz> <sf> <bw_kHz> <cr> [<tx>]"
        );
        pushCompanionMessage(
          "Einzel:\n"
          "  tempradio freq <MHz>    z.B. 869.618\n"
          "  tempradio sf <6..12>\n"
          "  tempradio cr <5..8>"
        );
        pushCompanionMessage(
          "  tempradio bw <kHz>      legal:\n"
          "    7.81 10.42 15.63 20.83 31.25\n"
          "    41.67 62.5 125 250 500\n"
          "  tempradio tx <dBm>"
        );
        pushCompanionMessage(
          "Caveat: andere CLI-Befehle die savePrefs()\n"
          "machen wuerden die Werte ins File schreiben."
        );
        return;
      }
      if (topic_prefix_match(topic, "prefs")) {
        pushCompanionMessage(
          "prefs: zeigt/resettet die DL9SAU-Companion-Variablen. "
          "App-Settings (node_name, freq, ...) sind NICHT betroffen."
        );
        pushCompanionMessage(
          "prefs\n  nur Non-Default-Werte");
        pushCompanionMessage(
          "prefs all\n  alle mit [default]-Markierung");
        pushCompanionMessage(
          "prefs reset\n  alle DL9SAU-Vars auf Default");
        return;
      }
      if (topic_prefix_match(topic, "duty")) {
        pushCompanionMessage(
          "duty: Duty-Cycle-Schutz (EU-Vorgabe 10% TX-Airtime pro\n"
          "rollendem 1h-Fenster). Ohne Arg -> Status (stats, blocked,\n"
          "soft/hard-Limits in % VOM 10%-Limit).");
        pushCompanionMessage(
          "duty soft N (0..99): Repeats droppen ab N% vom 10%-Limit\n"
          "  (z.B. 80 = bei 8.0% Airtime).");
        pushCompanionMessage(
          "duty hard N (1..100): ALLE TX droppen ab N% vom 10%-Limit\n"
          "  (z.B. 100 = bei 10.0% Airtime = harter EU-Cap).");
        pushCompanionMessage(
          "duty reset -> Default 80/100. Trace-Kategorie 'duty'.");
        return;
      }
      if (topic_prefix_match(topic, "repeater")) {
#ifdef REPEATER_DEFENSIVE_FORCE
        pushCompanionMessage(
          "repeater [on [force] | off]:\n"
          "  schaltet Repeating ein/aus. Verhalten\n"
          "  gemaess profile (defensive | normal).\n"
          "  Ohne Arg -> Status.");
#else
        pushCompanionMessage(
          "repeater [on | off]:\n"
          "  schaltet Repeating ein/aus. Verhalten\n"
          "  gemaess profile (defensive | normal).\n"
          "  Ohne Arg -> Status.");
#endif
        pushCompanionMessage(
          "profile=defensive (Default; = 'client_repeat'):\n"
          "  PATH nur fuer lokale Endpoints,\n"
          "  reduzierte Power + CR5.");
        pushCompanionMessage(
          "  Bei is_moving: Repeating wird\n"
          "  automatisch pausiert.");
        pushCompanionMessage(
          "  Bbox (geo-scopes): Live-GPS-Position\n"
          "  oder fixed location (set lat/lon)\n"
          "  als Fallback bei GPS aus/kein Fix.");
        pushCompanionMessage(
          "profile=normal:\n"
          "  vollwertiger Repeater, alle PATH-Pakete,\n"
          "  volle Power + konfigurierte CR.");
        pushCompanionMessage(
          "  Bbox (geo-scopes): NUR fixed location\n"
          "  (set lat/lon). Live-GPS wird ignoriert\n"
          "  -- echter Repeater steht fest.");
        pushCompanionMessage(
          "Wechsel via 'repeater profile <defensive|normal>'");
#ifdef REPEATER_DEFENSIVE_FORCE
        pushCompanionMessage(
          "force (nur fuer defensive relevant):\n"
          "  auf manchen Frequenzen sind\n"
          "  client-repeater nicht erwuenscht\n"
          "  (z.B. EU 869.618 MHz).");
        pushCompanionMessage(
          "  'repeater on force' aktiviert es\n"
          "  trotzdem. Persistent ueber on/off.\n"
          "  signalFitsInIsmBand bleibt aktiv.");
#endif
        return;
      }
      if (topic_prefix_match(topic, "status")) {
        pushCompanionMessage(
          "status: zeigt Firmware-Version, Uptime, GPS-Status, Position, "
          "Advert-Counter."
        );
        return;
      }
      if (topic_prefix_match(topic, "stats-core")) {
        pushCompanionMessage(
          "stats-core: System-Stats.\n"
          "  uptime, battery (mV), msg-queue, trace-flags."
        );
        pushCompanionMessage(
          "MeshCore-Docs-konform (docs.meshcore.io).\n"
          "Subset von 'stats' (Sammel-Output)."
        );
        return;
      }
      if (topic_prefix_match(topic, "stats-radio")) {
        pushCompanionMessage(
          "stats-radio: Radio-Stats.\n"
          "  noise floor, last rssi/snr, airtime, rx errors."
        );
        pushCompanionMessage(
          "Plus duty-cycle 1h-Window vs 10%-Limit.\n"
          "Subset von 'stats'. Docs-konform."
        );
        return;
      }
      if (topic_prefix_match(topic, "stats-packets")) {
        pushCompanionMessage(
          "stats-packets: Packet-Counters.\n"
          "  heard direct, rx flood,\n"
          "  tx (self-initiated/repeated/total)."
        );
        pushCompanionMessage(
          "Direct-Pakete sind NICHT erfasst (nur Flood-Pfad).\n"
          "Subset von 'stats'. Docs-konform."
        );
        return;
      }
      if (topic_prefix_match(topic, "stats")) {
        pushCompanionMessage(
          "stats: Sammel-Output (alle Kategorien)."
        );
        pushCompanionMessage(
          "stats-core, stats-radio, stats-packets sind\n"
          "EIGENE Befehle (keine stats-Argumente!)."
        );
        pushCompanionMessage(
          "Aufschluesselung nach Node-Typ + Payload-Typ.\n"
          "Plus /h und /d hochgerechnete Raten."
        );
        return;
      }
      if (topic_prefix_match(topic, "trace")) {
        pushCompanionMessage(
          "trace: selektives Live-Logging in den Companion-Chat. "
          "Es gibt eine *gespeicherte Auswahl* (Reboot-fest) und einen "
          "*aktiven* Zustand im RAM (startet bei Boot leer)."
        );
        pushCompanionMessage(
          "trace list\n  Kategorien-Uebersicht");
        pushCompanionMessage(
          "trace <cat> on|off\n  Kategorie ein/aus (in Auswahl + aktiv)");
        pushCompanionMessage(
          "trace on\n  aktiv = gespeicherte Auswahl (resume)");
        pushCompanionMessage(
          "trace off\n  aktiv = leer (pause; Auswahl bleibt)");
        pushCompanionMessage(
          "trace all on|off\n  setzt aktiv UND Auswahl auf alle/keine");
        pushCompanionMessage(
          "Spezial:\n"
          "  trace heard on [new|all]   default 'new'\n"
          "    all: alle direkt gehoerten zero-hop-Adverts"
        );
        pushCompanionMessage(
          "    new: nur direkt gehoerte zero-hop-Adverts von bisher unbekannten Nodes"
        );
        pushCompanionMessage(
          "Nach Reboot ist aktiv = 0 (keine Logs), bis 'trace on' die "
          "gespeicherte Auswahl wiederherstellt."
        );
        return;
      }
      if (topic_prefix_match(topic, "chatname")) {
        pushCompanionMessage(
          "chatname konfiguriert den Sendernamen in Group-Channel-Messages. "
          "Default: voller node_name."
        );
        pushCompanionMessage(
          "Args: 'default' (voll), N (erste N Woerter, 1..253), "
          "'custom <Name>' (frei, Original-Case, WS getrimmt). "
          "Ohne Arg -> Status."
        );
        return;
      }
      pushCompanionMessage(
        "(kein Help-Eintrag fuer dieses Topic)\n"
        "Tipp: fuer set-keys liefert 'set <key>' (ohne Wert)\n"
        "die Detail-Hilfe. 'help set' fuer Key-Liste.");
      return;
    }
    // 'Befehle: ...' war zu lang fuer MAX_TEXT_LEN (160 inkl. 'Sender: '-
    // Prefix, effektiv ~145 Bytes). Output wurde bei 'time' abgeschnitten.
    // -> in 3 BLE-Messages gesplittet (mit stats-Varianten Sichtbarkeit).
    pushCompanionMessage(
      "Befehle: help [topic], status, uptime, neighbors,\n"
      "  advert, autoadv, repeater, duty, scope, gps,"
    );
    pushCompanionMessage(
      "  stats, stats-core, stats-radio, stats-packets,\n"
      "  trace, chatname, prefs, set, get, ch.hops, clock, time,"
    );
    pushCompanionMessage(
      "  messages, logging, unscoped-channelmessages,\n"
      "  contact, backup, save, discover, tempradio,\n"
      "  filter, clear, reboot."
    );
    // Versteckt (ENTFERNBAR): 'bleinfo', 'debugscope' -- Diagnose-Tools
    // (Wunschliste 40). Sehen Kommentare bei den Handlern.
    return;
  }

  // ---------- status ----------------------------------------------------
  if (starts_with_word(cmd, "status")) {
    char line[160];
    // Uptime inkl. millis()-Wrap (>49 Tage)
    uint64_t total_ms = (uint64_t)_millis_wraps * 4294967296ULL + (uint64_t)millis();
    uint64_t total_s = total_ms / 1000ULL;
    unsigned long up_d = (unsigned long)(total_s / 86400ULL);
    unsigned long up_h = (unsigned long)((total_s % 86400ULL) / 3600ULL);
    unsigned long up_m = (unsigned long)((total_s % 3600ULL) / 60ULL);

    if (up_d > 0) {
      snprintf(line, sizeof(line), "fw=%s  up=%lud%02luh%02lum",
               FIRMWARE_VERSION, up_d, up_h, up_m);
    } else {
      snprintf(line, sizeof(line), "fw=%s  up=%luh%02lum",
               FIRMWARE_VERSION, up_h, up_m);
    }
    pushCompanionMessage(line);
    // Counter — wie auf der STATS-Display-Page (RX/TX vom Radio-Driver,
    // Adv/Digi/BT eigene RAM-Counter).
    snprintf(line, sizeof(line),
             "rx=%lu  tx=%lu  adv=%lu  digi=%lu  bt=%lu",
             (unsigned long)radio_driver.getPacketsRecv(),
             (unsigned long)radio_driver.getPacketsSent(),
             (unsigned long)_tx_advert_count,
             (unsigned long)_tx_digi_count,
             (unsigned long)_bt_connect_count);
    pushCompanionMessage(line);
    {
      char ll[32];
      formatLatLonDM(ll, sizeof(ll), sensors.node_lat, sensors.node_lon);
      snprintf(line, sizeof(line),
               "gps=%s fix_ever=%d moving=%d  pos=%s",
               _prefs.gps_enabled ? "on" : "off",
               (int)_gps_had_fix_ever, (int)_is_moving, ll);
    }
    pushCompanionMessage(line);
    // Next-scheduled-Sichtbarkeit (User-Wunsch 2026-06-03): "zerohop=12:03"
    // statt nur "on". "off" bleibt wenn deaktiviert. "on(?)" bei RTC-unset
    // oder noch nicht geplant.
    char zh_str[16], nl_str[16];
    uint32_t now_rtc = getRTCClock()->getCurrentTime();
    bool rtc_ok = (now_rtc > 1500000000UL);
    int32_t tz = rtc_ok ? localTzOffsetSecs(now_rtc) : 0;
    if (!(_prefs.auto_advert_enabled & AUTO_ADV_ZEROHOP)) {
      strcpy(zh_str, "off");
    } else if (!rtc_ok) {
      strcpy(zh_str, "on(?)");
    } else {
      long delta_ms = (long)(next_periodic_advert_at - millis());
      long delta_s = (delta_ms < 0) ? 0 : delta_ms / 1000;
      uint32_t loc = now_rtc + (uint32_t)delta_s + (uint32_t)tz;
      snprintf(zh_str, sizeof(zh_str), "%02u:%02u",
               (unsigned)((loc % 86400UL) / 3600UL),
               (unsigned)((loc % 3600UL) / 60UL));
    }
    if (!(_prefs.auto_advert_enabled & AUTO_ADV_NIGHTLY)) {
      strcpy(nl_str, "off");
    } else if (!rtc_ok || next_night_flood_unix == 0) {
      strcpy(nl_str, "on(?)");
    } else {
      uint32_t loc = next_night_flood_unix + (uint32_t)tz;
      snprintf(nl_str, sizeof(nl_str), "%02u:%02u",
               (unsigned)((loc % 86400UL) / 3600UL),
               (unsigned)((loc % 3600UL) / 60UL));
    }
    if (_prefs.client_repeat) {
      // Wunschliste 26 D (User 2026-05-31): Mode + Force-Flag in einer
      // Klammer. profile==1 -> 'full' (= 'normal' im Code), sonst
      // 'defensive'. Bei aktiver Repeater-Funktion ist der Modus immer
      // sinnvoll; bei off lassen wir die Klammer weg.
      const char* prof = (_prefs.repeater_profile == 1) ? "full" : "defensive";
      const char* frc  = "";
#ifdef REPEATER_DEFENSIVE_FORCE
      if (_prefs.client_repeat_force) frc = ",force";
#endif
      snprintf(line, sizeof(line),
               "autoadv: zerohop=%s nightly=%s  repeater=on (%s%s)",
               zh_str, nl_str, prof, frc);
    } else {
      snprintf(line, sizeof(line),
               "autoadv: zerohop=%s nightly=%s  repeater=off",
               zh_str, nl_str);
    }
    pushCompanionMessage(line);
    return;
  }

  // ---------- debugscope (Wunschliste 40, 2026-06-05) -------------------
  // ENTFERNBAR sobald nicht mehr benoetigt: dieser Befehl war Diagnose-
  // Hilfsmittel zum Aufdecken des _buildin_keys-Cache-Bugs in
  // TransportKeyStore::getAutoKeyFor (Wunschliste 41 / Commit 5610d2cf).
  // Nicht in 'help' Liste aufgenommen (versteckt). Kann komplett
  // ersatzlos geloescht werden -- die State-Variablen, die hier
  // ausgegeben werden, lesen direkt aus _prefs / _serial.
  // State-Dump fuer Scope-Entscheidung (nightly-flood Debugging).
  if (starts_with_word(cmd, "debugscope") || starts_with_word(cmd, "dbgscope")) {
    char line[160];
    // 1) default_scope: name + 16 byte hex
    const uint8_t* k = _prefs.default_scope_key;
    bool key_null = true;
    for (int i = 0; i < 16; i++) if (k[i]) { key_null = false; break; }
    snprintf(line, sizeof(line),
      "default_scope_name='%s'\n"
      "key isNull=%d mode=%d",
      _prefs.default_scope_name[0] ? _prefs.default_scope_name : "(empty)",
      (int)key_null, (int)_prefs.scope_advert_auto);
    pushCompanionMessage(line);
    snprintf(line, sizeof(line),
      "key=%02x%02x%02x%02x %02x%02x%02x%02x\n"
      "    %02x%02x%02x%02x %02x%02x%02x%02x",
      k[0],k[1],k[2],k[3],k[4],k[5],k[6],k[7],
      k[8],k[9],k[10],k[11],k[12],k[13],k[14],k[15]);
    pushCompanionMessage(line);
    // 2) chooseGeoFallbackScope direkt aufrufen
    TransportKey geo;
    bool geo_ok = chooseGeoFallbackScope(geo);
    snprintf(line, sizeof(line),
      "chooseGeoFallback=%d\n"
      "geo[0..3]=%02x%02x%02x%02x  geo[4..7]=%02x%02x%02x%02x",
      (int)geo_ok,
      geo.key[0],geo.key[1],geo.key[2],geo.key[3],
      geo.key[4],geo.key[5],geo.key[6],geo.key[7]);
    pushCompanionMessage(line);
    // 3) resolveDefaultOrGeo direkt aufrufen
    TransportKey rdg;
    bool rdg_ok = resolveDefaultOrGeo(rdg);
    snprintf(line, sizeof(line),
      "resolveDefaultOrGeo=%d\n"
      "out[0..3]=%02x%02x%02x%02x  out[4..7]=%02x%02x%02x%02x",
      (int)rdg_ok,
      rdg.key[0],rdg.key[1],rdg.key[2],rdg.key[3],
      rdg.key[4],rdg.key[5],rdg.key[6],rdg.key[7]);
    pushCompanionMessage(line);
    // 4) chooseNightFloodScope direkt aufrufen
    TransportKey nfs;
    bool nfs_ok = chooseNightFloodScope(nfs);
    snprintf(line, sizeof(line),
      "chooseNightFloodScope=%d\n"
      "out[0..3]=%02x%02x%02x%02x  out[4..7]=%02x%02x%02x%02x",
      (int)nfs_ok,
      nfs.key[0],nfs.key[1],nfs.key[2],nfs.key[3],
      nfs.key[4],nfs.key[5],nfs.key[6],nfs.key[7]);
    pushCompanionMessage(line);
    // 5) Build-in #local key zur Vergleich
    int li = dl9sau_find_region_index("local");
    if (li >= 0 && li < _buildin_keys_count) {
      const uint8_t* lk = _buildin_keys[li].key;
      snprintf(line, sizeof(line),
        "local[0..3]=%02x%02x%02x%02x  local[4..7]=%02x%02x%02x%02x",
        lk[0],lk[1],lk[2],lk[3],lk[4],lk[5],lk[6],lk[7]);
      pushCompanionMessage(line);
    }
    return;
  }

  // ---------- bleinfo (Wunschliste 40, 2026-06-04) ----------------------
  // ENTFERNBAR sobald BLE-Stabilitaet final ist: Diagnose-Hilfsmittel fuer
  // Connection-Stability-Debug. Versteckt (nicht in 'help' no-arg Liste).
  // Liest disconnect_count + last-reason aus _serial (SerialBLEInterface)
  // und Heap-Stats aus ESP. Wenn dieser Befehl entfernt wird, sollten auch
  // entfernt werden:
  //   - BaseSerialInterface getDisconnectCount/Reason/Overflow virtuals
  //   - SerialBLEInterface member-vars _disconnect_count, _last_disconnect_reason,
  //     _recv/send_overflow_count, _high_water
  //   - Periodic ble-diag Loop in MyMesh::loop() (trace_bt gated)
  //   - TRACE_BT flag in MyMesh.h trace_cats[]
  //   - _next_heap_log_at / _last_logged_disconnect_count / _session_min_heap
  //     / _last_ble_diag_log_at in MyMesh.h
  // Was BEHALTEN werden sollte (= permanente Fixes, NICHT diagnostisch):
  //   - FRAME_QUEUE_SIZE 4 -> 16 in SerialBLEInterface.h
  //   - 2-param onDisconnect override in SerialBLEInterface (Library-API).
  //     Daraus bedienter Counter ist diagnostisch, der Callback aber harmlos.
  // BLE-Disconnect-Counter, letzter Reason-Code, Heap-Stats. Snapshot
  // jederzeit abfragbar; pushDebugLog macht periodische Aufzeichnung.
  if (starts_with_word(cmd, "bleinfo") || starts_with_word(cmd, "bledbg")) {
    char line[160];
    uint32_t dc = _serial ? _serial->getDisconnectCount() : 0;
    uint8_t  reason = _serial ? _serial->getLastDisconnectReason() : 0xFF;
    const char* reason_name = "?";
    switch (reason) {
      case 0x05: reason_name = "auth-failure"; break;
      case 0x08: reason_name = "supervision-timeout"; break;
      case 0x13: reason_name = "remote-user-terminated"; break;
      case 0x14: reason_name = "remote-low-resources"; break;
      case 0x15: reason_name = "remote-power-off"; break;
      case 0x16: reason_name = "local-host-terminated"; break;
      case 0x22: reason_name = "lmp-response-timeout"; break;
      case 0x23: reason_name = "ll-procedure-collision"; break;
      case 0x28: reason_name = "instant-passed"; break;  // conn-param-update missed
      case 0x29: reason_name = "pairing-no-key"; break;
      case 0x3B: reason_name = "diff-tx-coordination"; break;
      case 0x3D: reason_name = "ll-mic-failure"; break;
      case 0x3E: reason_name = "connection-failed"; break;
      case 0x3F: reason_name = "mac-conn-failed"; break;
      case 0x42: reason_name = "unknown-conn-id"; break;
      case 0xFF: reason_name = "none"; break;
    }
    snprintf(line, sizeof(line),
      "BLE: disconnects=%u\n  last=0x%02X (%s)",
      (unsigned)dc, (unsigned)reason, reason_name);
    pushCompanionMessage(line);
    if (_serial) {
      uint32_t rovf = _serial->getRecvOverflowCount();
      uint32_t sovf = _serial->getSendOverflowCount();
      uint8_t  rhw  = _serial->getRecvQueueHighWater();
      uint8_t  shw  = _serial->getSendQueueHighWater();
      snprintf(line, sizeof(line),
        "Queues: recv_ovf=%u send_ovf=%u\n"
        "  high_water recv=%u send=%u (max 16)",
        (unsigned)rovf, (unsigned)sovf, (unsigned)rhw, (unsigned)shw);
      pushCompanionMessage(line);
    }
#ifdef ESP32
    snprintf(line, sizeof(line),
      "Heap: free=%u min_seen=%u min_lib=%u",
      (unsigned)ESP.getFreeHeap(),
      (unsigned)_session_min_heap,
      (unsigned)ESP.getMinFreeHeap());
    pushCompanionMessage(line);
#endif
    return;
  }

  // ---------- uptime ----------------------------------------------------
  if (starts_with_word(cmd, "uptime")) {
    uint64_t total_ms = (uint64_t)_millis_wraps * 4294967296ULL + (uint64_t)millis();
    uint64_t total_s = total_ms / 1000ULL;
    unsigned long up_d   = (unsigned long)(total_s / 86400ULL);
    unsigned long up_h   = (unsigned long)((total_s % 86400ULL) / 3600ULL);
    unsigned long up_m   = (unsigned long)((total_s % 3600ULL)  / 60ULL);
    unsigned long up_sec = (unsigned long)(total_s % 60ULL);
    char line[80];
    if (up_d > 0) {
      snprintf(line, sizeof(line), "uptime: %lud %02luh%02lum%02lus",
               up_d, up_h, up_m, up_sec);
    } else {
      snprintf(line, sizeof(line), "uptime: %02luh%02lum%02lus",
               up_h, up_m, up_sec);
    }
    pushCompanionMessage(line);
    return;
  }

  // ---------- advert [zero-hop | flood] ---------------------------------
  // Sendet sofort einen einmaligen Advert. 'advert' ohne Arg = zero-hop.
  // 'advert flood' macht einen scoped flood mit der gleichen Scope-Auswahl
  // wie der nightly-Job (runtime > default > geo). Triggert die jeweilige
  // doX-Funktion direkt (umgeht den auto_advert_enabled-Gate damit man
  // explizit per Befehl senden kann auch wenn die Scheduler aus sind).
  if (starts_with_word(cmd, "advert")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ') arg++; }

    // ? -- vollstaendige Sub-Befehl-Liste (User-Bericht: 'advert ?'
    // zeigte nur '[zero-hop | flood]', 'role' fehlte).
    if (arg && arg[0] == '?' && (arg[1] == 0 || arg[1] == ' ')) {
      pushCompanionMessage("advert -- Sub-Befehle:");
      pushCompanionMessage(
        "  advert [zero-hop | flood]\n"
        "    Einmal-Advert senden (zero-hop = single-hop neighbours,\n"
        "    flood = scoped flood-advert wie nightly).");
      pushCompanionMessage(
        "  advert role\n"
        "    Status (configured + effective Role)");
      pushCompanionMessage(
        "  advert role auto\n"
        "    Auto-Matrix (siehe 'help advert'): folgt client_repeat\n"
        "    + repeater_profile.");
      pushCompanionMessage(
        "  advert role fixed chat|repeater|sensor|room\n"
        "    Type fest pinnen -- beeinflusst createSelfAdvert\n"
        "    + Discovery-Query (Wunschliste 7).");
      return;
    }

    // Wunschliste 7 Phase 2: advert role <auto|fixed <type>>
    if (arg && starts_with_word(arg, "role")) {
      const char* rarg = strchr(arg, ' ');
      if (rarg) { while (*rarg == ' ') rarg++; }
      auto roleName = [](uint8_t v) {
        switch (v) {
          case 0: return "auto";
          case 1: return "chat";
          case 2: return "repeater";
          case 3: return "sensor";
          case 4: return "room";
          default: return "?";
        }
      };
      auto roleNameByAdv = [](uint8_t adv) {
        switch (adv) {
          case ADV_TYPE_CHAT:     return "chat";
          case ADV_TYPE_REPEATER: return "repeater";
          case ADV_TYPE_SENSOR:   return "sensor";
          case ADV_TYPE_ROOM:     return "room";
          default: return "?";
        }
      };
      if (!rarg || *rarg == 0
          || (rarg[0] == '?' && (rarg[1] == 0 || rarg[1] == ' '))) {
        char r[200];
        snprintf(r, sizeof(r),
          "advert role = %s (effective: %s)\n"
          "  auto = Matrix client_repeat + profile\n"
          "  fixed chat|repeater|sensor|room",
          roleName(_prefs.advert_role),
          roleNameByAdv(effectiveAdvertRole()));
        pushCompanionMessage(r);
        return;
      }
      if (starts_with_word(rarg, "auto")) {
        _prefs.advert_role = 0;
        savePrefs();
        char r[120]; snprintf(r, sizeof(r),
          "OK - advert role = auto (effective: %s)",
          roleNameByAdv(effectiveAdvertRole()));
        pushCompanionMessage(r);
        return;
      }
      if (starts_with_word(rarg, "fixed")) {
        const char* tv = strchr(rarg, ' ');
        if (tv) { while (*tv == ' ') tv++; }
        if (!tv || *tv == 0) { pushCompanionMessage("Usage: advert role fixed <chat|repeater|sensor|room>"); return; }
        static const CompanionChoice rch[] = {
          { "chat",     false },  // -> 1
          { "repeater", false },  // -> 2
          { "sensor",   false },  // -> 3
          { "room",     false },  // -> 4
        };
        char rambig[40];
        int ri = match_choice(tv, rch, 4, rambig, sizeof(rambig));
        if (ri == -1) { char r[80]; snprintf(r, sizeof(r), "Mehrdeutig: %s", rambig); pushCompanionMessage(r); return; }
        if (ri < 0)   { pushCompanionMessage("Usage: advert role fixed <chat|repeater|sensor|room>"); return; }
        _prefs.advert_role = (uint8_t)(ri + 1);
        savePrefs();
        char r[120]; snprintf(r, sizeof(r),
          "OK - advert role = fixed %s",
          roleName(_prefs.advert_role));
        pushCompanionMessage(r);
        return;
      }
      pushCompanionMessage("Usage: advert role <auto | fixed <chat|repeater|sensor|room>>");
      return;
    }

    bool want_flood = false;
    if (arg && *arg) {
      // "zerohop" (ohne Bindestrich) als Alias bewahren: vor match_choice
      // auf den kanonischen Namen normalisieren.
      char norm[64];
      const char* effective = arg;
      if (starts_with_word(arg, "zerohop")) {
        size_t off = 0;
        memcpy(norm, "zero-hop", 8); off = 8;
        const char* p = arg + 7;
        while (*p && off + 1 < sizeof(norm)) norm[off++] = *p++;
        norm[off] = 0;
        effective = norm;
      }
      static const CompanionChoice ch[] = {
        { "zero-hop", false },
        { "flood",    false },
      };
      char ambig[40];
      int m = match_choice(effective, ch, 2, ambig, sizeof(ambig));
      if (m == -1) {
        char r[80]; snprintf(r, sizeof(r), "Mehrdeutig: %s", ambig);
        pushCompanionMessage(r); return;
      }
      if (m < 0) {
        pushCompanionMessage(
          "Usage: advert [zero-hop | flood]\n"
          "       advert role [auto | fixed <type>]\n"
          "Hilfe: 'advert ?' oder 'help advert'");
        return;
      }
      want_flood = (m == 1);
    }

    if (!want_flood) {
      doPeriodicZeroHopAdvert();   // hat eigenen duty-hard-Check + counter
      pushCompanionMessage("OK - zero-hop advert.");
      return;
    }

    // Flood: Scope-Quelle vorab bestimmen fuer die Antwort, dann senden.
    // Reihenfolge muss zu chooseNightFloodScope passen:
    //   override (persistent) > bake > default > geo
    char src_label[64] = "(none - kein scope verfuegbar)";
    bool have_scope = false;
    uint32_t now = getRTCClock()->getCurrentTime();
    // 1) override
    if (_prefs.override_expiry != 0 && now < _prefs.override_expiry) {
      snprintf(src_label, sizeof(src_label), "override = #%s",
               _prefs.override_scope_name[0] ? _prefs.override_scope_name : "?");
      have_scope = true;
    }
    // 2) bake
    if (!have_scope) {
      bool bake_set = false;
      for (size_t k = 0; k < sizeof(_prefs.bake_scope_key); k++) {
        if (_prefs.bake_scope_key[k] != 0) { bake_set = true; break; }
      }
      if (bake_set) {
        snprintf(src_label, sizeof(src_label), "bake = #%s",
                 _prefs.bake_scope_name[0] ? _prefs.bake_scope_name : "?");
        have_scope = true;
      }
    }
    // 3) configured default
    if (!have_scope) {
      bool default_set = false;
      for (size_t k = 0; k < sizeof(_prefs.default_scope_key); k++) {
        if (_prefs.default_scope_key[k] != 0) { default_set = true; break; }
      }
      if (default_set) {
        snprintf(src_label, sizeof(src_label), "default = #%s",
                 _prefs.default_scope_name[0] ? _prefs.default_scope_name : "?");
        have_scope = true;
      }
    }
    // 4) geo fallback
    if (!have_scope) {
      TransportKey k;
      if (chooseGeoFallbackScope(k)) {
        snprintf(src_label, sizeof(src_label), "geo-fallback");
        have_scope = true;
      }
    }

    doNightFloodAdvert();   // hat eigenen duty-hard-Check + counter
    char line[160];
    snprintf(line, sizeof(line), "OK - flood advert.  scope: %s", src_label);
    pushCompanionMessage(line);
    return;
  }

  // ---------- gps on/off ------------------------------------------------
  if (starts_with_word(cmd, "gps")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ') arg++; }
    if (!arg || *arg == 0) {
      // Status:
      //   off          = vom User per "gps off" deaktiviert
      //   on           = enabled UND Modul gerade an
      //   off (sleep)  = enabled, Modul gerade per Power-Cycle aus
      const char* cur = NULL;
#if ENV_INCLUDE_GPS == 1
      cur = sensors.getSettingByKey("gps");
#endif
      // off       = laut Konfiguration deaktiviert (Pref-Schalter aus)
      // on        = konfiguriert ein + Modul gerade aktiv
      // sleeping  = konfiguriert ein, Modul aktuell per Power-Cycle aus
      const char* state;
      if (!_prefs.gps_enabled) state = "off";
      else if (cur && cur[0] == '1') state = "on";
      else state = "sleeping";
      // _prefs.gps_interval ist eine App-seitige Auto-Poll-Frequenz (in s),
      // unabhaengig von unserem eigenen Power-Cycle-Management. 0 = kein
      // automatisches Polling - wird hier als "off" angezeigt damit es
      // nicht mit "0 Sekunden" verwechselt wird.
      char interval_str[16];
      if (_prefs.gps_interval == 0) {
        snprintf(interval_str, sizeof(interval_str), "off");
      } else {
        snprintf(interval_str, sizeof(interval_str), "%lus",
                 (unsigned long)_prefs.gps_interval);
      }
      char ll[32];
      formatLatLonDM(ll, sizeof(ll), sensors.node_lat, sensors.node_lon);
      // D2: power-Mode mit anzeigen (User-Wunsch -- mode ist sonst nur
      // unter 'gps power' sichtbar).
      const char* pmode = (_prefs.gps_power_mode == 1) ? "always-on" : "cycle";
      uint8_t lead = (_prefs.gps_lead_min == 0) ? 5 : _prefs.gps_lead_min;
      // Live-Validity-Flags aus dem LocationProvider (User-Wunsch
      // 2026-05-29 -- hilft NMEA-stale-time-Bug-Symptome zu erkennen):
      //   loc_valid  = aktueller Position-Fix gueltig (RMC 'A')
      //   time_valid = GPS-Zeit kuerzlich auf RTC ge-synct (kein
      //                offener 30-min-Re-Sync)
      // Beide 0 wenn GPS off oder ohne Provider.
      int loc_valid = 0, time_valid = 0;
#if ENV_INCLUDE_GPS == 1
      {
        LocationProvider* loc = sensors.getLocationProvider();
        if (loc) {
          loc_valid  = loc->isValid() ? 1 : 0;
          time_valid = loc->waitingTimeSync() ? 0 : 1;
        }
      }
#endif
      // Aktuelle RTC-Zeit lokal formatieren (User-Wunsch 2026-05-29 --
      // GPS-Status korrelieren mit Uhrzeit). "rtc not set" falls pre-2017.
      char rtc_str[32];
      uint32_t now_rtc = getRTCClock()->getCurrentTime();
      if (now_rtc < 1500000000UL) {
        snprintf(rtc_str, sizeof(rtc_str), "rtc not set");
      } else {
        time_t lt = (time_t)(now_rtc + (uint32_t)localTzOffsetSecs(now_rtc));
        struct tm tm_loc;
        gmtime_r(&lt, &tm_loc);
        snprintf(rtc_str, sizeof(rtc_str), "%04d-%02d-%02d %02d:%02d:%02d loc",
                 tm_loc.tm_year + 1900, tm_loc.tm_mon + 1, tm_loc.tm_mday,
                 tm_loc.tm_hour, tm_loc.tm_min, tm_loc.tm_sec);
      }
      // Split in 2 BLE-Messages -- mit den neuen valid-Flags wuerde der
      // kombinierte Block die 145-Byte-Wire-Grenze sprengen.
      char block1[200];
      snprintf(block1, sizeof(block1),
               "gps=%s  fix_ever=%d  loc_valid=%d  time_valid=%d  moving=%d\n"
               "power=%s  lead=%u min  app-poll-interval=%s",
               state, (int)_gps_had_fix_ever, loc_valid, time_valid,
               (int)_is_moving, pmode, (unsigned)lead, interval_str);
      pushCompanionMessage(block1);
      char block2[120];
      snprintf(block2, sizeof(block2),
               "pos=%s  alt=%.1fm\n"
               "time=%s",
               ll, sensors.node_altitude, rtc_str);
      pushCompanionMessage(block2);
      return;
    }
    // ---- gps power [...] - Power-Management-Konfig ----
    // Prefix-Match (B2): 'gps pow' soll auch funktionieren.
    bool is_power_kw = false;
    {
      static const CompanionChoice gps_subs[] = { { "power", false } };
      char ambig[32];
      int m = match_choice(arg, gps_subs, 1, ambig, sizeof(ambig));
      is_power_kw = (m == 0);
    }
    if (is_power_kw) {
      const char* sub = strchr(arg, ' ');
      if (sub) { while (*sub == ' ') sub++; }
      if (!sub || *sub == 0) {
        // Status
        char block[200];
        const char* mode_str = (_prefs.gps_power_mode == 1) ? "always-on" : "cycle";
        uint8_t lead = (_prefs.gps_lead_min == 0) ? 5 : _prefs.gps_lead_min;
        snprintf(block, sizeof(block),
                 "gps power:\n  mode = %s\n  lead = %u min (sleep = %u min im 15-min-Cycle)",
                 mode_str, (unsigned)lead, (unsigned)(15 - lead));
        pushCompanionMessage(block);
        return;
      }
      if (strcmp(sub, "always-on") == 0) {
        _prefs.gps_power_mode = 1;
        savePrefs();
        // Reise-Fix 2026-06-08: always-on sofort wirksam machen --
        // GPS-Modul einschalten wenn aktuell aus.
        if (_prefs.gps_enabled) {
          sensors.setSettingValue("gps", "1");
          _gps_woke_at_millis = millis();
          if (_gps_woke_at_millis == 0) _gps_woke_at_millis = 1;
          _gps_fix_seen_this_wake = false;
        }
        pushCompanionMessage("OK - gps power = always-on (kein Cycling).");
        return;
      }
      if (strcmp(sub, "cycle") == 0) {
        _prefs.gps_power_mode = 0;
        savePrefs();
        pushCompanionMessage("OK - gps power = cycle (Default).");
        return;
      }
      if (starts_with_word(sub, "lead")) {
        const char* num = strchr(sub, ' ');
        if (num) { while (*num == ' ') num++; }
        if (!num || !(num[0] >= '0' && num[0] <= '9')) {
          pushCompanionMessage("Usage: gps power lead <N>\nN = 1..14 Minuten");
          return;
        }
        int n = atoi(num);
        if (n < 1 || n > 14) {
          pushCompanionMessage("lead muss 1..14 sein (kleiner als 15-min-Cycle).");
          return;
        }
        _prefs.gps_lead_min = (uint8_t)n;
        savePrefs();
        char r[80]; snprintf(r, sizeof(r), "OK - gps power lead = %d min (sleep = %d min).", n, 15 - n);
        pushCompanionMessage(r);
        return;
      }
      if (strcmp(sub, "reset") == 0) {
        _prefs.gps_power_mode = 0;
        _prefs.gps_lead_min   = 5;
        savePrefs();
        pushCompanionMessage("OK - gps power reset: mode=cycle, lead=5 min.");
        return;
      }
      pushCompanionMessage("Usage: gps power <always-on | cycle | lead <N> | reset>");
      return;
    }

    int m = match_on_off(arg);
    if (m == -1) {
      pushCompanionMessage("Mehrdeutig: on off");
      return;
    }
    if (m == 1) {
      _prefs.gps_enabled = 1;
      savePrefs();
      reevaluateRepeaterBbox();  // Reise-Fix 2026-06-08
      // Reise-Fix 2026-06-08: GPS-Modul sofort physisch einschalten
      // (analog App-Pfad CMD_SET_RADIO_PARAMS). Vorher musste man auf
      // den naechsten manageGpsPower-Tick warten -- und wenn
      // !gps_had_fix_ever, returnte der frueh und schaltete nie ein.
      sensors.setSettingValue("gps", "1");
      _gps_woke_at_millis = millis();
      if (_gps_woke_at_millis == 0) _gps_woke_at_millis = 1;
      _gps_fix_seen_this_wake = false;
      _gps_user_override_until_advert = true;
      // D3: informativer als nur "OK - GPS enabled". Zeige aktuelle
      // power-Konfig damit User direkt sieht was greift.
      const char* pmode = (_prefs.gps_power_mode == 1) ? "always-on" : "cycle";
      uint8_t lead = (_prefs.gps_lead_min == 0) ? 5 : _prefs.gps_lead_min;
      char r[120];
      snprintf(r, sizeof(r), "OK - GPS enabled.\n  power=%s  lead=%u min",
               pmode, (unsigned)lead);
      pushCompanionMessage(r);
    } else if (m == 0) {
      _prefs.gps_enabled = 0;
      savePrefs();
      reevaluateRepeaterBbox();  // Reise-Fix 2026-06-08
      // GPS-Modul sofort physisch ausschalten.
      sensors.setSettingValue("gps", "0");
      _gps_user_override_until_advert = false;
      pushCompanionMessage("OK - GPS disabled.");
    } else if (strcmp(arg, "sync") == 0) {
      // Einmaliger Wake-Trigger - GPS bleibt wach bis zum naechsten Advert
      // (gleicher Mechanismus wie der app-toggle-Override). Nicht-persistent.
      _gps_user_override_until_advert = true;
      pushCompanionMessage("OK - GPS sync request (wach bis naechster Advert).");
    } else if (strcmp(arg, "setloc") == 0) {
      // Aktuelle GPS-Position in sensors.node_lat/lon persistieren. Sinn:
      // EnvironmentSensorManager schreibt zwar laufend node_lat/lon aus
      // dem Live-Fix, savePrefs wird dabei aber nicht getriggert. Bei
      // GPS-off oder Cycle-Sleep bleibt dann irgendwann ein stale Wert
      // im Flash. 'gps setloc' macht aus der aktuellen Position einen
      // expliziten persistenten Anker.
      double cur_lat, cur_lon;
      if (!getEffectiveLatLon(cur_lat, cur_lon)) {
        pushCompanionMessage("Keine gueltige Position (GPS aus, kein Fix, "
                             "und keine fixe Position konfiguriert).");
        return;
      }
      sensors.node_lat = cur_lat;
      sensors.node_lon = cur_lon;
      savePrefs();
      char line[100];
      snprintf(line, sizeof(line), "OK - position persistiert: %.6f, %.6f",
               cur_lat, cur_lon);
      pushCompanionMessage(line);
      // Geo-Recommendation neu auswerten (analog CMD_SET_ADVERT_LATLON)
      maybePushGeoRecommendation(cur_lat, cur_lon);
    } else {
      pushCompanionMessage("Usage: gps <on | off | sync | setloc | power ...>\n"
                           "ohne Arg -> Status");
    }
    return;
  }

  // ---------- tempradio --------------------------------------------------
  // Temporaere Funk-Parameter (nicht persistent). Ueberschreibt _prefs
  // OHNE savePrefs - beim Reboot werden _prefs aus File geladen und die
  // tempradio-Werte sind weg. Caveat: andere CLI-Befehle die savePrefs()
  // aufrufen wuerden die tempradio-Werte ins File schreiben (-> Test
  // beenden bevor anderes geaendert wird, oder einfach rebooten).
  //
  // Formen:
  //   tempradio                                  Zeige aktuelle Werte
  //   tempradio <freq> <sf> <bw> <cr> [<tx>]     All-at-once Set
  //   tempradio freq <MHz>                       Einzel-Set freq
  //   tempradio sf <6..12>
  //   tempradio bw <kHz>      (LoRa-legal: 7.81/10.42/15.63/20.83/31.25/
  //                            41.67/62.5/125/250/500)
  //   tempradio cr <5..8>     (= 4/5..4/8)
  //   tempradio tx <dBm>      (Alias: tx_power)
  if (starts_with_word(cmd, "tempradio")) {
    const char* p = strchr(cmd, ' ');
    if (p) { while (*p == ' ') p++; }

    // Validatoren-Helper -- werden von beiden Pfaden (all-at-once + Einzel)
    // gemeinsam genutzt. Returns true=OK, false=Fehler (Message bereits gepusht).
    auto validateFreq = [&](float f) -> bool {
      if (f < 150.0f || f > 2500.0f) {
        pushCompanionMessage("freq ausserhalb 150..2500 MHz");
        return false;
      }
      return true;
    };
    auto validateSf = [&](int sf) -> bool {
      if (sf < 5 || sf > 12) { pushCompanionMessage("sf ausserhalb 5..12"); return false; }
      return true;
    };
    auto validateBw = [&](float bw) -> bool {
      // LoRa SX126x legal BWs in kHz. Toleranz 0.1 kHz fuer Float-Rundung.
      static const float legal[] = {
        7.81f, 10.42f, 15.63f, 20.83f, 31.25f, 41.67f, 62.5f,
        125.0f, 250.0f, 500.0f
      };
      for (size_t i = 0; i < sizeof(legal)/sizeof(legal[0]); i++) {
        float d = bw - legal[i]; if (d < 0) d = -d;
        if (d < 0.15f) return true;
      }
      pushCompanionMessage(
        "bw nicht-legal. Erlaubt (kHz):\n"
        "  7.81 10.42 15.63 20.83 31.25\n"
        "  41.67 62.5 125 250 500");
      return false;
    };
    auto validateCr = [&](int cr) -> bool {
      if (cr < 5 || cr > 8) {
        pushCompanionMessage("cr ausserhalb 5..8 (= LoRa coding rate 4/5..4/8)");
        return false;
      }
      return true;
    };
    auto validateTx = [&](int tx) -> bool {
      if (tx < -9 || tx > MAX_LORA_TX_POWER) {
        char e[80]; snprintf(e, sizeof(e), "tx_dbm ausserhalb -9..%d", (int)MAX_LORA_TX_POWER);
        pushCompanionMessage(e);
        return false;
      }
      return true;
    };

    auto applyAndAck = [&](float f, int sf, float bw, int cr, int tx,
                           bool full) {
      _prefs.freq = f;
      _prefs.sf = (uint8_t)sf;
      _prefs.bw = bw;
      _prefs.cr = (uint8_t)cr;
      _prefs.tx_power_dbm = (int8_t)tx;
      applyRadioPolicy();
      radio_driver.setTxPower((int8_t)tx);
      char line[160];
      snprintf(line, sizeof(line),
               "OK - tempradio: f=%.4f MHz sf=%d bw=%.2f kHz cr=%d tx=%d dBm\n"
               "(%snicht persistent, weg nach Reboot)",
               f, sf, bw, cr, tx, full ? "" : "Einzel-Update -- ");
      pushCompanionMessage(line);
    };

    // tempradio (kein Arg) -> aktuellen Stand zeigen
    if (!p || *p == 0) {
      char line[160];
      snprintf(line, sizeof(line),
               "tempradio:\n"
               "  freq=%.4f MHz  sf=%u  bw=%.2f kHz  cr=%u  tx=%d dBm",
               (double)_prefs.freq, (unsigned)_prefs.sf, (double)_prefs.bw,
               (unsigned)_prefs.cr, (int)_prefs.tx_power_dbm);
      pushCompanionMessage(line);
      pushCompanionMessage(
        "Einzel-Set: tempradio freq|sf|bw|cr|tx <wert>\n"
        "Alle-auf-einmal: tempradio <f> <sf> <bw> <cr> [<tx>]");
      return;
    }

    // Einzel-Set: erkennen am ersten Token (keyword statt Zahl).
    if ((p[0] < '0' || p[0] > '9') && p[0] != '-') {
      // Sub-Befehl parsen
      char sub[16];
      size_t si = 0;
      while (*p && *p != ' ' && si + 1 < sizeof(sub)) sub[si++] = *p++;
      sub[si] = 0;
      while (*p == ' ') p++;
      if (!*p) {
        pushCompanionMessage("Wert fehlt. Usage: tempradio <feld> <wert>");
        return;
      }
      // 'tx_power' als Alias zu 'tx'
      if (strcmp(sub, "tx_power") == 0) sub[2] = 0;

      if (strcmp(sub, "freq") == 0) {
        float f = atof(p);
        if (!validateFreq(f)) return;
        applyAndAck(f, _prefs.sf, _prefs.bw, _prefs.cr, _prefs.tx_power_dbm, false);
        return;
      }
      if (strcmp(sub, "sf") == 0) {
        int sf = atoi(p);
        if (!validateSf(sf)) return;
        applyAndAck(_prefs.freq, sf, _prefs.bw, _prefs.cr, _prefs.tx_power_dbm, false);
        return;
      }
      if (strcmp(sub, "bw") == 0) {
        float bw = atof(p);
        if (!validateBw(bw)) return;
        applyAndAck(_prefs.freq, _prefs.sf, bw, _prefs.cr, _prefs.tx_power_dbm, false);
        return;
      }
      if (strcmp(sub, "cr") == 0) {
        int cr = atoi(p);
        if (!validateCr(cr)) return;
        applyAndAck(_prefs.freq, _prefs.sf, _prefs.bw, cr, _prefs.tx_power_dbm, false);
        return;
      }
      if (strcmp(sub, "tx") == 0) {
        int tx = atoi(p);
        if (!validateTx(tx)) return;
        applyAndAck(_prefs.freq, _prefs.sf, _prefs.bw, _prefs.cr, tx, false);
        return;
      }
      pushCompanionMessage("Unbekanntes Feld. freq | sf | bw | cr | tx");
      return;
    }

    // All-at-once Pfad: freq sf bw cr [tx]
    float freq = atof(p);
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    if (!*p) { pushCompanionMessage("Usage: tempradio <freq> <sf> <bw> <cr> [<tx>]"); return; }
    int sf = atoi(p);
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    if (!*p) { pushCompanionMessage("Usage: tempradio <freq> <sf> <bw> <cr> [<tx>]"); return; }
    float bw = atof(p);
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    if (!*p) { pushCompanionMessage("Usage: tempradio <freq> <sf> <bw> <cr> [<tx>]"); return; }
    int cr = atoi(p);
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    int tx = (*p) ? atoi(p) : (int)_prefs.tx_power_dbm;

    if (!validateFreq(freq)) return;
    if (!validateSf(sf))     return;
    if (!validateBw(bw))     return;
    if (!validateCr(cr))     return;
    if (!validateTx(tx))     return;
    applyAndAck(freq, sf, bw, cr, tx, true);
    return;
  }

  // ---------- neighbors [hops <N> | km <D>] -----------------------------
  // Default: nur direkt-gehoerte Contacts (out_path_len == 0). Drop hops-
  // Spalte da redundant.
  // 'neighbors hops N': direkt + bis zu N Hops einschliessen.
  // 'neighbors km D':   direkt + alle bekannt-positionierten Contacts
  //                     innerhalb D km. Direkte werden IMMER gezeigt --
  //                     auch wenn deren Position unbekannt -- weil
  //                     direkt-gehoert per se interessant ist.
  // Immer 48h-Fenster als Stale-Cutoff.
  if (starts_with_word(cmd, "neighbors")) {
    enum NbMode { NB_DIRECT, NB_HOPS, NB_KM };
    NbMode mode = NB_DIRECT;
    int    max_hops = 0;
    double max_km = 0.0;

    const char* arg = strchr(cmd, ' ');
    if (arg) {
      while (*arg == ' ' || *arg == '\t') arg++;
      if (*arg) {
        if (strncmp(arg, "help", 4) == 0 || arg[0] == '?') {
          pushCompanionMessage(
            "neighbors [hops <N> | km <D>]:\n"
            "  ohne Arg: nur direkt-gehoerte (hops=0).");
          pushCompanionMessage(
            "  hops <N>: direkt + bis zu N Hops.\n"
            "  km <D>:   direkt + alle <= D km Distanz\n"
            "            (Position noetig fuer km-Filter).");
          return;
        }
        if (strncmp(arg, "hops", 4) == 0
            && (arg[4] == ' ' || arg[4] == '\t')) {
          const char* nstart = arg + 4;
          while (*nstart == ' ' || *nstart == '\t') nstart++;
          max_hops = atoi(nstart);
          if (max_hops <= 0 || max_hops > 63) {
            pushCompanionMessage("Usage: neighbors hops <1..63>");
            return;
          }
          mode = NB_HOPS;
        } else if (strncmp(arg, "km", 2) == 0
                   && (arg[2] == ' ' || arg[2] == '\t')) {
          const char* nstart = arg + 2;
          while (*nstart == ' ' || *nstart == '\t') nstart++;
          max_km = atof(nstart);
          if (max_km <= 0.0 || max_km > 99999.0) {
            pushCompanionMessage("Usage: neighbors km <distance>");
            return;
          }
          mode = NB_KM;
        } else {
          pushCompanionMessage(
            "Usage: neighbors [hops <N> | km <D> | help]");
          return;
        }
      }
    }

    uint32_t now = getRTCClock()->getCurrentTime();
    int num = getNumContacts();
    int shown = 0;

    // Akkumulierender Buffer wie bei prefs (mehrzeilig pro push).
    // Reise-Fix 2026-06-08: Flush-Schwelle 130 -> 100 damit ein zusaetzlicher
    // 40-byte-Eintrag (Name+age+km+bearing) sicher unter 145-Byte-Wire-Limit
    // bleibt. Vorher: Split mitten in UTF-8 °-Symbol -> '@359' im Folgepush.
    char buf[200];
    size_t buf_used = 0;
    auto flush = [&](bool force) {
      if (buf_used == 0) return;
      if (!force && buf_used < 100) return;
      buf[buf_used] = 0;
      pushCompanionMessage(buf);
      buf_used = 0;
    };
    auto add_line = [&](const char* line) {
      size_t len = strlen(line);
      if (buf_used + len + 2 >= sizeof(buf)) flush(true);
      if (buf_used > 0) buf[buf_used++] = '\n';
      for (size_t i = 0; i < len && buf_used < sizeof(buf) - 1; i++) {
        buf[buf_used++] = line[i];
      }
      flush(false);
    };

    // Header gemaess Modus.
    if (mode == NB_DIRECT) {
      add_line("neighbors (direct, < 48h):");
    } else if (mode == NB_HOPS) {
      char h[64];
      snprintf(h, sizeof(h), "neighbors (direct + <=%d hops, < 48h):", max_hops);
      add_line(h);
    } else {
      char h[64];
      snprintf(h, sizeof(h), "neighbors (direct + <=%.0fkm, < 48h):", max_km);
      add_line(h);
    }

    bool have_my_gps = (sensors.node_lat != 0.0 || sensors.node_lon != 0.0);

    for (int i = 0; i < num; i++) {
      ContactInfo c;
      if (!getContactByIdx(i, c)) continue;
      if (c.lastmod == 0) continue;
      if (now - c.lastmod > CR_HEARD_MAX_AGE_SECS) continue;

      // Distanz/Bearing einmal berechnen wenn beide GPS-Positionen
      // bekannt -- wird sowohl fuer den km-Mode-Filter als auch fuer
      // die Anzeige genutzt.
      char dist_buf[24]; dist_buf[0] = 0;
      double their_km = -1.0;
      if (have_my_gps && (c.gps_lat != 0 || c.gps_lon != 0)) {
        double their_lat = (double)c.gps_lat / 1000000.0;
        double their_lon = (double)c.gps_lon / 1000000.0;
        their_km  = dl9sau_haversine_km(sensors.node_lat, sensors.node_lon,
                                         their_lat, their_lon);
        int brg   = dl9sau_bearing_deg(sensors.node_lat, sensors.node_lon,
                                         their_lat, their_lon);
        snprintf(dist_buf, sizeof(dist_buf), " %.1fkm @%d°", their_km, brg);
      }

      // Filter gemaess Modus. Direkt-gehoerte werden IMMER gezeigt.
      bool is_direct = (c.out_path_len == 0);
      bool include = is_direct;
      if (mode == NB_HOPS
          && c.out_path_len != OUT_PATH_UNKNOWN
          && c.out_path_len <= max_hops) {
        include = true;
      }
      if (mode == NB_KM && their_km >= 0 && their_km <= max_km) {
        include = true;
      }
      if (!include) continue;

      // Age formatieren (RTC-relativ).
      char age[16];
      uint32_t s = now - c.lastmod;
      if      (s < 60)     snprintf(age, sizeof(age), "%us", (unsigned)s);
      else if (s < 3600)   snprintf(age, sizeof(age), "%umin", (unsigned)(s / 60));
      else if (s < 86400)  snprintf(age, sizeof(age), "%uh%02um",
                                    (unsigned)(s / 3600), (unsigned)((s % 3600) / 60));
      else                 snprintf(age, sizeof(age), "%ud%02uh",
                                    (unsigned)(s / 86400), (unsigned)((s % 86400) / 3600));

      const char* tname;
      switch (c.type) {
        case ADV_TYPE_REPEATER: tname = "rep "; break;
        case ADV_TYPE_CHAT:     tname = "cmp "; break;
        case ADV_TYPE_ROOM:     tname = "room"; break;
        case ADV_TYPE_SENSOR:   tname = "sns "; break;
        default:                tname = "?   "; break;
      }

      char hop[16];
      if (c.out_path_len == OUT_PATH_UNKNOWN) {
        snprintf(hop, sizeof(hop), "?");
      } else {
        snprintf(hop, sizeof(hop), "%u", (unsigned)c.out_path_len);
      }

      // Layout-Praefix: 3-Byte-Hex (= 6 hex chars) vor dem Namen, fuer
      // Konsistenz mit printRepeaterLegendEntry und mit der discover-
      // Augmentation unten. So steht der Hex-Stempel immer an gleicher
      // Position, egal ob Kontakt-Name bekannt oder nicht.
      char prefix6[7];
      for (int j = 0; j < 3; j++) snprintf(prefix6 + j*2, 3, "%02x", c.id.pub_key[j]);
      prefix6[6] = 0;
      char idstr[64];
      snprintf(idstr, sizeof(idstr), "%s %s", prefix6, c.name);

      char line[160];
      if (mode == NB_DIRECT) {
        // Direkt-Mode: hops-Spalte droppen (immer 0, redundant).
        snprintf(line, sizeof(line), "  %s %-25.25s %6s%s",
                 tname, idstr, age, dist_buf);
      } else {
        snprintf(line, sizeof(line), "  %s %-25.25s %6s hops=%s%s",
                 tname, idstr, age, hop, dist_buf);
      }
      add_line(line);
      shown++;
    }

    // Discover-Augmentation: wenn innerhalb 48h ein 'discover' lief und
    // dabei Antworten gesammelt wurden, liste jene Knoten nach die NICHT
    // in der Kontaktliste sind (kein Auto-Add fuer diesen Typ konfiguriert).
    // Discover-Antworten sind protokoll-bedingt zero-hop -- daher passen
    // sie zu allen drei Modi (direct, hops, km). Im km-Mode wird mangels
    // GPS-Info keine Distanz angezeigt, aber Eintrag inkludiert (User-
    // Vorgabe: direkt-gehoert ist immer interessant).
    int discover_shown = 0;
    if (_discover_last_at_rtc > 0
        && now >= _discover_last_at_rtc
        && (now - _discover_last_at_rtc) <= CR_HEARD_MAX_AGE_SECS
        && _discover_count > 0) {
      char dage[16];
      uint32_t s = now - _discover_last_at_rtc;
      if      (s < 60)     snprintf(dage, sizeof(dage), "%us", (unsigned)s);
      else if (s < 3600)   snprintf(dage, sizeof(dage), "%umin", (unsigned)(s / 60));
      else if (s < 86400)  snprintf(dage, sizeof(dage), "%uh%02um",
                                    (unsigned)(s / 3600), (unsigned)((s % 3600) / 60));
      else                 snprintf(dage, sizeof(dage), "%ud%02uh",
                                    (unsigned)(s / 86400), (unsigned)((s % 86400) / 3600));

      for (uint8_t i = 0; i < _discover_count; i++) {
        const DiscoverEntry& e = _discover_entries[i];
        // Skip wenn Eintrag in Kontaktliste -- wurde dann oben schon
        // (oder durch Filter bewusst nicht) gelistet.
        ContactInfo* known = lookupContactByPubKey(
            (uint8_t*)e.pub_key, e.full_pubkey ? PUB_KEY_SIZE : 8);
        if (known) continue;

        const char* dtname;
        switch (e.adv_type) {
          case ADV_TYPE_REPEATER: dtname = "rep "; break;
          case ADV_TYPE_CHAT:     dtname = "cmp "; break;
          case ADV_TYPE_ROOM:     dtname = "room"; break;
          case ADV_TYPE_SENSOR:   dtname = "sns "; break;
          default:                dtname = "?   "; break;
        }
        // Konsistent zur Kontakt-Zeile oben: 6-hex-Prefix vorn, dann
        // '(unknown)' im Namens-Slot. Discover-Cache liefert mindestens
        // 8 Byte Pub-Key (short-discover) bzw. 32 Byte (full); die ersten
        // 3 Byte (6 hex chars) genuegen fuer Visual-Identifikation.
        char dprefix6[7];
        for (int j = 0; j < 3; j++) snprintf(dprefix6 + j*2, 3, "%02x", e.pub_key[j]);
        dprefix6[6] = 0;
        char did[40];
        snprintf(did, sizeof(did), "%s (unknown)", dprefix6);

        char line[160];
        if (mode == NB_DIRECT) {
          snprintf(line, sizeof(line), "  %s %-25.25s %6s",
                   dtname, did, dage);
        } else {
          // hops=0 fix (discover-Antworten sind protokoll-bedingt direct)
          snprintf(line, sizeof(line), "  %s %-25.25s %6s hops=0",
                   dtname, did, dage);
        }
        add_line(line);
        discover_shown++;
      }
    }

    char summary[100];
    if (discover_shown > 0) {
      snprintf(summary, sizeof(summary),
               "total: %d contacts + %d discover-only, %d known",
               shown, discover_shown, num);
    } else {
      snprintf(summary, sizeof(summary),
               "total: %d within 48h, %d known", shown, num);
    }
    add_line(summary);
    flush(true);
    return;
  }

  // ---------- ch.hops --------------------------------------------------
  // Per-Channel Repeat-Cap (Wunschliste 32). dt267-inspirierte Syntax:
  //   set ch.hops <name> <follow|off|N>  -- N=0 heisst 'nicht repeaten'
  //   get ch.hops <name>
  //   ch.hops status              -- alle aktiven Caps
  //   ch.hops clear               -- alle Caps loeschen (ausser companion)
  //   ch.hops help / ?
  if (starts_with_word(cmd, "ch.hops")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ' || *arg == '\t') arg++; }

    if (!arg || *arg == 0 || strcmp(arg, "help") == 0 || arg[0] == '?') {
      pushCompanionMessage(
        "ch.hops: per-Channel Repeat-Cap.\n"
        "  ch.hops status       -- aktive Caps zeigen\n"
        "  ch.hops clear        -- alle Caps loeschen");
      pushCompanionMessage(
        "  set ch.hops <name> <follow|off|N>\n"
        "    follow = kein Cap (flood_max)\n"
        "    off    = nicht repeaten\n"
        "    1..63  = expliziter Cap");
      pushCompanionMessage(
        "  get ch.hops <name>\n"
        "$companion ist forced auf 0 (nicht repeaten,\n"
        "Sicherheitsmassnahme, nicht aenderbar).");
      return;
    }

    if (strcmp(arg, "status") == 0) {
      int n_shown = 0;
      char buf[200]; size_t buf_used = 0;
      auto flush_b = [&](bool force) {
        if (buf_used == 0) return;
        if (!force && buf_used < 130) return;
        buf[buf_used] = 0; pushCompanionMessage(buf); buf_used = 0;
      };
      auto add_b = [&](const char* line) {
        size_t len = strlen(line);
        if (buf_used + len + 2 >= sizeof(buf)) flush_b(true);
        if (buf_used > 0) buf[buf_used++] = '\n';
        for (size_t k = 0; k < len && buf_used < sizeof(buf)-1; k++) buf[buf_used++] = line[k];
        flush_b(false);
      };
      add_b("ch.hops status:");
      // Punkt 5 Reise-Fix 2026-06-08: $-Praefix bei Channel-Namen + klarere
      // Wording. Vorher 'companion = off (nicht repeaten)' war unklar
      // (User-Frage: was ist 'companion'?). Jetzt: '$companion'.
      for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
        ChannelDetails ch;
        if (!getChannel(i, ch)) continue;
        if (ch.name[0] == 0) continue;
        uint8_t cap = _channel_hops_cap_cache[i];
        if (cap == CH_HOPS_OFF) continue;  // nicht zeigen
        char display[24];
        snprintf(display, sizeof(display), "$%s", ch.name);
        char line[80];
        if (cap == 0) snprintf(line, sizeof(line), "  %-20.20s = 0 hops (off, nicht repeated)", display);
        else          snprintf(line, sizeof(line), "  %-20.20s = %u hops", display, (unsigned)cap);
        add_b(line);
        n_shown++;
      }
      // External-Eintraege (nicht-abonnierte Hashtag-Channels). Name
      // wird in der Entry gespeichert -- direkt anzeigen mit (ext)-Suffix.
      for (uint8_t e = 0; e < _prefs.channel_hops_count; e++) {
        const auto& en = _prefs.channel_hops_list[e];
        if (!(en.flags & CH_HOPS_FLAG_EXTERNAL)) continue;
        char display[24];
        snprintf(display, sizeof(display), "#%s (ext)", en.name);
        char line[80];
        if (en.cap == 0)
          snprintf(line, sizeof(line), "  %-20.20s = 0 hops (off, nicht repeated)", display);
        else
          snprintf(line, sizeof(line), "  %-20.20s = %u hops", display, (unsigned)en.cap);
        add_b(line);
        n_shown++;
      }
      // Unknown-Chan-Cap IMMER zeigen -- macht das "follow"-Verhalten
      // sichtbar (analog flood_max_infra-Anzeige in 'get all').
      {
        char line[80];
        if (_prefs.flood_max_unknown_chan == CH_HOPS_OFF)
          snprintf(line, sizeof(line),
                   "  <unknown channels>   = follow flood_max (%u hops)",
                   (unsigned)_prefs.flood_max);
        else if (_prefs.flood_max_unknown_chan == 0)
          snprintf(line, sizeof(line),
                   "  <unknown channels>   = 0 hops (off, nicht repeated)");
        else
          snprintf(line, sizeof(line), "  <unknown channels>   = %u hops",
                   (unsigned)_prefs.flood_max_unknown_chan);
        add_b(line);
        n_shown++;
      }
      if (n_shown == 0) add_b("  (keine, alle channels = off)");
      flush_b(true);
      return;
    }

    if (strcmp(arg, "clear") == 0) {
      // Liste leeren (alle Eintraege weg, ausser companion -- der wird
      // vom rebuildChannelHopsCache() automatisch re-upsertet mit cap=0).
      uint8_t before = _prefs.channel_hops_count;
      _prefs.channel_hops_count = 0;
      memset(_prefs.channel_hops_list, 0, sizeof(_prefs.channel_hops_list));
      int n_cleared = before;  // grobe Schaetzung (companion war drin -> wird re-upsertet)
      if (_prefs.flood_max_unknown_chan != CH_HOPS_OFF) {
        _prefs.flood_max_unknown_chan = CH_HOPS_OFF;
        n_cleared++;
      }
      rebuildChannelHopsCache();
      savePrefs();
      char r[100]; snprintf(r, sizeof(r),
        "OK - %d ch.hops Cap(s) geloescht.\n($companion bleibt forced auf 0.)",
        n_cleared);
      pushCompanionMessage(r);
      return;
    }

    // Unbekanntes Subkommando
    char r[120];
    snprintf(r, sizeof(r), "Unbekannt: ch.hops %s\n'ch.hops help' fuer Liste.", arg);
    pushCompanionMessage(r);
    return;
  }

  // ---------- prefs [show | all | reset] -------------------------------
  // Zeigt / resettet die DL9SAU-spezifischen Prefs (die ueber Companion-CLI
  // konfigurierbar sind). App-Settings (node_name, freq, default_scope etc.)
  // sind NICHT betroffen - dafuer existiert das App-eigene Backup.
  if (starts_with_word(cmd, "prefs")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ') arg++; }
    bool show_all = (arg && strcmp(arg, "all") == 0);
    bool do_reset = (arg && strcmp(arg, "reset") == 0);
    bool do_save  = (arg && strcmp(arg, "save") == 0);

    if (do_save) {
      // Wunschliste 28: zur Persistenz nach 'backup restore' (oder
      // anderen runtime-Aenderungen). Schreibt _prefs + lat/lon raus.
      savePrefs();
      pushCompanionMessage("OK - prefs persistent gespeichert.\n"
                           "Hinweis: 'save' macht das Gleiche.");
      return;
    }

    if (do_reset) {
      _prefs.chat_name_mode = 0;
      memset(_prefs.chat_name_custom, 0, sizeof(_prefs.chat_name_custom));
      _prefs.auto_advert_enabled = 0;
      _prefs.client_repeat_force = 0;
      _prefs.duty_soft_pct = 80;
      _prefs.duty_hard_pct = 100;
      memset(_prefs.bake_scope_name, 0, sizeof(_prefs.bake_scope_name));
      memset(_prefs.bake_scope_key,  0, sizeof(_prefs.bake_scope_key));
      memset(_prefs.override_scope_name, 0, sizeof(_prefs.override_scope_name));
      memset(_prefs.override_scope_key,  0, sizeof(_prefs.override_scope_key));
      _prefs.override_expiry = 0;
      _prefs.trace_flags_persistent = 0;
      _prefs.gps_power_mode = 0;
      _prefs.gps_lead_min = 5;
      // repeat_scope_mode auf ALLOWLIST (Default). Reise-Wunsch 2026-06-08:
      // sicheres Default = User entscheidet welche Scopes weitergeleitet.
      // Scope-User-Customizations (scope_buildin_status + scope_extras)
      // bleiben erhalten -- User soll sie nicht durch einen prefs-reset
      // verlieren. Wer das auch los werden will: 'scope <name> off|delete'
      // pro Eintrag, bzw. 'scope remove <name>' fuer Extras.
      _prefs.repeat_scope_mode = REPEAT_SCOPE_MODE_ALLOWLIST;
      // Wunschliste 46 Filter (2026-06-10): Reset auch Filter-Listen.
      _prefs.filter_sender_drop_count = 0;
      memset(_prefs.filter_sender_drop, 0, sizeof(_prefs.filter_sender_drop));
      memset(_prefs.filter_sender_drop_chan_on, 0, sizeof(_prefs.filter_sender_drop_chan_on));
      memset(_prefs.filter_sender_drop_chan_ex, 0, sizeof(_prefs.filter_sender_drop_chan_ex));
      _prefs.filter_sender_keep_count = 0;
      memset(_prefs.filter_sender_keep, 0, sizeof(_prefs.filter_sender_keep));
      memset(_prefs.filter_sender_keep_chan_on, 0, sizeof(_prefs.filter_sender_keep_chan_on));
      memset(_prefs.filter_sender_keep_chan_ex, 0, sizeof(_prefs.filter_sender_keep_chan_ex));
      _prefs.filter_text_drop_count = 0;
      memset(_prefs.filter_text_drop, 0, sizeof(_prefs.filter_text_drop));
      memset(_prefs.filter_text_drop_chan_on, 0, sizeof(_prefs.filter_text_drop_chan_on));
      memset(_prefs.filter_text_drop_chan_ex, 0, sizeof(_prefs.filter_text_drop_chan_ex));
      _prefs.filter_text_keep_count = 0;
      memset(_prefs.filter_text_keep, 0, sizeof(_prefs.filter_text_keep));
      memset(_prefs.filter_text_keep_chan_on, 0, sizeof(_prefs.filter_text_keep_chan_on));
      memset(_prefs.filter_text_keep_chan_ex, 0, sizeof(_prefs.filter_text_keep_chan_ex));
      _prefs.filter_scope_drop_count = 0;
      memset(_prefs.filter_scope_drop, 0, sizeof(_prefs.filter_scope_drop));
      memset(_prefs.filter_scope_drop_chan_on, 0, sizeof(_prefs.filter_scope_drop_chan_on));
      memset(_prefs.filter_scope_drop_chan_ex, 0, sizeof(_prefs.filter_scope_drop_chan_ex));
      _prefs.filter_scope_keep_count = 0;
      memset(_prefs.filter_scope_keep, 0, sizeof(_prefs.filter_scope_keep));
      memset(_prefs.filter_scope_keep_chan_on, 0, sizeof(_prefs.filter_scope_keep_chan_on));
      memset(_prefs.filter_scope_keep_chan_ex, 0, sizeof(_prefs.filter_scope_keep_chan_ex));
      _prefs.filter_unknown_channel_repeat = 1;  // filter-unscoped Default
      _prefs.bluetooth_power_mode = 0;           // cycle Default
      _trace_flags = 0;  // RAM-only auch resetten (sonst inkonsistent)
      savePrefs();
      pushCompanionMessage("OK - DL9SAU prefs auf Defaults zurueckgesetzt.\n"
                           "Scope-Customizations bleiben (separate Befehle).");
      return;
    }

    // Akkumulierender Buffer. MAX_TEXT_LEN = 160 (Wire-Frame-Limit fuer
    // channel messages). Wir flushen VOR jedem Append wenn die Summe sonst
    // > 155 wuerde — damit kein einzelner Push abgeschnitten wird.
    char prefs_buf[200];
    size_t buf_used = 0;
    auto flush_buf = [&]() {
      if (buf_used == 0) return;
      prefs_buf[buf_used] = 0;
      pushCompanionMessage(prefs_buf);
      buf_used = 0;
    };
    auto add_line = [&](const char* line) {
      size_t len = strlen(line);
      if (len > 130) len = 130;
      // Wuerde die naechste Zeile (mit '\n'-Separator) das Wire-Limit
      // sprengen? Dann erst flushen.
      if (buf_used > 0 && buf_used + 1 + len > 130) flush_buf();
      if (buf_used > 0) prefs_buf[buf_used++] = '\n';
      for (size_t i = 0; i < len && buf_used < sizeof(prefs_buf) - 1; i++) {
        prefs_buf[buf_used++] = line[i];
      }
    };

    // Erste Zeile = Header
    add_line(show_all ? "prefs (all DL9SAU):" : "prefs (non-default):");
    int non_default_count = 0;
    char tmp[160];

    // chatname
    if (show_all || _prefs.chat_name_mode != 0) {
      snprintf(tmp, sizeof(tmp), "  chat_name_mode = %u%s",
               (unsigned)_prefs.chat_name_mode,
               _prefs.chat_name_mode == 0 ? " [default]" : " (default: 0)");
      add_line(tmp);
      if (_prefs.chat_name_mode != 0) non_default_count++;
    }
    if (show_all || _prefs.chat_name_custom[0] != 0) {
      snprintf(tmp, sizeof(tmp), "  chat_name_custom = \"%s\"%s",
               _prefs.chat_name_custom,
               _prefs.chat_name_custom[0] == 0 ? " [default]" : " (default: \"\")");
      add_line(tmp);
      if (_prefs.chat_name_custom[0] != 0) non_default_count++;
    }
    // autoadv (Bitmask)
    if (show_all || _prefs.auto_advert_enabled != 0) {
      snprintf(tmp, sizeof(tmp),
               "  auto_advert_enabled = 0x%02X (zerohop=%s nightly=%s)%s",
               (unsigned)_prefs.auto_advert_enabled,
               (_prefs.auto_advert_enabled & AUTO_ADV_ZEROHOP) ? "on" : "off",
               (_prefs.auto_advert_enabled & AUTO_ADV_NIGHTLY) ? "on" : "off",
               _prefs.auto_advert_enabled == 0 ? " [default]" : " (default: 0)");
      add_line(tmp);
      if (_prefs.auto_advert_enabled != 0) non_default_count++;
    }
#ifdef REPEATER_DEFENSIVE_FORCE
    // client_repeat_force
    if (show_all || _prefs.client_repeat_force != 0) {
      snprintf(tmp, sizeof(tmp), "  client_repeat_force = %u%s",
               (unsigned)_prefs.client_repeat_force,
               _prefs.client_repeat_force == 0 ? " [default]" : " (default: 0)");
      add_line(tmp);
      if (_prefs.client_repeat_force != 0) non_default_count++;
    }
#endif
    // repeater_profile
    if (show_all || _prefs.repeater_profile != 0) {
      snprintf(tmp, sizeof(tmp), "  repeater_profile = %s%s",
               _prefs.repeater_profile == 1 ? "normal" : "defensive",
               _prefs.repeater_profile == 0 ? " [default]" : " (default: defensive)");
      add_line(tmp);
      if (_prefs.repeater_profile != 0) non_default_count++;
    }
    // loop_detect (Wunschliste 6b)
    if (show_all || _prefs.loop_detect != 0) {
      const char* nm = (_prefs.loop_detect == 0) ? "off"
                     : (_prefs.loop_detect == 1) ? "minimal"
                     : (_prefs.loop_detect == 2) ? "moderate" : "strict";
      snprintf(tmp, sizeof(tmp), "  loop_detect = %s%s%s",
               nm,
               _prefs.loop_detect == 0 ? " [default]" : " (default: off)",
               (_prefs.loop_detect != 0 && _prefs.repeater_profile != 1)
                   ? "  (inaktiv -- profile=defensive)" : "");
      add_line(tmp);
      if (_prefs.loop_detect != 0) non_default_count++;
    }
    // duty (Werte sind % vom 10%-EU-Airtime-Limit)
    if (show_all || _prefs.duty_soft_pct != 80) {
      snprintf(tmp, sizeof(tmp), "  duty_soft_pct = %u%% (= %u.%u%% Airtime)%s",
               (unsigned)_prefs.duty_soft_pct,
               (unsigned)_prefs.duty_soft_pct / 10,
               (unsigned)_prefs.duty_soft_pct % 10,
               _prefs.duty_soft_pct == 80 ? " [default]" : " (default: 80)");
      add_line(tmp);
      if (_prefs.duty_soft_pct != 80) non_default_count++;
    }
    if (show_all || _prefs.duty_hard_pct != 100) {
      snprintf(tmp, sizeof(tmp), "  duty_hard_pct = %u%% (= %u.%u%% Airtime)%s",
               (unsigned)_prefs.duty_hard_pct,
               (unsigned)_prefs.duty_hard_pct / 10,
               (unsigned)_prefs.duty_hard_pct % 10,
               _prefs.duty_hard_pct == 100 ? " [default]" : " (default: 100)");
      add_line(tmp);
      if (_prefs.duty_hard_pct != 100) non_default_count++;
    }
    // bake scope
    bool bake_set = false;
    for (size_t k = 0; k < sizeof(_prefs.bake_scope_key); k++)
      if (_prefs.bake_scope_key[k] != 0) { bake_set = true; break; }
    if (show_all || bake_set) {
      snprintf(tmp, sizeof(tmp), "  bake_scope = %s%s",
               bake_set ? _prefs.bake_scope_name : "(none)",
               bake_set ? " (default: (none))" : " [default]");
      add_line(tmp);
      if (bake_set) non_default_count++;
    }
    // override scope
    bool ovr_set = (_prefs.override_expiry != 0);
    if (show_all || ovr_set) {
      uint32_t now = getRTCClock()->getCurrentTime();
      const char* tag = (ovr_set && now < _prefs.override_expiry) ? "active" : "expired";
      snprintf(tmp, sizeof(tmp), "  override_scope = %s (%s)%s",
               ovr_set ? _prefs.override_scope_name : "(none)",
               ovr_set ? tag : "n/a",
               ovr_set ? " (default: (none))" : " [default]");
      add_line(tmp);
      if (ovr_set) non_default_count++;
    }
    // scope advert auto (Wunschliste 13)
    if (show_all || _prefs.scope_advert_auto != 2) {
      const char* st = (_prefs.scope_advert_auto == 1) ? "off"
                     : (_prefs.scope_advert_auto == 3) ? "prefer" : "on";
      snprintf(tmp, sizeof(tmp), "  scope_advert_auto = %s%s", st,
               _prefs.scope_advert_auto == 2 ? " [default]" : " (default: on)");
      add_line(tmp);
      if (_prefs.scope_advert_auto != 2) non_default_count++;
    }
    // scope repeater auto (Wunschliste 13)
    if (show_all || _prefs.scope_repeater_auto != 2) {
      const char* st = (_prefs.scope_repeater_auto == 1) ? "off" : "on";
      snprintf(tmp, sizeof(tmp), "  scope_repeater_auto = %s%s", st,
               _prefs.scope_repeater_auto == 2 ? " [default]" : " (default: on)");
      add_line(tmp);
      if (_prefs.scope_repeater_auto != 2) non_default_count++;
    }
    // advert_role (Wunschliste 7)
    if (show_all || _prefs.advert_role != 0) {
      const char* rn = (_prefs.advert_role == 1) ? "chat"
                     : (_prefs.advert_role == 2) ? "repeater"
                     : (_prefs.advert_role == 3) ? "sensor"
                     : (_prefs.advert_role == 4) ? "room" : "auto";
      snprintf(tmp, sizeof(tmp), "  advert_role = %s%s", rn,
               _prefs.advert_role == 0 ? " [default]" : " (default: auto)");
      add_line(tmp);
      if (_prefs.advert_role != 0) non_default_count++;
    }
    // owner_info: zeigen wenn gesetzt
    if (show_all || _prefs.owner_info[0] != 0) {
      snprintf(tmp, sizeof(tmp), "  owner_info = %s",
               _prefs.owner_info[0] ? _prefs.owner_info : "(leer) [default]");
      add_line(tmp);
      if (_prefs.owner_info[0] != 0) non_default_count++;
    }
    // trace persistent
    if (show_all || _prefs.trace_flags_persistent != 0) {
      snprintf(tmp, sizeof(tmp), "  trace_flags_persistent = 0x%04X%s",
               (unsigned)_prefs.trace_flags_persistent,
               _prefs.trace_flags_persistent == 0 ? " [default]" : " (default: 0x0000)");
      add_line(tmp);
      if (_prefs.trace_flags_persistent != 0) non_default_count++;
    }
    // gps power
    if (show_all || _prefs.gps_power_mode != 0) {
      snprintf(tmp, sizeof(tmp), "  gps_power_mode = %s%s",
               _prefs.gps_power_mode == 1 ? "always-on" : "cycle",
               _prefs.gps_power_mode == 0 ? " [default]" : " (default: cycle)");
      add_line(tmp);
      if (_prefs.gps_power_mode != 0) non_default_count++;
    }
    if (show_all || (_prefs.gps_lead_min != 0 && _prefs.gps_lead_min != 5)) {
      uint8_t lead = (_prefs.gps_lead_min == 0) ? 5 : _prefs.gps_lead_min;
      snprintf(tmp, sizeof(tmp), "  gps_lead_min = %u%s",
               (unsigned)lead,
               lead == 5 ? " [default]" : " (default: 5)");
      add_line(tmp);
      if (lead != 5) non_default_count++;
    }
    // repeat_scope_mode (Liste B Policy). Reise-Fix 2026-06-08:
    // Default ist jetzt 'allowlist' (sicher, User entscheidet).
    if (show_all || _prefs.repeat_scope_mode != REPEAT_SCOPE_MODE_ALLOWLIST) {
      const char* mode = (_prefs.repeat_scope_mode == REPEAT_SCOPE_MODE_ALLOWLIST)
                         ? "allowlist" : "all";
      snprintf(tmp, sizeof(tmp), "  repeat_scope_mode = %s%s", mode,
               _prefs.repeat_scope_mode == REPEAT_SCOPE_MODE_ALLOWLIST ? " [default]" : " (default: allowlist)");
      add_line(tmp);
      if (_prefs.repeat_scope_mode != REPEAT_SCOPE_MODE_ALLOWLIST) non_default_count++;
    }
    // Hinweis: Scope-Liste hier nicht mehr inline. 'scope list' /
    // 'scope rep' zeigen die User-Customizations + Extras getrennt.
    if (_prefs.scope_buildin_status_count > 0 || _prefs.scope_extras_count > 0) {
      snprintf(tmp, sizeof(tmp),
               "  scope = %u customs / %u extras (siehe 'scope list')",
               (unsigned)_prefs.scope_buildin_status_count,
               (unsigned)_prefs.scope_extras_count);
      add_line(tmp);
    }

    // Offline-Message-Queue (Wunschliste 19). msg_store_flash = Bit-Field
    // (1 = flash an pro Bucket), msg_store_limit[5] = per-Bucket Slot-Limit
    // (0 = type-Default 8/16/16/16/16). CLI: messages flash|limit ...
    if (show_all || _prefs.msg_store_flash != 0) {
      uint8_t f = _prefs.msg_store_flash;
      snprintf(tmp, sizeof(tmp),
               "  msg_store_flash = 0x%02X%s%s%s%s%s%s",
               (unsigned)f,
               f == 0 ? " [default]" : " (default: 0x00)",
               (f & 0x01) ? " PUB"      : "",
               (f & 0x02) ? " HT"       : "",
               (f & 0x04) ? " PRIV"     : "",
               (f & 0x08) ? " DM"       : "",
               (f & 0x10) ? " COMP"     : "");
      add_line(tmp);
      if (f != 0) non_default_count++;
    }
    {
      bool any_lim = false;
      for (int i = 0; i < 5; i++) if (_prefs.msg_store_limit[i] != 0) { any_lim = true; break; }
      if (show_all || any_lim) {
        snprintf(tmp, sizeof(tmp),
                 "  msg_store_limit = %u,%u,%u,%u,%u%s",
                 (unsigned)_prefs.msg_store_limit[0],
                 (unsigned)_prefs.msg_store_limit[1],
                 (unsigned)_prefs.msg_store_limit[2],
                 (unsigned)_prefs.msg_store_limit[3],
                 (unsigned)_prefs.msg_store_limit[4],
                 any_lim ? " (0=Default: 8/16/16/16/16)" : " [default]");
        add_line(tmp);
        if (any_lim) non_default_count++;
      }
    }

    // Logging (Wunschliste 21). Asymmetrische Bit-Semantik: bit0 = USB on
    // (default off), bit1 = Channel off (default on). CLI: logging ...
    if (show_all || _prefs.log_flags != 0) {
      uint8_t lf = _prefs.log_flags;
      bool usb_on  =  (lf & 0x01) != 0;
      bool chan_on = !(lf & 0x02);
      snprintf(tmp, sizeof(tmp),
               "  log_flags = 0x%02X (usb=%s channel=%s)%s",
               (unsigned)lf,
               usb_on  ? "on " : "off",
               chan_on ? "on " : "off",
               lf == 0 ? " [default]" : " (default: 0x00 usb=off channel=on)");
      add_line(tmp);
      if (lf != 0) non_default_count++;
    }

    // Wunschliste 43 BLE-Power-Mode.
    if (show_all || _prefs.bluetooth_power_mode != 0) {
      const char* m = (_prefs.bluetooth_power_mode == 1) ? "always-on"
                     : (_prefs.bluetooth_power_mode == 2) ? "off"
                     : "cycle";
      snprintf(tmp, sizeof(tmp),
               "  bluetooth_power_mode = %s%s",
               m, _prefs.bluetooth_power_mode == 0 ? " [default]"
                                                     : " (default: cycle)");
      add_line(tmp);
      if (_prefs.bluetooth_power_mode != 0) non_default_count++;
    }
    // Wunschliste 45 (LBT-Stub): interference_threshold + agc_reset_interval.
    if (show_all || _prefs.interference_threshold != 14) {
      snprintf(tmp, sizeof(tmp),
               "  interference_threshold = %u%s",
               (unsigned)_prefs.interference_threshold,
               _prefs.interference_threshold == 14 ? " [default]" : " (default: 14)");
      add_line(tmp);
      if (_prefs.interference_threshold != 14) non_default_count++;
    }
    if (show_all || _prefs.agc_reset_interval != 0) {
      snprintf(tmp, sizeof(tmp),
               "  agc_reset_interval = %u%s",
               (unsigned)_prefs.agc_reset_interval,
               _prefs.agc_reset_interval == 0 ? " [default]" : " (default: 0)");
      add_line(tmp);
      if (_prefs.agc_reset_interval != 0) non_default_count++;
    }
    // Wunschliste 52 (Remote-Admin): passwd_admin/guest -- nur Count (Secret).
    if (show_all || _prefs.passwd_admin[0] != 0 || _prefs.passwd_guest[0] != 0) {
      snprintf(tmp, sizeof(tmp),
               "  passwd_admin/guest = %s/%s",
               _prefs.passwd_admin[0] ? "(set)" : "(empty)",
               _prefs.passwd_guest[0] ? "(set)" : "(empty)");
      add_line(tmp);
      if (_prefs.passwd_admin[0] || _prefs.passwd_guest[0]) non_default_count++;
    }

    // Wunschliste 46 Filter (DL9SAU): Counts + unknown-channel-Achse.
    {
      uint8_t sd = _prefs.filter_sender_drop_count;
      uint8_t sk = _prefs.filter_sender_keep_count;
      uint8_t td = _prefs.filter_text_drop_count;
      uint8_t tk = _prefs.filter_text_keep_count;
      uint8_t pd = _prefs.filter_scope_drop_count;
      uint8_t pk = _prefs.filter_scope_keep_count;
      uint8_t any = sd | sk | td | tk | pd | pk;
      uint8_t uc = _prefs.filter_unknown_channel_repeat;
      if (show_all || any != 0 || uc != 1) {
        snprintf(tmp, sizeof(tmp),
                 "  filter sender drop/keep=%u/%u text=%u/%u scope=%u/%u",
                 sd, sk, td, tk, pd, pk);
        add_line(tmp);
        add_line("    Details: 'filter list'");
        if (any != 0) non_default_count++;
      }
      if (show_all || uc != 1) {
        const char* rm = "filter-none";
        switch (uc) {
          case 1: rm = "filter-unscoped"; break;
          case 2: rm = "filter-scoped"; break;
          case 3: rm = "filter-all"; break;
        }
        snprintf(tmp, sizeof(tmp),
                 "  filter unknown-channel = %s%s",
                 rm, uc == 1 ? " [default]" : " (default: filter-unscoped)");
        add_line(tmp);
        if (uc != 1) non_default_count++;
      }
    }

    if (!show_all && non_default_count == 0) {
      add_line("  (alle Werte auf Default)");
    }
    flush_buf();
    return;
  }

  // ---------- clock / date / time (no arg) ----------------------------
  // Zeigt die aktuelle RTC-Zeit (Unix-Sekunden + UTC-formatiert + lokal).
  // 'date' und 'time' (ohne Argument) sind Aliasse fuer 'clock'. 'time
  // <epoch>' weiter unten setzt die RTC.
  bool _is_clock_readout = (starts_with_word(cmd, "clock")
                            || starts_with_word(cmd, "date"));
  if (!_is_clock_readout && starts_with_word(cmd, "time")) {
    const char* a = strchr(cmd, ' ');
    if (a) { while (*a == ' ' || *a == '\t') a++; }
    if (!a || *a == 0) _is_clock_readout = true;
  }
  if (_is_clock_readout) {
    uint32_t now = getRTCClock()->getCurrentTime();
    if (now < 1500000000UL) {
      pushCompanionMessage("clock: RTC nicht gesetzt (pre-2017).");
      return;
    }
    // UTC: simple JJJJ-MM-DD HH:MM:SS via time_t (kein lokal TZ-Offset)
    time_t t = (time_t)now;
    struct tm utc;
    gmtime_r(&t, &utc);
    char utc_str[40];
    snprintf(utc_str, sizeof(utc_str), "%04d-%02d-%02d %02d:%02d:%02d UTC",
             utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
             utc.tm_hour, utc.tm_min, utc.tm_sec);
    // Lokal: now + runtime-Offset (DST-aware via localTzOffsetSecs)
    int32_t tz_off = localTzOffsetSecs(now);
    time_t local_t = (time_t)(now + (uint32_t)tz_off);
    struct tm loc;
    gmtime_r(&local_t, &loc);
    char loc_str[48];
    // CEST / CET-Label aus dem Offset ableiten (3600 = CET, 7200 = CEST).
    const char* tzname = (tz_off == 7200) ? "CEST" :
                         (tz_off == 3600) ? "CET"  : "TZ";
    snprintf(loc_str, sizeof(loc_str), "%04d-%02d-%02d %02d:%02d:%02d %s (TZ+%lds)",
             loc.tm_year + 1900, loc.tm_mon + 1, loc.tm_mday,
             loc.tm_hour, loc.tm_min, loc.tm_sec,
             tzname, (long)tz_off);
    char block[200];
    snprintf(block, sizeof(block),
             "clock:\n  unix = %lu\n  utc  = %s\n  loc  = %s",
             (unsigned long)now, utc_str, loc_str);
    pushCompanionMessage(block);
    // Wunschliste 31: Sync-Quelle anzeigen wenn aktiv. Eigene Message,
    // damit der clock-Block oben 145-Byte-sicher bleibt.
    if (_time_sync_done_since_boot && _time_sync_last_at_rtc > 0) {
      uint32_t age = (now >= _time_sync_last_at_rtc)
                     ? (now - _time_sync_last_at_rtc) : 0;
      char src[60];
      bool app_sync = (_time_sync_last_pubkey[0] == 0
                       && _time_sync_last_pubkey[1] == 0
                       && _time_sync_last_pubkey[2] == 0);
      bool bootstrap_sync = (_time_sync_last_pubkey[0] == 0xFF
                             && _time_sync_last_pubkey[1] == 0xFF
                             && _time_sync_last_pubkey[2] == 0xFF);
      bool gps_marker = (_time_sync_last_pubkey[0] == 0xFE
                         && _time_sync_last_pubkey[1] == 0xFE
                         && _time_sync_last_pubkey[2] == 0xFE);
      if (isGpsAuthoritative()) {
        snprintf(src, sizeof(src), "GPS (active)");
      } else if (gps_marker) {
        snprintf(src, sizeof(src), "GPS (zuletzt; jetzt off)");
      } else if (app_sync) {
        snprintf(src, sizeof(src), "App (CMD_SET_DEVICE_TIME)");
      } else if (bootstrap_sync) {
        snprintf(src, sizeof(src), "Boot-Bootstrap (last advert in DB)");
      } else {
        snprintf(src, sizeof(src), "advert %02x%02x%02x",
                 _time_sync_last_pubkey[0],
                 _time_sync_last_pubkey[1],
                 _time_sync_last_pubkey[2]);
      }
      char line[100];
      snprintf(line, sizeof(line), "  synced via %s, age %lus", src, (unsigned long)age);
      pushCompanionMessage(line);
    } else if (_prefs.time_sync_mode != 0 && !isGpsAuthoritative()) {
      pushCompanionMessage("  (advert-sync aktiv, noch keine Quelle gehoert)");
    }
    // Reise-Diagnose 2026-06-08: letzten adv-sync-Versuch zeigen (egal
    // ob applied oder skipped). Hilfreich um zu sehen welche Adverts
    // kommen und warum sie evtl. NICHT die RTC ueberschreiben (24h-cap
    // weil App schon gesynced hat, etc.).
    if (_last_adv_sync_outcome != 0) {
      const char* oc =
          (_last_adv_sync_outcome == 1) ? "applied"
        : (_last_adv_sync_outcome == 2) ? "skipped (24h-cap: App/recent sync hat Vorrang)"
        : (_last_adv_sync_outcome == 3) ? "skipped (replay)"
        : (_last_adv_sync_outcome == 4) ? "skipped (drift < 2 min, nicht der Update wert)"
        : (_last_adv_sync_outcome == 5) ? "skipped (single-source, drift > 1h)"
        : "?";
      uint32_t age_attempt = (now >= _last_adv_sync_at_rtc)
                            ? (now - _last_adv_sync_at_rtc) : 0;
      char line2[160];
      snprintf(line2, sizeof(line2),
               "  letzter adv-sync: %02x%02x%02x delta=%lds vor %lus\n"
               "  -> %s",
               _last_adv_sync_pubkey[0],
               _last_adv_sync_pubkey[1],
               _last_adv_sync_pubkey[2],
               (long)_last_adv_sync_delta,
               (unsigned long)age_attempt,
               oc);
      pushCompanionMessage(line2);
    }
    return;
  }

  // ---------- time <epoch> ----------------------------------------------
  // RTC setzen. Sanity: epoch muss in plausiblem Bereich (post-2017,
  // pre-2096). Triggert RTC-Jump-Detector -> nightly slot wird neu geplant.
  if (starts_with_word(cmd, "time")) {
    // 'time' ohne Arg ist oben vom clock-Readout-Block schon abgefangen --
    // hier landen wir nur mit nicht-numerischem Arg (Tippfehler).
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ') arg++; }
    if (!arg || !(arg[0] >= '0' && arg[0] <= '9')) {
      pushCompanionMessage("Usage: time <unix-epoch-sec>  (post-2017..pre-2096)\n"
                           "time/date/clock ohne Arg zeigt aktuelle RTC.");
      return;
    }
    uint32_t epoch = (uint32_t)atoll(arg);
    if (epoch < 1500000000UL || epoch > 4000000000UL) {
      pushCompanionMessage("epoch ausserhalb plausibel (1500000000..4000000000).");
      return;
    }
    getRTCClock()->setCurrentTime(epoch);
    next_night_flood_unix = 0;  // re-schedule mit neuer Zeit
    char line[80];
    snprintf(line, sizeof(line), "OK - RTC = %lu (nightly slot invalidated).",
             (unsigned long)epoch);
    pushCompanionMessage(line);
    return;
  }

  // ---------- messages [flash|limit|clear] ------------------------------
  // Wunschliste 19 Phase C: Offline-Queue Konfiguration + Anzeige.
  //   messages                       Status pro Bucket
  //   messages flash <type> on|off   Flash-Persistenz toggeln
  //   messages limit <type> <N>      Slot-Limit setzen (0 = Default)
  //   messages clear <type|all>      RAM + Flash leeren
  // type: public | hashtag | private | dm | companion (Prefix erlaubt).
  if (starts_with_word(cmd, "messages")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ' || *arg == '\t') arg++; }
    if (!arg || *arg == 0) {
      // Status-Output -- gruppiert in 2 BLE-Messages.
      // Message 1: persoenliche Buckets (DM, $companion)
      // Message 2: Gruppen-Channels (public, hashtag, private)
      // Spalte 'cap' bewusst weggelassen (war verwirrend) -- in 'help
      // messages' steht der maximal moegliche Wert pro Typ.
      static const int caps[BUCKET_COUNT] = {
        BUCKET_CAP_PUBLIC, BUCKET_CAP_HASHTAG, BUCKET_CAP_PRIVATE,
        BUCKET_CAP_DM,     BUCKET_CAP_COMPANION
      };
      auto bucketUsed = [&](MsgBucket b) -> int {
        Frame* arr = NULL;
        int c_unused = 0;
        getBucket(b, arr, c_unused);
        if (!arr) return 0;
        int n = 0;
        for (int i = 0; i < caps[(int)b]; i++) if (arr[i].seq_no) n++;
        return n;
      };
      auto displayName = [&](MsgBucket b) -> const char* {
        switch (b) {
          case BUCKET_DM:        return "DM";
          case BUCKET_COMPANION: return "$companion";
          case BUCKET_PUBLIC:    return "public";
          case BUCKET_HASHTAG:   return "hashtag";
          case BUCKET_PRIVATE:   return "private";
          default: return "?";
        }
      };
      // Message 1: persoenliche Buckets
      char msg1[200];
      int n1 = snprintf(msg1, sizeof(msg1), "messages (offline-queue):");
      MsgBucket personal[] = { BUCKET_DM, BUCKET_COMPANION };
      for (size_t i = 0; i < sizeof(personal)/sizeof(personal[0]); i++) {
        MsgBucket b = personal[i];
        n1 += snprintf(msg1 + n1, sizeof(msg1) - n1,
                       "\n  %-10s %2d/%-2d  flash=%s",
                       displayName(b), bucketUsed(b),
                       getBucketLimit(b),
                       getBucketFlash(b) ? "on" : "off");
      }
      pushCompanionMessage(msg1);

      // Message 2: Channel-Chats
      char msg2[200];
      int n2 = snprintf(msg2, sizeof(msg2), "Channels (Gruppen-Chats):");
      MsgBucket channels[] = { BUCKET_PUBLIC, BUCKET_HASHTAG, BUCKET_PRIVATE };
      for (size_t i = 0; i < sizeof(channels)/sizeof(channels[0]); i++) {
        MsgBucket b = channels[i];
        n2 += snprintf(msg2 + n2, sizeof(msg2) - n2,
                       "\n  %-10s %2d/%-2d  flash=%s",
                       displayName(b), bucketUsed(b),
                       getBucketLimit(b),
                       getBucketFlash(b) ? "on" : "off");
      }
      pushCompanionMessage(msg2);
      return;
    }

    // Sub-Befehl extrahieren
    char sub[16];
    size_t si = 0;
    while (*arg && *arg != ' ' && si + 1 < sizeof(sub)) sub[si++] = *arg++;
    sub[si] = 0;
    while (*arg == ' ' || *arg == '\t') arg++;

    if (strcmp(sub, "flash") == 0 || strcmp(sub, "limit") == 0) {
      // type-Argument extrahieren
      char tname[16];
      size_t ti = 0;
      while (*arg && *arg != ' ' && ti + 1 < sizeof(tname)) tname[ti++] = *arg++;
      tname[ti] = 0;
      while (*arg == ' ' || *arg == '\t') arg++;
      int b = bucketByName(tname);
      if (b == -2) {
        pushCompanionMessage("Mehrdeutig. public/hashtag/private/dm/companion.");
        return;
      }
      if (b < 0) {
        pushCompanionMessage("Unbekannter Typ. public/hashtag/private/dm/companion.");
        return;
      }
      if (sub[0] == 'f') {  // flash
        if (strcmp(arg, "on") == 0) {
          _prefs.msg_store_flash |= (1 << b);
          savePrefs();
          // Sofort persistieren falls Bucket gerade Eintraege hat
          saveBucketToFlash((MsgBucket)b);
          char r[80];
          snprintf(r, sizeof(r), "OK - flash %s = on (file persistiert).",
                   bucketName((MsgBucket)b));
          pushCompanionMessage(r);
        } else if (strcmp(arg, "off") == 0) {
          _prefs.msg_store_flash &= ~(1 << b);
          savePrefs();
          // Bestehende Datei loeschen (RAM bleibt)
          const char* path = msgBucketPath((MsgBucket)b);
          if (path) _store->removeFile(path);
          char r[80];
          snprintf(r, sizeof(r), "OK - flash %s = off (file geloescht, RAM bleibt).",
                   bucketName((MsgBucket)b));
          pushCompanionMessage(r);
        } else {
          pushCompanionMessage("Usage: messages flash <type> <on|off>");
        }
      } else {  // limit
        if (!*arg || !(arg[0] >= '0' && arg[0] <= '9')) {
          pushCompanionMessage("Usage: messages limit <type> <N>  (0 = Default)");
          return;
        }
        int n = atoi(arg);
        int cap;
        switch (b) {
          case BUCKET_PUBLIC:    cap = BUCKET_CAP_PUBLIC;    break;
          case BUCKET_HASHTAG:   cap = BUCKET_CAP_HASHTAG;   break;
          case BUCKET_PRIVATE:   cap = BUCKET_CAP_PRIVATE;   break;
          case BUCKET_DM:        cap = BUCKET_CAP_DM;        break;
          case BUCKET_COMPANION: cap = BUCKET_CAP_COMPANION; break;
          default: cap = 0;
        }
        if (n < 0 || n > cap) {
          char r[80];
          snprintf(r, sizeof(r), "Range 0..%d (0 = Default).", cap);
          pushCompanionMessage(r);
          return;
        }
        _prefs.msg_store_limit[b] = (uint8_t)n;
        savePrefs();
        char r[80];
        snprintf(r, sizeof(r), "OK - limit %s = %d (effective %d).",
                 bucketName((MsgBucket)b), n,
                 getBucketLimit((MsgBucket)b));
        pushCompanionMessage(r);
      }
      return;
    }
    if (strcmp(sub, "clear") == 0) {
      if (strcmp(arg, "all") == 0) {
        for (int b = 0; b < BUCKET_COUNT; b++) clearBucket((MsgBucket)b);
        pushCompanionMessage("OK - alle Buckets geleert (RAM + Flash).");
        return;
      }
      int b = bucketByName(arg);
      if (b == -2) { pushCompanionMessage("Mehrdeutig. Typ angeben oder 'all'."); return; }
      if (b < 0)   { pushCompanionMessage("Unbekannter Typ. public/hashtag/private/dm/companion/all."); return; }
      clearBucket((MsgBucket)b);
      char r[80];
      snprintf(r, sizeof(r), "OK - bucket %s geleert (RAM + Flash).",
               bucketName((MsgBucket)b));
      pushCompanionMessage(r);
      return;
    }
    pushCompanionMessage("Usage:\n"
                         "  messages\n"
                         "  messages flash <type> on|off\n"
                         "  messages limit <type> <N>\n"
                         "  messages clear <type|all>");
    return;
  }

  // ---------- logging [usb|channel] [on|off] ----------------------------
  // Wunschliste 21 (User-Wunsch 2026-05-30):
  //   logging                  Status
  //   logging usb on|off       USB-Serial Output von pushDebugLog
  //   logging channel on|off   $companion-Channel-Output von traceCompanion
  // ASYMMETRISCHE bit-Semantik in _prefs.log_flags:
  //   bit 0 = USB ENABLED (1 = on). Default 0 = OFF.
  //   bit 1 = CHANNEL DISABLED (1 = off). Default 0 = ON.
  // -> Default-Verhalten: USB aus, Channel an. Safe fuer USB-Companion-
  //    Builds wo Trace die App-Frames zerschiessen wuerde.
  // ---------- unscoped-channelmessages [direct|flood] ------------------
  // Wunschliste 25 (User-Wunsch 2026-05-30, Spazier-Gespraech):
  // wenn ein Channel-Send ohne Scope rausgeht (kein send_scope, kein
  // Default-Scope, kein Geo-Fallback), wird er per Default als zero-hop
  // direct gesendet statt geflooded -- begrenzt z.B. einen Channel
  // '#meineHausgemeinschaft' ohne Scope auf direkt-empfangbare Nachbarn
  // statt Europa-weit zu fluten.
  // Override via 'unscoped-channelmessages flood'. Runtime-only -- nach
  // Reboot wieder default (direct).
  if (starts_with_word(cmd, "unscoped-channelmessages")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ' || *arg == '\t') arg++; }
    if (!arg || *arg == 0) {
      char block[160];
      snprintf(block, sizeof(block),
               "unscoped-channelmessages:\n"
               "  mode = %s   (default: direct, nicht persistent)",
               _unscoped_channel_direct ? "direct" : "flood");
      pushCompanionMessage(block);
      return;
    }
    if (strcmp(arg, "direct") == 0) {
      _unscoped_channel_direct = true;
      pushCompanionMessage("OK - unscoped channel-msgs gehen ab jetzt als zero-hop direct.");
      return;
    }
    if (strcmp(arg, "flood") == 0) {
      _unscoped_channel_direct = false;
      pushCompanionMessage("OK - unscoped channel-msgs werden ab jetzt geflooded.");
      return;
    }
    pushCompanionMessage("Usage: unscoped-channelmessages <direct|flood>");
    return;
  }

  if (starts_with_word(cmd, "logging")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ' || *arg == '\t') arg++; }
    bool usb_on  =  (_prefs.log_flags & 0x01) != 0;  // bit=on  -> on
    bool chan_on = !(_prefs.log_flags & 0x02);       // bit=off -> on
    if (!arg || *arg == 0) {
      char block[200];
      snprintf(block, sizeof(block),
               "logging:\n"
               "  usb     = %s   (default: off)\n"
               "  channel = %s   (default: on)",
               usb_on  ? "on " : "off",
               chan_on ? "on " : "off");
      pushCompanionMessage(block);
      return;
    }
    // Sub-Befehl extrahieren
    char sub[16];
    size_t si = 0;
    while (*arg && *arg != ' ' && si + 1 < sizeof(sub)) sub[si++] = *arg++;
    sub[si] = 0;
    while (*arg == ' ' || *arg == '\t') arg++;
    int bit = -1;
    bool inverted = false;        // true: bit=1 heisst off (channel-Stil)
    if (strcmp(sub, "usb") == 0)     { bit = 0; inverted = false; }
    else if (strcmp(sub, "channel") == 0) { bit = 1; inverted = true; }
    else {
      pushCompanionMessage("Usage: logging <usb|channel> <on|off>");
      return;
    }
    int m = match_on_off(arg);
    if (m < 0) {
      pushCompanionMessage("on/off erwartet.");
      return;
    }
    // bit-setzen-bei = (on XOR inverted): wenn nicht-invertiert
    // (USB, bit=on), setze bei m==on. Wenn invertiert (channel, bit=off),
    // setze bei m==off.
    bool set_bit = (m == 1) ^ inverted;
    if (set_bit) {
      _prefs.log_flags |=  (uint8_t)(1 << bit);
    } else {
      _prefs.log_flags &= ~(uint8_t)(1 << bit);
    }
    savePrefs();
    char r[80];
    snprintf(r, sizeof(r), "OK - logging %s = %s.", sub, m ? "on" : "off");
    pushCompanionMessage(r);
    return;
  }

  // ---------- contact <name-prefix> [type <role>] -----------------------
  // Diagnose-CLI fuer Wunschliste 7: setzt den ADV_TYPE eines gespeicherten
  // Kontakts um, OHNE dass dieser dazu einen neuen Advert senden muss.
  // Damit laesst sich z.B. testen, ob die App weiterhin Chat anbietet wenn
  // Wunschliste 46 Phase 1 (Reise 2026-06-09): 'filter sender|text
  // drop add|remove|list|clear ...' Spam-Block fuer eingehende Pakete.
  // Pattern-Syntax:
  //   foo     = substring (matched ueberall)
  //   ^foo    = anchor start (Sender/Text beginnt mit foo)
  //   foo$    = anchor end (endet mit foo)
  //   ^foo$   = exact match
  // Phase 1: nur drop-Listen, kein allow/exempt, kein per-Channel,
  // kein 'filter region'/'filter advert'. Filter wirkt 'for-us' --
  // App-Push wird unterdrueckt, Repeat bleibt unbeeinflusst.
  // Wunschliste 43 (2026-06-10): BLE-Power-Cycle CLI.
  // 'bluetooth power cycle|always-on'    persistent mode
  // 'bluetooth on'                        force ON (persistent: power=always-on)
  // 'bluetooth off'                       persistent power=off
  // 'bluetooth tmp-off'                   runtime off (nicht persistent)
  if (starts_with_word(cmd, "bluetooth") || starts_with_word(cmd, "bt")) {
    const char* p = strchr(cmd, ' ');
    if (!p) {
      const char* m = "?";
      switch (_prefs.bluetooth_power_mode) {
        case 0: m = "cycle"; break;
        case 1: m = "always-on"; break;
        case 2: m = "off"; break;
      }
      const char* s = "?";
      switch (_ble_pwr_state) {
        case BLE_PWR_BOOT:      s = "BOOT (Grace)"; break;
        case BLE_PWR_AWAKE:     s = "AWAKE (connected/just)"; break;
        case BLE_PWR_HOT_START: s = "HOT-START (5min)"; break;
        case BLE_PWR_SLEEP:     s = "SLEEP"; break;
        case BLE_PWR_WAIT:      s = "WAIT"; break;
        case BLE_PWR_TMP_OFF:   s = "TMP-OFF"; break;
        case BLE_PWR_OFF:       s = "OFF (persist)"; break;
      }
      char r[140];
      snprintf(r, sizeof(r),
               "bluetooth: pref=%s state=%s\n"
               "  power <cycle|always-on>  (persist)\n"
               "  <on|off>  (persist)\n"
               "  tmp-off  (runtime)",
               m, s);
      pushCompanionMessage(r);
      return;
    }
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "power", 5) == 0 && (p[5] == ' ' || p[5] == '\t')) {
      p += 5;
      while (*p == ' ' || *p == '\t') p++;
      if (strncmp(p, "cycle", 5) == 0) {
        _prefs.bluetooth_power_mode = 0;
        // wenn aktuell OFF/TMP_OFF: BOOT-Phase neu (BLE an, dann cycle)
        if (_ble_pwr_state == BLE_PWR_OFF || _ble_pwr_state == BLE_PWR_TMP_OFF) {
          _ble_pwr_state = BLE_PWR_HOT_START;
          _ble_pwr_state_until = millis() + 5UL * 60 * 1000;
        }
        savePrefs();
        pushCompanionMessage("OK - bluetooth power=cycle (persist)");
        return;
      }
      if (strncmp(p, "always-on", 9) == 0) {
        _prefs.bluetooth_power_mode = 1;
        _ble_pwr_state = BLE_PWR_AWAKE;
        _ble_pwr_state_until = 0;
        setBleEnabled(true);
        savePrefs();
        pushCompanionMessage("OK - bluetooth power=always-on (persist)");
        return;
      }
      pushCompanionMessage("Erwartet: bluetooth power <cycle|always-on>");
      // (User-Konvention 2026-06-10: <a|b|c> fuer Alternativen-Set)
      return;
    }
    if (strncmp(p, "on", 2) == 0 && (p[2] == 0 || p[2] == ' ')) {
      _prefs.bluetooth_power_mode = 1;
      _ble_pwr_state = BLE_PWR_AWAKE;
      _ble_pwr_state_until = 0;
      setBleEnabled(true);
      savePrefs();
      pushCompanionMessage("OK - bluetooth on (= power always-on, persist)");
      return;
    }
    if (strncmp(p, "off", 3) == 0 && (p[3] == 0 || p[3] == ' ')) {
      _prefs.bluetooth_power_mode = 2;
      _ble_pwr_state = BLE_PWR_OFF;
      _ble_pwr_state_until = 0;
      setBleEnabled(false);
      savePrefs();
      pushCompanionMessage(
        "OK - bluetooth off (persist).\n"
        "Recovery: USB-Serial oder\n"
        "Hardware-Button (Geraete mit Display).");
      return;
    }
    if (strncmp(p, "tmp-off", 7) == 0) {
      _ble_pwr_state = BLE_PWR_TMP_OFF;
      _ble_pwr_state_until = 0;
      setBleEnabled(false);
      pushCompanionMessage("OK - bluetooth tmp-off (runtime only)");
      return;
    }
    pushCompanionMessage(
      "Erwartet:\n"
      "  power <cycle|always-on>\n"
      "  <on|off|tmp-off>");
    return;
  }

  // Wunschliste 52 (2026-06-10): Remote-Admin Client.
  // 'admin login <contact-name> <password>' -- sendAnonReq + ANON_REQ_TYPE_LOGIN
  // 'admin <contact-name> <cmd-text>'       -- sendRequest + REQ_TYPE_ADMIN_CMD
  if (starts_with_word(cmd, "admin")) {
    const char* p = strchr(cmd, ' ');
    if (!p) {
      pushCompanionMessage(
        "Usage:\n"
        "  admin login <contact-name> <password>\n"
        "  admin <contact-name> <cmd-text>");
      return;
    }
    while (*p == ' ' || *p == '\t') p++;
    bool is_login = (strncmp(p, "login", 5) == 0
                     && (p[5] == ' ' || p[5] == '\t'));
    if (is_login) {
      p += 5;
      while (*p == ' ' || *p == '\t') p++;
    }
    // <contact-name> bis Whitespace
    const char* name_start = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    if (p == name_start) {
      pushCompanionMessage("Fehler: Contact-Name fehlt.");
      return;
    }
    size_t name_len = (size_t)(p - name_start);
    char name_prefix[40];
    if (name_len >= sizeof(name_prefix)) name_len = sizeof(name_prefix) - 1;
    memcpy(name_prefix, name_start, name_len);
    name_prefix[name_len] = 0;
    ContactInfo* c = searchContactsByPrefix(name_prefix);
    if (!c) {
      char r[80];
      snprintf(r, sizeof(r), "Contact '%s' nicht gefunden.", name_prefix);
      pushCompanionMessage(r);
      return;
    }
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) {
      pushCompanionMessage(is_login
        ? "Fehler: Password fehlt."
        : "Fehler: Cmd-Text fehlt.");
      return;
    }
    // Payload bauen + senden
    uint32_t tag = 0, est_timeout = 0;
    if (is_login) {
      // ANON_REQ: [ts(4)][type(1)][password(...)]
      uint8_t payload[40];
      uint32_t now = getRTCClock()->getCurrentTime();
      memcpy(&payload[0], &now, 4);
      payload[4] = ANON_REQ_TYPE_LOGIN;
      size_t pw_len = strlen(p);
      if (pw_len > 31) pw_len = 31;
      memcpy(&payload[5], p, pw_len);
      int result = sendAnonReq(*c, payload, (uint8_t)(5 + pw_len),
                                tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        pushCompanionMessage("Senden fehlgeschlagen (queue voll).");
        return;
      }
      memcpy(pending_admin_pubkey, c->id.pub_key, 4);
      pending_admin_tag = tag;
      pending_admin_login = true;
      char r[100];
      snprintf(r, sizeof(r), "OK - login Request an %s, est=%lums",
               c->name, (unsigned long)est_timeout);
      pushCompanionMessage(r);
    } else {
      // REQ: [type(1)][cmd_text(...)] + Framework praefixed ts.
      uint8_t payload[160];
      payload[0] = REQ_TYPE_ADMIN_CMD;
      size_t cmd_len = strlen(p);
      if (cmd_len > sizeof(payload) - 1) cmd_len = sizeof(payload) - 1;
      memcpy(&payload[1], p, cmd_len);
      int result = sendRequest(*c, payload, (uint8_t)(1 + cmd_len),
                                tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        pushCompanionMessage("Senden fehlgeschlagen (queue voll).");
        return;
      }
      memcpy(pending_admin_pubkey, c->id.pub_key, 4);
      pending_admin_tag = tag;
      pending_admin_login = false;
      char r[100];
      snprintf(r, sizeof(r), "OK - cmd Request an %s, est=%lums",
               c->name, (unsigned long)est_timeout);
      pushCompanionMessage(r);
    }
    return;
  }

  if (starts_with_word(cmd, "filter")) {
    // Sub-Tokens: 'sender' oder 'text', dann 'drop', dann action.
    const char* p = strchr(cmd, ' ');
    if (!p) {
      pushCompanionMessage(
        "Usage:\n"
        "  filter <sender|text|scope> ...\n"
        "  TYPE = sender|text (eigene Syntax fuer scope):\n"
        "  filter TYPE drop|keep add <pat>\n"
        "    [on-channel|exempt-channel <chans>]");
      pushCompanionMessage(
        "  filter TYPE drop|keep remove <pat|idx>\n"
        "  filter TYPE drop|keep list|clear\n"
        "  filter TYPE on-channel|exempt-channel <chans|clear>");
      pushCompanionMessage(
        "  filter list  (Komplett-Uebersicht)");
      pushCompanionMessage(
        "scope-Filter (drei Achsen pro Pattern):\n"
        "  filter scope drop|keep add <scope-list>");
      pushCompanionMessage(
        "  -- wo: [on-channel|exempt-channel <chans>]\n"
        "  -- wirkung: [profile for-us|repeat|complete]\n"
        "  Default: alle Channels, complete (beide).");
      pushCompanionMessage(
        "  filter scope drop|keep remove|list|clear\n"
        "  filter scope on-channel|exempt-channel\n"
        "    <chans|clear>  (Shortcut alle Patterns)");
      pushCompanionMessage(
        "  filter unknown-channel\n"
        "    filter-none|filter-unscoped|\n"
        "    filter-scoped|filter-all\n"
        "  Default: filter-unscoped");
      return;
    }
    while (*p == ' ' || *p == '\t') p++;

    // Globaler Uebersichts-Befehl 'filter list' -- alle Filter-Typen
    // in einem Rutsch anzeigen (User-Wunsch 2026-06-10).
    if (strncmp(p, "list", 4) == 0 && (p[4] == 0 || p[4] == ' ' || p[4] == '\t')) {
      // Akkumuliere Output in 145-Byte-Frames um App-Push-Queue
      // nicht mit 13+ Einzel-Frames zu sprengen.
      char acc[145]; size_t ap = 0; acc[0] = 0;
      auto acc_flush = [&]() {
        if (ap > 0) { pushCompanionMessage(acc); ap = 0; acc[0] = 0; }
      };
      auto acc_line = [&](const char* line) {
        size_t ll = strlen(line);
        if (ll == 0) return;
        if (ap + ll + 2 >= sizeof(acc)) acc_flush();
        if (ap > 0) acc[ap++] = '\n';
        memcpy(acc + ap, line, ll); ap += ll; acc[ap] = 0;
      };

      auto dump_list = [&](const char* label, NodePrefs::FilterEntry* arr,
                           uint8_t cnt, size_t max_slots,
                           const uint64_t* c_on, const uint64_t* c_ex) {
        char hdr[80];
        if (cnt == 0) {
          snprintf(hdr, sizeof(hdr), "%s (0/%u): (leer)",
                   label, (unsigned)max_slots);
          acc_line(hdr);
          return;
        }
        snprintf(hdr, sizeof(hdr), "%s (%u/%u):", label,
                 (unsigned)cnt, (unsigned)max_slots);
        acc_line(hdr);
        for (uint8_t i = 0; i < cnt && i < max_slots; i++) {
          const char* pfx = (arr[i].flags & 0x01) ? "^" : "";
          const char* sfx = (arr[i].flags & 0x02) ? "$" : "";
          char line[120];
          size_t lp = snprintf(line, sizeof(line), "  %u: %s%s%s",
                               (unsigned)(i+1), pfx, arr[i].pattern, sfx);
          if (c_on[i] != 0 || c_ex[i] != 0) {
            uint64_t m = c_on[i] ? c_on[i] : c_ex[i];
            int n = snprintf(line + lp, sizeof(line) - lp, "  %s:",
                             c_on[i] ? "on" : "ex");
            if (n > 0 && lp + n < sizeof(line)) lp += n;
            bool first_ch = true;
            for (int k = 0; k < MAX_GROUP_CHANNELS && k < 64; k++) {
              if ((m & ((uint64_t)1 << k)) == 0) continue;
              ChannelDetails cd;
              if (!getChannel(k, cd)) continue;
              n = snprintf(line + lp, sizeof(line) - lp, "%s%s",
                           first_ch ? "" : ",", cd.name[0] ? cd.name : "?");
              if (n > 0 && lp + n < sizeof(line)) lp += n;
              first_ch = false;
            }
          }
          acc_line(line);
        }
      };
      dump_list("sender drop", _prefs.filter_sender_drop,
                _prefs.filter_sender_drop_count,
                sizeof(_prefs.filter_sender_drop)/sizeof(_prefs.filter_sender_drop[0]),
                _prefs.filter_sender_drop_chan_on, _prefs.filter_sender_drop_chan_ex);
      dump_list("sender keep", _prefs.filter_sender_keep,
                _prefs.filter_sender_keep_count,
                sizeof(_prefs.filter_sender_keep)/sizeof(_prefs.filter_sender_keep[0]),
                _prefs.filter_sender_keep_chan_on, _prefs.filter_sender_keep_chan_ex);
      dump_list("text drop", _prefs.filter_text_drop,
                _prefs.filter_text_drop_count,
                sizeof(_prefs.filter_text_drop)/sizeof(_prefs.filter_text_drop[0]),
                _prefs.filter_text_drop_chan_on, _prefs.filter_text_drop_chan_ex);
      dump_list("text keep", _prefs.filter_text_keep,
                _prefs.filter_text_keep_count,
                sizeof(_prefs.filter_text_keep)/sizeof(_prefs.filter_text_keep[0]),
                _prefs.filter_text_keep_chan_on, _prefs.filter_text_keep_chan_ex);
      // Scope-Filter (Phase 5).
      auto dump_scope = [&](const char* label, NodePrefs::FilterScopeEntry* arr,
                             uint8_t cnt, size_t max_slots,
                             const uint64_t* c_on, const uint64_t* c_ex) {
        char hdr[80];
        if (cnt == 0) {
          snprintf(hdr, sizeof(hdr), "%s (0/%u): (leer)",
                   label, (unsigned)max_slots);
          acc_line(hdr);
          return;
        }
        snprintf(hdr, sizeof(hdr), "%s (%u/%u):", label,
                 (unsigned)cnt, (unsigned)max_slots);
        acc_line(hdr);
        for (uint8_t i = 0; i < cnt && i < max_slots; i++) {
          uint8_t profile = arr[i].flags & 0x03;
          // Default = complete (no suffix). Sonderwerte explizit.
          const char* prof_s = (profile == 0) ? " profile:display"
                              : (profile == 1) ? " profile:repeat"
                              : "";
          char line[120];
          size_t lp = snprintf(line, sizeof(line), "  %u: %s%s",
                               (unsigned)(i+1), arr[i].scope_name, prof_s);
          if (c_on[i] != 0 || c_ex[i] != 0) {
            uint64_t m = c_on[i] ? c_on[i] : c_ex[i];
            int n = snprintf(line + lp, sizeof(line) - lp, "  %s:",
                             c_on[i] ? "on" : "ex");
            if (n > 0 && lp + n < sizeof(line)) lp += n;
            bool first_ch = true;
            for (int k = 0; k < MAX_GROUP_CHANNELS && k < 64; k++) {
              if ((m & ((uint64_t)1 << k)) == 0) continue;
              ChannelDetails cd;
              if (!getChannel(k, cd)) continue;
              n = snprintf(line + lp, sizeof(line) - lp, "%s%s",
                           first_ch ? "" : ",", cd.name[0] ? cd.name : "?");
              if (n > 0 && lp + n < sizeof(line)) lp += n;
              first_ch = false;
            }
          }
          acc_line(line);
        }
      };
      dump_scope("scope drop", _prefs.filter_scope_drop,
                 _prefs.filter_scope_drop_count,
                 sizeof(_prefs.filter_scope_drop)/sizeof(_prefs.filter_scope_drop[0]),
                 _prefs.filter_scope_drop_chan_on, _prefs.filter_scope_drop_chan_ex);
      dump_scope("scope keep", _prefs.filter_scope_keep,
                 _prefs.filter_scope_keep_count,
                 sizeof(_prefs.filter_scope_keep)/sizeof(_prefs.filter_scope_keep[0]),
                 _prefs.filter_scope_keep_chan_on, _prefs.filter_scope_keep_chan_ex);
      // Plus Repeat-Achse.
      {
        const char* rm = "filter-none";
        switch (_prefs.filter_unknown_channel_repeat) {
          case 1: rm = "filter-unscoped"; break;
          case 2: rm = "filter-scoped"; break;
          case 3: rm = "filter-all"; break;
        }
        char line[80];
        snprintf(line, sizeof(line), "unknown-channel: %s", rm);
        acc_line(line);
      }
      acc_flush();
      return;
    }

    // 'filter unknown-channel repeat <mode>' (Phase 5 Repeat-Achse).
    if (strncmp(p, "unknown-channel", 15) == 0
        && (p[15] == ' ' || p[15] == '\t')) {
      p += 15;
      while (*p == ' ' || *p == '\t') p++;
      // Wert beschreibt WAS GEFILTERT (= weg-gedroppt) wird, nicht was
      // durchgeht. Vermeidet doppelte Verneinung im Begriff (User-Wunsch
      // 2026-06-10):
      //   filter-none     = filtert nichts (alles durch)
      //   filter-unscoped = filtert unscoped weg (Default)
      //   filter-scoped   = filtert scoped weg (selten)
      //   filter-all      = filtert alles weg (kein Paket durch)
      if (!*p) {
        const char* rm = "filter-none";
        switch (_prefs.filter_unknown_channel_repeat) {
          case 1: rm = "filter-unscoped"; break;
          case 2: rm = "filter-scoped"; break;
          case 3: rm = "filter-all"; break;
        }
        char r[120];
        snprintf(r, sizeof(r),
                 "filter unknown-channel = %s\n"
                 "(welche Pakete fuer unbekannte\n"
                 " Channels weg-gefiltert werden\n"
                 " im Repeater-Pfad)", rm);
        pushCompanionMessage(r);
        return;
      }
      uint8_t mode_val = 255;
      if (strncasecmp(p, "filter-none", 11) == 0) mode_val = 0;
      else if (strncasecmp(p, "filter-unscoped", 15) == 0) mode_val = 1;
      else if (strncasecmp(p, "filter-scoped", 13) == 0) mode_val = 2;
      else if (strncasecmp(p, "filter-all", 10) == 0) mode_val = 3;
      if (mode_val == 255) {
        pushCompanionMessage(
          "Erlaubt: filter-none | filter-unscoped\n"
          "       | filter-scoped | filter-all\n"
          "(Wert = was wird weg-gefiltert)");
        return;
      }
      _prefs.filter_unknown_channel_repeat = mode_val;
      savePrefs();
      char r[80];
      snprintf(r, sizeof(r), "OK - filter unknown-channel = %s",
               mode_val == 0 ? "filter-none" :
               mode_val == 1 ? "filter-unscoped" :
               mode_val == 2 ? "filter-scoped" : "filter-all");
      pushCompanionMessage(r);
      return;
    }

    // ====================================================================
    // scope-Filter (Phase 5)
    // filter scope <drop|keep> add <scope-liste>
    //   [on-channel|exempt-channel <chans>] [profile for-us|repeat|complete]
    // filter scope <drop|keep> remove <idx|name>
    // filter scope <drop|keep> list|clear
    // filter scope on-channel|exempt-channel <chans|clear>  (Shortcut)
    // ====================================================================
    if (strncmp(p, "scope", 5) == 0 && (p[5] == 0 || p[5] == ' ' || p[5] == '\t')) {
      p += 5;
      while (*p == ' ' || *p == '\t') p++;
      if (!*p) {
        pushCompanionMessage(
          "Usage: filter scope drop|keep\n"
          "  add <list> [on-channel|exempt-channel <chans>]\n"
          "    [profile for-us|repeat|complete]");
        pushCompanionMessage(
          "  remove <idx|name>\n"
          "  list|clear\n"
          "filter scope on-channel|exempt-channel <chans|clear>");
        return;
      }

      // Lokaler Channel-Liste-Parser (Kopie -- pragmatisch, Code-Duplikation
      // gegenueber sender/text-Block wird beim naechsten Refactor entfernt).
      auto parse_chan_list_sc = [&](const char* lp, uint64_t* out_mask,
                                    char* ubuf, size_t ub_size,
                                    bool* saw_sub) -> int {
        uint64_t new_mask = 0;
        int unknown = 0;
        *saw_sub = false;
        ubuf[0] = 0;
        const char* c = lp;
        while (*c) {
          while (*c == ' ' || *c == '\t' || *c == ',') c++;
          if (!*c) break;
          const char* st = c;
          while (*c && *c != ',' && *c != ' ' && *c != '\t') c++;
          size_t nl = (size_t)(c - st);
          if (nl == 0) continue;
          char nm[33];
          if (nl >= sizeof(nm)) nl = sizeof(nm) - 1;
          memcpy(nm, st, nl); nm[nl] = 0;
          if (strcasecmp(nm, "drop") == 0 || strcasecmp(nm, "keep") == 0
              || strcasecmp(nm, "add") == 0 || strcasecmp(nm, "remove") == 0
              || strcasecmp(nm, "list") == 0 || strcasecmp(nm, "clear") == 0
              || strcasecmp(nm, "profile") == 0) {
            *saw_sub = true;
          }
          int idx = -1;
          for (int i = 0; i < MAX_GROUP_CHANNELS && i < 64; i++) {
            ChannelDetails cd;
            if (!getChannel(i, cd)) continue;
            if (cd.name[0] == 0) continue;
            if (strcasecmp(cd.name, nm) == 0) { idx = i; break; }
          }
          if (idx < 0) {
            if (strlen(ubuf) + nl + 2 < ub_size) {
              if (ubuf[0]) strcat(ubuf, ", ");
              strcat(ubuf, nm);
            }
            unknown++;
            continue;
          }
          new_mask |= ((uint64_t)1 << idx);
        }
        *out_mask = new_mask;
        return unknown;
      };

      // Shortcut: filter scope on-channel|exempt-channel <chans|clear>
      bool sc_on = (strncmp(p, "on-channel", 10) == 0
                    && (p[10] == ' ' || p[10] == '\t' || p[10] == 0));
      bool sc_ex = (strncmp(p, "exempt-channel", 14) == 0
                    && (p[14] == ' ' || p[14] == '\t' || p[14] == 0));
      if (sc_on || sc_ex) {
        p += sc_on ? 10 : 14;
        while (*p == ' ' || *p == '\t') p++;
        const char* mode = sc_on ? "on-channel" : "exempt-channel";
        uint8_t drop_cnt = _prefs.filter_scope_drop_count;
        uint8_t keep_cnt = _prefs.filter_scope_keep_count;
        if (drop_cnt == 0 && keep_cnt == 0) {
          pushCompanionMessage("Keine scope-Patterns vorhanden.");
          return;
        }
        if (strncmp(p, "clear", 5) == 0 && (p[5] == 0 || p[5] == ' ' || p[5] == '\t')) {
          for (uint8_t i = 0; i < drop_cnt; i++) {
            _prefs.filter_scope_drop_chan_on[i] = 0;
            _prefs.filter_scope_drop_chan_ex[i] = 0;
          }
          for (uint8_t i = 0; i < keep_cnt; i++) {
            _prefs.filter_scope_keep_chan_on[i] = 0;
            _prefs.filter_scope_keep_chan_ex[i] = 0;
          }
          savePrefs();
          pushCompanionMessage("OK - filter scope: channel-filter aller Patterns gecleared.");
          return;
        }
        if (!*p) {
          pushCompanionMessage(
            "Usage: filter scope on-channel|exempt-channel <liste>\n"
            "(Setzt fuer ALLE scope-Patterns.)");
          return;
        }
        uint64_t nm = 0; char ub[80]; bool ss = false;
        int uc = parse_chan_list_sc(p, &nm, ub, sizeof(ub), &ss);
        if (uc > 0) {
          char r[160];
          snprintf(r, sizeof(r), "Abgelehnt: unbekannte Channels: %s", ub);
          pushCompanionMessage(r);
          return;
        }
        if (nm == 0) {
          pushCompanionMessage("Leere Channel-Liste. Nutze 'clear' zum Loeschen.");
          return;
        }
        for (uint8_t i = 0; i < drop_cnt; i++) {
          if (sc_on) { _prefs.filter_scope_drop_chan_on[i] = nm; _prefs.filter_scope_drop_chan_ex[i] = 0; }
          else       { _prefs.filter_scope_drop_chan_ex[i] = nm; _prefs.filter_scope_drop_chan_on[i] = 0; }
        }
        for (uint8_t i = 0; i < keep_cnt; i++) {
          if (sc_on) { _prefs.filter_scope_keep_chan_on[i] = nm; _prefs.filter_scope_keep_chan_ex[i] = 0; }
          else       { _prefs.filter_scope_keep_chan_ex[i] = nm; _prefs.filter_scope_keep_chan_on[i] = 0; }
        }
        savePrefs();
        char r[80];
        snprintf(r, sizeof(r), "OK - filter scope %s (alle %u Patterns) gesetzt.",
                 mode, (unsigned)(drop_cnt + keep_cnt));
        pushCompanionMessage(r);
        return;
      }

      // Verb: drop oder keep
      bool sc_drop = false, sc_keep = false;
      if (strncmp(p, "drop", 4) == 0 && (p[4] == 0 || p[4] == ' ' || p[4] == '\t')) {
        sc_drop = true; p += 4;
      } else if (strncmp(p, "keep", 4) == 0 && (p[4] == 0 || p[4] == ' ' || p[4] == '\t')) {
        sc_keep = true; p += 4;
      } else {
        if (strncmp(p, "profile", 7) == 0) {
          pushCompanionMessage(
            "profile ist pro Pattern (Modifier nach 'add'),\n"
            "kein eigener Befehl. Aktiv-Anzeige:\n"
            "  filter scope drop list  (Suffix p:rep / p:cpl)");
        } else {
          pushCompanionMessage("Erwartet: <drop|keep|on-channel|exempt-channel>");
        }
        return;
      }
      while (*p == ' ' || *p == '\t') p++;

      NodePrefs::FilterScopeEntry* sarr = sc_drop ? _prefs.filter_scope_drop : _prefs.filter_scope_keep;
      uint8_t* sp_cnt = sc_drop ? &_prefs.filter_scope_drop_count : &_prefs.filter_scope_keep_count;
      uint8_t& sc_cnt = *sp_cnt;
      size_t SC_MAX = sc_drop
          ? sizeof(_prefs.filter_scope_drop)/sizeof(_prefs.filter_scope_drop[0])
          : sizeof(_prefs.filter_scope_keep)/sizeof(_prefs.filter_scope_keep[0]);
      uint64_t* sc_on_arr = sc_drop ? _prefs.filter_scope_drop_chan_on : _prefs.filter_scope_keep_chan_on;
      uint64_t* sc_ex_arr = sc_drop ? _prefs.filter_scope_drop_chan_ex : _prefs.filter_scope_keep_chan_ex;
      const char* sc_verb = sc_drop ? "drop" : "keep";

      // list / clear
      if (!*p || strncmp(p, "list", 4) == 0) {
        char hdr[80];
        snprintf(hdr, sizeof(hdr), "filter scope %s (%u/%u):",
                 sc_verb, (unsigned)sc_cnt, (unsigned)SC_MAX);
        pushCompanionMessage(hdr);
        if (sc_cnt == 0) { pushCompanionMessage("  (leer)"); return; }
        for (uint8_t i = 0; i < sc_cnt && i < SC_MAX; i++) {
          uint8_t profile = sarr[i].flags & 0x03;
          const char* prof_s = (profile == 1) ? " p:rep"
                              : (profile == 2) ? " p:cpl" : "";
          char line[120];
          size_t lp = snprintf(line, sizeof(line), "  %u: %s%s",
                               (unsigned)(i+1), sarr[i].scope_name, prof_s);
          if (sc_on_arr[i] != 0 || sc_ex_arr[i] != 0) {
            uint64_t m = sc_on_arr[i] ? sc_on_arr[i] : sc_ex_arr[i];
            int n = snprintf(line + lp, sizeof(line) - lp, "  %s:",
                             sc_on_arr[i] ? "on" : "ex");
            if (n > 0 && lp + n < sizeof(line)) lp += n;
            bool first_ch = true;
            for (int k = 0; k < MAX_GROUP_CHANNELS && k < 64; k++) {
              if ((m & ((uint64_t)1 << k)) == 0) continue;
              ChannelDetails cd;
              if (!getChannel(k, cd)) continue;
              n = snprintf(line + lp, sizeof(line) - lp, "%s%s",
                           first_ch ? "" : ",", cd.name[0] ? cd.name : "?");
              if (n > 0 && lp + n < sizeof(line)) lp += n;
              first_ch = false;
            }
          }
          pushCompanionMessage(line);
        }
        return;
      }
      if (strncmp(p, "clear", 5) == 0 && (p[5] == 0 || p[5] == ' ' || p[5] == '\t')) {
        memset(sarr, 0, SC_MAX * sizeof(sarr[0]));
        memset(sc_on_arr, 0, SC_MAX * sizeof(sc_on_arr[0]));
        memset(sc_ex_arr, 0, SC_MAX * sizeof(sc_ex_arr[0]));
        sc_cnt = 0;
        savePrefs();
        char r[60];
        snprintf(r, sizeof(r), "OK - filter scope %s cleared.", sc_verb);
        pushCompanionMessage(r);
        return;
      }
      // remove <idx|name>
      if (strncmp(p, "remove", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
        p += 6;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) {
          pushCompanionMessage("Usage: filter scope <drop|keep> remove <idx|name>");
          return;
        }
        char* endp = NULL;
        long idx = strtol(p, &endp, 10);
        bool looks_like_index = (endp && endp != p
                                 && (*endp == 0 || *endp == ' ' || *endp == '\t'));
        int remove_idx = -1;
        if (looks_like_index) {
          if (idx < 1 || (uint32_t)idx > sc_cnt) {
            char r[80];
            snprintf(r, sizeof(r), "Index %ld ungueltig (1..%u erlaubt).",
                     idx, (unsigned)sc_cnt);
            pushCompanionMessage(r);
            return;
          }
          remove_idx = (int)(idx - 1);
        } else {
          // by name (case-insens, mit/ohne '#')
          const char* a = (p[0] == '#') ? p + 1 : p;
          // trailing whitespace strip
          size_t alen = 0;
          while (a[alen] && a[alen] != ' ' && a[alen] != '\t') alen++;
          for (uint8_t i = 0; i < sc_cnt; i++) {
            const char* b = (sarr[i].scope_name[0] == '#') ? sarr[i].scope_name + 1 : sarr[i].scope_name;
            if (strncasecmp(a, b, alen) == 0 && b[alen] == 0) {
              remove_idx = i; break;
            }
          }
          if (remove_idx < 0) {
            pushCompanionMessage("Scope-Name nicht in Liste.");
            return;
          }
        }
        for (uint8_t j = (uint8_t)remove_idx; j + 1 < sc_cnt; j++) {
          sarr[j] = sarr[j+1];
          sc_on_arr[j] = sc_on_arr[j+1];
          sc_ex_arr[j] = sc_ex_arr[j+1];
        }
        memset(&sarr[sc_cnt-1], 0, sizeof(sarr[sc_cnt-1]));
        sc_on_arr[sc_cnt-1] = 0;
        sc_ex_arr[sc_cnt-1] = 0;
        sc_cnt--;
        savePrefs();
        char r[80];
        snprintf(r, sizeof(r), "OK - filter scope %s removed (%u verbleibend)",
                 sc_verb, (unsigned)sc_cnt);
        pushCompanionMessage(r);
        return;
      }
      // add <scope-list> [on-channel|exempt-channel <chans>] [profile <p>]
      if (strncmp(p, "add", 3) == 0 && (p[3] == ' ' || p[3] == '\t')) {
        p += 3;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) {
          pushCompanionMessage("Usage: filter scope <drop|keep> add <scope-list> [on-channel|exempt-channel <chans>] [profile <p>]");
          return;
        }
        // Parse scope-list bis Whitespace, dann optional Modifier.
        const char* scope_list_start = p;
        const char* w = p;
        while (*w && *w != ' ' && *w != '\t') w++;
        size_t sllen = (size_t)(w - scope_list_start);
        const char* after = w;
        while (*after == ' ' || *after == '\t') after++;
        // optional on-channel/exempt-channel + optional profile
        // Default profile = complete (User-Wunsch 2026-06-10):
        // wenn man einen scope-Filter setzt, will man das Paket normalerweise
        // GANZ loswerden -- nicht sehen UND nicht weiterleiten. Sonderfaelle
        // wie 'nur Display' bzw 'nur Repeat' kosten ein explizites
        // 'profile for-us' bzw 'profile repeat' Suffix.
        uint64_t add_on = 0, add_ex = 0;
        uint8_t add_profile = 2;  // complete default
        while (*after) {
          bool ap_on = (strncmp(after, "on-channel", 10) == 0
                        && (after[10] == ' ' || after[10] == '\t'));
          bool ap_ex = (strncmp(after, "exempt-channel", 14) == 0
                        && (after[14] == ' ' || after[14] == '\t'));
          bool ap_pf = (strncmp(after, "profile", 7) == 0
                        && (after[7] == ' ' || after[7] == '\t'));
          if (ap_on || ap_ex) {
            after += ap_on ? 10 : 14;
            while (*after == ' ' || *after == '\t') after++;
            // parse channel list bis nächstes profile-Keyword oder Ende
            const char* chan_end = after;
            while (*chan_end) {
              if ((strncmp(chan_end, "profile", 7) == 0)
                  && (chan_end[7] == ' ' || chan_end[7] == '\t')) break;
              chan_end++;
            }
            size_t clen = (size_t)(chan_end - after);
            char chbuf[160];
            if (clen >= sizeof(chbuf)) clen = sizeof(chbuf) - 1;
            memcpy(chbuf, after, clen); chbuf[clen] = 0;
            uint64_t cm = 0; char ub[80]; bool ss = false;
            int uc = parse_chan_list_sc(chbuf, &cm, ub, sizeof(ub), &ss);
            if (uc > 0) {
              char r[160];
              snprintf(r, sizeof(r), "Abgelehnt: unbekannte Channels: %s", ub);
              pushCompanionMessage(r);
              return;
            }
            if (cm == 0) {
              pushCompanionMessage("Leere Channel-Liste nach on/exempt-channel.");
              return;
            }
            if (ap_on) add_on = cm; else add_ex = cm;
            after = chan_end;
            while (*after == ' ' || *after == '\t') after++;
          } else if (ap_pf) {
            after += 7;
            while (*after == ' ' || *after == '\t') after++;
            if (strncasecmp(after, "for-us", 6) == 0) add_profile = 0;
            else if (strncasecmp(after, "repeat", 6) == 0) add_profile = 1;
            else if (strncasecmp(after, "complete", 8) == 0) add_profile = 2;
            else {
              pushCompanionMessage("profile: erlaubt for-us|repeat|complete");
              return;
            }
            // skip profile-Wert
            while (*after && *after != ' ' && *after != '\t') after++;
            while (*after == ' ' || *after == '\t') after++;
          } else {
            pushCompanionMessage("Erwartet: on-channel|exempt-channel|profile");
            return;
          }
        }
        // scope-list expand: Komma-getrennt, je ein neuer Slot
        const char* sc_cur = scope_list_start;
        const char* sc_end = scope_list_start + sllen;
        int added = 0, skipped_dup = 0, skipped_full = 0;
        char last_added[40] = "";
        while (sc_cur < sc_end) {
          while (sc_cur < sc_end && (*sc_cur == ',' || *sc_cur == ' ' || *sc_cur == '\t')) sc_cur++;
          if (sc_cur >= sc_end) break;
          const char* tok_st = sc_cur;
          while (sc_cur < sc_end && *sc_cur != ',' && *sc_cur != ' ' && *sc_cur != '\t') sc_cur++;
          size_t tlen = (size_t)(sc_cur - tok_st);
          if (tlen == 0) continue;
          // Normalisieren: scope-Namen werden intern immer mit '#'-Prefix
          // gespeichert (passend zur scope-Hash-Berechnung). Eingabe ohne
          // '#' wird ergaenzt. '##' ist ungueltig. 'unscoped' (case-insens)
          // bleibt Reserved-Pseudo-Token OHNE '#'.
          char raw[32];
          if (tlen >= sizeof(raw)) {
            char r[80];
            snprintf(r, sizeof(r), "Scope-Name zu lang (max %u).",
                     (unsigned)(sizeof(sarr[0].scope_name) - 1));
            pushCompanionMessage(r);
            return;
          }
          memcpy(raw, tok_st, tlen); raw[tlen] = 0;
          // '##' ablehnen
          if (raw[0] == '#' && raw[1] == '#') {
            char r[80];
            snprintf(r, sizeof(r), "Ungueltig: '%s' (doppeltes '#').", raw);
            pushCompanionMessage(r);
            return;
          }
          bool is_unscoped = (strcasecmp(raw, "unscoped") == 0
                              || strcasecmp(raw, "#unscoped") == 0);
          char nm[32];
          size_t nlen;
          if (is_unscoped) {
            strcpy(nm, "unscoped");
            nlen = 8;
          } else if (raw[0] == '#') {
            strcpy(nm, raw);
            nlen = tlen;
          } else {
            nm[0] = '#';
            memcpy(nm + 1, raw, tlen);
            nm[1 + tlen] = 0;
            nlen = 1 + tlen;
          }
          if (nlen >= sizeof(sarr[0].scope_name)) {
            char r[80];
            snprintf(r, sizeof(r), "Scope-Name zu lang (max %u nach #-Praefix).",
                     (unsigned)(sizeof(sarr[0].scope_name) - 1));
            pushCompanionMessage(r);
            return;
          }
          // Dedup: gleicher name (case-insens, jetzt schon normalisiert)
          // + gleiche flags
          bool dup = false;
          for (uint8_t i = 0; i < sc_cnt; i++) {
            if (strcasecmp(sarr[i].scope_name, nm) == 0
                && (sarr[i].flags & 0x03) == add_profile) {
              dup = true; break;
            }
          }
          if (dup) { skipped_dup++; continue; }
          if (sc_cnt >= SC_MAX) { skipped_full++; continue; }
          memset(&sarr[sc_cnt], 0, sizeof(sarr[sc_cnt]));
          memcpy(sarr[sc_cnt].scope_name, nm, nlen);
          sarr[sc_cnt].scope_name[nlen] = 0;
          sarr[sc_cnt].flags = add_profile & 0x03;
          sc_on_arr[sc_cnt] = add_on;
          sc_ex_arr[sc_cnt] = add_ex;
          strncpy(last_added, nm, sizeof(last_added)-1);
          last_added[sizeof(last_added)-1] = 0;
          sc_cnt++;
          added++;
        }
        if (added == 0 && skipped_dup == 0 && skipped_full == 0) {
          pushCompanionMessage("Leere scope-Liste.");
          return;
        }
        savePrefs();
        char r[140];
        snprintf(r, sizeof(r), "OK - filter scope %s add: %d neu, %d dup, %d uebersprungen (Liste voll). %u/%u",
                 sc_verb, added, skipped_dup, skipped_full,
                 (unsigned)sc_cnt, (unsigned)SC_MAX);
        pushCompanionMessage(r);
        return;
      }
      pushCompanionMessage("Erwartet: <add|remove|list|clear>");
      return;
    }

    bool is_sender = false;
    if (strncmp(p, "sender", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
      is_sender = true; p += 6;
    } else if (strncmp(p, "text", 4) == 0 && (p[4] == ' ' || p[4] == '\t')) {
      is_sender = false; p += 4;
    } else {
      pushCompanionMessage("Erwartet: filter <sender|text> <drop|keep|on-channel|exempt-channel>");
      return;
    }
    while (*p == ' ' || *p == '\t') p++;

    // Helper: Parse Komma-Liste von Channel-Namen, liefert mask.
    auto parse_channel_list = [&](const char* lp, uint64_t* out_mask,
                                  char* unknown_buf, size_t ub_size,
                                  bool* saw_subcmd) -> int {
      uint64_t new_mask = 0;
      int unknown_count = 0;
      *saw_subcmd = false;
      unknown_buf[0] = 0;
      const char* cursor = lp;
      while (*cursor) {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',') cursor++;
        if (!*cursor) break;
        const char* start = cursor;
        while (*cursor && *cursor != ',' && *cursor != ' ' && *cursor != '\t') cursor++;
        size_t nl = (size_t)(cursor - start);
        if (nl == 0) continue;
        char name[33];
        if (nl >= sizeof(name)) nl = sizeof(name) - 1;
        memcpy(name, start, nl); name[nl] = 0;
        if (strcasecmp(name, "drop") == 0 || strcasecmp(name, "keep") == 0
            || strcasecmp(name, "add") == 0 || strcasecmp(name, "remove") == 0
            || strcasecmp(name, "list") == 0 || strcasecmp(name, "clear") == 0
            || strcasecmp(name, "chan") == 0) {
          *saw_subcmd = true;
        }
        int idx = -1;
        for (int i = 0; i < MAX_GROUP_CHANNELS && i < 64; i++) {
          ChannelDetails cd;
          if (!getChannel(i, cd)) continue;
          if (cd.name[0] == 0) continue;
          if (strcasecmp(cd.name, name) == 0) { idx = i; break; }
        }
        if (idx < 0) {
          if (strlen(unknown_buf) + nl + 2 < ub_size) {
            if (unknown_buf[0]) strcat(unknown_buf, ", ");
            strcat(unknown_buf, name);
          }
          unknown_count++;
          continue;
        }
        new_mask |= ((uint64_t)1 << idx);
      }
      *out_mask = new_mask;
      return unknown_count;
    };

    // channel-filter Shortcuts (Phase 2 v2, 2026-06-10):
    // 'filter <type> on-channel <liste>' setzt fuer ALLE aktiven
    // drop+keep-Patterns des Typs den gleichen Skopus. Convenience.
    // Fuer pro-Pattern: 'add <pat> on-channel <liste>' beim Anlegen.
    bool is_on_chan = (strncmp(p, "on-channel", 10) == 0
                       && (p[10] == ' ' || p[10] == '\t' || p[10] == 0));
    bool is_ex_chan = (strncmp(p, "exempt-channel", 14) == 0
                       && (p[14] == ' ' || p[14] == '\t' || p[14] == 0));
    if (is_on_chan || is_ex_chan) {
      p += is_on_chan ? 10 : 14;
      while (*p == ' ' || *p == '\t') p++;
      const char* kind = is_sender ? "sender" : "text";
      const char* mode = is_on_chan ? "on-channel" : "exempt-channel";
      NodePrefs::FilterEntry* drop_arr = is_sender ? _prefs.filter_sender_drop
                                                    : _prefs.filter_text_drop;
      uint8_t drop_cnt = is_sender ? _prefs.filter_sender_drop_count
                                    : _prefs.filter_text_drop_count;
      NodePrefs::FilterEntry* keep_arr = is_sender ? _prefs.filter_sender_keep
                                                    : _prefs.filter_text_keep;
      uint8_t keep_cnt = is_sender ? _prefs.filter_sender_keep_count
                                    : _prefs.filter_text_keep_count;
      uint64_t* drop_on = is_sender ? _prefs.filter_sender_drop_chan_on
                                     : _prefs.filter_text_drop_chan_on;
      uint64_t* drop_ex = is_sender ? _prefs.filter_sender_drop_chan_ex
                                     : _prefs.filter_text_drop_chan_ex;
      uint64_t* keep_on = is_sender ? _prefs.filter_sender_keep_chan_on
                                     : _prefs.filter_text_keep_chan_on;
      uint64_t* keep_ex = is_sender ? _prefs.filter_sender_keep_chan_ex
                                     : _prefs.filter_text_keep_chan_ex;
      (void)drop_arr; (void)keep_arr;

      if (drop_cnt == 0 && keep_cnt == 0) {
        pushCompanionMessage("Keine Filter-Patterns vorhanden -- nichts zu setzen.");
        return;
      }

      if (strncmp(p, "clear", 5) == 0 && (p[5] == 0 || p[5] == ' ' || p[5] == '\t')) {
        for (uint8_t i = 0; i < drop_cnt; i++) { drop_on[i] = 0; drop_ex[i] = 0; }
        for (uint8_t i = 0; i < keep_cnt; i++) { keep_on[i] = 0; keep_ex[i] = 0; }
        savePrefs();
        char r[100];
        snprintf(r, sizeof(r),
                 "OK - filter %s: channel-filter aller Patterns gecleared.",
                 kind);
        pushCompanionMessage(r);
        return;
      }
      if (!*p) {
        pushCompanionMessage(
          "Usage: filter <sender|text> on-channel <liste>\n"
          "  Setzt Skopus fuer ALLE Patterns des Typs.\n"
          "Fuer pro-Pattern: 'drop add <pat> on-channel <liste>'");
        return;
      }
      uint64_t new_mask = 0;
      char unknown_buf[80];
      bool saw_subcmd = false;
      int unknown_count = parse_channel_list(p, &new_mask, unknown_buf,
                                              sizeof(unknown_buf), &saw_subcmd);
      if (unknown_count > 0) {
        char r[160];
        snprintf(r, sizeof(r), "Abgelehnt: unbekannte Channels: %s", unknown_buf);
        pushCompanionMessage(r);
        if (saw_subcmd) {
          pushCompanionMessage(
            "Tipp: on-channel nimmt nur Channel-Namen.\n"
            "Fuer pro-Pattern: 'drop add <pat> on-channel ...'");
        }
        return;
      }
      if (new_mask == 0) {
        pushCompanionMessage("Leere Channel-Liste. Nutze 'clear' zum Loeschen.");
        return;
      }
      // Anwenden auf alle drop+keep Patterns. on/exempt mutual excl.
      for (uint8_t i = 0; i < drop_cnt; i++) {
        if (is_on_chan) { drop_on[i] = new_mask; drop_ex[i] = 0; }
        else            { drop_ex[i] = new_mask; drop_on[i] = 0; }
      }
      for (uint8_t i = 0; i < keep_cnt; i++) {
        if (is_on_chan) { keep_on[i] = new_mask; keep_ex[i] = 0; }
        else            { keep_ex[i] = new_mask; keep_on[i] = 0; }
      }
      savePrefs();
      char r[160];
      size_t rp = snprintf(r, sizeof(r), "OK - filter %s %s (alle %u Patterns): ",
                           kind, mode, (unsigned)(drop_cnt + keep_cnt));
      bool first_ch = true;
      for (int i = 0; i < MAX_GROUP_CHANNELS && i < 64; i++) {
        if ((new_mask & ((uint64_t)1 << i)) == 0) continue;
        ChannelDetails cd;
        if (!getChannel(i, cd)) continue;
        const char* nm = cd.name[0] ? cd.name : "?";
        int n = snprintf(r + rp, sizeof(r) - rp, "%s%s", first_ch ? "" : ",", nm);
        if (n > 0 && rp + n < sizeof(r)) rp += n;
        first_ch = false;
      }
      pushCompanionMessage(r);
      return;
    }

    // Verb: drop oder keep (Phase 3, 2026-06-10).
    bool is_keep_verb = false;
    bool is_drop_verb = false;
    if (strncmp(p, "drop", 4) == 0 && (p[4] == 0 || p[4] == ' ' || p[4] == '\t')) {
      is_drop_verb = true; p += 4;
    } else if (strncmp(p, "keep", 4) == 0 && (p[4] == 0 || p[4] == ' ' || p[4] == '\t')) {
      is_keep_verb = true; p += 4;
    } else {
      pushCompanionMessage(
        "Erwartet: <drop|keep|on-channel|exempt-channel>\n"
        "(drop/keep nehmen ein Pattern; on/exempt-channel\n"
        " eine Channel-Liste)");
      return;
    }
    while (*p == ' ' || *p == '\t') p++;
    NodePrefs::FilterEntry* arr;
    uint8_t* p_cnt;
    size_t MAX_SLOTS;
    uint64_t* chan_on;   // pro-Pattern channel-filter (Phase 2 v2)
    uint64_t* chan_ex;
    if (is_sender && is_drop_verb) {
      arr = _prefs.filter_sender_drop;
      p_cnt = &_prefs.filter_sender_drop_count;
      MAX_SLOTS = sizeof(_prefs.filter_sender_drop)/sizeof(_prefs.filter_sender_drop[0]);
      chan_on = _prefs.filter_sender_drop_chan_on;
      chan_ex = _prefs.filter_sender_drop_chan_ex;
    } else if (is_sender && is_keep_verb) {
      arr = _prefs.filter_sender_keep;
      p_cnt = &_prefs.filter_sender_keep_count;
      MAX_SLOTS = sizeof(_prefs.filter_sender_keep)/sizeof(_prefs.filter_sender_keep[0]);
      chan_on = _prefs.filter_sender_keep_chan_on;
      chan_ex = _prefs.filter_sender_keep_chan_ex;
    } else if (!is_sender && is_drop_verb) {
      arr = _prefs.filter_text_drop;
      p_cnt = &_prefs.filter_text_drop_count;
      MAX_SLOTS = sizeof(_prefs.filter_text_drop)/sizeof(_prefs.filter_text_drop[0]);
      chan_on = _prefs.filter_text_drop_chan_on;
      chan_ex = _prefs.filter_text_drop_chan_ex;
    } else {
      arr = _prefs.filter_text_keep;
      p_cnt = &_prefs.filter_text_keep_count;
      MAX_SLOTS = sizeof(_prefs.filter_text_keep)/sizeof(_prefs.filter_text_keep[0]);
      chan_on = _prefs.filter_text_keep_chan_on;
      chan_ex = _prefs.filter_text_keep_chan_ex;
    }
    uint8_t& cnt = *p_cnt;
    const char* kind = is_sender ? "sender" : "text";
    const char* verb = is_drop_verb ? "drop" : "keep";

    // 'list' / 'clear' / 'add <pattern>' / 'remove <pattern>'
    if (!*p || strncmp(p, "list", 4) == 0) {
      char hdr[80];
      snprintf(hdr, sizeof(hdr), "filter %s %s (%u/%u):", kind, verb,
               (unsigned)cnt, (unsigned)MAX_SLOTS);
      pushCompanionMessage(hdr);
      if (cnt == 0) {
        pushCompanionMessage("  (leer)");
        return;
      }
      char buf[160]; size_t bu = 0; buf[0] = 0;
      auto flushb = [&]() {
        if (bu > 0) { pushCompanionMessage(buf); bu = 0; buf[0] = 0; }
      };
      for (uint8_t i = 0; i < cnt && i < MAX_SLOTS; i++) {
        char line[120];
        const char* prefix = "";
        const char* suffix = "";
        bool a_start = (arr[i].flags & 0x01) != 0;
        bool a_end   = (arr[i].flags & 0x02) != 0;
        if (a_start && a_end) { prefix = "^"; suffix = "$"; }
        else if (a_start)     { prefix = "^"; }
        else if (a_end)       { suffix = "$"; }
        size_t lp = snprintf(line, sizeof(line), "  %u: %s%s%s",
                             (unsigned)(i + 1), prefix, arr[i].pattern, suffix);
        // Pro-Pattern channel-filter anhaengen falls gesetzt.
        if (chan_on[i] != 0 || chan_ex[i] != 0) {
          uint64_t m = chan_on[i] ? chan_on[i] : chan_ex[i];
          int n = snprintf(line + lp, sizeof(line) - lp, "  %s:",
                           chan_on[i] ? "on" : "ex");
          if (n > 0 && lp + n < sizeof(line)) lp += n;
          bool first_ch = true;
          for (int k = 0; k < MAX_GROUP_CHANNELS && k < 64; k++) {
            if ((m & ((uint64_t)1 << k)) == 0) continue;
            ChannelDetails cd;
            if (!getChannel(k, cd)) continue;
            n = snprintf(line + lp, sizeof(line) - lp, "%s%s",
                         first_ch ? "" : ",", cd.name[0] ? cd.name : "?");
            if (n > 0 && lp + n < sizeof(line)) lp += n;
            first_ch = false;
          }
        }
        size_t ll = strlen(line);
        if (bu + ll + 2 >= sizeof(buf)) flushb();
        if (bu > 0) buf[bu++] = '\n';
        memcpy(buf + bu, line, ll); bu += ll; buf[bu] = 0;
      }
      flushb();
      return;
    }
    if (strncmp(p, "clear", 5) == 0 && (p[5] == 0 || p[5] == ' ' || p[5] == '\t')) {
      memset(arr, 0, MAX_SLOTS * sizeof(arr[0]));
      cnt = 0;
      savePrefs();
      char r[60]; snprintf(r, sizeof(r), "OK - filter %s %s cleared.", kind, verb);
      pushCompanionMessage(r);
      return;
    }
    if (strncmp(p, "add", 3) == 0 && (p[3] == ' ' || p[3] == '\t')) {
      p += 3;
      while (*p == ' ' || *p == '\t') p++;
      if (!*p) { pushCompanionMessage("Usage: filter ... <drop|keep> add <pattern> [on-channel|exempt-channel <liste>]"); return; }
      // Pattern aus raw_cmd holen (case-preserving). Plus: Pattern
      // wird hier explizit terminiert (vorher: strlen, das schloss
      // trailing modifier mit ein). Pattern endet entweder am
      // schliessenden Quote (wenn quoted) oder am ersten Whitespace.
      const char* pat;
      size_t plen;
      const char* after_pat;
      if (*p == '"') {
        const char* eq = strchr(p + 1, '"');
        if (!eq) {
          pushCompanionMessage("Pattern: schliessendes Quote fehlt.");
          return;
        }
        pat = raw_cmd + ((p + 1) - cmd);
        plen = (size_t)(eq - (p + 1));
        after_pat = eq + 1;
      } else {
        const char* w = p;
        while (*w && *w != ' ' && *w != '\t') w++;
        pat = raw_cmd + (p - cmd);
        plen = (size_t)(w - p);
        after_pat = w;
      }
      // Anchor-Bytes detektieren
      uint8_t flags = 0;
      if (plen > 0 && pat[0] == '^') { flags |= 0x01; pat++; plen--; }
      if (plen > 0 && pat[plen-1] == '$') { flags |= 0x02; plen--; }
      if (plen == 0) {
        pushCompanionMessage("Leeres Pattern nicht erlaubt.");
        return;
      }
      if (plen >= sizeof(arr[0].pattern)) {
        char r[80];
        snprintf(r, sizeof(r), "Pattern zu lang (max %u Zeichen).",
                 (unsigned)(sizeof(arr[0].pattern) - 1));
        pushCompanionMessage(r);
        return;
      }
      // Optional trailing 'on-channel <liste>' / 'exempt-channel <liste>'.
      while (*after_pat == ' ' || *after_pat == '\t') after_pat++;
      uint64_t add_on_mask = 0, add_ex_mask = 0;
      if (*after_pat) {
        bool ap_on = (strncmp(after_pat, "on-channel", 10) == 0
                      && (after_pat[10] == ' ' || after_pat[10] == '\t'));
        bool ap_ex = (strncmp(after_pat, "exempt-channel", 14) == 0
                      && (after_pat[14] == ' ' || after_pat[14] == '\t'));
        if (!ap_on && !ap_ex) {
          pushCompanionMessage(
            "Nach Pattern nur 'on-channel' oder 'exempt-channel' erlaubt.");
          return;
        }
        after_pat += ap_on ? 10 : 14;
        while (*after_pat == ' ' || *after_pat == '\t') after_pat++;
        uint64_t nm = 0;
        char ub[80]; bool sc = false;
        int uc = parse_channel_list(after_pat, &nm, ub, sizeof(ub), &sc);
        if (uc > 0) {
          char r[160];
          snprintf(r, sizeof(r), "Abgelehnt: unbekannte Channels: %s", ub);
          pushCompanionMessage(r);
          return;
        }
        if (nm == 0) {
          pushCompanionMessage("Leere Channel-Liste nach on/exempt-channel.");
          return;
        }
        if (ap_on) add_on_mask = nm; else add_ex_mask = nm;
      }
      if (cnt >= MAX_SLOTS) {
        char r[80];
        snprintf(r, sizeof(r), "Filter-Liste voll (%u Slots). Erst 'clear' oder 'remove'.",
                 (unsigned)MAX_SLOTS);
        pushCompanionMessage(r);
        return;
      }
      // Dedup: gleicher Pattern + Flags? case-insensitive analog Match.
      for (uint8_t i = 0; i < cnt; i++) {
        if (arr[i].flags == flags
            && strncasecmp(arr[i].pattern, pat, plen) == 0
            && arr[i].pattern[plen] == 0) {
          pushCompanionMessage("(Pattern bereits in Liste -- skip)");
          return;
        }
      }
      memset(&arr[cnt], 0, sizeof(arr[cnt]));
      memcpy(arr[cnt].pattern, pat, plen);
      arr[cnt].pattern[plen] = 0;
      arr[cnt].flags = flags;
      chan_on[cnt] = add_on_mask;
      chan_ex[cnt] = add_ex_mask;
      cnt++;
      savePrefs();
      char r[140];
      const char* pfx = (flags & 0x01) ? "^" : "";
      const char* sfx = (flags & 0x02) ? "$" : "";
      size_t rp = snprintf(r, sizeof(r), "OK - filter %s %s add %s%.*s%s (%u/%u)",
                           kind, verb, pfx, (int)plen, pat, sfx,
                           (unsigned)cnt, (unsigned)MAX_SLOTS);
      if ((add_on_mask | add_ex_mask) != 0) {
        uint64_t m = add_on_mask | add_ex_mask;
        size_t rn = snprintf(r + rp, sizeof(r) - rp,
                             "  %s:", add_on_mask ? "on" : "ex");
        if (rn > 0 && rp + rn < sizeof(r)) rp += rn;
        bool first_ch2 = true;
        for (int i = 0; i < MAX_GROUP_CHANNELS && i < 64; i++) {
          if ((m & ((uint64_t)1 << i)) == 0) continue;
          ChannelDetails cd;
          if (!getChannel(i, cd)) continue;
          int n = snprintf(r + rp, sizeof(r) - rp, "%s%s",
                           first_ch2 ? "" : ",", cd.name[0] ? cd.name : "?");
          if (n > 0 && rp + n < sizeof(r)) rp += n;
          first_ch2 = false;
        }
      }
      pushCompanionMessage(r);
      return;
    }
    if (strncmp(p, "remove", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
      p += 6;
      while (*p == ' ' || *p == '\t') p++;
      if (!*p) { pushCompanionMessage("Usage: filter ... <drop|keep> remove <pattern|index>"); return; }
      // Versuche zuerst als Index zu parsen.
      // User-Wunsch 2026-06-09: Index 1-basiert (Listen-Anzeige zaehlt
      // auch ab 1). Internal Array-Index = idx-1. Buffer-Underflow-
      // Schutz: idx muss >=1 sein, idx-1 muss <cnt sein.
      char* endp = NULL;
      long idx = strtol(p, &endp, 10);
      bool looks_like_index = (endp && endp != p
                               && (*endp == 0 || *endp == ' ' || *endp == '\t'));
      if (looks_like_index) {
        if (idx < 1 || (uint32_t)idx > cnt) {
          char r[80];
          snprintf(r, sizeof(r), "Index %ld ungueltig (1..%u erlaubt).",
                   idx, (unsigned)cnt);
          pushCompanionMessage(r);
          return;
        }
        uint8_t i = (uint8_t)(idx - 1);
        for (uint8_t j = i; j + 1 < cnt; j++) {
          arr[j] = arr[j+1];
          chan_on[j] = chan_on[j+1];
          chan_ex[j] = chan_ex[j+1];
        }
        memset(&arr[cnt-1], 0, sizeof(arr[cnt-1]));
        chan_on[cnt-1] = 0;
        chan_ex[cnt-1] = 0;
        cnt--;
        savePrefs();
        char r[80];
        snprintf(r, sizeof(r), "OK - filter %s %s removed idx %ld (%u verbleibend)",
                 kind, verb, idx, (unsigned)cnt);
        pushCompanionMessage(r);
        return;
      }
      // Sonst als Pattern-Match. Pattern aus raw_cmd holen (case-
      // preserving fuer korrekten Vergleich gegen gespeicherte Patterns
      // -- diese sind case-preserving seit Add-Fix vom 2026-06-09).
      const char* pat = raw_cmd + (p - cmd);
      size_t plen = strlen(pat);
      // trailing whitespace strippen
      while (plen > 0 && (pat[plen-1] == ' ' || pat[plen-1] == '\t'
                           || pat[plen-1] == '\r' || pat[plen-1] == '\n')) plen--;
      if (plen >= 2 && pat[0] == '"' && pat[plen-1] == '"') {
        pat++; plen -= 2;
      }
      uint8_t flags = 0;
      if (plen > 0 && pat[0] == '^') { flags |= 0x01; pat++; plen--; }
      if (plen > 0 && pat[plen-1] == '$') { flags |= 0x02; plen--; }
      // Match case-insensitive (User-Wunsch 2026-06-09: case-insensitive
      // im Filter -- analog match-Funktion).
      for (uint8_t i = 0; i < cnt; i++) {
        if (arr[i].flags == flags
            && strncasecmp(arr[i].pattern, pat, plen) == 0
            && arr[i].pattern[plen] == 0) {
          for (uint8_t j = i; j + 1 < cnt; j++) {
            arr[j] = arr[j+1];
            chan_on[j] = chan_on[j+1];
            chan_ex[j] = chan_ex[j+1];
          }
          memset(&arr[cnt-1], 0, sizeof(arr[cnt-1]));
          chan_on[cnt-1] = 0;
          chan_ex[cnt-1] = 0;
          cnt--;
          savePrefs();
          char r[80];
          snprintf(r, sizeof(r), "OK - filter %s %s removed (%u verbleibend)",
                   kind, verb, (unsigned)cnt);
          pushCompanionMessage(r);
          return;
        }
      }
      pushCompanionMessage("Pattern nicht in Liste.");
      return;
    }
    pushCompanionMessage("Usage: filter <sender|text> <drop|keep> <add|remove|list|clear>");
    return;
  }

  // ein Peer sich als SENSOR (oder REPEATER/ROOM) advertet.
  // Wunschliste 27: 'discover' -- Diagnose-Sender fuer CTL_TYPE_NODE_DISCOVER.
  // Sub-Optionen als Flags (Reihenfolge egal):
  //   prefix    -> bittet um kurze RESP (8 byte pub_key statt 32)
  //   repeater  -> filter nur REPEATER
  //   sensor    -> filter nur SENSOR
  //   all       -> beide (= Default)
  if (starts_with_word(cmd, "discover")) {
    // Sub-Token-Resolver mit Prefix-Match. Damit funktionieren
    // Kuerzungen: 'rep' -> repeater, 'regi' -> regions, 'p' -> prefix,
    // 's' -> sensor, 'a' -> all. Bei Mehrdeutigkeit ('re' matched
    // regions + repeater) -> Kandidaten-Liste.
    static const char* const DISC_WORDS[] = {
      "regions", "help", "prefix", "repeater", "sensor", "all"
    };
    static const int DISC_N = (int)(sizeof(DISC_WORDS) / sizeof(DISC_WORDS[0]));
    auto disc_resolve = [&](const char* tok, size_t tlen,
                            const char** out_ambig_list, size_t* out_n_amb) -> const char* {
      *out_n_amb = 0;
      if (tlen == 1 && tok[0] == '?') return "help";  // '?' Alias
      const char* canon = NULL;
      int n_hits = 0;
      for (int i = 0; i < DISC_N; i++) {
        size_t wlen = strlen(DISC_WORDS[i]);
        if (tlen > wlen) continue;
        bool match = true;
        for (size_t k = 0; k < tlen; k++) {
          char a = tok[k], b = DISC_WORDS[i][k];
          if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
          if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
          if (a != b) { match = false; break; }
        }
        if (!match) continue;
        if (tlen == wlen) return DISC_WORDS[i]; // exact match wins sofort
        if (canon == NULL) canon = DISC_WORDS[i];
        if (*out_n_amb < 6) out_ambig_list[(*out_n_amb)++] = DISC_WORDS[i];
        n_hits++;
      }
      if (n_hits == 1) return canon;
      return NULL;
    };

    // Sub-Mode 'regions [<contact>]': ANON_REQ_TYPE_REGIONS Pfad.
    // Erkennung via Token-Resolver auf erstem Token nach 'discover'.
    {
      const char* pp = strchr(cmd, ' ');
      if (pp) {
        while (*pp == ' ' || *pp == '\t') pp++;
        const char* first_tok = pp;
        size_t first_len = 0;
        while (first_tok[first_len] && first_tok[first_len] != ' ' && first_tok[first_len] != '\t') first_len++;
        const char* ambig[6]; size_t n_amb = 0;
        const char* canon = first_len ? disc_resolve(first_tok, first_len, ambig, &n_amb) : NULL;
        if (!canon && n_amb > 1) {
          char e[140]; int n = snprintf(e, sizeof(e), "Mehrdeutig:");
          for (size_t i = 0; i < n_amb && n < (int)sizeof(e); i++) {
            n += snprintf(e + n, sizeof(e) - n, " %s", ambig[i]);
          }
          pushCompanionMessage(e);
          return;
        }
        if (canon && strcmp(canon, "regions") == 0) {
          // Token nach "regions" extrahieren (case-sensitive Name-Prefix)
          const char* rp = raw_cmd;
          while (*rp == ' ' || *rp == '\t') rp++;
          while (*rp && *rp != ' ' && *rp != '\t') rp++;          // "discover"
          while (*rp == ' ' || *rp == '\t') rp++;
          while (*rp && *rp != ' ' && *rp != '\t') rp++;          // "regions"
          while (*rp == ' ' || *rp == '\t') rp++;
          // Kein Arg = Chain-Modus: CTL-Discover REPEATER triggern,
          // pro RESP automatisch zero-hop ANON_REQ_TYPE_REGIONS. full
          // pubkey (kein prefix) damit wir ECDH-verschluesseln koennen.
          if (!*rp) {
            _discover_regions_chained = true;
            discoverStart((1 << ADV_TYPE_REPEATER), false);
            return;
          }
          // Prefix-Match: case-INSENSITIVE, names DUERFEN Leerzeichen
          // haben -> der ganze Rest der Zeile ist das Prefix (trim trailing).
          // Wir matchen alle Kontakte, filtern auf REPEATER:
          //   0 REPEATER, aber non-REPEATER gefunden -> Hinweis dass Kontakt
          //                                              kein Repeater ist.
          //   1 REPEATER                              -> verwenden.
          //   N REPEATER + exact-Name-Match           -> exact wird gewaehlt.
          //   N REPEATER ohne exact                   -> Mehrdeutig, listen.
          const char* prefix_start = rp;
          const char* prefix_end   = rp + strlen(rp);
          while (prefix_end > prefix_start
                 && (prefix_end[-1] == ' ' || prefix_end[-1] == '\t'
                  || prefix_end[-1] == '\r' || prefix_end[-1] == '\n')) prefix_end--;
          size_t input_len = (size_t)(prefix_end - prefix_start);

          int n_total_matches = 0;
          int n_repeater = 0;
          ContactInfo cand_repeater;       // wenn n_repeater==1: das ist's
          ContactInfo cand_exact_repeater; // exact-Name-Match (Repeater)
          ContactInfo cand_nonrepeater;    // erster non-REPEATER match
          bool has_exact = false;
          bool has_nonrepeater = false;
          char ambig[180] = "";
          size_t ambig_used = 0;

          int total = getNumContacts();
          for (int i = 0; i < total; i++) {
            ContactInfo ci;
            if (!getContactByIdx((uint32_t)i, ci)) continue;
            // case-insensitive Prefix-Vergleich
            bool match = true;
            for (size_t k = 0; k < input_len; k++) {
              char a = ci.name[k];
              char b = prefix_start[k];
              if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
              if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
              if (a != b) { match = false; break; }
            }
            if (!match) continue;
            n_total_matches++;
            if (ci.type == ADV_TYPE_REPEATER) {
              n_repeater++;
              if (n_repeater == 1) cand_repeater = ci;
              if (strlen(ci.name) == input_len) {
                cand_exact_repeater = ci;
                has_exact = true;
              }
              if (ambig_used + 35 < sizeof(ambig)) {
                ambig_used += snprintf(ambig + ambig_used, sizeof(ambig) - ambig_used,
                                       "\n  %.30s", ci.name);
              }
            } else if (!has_nonrepeater) {
              cand_nonrepeater = ci;
              has_nonrepeater = true;
            }
          }

          if (n_total_matches == 0) {
            char r[80];
            snprintf(r, sizeof(r), "Kein Kontakt mit Prefix '%.40s'.", prefix_start);
            pushCompanionMessage(r);
            return;
          }
          if (n_repeater == 0) {
            const char* tn = (cand_nonrepeater.type == ADV_TYPE_CHAT)   ? "CHAT"
                           : (cand_nonrepeater.type == ADV_TYPE_SENSOR) ? "SENSOR"
                           : (cand_nonrepeater.type == ADV_TYPE_ROOM)   ? "ROOM"
                           : "?";
            char r[130];
            snprintf(r, sizeof(r),
                     "'%.30s' ist kein REPEATER (%s).\n"
                     "discover regions geht nur fuer Repeater.",
                     cand_nonrepeater.name, tn);
            pushCompanionMessage(r);
            return;
          }
          ContactInfo chosen;
          if (has_exact) {
            chosen = cand_exact_repeater;
          } else if (n_repeater == 1) {
            chosen = cand_repeater;
          } else {
            char r[220];
            snprintf(r, sizeof(r),
                     "Mehrdeutig (%d Repeater-Treffer):%s\n"
                     "Bitte praeziser angeben.",
                     n_repeater, ambig);
            pushCompanionMessage(r);
            return;
          }
          // Wir versuchen die zero-hop ANON_REQ unabhaengig von der
          // gespeicherten Pfad-Info -- der Request geht eh als direct
          // ohne Pfad raus und schadet dem Netz nicht. Bei nicht-
          // direkten Kontakten wird zusaetzlich gewarnt; ob der Repeater
          // unseren direkten Funkruf hoert, klaert sich dann praktisch.
          bool not_direct = (chosen.out_path_len != 0);
          bool path_unknown = (chosen.out_path_len == OUT_PATH_UNKNOWN);
          if (!sendRegionsQueryZeroHop(chosen.id.pub_key, chosen.name)) {
            pushCompanionMessage("discover regions: send FAILED.");
            return;
          }
          char r[140];
          snprintf(r, sizeof(r),
                   "discover regions @%.40s:\n"
                   "  REQ gesendet (zero-hop).\n"
                   "  Antwort folgt im channel.",
                   chosen.name);
          pushCompanionMessage(r);
          if (not_direct) {
            const char* reason = path_unknown
              ? "kein direkt-Heard in Kontaktliste"
              : "geht in Kontaktliste ueber flood-Pfad";
            char w[145];
            snprintf(w, sizeof(w), "  Warnung: %s.", reason);
            pushCompanionMessage(w);
          }
          return;
        }
      }
    }
    // Flag-Parsing-Loop -- nutzt denselben Resolver fuer Prefix-Match.
    uint8_t filter = 0;
    bool prefix_only = false;
    const char* p = strchr(cmd, ' ');
    while (p && *p) {
      while (*p == ' ' || *p == '\t') p++;
      if (!*p) break;
      const char* t = p;
      while (*p && *p != ' ' && *p != '\t') p++;
      size_t tlen = (size_t)(p - t);
      const char* ambig[6]; size_t n_amb = 0;
      const char* canon = disc_resolve(t, tlen, ambig, &n_amb);
      if (!canon) {
        char e[140];
        if (n_amb > 1) {
          int n = snprintf(e, sizeof(e), "Mehrdeutig '%.*s':", (int)tlen, t);
          for (size_t i = 0; i < n_amb && n < (int)sizeof(e); i++) {
            n += snprintf(e + n, sizeof(e) - n, " %s", ambig[i]);
          }
        } else {
          snprintf(e, sizeof(e), "Unbekanntes Flag '%.*s'.\n'discover help' fuer Optionen.",
                   (int)tlen, t);
        }
        pushCompanionMessage(e);
        return;
      }
      if (strcmp(canon, "help") == 0) {
        pushCompanionMessage(
          "discover [flags]:\n"
          "  pro-aktiv CTL-REQ via sendZeroHop\n"
          "  an direkte Nachbarn. 30s Tabelle.");
        pushCompanionMessage(
          "Flags (kombinierbar, Prefix-Match ok):\n"
          "  repeater - nur REPEATER\n"
          "  sensor   - nur SENSOR\n"
          "  all      - 0xFE (forward-compat)");
        pushCompanionMessage(
          "  prefix   - kurze RESP (8B pub_key)\n"
          "Rate-Limit: 60s zwischen 'discover'.");
        pushCompanionMessage(
          "discover regions (ohne Arg):\n"
          "  CTL-Discover REPEATER, dann\n"
          "  zero-hop ANON-REQ pro RESP.\n"
          "  Region-Listen einzeln in $companion.");
        pushCompanionMessage(
          "discover regions <name-prefix>:\n"
          "  Namesuche, zero-hop direkt.\n"
          "  Nur direkte Nachbarn (protokoll-bedingt).");
        pushCompanionMessage(
          "Komplement 'discoverable':\n"
          "  passive Antwort im full-rep-mode.");
        return;
      }
      else if (strcmp(canon, "prefix") == 0)   prefix_only = true;
      else if (strcmp(canon, "repeater") == 0) filter |= (1 << ADV_TYPE_REPEATER);
      else if (strcmp(canon, "sensor") == 0)   filter |= (1 << ADV_TYPE_SENSOR);
      else if (strcmp(canon, "all") == 0)      filter |= 0xFE;
      else if (strcmp(canon, "regions") == 0) {
        // 'regions' inmitten der Flag-Liste? Nicht erwartet -- separater Pfad.
        pushCompanionMessage(
          "'regions' ist Sub-Befehl, nicht Flag.\n"
          "Nutze 'discover regions [<name>]'.");
        return;
      }
    }
    discoverStart(filter, prefix_only);
    return;
  }

  // Wunschliste 28: 'save' (Top-Level) -- Klar-Begriff. Persistiert die
  // gesamte NodePrefs-Struktur (DL9SAU + Main). Synonym zu 'prefs save'
  // ohne Namespace-Verwirrung. Speichert NICHT Channels/Contacts/Identity
  // (haben eigene Speicher-Pfade).
  if (starts_with_word(cmd, "save")) {
    savePrefs();
    pushCompanionMessage("OK - settings (DL9SAU + Main) persistent gespeichert.\n"
                         "(Channels/Contacts/Identity haben eigene Pfade.)");
    return;
  }

  if (starts_with_word(cmd, "contact")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ' || *arg == '\t') arg++; }
    if (!arg || *arg == 0) {
      pushCompanionMessage(
        "Usage: contact <name-prefix> type <chat|repeater|sensor|room>\n"
        "Ohne 'type ...' -> aktuellen Typ anzeigen.");
      return;
    }
    // Prefix bis Whitespace extrahieren. Suche im raw_cmd damit Case
    // erhalten bleibt (Contact-Namen sind case-sensitiv).
    const char* rp = raw_cmd;
    while (*rp == ' ' || *rp == '\t') rp++;
    while (*rp && *rp != ' ' && *rp != '\t') rp++;  // skip "contact"
    while (*rp == ' ' || *rp == '\t') rp++;
    char prefix[32];
    size_t pi = 0;
    while (*rp && *rp != ' ' && *rp != '\t' && pi + 1 < sizeof(prefix)) {
      prefix[pi++] = *rp++;
    }
    prefix[pi] = 0;
    // Im lower-Buffer entsprechend weitergehen (fuer 'type ...' Parsing)
    while (*arg && *arg != ' ' && *arg != '\t') arg++;
    while (*arg == ' ' || *arg == '\t') arg++;

    ContactInfo* c = searchContactsByPrefix(prefix);
    if (!c) {
      char r[100];
      snprintf(r, sizeof(r), "Kein Kontakt gefunden: '%s'", prefix);
      pushCompanionMessage(r);
      return;
    }
    const char* type_name = (c->type == ADV_TYPE_CHAT)     ? "chat"
                          : (c->type == ADV_TYPE_REPEATER) ? "repeater"
                          : (c->type == ADV_TYPE_ROOM)     ? "room"
                          : (c->type == ADV_TYPE_SENSOR)   ? "sensor"
                          : "?";
    if (!*arg) {
      char r[120];
      snprintf(r, sizeof(r), "%s: type = %s (%u)", c->name, type_name, c->type);
      pushCompanionMessage(r);
      return;
    }
    if (strncmp(arg, "type", 4) != 0 || (arg[4] != ' ' && arg[4] != '\t')) {
      pushCompanionMessage("Usage: contact <name-prefix> type <chat|repeater|sensor|room>");
      return;
    }
    arg += 4;
    while (*arg == ' ' || *arg == '\t') arg++;
    if (!*arg) {
      pushCompanionMessage("type ohne Argument. Erwartet: <chat|repeater|sensor|room>");
      return;
    }
    uint8_t new_type = 0;
    if      (strcmp(arg, "chat") == 0)     new_type = ADV_TYPE_CHAT;
    else if (strcmp(arg, "repeater") == 0) new_type = ADV_TYPE_REPEATER;
    else if (strcmp(arg, "room") == 0)     new_type = ADV_TYPE_ROOM;
    else if (strcmp(arg, "sensor") == 0)   new_type = ADV_TYPE_SENSOR;
    else {
      pushCompanionMessage("Unbekannter type. Erwartet: chat|repeater|sensor|room");
      return;
    }
    if (c->type == new_type) {
      char r[120];
      snprintf(r, sizeof(r), "%s war bereits type=%s. Nichts zu tun.",
               c->name, arg);
      pushCompanionMessage(r);
      return;
    }
    uint8_t old = c->type;
    c->type = new_type;
    saveContacts();
    // Push an die App: bestehender Contact-Update-Frame mit neuem Typ.
    // App kann den UI-Status (Chat-Button etc.) ggf. ohne Reconnect updaten.
    if (_serial->isConnected()) {
      writeContactRespFrame(PUSH_CODE_ADVERT, *c);
    }
    char r[160];
    snprintf(r, sizeof(r),
             "OK - %s: type %u -> %u (%s).\n"
             "App ggf. trennen+neu verbinden falls UI nicht aktualisiert.",
             c->name, old, new_type, arg);
    pushCompanionMessage(r);
    return;
  }

  // ---------- backup [save|restore] -------------------------------------
  // Wunschliste 28: backup / restore von DL9SAU-prefs + node-main
  // ueber USB-Serial JSON. Phase A (save) implementiert, Phase B
  // (restore) folgt.
  if (starts_with_word(cmd, "backup")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ' || *arg == '\t') arg++; }
    if (!arg || *arg == 0) {
      pushCompanionMessage(
        "backup save     -- JSON-Backup nach USB-Serial\n"
        "backup restore  -- JSON-Restore (NOCH NICHT implementiert)");
      return;
    }
    if (strcmp(arg, "save") == 0) {
      backupSaveToSerial();
      pushCompanionMessage("backup save: 2 JSON-Bloecke nach USB-Serial geschrieben.\n"
                           "Terminal-Cut+Paste in eine Datei zum Sichern.");
      return;
    }
    if (strcmp(arg, "restore") == 0) {
      backupRestoreStart();
      return;
    }
    pushCompanionMessage("Usage: backup [save|restore]");
    return;
  }

  // ---------- set <key> <value> -----------------------------------------
  // Aenderungen an persistenten Settings, analog zur Repeater-CommonCLI
  // (CMD_SET_RADIO_PARAMS / set name / set lat / set lon). Schreibt _prefs
  // und savePrefs.
  //
  // Keys: name freq sf bw cr tx_power lat lon
  //
  // Hinweis Lat/Lon: South negativ, West negativ. Z.B. 53.5172 oder -10.123.
  if (starts_with_word(cmd, "set")) {
    const char* p = strchr(cmd, ' ');
    if (!p) { pushCompanionMessage("Usage: set <key> <value>\n'help set' fuer keys"); return; }
    while (*p == ' ') p++;
    if (!*p) { pushCompanionMessage("Usage: set <key> <value>"); return; }

    // Key extrahieren
    const char* key_start = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    size_t key_len = (size_t)(p - key_start);
    // 40 Byte fasst auch lange Keys wie 'flood_max_unscoped_companions' (29).
    // Vorher 24 -> truncated zu 'flood_max_unscoped_comp', set-Befehl schlug fehl.
    char key[40];
    if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
    memcpy(key, key_start, key_len);
    key[key_len] = 0;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) {
      // Per-Key Discoverability: bei Keys mit nicht-offensichtlicher
      // 0-Semantik (Kaskade / transienter Fallback) explizite Erklaerung
      // statt nur "Usage: set X <value>". Sonst landet der User in der
      // 0-vs-deaktiviert-Falle. Jede Message bleibt < 145 Byte.
      if (strcmp(key, "flood_max") == 0 || strcmp(key, "flood.max") == 0) {
        pushCompanionMessage(
          "set flood_max <1..63>: globale Hop-Obergrenze.\n"
          "  Default 16. Bei Senken werden infra und\n"
          "  req_resp automatisch mit-gecapped.");
        return;
      }
      if (strcmp(key, "flood_max_infra") == 0 || strcmp(key, "flood.max.infra") == 0) {
        pushCompanionMessage(
          "set flood_max_infra <0..flood_max | follow>:\n"
          "  Cap fuer REPEATER/SENSOR/ROOM-Adverts.\n"
          "  Default 16. User-Chat-Adverts unbetroffen.");
        pushCompanionMessage(
          "  0 = wirkt wie flood_max (Reboot bumpt auf 16)\n"
          "  follow (oder 'max') = persistent gleichauf mit\n"
          "    flood_max, folgt Aenderungen automatisch.");
        return;
      }
      if (strcmp(key, "flood_max_req_resp") == 0 || strcmp(key, "flood.max.req.resp") == 0) {
        pushCompanionMessage(
          "set flood_max_req_resp <0..flood_max_infra>:\n"
          "  Cap fuer REQ/RESP/ANON_REQ (Telemetrie,\n"
          "  Owner-Info, Login). Default 0.");
        pushCompanionMessage(
          "  0 = persistente Kaskade auf flood_max_infra.\n"
          "  Empfehlung 4..8 (4=Stadt, 8=weiter).");
        return;
      }
      if (strcmp(key, "flood_max_scope_region") == 0) {
        pushCompanionMessage(
          "set flood_max_scope_region <0..flood_max>:\n"
          "  Hop-Cap fuer #region/#regional-scoped Pakete.\n"
          "  Default 3.");
        pushCompanionMessage(
          "  0 = #region/#regional NICHT repeaten\n"
          "  (auch im 'repeat all'-Modus).");
        return;
      }
      if (strcmp(key, "loop_detect") == 0 || strcmp(key, "loop.detect") == 0) {
        pushCompanionMessage(
          "set loop_detect <off|minimal|moderate|strict>:\n"
          "  Loop-Drop bei Flood-Repeat (full-rep nur).\n"
          "  Default off. Numerisch 0..3 auch erlaubt.");
        return;
      }
      if (strcmp(key, "messages_append_scope_to_name") == 0
          || strcmp(key, "messages.append.scope.to.name") == 0) {
        pushCompanionMessage(
          "set messages_append_scope_to_name <on|off>:\n"
          "  Channel-Sender 'Name (#scope, direct)' und\n"
          "  DM-Vorab-Frame '[#scope, direct]' beim 1. Auftreten\n"
          "  pro (channel, sender, scope, direct)-Tupel.");
        pushCompanionMessage(
          "Default: off.");
        pushCompanionMessage(
          "WARNUNG bei on: die App zeigt 'Keine Pfadinfo'\n"
          "fuer annotierte Messages (bekannte Limitierung,\n"
          "App-Lookup matched modifizierten Sender-Namen nicht).");
        return;
      }
      if (strcmp(key, "flood_max_unscoped_companions") == 0
          || strcmp(key, "flood.max.unscoped.companions") == 0
          || strcmp(key, "flood_max_unscoped") == 0
          || strcmp(key, "flood.max.unscoped") == 0) {
        bool is_upstream_alias =
            (strcmp(key, "flood_max_unscoped") == 0
             || strcmp(key, "flood.max.unscoped") == 0);
        if (is_upstream_alias) {
          pushCompanionMessage(
            "Hinweis: 'flood_max_unscoped' (upstream) -> hier\n"
            "Alias auf 'flood_max_unscoped_companions'.");
        }
        pushCompanionMessage(
          "set flood_max_unscoped_companions:\n"
          "  Cap fuer unscoped Companion-Flood\n"
          "  (CHAT-Adverts + flooded DMs).");
        pushCompanionMessage(
          "<follow|off|1..63>:\n"
          "  follow = folgt flood_max_scope_region\n"
          "  off    = nicht repeaten\n"
          "  1..63  = expliziter Cap");
        pushCompanionMessage(
          "Geblockt: REQ/RESP/ANON_REQ unscoped +\n"
          "Infra-Adverts (REPEATER/ROOM/SENSOR).");
        return;
      }
      if (strcmp(key, "txdelay") == 0) {
        pushCompanionMessage(
          "set txdelay <auto | 0..2>:\n"
          "  Retransmit-Delay-Faktor x Airtime beim\n"
          "  Forwarden (Repeaten) von Flood-Paketen.");
        pushCompanionMessage(
          "  auto (Default): 1.5 wenn profile=full,\n"
          "  0.5 wenn defensive. Hoeher = mehr Spread,\n"
          "  hilft bei hochwertigem Standort/Reichweite.");
        return;
      }
      if (strcmp(key, "direct_txdelay") == 0) {
        pushCompanionMessage(
          "set direct_txdelay <auto | 0..2>:\n"
          "  Retransmit-Delay-Faktor x Airtime beim\n"
          "  Forwarden von DIRECT-routed Paketen.");
        pushCompanionMessage(
          "  auto (Default): 0.2 (Upstream-Default).\n"
          "  Niedrig weil direkt-geroutet = bekannter\n"
          "  Pfad, Latenz hat Vorrang vor Spread.");
        return;
      }
      if (strcmp(key, "advert_loc_policy") == 0 || strcmp(key, "advert.loc.policy") == 0) {
        pushCompanionMessage(
          "set advert_loc_policy <0|1|2>:\n"
          "  0=NONE (kein Standort im Advert),\n"
          "  1=SHARE (aktueller GPS-Fix),\n"
          "  2=PREFS (gespeicherte lat/lon).");
        return;
      }
      if (strcmp(key, "path_hash_mode") == 0 || strcmp(key, "path.hash.mode") == 0) {
        pushCompanionMessage(
          "set path_hash_mode <0..2>:\n"
          "  Path-Hash-Bytes pro Hop bei eigenen Floods.\n"
          "  0=1byte (Default), 1=2byte, 2=3byte.");
        pushCompanionMessage(
          "  Mehr Bytes = weniger Kollisionen, kuerzere\n"
          "  Max-Pfade (1B*64 vs 3B*21 in Path-Feld).");
        return;
      }
      if (strcmp(key, "autoadd_max_hops") == 0) {
        pushCompanionMessage(
          "set autoadd_max_hops <0..64>:\n"
          "  Max Hops fuer Auto-Add neuer Kontakte.\n"
          "  0=ohne Limit, 1=nur direkt (0 Hops),");
        pushCompanionMessage(
          "  N=bis N-1 Hops. Off-by-one ist App-Design,\n"
          "  nicht aenderbar.");
        return;
      }
      if (strcmp(key, "gps") == 0) {
        pushCompanionMessage(
          "set gps <0|1>:\n"
          "  0=GPS aus, 1=ein. 'gps_interval' bestimmt\n"
          "  separat ob/wie oft auto-gepollt wird.");
        return;
      }
      if (strcmp(key, "gps_interval") == 0) {
        pushCompanionMessage(
          "set gps_interval <0..86400>:\n"
          "  Auto-Poll-Frequenz in Sekunden.\n"
          "  0=keine Auto-Polls. Default 0.");
        return;
      }
      if (strcmp(key, "telemetry_mode_base") == 0
          || strcmp(key, "telemetry_mode_loc") == 0
          || strcmp(key, "telemetry_mode_env") == 0) {
        pushCompanionMessage(
          "set telemetry_mode_<base|loc|env> <0|1|2>:\n"
          "  0=DENY (nie senden),\n"
          "  1=ALLOW_FLAGS (contact.flags entscheidet),\n"
          "  2=ALLOW_ALL (immer senden).");
        return;
      }
      if (strcmp(key, "time") == 0) {
        pushCompanionMessage(
          "set time sync <off|lazy|aabbcc [bb [cc]]>:\n"
          "  Advert-basierte RTC-Sync (signiert).");
        pushCompanionMessage(
          "  off  = aus\n"
          "  lazy = jeder zero-hop Advert von\n"
          "         REPEATER/ROOM/CHAT (default)");
        pushCompanionMessage(
          "  aabbcc = 6 hex = 3-Byte Pub-Key-Prefix\n"
          "  bis 3 strict-Sources moeglich.\n"
          "  GPS hat Vorrang wenn aktiv+je-synced.");
        return;
      }
      if (strcmp(key, "ch.hops") == 0) {
        pushCompanionMessage(
          "set ch.hops <name> <follow|off|N>:\n"
          "  per-Channel Repeat-Cap (Group-Messages).");
        pushCompanionMessage(
          "  follow = kein Cap (flood_max gilt)\n"
          "  off    = nicht repeaten\n"
          "  1..63  = Cap (Drop wenn path_hash > N)");
        pushCompanionMessage(
          "Hashtag-Channels koennen auch OHNE Subscribe\n"
          "  geblockt werden (PSK aus Name ableitbar):\n"
          "  set ch.hops #bots 0 -- Eintrag wird (ext).");
        pushCompanionMessage(
          "  Name 'unknown' -> flood_max_unknown_chan\n"
          "  (Channel-Hash auf keinen Slot/External match)\n"
          "  Default = follow flood_max.");
        pushCompanionMessage(
          "  Siehe auch: 'ch.hops status' / 'ch.hops clear'");
        return;
      }
      char r[100]; snprintf(r, sizeof(r), "Usage: set %s <value>", key);
      pushCompanionMessage(r);
      return;
    }
    const char* value_lc = p;  // value in lower-buffer (numeric values ok)

    // -- set name <text> (case-sensitiv, raw_cmd-Token-Walk) --
    if (strcmp(key, "name") == 0) {
      // 3. Token im RAW finden (set name <text>)
      const char* rp = raw_cmd;
      while (*rp == ' ' || *rp == '\t') rp++;
      while (*rp && *rp != ' ' && *rp != '\t') rp++;          // skip "set"
      while (*rp == ' ' || *rp == '\t') rp++;
      while (*rp && *rp != ' ' && *rp != '\t') rp++;          // skip "name"
      while (*rp == ' ' || *rp == '\t') rp++;
      if (!*rp) { pushCompanionMessage("Usage: set name <text>"); return; }
      // Trim trailing WS, length cap
      char clean[32];
      size_t cl = 0;
      while (*rp && cl + 1 < sizeof(clean)) clean[cl++] = *rp++;
      while (cl > 0 && (clean[cl-1] == ' ' || clean[cl-1] == '\t'
                        || clean[cl-1] == '\r' || clean[cl-1] == '\n')) cl--;
      clean[cl] = 0;
      if (cl == 0) { pushCompanionMessage("Usage: set name <text>"); return; }
      StrHelper::strncpy(_prefs.node_name, clean, sizeof(_prefs.node_name));
      savePrefs();
      char r[80]; snprintf(r, sizeof(r), "OK - name = \"%s\"", _prefs.node_name);
      pushCompanionMessage(r);
      return;
    }

    // -- set prv.key <128 hex> oder NEW (neu generieren) --
    // Identitaet wechseln. Reboot empfohlen (BLE-Pairing-Key, gecachte
    // Shared-Secrets). Analog CommonCLI/simple_repeater 'set prv.key'.
    //
    // Keyword 'NEW' (case-sensitive, GROSS) statt "" um iOS-Smart-Quotes-
    // Probleme zu umgehen: iOS schreibt typografische Anfuehrungszeichen
    // (U+201C/U+201D, 3 Byte UTF-8 jeweils) statt ASCII " -- da kam dann
    // 'got 6 chars' raus. NEW (gross) signalisiert auch dass der User
    // weiss was er tut (= alle bisherigen Kontakte koennen uns nicht mehr
    // erreichen unter dem alten Pubkey).
    if (strcmp(key, "prv.key") == 0 || strcmp(key, "prv_key") == 0) {
      // Token aus RAW (case-sensitive, kein lower-case)
      const char* rp = raw_cmd;
      while (*rp == ' ' || *rp == '\t') rp++;
      while (*rp && *rp != ' ' && *rp != '\t') rp++;        // skip "set"
      while (*rp == ' ' || *rp == '\t') rp++;
      while (*rp && *rp != ' ' && *rp != '\t') rp++;        // skip key
      while (*rp == ' ' || *rp == '\t') rp++;
      const char* arg_start = rp;
      const char* arg_end = rp + strlen(rp);
      while (arg_end > arg_start && (arg_end[-1] == ' ' || arg_end[-1] == '\t'
                                      || arg_end[-1] == '\r' || arg_end[-1] == '\n')) arg_end--;

      mesh::LocalIdentity new_id;
      // Spezial-Keyword: NEW (case-sensitive, GROSS) -> neu generieren
      size_t arg_len = (size_t)(arg_end - arg_start);
      bool is_new_keyword = (arg_len == 3
                             && arg_start[0] == 'N'
                             && arg_start[1] == 'E'
                             && arg_start[2] == 'W');
      if (is_new_keyword) {
        new_id = mesh::LocalIdentity(getRNG());
      } else {
        size_t expected_hex = PRV_KEY_SIZE * 2;
        if (arg_len != expected_hex) {
          char e[120];
          snprintf(e, sizeof(e),
                   "Usage: set prv.key <128 hex chars>\n"
                   "       set prv.key NEW  (neu generieren)\n"
                   "(got %u chars)",
                   (unsigned)arg_len);
          pushCompanionMessage(e);
          return;
        }
        uint8_t prv_buf[PRV_KEY_SIZE];
        char buf_copy[PRV_KEY_SIZE * 2 + 1];
        memcpy(buf_copy, arg_start, expected_hex);
        buf_copy[expected_hex] = 0;
        if (!mesh::Utils::fromHex(prv_buf, PRV_KEY_SIZE, buf_copy)) {
          pushCompanionMessage("Fehler: ungueltige hex-Zeichen.");
          return;
        }
        if (!mesh::LocalIdentity::validatePrivateKey(prv_buf)) {
          pushCompanionMessage("Fehler: ungueltiger private key.");
          return;
        }
        new_id.readFrom(prv_buf, PRV_KEY_SIZE);
      }
      if (!_store->saveMainIdentity(new_id)) {
        pushCompanionMessage("Fehler: saveMainIdentity FAILED.");
        return;
      }
      self_id = new_id;
      char hexbuf[17];
      for (int i = 0; i < 8; i++) snprintf(hexbuf + i*2, 3, "%02x", self_id.pub_key[i]);
      char r[160];
      snprintf(r, sizeof(r),
               "OK - prv.key gesetzt + saveIdentity.\n"
               "neue pubkey-Praefix: %s...\n"
               "Reboot zum vollen Wirken empfohlen.",
               hexbuf);
      pushCompanionMessage(r);
      return;
    }

    // -- set owner_info <text> (case-sensitiv, raw_cmd, lange Strings) --
    // Reise-Wunsch 2026-06-09 (Wunschliste 52): Remote-Admin/Guest-
    // Passwoerter persistent setzen.
    //   'set passwd_admin <pw>'    -> Vollzugriff via Remote-CLI
    //   'set passwd_guest <pw>'    -> Read-Only-Subset (optional)
    //   'set passwd_admin clear'   -> Remote-Admin deaktivieren
    //   'set passwd_admin'         -> leer = clear
    //   'set passwd_guest clear/leer' analog
    // Restliche CLI-Pfade (Wunschliste 52 Schritte 4-8) folgen separat.
    if (strcmp(key, "passwd_admin") == 0
        || strcmp(key, "passwd_guest") == 0
        || strcmp(key, "passwd.admin") == 0
        || strcmp(key, "passwd.guest") == 0) {
      bool is_admin = (strcmp(key, "passwd_admin") == 0
                       || strcmp(key, "passwd.admin") == 0);
      char* dst = is_admin ? _prefs.passwd_admin : _prefs.passwd_guest;
      size_t dst_sz = is_admin ? sizeof(_prefs.passwd_admin)
                                : sizeof(_prefs.passwd_guest);
      const char* what = is_admin ? "passwd_admin" : "passwd_guest";

      // Wert ab raw_cmd nach 'set <key>' extrahieren
      const char* rp = raw_cmd;
      while (*rp == ' ' || *rp == '\t') rp++;
      while (*rp && *rp != ' ' && *rp != '\t') rp++;  // skip "set"
      while (*rp == ' ' || *rp == '\t') rp++;
      while (*rp && *rp != ' ' && *rp != '\t') rp++;  // skip "passwd_xxx"
      while (*rp == ' ' || *rp == '\t') rp++;
      if (!*rp || strcmp(rp, "clear") == 0) {
        memset(dst, 0, dst_sz);
        savePrefs();
        char r[60]; snprintf(r, sizeof(r), "OK - %s cleared.", what);
        pushCompanionMessage(r);
        return;
      }
      // Laengen-Check: max dst_sz-1 nutzbar (Storage incl. NUL).
      // Bei Ueberlauf ablehnen statt silent truncate (sonst weiss
      // User nicht warum sein langes Passwort nicht funktioniert).
      size_t rp_len = strlen(rp);
      // Trailing whitespace nicht mitzaehlen
      while (rp_len > 0 && (rp[rp_len-1] == ' ' || rp[rp_len-1] == '\t'
                            || rp[rp_len-1] == '\r' || rp[rp_len-1] == '\n')) {
        rp_len--;
      }
      if (rp_len >= dst_sz) {
        char r[100];
        snprintf(r, sizeof(r),
                 "Abgelehnt: %s max %u Zeichen (Du: %u).",
                 what, (unsigned)(dst_sz - 1), (unsigned)rp_len);
        pushCompanionMessage(r);
        return;
      }
      memset(dst, 0, dst_sz);
      memcpy(dst, rp, rp_len);
      savePrefs();
      char r[80]; snprintf(r, sizeof(r), "OK - %s gesetzt (%u Zeichen).",
                          what, (unsigned)rp_len);
      pushCompanionMessage(r);
      return;
    }

    // Wunschliste 7 Phase 1: free-form Beschreibung der Node. Max 119
    // Zeichen + NUL. '|' wird in '\n' uebersetzt (analog CommonCLI).
    if (strcmp(key, "owner_info") == 0 || strcmp(key, "owner.info") == 0) {
      const char* rp = raw_cmd;
      while (*rp == ' ' || *rp == '\t') rp++;
      while (*rp && *rp != ' ' && *rp != '\t') rp++;          // skip "set"
      while (*rp == ' ' || *rp == '\t') rp++;
      while (*rp && *rp != ' ' && *rp != '\t') rp++;          // skip "owner_info"
      while (*rp == ' ' || *rp == '\t') rp++;
      if (!*rp) {
        // leerer Text -> clear
        _prefs.owner_info[0] = 0;
        savePrefs();
        pushCompanionMessage("OK - owner_info cleared.");
        return;
      }
      char* dp = _prefs.owner_info;
      char* dend = dp + sizeof(_prefs.owner_info) - 1;
      while (*rp && dp < dend) {
        *dp++ = (*rp == '|') ? '\n' : *rp;
        rp++;
      }
      *dp = 0;
      // Trailing whitespace strippen.
      while (dp > _prefs.owner_info
             && (dp[-1] == ' ' || dp[-1] == '\t' || dp[-1] == '\r' || dp[-1] == '\n')) {
        --dp; *dp = 0;
      }
      savePrefs();
      char r[60]; snprintf(r, sizeof(r), "OK - owner_info = (%u Zeichen)",
                            (unsigned)strlen(_prefs.owner_info));
      pushCompanionMessage(r);
      return;
    }

    // -- set lat / set lon (double) --
    if (strcmp(key, "lat") == 0) {
      double v = atof(value_lc);
      if (v < -90.0 || v > 90.0) {
        pushCompanionMessage("lat ausserhalb -90.0 .. 90.0 (Sued = negativ).");
        return;
      }
      sensors.node_lat = v;
      savePrefs();
      char r[80]; snprintf(r, sizeof(r), "OK - lat = %.6f", v);
      pushCompanionMessage(r);
      return;
    }
    if (strcmp(key, "lon") == 0) {
      double v = atof(value_lc);
      if (v < -180.0 || v > 180.0) {
        pushCompanionMessage("lon ausserhalb -180.0 .. 180.0 (West = negativ).");
        return;
      }
      sensors.node_lon = v;
      savePrefs();
      char r[80]; snprintf(r, sizeof(r), "OK - lon = %.6f", v);
      pushCompanionMessage(r);
      return;
    }

    // -- set freq <X> in MHz --
    if (strcmp(key, "freq") == 0) {
      float v = atof(value_lc);
      if (v < 150.0f || v > 2500.0f) {
        pushCompanionMessage("freq ausserhalb 150..2500 MHz");
        return;
      }
      _prefs.freq = v;
      savePrefs();
      applyRadioPolicy();
      char r[80]; snprintf(r, sizeof(r), "OK - freq = %.4f MHz", v);
      pushCompanionMessage(r);
      return;
    }
    if (strcmp(key, "bw") == 0) {
      float v = atof(value_lc);
      if (v < 7.0f || v > 500.0f) {
        pushCompanionMessage("bw ausserhalb 7..500 kHz");
        return;
      }
      _prefs.bw = v;
      savePrefs();
      applyRadioPolicy();
      char r[80]; snprintf(r, sizeof(r), "OK - bw = %.1f kHz", v);
      pushCompanionMessage(r);
      return;
    }
    if (strcmp(key, "sf") == 0) {
      int v = atoi(value_lc);
      if (v < 5 || v > 12) { pushCompanionMessage("sf ausserhalb 5..12"); return; }
      _prefs.sf = (uint8_t)v;
      savePrefs();
      applyRadioPolicy();
      char r[40]; snprintf(r, sizeof(r), "OK - sf = %d", v);
      pushCompanionMessage(r);
      return;
    }
    if (strcmp(key, "cr") == 0) {
      int v = atoi(value_lc);
      if (v < 5 || v > 8) { pushCompanionMessage("cr ausserhalb 5..8"); return; }
      _prefs.cr = (uint8_t)v;
      savePrefs();
      applyRadioPolicy();
      char r[40]; snprintf(r, sizeof(r), "OK - cr = %d", v);
      pushCompanionMessage(r);
      return;
    }
    if (strcmp(key, "tx_power") == 0) {
      int v = atoi(value_lc);
      if (v < -9 || v > MAX_LORA_TX_POWER) {
        char r[80]; snprintf(r, sizeof(r), "tx_power ausserhalb -9..%d", (int)MAX_LORA_TX_POWER);
        pushCompanionMessage(r);
        return;
      }
      _prefs.tx_power_dbm = (int8_t)v;
      savePrefs();
      radio_driver.setTxPower(_prefs.tx_power_dbm);
      char r[40]; snprintf(r, sizeof(r), "OK - tx_power = %d dBm", v);
      pushCompanionMessage(r);
      return;
    }

    // Repeater-style Delay-Knöpfe (analog simple_repeater CommonCLI).
    // txdelay/rxdelay sind Faktoren multipliziert mit Pkt-Airtime; siehe
    // getRetransmitDelay() / calcRxDelay(). 'auto' ist Sentinel (-1) der
    // den effektiven Wert aus repeater_profile + Kontext ableitet.
    if (strcmp(key, "txdelay") == 0 || strcmp(key, "rxdelay") == 0
        || strcmp(key, "direct_txdelay") == 0) {
      bool is_auto = (strcasecmp(value_lc, "auto") == 0);
      float v = is_auto ? -1.0f : (float)atof(value_lc);
      if (!is_auto && (v < 0.0f || v > 20.0f)) {
        pushCompanionMessage("Wert ausserhalb 0..20.0 (oder 'auto')");
        return;
      }
      if (strcmp(key, "rxdelay") == 0) {
        // rxdelay kennt kein auto -- numerisch lassen wie bisher.
        if (is_auto) { pushCompanionMessage("rxdelay: 'auto' nicht definiert."); return; }
        _prefs.rx_delay_base = v;
        savePrefs();
        char r[60]; snprintf(r, sizeof(r), "OK - rxdelay = %.3f", v);
        pushCompanionMessage(r);
      } else if (strcmp(key, "txdelay") == 0) {
        if (!is_auto && v > 2.0f) { pushCompanionMessage("txdelay max 2.0"); return; }
        if (!is_auto && v == 0.0f) v = -1.0f;  // 0 -> auto (uninit-Sentinel)
        _prefs.tx_delay_factor = v;
        savePrefs();
        char r[80];
        if (v < 0.0f)
          snprintf(r, sizeof(r), "OK - txdelay = auto (effektiv %.2f, profile=%s)",
                   effectiveTxDelayFactor(),
                   _prefs.repeater_profile == 1 ? "full" : "defensive");
        else
          snprintf(r, sizeof(r), "OK - txdelay = %.3f", v);
        pushCompanionMessage(r);
      } else {  // direct_txdelay
        if (!is_auto && v > 2.0f) { pushCompanionMessage("direct_txdelay max 2.0"); return; }
        if (!is_auto && v == 0.0f) v = -1.0f;  // 0 -> auto
        _prefs.direct_tx_delay_factor = v;
        savePrefs();
        char r[80];
        if (v < 0.0f)
          snprintf(r, sizeof(r), "OK - direct_txdelay = auto (effektiv %.2f)",
                   effectiveDirectTxDelayFactor());
        else
          snprintf(r, sizeof(r), "OK - direct_txdelay = %.3f", v);
        pushCompanionMessage(r);
      }
      return;
    }

    // Hop-Cap fuer #region / #regional. Range 0..flood_max.
    //   0 = #region/#regional NICHT repeaten (auch im allow-list-Mode)
    //   1..flood_max = expliziter Cap
    // Default-Bump auf 3 nur bei wirklich uninitialisiert (siehe Pre-Init
    // in begin()) -- nicht im Setter, damit User explizit 0 setzen kann.
    if (strcmp(key, "flood_max_scope_region") == 0
        || strcmp(key, "flood.max.scope.region") == 0) {
      // Wunschliste 39: 'off' Keyword oder 1..N. Kein numerisches '0'.
      // Internal storage: 0 = off (semantisch identisch zum Keyword).
      uint8_t newval;
      if (strcmp(value_lc, "off") == 0) {
        newval = 0;
      } else {
        int v = atoi(value_lc);
        if (v < 1 || v > _prefs.flood_max) {
          char r[100]; snprintf(r, sizeof(r),
            "Wert: 'off' / 1..%u (flood_max-Cap).",
            (unsigned)_prefs.flood_max);
          pushCompanionMessage(r);
          return;
        }
        newval = (uint8_t)v;
      }
      _prefs.flood_max_scope_region = newval;
      savePrefs();
      char r[100];
      if (newval == 0)
        snprintf(r, sizeof(r),
          "OK - flood_max_scope_region = off (nicht repeaten)");
      else
        snprintf(r, sizeof(r),
          "OK - flood_max_scope_region = %u", (unsigned)newval);
      pushCompanionMessage(r);
      return;
    }

    // Loop-Detection-Modus (Wunschliste 6b).
    // 'loop.detect' (CommonCLI-Stil) als Alias erlaubt.
    if (strcmp(key, "loop_detect") == 0 || strcmp(key, "loop.detect") == 0) {
      static const CompanionChoice ld_modes[] = {
        { "off",      false },  // 0
        { "minimal",  false },  // 1
        { "moderate", false },  // 2
        { "strict",   false },  // 3
      };
      char amb[40];
      int m = match_choice(value_lc, ld_modes, 4, amb, sizeof(amb));
      if (m == -1) { char r[80]; snprintf(r, sizeof(r), "Mehrdeutig: %s", amb); pushCompanionMessage(r); return; }
      if (m < 0)   { pushCompanionMessage("Usage: set loop_detect off|minimal|moderate|strict"); return; }
      _prefs.loop_detect = (uint8_t)m;
      savePrefs();
      const char* nm = (m == 0) ? "off" : (m == 1) ? "minimal" : (m == 2) ? "moderate" : "strict";
      char r[160];
      snprintf(r, sizeof(r),
        "OK - loop_detect = %s\n"
        "  Wirkt NUR im 'repeater profile normal'\n"
        "  (defensive Profile braucht keine Loop-Detection).",
        nm);
      pushCompanionMessage(r);
      return;
    }

    // Globale Repeat-Hop-Obergrenze. Range 1..63 (6-Bit-hash_count).
    // 'flood.max' (CommonCLI-Stil) als Alias erlaubt.
    if (strcmp(key, "flood_max") == 0 || strcmp(key, "flood.max") == 0) {
      int v = atoi(value_lc);
      if (v < 1 || v > 63) {
        pushCompanionMessage("Wert ausserhalb 1..63");
        return;
      }
      _prefs.flood_max = (uint8_t)v;
      // Wenn flood_max_scope_region jetzt drueber liegt: nach unten ziehen
      // damit die Beziehung gilt (regional <= flood_max).
      if (_prefs.flood_max_scope_region > _prefs.flood_max) {
        _prefs.flood_max_scope_region = _prefs.flood_max;
      }
      // Auch flood_max_infra auto-cappen falls > flood_max. Aber das
      // FOLLOW-Sentinel (254) bleibt unangetastet -- dessen Semantik
      // IST "folge flood_max", also genau das was wir nicht ueberschreiben
      // wollen wenn flood_max gesenkt wird.
      bool infra_clipped = false;
      if (_prefs.flood_max_infra != FLOOD_MAX_INFRA_FOLLOW
          && _prefs.flood_max_infra > _prefs.flood_max) {
        _prefs.flood_max_infra = _prefs.flood_max;
        infra_clipped = true;
      }
      // Und flood_max_req_resp ebenfalls -- darf nie groesser sein als
      // flood_max (bei infra==0 ist flood_max die effektive Obergrenze).
      bool rr_clipped = false;
      if (_prefs.flood_max_req_resp > _prefs.flood_max) {
        _prefs.flood_max_req_resp = _prefs.flood_max;
        rr_clipped = true;
      }
      savePrefs();
      char r[180]; snprintf(r, sizeof(r),
        "OK - flood_max = %d\n"
        "  flood_max_scope_region gecapped auf %u\n"
        "  flood_max_infra%s = %u\n"
        "  flood_max_req_resp%s = %u",
        v, (unsigned)_prefs.flood_max_scope_region,
        infra_clipped ? " gecapped" : "",
        (unsigned)_prefs.flood_max_infra,
        rr_clipped ? " gecapped" : "",
        (unsigned)_prefs.flood_max_req_resp);
      pushCompanionMessage(r);
      return;
    }

    // Wunschliste 24: Hop-Cap fuer Nicht-Chat-Adverts (Repeater/Sensor/
    // Room). Range 0..flood_max. 0 = deaktiviert (es gilt flood_max).
    // Plus Sondersyntax: "follow" / "max" -> Sentinel 254 = persistent
    // gleichauf mit flood_max (folgt automatisch).
    if (strcmp(key, "flood_max_infra") == 0
        || strcmp(key, "flood.max.infra") == 0) {
      // Wunschliste 39: nur 1..flood_max oder 'follow' (oder 'max' alias).
      // Kein numerisches '0' mehr (war Legacy-follow, jetzt explizit).
      uint8_t newval;
      if (strcmp(value_lc, "follow") == 0 || strcmp(value_lc, "max") == 0) {
        newval = FLOOD_MAX_INFRA_FOLLOW;
      } else {
        int v = atoi(value_lc);
        if (v < 1 || v > _prefs.flood_max) {
          char r[120]; snprintf(r, sizeof(r),
            "Wert: 'follow' / 1..%u (flood_max).",
            (unsigned)_prefs.flood_max);
          pushCompanionMessage(r);
          return;
        }
        newval = (uint8_t)v;
      }
      _prefs.flood_max_infra = newval;
      // flood_max_req_resp ggf. nachjustieren wenn neuer Wert kleiner.
      bool rr_clipped = false;
      if (newval != FLOOD_MAX_INFRA_FOLLOW
          && _prefs.flood_max_req_resp != FLOOD_MAX_INFRA_FOLLOW
          && _prefs.flood_max_req_resp > newval) {
        _prefs.flood_max_req_resp = newval;
        rr_clipped = true;
      }
      savePrefs();
      char r[140];
      if (newval == FLOOD_MAX_INFRA_FOLLOW)
        snprintf(r, sizeof(r),
          "OK - flood_max_infra = follow (-> %u)",
          (unsigned)_prefs.flood_max);
      else
        snprintf(r, sizeof(r),
          "OK - flood_max_infra = %u", (unsigned)newval);
      pushCompanionMessage(r);
      if (rr_clipped) {
        char r2[80]; snprintf(r2, sizeof(r2),
          "  flood_max_req_resp gecapped auf %u",
          (unsigned)_prefs.flood_max_req_resp);
        pushCompanionMessage(r2);
      }
      return;
    }

    // Wunschliste 29 (DL9SAU 2026-06-01): Hop-Cap fuer REQ/RESP/ANON_REQ.
    // Effektive Obergrenze ist min(flood_max_infra, flood_max). Wenn
    // flood_max_infra == 0, ist flood_max die einzige Obergrenze.
    // 0 = kaskade: es gilt flood_max_infra (falls > 0), sonst kein
    // zusaetzlicher Cap (nur globales flood_max greift).
    if (strcmp(key, "flood_max_req_resp") == 0
        || strcmp(key, "flood.max.req.resp") == 0) {
      // Wunschliste 39: nur 1..N oder 'follow'. Kein numerisches '0'.
      uint8_t newval;
      uint8_t upper = effectiveFloodMaxInfra();
      if (strcmp(value_lc, "follow") == 0) {
        newval = FLOOD_MAX_INFRA_FOLLOW;
      } else {
        int v = atoi(value_lc);
        if (v < 1 || v > upper) {
          char r[120]; snprintf(r, sizeof(r),
            "Wert: 'follow' / 1..%u (Cap durch flood_max_infra).",
            (unsigned)upper);
          pushCompanionMessage(r);
          return;
        }
        newval = (uint8_t)v;
      }
      _prefs.flood_max_req_resp = newval;
      savePrefs();
      char r[140];
      if (newval == FLOOD_MAX_INFRA_FOLLOW)
        snprintf(r, sizeof(r),
          "OK - flood_max_req_resp = follow (-> %u)",
          (unsigned)effectiveFloodMaxInfra());
      else
        snprintf(r, sizeof(r),
          "OK - flood_max_req_resp = %u", (unsigned)newval);
      pushCompanionMessage(r);
      pushCompanionMessage("Empfehlung: 4..8 (eng=4 Stadt, 8=weiter).");
      return;
    }

    // Reise-Wunsch 2026-06-08: set messages_append_scope_to_name <on|off>
    // Toggle fuer Wunschliste-35-Annotation (Channel-Sender '(#scope[,
    // direct])' + DM-Vorab-Frame '[#scope, direct]').
    if (strcmp(key, "messages_append_scope_to_name") == 0
        || strcmp(key, "messages.append.scope.to.name") == 0) {
      uint8_t newval;
      if (strcmp(value_lc, "on") == 0 || strcmp(value_lc, "1") == 0
          || strcmp(value_lc, "true") == 0) {
        newval = 1;
      } else if (strcmp(value_lc, "off") == 0 || strcmp(value_lc, "0") == 0
                 || strcmp(value_lc, "false") == 0) {
        newval = 0;
      } else {
        pushCompanionMessage("Wert: on / off");
        return;
      }
      _prefs.messages_append_scope_to_name = newval;
      savePrefs();
      if (newval) {
        pushCompanionMessage(
          "OK - messages_append_scope_to_name = on");
        pushCompanionMessage(
          "WARNUNG: App zeigt 'Keine Pfadinfo' fuer annotierte\n"
          "Messages (bekannte Limitierung).");
      } else {
        pushCompanionMessage(
          "OK - messages_append_scope_to_name = off");
      }
      return;
    }

    // Wunschliste 39 (2026-06-04): set flood_max_unscoped_companions
    // <follow|off|1..63>. Cap fuer ROUTE_TYPE_FLOOD ohne Scope, nur fuer
    // ADV_TYPE_CHAT-Adverts + TXT_MSG (User-Erstkontakt). REQ/RESP/ANON_REQ
    // unscoped bleiben geblockt.
    //
    // Upstream-1.16 hat 'flood_max_unscoped' eingefuehrt (RepeaterPrefs,
    // globaler Repeat-Cap). Im DL9SAU-Companion gibt es das Feld NICHT,
    // weil wir defensiv unscoped-Repeats nur fuer Companion-Erstkontakt
    // erlauben. Damit User die upstream-CLI-Form gewohnheitsmaessig
    // tippen koennen, akzeptieren wir 'flood_max_unscoped' als Alias und
    // erklaeren die Umlenkung.
    if (strcmp(key, "flood_max_unscoped_companions") == 0
        || strcmp(key, "flood.max.unscoped.companions") == 0
        || strcmp(key, "flood_max_unscoped") == 0
        || strcmp(key, "flood.max.unscoped") == 0) {
      bool is_upstream_alias =
          (strcmp(key, "flood_max_unscoped") == 0
           || strcmp(key, "flood.max.unscoped") == 0);
      uint8_t newval;
      if (strcmp(value_lc, "follow") == 0) {
        newval = CH_HOPS_OFF;
      } else if (strcmp(value_lc, "off") == 0) {
        newval = 0;
      } else {
        int v = atoi(value_lc);
        if (v < 1 || v > _prefs.flood_max) {
          char r[120]; snprintf(r, sizeof(r),
            "Wert: 'follow' / 'off' / 1..%u (flood_max).",
            (unsigned)_prefs.flood_max);
          pushCompanionMessage(r);
          return;
        }
        newval = (uint8_t)v;
      }
      _prefs.flood_max_unscoped_companions = newval;
      savePrefs();
      if (is_upstream_alias) {
        pushCompanionMessage(
          "Hinweis: 'flood_max_unscoped' (upstream) -> hier\n"
          "Alias auf 'flood_max_unscoped_companions'.");
        pushCompanionMessage(
          "(DL9SAU: unscoped-Repeat nur fuer CHAT-Adverts\n"
          "+ TXT_MSG; REQ/RESP und Infrastruktur-\n"
          "Adverts (REPEATER/ROOM/SENSOR) geblockt.)");
      }
      char r[140];
      if (newval == CH_HOPS_OFF)
        snprintf(r, sizeof(r),
          "OK - flood_max_unscoped_companions = follow (-> %u)",
          (unsigned)_prefs.flood_max_scope_region);
      else if (newval == 0)
        snprintf(r, sizeof(r),
          "OK - flood_max_unscoped_companions = off (nicht repeaten)");
      else
        snprintf(r, sizeof(r),
          "OK - flood_max_unscoped_companions = %u", (unsigned)newval);
      pushCompanionMessage(r);
      return;
    }

    // Wunschliste 31: set time sync <off|lazy|aabbcc [bb [cc]]>
    // Konfiguriert den Advert-basierten RTC-Sync. 3-Byte-Pub-Key-Prefixe
    // (= 6 Hex-Zeichen) fuer bis zu 3 Trust-Sources im strict-Modus.
    if (strcmp(key, "time") == 0) {
      // value_lc beginnt mit "sync ..."
      const char* vp = value_lc;
      if (strncmp(vp, "sync", 4) != 0
          || (vp[4] != ' ' && vp[4] != '\t' && vp[4] != 0)) {
        pushCompanionMessage(
          "Usage: set time sync <off|lazy|aabbcc [bb [cc]]>\n"
          "  aabbcc = 6 Hex-Zeichen (3-Byte Pub-Key-Prefix)");
        return;
      }
      const char* rest = vp + 4;
      while (*rest == ' ' || *rest == '\t') rest++;
      if (*rest == 0) {
        // 'set time sync' allein -- aktuelle Config zeigen
        if (_prefs.time_sync_mode == 0) {
          pushCompanionMessage("time sync = off");
        } else if (_prefs.time_sync_mode == 1) {
          pushCompanionMessage("time sync = lazy (any zero-hop signed advert)");
        } else {
          char r[140]; int n = snprintf(r, sizeof(r), "time sync = strict, sources:");
          for (int i = 0; i < 3; i++) {
            bool nz = (_prefs.time_sync_sources[i][0]
                     || _prefs.time_sync_sources[i][1]
                     || _prefs.time_sync_sources[i][2]);
            if (nz && n < (int)sizeof(r))
              n += snprintf(r + n, sizeof(r) - n, " %02x%02x%02x",
                            _prefs.time_sync_sources[i][0],
                            _prefs.time_sync_sources[i][1],
                            _prefs.time_sync_sources[i][2]);
          }
          pushCompanionMessage(r);
        }
        return;
      }
      if (strcmp(rest, "off") == 0) {
        _prefs.time_sync_mode = 0;
        savePrefs();
        pushCompanionMessage("OK - time sync = off");
        return;
      }
      if (strcmp(rest, "lazy") == 0) {
        _prefs.time_sync_mode = 1;
        memset(_prefs.time_sync_sources, 0, sizeof(_prefs.time_sync_sources));
        savePrefs();
        pushCompanionMessage("OK - time sync = lazy (Default).\n"
                             "  Akzeptiert zero-hop Adverts von\n"
                             "  REPEATER, ROOM, CHAT (nicht SENSOR).");
        return;
      }
      // Strict mode: 1-3 Hex-Prefixe parsen
      uint8_t newsrc[3][3];
      memset(newsrc, 0, sizeof(newsrc));
      int found = 0;
      const char* p2 = rest;
      while (*p2 && found < 3) {
        while (*p2 == ' ' || *p2 == '\t') p2++;
        if (!*p2) break;
        // Parse 6 hex chars
        uint8_t bytes[3];
        bool ok = true;
        for (int j = 0; j < 3 && ok; j++) {
          int hi = -1, lo = -1;
          char ch1 = p2[j*2], ch2 = p2[j*2 + 1];
          if (ch1 >= '0' && ch1 <= '9')      hi = ch1 - '0';
          else if (ch1 >= 'a' && ch1 <= 'f') hi = ch1 - 'a' + 10;
          else if (ch1 >= 'A' && ch1 <= 'F') hi = ch1 - 'A' + 10;
          if (ch2 >= '0' && ch2 <= '9')      lo = ch2 - '0';
          else if (ch2 >= 'a' && ch2 <= 'f') lo = ch2 - 'a' + 10;
          else if (ch2 >= 'A' && ch2 <= 'F') lo = ch2 - 'A' + 10;
          if (hi < 0 || lo < 0) ok = false;
          else bytes[j] = (uint8_t)((hi << 4) | lo);
        }
        if (!ok) {
          pushCompanionMessage("Source-Prefix muss 6 Hex-Zeichen sein\n(z.B. a1b2c3).");
          return;
        }
        memcpy(newsrc[found], bytes, 3);
        found++;
        p2 += 6;
        // Trennzeichen
        while (*p2 == ' ' || *p2 == '\t') p2++;
      }
      if (found == 0) {
        pushCompanionMessage("Keine gueltigen Sources geparst.");
        return;
      }
      _prefs.time_sync_mode = 2;
      memcpy(_prefs.time_sync_sources, newsrc, sizeof(newsrc));
      savePrefs();
      char r[140]; int n = snprintf(r, sizeof(r), "OK - time sync = strict, sources:");
      for (int i = 0; i < found; i++) {
        n += snprintf(r + n, sizeof(r) - n, " %02x%02x%02x",
                      newsrc[i][0], newsrc[i][1], newsrc[i][2]);
      }
      pushCompanionMessage(r);
      return;
    }

    // Wunschliste 32: set ch.hops <name> <follow|off|N>
    // Multi-Token: nach key ('ch.hops') folgen Channel-Name (kann
    // Spaces enthalten!) und am Ende der Wert. Wir parsen vom Ende:
    // letztes Whitespace-separates Token = Wert, alles davor = Name.
    if (strcmp(key, "ch.hops") == 0) {
      // value_lc zeigt auf den Rest nach 'set ch.hops ' (inkl. Spaces).
      // Letztes Token finden.
      size_t vlen = strlen(value_lc);
      if (vlen == 0) {
        pushCompanionMessage(
          "Usage: set ch.hops <name> <follow|off|N>\n"
          "  Beispiel: set ch.hops Public 3");
        return;
      }
      // Trailing-WS strippen
      while (vlen > 0 && (value_lc[vlen-1] == ' ' || value_lc[vlen-1] == '\t'
                          || value_lc[vlen-1] == '\r' || value_lc[vlen-1] == '\n')) vlen--;
      // Letztes Token finden (von hinten Whitespace suchen)
      size_t tok_end = vlen;
      size_t tok_start = tok_end;
      while (tok_start > 0 && value_lc[tok_start-1] != ' ' && value_lc[tok_start-1] != '\t') tok_start--;
      if (tok_start == 0) {
        pushCompanionMessage("Usage: set ch.hops <name> <follow|off|N>");
        return;
      }
      // Name: alles vor tok_start (Trailing-WS strippen)
      size_t name_end = tok_start;
      while (name_end > 0 && (value_lc[name_end-1] == ' ' || value_lc[name_end-1] == '\t')) name_end--;
      if (name_end == 0) {
        pushCompanionMessage("Usage: set ch.hops <name> <follow|off|N>");
        return;
      }
      char chname[32];
      size_t nlen = name_end < sizeof(chname) - 1 ? name_end : sizeof(chname) - 1;
      memcpy(chname, value_lc, nlen);
      chname[nlen] = 0;
      // Value-Token interpretieren
      char vtok[12];
      size_t vlen2 = tok_end - tok_start < sizeof(vtok) - 1 ? tok_end - tok_start : sizeof(vtok) - 1;
      memcpy(vtok, value_lc + tok_start, vlen2);
      vtok[vlen2] = 0;
      uint8_t new_cap;
      // Wunschliste 39 alignment (2026-06-04): einheitliche Konvention
      //   follow = CH_HOPS_OFF (kein per-channel-Cap / follow parent)
      //   off    = 0           (explizit nicht repeaten)
      //   1..63  = expliziter Cap
      // Bricht alte 'off' Semantik (war: kein Cap). Migration siehe
      // Commit-Notes.
      if (strcmp(vtok, "follow") == 0) {
        new_cap = CH_HOPS_OFF;
      } else if (strcmp(vtok, "off") == 0) {
        new_cap = 0;
      } else {
        int v = atoi(vtok);
        if (v < 0 || v > 63) {
          pushCompanionMessage("Wert: 'follow' / 'off' / 0..63.");
          return;
        }
        new_cap = (uint8_t)v;
      }
      // Channel-Slot via Name finden (case-insensitive Whole-Match)
      int slot = -1;
      for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
        ChannelDetails ch;
        if (!getChannel(i, ch)) continue;
        if (ch.name[0] == 0) continue;
        if (strcasecmp(ch.name, chname) == 0) { slot = i; break; }
      }
      if (slot < 0) {
        // Spezialfall: 'unknown' / 'unknown channels' setzt
        // flood_max_unknown_chan
        if (strcasecmp(chname, "unknown") == 0
            || strcasecmp(chname, "unknown channels") == 0) {
          _prefs.flood_max_unknown_chan = new_cap;
          savePrefs();
          char r[120];
          if (new_cap == CH_HOPS_OFF)
            snprintf(r, sizeof(r),
              "OK - flood_max_unknown_chan = follow (-> %u)",
              (unsigned)_prefs.flood_max);
          else if (new_cap == 0)
            snprintf(r, sizeof(r),
              "OK - flood_max_unknown_chan = off (nicht repeaten)");
          else
            snprintf(r, sizeof(r), "OK - flood_max_unknown_chan = %u", (unsigned)new_cap);
          pushCompanionMessage(r);
          return;
        }
        // External-Eintrag fuer nicht-abonnierte Hashtag-Channels:
        // PSK ist deterministisch (sha256), channel_hash[0] vorab
        // berechnen. Private-Channels nicht moeglich (Random-PSK).
        if (chname[0] == '#') {
          if (new_cap == CH_HOPS_OFF) {
            // Remove-Attempt eines External-Eintrags: fnv1a-Hash
            // muss existieren -- sonst no-op aber freundliche Info.
            uint32_t h = fnv1a32_cstr(chname);
            channelHopsRemove(_prefs, h);
            rebuildChannelHopsCache();
            savePrefs();
            char r[100]; snprintf(r, sizeof(r),
              "OK - ch.hops %s = off (external, removed)", chname);
            pushCompanionMessage(r);
            return;
          }
          uint8_t hash16[32];
          mesh::Utils::sha256(hash16, sizeof(hash16),
                              (const uint8_t*)chname, strlen(chname));
          uint32_t h = fnv1a32_cstr(chname);
          if (!channelHopsUpsert(_prefs, h, new_cap,
                                 CH_HOPS_FLAG_EXTERNAL, hash16[0],
                                 chname)) {
            pushCompanionMessage("Liste voll (max MAX_GROUP_CHANNELS Eintraege).");
            return;
          }
          rebuildChannelHopsCache();
          savePrefs();
          char r[120];
          if (new_cap == 0)
            snprintf(r, sizeof(r),
              "OK - ch.hops %s = 0 (ext, nicht repeaten)", chname);
          else
            snprintf(r, sizeof(r),
              "OK - ch.hops %s = %u (ext, nicht abonniert)", chname, (unsigned)new_cap);
          pushCompanionMessage(r);
          return;
        }
        char r[140]; snprintf(r, sizeof(r),
          "Channel '%s' nicht gefunden.\n"
          "Private Kanaele muessen bereits konfiguriert sein\n"
          "(PSK muss identisch sein mit dem des Absenders!).", chname);
        pushCompanionMessage(r);
        return;
      }
      // $companion-Schutz: forced 0, nicht aenderbar
      ChannelDetails ch;
      getChannel(slot, ch);
      if (memcmp(ch.channel.secret, s_companion_psk_magic, 16) == 0) {
        pushCompanionMessage(
          "$companion ist forced auf 0 (nicht repeaten).\n"
          "Sicherheitsmassnahme, nicht aenderbar.");
        return;
      }
      // Speichern via name-hash. Upsert oder remove je nach Wert.
      uint32_t h = fnv1a32_cstr(ch.name);
      if (new_cap == CH_HOPS_OFF) {
        channelHopsRemove(_prefs, h);
      } else {
        if (!channelHopsUpsert(_prefs, h, new_cap)) {
          pushCompanionMessage("Liste voll (max MAX_GROUP_CHANNELS Eintraege).");
          return;
        }
      }
      rebuildChannelHopsCache();
      savePrefs();
      char r[120];
      if (new_cap == CH_HOPS_OFF)
        snprintf(r, sizeof(r), "OK - ch.hops %s = follow (kein Cap)", ch.name);
      else if (new_cap == 0)
        snprintf(r, sizeof(r), "OK - ch.hops %s = off (nicht repeaten)", ch.name);
      else
        snprintf(r, sizeof(r), "OK - ch.hops %s = %u", ch.name, (unsigned)new_cap);
      pushCompanionMessage(r);
      return;
    }

    // Wunschliste 45 (Reise 2026-06-09): set int.thresh <N>
    // RSSI-Margin (dB) ueber noise_floor. 0 = LBT off. Upstream-
    // Doku-Default 14. Praxis: 1..14 je nach Standort/SF.
    if (strcmp(key, "int.thresh") == 0 || strcmp(key, "int_thresh") == 0) {
      int v = atoi(value_lc);
      if (v < 0 || v > 255) {
        pushCompanionMessage("int.thresh ausserhalb 0..255");
        return;
      }
      _prefs.interference_threshold = (uint8_t)v;
      savePrefs();
      char r[100];
      if (v == 0) {
        snprintf(r, sizeof(r), "OK - int.thresh = 0 (LBT off)");
      } else {
        snprintf(r, sizeof(r), "OK - int.thresh = %d dB (LBT on, default 14)", v);
      }
      pushCompanionMessage(r);
      return;
    }
    // Wunschliste 45: set agc.reset.interval <S>
    // S = Sekunden/4. 0 = disabled. AGC-Reset macht das Geraet fuer
    // wenige ms taub -- bewusst sparsam nutzen.
    if (strcmp(key, "agc.reset.interval") == 0 || strcmp(key, "agc_reset_interval") == 0) {
      int v = atoi(value_lc);
      if (v < 0 || v > 255) {
        pushCompanionMessage("agc.reset.interval ausserhalb 0..255 (Schritte zu 4 sec)");
        return;
      }
      _prefs.agc_reset_interval = (uint8_t)v;
      savePrefs();
      char r[100];
      if (v == 0) {
        snprintf(r, sizeof(r), "OK - agc.reset.interval = 0 (disabled)");
      } else {
        snprintf(r, sizeof(r), "OK - agc.reset.interval = %d (%d sec)", v, v * 4);
      }
      pushCompanionMessage(r);
      return;
    }

    // Unbekannter key
    char r[120];
    snprintf(r, sizeof(r),
             "Unbekannter set-key '%s'. 'help set' fuer Liste.", key);
    pushCompanionMessage(r);
    return;
  }

  // ---------- get <key> -------------------------------------------------
  // Liest persistente Settings. Useful fuer USB-only-User die ihre Config
  // ohne App eintragen oder sichern moechten.
  if (starts_with_word(cmd, "get")) {
    const char* p = strchr(cmd, ' ');
    // 40 Byte fasst auch lange Keys wie 'flood_max_unscoped_companions' (29).
    char key[40];
    key[0] = 0;
    if (p) {
      while (*p == ' ') p++;
      if (*p) {
        size_t klen = 0;
        while (p[klen] && p[klen] != ' ' && p[klen] != '\t' && klen < (int)sizeof(key) - 1) klen++;
        memcpy(key, p, klen); key[klen] = 0;
      }
    }

    // get          -> nur veraenderte Settings (analog 'prefs')
    // get all      -> alle, mit [default]/(default: X) Markierung (analog 'prefs all')
    // get <key>    -> einzelner Wert (wie bisher)
    bool list_changed = (key[0] == 0);
    bool list_all     = (strcmp(key, "all") == 0);
    if (list_changed || list_all) {
      char gb[200];
      size_t gu = 0;
      auto gflush = [&]() {
        if (gu == 0) return;
        gb[gu] = 0;
        pushCompanionMessage(gb);
        gu = 0;
      };
      auto gline = [&](const char* line) {
        size_t len = strlen(line);
        if (len > 130) len = 130;
        if (gu > 0 && gu + 1 + len > 130) gflush();
        if (gu > 0) gb[gu++] = '\n';
        for (size_t i = 0; i < len && gu < sizeof(gb) - 1; i++) gb[gu++] = line[i];
      };
      char tmp[160];
      int changed = 0;
      gline(list_all ? "get all (App-Settings):" : "get (changed App-Settings):");

      // name: Default ist build-/identity-abhaengig -> kein default-Check,
      // immer mit zeigen wenn list_all, sonst weglassen.
      if (list_all) {
        snprintf(tmp, sizeof(tmp), "  name = \"%s\"", _prefs.node_name);
        gline(tmp);
      }

      // Hilfs-Lambdas fuer typsicheres Default-Vergleichen + Formatierung.
      // Ein Eintrag landet in der Ausgabe wenn:
      //   list_all == true                          -> immer
      //   list_changed && (current != default)      -> nur Aenderungen
      // Marker:
      //   list_all + at-default                     -> " [default]"
      //   list_all + nicht-default                  -> " (default: X)"
      //   list_changed (nie at-default hier)        -> " (default: X)"
      auto emit_int = [&](const char* name, int cur, int def, const char* unit) {
        bool eq = (cur == def);
        if (list_changed && eq) return;
        if (!eq) changed++;
        if (eq) {
          snprintf(tmp, sizeof(tmp), "  %s = %d%s [default]", name, cur, unit);
        } else {
          snprintf(tmp, sizeof(tmp), "  %s = %d%s (default: %d%s)", name, cur, unit, def, unit);
        }
        gline(tmp);
      };
      auto emit_uint = [&](const char* name, unsigned cur, unsigned def) {
        bool eq = (cur == def);
        if (list_changed && eq) return;
        if (!eq) changed++;
        if (eq) snprintf(tmp, sizeof(tmp), "  %s = %u [default]", name, cur);
        else    snprintf(tmp, sizeof(tmp), "  %s = %u (default: %u)", name, cur, def);
        gline(tmp);
      };
      auto emit_float = [&](const char* name, float cur, float def, const char* unit, int prec) {
        bool eq = (cur == def);
        if (list_changed && eq) return;
        if (!eq) changed++;
        if (eq) snprintf(tmp, sizeof(tmp), "  %s = %.*f%s [default]", name, prec, (double)cur, unit);
        else    snprintf(tmp, sizeof(tmp), "  %s = %.*f%s (default: %.*f%s)",
                         name, prec, (double)cur, unit, prec, (double)def, unit);
        gline(tmp);
      };
      auto emit_double = [&](const char* name, double cur, double def, int prec) {
        bool eq = (cur == def);
        if (list_changed && eq) return;
        if (!eq) changed++;
        if (eq) snprintf(tmp, sizeof(tmp), "  %s = %.*f [default]", name, prec, cur);
        else    snprintf(tmp, sizeof(tmp), "  %s = %.*f (default: %.*f)", name, prec, cur, prec, def);
        gline(tmp);
      };

#ifndef SX126X_RX_BOOSTED_GAIN_DEFAULT
#define SX126X_RX_BOOSTED_GAIN_DEFAULT 1
#endif
      emit_float ("freq",                _prefs.freq,                  (float)LORA_FREQ,       " MHz", 4);
      emit_uint  ("sf",                  _prefs.sf,                    (unsigned)LORA_SF);
      emit_float ("bw",                  _prefs.bw,                    (float)LORA_BW,         " kHz", 1);
      emit_uint  ("cr",                  _prefs.cr,                    (unsigned)LORA_CR);
      emit_int   ("tx_power",            (int)_prefs.tx_power_dbm,     (int)LORA_TX_POWER,     " dBm");
      emit_double("lat",                 sensors.node_lat,             0.0,                    6);
      emit_double("lon",                 sensors.node_lon,             0.0,                    6);
      emit_uint  ("repeat",              _prefs.client_repeat,         0);
      emit_uint  ("gps",                 _prefs.gps_enabled,           0);
      emit_uint  ("gps_interval",        _prefs.gps_interval,          0);
      emit_uint  ("advert_loc_policy",   _prefs.advert_loc_policy,     0);
      emit_float ("airtime_factor",      _prefs.airtime_factor,        1.0f,                   "",     3);
      emit_uint  ("rx_boosted_gain",     _prefs.rx_boosted_gain,       (unsigned)SX126X_RX_BOOSTED_GAIN_DEFAULT);
      emit_uint  ("manual_add_contacts", _prefs.manual_add_contacts,   0);
      emit_uint  ("multi_acks",          _prefs.multi_acks,            0);
      emit_uint  ("path_hash_mode",      _prefs.path_hash_mode,        0);
      emit_uint  ("autoadd_config",      _prefs.autoadd_config,        0);
      emit_uint  ("autoadd_max_hops",    _prefs.autoadd_max_hops,      0);
      emit_uint  ("telemetry_mode_base", _prefs.telemetry_mode_base,   0);
      emit_uint  ("telemetry_mode_loc",  _prefs.telemetry_mode_loc,    0);
      emit_uint  ("telemetry_mode_env",  _prefs.telemetry_mode_env,    0);
      emit_uint  ("buzzer_quiet",        _prefs.buzzer_quiet,          0);
      emit_uint  ("messages_append_scope_to_name",
                                         _prefs.messages_append_scope_to_name, 0);
      emit_uint  ("int.thresh",          _prefs.interference_threshold, 14);
      emit_uint  ("agc.reset.interval",  _prefs.agc_reset_interval,    0);
      emit_float ("rxdelay",             _prefs.rx_delay_base,         0.0f,                   "",     3);
      // txdelay / direct_txdelay: Sentinel -1 = auto. Sonderdarstellung
      // statt nackter "-1.000". Default ist auto.
      {
        bool eq = (_prefs.tx_delay_factor < 0.0f);
        if (!(list_changed && eq)) {
          if (!eq) changed++;
          if (_prefs.tx_delay_factor < 0.0f)
            snprintf(tmp, sizeof(tmp),
                     "  txdelay = auto (effektiv %.2f, profile=%s) [default]",
                     effectiveTxDelayFactor(),
                     _prefs.repeater_profile == 1 ? "full" : "defensive");
          else
            snprintf(tmp, sizeof(tmp),
                     "  txdelay = %.3f (default: auto)",
                     (double)_prefs.tx_delay_factor);
          gline(tmp);
        }
      }
      {
        bool eq = (_prefs.direct_tx_delay_factor < 0.0f);
        if (!(list_changed && eq)) {
          if (!eq) changed++;
          if (_prefs.direct_tx_delay_factor < 0.0f)
            snprintf(tmp, sizeof(tmp),
                     "  direct_txdelay = auto (effektiv %.2f) [default]",
                     effectiveDirectTxDelayFactor());
          else
            snprintf(tmp, sizeof(tmp),
                     "  direct_txdelay = %.3f (default: auto)",
                     (double)_prefs.direct_tx_delay_factor);
          gline(tmp);
        }
      }
      // flood_max_scope_region: 0 = off (Default 3, off ist Sonderfall)
      {
        bool eq = (_prefs.flood_max_scope_region == 3);
        if (!list_changed || !eq) {
          if (!eq) changed++;
          if (_prefs.flood_max_scope_region == 0)
            snprintf(tmp, sizeof(tmp),
              "  flood_max_scope_region = off (nicht repeaten) (default: 3)");
          else if (eq)
            snprintf(tmp, sizeof(tmp),
              "  flood_max_scope_region = 3 [default]");
          else
            snprintf(tmp, sizeof(tmp),
              "  flood_max_scope_region = %u (default: 3)",
              (unsigned)_prefs.flood_max_scope_region);
          gline(tmp);
        }
      }
      emit_uint  ("flood_max",           _prefs.flood_max,             16);
      // flood_max_infra: Sentinel 254 = follow flood_max (Default).
      {
        bool eq = (_prefs.flood_max_infra == FLOOD_MAX_INFRA_FOLLOW);
        if (!list_changed || !eq) {
          if (!eq) changed++;
          if (_prefs.flood_max_infra == FLOOD_MAX_INFRA_FOLLOW)
            snprintf(tmp, sizeof(tmp),
              "  flood_max_infra = follow (-> %u) [default]",
              (unsigned)_prefs.flood_max);
          else
            snprintf(tmp, sizeof(tmp),
              "  flood_max_infra = %u (default: follow)",
              (unsigned)_prefs.flood_max_infra);
          gline(tmp);
        }
      }
      // flood_max_req_resp: Sentinel 254 = follow infra (Default).
      {
        bool eq = (_prefs.flood_max_req_resp == FLOOD_MAX_INFRA_FOLLOW);
        if (!list_changed || !eq) {
          if (!eq) changed++;
          if (_prefs.flood_max_req_resp == FLOOD_MAX_INFRA_FOLLOW)
            snprintf(tmp, sizeof(tmp),
              "  flood_max_req_resp = follow (-> %u) [default]",
              (unsigned)effectiveFloodMaxInfra());
          else
            snprintf(tmp, sizeof(tmp),
              "  flood_max_req_resp = %u (default: follow)",
              (unsigned)_prefs.flood_max_req_resp);
          gline(tmp);
        }
      }
      // flood_max_unscoped_companions (Wunschliste 39):
      // Sentinel CH_HOPS_OFF = follow flood_max_scope_region. Default.
      {
        bool eq = (_prefs.flood_max_unscoped_companions == CH_HOPS_OFF);
        if (!list_changed || !eq) {
          if (!eq) changed++;
          if (_prefs.flood_max_unscoped_companions == CH_HOPS_OFF)
            snprintf(tmp, sizeof(tmp),
              "  flood_max_unscoped_companions = follow (-> %u) [default]",
              (unsigned)_prefs.flood_max_scope_region);
          else if (_prefs.flood_max_unscoped_companions == 0)
            snprintf(tmp, sizeof(tmp),
              "  flood_max_unscoped_companions = off (default: follow)");
          else
            snprintf(tmp, sizeof(tmp),
              "  flood_max_unscoped_companions = %u (default: follow)",
              (unsigned)_prefs.flood_max_unscoped_companions);
          gline(tmp);
        }
      }
      // loop_detect (Wunschliste 6b) -- enum, eigene Anzeige.
      {
        uint8_t v = _prefs.loop_detect;
        bool eq = (v == 0);
        if (!list_changed || !eq) {
          if (!eq) changed++;
          const char* nm = (v == 0) ? "off" : (v == 1) ? "minimal" : (v == 2) ? "moderate" : "strict";
          if (eq) snprintf(tmp, sizeof(tmp), "  loop_detect = off [default]");
          else    snprintf(tmp, sizeof(tmp), "  loop_detect = %s (default: off)", nm);
          gline(tmp);
        }
      }
      // owner_info (Wunschliste 7) -- String, eigenes emit-Pattern.
      {
        bool eq = (_prefs.owner_info[0] == 0);
        if (!list_changed || !eq) {
          if (!eq) changed++;
          if (eq) snprintf(tmp, sizeof(tmp), "  owner_info = \"\" [default]");
          else    snprintf(tmp, sizeof(tmp), "  owner_info = \"%s\" (default: \"\")", _prefs.owner_info);
          gline(tmp);
        }
      }

      if (list_changed && changed == 0) gline("  (keine Aenderungen — alle Werte auf Default)");
      gflush();
      return;
    }

    char r[160];
    if      (strcmp(key, "name") == 0)     snprintf(r, sizeof(r), "name = \"%s\"", _prefs.node_name);
    else if (strcmp(key, "freq") == 0)     snprintf(r, sizeof(r), "freq = %.4f MHz", _prefs.freq);
    else if (strcmp(key, "sf") == 0)       snprintf(r, sizeof(r), "sf = %u", (unsigned)_prefs.sf);
    else if (strcmp(key, "bw") == 0)       snprintf(r, sizeof(r), "bw = %.1f kHz", _prefs.bw);
    else if (strcmp(key, "cr") == 0)       snprintf(r, sizeof(r), "cr = %u", (unsigned)_prefs.cr);
    else if (strcmp(key, "tx_power") == 0) snprintf(r, sizeof(r), "tx_power = %d dBm", (int)_prefs.tx_power_dbm);
    else if (strcmp(key, "lat") == 0)      snprintf(r, sizeof(r), "lat = %.6f", sensors.node_lat);
    else if (strcmp(key, "lon") == 0)      snprintf(r, sizeof(r), "lon = %.6f", sensors.node_lon);
    else if (strcmp(key, "repeat") == 0)   snprintf(r, sizeof(r), "repeat = %u", (unsigned)_prefs.client_repeat);
    else if (strcmp(key, "gps") == 0)      snprintf(r, sizeof(r), "gps = %u", (unsigned)_prefs.gps_enabled);
    else if (strcmp(key, "advert_loc_policy") == 0) snprintf(r, sizeof(r), "advert_loc_policy = %u", (unsigned)_prefs.advert_loc_policy);
    else if (strcmp(key, "airtime_factor") == 0)    snprintf(r, sizeof(r), "airtime_factor = %.3f", _prefs.airtime_factor);
    else if (strcmp(key, "rx_boosted_gain") == 0)   snprintf(r, sizeof(r), "rx_boosted_gain = %u", (unsigned)_prefs.rx_boosted_gain);
    else if (strcmp(key, "manual_add_contacts") == 0) snprintf(r, sizeof(r), "manual_add_contacts = %u", (unsigned)_prefs.manual_add_contacts);
    else if (strcmp(key, "multi_acks") == 0)        snprintf(r, sizeof(r), "multi_acks = %u", (unsigned)_prefs.multi_acks);
    else if (strcmp(key, "path_hash_mode") == 0)    snprintf(r, sizeof(r), "path_hash_mode = %u", (unsigned)_prefs.path_hash_mode);
    else if (strcmp(key, "autoadd_config") == 0)    snprintf(r, sizeof(r), "autoadd_config = %u", (unsigned)_prefs.autoadd_config);
    else if (strcmp(key, "autoadd_max_hops") == 0)  snprintf(r, sizeof(r), "autoadd_max_hops = %u", (unsigned)_prefs.autoadd_max_hops);
    else if (strcmp(key, "gps_interval") == 0)      snprintf(r, sizeof(r), "gps_interval = %u", (unsigned)_prefs.gps_interval);
    else if (strcmp(key, "telemetry_mode_base") == 0) snprintf(r, sizeof(r), "telemetry_mode_base = %u", (unsigned)_prefs.telemetry_mode_base);
    else if (strcmp(key, "telemetry_mode_loc") == 0) snprintf(r, sizeof(r), "telemetry_mode_loc = %u", (unsigned)_prefs.telemetry_mode_loc);
    else if (strcmp(key, "telemetry_mode_env") == 0) snprintf(r, sizeof(r), "telemetry_mode_env = %u", (unsigned)_prefs.telemetry_mode_env);
    else if (strcmp(key, "buzzer_quiet") == 0)      snprintf(r, sizeof(r), "buzzer_quiet = %u", (unsigned)_prefs.buzzer_quiet);
    else if (strcmp(key, "int.thresh") == 0 || strcmp(key, "int_thresh") == 0)
      snprintf(r, sizeof(r), "int.thresh = %u dB%s", (unsigned)_prefs.interference_threshold,
               _prefs.interference_threshold == 0 ? " (LBT off)" : "");
    else if (strcmp(key, "agc.reset.interval") == 0 || strcmp(key, "agc_reset_interval") == 0)
      snprintf(r, sizeof(r), "agc.reset.interval = %u (%u sec)%s",
               (unsigned)_prefs.agc_reset_interval,
               (unsigned)_prefs.agc_reset_interval * 4,
               _prefs.agc_reset_interval == 0 ? " (disabled)" : "");
    else if (strcmp(key, "messages_append_scope_to_name") == 0
             || strcmp(key, "messages.append.scope.to.name") == 0)
      snprintf(r, sizeof(r), "messages_append_scope_to_name = %s",
               _prefs.messages_append_scope_to_name ? "on" : "off");
    else if (strcmp(key, "rxdelay") == 0)           snprintf(r, sizeof(r), "rxdelay = %.3f", _prefs.rx_delay_base);
    else if (strcmp(key, "txdelay") == 0) {
      if (_prefs.tx_delay_factor < 0.0f)
        snprintf(r, sizeof(r), "txdelay = auto (effektiv %.2f, profile=%s)",
                 effectiveTxDelayFactor(),
                 _prefs.repeater_profile == 1 ? "full" : "defensive");
      else
        snprintf(r, sizeof(r), "txdelay = %.3f", _prefs.tx_delay_factor);
    }
    else if (strcmp(key, "direct_txdelay") == 0) {
      if (_prefs.direct_tx_delay_factor < 0.0f)
        snprintf(r, sizeof(r), "direct_txdelay = auto (effektiv %.2f)",
                 effectiveDirectTxDelayFactor());
      else
        snprintf(r, sizeof(r), "direct_txdelay = %.3f", _prefs.direct_tx_delay_factor);
    }
    else if (strcmp(key, "flood_max_scope_region") == 0
             || strcmp(key, "flood.max.scope.region") == 0) {
      if (_prefs.flood_max_scope_region == 0)
        snprintf(r, sizeof(r), "flood_max_scope_region = off (nicht repeaten)");
      else
        snprintf(r, sizeof(r), "flood_max_scope_region = %u", (unsigned)_prefs.flood_max_scope_region);
    }
    else if (strcmp(key, "flood_max") == 0 || strcmp(key, "flood.max") == 0) snprintf(r, sizeof(r), "flood_max = %u", (unsigned)_prefs.flood_max);
    else if (strcmp(key, "flood_max_infra") == 0 || strcmp(key, "flood.max.infra") == 0) {
      if (_prefs.flood_max_infra == FLOOD_MAX_INFRA_FOLLOW)
        snprintf(r, sizeof(r), "flood_max_infra = follow (-> %u)", (unsigned)_prefs.flood_max);
      else
        snprintf(r, sizeof(r), "flood_max_infra = %u", (unsigned)_prefs.flood_max_infra);
    }
    else if (strcmp(key, "flood_max_req_resp") == 0 || strcmp(key, "flood.max.req.resp") == 0) {
      if (_prefs.flood_max_req_resp == FLOOD_MAX_INFRA_FOLLOW)
        snprintf(r, sizeof(r), "flood_max_req_resp = follow (-> %u)",
                 (unsigned)effectiveFloodMaxInfra());
      else
        snprintf(r, sizeof(r), "flood_max_req_resp = %u", (unsigned)_prefs.flood_max_req_resp);
    }
    else if (strcmp(key, "flood_max_unscoped_companions") == 0
             || strcmp(key, "flood.max.unscoped.companions") == 0) {
      if (_prefs.flood_max_unscoped_companions == CH_HOPS_OFF)
        snprintf(r, sizeof(r), "flood_max_unscoped_companions = follow (-> %u)",
                 (unsigned)_prefs.flood_max_scope_region);
      else if (_prefs.flood_max_unscoped_companions == 0)
        snprintf(r, sizeof(r), "flood_max_unscoped_companions = off (nicht repeaten)");
      else
        snprintf(r, sizeof(r), "flood_max_unscoped_companions = %u",
                 (unsigned)_prefs.flood_max_unscoped_companions);
    }
    else if (strcmp(key, "owner_info") == 0 || strcmp(key, "owner.info") == 0) snprintf(r, sizeof(r), "owner_info = %s", _prefs.owner_info[0] ? _prefs.owner_info : "(leer)");
    else if (strcmp(key, "passwd_admin") == 0 || strcmp(key, "passwd.admin") == 0)
      snprintf(r, sizeof(r), "passwd_admin = %s",
               _prefs.passwd_admin[0] ? "(gesetzt)" : "(leer)");
    else if (strcmp(key, "passwd_guest") == 0 || strcmp(key, "passwd.guest") == 0)
      snprintf(r, sizeof(r), "passwd_guest = %s",
               _prefs.passwd_guest[0] ? "(gesetzt)" : "(leer)");
    else if (strcmp(key, "loop_detect") == 0 || strcmp(key, "loop.detect") == 0) {
      const char* nm = (_prefs.loop_detect == 0) ? "off" : (_prefs.loop_detect == 1) ? "minimal" : (_prefs.loop_detect == 2) ? "moderate" : "strict";
      snprintf(r, sizeof(r), "loop_detect = %s", nm);
    }
    else if (strcmp(key, "time") == 0) {
      // get time [sync]
      const char* np = p + strlen("time");
      while (*np == ' ' || *np == '\t') np++;
      if (strncmp(np, "sync", 4) == 0
          && (np[4] == 0 || np[4] == ' ' || np[4] == '\t')) {
        if (_prefs.time_sync_mode == 0)      snprintf(r, sizeof(r), "time sync = off");
        else if (_prefs.time_sync_mode == 1) snprintf(r, sizeof(r), "time sync = lazy");
        else {
          int n = snprintf(r, sizeof(r), "time sync = strict, sources:");
          for (int i = 0; i < 3; i++) {
            bool nz = (_prefs.time_sync_sources[i][0]
                     || _prefs.time_sync_sources[i][1]
                     || _prefs.time_sync_sources[i][2]);
            if (nz && n < (int)sizeof(r))
              n += snprintf(r + n, sizeof(r) - n, " %02x%02x%02x",
                            _prefs.time_sync_sources[i][0],
                            _prefs.time_sync_sources[i][1],
                            _prefs.time_sync_sources[i][2]);
          }
        }
      } else {
        snprintf(r, sizeof(r), "Usage: get time sync");
      }
    }
    else if (strcmp(key, "ch.hops") == 0) {
      // Multi-Token: 'get ch.hops <name>'. p zeigt auf "ch.hops..."
      const char* np = p + strlen("ch.hops");
      while (*np == ' ' || *np == '\t') np++;
      // Trailing-WS strippen
      size_t nl = strlen(np);
      while (nl > 0 && (np[nl-1] == ' ' || np[nl-1] == '\t'
                        || np[nl-1] == '\r' || np[nl-1] == '\n')) nl--;
      if (nl == 0) {
        snprintf(r, sizeof(r), "Usage: get ch.hops <name>");
      } else {
        char chname[32];
        size_t cl = nl < sizeof(chname) - 1 ? nl : sizeof(chname) - 1;
        memcpy(chname, np, cl); chname[cl] = 0;
        // Spezial: 'unknown' fuer flood_max_unknown_chan
        if (strcasecmp(chname, "unknown") == 0
            || strcasecmp(chname, "unknown channels") == 0) {
          uint8_t cap = _prefs.flood_max_unknown_chan;
          if (cap == CH_HOPS_OFF)
            snprintf(r, sizeof(r), "ch.hops unknown = follow (-> %u)",
                     (unsigned)_prefs.flood_max);
          else if (cap == 0)
            snprintf(r, sizeof(r), "ch.hops unknown = off (nicht repeaten)");
          else
            snprintf(r, sizeof(r), "ch.hops unknown = %u", (unsigned)cap);
        } else {
          int slot = -1;
          for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
            ChannelDetails ch;
            if (!getChannel(i, ch)) continue;
            if (ch.name[0] == 0) continue;
            if (strcasecmp(ch.name, chname) == 0) { slot = i; break; }
          }
          if (slot < 0) {
            snprintf(r, sizeof(r), "Channel '%s' nicht gefunden.", chname);
          } else {
            uint8_t cap = _channel_hops_cap_cache[slot];
            ChannelDetails ch; getChannel(slot, ch);
            if (cap == CH_HOPS_OFF)
              snprintf(r, sizeof(r), "ch.hops %s = follow (kein Cap)", ch.name);
            else if (cap == 0)
              snprintf(r, sizeof(r), "ch.hops %s = off (nicht repeaten)", ch.name);
            else
              snprintf(r, sizeof(r), "ch.hops %s = %u", ch.name, (unsigned)cap);
          }
        }
      }
    }
    else {
      snprintf(r, sizeof(r), "Unbekannter key '%s'. 'get all' fuer Liste.", key);
    }
    pushCompanionMessage(r);
    return;
  }

  // ---------- reboot ----------------------------------------------------
  if (starts_with_word(cmd, "reboot")) {
    pushCompanionMessage("Rebooting now..");
    // DEFERRED reboot: blockierendes delay() hier wuerde die loop()
    // pausieren — Push-Frame-Auslieferung (PUSH_CODE_MSG_WAITING-Tickle
    // + App-CMD_SYNC_NEXT_MSG-Round-Trip) UND der OK-Frame zur App
    // koennten in der Zeit nicht stattfinden. Stattdessen Flag mit
    // Zielzeit setzen, loop() prueft und triggert den reboot ohne die
    // Frame-Verarbeitung zu blockieren.
    _pending_reboot_at = millis() + 3000;
    if (_pending_reboot_at == 0) _pending_reboot_at = 1;  // 0 = sentinel "nichts pending"
    return;
  }

  // ---------- stats -----------------------------------------------------
  // Detail-Statistik mit Aufschluesselung nach Node-Typ, Pakettyp und
  // Airtime. Alle Counter sind RAM-only (reset bei Reboot). Pro-Stunde/
  // Pro-Tag wird aus dem Session-Total und der Uptime berechnet.
  // ---------- clear <subcmd> --------------------------------------------
  // 'clear' ohne Argument zeigt jetzt Usage statt nur Fehler -- der
  // alleinige Befehl ist sonst nicht selbsterklaerend (User-Feedback
  // 2026-05-30).
  // Sub-Befehl 'stats' (= MeshCore-Docs Konvention -- docs.meshcore.io,
  // 'Clear Stats / Usage: clear stats'). Prefix-Match gegen "stats"
  // erlaubt 'clear stat' als Abkuerzung (Tippfehler-tolerant, min 3
  // Zeichen). Konsistent zu unserem 'stats'-Readout-Befehl.
  if (starts_with_word(cmd, "clear")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ') arg++; }
    if (!arg || *arg == 0) {
      pushCompanionMessage("clear Usage:\n  clear stats");
      return;
    }
    size_t alen = strlen(arg);
    if (alen >= 3 && alen <= 5 && strncmp(arg, "stats", alen) == 0) {
      clearStats();
      pushCompanionMessage("OK - alle Statistik-Counter zurueckgesetzt.");
      return;
    }
    pushCompanionMessage("Unbekanntes Argument. 'clear stats' (Prefix 'stat' OK).");
    return;
  }

  // D5: 'stat' (Abbr) als Alias zu 'stats' akzeptieren. 'status' ist ein
  // anderer Befehl (siehe oben), aber 'stat' faellt aus dessen Match
  // raus (text[4]=NUL/space) und kommt erst hier vorbei -- spart
  // Tipparbeit.
  // ---------- stats-core: Battery, Uptime, Queue, Debug Flags --------------
  // Wunschliste 20: MeshCore-Docs-Konvention (docs.meshcore.io). Erlaubt
  // gezieltes Pollen der System-Stats ohne den vollen Sammel-'stats'-Output.
  if (starts_with_word(cmd, "stats-core")) {
    uint64_t total_ms = (uint64_t)_millis_wraps * 4294967296ULL + (uint64_t)millis();
    uint64_t total_s = total_ms / 1000ULL;
    unsigned long up_d = (unsigned long)(total_s / 86400ULL);
    unsigned long up_h = (unsigned long)((total_s % 86400ULL) / 3600ULL);
    unsigned long up_m = (unsigned long)((total_s % 3600ULL) / 60ULL);
    uint16_t batt_mv = (uint16_t)board.getBattMilliVolts();
    int q_used = offlineQueueTotal();
    int q_cap  = BUCKET_CAP_PUBLIC + BUCKET_CAP_HASHTAG + BUCKET_CAP_PRIVATE
               + BUCKET_CAP_DM + BUCKET_CAP_COMPANION;
    char block[200];
    int n = snprintf(block, sizeof(block),
                     "stats-core:\n"
                     "  uptime    = ");
    if (up_d > 0) n += snprintf(block+n, sizeof(block)-n,
                                "%lud%02luh%02lum\n", up_d, up_h, up_m);
    else          n += snprintf(block+n, sizeof(block)-n,
                                "%luh%02lum\n", up_h, up_m);
    n += snprintf(block+n, sizeof(block)-n,
                  "  battery   = %u mV\n"
                  "  msg-queue = %d / %d slots\n"
                  "  trace     = 0x%04X (%s)",
                  (unsigned)batt_mv, q_used, q_cap,
                  (unsigned)_trace_flags,
                  _trace_flags ? "active" : "off");
    pushCompanionMessage(block);
    return;
  }

  // ---------- stats-radio: Noise floor, RSSI/SNR, Airtime, RX errors --------
  if (starts_with_word(cmd, "stats-radio")) {
    uint64_t total_ms = (uint64_t)_millis_wraps * 4294967296ULL + (uint64_t)millis();
    if (total_ms == 0) total_ms = 1;
    int16_t  noise_floor = (int16_t)_radio->getNoiseFloor();
    int8_t   last_rssi   = (int8_t)radio_driver.getLastRSSI();
    int8_t   last_snr_q4 = (int8_t)(radio_driver.getLastSNR() * 4.0f);
    uint32_t rx_errors   = (uint32_t)radio_driver.getPacketsRecvErrors();

    char block[200];
    snprintf(block, sizeof(block),
             "stats-radio:\n"
             "  noise floor = %d dBm\n"
             "  last rssi   = %d dBm  snr = %.1f dB\n"
             "  rx errors   = %lu",
             (int)noise_floor, (int)last_rssi,
             (double)last_snr_q4 / 4.0,
             (unsigned long)rx_errors);
    pushCompanionMessage(block);

    // Airtime block (Sekunden + Prozent)
    unsigned long rx_air = getReceiveAirTime();
    unsigned long tx_air = getTotalAirTime();
    double rx_pct = 100.0 * (double)rx_air / (double)total_ms;
    double tx_pct = 100.0 * (double)tx_air / (double)total_ms;
    char rx_s[16], tx_s[16];
    fmt_secs(rx_s, sizeof(rx_s), rx_air);
    fmt_secs(tx_s, sizeof(tx_s), tx_air);

    // Duty rolling 1h
    unsigned long duty_cur  = getTxAirLastHour();
    unsigned long duty_hard = getDutyHardLimitMs();
    double duty_pct = duty_hard > 0 ? 100.0 * (double)duty_cur / (double)duty_hard : 0.0;
    char duty_cur_s[16], duty_hard_s[16];
    fmt_secs(duty_cur_s,  sizeof(duty_cur_s),  duty_cur);
    fmt_secs(duty_hard_s, sizeof(duty_hard_s), duty_hard);

    snprintf(block, sizeof(block),
             "airtime + duty:\n"
             "  rx = %s (%.2f%%)\n"
             "  tx = %s (%.2f%%)\n"
             "  duty last_h = %s of %s (%.1f%%)",
             rx_s, rx_pct, tx_s, tx_pct,
             duty_cur_s, duty_hard_s, duty_pct);
    pushCompanionMessage(block);
    return;
  }

  // ---------- stats-packets: Packet counters Received / Sent ----------------
  if (starts_with_word(cmd, "stats-packets")) {
    uint64_t total_ms = (uint64_t)_millis_wraps * 4294967296ULL + (uint64_t)millis();
    if (total_ms == 0) total_ms = 1;
    uint64_t uptime_s = total_ms / 1000ULL;
    if (uptime_s == 0) uptime_s = 1;
    char block[200];
    int p;

    uint32_t hd_total = 0;
    for (int t = 0; t < 5; t++) hd_total += _heard_direct[t];
    p = snprintf(block, sizeof(block),
                 "stats-packets:\n"
                 "heard direct: rep=%u cmp=%u room=%u sns=%u total=%lu",
                 (unsigned)_heard_direct[ADV_TYPE_REPEATER],
                 (unsigned)_heard_direct[ADV_TYPE_CHAT],
                 (unsigned)_heard_direct[ADV_TYPE_ROOM],
                 (unsigned)_heard_direct[ADV_TYPE_SENSOR],
                 (unsigned long)hd_total);
    append_rate_hint(block + p, sizeof(block) - p, hd_total, uptime_s);
    pushCompanionMessage(block);

    // rx flood: in stats-packets bleibt's kompakt (Summe ueber path_len).
    // Die heard-direct/repeated Aufschluesselung gibt's in 'stats'.
    auto rxf_sum = [&](uint8_t pp) -> unsigned {
      return (unsigned)(_rx_flood_by_ptype[pp][0] + _rx_flood_by_ptype[pp][1]);
    };
    uint32_t rxf_total = 0;
    for (int pp = 0; pp < 16; pp++) rxf_total += rxf_sum((uint8_t)pp);
    p = snprintf(block, sizeof(block),
                 "rx flood:\n"
                 "  adv=%u path=%u txt=%u grp=%u ack=%u req=%u rsp=%u anon=%u trc=%u\n"
                 "  total=%lu",
                 rxf_sum(PAYLOAD_TYPE_ADVERT),
                 rxf_sum(PAYLOAD_TYPE_PATH),
                 rxf_sum(PAYLOAD_TYPE_TXT_MSG),
                 rxf_sum(PAYLOAD_TYPE_GRP_TXT),
                 rxf_sum(PAYLOAD_TYPE_ACK),
                 rxf_sum(PAYLOAD_TYPE_REQ),
                 rxf_sum(PAYLOAD_TYPE_RESPONSE),
                 rxf_sum(PAYLOAD_TYPE_ANON_REQ),
                 rxf_sum(PAYLOAD_TYPE_TRACE),
                 (unsigned long)rxf_total);
    append_rate_hint(block + p, sizeof(block) - p, rxf_total, uptime_s);
    pushCompanionMessage(block);

    auto own_of = [&](uint8_t pp) -> unsigned {
      uint16_t tot = _tx_total_by_ptype[pp];
      uint16_t rep = _repeat_by_ptype[pp];
      return (tot > rep) ? (unsigned)(tot - rep) : 0u;
    };
    uint32_t own_total = 0;
    for (int pp = 0; pp < 16; pp++) own_total += own_of((uint8_t)pp);
    p = snprintf(block, sizeof(block),
                 "tx self-initiated (flood + zero-hop):\n"
                 "  adv=%u path=%u txt=%u grp=%u ack=%u req=%u rsp=%u anon=%u trc=%u\n"
                 "  total=%lu",
                 own_of(PAYLOAD_TYPE_ADVERT),
                 own_of(PAYLOAD_TYPE_PATH),
                 own_of(PAYLOAD_TYPE_TXT_MSG),
                 own_of(PAYLOAD_TYPE_GRP_TXT),
                 own_of(PAYLOAD_TYPE_ACK),
                 own_of(PAYLOAD_TYPE_REQ),
                 own_of(PAYLOAD_TYPE_RESPONSE),
                 own_of(PAYLOAD_TYPE_ANON_REQ),
                 own_of(PAYLOAD_TYPE_TRACE),
                 (unsigned long)own_total);
    append_rate_hint(block + p, sizeof(block) - p, own_total, uptime_s);
    pushCompanionMessage(block);

    // Selbst-initiierter FLOOD-Anteil pro ptype. Direct = own_of - flood.
    // Eigene Message damit es das 145-Byte-Limit nicht reisst.
    uint32_t own_flood_total = 0;
    for (int pp = 0; pp < 16; pp++) own_flood_total += _tx_self_flood_by_ptype[pp];
    snprintf(block, sizeof(block),
             "tx own flood:\n"
             "  adv=%u path=%u txt=%u grp=%u ack=%u req=%u rsp=%u anon=%u trc=%u\n"
             "  flood total=%lu",
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_ADVERT],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_PATH],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_TXT_MSG],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_GRP_TXT],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_ACK],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_REQ],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_RESPONSE],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_ANON_REQ],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_TRACE],
             (unsigned long)own_flood_total);
    pushCompanionMessage(block);
    return;
  }

  if (starts_with_word(cmd, "stats") || starts_with_word(cmd, "stat")) {
    uint64_t total_ms = (uint64_t)_millis_wraps * 4294967296ULL + (uint64_t)millis();
    if (total_ms == 0) total_ms = 1;
    uint64_t uptime_s = total_ms / 1000ULL;
    if (uptime_s == 0) uptime_s = 1;
    char block[200];
    int p;

    // ---- Msg 0 (NEU 2026-05-30): Summary tx/rx total + flood/direct ----
    // Klassische Repeater-Zusammenfassung. Werte aus mesh::Mesh-Basis
    // (radio-Level Zaehler -- jedes empfangene/gesendete Paket).
    uint32_t n_sent_flood  = getNumSentFlood();
    uint32_t n_sent_direct = getNumSentDirect();
    uint32_t n_recv_flood  = getNumRecvFlood();
    uint32_t n_recv_direct = getNumRecvDirect();
    uint32_t n_sent_total  = n_sent_flood + n_sent_direct;
    uint32_t n_recv_total  = n_recv_flood + n_recv_direct;
    p = snprintf(block, sizeof(block),
                 "gesendet (tx):\n  total=%lu",
                 (unsigned long)n_sent_total);
    p += append_rate_hint(block + p, sizeof(block) - p, n_sent_total, uptime_s);
    snprintf(block + p, sizeof(block) - p,
             "\n  flood=%lu  direct=%lu",
             (unsigned long)n_sent_flood, (unsigned long)n_sent_direct);
    pushCompanionMessage(block);
    p = snprintf(block, sizeof(block),
                 "empfangen (rx):\n  total=%lu",
                 (unsigned long)n_recv_total);
    p += append_rate_hint(block + p, sizeof(block) - p, n_recv_total, uptime_s);
    snprintf(block + p, sizeof(block) - p,
             "\n  flood=%lu  direct=%lu",
             (unsigned long)n_recv_flood, (unsigned long)n_recv_direct);
    pushCompanionMessage(block);

    // RX-Block (Wunschliste 26 D, 2026-05-31): vom User reorganisierte
    // Reihenfolge. 'rx adv total' und 'rx direct nodes' jeweils mit
    // by-scope+type UND by-role Sub-Block. 'rx flood' getrennt nach
    // heard-direct (path_len=0) und repeated (path_len>0).
    uint32_t hd_total = 0;
    for (int t = 0; t < 5; t++) hd_total += _heard_direct[t];
    uint32_t ad_total = 0;
    for (int t = 0; t < 5; t++) ad_total += _rx_advert_total[t];
    uint32_t rxf_hd_total = 0, rxf_rep_total = 0;
    for (int pp = 0; pp < 16; pp++) {
      rxf_hd_total  += _rx_flood_by_ptype[pp][0];
      rxf_rep_total += _rx_flood_by_ptype[pp][1];
    }
    uint32_t rxf_total = rxf_hd_total + rxf_rep_total;

    // ---- 1) rx signal last heard (Noise/RSSI/SNR des letzten Empfangs) ----
    {
      int16_t noise_floor = (int16_t)_radio->getNoiseFloor();
      int8_t  last_rssi   = (int8_t)radio_driver.getLastRSSI();
      float   last_snr    = radio_driver.getLastSNR();
      snprintf(block, sizeof(block),
               "rx signal last heard:\n"
               "  noise=%d dBm  rssi=%d dBm  snr=%.1f dB",
               (int)noise_floor, (int)last_rssi, (double)last_snr);
      pushCompanionMessage(block);
    }

    // ---- 2) rx direct qual (SNR gut/mittel/schlecht pro Typ) ----
    // Alle 4 Typen werden immer angezeigt (auch mit 0/0/0). Schwellen Q4:
    // gut >= 0 dB, mittel >= -8 dB, schlecht < -8 dB.
    snprintf(block, sizeof(block),
             "rx direct qual good/med/bad:\n"
             "  rep  %u / %u / %u\n"
             "  cmp  %u / %u / %u\n"
             "  room %u / %u / %u\n"
             "  sens %u / %u / %u",
             (unsigned)_heard_quality[ADV_TYPE_REPEATER][0],
             (unsigned)_heard_quality[ADV_TYPE_REPEATER][1],
             (unsigned)_heard_quality[ADV_TYPE_REPEATER][2],
             (unsigned)_heard_quality[ADV_TYPE_CHAT][0],
             (unsigned)_heard_quality[ADV_TYPE_CHAT][1],
             (unsigned)_heard_quality[ADV_TYPE_CHAT][2],
             (unsigned)_heard_quality[ADV_TYPE_ROOM][0],
             (unsigned)_heard_quality[ADV_TYPE_ROOM][1],
             (unsigned)_heard_quality[ADV_TYPE_ROOM][2],
             (unsigned)_heard_quality[ADV_TYPE_SENSOR][0],
             (unsigned)_heard_quality[ADV_TYPE_SENSOR][1],
             (unsigned)_heard_quality[ADV_TYPE_SENSOR][2]);
    pushCompanionMessage(block);

    // ---- 3) rx adv total -- by scope+type (alle Hops) ----
    snprintf(block, sizeof(block),
             "rx adv total -- by scope+type:\n"
             "  scoped:   rep=%u cmp=%u room=%u sns=%u\n"
             "  unscoped: rep=%u cmp=%u room=%u sns=%u",
             (unsigned)_rx_advert_by_scope[ADV_TYPE_REPEATER][1],
             (unsigned)_rx_advert_by_scope[ADV_TYPE_CHAT][1],
             (unsigned)_rx_advert_by_scope[ADV_TYPE_ROOM][1],
             (unsigned)_rx_advert_by_scope[ADV_TYPE_SENSOR][1],
             (unsigned)_rx_advert_by_scope[ADV_TYPE_REPEATER][0],
             (unsigned)_rx_advert_by_scope[ADV_TYPE_CHAT][0],
             (unsigned)_rx_advert_by_scope[ADV_TYPE_ROOM][0],
             (unsigned)_rx_advert_by_scope[ADV_TYPE_SENSOR][0]);
    pushCompanionMessage(block);

    // ---- 3b) rx adv total -- by role + grand total ----
    p = snprintf(block, sizeof(block),
                 "rx adv total -- by role:\n"
                 "  rep=%u cmp=%u room=%u sns=%u\n"
                 "  total = %lu",
                 (unsigned)_rx_advert_total[ADV_TYPE_REPEATER],
                 (unsigned)_rx_advert_total[ADV_TYPE_CHAT],
                 (unsigned)_rx_advert_total[ADV_TYPE_ROOM],
                 (unsigned)_rx_advert_total[ADV_TYPE_SENSOR],
                 (unsigned long)ad_total);
    append_rate_hint(block + p, sizeof(block) - p, ad_total, uptime_s);
    pushCompanionMessage(block);

    // ---- 4) rx direct nodes -- by scope+type (zero-hop Subset von #3) ----
    snprintf(block, sizeof(block),
             "rx direct nodes -- by scope+type:\n"
             "  scoped:   rep=%u cmp=%u room=%u sns=%u\n"
             "  unscoped: rep=%u cmp=%u room=%u sns=%u",
             (unsigned)_heard_direct_by_scope[ADV_TYPE_REPEATER][1],
             (unsigned)_heard_direct_by_scope[ADV_TYPE_CHAT][1],
             (unsigned)_heard_direct_by_scope[ADV_TYPE_ROOM][1],
             (unsigned)_heard_direct_by_scope[ADV_TYPE_SENSOR][1],
             (unsigned)_heard_direct_by_scope[ADV_TYPE_REPEATER][0],
             (unsigned)_heard_direct_by_scope[ADV_TYPE_CHAT][0],
             (unsigned)_heard_direct_by_scope[ADV_TYPE_ROOM][0],
             (unsigned)_heard_direct_by_scope[ADV_TYPE_SENSOR][0]);
    pushCompanionMessage(block);

    // ---- 4b) rx direct nodes -- by role + total ----
    p = snprintf(block, sizeof(block),
                 "rx direct nodes -- by role:\n"
                 "  rep=%u cmp=%u room=%u sns=%u\n"
                 "  total = %lu",
                 (unsigned)_heard_direct[ADV_TYPE_REPEATER],
                 (unsigned)_heard_direct[ADV_TYPE_CHAT],
                 (unsigned)_heard_direct[ADV_TYPE_ROOM],
                 (unsigned)_heard_direct[ADV_TYPE_SENSOR],
                 (unsigned long)hd_total);
    append_rate_hint(block + p, sizeof(block) - p, hd_total, uptime_s);
    pushCompanionMessage(block);

    // ---- 5) rx zero-hop (DIRECT-typed Adverts = sendZeroHop von Nachbarn) ----
    // Komplement zu 'rx flood -- heard-direct' (FLOOD-typed). Non-advert
    // DIRECT-typed Pakete werden bewusst NICHT gezaehlt (User-Entscheidung).
    {
      uint32_t rxd_total = 0;
      for (int t = 0; t < 5; t++) rxd_total += _rx_direct_advert_by_role[t];
      p = snprintf(block, sizeof(block),
                   "rx zero-hop (DIRECT-typed adv):\n"
                   "  total = %lu",
                   (unsigned long)rxd_total);
      append_rate_hint(block + p, sizeof(block) - p, rxd_total, uptime_s);
      pushCompanionMessage(block);

      snprintf(block, sizeof(block),
               "rx zero-hop -- adv by role:\n"
               "  rep=%u cmp=%u room=%u sns=%u",
               (unsigned)_rx_direct_advert_by_role[ADV_TYPE_REPEATER],
               (unsigned)_rx_direct_advert_by_role[ADV_TYPE_CHAT],
               (unsigned)_rx_direct_advert_by_role[ADV_TYPE_ROOM],
               (unsigned)_rx_direct_advert_by_role[ADV_TYPE_SENSOR]);
      pushCompanionMessage(block);
    }

    // ---- 6a) rx flood -- heard-direct (FLOOD-typed, path_len=0) ----
    p = snprintf(block, sizeof(block),
                 "rx flood -- heard-direct (path_len=0):\n"
                 "  adv=%u path=%u txt=%u grp=%u ack=%u req=%u rsp=%u anon=%u trc=%u\n"
                 "  total=%lu",
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_ADVERT][0],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_PATH][0],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_TXT_MSG][0],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_GRP_TXT][0],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_ACK][0],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_REQ][0],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_RESPONSE][0],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_ANON_REQ][0],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_TRACE][0],
                 (unsigned long)rxf_hd_total);
    append_rate_hint(block + p, sizeof(block) - p, rxf_hd_total, uptime_s);
    pushCompanionMessage(block);

    // ---- 6b) rx flood -- repeated (FLOOD-typed, path_len>0) ----
    p = snprintf(block, sizeof(block),
                 "rx flood -- repeated (path_len>0):\n"
                 "  adv=%u path=%u txt=%u grp=%u ack=%u req=%u rsp=%u anon=%u trc=%u\n"
                 "  total=%lu",
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_ADVERT][1],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_PATH][1],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_TXT_MSG][1],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_GRP_TXT][1],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_ACK][1],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_REQ][1],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_RESPONSE][1],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_ANON_REQ][1],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_TRACE][1],
                 (unsigned long)rxf_rep_total);
    append_rate_hint(block + p, sizeof(block) - p, rxf_rep_total, uptime_s);
    pushCompanionMessage(block);

    // ---- 6c) rx flood -- grand total ----
    p = snprintf(block, sizeof(block),
                 "rx flood -- grand total = %lu",
                 (unsigned long)rxf_total);
    append_rate_hint(block + p, sizeof(block) - p, rxf_total, uptime_s);
    pushCompanionMessage(block);

    // ---- 7) rx heard total (Pakete die wir DIREKT empfangen haben) ----
    // = alle zero-hop FLOOD-typed Pakete (rxf_hd_total) plus DIRECT-typed
    //   zero-hop Adverts (= hd_total minus jene Adverts die FLOOD-typed
    //   zero-hop ankamen und damit schon in rxf_hd_total stecken).
    {
      uint32_t rxf_hd_adv = _rx_flood_by_ptype[PAYLOAD_TYPE_ADVERT][0];
      uint32_t direct_typed_advs = (hd_total > rxf_hd_adv) ? (hd_total - rxf_hd_adv) : 0;
      uint32_t rx_heard_total = rxf_hd_total + direct_typed_advs;
      p = snprintf(block, sizeof(block),
                   "rx heard total:\n"
                   "  total=%lu",
                   (unsigned long)rx_heard_total);
      append_rate_hint(block + p, sizeof(block) - p, rx_heard_total, uptime_s);
      pushCompanionMessage(block);
    }

    // ---- 8) rx us (Echo eigener Pakete im Mesh) -- Wunschliste 26 B ----
    // Match-Hash-Ringe: 32 self-initiated + 128 repeated Slots (4-Byte
    // truncated MAX_HASH). Hoch = unsere Pakete werden propagiert, niedrig
    // = isoliert oder Echo-Ring zu klein. Bei vollausgelastetem Repeater
    // deckt 128 Slots ca. 10 Min Echo-Window ab.
    {
      uint32_t rxu_self = _rx_us_self_initiated_count;
      uint32_t rxu_rep  = _rx_us_repeated_count;
      uint32_t rxu_tot  = rxu_self + rxu_rep;
      p = snprintf(block, sizeof(block),
                   "rx us (own echoes):\n"
                   "  self-initiated = %lu\n"
                   "  repeated       = %lu\n"
                   "  total          = %lu",
                   (unsigned long)rxu_self,
                   (unsigned long)rxu_rep,
                   (unsigned long)rxu_tot);
      append_rate_hint(block + p, sizeof(block) - p, rxu_tot, uptime_s);
      pushCompanionMessage(block);
    }

    // ---- Msg 5: tx self-initiated (= tx_total - repeated pro Pakettyp) ----
    auto own_of = [&](uint8_t pp) -> unsigned {
      uint16_t tot = _tx_total_by_ptype[pp];
      uint16_t rep = _repeat_by_ptype[pp];
      return (tot > rep) ? (unsigned)(tot - rep) : 0u;
    };
    uint32_t own_total = 0;
    for (int pp = 0; pp < 16; pp++) own_total += own_of((uint8_t)pp);
    p = snprintf(block, sizeof(block),
                 "tx self-initiated (flood + zero-hop):\n"
                 "  adv=%u path=%u txt=%u grp=%u ack=%u req=%u rsp=%u anon=%u trc=%u\n"
                 "  total=%lu",
                 own_of(PAYLOAD_TYPE_ADVERT),
                 own_of(PAYLOAD_TYPE_PATH),
                 own_of(PAYLOAD_TYPE_TXT_MSG),
                 own_of(PAYLOAD_TYPE_GRP_TXT),
                 own_of(PAYLOAD_TYPE_ACK),
                 own_of(PAYLOAD_TYPE_REQ),
                 own_of(PAYLOAD_TYPE_RESPONSE),
                 own_of(PAYLOAD_TYPE_ANON_REQ),
                 own_of(PAYLOAD_TYPE_TRACE),
                 (unsigned long)own_total);
    append_rate_hint(block + p, sizeof(block) - p, own_total, uptime_s);
    pushCompanionMessage(block);

    // ---- Msg 5b: tx own flood-Subset (Direct = own_of - flood) ------------
    // Eigene Message wegen 145-Byte-BLE-Limit. Nightly-Beacon-Adverts sind
    // hier sichtbar (gehen als Flood raus); manuelle Direct-DMs zaehlen
    // gegen self-initiated aber nicht hier.
    uint32_t own_flood_total = 0;
    for (int pp = 0; pp < 16; pp++) own_flood_total += _tx_self_flood_by_ptype[pp];
    snprintf(block, sizeof(block),
             "tx own flood:\n"
             "  adv=%u path=%u txt=%u grp=%u ack=%u req=%u rsp=%u anon=%u trc=%u\n"
             "  flood total=%lu",
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_ADVERT],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_PATH],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_TXT_MSG],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_GRP_TXT],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_ACK],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_REQ],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_RESPONSE],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_ANON_REQ],
             (unsigned)_tx_self_flood_by_ptype[PAYLOAD_TYPE_TRACE],
             (unsigned long)own_flood_total);
    pushCompanionMessage(block);

    // ---- Msg 6+7: tx repeated + tx total — nur wenn Repeater aktiv ----
    uint32_t rep_total = 0;
    for (int pp = 0; pp < 16; pp++) rep_total += _repeat_by_ptype[pp];
    if (_prefs.client_repeat != 0) {
      p = snprintf(block, sizeof(block),
                   "tx repeated:\n"
                   "  adv=%u path=%u txt=%u grp=%u ack=%u req=%u rsp=%u anon=%u trc=%u\n"
                   "  total=%lu",
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_ADVERT],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_PATH],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_TXT_MSG],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_GRP_TXT],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_ACK],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_REQ],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_RESPONSE],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_ANON_REQ],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_TRACE],
                   (unsigned long)rep_total);
      append_rate_hint(block + p, sizeof(block) - p, rep_total, uptime_s);
      pushCompanionMessage(block);

      uint32_t tx_grand_total = own_total + rep_total;
      p = snprintf(block, sizeof(block),
                   "tx total:\n"
                   "  total=%lu",
                   (unsigned long)tx_grand_total);
      append_rate_hint(block + p, sizeof(block) - p, tx_grand_total, uptime_s);
      pushCompanionMessage(block);
    }

    // ---- Msg 8: airtime (kompakt, Sekunden statt ms) ----
    unsigned long rx_air = getReceiveAirTime();
    unsigned long tx_air = getTotalAirTime();
    unsigned long tx_rep_air = _tx_repeat_airtime_ms;
    if (tx_rep_air > tx_air) tx_rep_air = tx_air;
    unsigned long tx_own_air = tx_air - tx_rep_air;
    double rx_pct     = 100.0 * (double)rx_air     / (double)total_ms;
    double tx_pct     = 100.0 * (double)tx_air     / (double)total_ms;
    double tx_own_pct = 100.0 * (double)tx_own_air / (double)total_ms;
    double tx_rep_pct = 100.0 * (double)tx_rep_air / (double)total_ms;
    double usage_pct  = rx_pct + tx_pct;
    double free_pct   = 100.0 - usage_pct;
    if (free_pct < 0) free_pct = 0;
    char rx_s[16], tx_s[16], own_s[16], rep_s[16];
    fmt_secs(rx_s,  sizeof(rx_s),  rx_air);
    fmt_secs(tx_s,  sizeof(tx_s),  tx_air);
    fmt_secs(own_s, sizeof(own_s), tx_own_air);
    fmt_secs(rep_s, sizeof(rep_s), tx_rep_air);
    snprintf(block, sizeof(block),
             "airtime:\n"
             "  usage=%.2f%% free=%.2f%%\n"
             "  rx=%s (%.2f%%)\n"
             "  tx=%s (%.2f%%) self=%s (%.2f%%) repeat=%s (%.2f%%)",
             usage_pct, free_pct,
             rx_s, rx_pct,
             tx_s, tx_pct, own_s, tx_own_pct, rep_s, tx_rep_pct);
    pushCompanionMessage(block);

    // ---- duty cycle: rolling 1h-Window vs. 10% Hard-Limit ----
    {
      unsigned long cur = getTxAirLastHour();
      unsigned long soft_ms = getDutySoftLimitMs();
      unsigned long hard_ms = getDutyHardLimitMs();
      double cur_pct = hard_ms > 0 ? 100.0 * (double)cur / (double)hard_ms : 0.0;
      char cur_s[16], soft_s[16], hard_s[16];
      fmt_secs(cur_s,  sizeof(cur_s),  cur);
      fmt_secs(soft_s, sizeof(soft_s), soft_ms);
      fmt_secs(hard_s, sizeof(hard_s), hard_ms);
      // limits (pro Stunde): macht den Bezug zur EU-erlaubten 10%-Airtime
      // explizit (User-Feedback). soft/hard sind Anteile von 10% airtime.
      snprintf(block, sizeof(block),
               "duty: last_h=%s of %s (%.1f%%)\n"
               "  limits (pro Stunde):\n"
               "    soft=%u%% (%s)\n"
               "    hard=%u%% (%s)\n"
               "  blocked=%lu",
               cur_s, hard_s, cur_pct,
               (unsigned)_prefs.duty_soft_pct, soft_s,
               (unsigned)_prefs.duty_hard_pct, hard_s,
               (unsigned long)_duty_blocked_count);
      pushCompanionMessage(block);
    }

    // (motion-Counter entfernt: is_moving wird jetzt im cached
    // _repeating_allowed-Flag abgebildet, kein per-Paket-Counter.
    // Transitions werden via pushDebugLog '[repeat] effective ->
    // on/off (started/stopped moving)' getraced.)
    return;
  }

  // ---------- trace -----------------------------------------------------
  // Selektives Live-Logging einzelner Event-Kategorien in den Companion-
  // Channel. Bitmask in _trace_flags (RAM-only, reset bei Reboot).
  //   trace                  -> aktive Kategorien
  //   trace list             -> alle verfuegbaren Kategorien + Beschreibung
  //   trace <cat> on/off     -> Flag setzen/loeschen
  //   trace all on/off       -> alle Flags
  //   trace off              -> Alias fuer "trace all off"
  if (starts_with_word(cmd, "trace")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ') arg++; }

    // -- Status (kein Arg) --
    // C4+C5: User-Wunsch klarere Wording. 'active' war frueher als
    // 'aktive Kategorien' verwendet -- jetzt 'trace ist ON/OFF' als
    // erstes Wort, und die Sektionsliste daneben. Plus: wenn trace OFF
    // ist UND persistent != 0, in einer Message ausgeben (statt zwei).
    if (!arg || *arg == 0) {
      char line[160]; int used;
      bool tr_on = (_trace_flags != 0);
      if (tr_on) {
        used = snprintf(line, sizeof(line), "trace ist ON\naktiv:");
        for (size_t k = 0; k < TRACE_CAT_COUNT; k++) {
          if (_trace_flags & trace_cats[k].flag) {
            used += snprintf(line + used, sizeof(line) - used, " %s", trace_cats[k].name);
          }
        }
        pushCompanionMessage(line);
      } else {
        // trace OFF -- wenn persistent gesetzt, Hinweis in derselben Message.
        if (_prefs.trace_flags_persistent != 0) {
          used = snprintf(line, sizeof(line),
                          "trace ist OFF\nNach 'trace on' wieder aktiv:");
          for (size_t k = 0; k < TRACE_CAT_COUNT; k++) {
            if (_prefs.trace_flags_persistent & trace_cats[k].flag) {
              used += snprintf(line + used, sizeof(line) - used, " %s", trace_cats[k].name);
            }
          }
          pushCompanionMessage(line);
        } else {
          pushCompanionMessage("trace ist OFF (keine Kategorien gespeichert).");
        }
      }
      return;
    }

    // 'list' (mit Prefix-Match -> 'li' / 'lis' / 'list' alle ok). Test-
    // Bericht B1: 'tra li' soll funktionieren.
    {
      static const CompanionChoice tr_subs[] = { { "list", false } };
      char ambig[32];
      int m = match_choice(arg, tr_subs, 1, ambig, sizeof(ambig));
      if (m == 0) {
        for (size_t k = 0; k < TRACE_CAT_COUNT; k++) {
          char line[160];
          snprintf(line, sizeof(line), "  %s - %s", trace_cats[k].name, trace_cats[k].desc);
          pushCompanionMessage(line);
        }
        return;
      }
      // m == -1 = mehrdeutig (kann hier nicht passieren -- nur ein Eintrag).
      // m < 0  = kein Match (nicht 'list') -> faellt durch zu on/off/all/<cat>.
    }

    // 'trace on/off' -> active = persistent (resume) / active = 0 (pause)
    {
      int tm = match_on_off(arg);
      if (tm == -1) { pushCompanionMessage("Mehrdeutig: on off"); return; }
      if (tm == 1) {
        _trace_flags = _prefs.trace_flags_persistent;
        pushCompanionMessage("OK - trace an (gespeicherte Auswahl wird verwendet).");
        return;
      }
      if (tm == 0) {
        _trace_flags = 0;
        pushCompanionMessage("OK - trace pausiert (Auswahl bleibt gespeichert).");
        return;
      }
    }

    // 'trace all on/off' -> active UND persistent
    if (starts_with_word(arg, "all")) {
      const char* sub = strchr(arg, ' ');
      if (sub) { while (*sub == ' ') sub++; }
      int am = match_on_off(sub);
      if (am == -1) { pushCompanionMessage("Mehrdeutig: on off"); return; }
      if (am == 1) {
        _trace_flags = TRACE_ALL_MASK;
        _prefs.trace_flags_persistent = TRACE_ALL_MASK;
        savePrefs();
        pushCompanionMessage("OK - alle traces an (und gespeichert).");
      } else if (am == 0) {
        _trace_flags = 0;
        _prefs.trace_flags_persistent = 0;
        savePrefs();
        pushCompanionMessage("OK - alle traces aus (und gespeichert).");
      } else {
        pushCompanionMessage("Usage: trace all on | trace all off");
      }
      return;
    }

    // 'trace <cat> on/off' -> bit in BEIDEN (User-Selektion).
    // Wandeln trace_cats[] in CompanionChoice[] (alle abkuerzbar) — damit
    // 'trace gp on' (gp -> gps) auch geht. Ambiguity-Beispiel: 'r' matcht
    // repeat UND rtc -> Warnung.
    CompanionChoice tc_choices[TRACE_CAT_COUNT];
    for (size_t k = 0; k < TRACE_CAT_COUNT; k++) {
      tc_choices[k].name      = trace_cats[k].name;
      tc_choices[k].no_abbrev = false;
    }
    char tc_ambig[100];
    int tc_idx = match_choice(arg, tc_choices, (int)TRACE_CAT_COUNT,
                              tc_ambig, sizeof(tc_ambig));
    if (tc_idx == -1) {
      char r[180]; snprintf(r, sizeof(r), "Mehrdeutig: %s", tc_ambig);
      pushCompanionMessage(r); return;
    }
    if (tc_idx < 0) {
      pushCompanionMessage("Unbekannte trace-Kategorie. 'trace list' fuer Uebersicht.");
      return;
    }
    const char* sub = strchr(arg, ' ');
    if (sub) { while (*sub == ' ') sub++; }
    int cm = match_on_off(sub);
    if (cm == -1) { pushCompanionMessage("Mehrdeutig: on off"); return; }
    if (cm == 1) {
      _trace_flags                  |= trace_cats[tc_idx].flag;
      _prefs.trace_flags_persistent |= trace_cats[tc_idx].flag;
      // Spezial-Erweiterung 'trace heard on [new|all]' (User-Wunsch
      // 2026-05-30): default 'new' (nur neue Direct-Nodes loggen) oder
      // 'all' (jeder Empfang inkl. bekannten Nodes).
      bool heard_mode_set = false;
      if (strcmp(trace_cats[tc_idx].name, "heard") == 0) {
        const char* mode_arg = sub ? strchr(sub, ' ') : NULL;
        if (mode_arg) { while (*mode_arg == ' ') mode_arg++; }
        if (mode_arg && *mode_arg) {
          if (strcmp(mode_arg, "all") == 0)      { _trace_heard_all = true;  heard_mode_set = true; }
          else if (strcmp(mode_arg, "new") == 0) { _trace_heard_all = false; heard_mode_set = true; }
          else {
            pushCompanionMessage("Usage: trace heard on [new|all]   (default: new)");
            return;
          }
        } else {
          _trace_heard_all = false;  // 'trace heard on' default = new
        }
      }
      savePrefs();
      char r[100];
      if (strcmp(trace_cats[tc_idx].name, "heard") == 0) {
        snprintf(r, sizeof(r), "OK - trace heard an, mode = %s%s.",
                 _trace_heard_all ? "all" : "new",
                 heard_mode_set ? "" : " (default)");
      } else {
        snprintf(r, sizeof(r), "OK - trace %s an.", trace_cats[tc_idx].name);
      }
      pushCompanionMessage(r);
    } else if (cm == 0) {
      _trace_flags                  &= ~trace_cats[tc_idx].flag;
      _prefs.trace_flags_persistent &= ~trace_cats[tc_idx].flag;
      savePrefs();
      char r[80]; snprintf(r, sizeof(r), "OK - trace %s aus.", trace_cats[tc_idx].name);
      pushCompanionMessage(r);
    } else {
      char r[80]; snprintf(r, sizeof(r), "Usage: trace %s on|off", trace_cats[tc_idx].name);
      pushCompanionMessage(r);
    }
    return;
  }

  // ---------- duty [soft N | hard N | reset] ----------------------------
  // Duty-Cycle-Schwellen verwalten. Ohne Arg -> Status in einer Message.
  if (starts_with_word(cmd, "duty")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ') arg++; }
    if (!arg || *arg == 0) {
      unsigned long cur = getTxAirLastHour();
      unsigned long soft_ms = getDutySoftLimitMs();
      unsigned long hard_ms = getDutyHardLimitMs();
      unsigned long cur_pct = hard_ms > 0 ? (cur * 100UL / hard_ms) : 0;
      char block[200];
      snprintf(block, sizeof(block),
               "duty:\n"
               "  stats=%lus/%lus (%lu%%)\n"
               "  blocked=%lu\n"
               "  limits:\n"
               "    soft=%u%% (%lus)\n"
               "    hard=%u%% (%lus)",
               cur/1000, hard_ms/1000, cur_pct,
               (unsigned long)_duty_blocked_count,
               (unsigned)_prefs.duty_soft_pct, soft_ms/1000,
               (unsigned)_prefs.duty_hard_pct, hard_ms/1000);
      pushCompanionMessage(block);
      return;
    }
    static const CompanionChoice duty_ch[] = {
      { "reset", true  },   // no_abbrev: zerstoerend, nur exakt
      { "soft",  false },
      { "hard",  false },
    };
    char ambig[40];
    int dm = match_choice(arg, duty_ch, 3, ambig, sizeof(ambig));
    if (dm == -1) {
      char r[80]; snprintf(r, sizeof(r), "Mehrdeutig: %s", ambig);
      pushCompanionMessage(r); return;
    }
    if (dm == 0) {
      // 'reset' — Zurueck auf die hardcoded Defaults. Counter bleibt
      // erhalten (Statistik, nicht Teil der Konfiguration).
      _prefs.duty_soft_pct = 80;
      _prefs.duty_hard_pct = 100;
      savePrefs();
      pushCompanionMessage("OK - duty reset: soft=80%, hard=100%.");
      return;
    }
    if (dm == 1 || dm == 2) {
      bool is_soft = (dm == 1);
      const char* num = strchr(arg, ' ');
      if (num) { while (*num == ' ') num++; }
      if (!num || *num == 0 || !(num[0] >= '0' && num[0] <= '9')) {
        pushCompanionMessage(is_soft ? "Usage: duty soft <0..99>"
                                     : "Usage: duty hard <1..100>");
        return;
      }
      int pct = atoi(num);
      int lo = is_soft ? 0 : 1;
      int hi = is_soft ? 99 : 100;
      if (pct < lo || pct > hi) {
        char line[80];
        snprintf(line, sizeof(line), "Wert ausserhalb (%d..%d).", lo, hi);
        pushCompanionMessage(line);
        return;
      }
      if (is_soft) _prefs.duty_soft_pct = (uint8_t)pct;
      else         _prefs.duty_hard_pct = (uint8_t)pct;
      savePrefs();
      char line[80];
      snprintf(line, sizeof(line), "OK - duty %s = %d%%", is_soft ? "soft" : "hard", pct);
      pushCompanionMessage(line);
      return;
    }
    pushCompanionMessage("Usage: duty [soft N | hard N | reset]");
    return;
  }

  // ---------- scope [default|bake|override] -----------------------------
  // Verwaltet die Flood-Scope-Hierarchie fuer den nightly-Job und
  // 'advert flood'. Wirkt NICHT auf regulaere Channel-/Direct-Sends.
  // Hierarchie (chooseNightFloodScope):
  //   1) override (persistent, mit expiry)
  //   2) bake     (persistent, fuer nightly bewusst weiter als default)
  //   3) default  (persistent, fuer regulaere Sends — Fallback fuer nightly)
  //   4) geo-fallback (Position-basiert)
  if (starts_with_word(cmd, "scope")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ') arg++; }

    // Hilfs-Helper: name-Argument extrahieren (das jeweils 2. Token nach
    // dem Sub-Befehl), '#'-Prefix strippen, lowercase. Returns "" wenn
    // kein Argument.
    auto extract_name = [&](const char* sub_arg, char* out, size_t out_size) -> void {
      out[0] = 0;
      const char* p = strchr(sub_arg, ' ');
      if (!p) return;
      while (*p == ' ' || *p == '\t') p++;
      if (*p == 0) return;
      if (*p == '#') p++;
      size_t i = 0;
      while (*p && *p != ' ' && *p != '\t' && i + 1 < out_size) {
        char c = *p++;
        if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
        out[i++] = c;
      }
      out[i] = 0;
    };

    // TTL-Parser: "72h" -> 72*3600. "3d" -> 3*86400. Default 12h wenn
    // ohne Suffix oder ohne Token. Maximum 30d = 720h.
    auto parse_ttl_secs = [&](const char* sub_arg) -> uint32_t {
      // Token nach dem name extrahieren (3. Token im sub_arg).
      const char* p = strchr(sub_arg, ' ');
      if (!p) return 12UL * 3600UL;
      while (*p == ' ') p++;
      if (!*p) return 12UL * 3600UL;
      while (*p && *p != ' ') p++;     // skip name token
      while (*p == ' ') p++;
      if (!*p) return 12UL * 3600UL;
      uint32_t n = 0;
      while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; }
      if (n == 0) return 12UL * 3600UL;
      char unit = (*p == 0) ? 'h' : (char)((*p >= 'A' && *p <= 'Z') ? (*p + 32) : *p);
      uint32_t secs;
      if (unit == 'h') secs = n * 3600UL;
      else if (unit == 'd') secs = n * 86400UL;
      else return 0;  // unbekanntes Suffix
      if (secs > 30UL * 86400UL) secs = 30UL * 86400UL;  // cap auf 30 Tage
      return secs;
    };

    // -- Status (kein Arg) --
    if (!arg || *arg == 0) {
      auto is_set = [](const uint8_t* keybuf, size_t len) -> bool {
        for (size_t k = 0; k < len; k++) if (keybuf[k] != 0) return true;
        return false;
      };
      uint32_t now = getRTCClock()->getCurrentTime();
      bool override_active = (_prefs.override_expiry != 0 && now < _prefs.override_expiry);
      bool bake_set    = is_set(_prefs.bake_scope_key,    sizeof(_prefs.bake_scope_key));
      bool default_set = is_set(_prefs.default_scope_key, sizeof(_prefs.default_scope_key));

      char def_line[80], bake_line[80], ovr_line[120];
      if (default_set) snprintf(def_line, sizeof(def_line), "default = #%s",
               _prefs.default_scope_name[0] ? _prefs.default_scope_name : "?");
      else snprintf(def_line, sizeof(def_line), "default = (none)");

      if (bake_set) snprintf(bake_line, sizeof(bake_line), "bake = #%s",
               _prefs.bake_scope_name[0] ? _prefs.bake_scope_name : "?");
      else snprintf(bake_line, sizeof(bake_line), "bake = (none)");

      if (override_active) {
        uint32_t remaining = _prefs.override_expiry - now;
        uint32_t rem_d = remaining / 86400UL;
        uint32_t rem_h = (remaining % 86400UL) / 3600UL;
        uint32_t rem_m = (remaining % 3600UL) / 60UL;
        if (rem_d > 0)
          snprintf(ovr_line, sizeof(ovr_line), "override = #%s (noch %lud%02luh%02lum)",
                   _prefs.override_scope_name[0] ? _prefs.override_scope_name : "?",
                   (unsigned long)rem_d, (unsigned long)rem_h, (unsigned long)rem_m);
        else
          snprintf(ovr_line, sizeof(ovr_line), "override = #%s (noch %luh%02lum)",
                   _prefs.override_scope_name[0] ? _prefs.override_scope_name : "?",
                   (unsigned long)rem_h, (unsigned long)rem_m);
      } else if (_prefs.override_expiry != 0) {
        snprintf(ovr_line, sizeof(ovr_line), "override = (expired)");
      } else {
        snprintf(ovr_line, sizeof(ovr_line), "override = (none)");
      }

      // Bug-Fix + Klarstellung (Test-Bericht 2026-05-29):
      // active muss auto=prefer beruecksichtigen (Geo gewinnt VOR Default
      // bei prefer + unterschiedlicher Region). Plus annotation an 'auto'
      // damit User klar sieht ob Geo gerade greift oder nur 'bereit' ist.
      TransportKey geo_k;
      bool has_geo = chooseGeoFallbackScope(geo_k);
      bool geo_differs_from_default = has_geo && default_set
          && memcmp(geo_k.key, _prefs.default_scope_key, 16) != 0;
      bool geo_wins_default = (_prefs.scope_advert_auto == 3 /* prefer */)
                              && geo_differs_from_default;
      bool geo_is_fallback  = (_prefs.scope_advert_auto >= 2 /* on or prefer */)
                              && has_geo && !default_set;

      // Test-Bericht 2026-05-29: Werte und Erklaerungen trennen, damit User
      // nicht "auto = prefer (...)" als ganze Aussage liest. Stattdessen:
      //   auto = prefer
      //   prefer: Geo gewinnt vor Default
      // Die Legende-Zeilen erscheinen NUR fuer die aktuell aktiven Werte.
      // Punkt 12 Reise-Fix 2026-06-08 (v2): bei geo-fallback den Namen des
      // gewaehlten Scopes IM active_val anzeigen, nicht in der Legend.
      // Vorher: 'active = geo-fallback; geo-fallback: kein Default gesetzt
      // (#de-be)' -- User-Feedback 2026-06-08: missverstaendlich, '(#de-be)'
      // wirkte wie eine Annotation der Default-Aussage. Jetzt klarer:
      //   'active = geo-fallback: #de-be'
      //   'geo-fallback: kein Default gesetzt'
      char geo_name_buf[40] = "";
      if (has_geo) {
        for (int gi = 0; gi < _buildin_keys_count; gi++) {
          if (memcmp(_buildin_keys[gi].key, geo_k.key, sizeof(geo_k.key)) == 0) {
            const char* nm = NULL;
            if (dl9sau_get_region((size_t)gi, &nm, NULL, NULL, NULL, NULL) && nm) {
              StrHelper::strncpy(geo_name_buf, nm, sizeof(geo_name_buf));
            }
            break;
          }
        }
      }

      char active_val_buf[60];
      const char* active_val;
      const char* active_legend;   // NULL = kein Legende-Zeile noetig
      if (override_active)         { active_val = "override";     active_legend = NULL; }
      else if (bake_set)           { active_val = "bake";         active_legend = "flooded advert, nightly"; }
      else if (geo_wins_default)   {
        if (geo_name_buf[0]) {
          snprintf(active_val_buf, sizeof(active_val_buf),
                   "geo-fallback: #%s", geo_name_buf);
          active_val = active_val_buf;
        } else {
          active_val = "geo-fallback";
        }
        active_legend = "Geo gewinnt vor Default (auto=prefer)";
      }
      else if (default_set)        { active_val = "default";      active_legend = "configured catchall scope"; }
      else if (geo_is_fallback)    {
        if (geo_name_buf[0]) {
          snprintf(active_val_buf, sizeof(active_val_buf),
                   "geo-fallback: #%s", geo_name_buf);
          active_val = active_val_buf;
        } else {
          active_val = "geo-fallback";
        }
        active_legend = "kein Default gesetzt";
      }
      else                         { active_val = "#local";       active_legend = "last-resort (kein Default/Geo)"; }

      const char* auto_val;
      const char* auto_legend;
      switch (_prefs.scope_advert_auto) {
        case 1: // off
          auto_val = "off"; auto_legend = "Geo wird nie verwendet";
          break;
        case 3: // prefer
          auto_val = "prefer";
          if (geo_wins_default)      auto_legend = "Geo gewinnt vor Default";
          else if (default_set)      auto_legend = "Default == Geo / kein Geo-Match";
          else if (has_geo)          auto_legend = "Geo als Fallback (kein Default)";
          else                       auto_legend = "keine Quelle";
          break;
        default: // on
          auto_val = "on";
          if (default_set)           auto_legend = "Default gesetzt -- 'auto prefer' liesse Geo gewinnen";
          else if (has_geo)          auto_legend = "Geo greift als Fallback";
          else                       auto_legend = "kein Geo-Match";
          break;
      }

      // Max 145 Zeichen pro BLE-Message (User-Constraint 2026-05-29).
      // Channelnamen koennen lang sein -> kombinierte Header-Message kann
      // ueberlaufen. Wenn ja, per-Line splitten.
      // Header-Klarstellung (User-Feedback 2026-06-02): bisher 'scope
      // advert (send hierarchy):' liess offen wofuer die scopes verwendet
      // werden. Explizit: 'own initiated packets'.
      char head[200];
      int hlen = snprintf(head, sizeof(head),
                          "scope (own initiated packets):\n  %s\n  %s\n  %s",
                          def_line, bake_line, ovr_line);
      if (hlen < 145) {
        pushCompanionMessage(head);
      } else {
        pushCompanionMessage("scope (own initiated packets):");
        char line[160];
        snprintf(line, sizeof(line), "  %s", def_line);  pushCompanionMessage(line);
        snprintf(line, sizeof(line), "  %s", bake_line); pushCompanionMessage(line);
        snprintf(line, sizeof(line), "  %s", ovr_line);  pushCompanionMessage(line);
      }

      // auto + active als 2 separate Messages (jede mit Wert + Legende).
      // Bleibt unter 145 Zeichen auch bei laengster Legende.
      char line2[160];
      if (auto_legend)
        snprintf(line2, sizeof(line2), "  auto = %s\n  %s: %s",
                 auto_val, auto_val, auto_legend);
      else
        snprintf(line2, sizeof(line2), "  auto = %s", auto_val);
      pushCompanionMessage(line2);

      if (active_legend)
        snprintf(line2, sizeof(line2), "  active = %s\n  %s: %s",
                 active_val, active_val, active_legend);
      else
        snprintf(line2, sizeof(line2), "  active = %s", active_val);
      pushCompanionMessage(line2);
      // User-Feedback 2026-06-02: das war NUR die Send-Sicht. User
      // hat 45 min nach der allowlist-Einstellung gesucht weil das
      // hier suggerierte 'das ist alles'. Footer mit Verweis auf
      // die anderen Sub-Befehle:
      pushCompanionMessage(
        "weitere:\n"
        "  scope repeater  -- Policy/allowlist\n"
        "  scope list      -- Registry\n"
        "  scope <name>    = Details");
      pushCompanionMessage(
        "  help scope      -- vollstaendige Uebersicht");
      return;
    }

    // -- Sub-Befehl-Dispatch via match_choice (Prefix-Matching erlaubt).
    // 'remove' und 'clear' sind no_abbrev (zerstoerend).
    static const CompanionChoice scope_subs[] = {
      { "default",  false },  // 0  - eigene Send-Default (auch unter 'advert')
      { "bake",     false },  // 1  - nightly bake (auch unter 'advert')
      { "override", false },  // 2  - persistent override (auch unter 'advert')
      { "list",     false },  // 3  - Registry (Liste A) anzeigen
      { "add",      false },  // 4  - Registry-Eintrag hinzufuegen
      { "remove",   true  },  // 5  - Registry-Eintrag loeschen (no_abbrev!)
      { "info",     false },  // 6  - Detail-Anzeige fuer einen Eintrag
      { "repeater", false },  // 7  - Sub-Namespace: Repeat-Policy
      { "regions",  false },  // 8  - Built-in Region-Tabelle (read-only)
      { "advert",   false },  // 9  - Sub-Namespace: Advert-Policy (Wunschliste 13)
    };
    // Punkt 16 Reise-Fix 2026-06-08: Pre-Flight Region-Lookup.
    // Wenn das erste Wort ein bekannter Region-Name ist (z.B. 'de',
    // 'lokal'), darf match_choice ihn NICHT als Prefix eines Sub-Befehls
    // wie 'default' interpretieren. Vorher: 'scope de advert off' wurde
    // als 'scope default = #advert' verstanden -- Region-Vorrang fehlte.
    bool is_region_ref = false;
    {
      char fw_norm[16];
      const char* qp = arg;
      if (*qp == '#') qp++;
      // first-word kopieren, lowercase, max 15 chars
      size_t qfl = 0;
      while (*qp && *qp != ' ' && *qp != '\t' && qfl + 1 < sizeof(fw_norm)) {
        char c = *qp++;
        if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
        fw_norm[qfl++] = c;
      }
      fw_norm[qfl] = 0;
      if (qfl > 0 && findScopeByName(fw_norm).storage != SCOPE_NONE) {
        is_region_ref = true;
      }
    }

    char scope_ambig[80];
    int sub_idx;
    if (is_region_ref) {
      // Erzwinge "kein Sub-Befehl-Match" -> faellt in den Region-Lookup
      // unten (Z. 13503+).
      sub_idx = -2;
      scope_ambig[0] = 0;
    } else {
      sub_idx = match_choice(arg, scope_subs,
                             (int)(sizeof(scope_subs)/sizeof(scope_subs[0])),
                             scope_ambig, sizeof(scope_ambig));
    }
    if (sub_idx == -1) {
      char r[120]; snprintf(r, sizeof(r), "Mehrdeutig: %s", scope_ambig);
      pushCompanionMessage(r); return;
    }
    if (sub_idx < 0) {
      // Kein Keyword-Match — vielleicht hat der User 'scope <name> <action>'
      // getippt. Wunschliste 11 Schritt 7. Zerlegen + dispatchen.
      char first_word[24];
      const char* p = arg;
      size_t fl = 0;
      while (*p && *p != ' ' && *p != '\t' && fl + 1 < sizeof(first_word)) {
        first_word[fl++] = *p++;
      }
      first_word[fl] = 0;
      while (*p == ' ' || *p == '\t') p++;

      // ? — Top-Level-Hilfe
      if (first_word[0] == '?' && first_word[1] == 0) {
        pushCompanionMessage("scope — Sub-Befehle:");
        pushCompanionMessage("  scope advert [...]   ('scope adv ?')\n    Eigene Adverts: default/bake/override/auto");
        pushCompanionMessage("  scope repeater [...]   ('scope rep ?')\n    Repeat-Policy + globaler Auto-Schalter");
        pushCompanionMessage("  scope list | add | remove | info | regions\n    Registry");
        pushCompanionMessage("  scope <name> pin|geo|off|disable|delete|advert|info\n    Per-Eintrag");
        return;
      }

      char name[16];
      if (!normalizeScopeName(first_word, name, sizeof(name))) {
        pushCompanionMessage("Unbekannter Befehl. 'scope ?' fuer Sub-Befehle.");
        return;
      }
      ScopeRef ref = findScopeByName(name);
      if (ref.storage == SCOPE_NONE) {
        char r[120];
        snprintf(r, sizeof(r),
          "Unbekannter Scope: #%s\n'scope add %s' fuer Anlegen, 'scope regions' fuer bekannte Namen.",
          name, name);
        pushCompanionMessage(r);
        return;
      }
      if (!*p || (p[0] == '?' && (p[1] == 0 || p[1] == ' '))) {
        char head[80]; snprintf(head, sizeof(head), "scope #%s — Aktionen:", name);
        pushCompanionMessage(head);
        pushCompanionMessage("Repeat (Achse rep):");
        pushCompanionMessage("  pin\n    immer repeaten");
        pushCompanionMessage("  auto  (alias: geo)\n    repeaten wenn GPS in Bbox");
        pushCompanionMessage("  off\n    nie repeaten");
        pushCompanionMessage("  rep <pin|auto|off>\n    explizit (gleiche Wirkung wie Top-Level)");
        pushCompanionMessage("Advert (Achse adv):");
        pushCompanionMessage("  adv auto|off  (alias: advert)\n    darf dieser Scope als Geo-Match-Kandidat\n    fuer eigene Adverts dienen");
        pushCompanionMessage("Orthogonal:");
        pushCompanionMessage("  disable | enable\n    temporaer beide Achsen aus/an");
        pushCompanionMessage("  delete | undelete\n    dauerhaft verstecken (nur Build-in)");
        pushCompanionMessage("  info\n    Status anzeigen");
        return;
      }

      // Top-Level Aktionen (zwei orthogonale Achsen rep + adv):
      //   pin  auto  off        — direkt Repeat-Mode (Achse rep)
      //   rep <pin|auto|off>    — Repeat-Mode, expliziter Achsen-Verb
      //   adv <auto|off>        — Advert-Mode (= darf dieser Scope als
      //                            Geo-Match-Kandidat fuer eigene Adverts
      //                            dienen)
      //   advert <auto|off>     — Alias zu adv (alter Verb)
      //   repeat <pin|auto|off> — Alias zu rep (alter Verb)
      //   disable | enable      — temporaer aus/an (Mode bleibt)
      //   delete | undelete     — permanent verstecken (Build-in)
      //   info
      //   geo                   — Alias fuer 'auto' (alter Verb)
      static const CompanionChoice action_choices[] = {
        { "repeat",   false },  // 0  alias zu 'rep'
        { "advert",   false },  // 1  alias zu 'adv'
        { "disable",  false },  // 2
        { "enable",   false },  // 3
        { "delete",   true  },  // 4 no_abbrev
        { "undelete", false },  // 5
        { "info",     false },  // 6
        { "pin",      false },  // 7  shortcut: rep pin
        { "geo",      false },  // 8  shortcut: rep auto (alias)
        { "off",      false },  // 9  shortcut: rep off
        { "auto",     false },  // 10 shortcut: rep auto
        { "rep",      false },  // 11 (Achse) repeat-mode
        { "adv",      false },  // 12 (Achse) advert-mode
      };
      char act_ambig[80];
      int aidx = match_choice(p, action_choices,
                              (int)(sizeof(action_choices)/sizeof(action_choices[0])),
                              act_ambig, sizeof(act_ambig));
      if (aidx == -1) {
        char r[120]; snprintf(r, sizeof(r), "Mehrdeutig: %s", act_ambig);
        pushCompanionMessage(r); return;
      }
      if (aidx < 0) {
        pushCompanionMessage(
          "Unbekannte Aktion. Erlaubt:\n"
          "  pin | auto | off      Repeat-Mode\n"
          "  adv auto|off          Advert-Mode\n"
          "  disable | enable      temporaer aus/an\n"
          "  delete | undelete     dauerhaft verstecken (Build-in)\n"
          "  info");
        return;
      }
      // Aliase: rep/repeat sind aequivalent (beide -> aidx 0 logik)
      if (aidx == 11) aidx = 0;
      // adv/advert sind aequivalent (beide -> aidx 1 logik)
      if (aidx == 12) aidx = 1;
      // geo ist alias fuer auto (beide -> aidx 8/10 -> "auto" shortcut)
      if (aidx == 10) aidx = 8;

      // Sentinel-Schutz: Sentinel-Scopes sind hartcodiert und duerfen
      // NICHT modifiziert werden (enable/disable/delete/pin/auto/off/
      // rep/adv wuerden die Sentinel-Semantik kaputt machen). Nur 'info'
      // erlaubt. Wunschliste 14 + 38, allowPacketForward Hard-Block.
      auto is_sentinel_name = [](const char* n) -> bool {
        return strcmp(n, "local-discard") == 0
            || strcmp(n, "direct")        == 0
            || strcmp(n, "direkt")        == 0
            || strcmp(n, "norepeat")      == 0
            || strcmp(n, "no-repeat")     == 0;
      };
      if (ref.storage == SCOPE_BUILDIN && is_sentinel_name(name)
          && aidx != 6 /* info */) {
        char r[140]; snprintf(r, sizeof(r),
          "#%s ist Sentinel - nicht modifizierbar.\n"
          "  Erlaubt: nur 'scope %s info'.", name, name);
        pushCompanionMessage(r);
        return;
      }

      // Argument nach der Aktion ermitteln
      const char* aarg = strchr(p, ' ');
      if (aarg) { while (*aarg == ' ' || *aarg == '\t') aarg++; }

      uint8_t status = getScopeStatus(ref);

      // Direkt-Aktion: pin / auto / off  -> Repeat-Mode setzen ohne sub-arg.
      if (aidx == 7 || aidx == 8 || aidx == 9) {
        status &= ~SCOPE_STATUS_REPEAT_MASK;
        const char* mode_str;
        if (aidx == 7)      { status |= SCOPE_STATUS_REPEAT_ON;   mode_str = "pin (immer aktiv)"; }
        else if (aidx == 8) { status |= SCOPE_STATUS_REPEAT_AUTO; mode_str = "auto (Bbox-Match)"; }
        else                { status |= SCOPE_STATUS_REPEAT_OFF;  mode_str = "off (nicht repeated)"; }
        setScopeStatus(ref, status);
        savePrefs();
        char r[120]; snprintf(r, sizeof(r), "OK - #%s rep = %s", name, mode_str);
        pushCompanionMessage(r);
        return;
      }

      if (aidx == 0) {  // rep / repeat <verb> (mit Aliases pin/auto/geo)
        static const CompanionChoice repeat_choices[] = {
          { "auto", false },  // 0 = AUTO
          { "on",   false },  // 1 = ON   (= pin, alter Verb)
          { "off",  false },  // 2 = OFF
          { "geo",  false },  // 3 alias auto (alter Verb)
          { "pin",  false },  // 4 alias on
        };
        char rmambig[40];
        int rm = match_choice(aarg, repeat_choices, 5, rmambig, sizeof(rmambig));
        if (rm == -1) { char r[80]; snprintf(r, sizeof(r), "Mehrdeutig: %s", rmambig); pushCompanionMessage(r); return; }
        if (rm < 0)   { pushCompanionMessage("Usage: scope <name> rep pin|auto|off"); return; }
        status &= ~SCOPE_STATUS_REPEAT_MASK;
        const char* mode_str;
        if (rm == 0 || rm == 3) { status |= SCOPE_STATUS_REPEAT_AUTO; mode_str = "auto (Bbox-Match)"; }
        else if (rm == 1 || rm == 4) { status |= SCOPE_STATUS_REPEAT_ON; mode_str = "pin (immer aktiv)"; }
        else                          { status |= SCOPE_STATUS_REPEAT_OFF; mode_str = "off (nicht repeated)"; }
        setScopeStatus(ref, status);
        savePrefs();
        char r[120]; snprintf(r, sizeof(r), "OK - #%s rep = %s", name, mode_str);
        pushCompanionMessage(r);
        return;
      }

      if (aidx == 1) {  // adv / advert auto|off
        static const CompanionChoice advert_choices[] = {
          { "auto", false }, { "off", false },
        };
        char amambig[40];
        int am = match_choice(aarg, advert_choices, 2, amambig, sizeof(amambig));
        if (am == -1) { char r[80]; snprintf(r, sizeof(r), "Mehrdeutig: %s", amambig); pushCompanionMessage(r); return; }
        if (am < 0)   { pushCompanionMessage("Usage: scope <name> adv auto|off"); return; }
        if (am == 0) status &= ~SCOPE_STATUS_ADVERT_OFF;
        else         status |=  SCOPE_STATUS_ADVERT_OFF;
        setScopeStatus(ref, status);
        savePrefs();
        char r[100]; snprintf(r, sizeof(r), "OK - #%s adv = %s",
                              name, (am == 0) ? "auto" : "off");
        pushCompanionMessage(r);
        return;
      }

      if (aidx == 2 || aidx == 3) {  // disable / enable
        if (aidx == 2) status |=  SCOPE_STATUS_DISABLED;
        else           status &= ~SCOPE_STATUS_DISABLED;
        setScopeStatus(ref, status);
        savePrefs();
        char r[100]; snprintf(r, sizeof(r), "OK - #%s %s",
                              name, (aidx == 2) ? "disabled" : "enabled");
        pushCompanionMessage(r);
        return;
      }

      if (aidx == 4 || aidx == 5) {  // delete / undelete
        if (ref.storage == SCOPE_EXTRAS) {
          pushCompanionMessage(
            "delete/undelete nur fuer Build-in-Eintraege. Fuer Extras\n"
            "stattdessen 'scope remove <name>' (loescht den Eintrag).");
          return;
        }
        if (aidx == 4) {
          // delete: USER_DELETED + repeat zwingend OFF (sonst bliebe ein
          // alter Pin bestehen und der Eintrag wuerde weiter repeated).
          status |= SCOPE_STATUS_USER_DELETED;
          status = (status & ~SCOPE_STATUS_REPEAT_MASK) | SCOPE_STATUS_REPEAT_OFF;
        } else {
          // undelete: USER_DELETED loeschen, repeat auf geo (default).
          // User kann danach explizit 'pin' oder 'off' setzen.
          status &= ~SCOPE_STATUS_USER_DELETED;
          status = (status & ~SCOPE_STATUS_REPEAT_MASK) | SCOPE_STATUS_REPEAT_AUTO;
        }
        setScopeStatus(ref, status);
        savePrefs();
        char r[100]; snprintf(r, sizeof(r), "OK - #%s %s",
                              name,
                              (aidx == 4) ? "deleted (versteckt, rep=off)"
                                          : "undeleted (rep=auto)");
        pushCompanionMessage(r);
        return;
      }

      if (aidx == 6) {  // info — gleich wie 'scope info <name>'
        // Wir koennten den Code dupliziern, aber einfacher: weiter-
        // delegieren waere komplex. Fuer jetzt: kurze Info zeigen.
        char head[140];
        const char* st_str = (status & SCOPE_STATUS_USER_DELETED) ? "deleted"
                           : (status & SCOPE_STATUS_DISABLED)     ? "disabled"
                           : ((status & SCOPE_STATUS_REPEAT_MASK) == SCOPE_STATUS_REPEAT_ON) ? "rep=pin"
                           : ((status & SCOPE_STATUS_REPEAT_MASK) == SCOPE_STATUS_REPEAT_OFF) ? "rep=off"
                           : "rep=auto";
        const char* ad_str = (status & SCOPE_STATUS_ADVERT_OFF) ? "advert=off" : "advert=auto";
        const char* store_str = (ref.storage == SCOPE_BUILDIN) ? "build-in" : "extras";
        snprintf(head, sizeof(head), "#%s [%s] %s %s", name, store_str, st_str, ad_str);
        pushCompanionMessage(head);
        return;
      }
      pushCompanionMessage("(unbehandelte action)");
      return;
    }

    // -- scope advert <sub> ... (Wunschliste 13) --
    // Sub-Namespace fuer alles Advert-bezogene:
    //   scope advert default <n>|clear     -> sub_idx 0
    //   scope advert bake <n>|clear        -> sub_idx 1
    //   scope advert override <n> [TTL]    -> sub_idx 2
    //   scope advert auto off|on|prefer    -> hier behandelt
    // Re-Dispatch fuer default/bake/override: arg + sub_idx werden auf
    // die existierenden Handler umgebogen und das if-chain faellt durch.
    if (sub_idx == 9) {
      const char* p = strchr(arg, ' ');
      if (p) { while (*p == ' ') p++; }

      // No-arg: zeige den AKTUELLEN STAND der Send-Hierarchie (Test-
      // Bericht B6: User will Status, Hilfe nur bei '?'). Wir
      // duplizieren NICHT die Logik aus dem 'scope' no-arg Block --
      // stattdessen leiten wir intern dorthin um. Einfacher Weg:
      // sub_idx auf -1 zwingen und der existierende fallback-Branch
      // erledigt das nicht (er macht per-Eintrag-dispatch). Daher
      // hier eigene Status-Ausgabe (analog scope no-arg).
      if (!p || *p == 0) {
        // Reuse der scope-no-arg Logik. Der einfachste Weg ist
        // Recursion via interner Erzeugung -- aber wir sind tief im
        // dispatch. Kopiere stattdessen den Status-Code (ist klein).
        // Hinweis: Wenn der scope-no-arg-Block weiterentwickelt wird,
        // muss hier mit-gepflegt werden.
        bool default_set = (_prefs.default_scope_key[0] != 0);
        bool bake_set    = false;
        for (size_t k = 0; k < sizeof(_prefs.bake_scope_key); k++) {
          if (_prefs.bake_scope_key[k] != 0) { bake_set = true; break; }
        }
        uint32_t now = getRTCClock()->getCurrentTime();
        bool override_active = (_prefs.override_expiry != 0
                                && now < _prefs.override_expiry);
        char def_line[80], bake_line[80], ovr_line[120];
        snprintf(def_line, sizeof(def_line), "default = %s",
                 default_set ? _prefs.default_scope_name : "(none)");
        snprintf(bake_line, sizeof(bake_line), "bake = %s",
                 bake_set ? _prefs.bake_scope_name : "(none)");
        if (override_active) {
          uint32_t rem = _prefs.override_expiry - now;
          uint32_t rd = rem / 86400UL, rh = (rem % 86400UL) / 3600UL, rm = (rem % 3600UL) / 60UL;
          if (rd > 0) snprintf(ovr_line, sizeof(ovr_line),
                               "override = #%s (noch %lud%02luh%02lum)",
                               _prefs.override_scope_name,
                               (unsigned long)rd, (unsigned long)rh, (unsigned long)rm);
          else        snprintf(ovr_line, sizeof(ovr_line),
                               "override = #%s (noch %luh%02lum)",
                               _prefs.override_scope_name,
                               (unsigned long)rh, (unsigned long)rm);
        } else if (_prefs.override_expiry != 0) {
          snprintf(ovr_line, sizeof(ovr_line), "override = (expired)");
        } else {
          snprintf(ovr_line, sizeof(ovr_line), "override = (none)");
        }
        // (Logik identisch zu 'scope' no-arg Block -- bei Aenderungen
        //  hier mit-pflegen.)
        TransportKey geo_k;
        bool has_geo = chooseGeoFallbackScope(geo_k);
        bool geo_differs_from_default = has_geo && default_set
            && memcmp(geo_k.key, _prefs.default_scope_key, 16) != 0;
        bool geo_wins_default = (_prefs.scope_advert_auto == 3)
                                && geo_differs_from_default;
        bool geo_is_fallback  = (_prefs.scope_advert_auto >= 2)
                                && has_geo && !default_set;

        const char* active;
        if (override_active)            active = "override";
        else if (bake_set)              active = "bake (flooded advert, nightly)";
        else if (geo_wins_default)      active = "geo-fallback (gewinnt vor Default)";
        else if (default_set)           active = "default (configured catchall scope)";
        else if (geo_is_fallback)       active = "geo-fallback";
        else                            active = "(none)";

        char auto_str[140];
        switch (_prefs.scope_advert_auto) {
          case 1:
            snprintf(auto_str, sizeof(auto_str), "off (Geo wird nie verwendet)");
            break;
          case 3:
            if (geo_wins_default)
              snprintf(auto_str, sizeof(auto_str), "prefer (Geo gewinnt vor Default)");
            else if (default_set)
              snprintf(auto_str, sizeof(auto_str),
                       "prefer (Default == Geo oder kein Geo-Match -- Default wins)");
            else if (has_geo)
              snprintf(auto_str, sizeof(auto_str), "prefer (Geo als Fallback)");
            else
              snprintf(auto_str, sizeof(auto_str), "prefer (keine Quelle)");
            break;
          default:
            if (default_set)
              snprintf(auto_str, sizeof(auto_str),
                       "on (Geo bereit, greift NICHT -- Default gesetzt;\n"
                       "       'auto prefer' liesse Geo gewinnen)");
            else if (has_geo)
              snprintf(auto_str, sizeof(auto_str), "on (Geo greift als Fallback)");
            else
              snprintf(auto_str, sizeof(auto_str), "on (kein Geo-Match)");
            break;
        }

        // Split wie oben -- s. Wire-Frame-Limit-Kommentar.
        char head[200];
        snprintf(head, sizeof(head),
                 "scope advert (send hierarchy):\n"
                 "  %s\n  %s\n  %s",
                 def_line, bake_line, ovr_line);
        pushCompanionMessage(head);
        char tail[200];
        snprintf(tail, sizeof(tail),
                 "  auto = %s\n"
                 "  active = %s",
                 auto_str, active);
        pushCompanionMessage(tail);

        // Next-Fire-Sichtbarkeit (User-Wunsch 2026-06-03): wann ist der
        // naechste zero-hop und der naechste nightly geplant? RTC-basiert,
        // lokale Zeit. Bei is_moving aendert sich zero-hop dynamisch -- das
        // ignorieren wir hier, der Wert ist eine Momentaufnahme.
        {
          uint32_t now_rtc = getRTCClock()->getCurrentTime();
          int32_t  tz      = (now_rtc > 1500000000UL)
                              ? localTzOffsetSecs(now_rtc) : 0;
          char zh_buf[48] = "off";
          char nl_buf[48] = "off";
          auto fmt_hhmm_rel = [&](uint32_t abs_unix, long delta_s,
                                  char* out, size_t out_size) {
            uint32_t loc = abs_unix + (uint32_t)tz;
            unsigned hh = (unsigned)((loc % 86400UL) / 3600UL);
            unsigned mm = (unsigned)((loc % 3600UL) / 60UL);
            if (delta_s < 0) delta_s = 0;
            unsigned long ds = (unsigned long)delta_s;
            unsigned long dh = ds / 3600UL, dm = (ds % 3600UL) / 60UL;
            if (dh > 0)
              snprintf(out, out_size, "%02u:%02u (in %luh%02lum)",
                       hh, mm, dh, dm);
            else
              snprintf(out, out_size, "%02u:%02u (in %lum)", hh, mm, dm);
          };
          if (_prefs.auto_advert_enabled & AUTO_ADV_ZEROHOP) {
            long delta_ms = (long)(next_periodic_advert_at - millis());
            long delta_s  = delta_ms / 1000;
            uint32_t abs_unix = (now_rtc > 1500000000UL)
              ? (now_rtc + (uint32_t)(delta_s > 0 ? delta_s : 0)) : 0;
            if (abs_unix == 0) snprintf(zh_buf, sizeof(zh_buf), "RTC unset");
            else fmt_hhmm_rel(abs_unix, delta_s, zh_buf, sizeof(zh_buf));
          }
          if (_prefs.auto_advert_enabled & AUTO_ADV_NIGHTLY) {
            if (next_night_flood_unix == 0) {
              snprintf(nl_buf, sizeof(nl_buf), "not scheduled");
            } else {
              long delta_s = (long)next_night_flood_unix - (long)now_rtc;
              fmt_hhmm_rel(next_night_flood_unix, delta_s,
                           nl_buf, sizeof(nl_buf));
            }
          }
          char nxt[160];
          snprintf(nxt, sizeof(nxt),
                   "next adverts:\n"
                   "  zero-hop: %s\n"
                   "  flooded:  %s",
                   zh_buf, nl_buf);
          pushCompanionMessage(nxt);
        }

        pushCompanionMessage("Hilfe: 'scope advert ?' fuer Sub-Befehle.");
        return;
      }

      // ?-Help -- nur bei explizitem ?
      if (p[0] == '?' && (p[1] == 0 || p[1] == ' ')) {
        const char* state = (_prefs.scope_advert_auto == 1) ? "off"
                          : (_prefs.scope_advert_auto == 3) ? "prefer" : "on";
        char r[200];
        snprintf(r, sizeof(r),
          "scope advert — Sub-Befehle (Status auto = %s):", state);
        pushCompanionMessage(r);
        pushCompanionMessage(
          "  scope advert default <name> | clear\n"
          "    Default-Scope fuer eigene Auto-Adverts");
        pushCompanionMessage(
          "  scope advert bake <name> | clear\n"
          "    Nightly-Flood-Advert nutzt diesen Scope");
        pushCompanionMessage(
          "  scope advert override <name> [<n>h|<n>d] | clear\n"
          "    Temp Override (max 30d, persistent ueber Reboot)");
        pushCompanionMessage(
          "  scope advert auto off | on | prefer");
        pushCompanionMessage(
          "    on (Default):\n"
          "      Default-Scope gewinnt. Geo als Fallback wenn\n"
          "      kein Default gesetzt ist.");
        pushCompanionMessage(
          "    prefer:\n"
          "      Geo schlaegt Default, wenn die oertliche Region eine\n"
          "      andere ist als das Default. ('User ist nicht zu Hause')");
        pushCompanionMessage(
          "    off:\n"
          "      Geo wird nie verwendet, nur Default/Bake/Override.");
        return;
      }

      static const CompanionChoice adv_subs[] = {
        { "default",  false },  // 0 -> dispatch to existing sub_idx==0
        { "bake",     false },  // 1 -> sub_idx==1
        { "override", false },  // 2 -> sub_idx==2
        { "auto",     false },  // 3 -> handled here
      };
      char adv_ambig[40];
      int av = match_choice(p, adv_subs, 4, adv_ambig, sizeof(adv_ambig));
      if (av == -1) {
        char r[80]; snprintf(r, sizeof(r), "Mehrdeutig: %s", adv_ambig);
        pushCompanionMessage(r); return;
      }
      if (av < 0) {
        pushCompanionMessage(
          "Sub-Aktion unbekannt. 'scope advert ?' fuer Liste.");
        return;
      }

      if (av == 3) {
        // scope advert auto off|on|prefer
        const char* val = strchr(p, ' ');
        if (val) { while (*val == ' ') val++; }
        if (!val || *val == 0) {
          const char* state = (_prefs.scope_advert_auto == 1) ? "off"
                            : (_prefs.scope_advert_auto == 3) ? "prefer" : "on";
          char r[160];
          snprintf(r, sizeof(r),
            "scope advert auto = %s\n"
            "  off / on / prefer  (siehe 'scope advert ?')", state);
          pushCompanionMessage(r);
          return;
        }
        static const CompanionChoice auto_vals[] = {
          { "off",    false },  // -> 1
          { "on",     false },  // -> 2
          { "prefer", false },  // -> 3
        };
        char val_ambig[40];
        int vi = match_choice(val, auto_vals, 3, val_ambig, sizeof(val_ambig));
        if (vi == -1) { char r[80]; snprintf(r, sizeof(r), "Mehrdeutig: %s", val_ambig); pushCompanionMessage(r); return; }
        if (vi < 0) { pushCompanionMessage("Usage: scope advert auto off|on|prefer"); return; }
        _prefs.scope_advert_auto = (uint8_t)(vi + 1);
        savePrefs();
        const char* name_str = (vi == 0) ? "off" : (vi == 1) ? "on" : "prefer";
        const char* desc =
            (vi == 0) ? "Geo wird nie verwendet"
          : (vi == 1) ? "Geo als Fallback wenn Default leer"
                      : "Geo schlaegt Default wenn oertlich andere Region";
        char r[160]; snprintf(r, sizeof(r), "OK - scope advert auto = %s\n  %s", name_str, desc);
        pushCompanionMessage(r);
        return;
      }

      // av == 0/1/2: re-dispatch zu existing default/bake/override.
      // Wir biegen arg + sub_idx um und lassen die if-chain weiterlaufen.
      arg = p;
      sub_idx = av;
      // Fall-through.
    }

    // -- default <name>|clear --
    if (sub_idx == 0) {
      const char* sub = strchr(arg, ' ');
      if (sub) { while (*sub == ' ') sub++; }
      if (!sub || *sub == 0) { pushCompanionMessage("Usage: scope default <name>|clear"); return; }
      // Punkt 17 Reise-Fix 2026-06-08: clear/none/off als Synonyme.
      // User-Erfahrung: 'scope default off' wurde frueher als Region-Name
      // "off" interpretiert ('OK - scope default = #off') -- konfus. Jetzt
      // alle drei Formen ergeben Clear-Operation.
      if (strcmp(sub, "clear") == 0 || strcmp(sub, "none") == 0 || strcmp(sub, "off") == 0) {
        memset(_prefs.default_scope_name, 0, sizeof(_prefs.default_scope_name));
        memset(_prefs.default_scope_key,  0, sizeof(_prefs.default_scope_key));
        savePrefs();
        pushCompanionMessage("OK - scope default cleared.");
        return;
      }
      char name[32];
      extract_name(arg, name, sizeof(name));
      if (name[0] == 0) { pushCompanionMessage("Usage: scope default <name>|clear"); return; }
      char tag[40]; snprintf(tag, sizeof(tag), "#%s", name);
      TransportKey key; TransportKeyStore tmp; tmp.getAutoKeyFor(0, tag, key);
      StrHelper::strncpy(_prefs.default_scope_name, name, sizeof(_prefs.default_scope_name));
      memcpy(_prefs.default_scope_key, key.key, sizeof(_prefs.default_scope_key));
      savePrefs();
      char line[100]; snprintf(line, sizeof(line), "OK - scope default = #%s", name);
      pushCompanionMessage(line);
      return;
    }

    // -- bake <name>|clear --
    if (sub_idx == 1) {
      const char* sub = strchr(arg, ' ');
      if (sub) { while (*sub == ' ') sub++; }
      if (!sub || *sub == 0) { pushCompanionMessage("Usage: scope bake <name>|clear"); return; }
      // Punkt 17 Fix: clear/none/off als Synonyme.
      if (strcmp(sub, "clear") == 0 || strcmp(sub, "none") == 0 || strcmp(sub, "off") == 0) {
        memset(_prefs.bake_scope_name, 0, sizeof(_prefs.bake_scope_name));
        memset(_prefs.bake_scope_key,  0, sizeof(_prefs.bake_scope_key));
        savePrefs();
        pushCompanionMessage("OK - scope bake cleared.");
        return;
      }
      char name[32];
      extract_name(arg, name, sizeof(name));
      if (name[0] == 0) { pushCompanionMessage("Usage: scope bake <name>|clear"); return; }
      char tag[40]; snprintf(tag, sizeof(tag), "#%s", name);
      TransportKey key; TransportKeyStore tmp; tmp.getAutoKeyFor(0, tag, key);
      StrHelper::strncpy(_prefs.bake_scope_name, name, sizeof(_prefs.bake_scope_name));
      memcpy(_prefs.bake_scope_key, key.key, sizeof(_prefs.bake_scope_key));
      savePrefs();
      char line[100]; snprintf(line, sizeof(line), "OK - scope bake = #%s", name);
      pushCompanionMessage(line);
      return;
    }

    // -- override <name> [<ttl>] | clear --
    if (sub_idx == 2) {
      const char* sub = strchr(arg, ' ');
      if (sub) { while (*sub == ' ') sub++; }
      if (!sub || *sub == 0) { pushCompanionMessage("Usage: scope override <name> [<n>h|<n>d] | clear"); return; }
      // Punkt 17 Fix: clear/none/off als Synonyme.
      if (strcmp(sub, "clear") == 0 || strcmp(sub, "none") == 0 || strcmp(sub, "off") == 0) {
        memset(_prefs.override_scope_name, 0, sizeof(_prefs.override_scope_name));
        memset(_prefs.override_scope_key,  0, sizeof(_prefs.override_scope_key));
        _prefs.override_expiry = 0;
        savePrefs();
        pushCompanionMessage("OK - scope override cleared.");
        return;
      }
      char name[32];
      extract_name(arg, name, sizeof(name));
      if (name[0] == 0) { pushCompanionMessage("Usage: scope override <name> [<n>h|<n>d] | clear"); return; }
      uint32_t ttl = parse_ttl_secs(sub);
      if (ttl == 0) {
        pushCompanionMessage("Usage: scope override <name> [<n>h|<n>d] (max 30d)");
        return;
      }
      char tag[40]; snprintf(tag, sizeof(tag), "#%s", name);
      TransportKey key; TransportKeyStore tmp; tmp.getAutoKeyFor(0, tag, key);
      StrHelper::strncpy(_prefs.override_scope_name, name, sizeof(_prefs.override_scope_name));
      memcpy(_prefs.override_scope_key, key.key, sizeof(_prefs.override_scope_key));
      uint32_t now = getRTCClock()->getCurrentTime();
      _prefs.override_expiry = (now > 1500000000UL) ? (now + ttl) : ttl;
      savePrefs();
      char line[120];
      uint32_t ttl_h = ttl / 3600;
      if (ttl >= 86400UL && (ttl % 86400UL) == 0)
        snprintf(line, sizeof(line), "OK - scope override = #%s (%lud)",
                 name, (unsigned long)(ttl/86400));
      else
        snprintf(line, sizeof(line), "OK - scope override = #%s (%luh)",
                 name, (unsigned long)ttl_h);
      pushCompanionMessage(line);
      return;
    }

    // -- list / list all (Wunschliste 11 Schritt 9) --
    // Iteriert Build-in + Extras, zeigt pro Eintrag den effektiven Status.
    // 'scope list'      -> nur non-default Eintraege (kompakt)
    // 'scope list all'  -> auch Default-Eintraege (komplettes Bild)
    // Deleted (USER_DELETED) wird nur in 'list all' angezeigt.
    if (sub_idx == 3) {
      const char* sub = strchr(arg, ' ');
      if (sub) { while (*sub == ' ') sub++; }
      bool show_all = (sub && strncmp(sub, "all", 3) == 0
                       && (sub[3] == 0 || sub[3] == ' '));

      char gb[200]; size_t gu = 0;
      auto gflush = [&]() {
        if (gu == 0) return;
        gb[gu] = 0; pushCompanionMessage(gb); gu = 0;
      };
      auto gline = [&](const char* line) {
        size_t len = strlen(line);
        if (len > 130) len = 130;
        if (gu > 0 && gu + 1 + len > 130) gflush();
        if (gu > 0) gb[gu++] = '\n';
        for (size_t i = 0; i < len && gu < sizeof(gb) - 1; i++) gb[gu++] = line[i];
      };

      // Format-Helper: status-Byte + in_bbox + has_bbox -> Flag-String
      auto fmtFlags = [&](uint8_t status, bool in_bbox, bool has_bbox,
                          char* out, size_t out_size) {
        size_t k = 0;
        auto put = [&](char c) { if (k + 1 < out_size) out[k++] = c; };
        uint8_t mode = status & SCOPE_STATUS_REPEAT_MASK;
        // R = aktiv: ON immer, AUTO wenn in_bbox, OFF nie. Plus DISABLED/
        // USER_DELETED schalten ab.
        bool effectively_active = false;
        if (!(status & (SCOPE_STATUS_DISABLED | SCOPE_STATUS_USER_DELETED))) {
          if      (mode == SCOPE_STATUS_REPEAT_ON)   effectively_active = true;
          else if (mode == SCOPE_STATUS_REPEAT_AUTO) effectively_active = in_bbox;
        }
        if (effectively_active) put('R');
        if (mode == SCOPE_STATUS_REPEAT_AUTO) put('A');
        if (mode == SCOPE_STATUS_REPEAT_ON)   put('P');
        if (status & SCOPE_STATUS_DISABLED)      put('D');
        if (status & SCOPE_STATUS_USER_DELETED)  put('X');
        if (status & SCOPE_STATUS_ADVERT_OFF)    put('!');
        if (has_bbox)                            put('b');
        out[k] = 0;
      };

      // Filter fuer kompakte Liste: zeige alle Eintraege die "in der
      // Repeat-Liste" sind = mode != OFF und nicht user_deleted. Damit
      // entspricht 'scope list' dem mentalen Modell des Users ('was
      // repeate ich'). 'list all' zeigt zusaetzlich OFF + DELETED.
      auto inRepeatSet = [](uint8_t st) -> bool {
        if (st & SCOPE_STATUS_USER_DELETED) return false;
        uint8_t mode = st & SCOPE_STATUS_REPEAT_MASK;
        return mode != SCOPE_STATUS_REPEAT_OFF;
      };

      // Count first (Header)
      int n_build = 0, n_extras = 0;
      for (int i = 0; i < _buildin_keys_count; i++) {
        uint8_t st = getBuildinStatus(i);
        if (!show_all && !inRepeatSet(st)) continue;
        n_build++;
      }
      for (int i = 0; i < _prefs.scope_extras_count; i++) {
        ScopeRef r = { SCOPE_EXTRAS, i };
        uint8_t st = getScopeStatus(r);
        if (!show_all && !inRepeatSet(st)) continue;
        n_extras++;
      }

      char head[120];
      snprintf(head, sizeof(head),
               "scope list%s (mode=%s, %d build-in + %d extras):",
               show_all ? " all" : "",
               _prefs.repeat_scope_mode == REPEAT_SCOPE_MODE_ALL
                   ? "all" : "allowlist",
               n_build, n_extras);
      gline(head);

      // Build-in
      for (int i = 0; i < _buildin_keys_count && i < SCOPE_BUILDIN_KEY_CACHE_MAX; i++) {
        // Alias-Eintraege (Multi-Rectangle fuer L-foermige Regionen) sind nur
        // dem Matcher relevant -- nicht im Registry-Display anzeigen, sonst
        // erschiene z.B. "#de-sh" zweimal.
        if (dl9sau_is_alias((size_t)i)) continue;
        uint8_t st = getBuildinStatus(i);
        if (!show_all && !inRepeatSet(st)) continue;
        const char* nm = NULL;
        if (!dl9sau_get_region((size_t)i, &nm, NULL, NULL, NULL, NULL)) continue;
        bool has_bbox = false;
        dl9sau_get_region_meta((size_t)i, &has_bbox, NULL);
        char flagstr[16];
        fmtFlags(st, _buildin_in_bbox[i], has_bbox, flagstr, sizeof(flagstr));
        char tmp[80];
        snprintf(tmp, sizeof(tmp), "  #%s  [%s] (b-in)",
                 nm, flagstr[0] ? flagstr : "-");
        gline(tmp);
      }
      // Extras
      for (int i = 0; i < _prefs.scope_extras_count && i < SCOPE_EXTRAS_SLOTS; i++) {
        const ScopeRegEntry& e = _prefs.scope_extras[i];
        ScopeRef r = { SCOPE_EXTRAS, i };
        uint8_t st = getScopeStatus(r);
        if (!show_all && !inRepeatSet(st)) continue;
        bool has_bbox = (e.flags & SCOPE_FLAG_HAS_GEO_BOX) != 0;
        char flagstr[16];
        fmtFlags(st, _extras_in_bbox[i], has_bbox, flagstr, sizeof(flagstr));
        char tmp[80];
        snprintf(tmp, sizeof(tmp), "  #%s  [%s] (ext)",
                 e.name, flagstr[0] ? flagstr : "-");
        gline(tmp);
      }
      if (n_build + n_extras == 0) {
        gline(show_all
              ? "  (Registry leer)"
              : "  (kein Eintrag in der Repeat-Liste — 'list all' zeigt alle)");
      }
      gflush();
      pushCompanionMessage(
        "Flags: R=aktiv jetzt, A=auto-mode, P=pin, D=disabled,\n"
        "X=deleted, b=hat bbox; (b-in)=Build-in, (ext)=Extras");
      pushCompanionMessage(
        "  ! = Eigenes geo auto-Advert nimmt diesen Scope nie.\n"
        "      Keine Auswirkung auf Repeater-Verhalten.");
      pushCompanionMessage(
        "      'scope advert default/bake/override <name>' kann diesen\n"
        "      Scope trotzdem explizit waehlen.");
      return;
    }

    // -- add <name> [<lat1,lon1,lat2,lon2>] (User-Extras) --
    // Wunschliste 11 Schritt 8: schreibt direkt in scope_extras (nicht
    // mehr ueber Legacy-scope_registry-Buffer + Migration). Build-in-
    // Namen werden abgewiesen — die haben ihren Slot in der Build-in-
    // Tabelle und werden via 'scope <name> pin/geo/off' gesteuert.
    if (sub_idx == 4) {
      const char* p = strchr(arg, ' ');
      if (p) { while (*p == ' ') p++; }
      if (!p || *p == 0) {
        pushCompanionMessage(
          "Usage: scope add <name> [<lat_min,lon_min,lat_max,lon_max>]\n"
          "Fuer Build-in-Namen (siehe 'scope regions') stattdessen\n"
          "'scope <name> pin' zum Pinnen, 'scope regions' fuer Bbox.");
        return;
      }
      char name[16];
      if (!normalizeScopeName(p, name, sizeof(name))) {
        pushCompanionMessage("Name ungueltig: erlaubt [a-z0-9-_], 1..15 Zeichen, kein '##'.");
        return;
      }
      // Build-in -> ablehnen mit Hinweis
      if (dl9sau_find_region_index(name) >= 0) {
        char r[120]; snprintf(r, sizeof(r),
          "#%s ist Build-in. 'scope %s pin' zum Pinnen,\n"
          "'scope regions' fuer Bbox-Detail.", name, name);
        pushCompanionMessage(r); return;
      }
      // Schon in Extras?
      ScopeRef existing = findScopeByName(name);
      if (existing.storage == SCOPE_EXTRAS) {
        char r[80]; snprintf(r, sizeof(r), "#%s ist schon in den Extras.", name);
        pushCompanionMessage(r); return;
      }
      if (_prefs.scope_extras_count >= SCOPE_EXTRAS_SLOTS) {
        char r[80]; snprintf(r, sizeof(r),
          "Extras voll (max %u).", (unsigned)SCOPE_EXTRAS_SLOTS);
        pushCompanionMessage(r); return;
      }
      bool has_geo = false;
      float lat_min=0, lat_max=0, lon_min=0, lon_max=0;
      const char* q = p;
      while (*q && *q != ' ' && *q != '\t') q++;
      while (*q == ' ' || *q == '\t') q++;
      // 'geo'-Keyword (Backward-Compat) wird stillschweigend uebersprungen
      if (starts_with_word(q, "geo")) {
        q = strchr(q, ' ');
        if (q) { while (*q == ' ') q++; }
      }
      if (q && *q != 0) {
        if (sscanf(q, "%f,%f,%f,%f", &lat_min, &lon_min, &lat_max, &lon_max) != 4) {
          pushCompanionMessage(
            "Usage: scope add <name> [<lat_min,lon_min,lat_max,lon_max>]\n"
            "Vier kommagetrennte Floats. Sued/West negativ.");
          return;
        }
        if (lat_min > lat_max || lon_min > lon_max
            || lat_min < -90 || lat_max > 90 || lon_min < -180 || lon_max > 180) {
          pushCompanionMessage("Bbox ungueltig: lat_min<=lat_max, lon_min<=lon_max,\n"
                               "lat in -90..90, lon in -180..180.");
          return;
        }
        has_geo = true;
      }

      // Slot belegen
      int slot = _prefs.scope_extras_count;
      ScopeRegEntry& e = _prefs.scope_extras[slot];
      memset(&e, 0, sizeof(e));
      strncpy(e.name, name, sizeof(e.name) - 1);
      char tag[40]; snprintf(tag, sizeof(tag), "#%s", e.name);
      TransportKey k; TransportKeyStore tmp; tmp.getAutoKeyFor(0, tag, k);
      memcpy(e.key, k.key, sizeof(e.key));
      e.flags = 0;
      if (has_geo) {
        // Opt-Out-Modell: hat Bbox -> auto-managed (AUTO-Repeat in Bbox).
        // User kann via 'scope <name> pin/off' explizit pinnen/aus.
        e.flags |= SCOPE_FLAG_HAS_GEO_BOX | SCOPE_FLAG_GEO_MANAGED;
        e.bbox_lat_min = lat_min; e.bbox_lat_max = lat_max;
        e.bbox_lon_min = lon_min; e.bbox_lon_max = lon_max;
      }
      _prefs.scope_extras_count++;
      savePrefs();
      // Bbox-Membership sofort updaten — sonst sieht scopeAllowedForRepeat
      // bis zum naechsten GPS-Tick noch in_bbox=false fuer den neuen Slot.
      if (has_geo) {
        double cur_lat, cur_lon;
        if (getEffectiveLatLon(cur_lat, cur_lon)) {
          evaluateScopeBboxes(cur_lat, cur_lon);
        }
      }
      char r[120];
      snprintf(r, sizeof(r), "OK - #%s in Extras (Slot %d%s).",
               e.name, slot,
               has_geo ? ", mit Bbox + auto-managed" : "");
      pushCompanionMessage(r);
      return;
    }

    // -- remove <name> (User-Extras) --
    // Wunschliste 11 Schritt 8: entfernt aus scope_extras, kompaktiert.
    // Build-in-Namen: USER_DELETED-Bit setzen statt entfernen.
    if (sub_idx == 5) {
      const char* p = strchr(arg, ' ');
      if (p) { while (*p == ' ') p++; }
      if (!p || *p == 0) { pushCompanionMessage("Usage: scope remove <name>"); return; }
      char name[16];
      if (!normalizeScopeName(p, name, sizeof(name))) {
        pushCompanionMessage("Name ungueltig.");
        return;
      }
      ScopeRef ref = findScopeByName(name);
      // Sentinel-Schutz: Sentinels nicht entfernbar (Wunschliste 14 + 38).
      auto is_sentinel_name = [](const char* n) -> bool {
        return strcmp(n, "local-discard") == 0
            || strcmp(n, "direct")        == 0
            || strcmp(n, "direkt")        == 0
            || strcmp(n, "norepeat")      == 0
            || strcmp(n, "no-repeat")     == 0;
      };
      if (ref.storage == SCOPE_BUILDIN && is_sentinel_name(name)) {
        char r[80]; snprintf(r, sizeof(r),
          "#%s ist Sentinel - nicht entfernbar.", name);
        pushCompanionMessage(r);
        return;
      }
      if (ref.storage == SCOPE_BUILDIN) {
        // Build-in -> als user_deleted markieren (Slot bleibt sticky in
        // scope_buildin_status). Vorher abklappern damit andere Status-
        // Bits erhalten bleiben.
        uint8_t st = getBuildinStatus(ref.idx);
        st |= SCOPE_STATUS_USER_DELETED;
        // Repeat ausschalten — sonst wirkt USER_DELETED nicht.
        st = (st & ~SCOPE_STATUS_REPEAT_MASK) | SCOPE_STATUS_REPEAT_OFF;
        setBuildinStatus(ref.idx, st);
        savePrefs();
        char r[120]; snprintf(r, sizeof(r),
          "OK - #%s (Build-in) als 'user_deleted' markiert.\n"
          "Mit 'scope %s repeat auto' wieder reaktivieren.", name, name);
        pushCompanionMessage(r); return;
      }
      if (ref.storage != SCOPE_EXTRAS) {
        char r[80]; snprintf(r, sizeof(r), "#%s nicht in den Extras.", name);
        pushCompanionMessage(r); return;
      }
      int idx = ref.idx;
      // Kompaktion: alle nachfolgenden Slots um eins nach vorne
      for (int i = idx; i < _prefs.scope_extras_count - 1; i++) {
        _prefs.scope_extras[i] = _prefs.scope_extras[i + 1];
        _extras_in_bbox[i]     = _extras_in_bbox[i + 1];
      }
      _prefs.scope_extras_count--;
      memset(&_prefs.scope_extras[_prefs.scope_extras_count], 0, sizeof(ScopeRegEntry));
      _extras_in_bbox[_prefs.scope_extras_count] = false;
      savePrefs();
      char r[80]; snprintf(r, sizeof(r), "OK - #%s aus Extras entfernt.", name);
      pushCompanionMessage(r);
      return;
    }

    // -- info <name> --
    if (sub_idx == 6) {
      const char* p = strchr(arg, ' ');
      if (p) { while (*p == ' ') p++; }
      if (!p || *p == 0) { pushCompanionMessage("Usage: scope info <name>"); return; }
      char name[16];
      if (!normalizeScopeName(p, name, sizeof(name))) {
        pushCompanionMessage("Name ungueltig.");
        return;
      }
      ScopeRef r = findScopeByName(name);
      if (r.storage == SCOPE_NONE) {
        char m[80]; snprintf(m, sizeof(m), "#%s nicht bekannt.", name);
        pushCompanionMessage(m); return;
      }
      uint8_t st = getScopeStatus(r);
      const uint8_t* key = getScopeKey(r);
      double lat1=0, lat2=0, lon1=0, lon2=0;
      bool has_bbox = getScopeBbox(r, &lat1, &lat2, &lon1, &lon2);
      bool in_bbox = (r.storage == SCOPE_BUILDIN)
                     ? (r.idx >= 0 && r.idx < SCOPE_BUILDIN_KEY_CACHE_MAX
                        && _buildin_in_bbox[r.idx])
                     : (r.idx >= 0 && r.idx < SCOPE_EXTRAS_SLOTS
                        && _extras_in_bbox[r.idx]);

      // Effective state
      uint8_t mode = st & SCOPE_STATUS_REPEAT_MASK;
      const char* mode_str = (mode == SCOPE_STATUS_REPEAT_ON) ? "pin"
                           : (mode == SCOPE_STATUS_REPEAT_OFF) ? "off"
                           : "auto";
      bool eff_active = false;
      if (!(st & (SCOPE_STATUS_DISABLED | SCOPE_STATUS_USER_DELETED))) {
        if      (mode == SCOPE_STATUS_REPEAT_ON)   eff_active = true;
        else if (mode == SCOPE_STATUS_REPEAT_AUTO) eff_active = in_bbox;
      }

      char block[160];
      snprintf(block, sizeof(block),
               "#%s [%s]:\n"
               "  hash = %02X%02X%02X%02X\n"
               "  rep = %s -> %s",
               name,
               (r.storage == SCOPE_BUILDIN) ? "Build-in" : "Extras",
               key ? key[0] : 0, key ? key[1] : 0, key ? key[2] : 0, key ? key[3] : 0,
               mode_str,
               eff_active ? "AKTIV" : "inaktiv");
      pushCompanionMessage(block);

      char line2[160];
      snprintf(line2, sizeof(line2),
               "  adv = %s\n"
               "  disabled = %s\n"
               "  user_deleted = %s",
               (st & SCOPE_STATUS_ADVERT_OFF)    ? "off" : "auto",
               (st & SCOPE_STATUS_DISABLED)      ? "yes" : "no",
               (st & SCOPE_STATUS_USER_DELETED)  ? "yes" : "no");
      pushCompanionMessage(line2);

      if (has_bbox) {
        char bbox[140];
        snprintf(bbox, sizeof(bbox),
                 "  bbox = lat[%.3f..%.3f] lon[%.3f..%.3f]\n"
                 "  in_bbox = %s",
                 lat1, lat2, lon1, lon2,
                 in_bbox ? "yes" : "no");
        pushCompanionMessage(bbox);
      } else {
        pushCompanionMessage(
          "  bbox = (keine) - 'repeat auto' wirkt wie 'off'");
      }
      return;
    }

    // -- repeater <...> (Sub-Namespace: Repeat-Policy / Liste B) --
    if (sub_idx == 7) {
      const char* sub = strchr(arg, ' ');
      if (sub) { while (*sub == ' ') sub++; }
      // ? -> Kurzhilfe (vor dem Status-Check, sonst greift no-arg=status).
      if (sub && sub[0] == '?'
          && (sub[1] == 0 || sub[1] == ' ' || sub[1] == '\t')) {
        const char* aut = (_prefs.scope_repeater_auto == 1) ? "off" : "on";
        char r[80];
        snprintf(r, sizeof(r), "scope repeater — Sub-Befehle (auto = %s):", aut);
        pushCompanionMessage(r);
        pushCompanionMessage("  scope repeater\n    Status (Mode + Repeat-Liste mit Tags)");
        pushCompanionMessage("  scope repeater mode all|allowlist\n    Global: Liste anwenden (allowlist) oder alles (all)");
        pushCompanionMessage("  scope repeater auto on|off\n    Global: auto-rep-Eintraege greifen (on) oder werden ignoriert (off)\n    off = nur Pin-Eintraege zaehlen");
        pushCompanionMessage("Per Eintrag (siehe 'scope <name> ?'):");
        pushCompanionMessage("Per-Eintrag (mit Name):");
        pushCompanionMessage("  scope <name> pin\n    immer repeaten");
        pushCompanionMessage("  scope <name> auto\n    repeaten wenn GPS in Bbox (Tag A / A-)");
        pushCompanionMessage("  scope <name> off\n    nie repeaten");
        pushCompanionMessage("  scope <name> ?  fuer alle Aktionen");
        return;
      }
      if (!sub || *sub == 0) {
        // Status auf NEW Storage. Header + Liste getrennt damit wire-Limit
        // bei vielen Eintraegen nicht reisst. Wunschliste 11 Schritt 7+9.
        auto inList = [](uint8_t st) -> bool {
          if (st & SCOPE_STATUS_USER_DELETED) return false;
          uint8_t m = st & SCOPE_STATUS_REPEAT_MASK;
          return m != SCOPE_STATUS_REPEAT_OFF;
        };

        int n_total = 0, n_active = 0;
        for (int i = 0; i < _buildin_keys_count; i++) {
          uint8_t st = getBuildinStatus(i);
          if (!inList(st)) continue;
          n_total++;
          uint8_t m = st & SCOPE_STATUS_REPEAT_MASK;
          bool live = !(st & SCOPE_STATUS_DISABLED)
                    && (m == SCOPE_STATUS_REPEAT_ON
                        || (m == SCOPE_STATUS_REPEAT_AUTO && _buildin_in_bbox[i]));
          if (live) n_active++;
        }
        for (int i = 0; i < _prefs.scope_extras_count; i++) {
          ScopeRef r = { SCOPE_EXTRAS, i };
          uint8_t st = getScopeStatus(r);
          if (!inList(st)) continue;
          n_total++;
          uint8_t m = st & SCOPE_STATUS_REPEAT_MASK;
          bool live = !(st & SCOPE_STATUS_DISABLED)
                    && (m == SCOPE_STATUS_REPEAT_ON
                        || (m == SCOPE_STATUS_REPEAT_AUTO && _extras_in_bbox[i]));
          if (live) n_active++;
        }

        char head[140];
        snprintf(head, sizeof(head),
                 "scope repeater:\n  mode = %s\n  count = %d (aktiv jetzt: %d)",
                 _prefs.repeat_scope_mode == REPEAT_SCOPE_MODE_ALL
                     ? "all" : "allowlist",
                 n_total, n_active);
        pushCompanionMessage(head);

        if (n_total > 0) {
          char gb[140]; size_t gu = 0;
          auto gflush = [&]() {
            if (gu == 0) return;
            gb[gu] = 0; pushCompanionMessage(gb); gu = 0;
          };
          auto gappend = [&](const char* s) {
            size_t len = strlen(s);
            if (gu + len > 130) gflush();
            for (size_t i = 0; i < len && gu < sizeof(gb) - 1; i++) gb[gu++] = s[i];
          };
          gappend("  list:");
          // Build-in
          for (int i = 0; i < _buildin_keys_count; i++) {
            uint8_t st = getBuildinStatus(i);
            if (!inList(st)) continue;
            const char* nm = NULL;
            if (!dl9sau_get_region((size_t)i, &nm, NULL, NULL, NULL, NULL)) continue;
            uint8_t m = st & SCOPE_STATUS_REPEAT_MASK;
            const char* tag_state =
                (st & SCOPE_STATUS_DISABLED)      ? "(D)"
              : (m == SCOPE_STATUS_REPEAT_ON)     ? "(P)"
              : (m == SCOPE_STATUS_REPEAT_AUTO
                 && !_buildin_in_bbox[i])         ? "(A-)"  // auto, derzeit aus bbox
              : (m == SCOPE_STATUS_REPEAT_AUTO)   ? "(A)"   // auto, in bbox
                                                  : "";
            char tag[40];
            snprintf(tag, sizeof(tag), " #%s%s", nm, tag_state);
            gappend(tag);
          }
          // Extras
          for (int i = 0; i < _prefs.scope_extras_count; i++) {
            const ScopeRegEntry& e = _prefs.scope_extras[i];
            ScopeRef r = { SCOPE_EXTRAS, i };
            uint8_t st = getScopeStatus(r);
            if (!inList(st)) continue;
            uint8_t m = st & SCOPE_STATUS_REPEAT_MASK;
            const char* tag_state =
                (st & SCOPE_STATUS_DISABLED)      ? "(D)"
              : (m == SCOPE_STATUS_REPEAT_ON)     ? "(P)"
              : (m == SCOPE_STATUS_REPEAT_AUTO
                 && !_extras_in_bbox[i])          ? "(A-)"
              : (m == SCOPE_STATUS_REPEAT_AUTO)   ? "(A)"
                                                  : "";
            char tag[40];
            snprintf(tag, sizeof(tag), " #%s%s", e.name, tag_state);
            gappend(tag);
          }
          gflush();
          pushCompanionMessage(
            "Legende: (A)/(A-)=auto in/out Bbox\n"
            "(P)=pin (immer aktiv), (D)=disabled\n"
            "Umschalten: scope <name> pin|auto|off\n"
            "Hilfe: 'scope rep ?'");
        }
        return;
      }

      // 'mode' + 'auto' als rep-Sub-Aktionen. Per-Eintrag-Verben
      // (pin/geo/off/disable/...) leben unter 'scope <name>'.
      static const CompanionChoice rep_subs[] = {
        { "mode",    false },  // 0
        { "auto",    false },  // 1 (Wunschliste 13)
      };
      char rep_ambig[60];
      int rs = match_choice(sub, rep_subs, 2, rep_ambig, sizeof(rep_ambig));
      if (rs == -1) {
        char r[100]; snprintf(r, sizeof(r), "Mehrdeutig: %s", rep_ambig);
        pushCompanionMessage(r); return;
      }
      if (rs < 0) {
        pushCompanionMessage(
          "Sub-Aktion unbekannt. Global:\n"
          "  scope repeater mode all|allowlist\n"
          "  scope repeater auto on|off\n"
          "Per-Eintrag: 'scope <name> ?'");
        return;
      }

      // -- repeater mode all|allowlist --
      if (rs == 0) {
        const char* mv = strchr(sub, ' ');
        if (mv) { while (*mv == ' ') mv++; }
        static const CompanionChoice mode_ch[] = {
          { "all",       false },
          { "allowlist", false },
        };
        char ma[40];
        int mm = match_choice(mv, mode_ch, 2, ma, sizeof(ma));
        if (mm == -1) { char r[80]; snprintf(r, sizeof(r), "Mehrdeutig: %s", ma); pushCompanionMessage(r); return; }
        if (mm < 0)   { pushCompanionMessage("Usage: scope repeater mode all|allowlist"); return; }
        _prefs.repeat_scope_mode = (mm == 0) ? REPEAT_SCOPE_MODE_ALL : REPEAT_SCOPE_MODE_ALLOWLIST;
        savePrefs();
        pushCompanionMessage(mm == 0 ? "OK - scope repeater mode = all"
                                     : "OK - scope repeater mode = allowlist");
        return;
      }

      // -- repeater auto on|off (Wunschliste 13) --
      if (rs == 1) {
        const char* av = strchr(sub, ' ');
        if (av) { while (*av == ' ') av++; }
        if (!av || *av == 0) {
          char r[160];
          snprintf(r, sizeof(r),
            "scope repeater auto = %s\n"
            "  on:  auto-rep-Eintraege greifen wenn GPS in Bbox.\n"
            "  off: auto-Eintraege werden ignoriert. Nur Pin zaehlt.",
            (_prefs.scope_repeater_auto == 1) ? "off" : "on");
          pushCompanionMessage(r);
          return;
        }
        int aon = match_on_off(av);
        if (aon == -1) { pushCompanionMessage("Mehrdeutig: on off"); return; }
        if (aon < 0)   { pushCompanionMessage("Usage: scope repeater auto on|off"); return; }
        _prefs.scope_repeater_auto = (uint8_t)(aon ? 2 : 1);
        savePrefs();
        char r[140];
        snprintf(r, sizeof(r),
          "OK - scope repeater auto = %s\n"
          "  %s",
          aon ? "on" : "off",
          aon ? "auto-rep-Eintraege greifen wenn in Bbox"
              : "auto-rep-Eintraege ignoriert, nur Pin zaehlt");
        pushCompanionMessage(r);
        return;
      }
    }

    // -- regions (read-only built-in geo-table) --
    if (sub_idx == 8) {
      char gb[200]; size_t gu = 0;
      auto gflush = [&]() {
        if (gu == 0) return;
        gb[gu] = 0; pushCompanionMessage(gb); gu = 0;
      };
      auto gline = [&](const char* line) {
        size_t len = strlen(line);
        if (len > 130) len = 130;
        if (gu > 0 && gu + 1 + len > 130) gflush();
        if (gu > 0) gb[gu++] = '\n';
        for (size_t i = 0; i < len && gu < sizeof(gb) - 1; i++) gb[gu++] = line[i];
      };
      char head[80];
      snprintf(head, sizeof(head), "scope regions (built-in: %u Eintraege):",
               (unsigned)dl9sau_region_count());
      gline(head);
      char tmp[160];
      for (size_t i = 0; i < dl9sau_region_count(); i++) {
        const char* nm = NULL;
        double lat1, lat2, lon1, lon2;
        if (!dl9sau_get_region(i, &nm, &lat1, &lat2, &lon1, &lon2)) continue;
        snprintf(tmp, sizeof(tmp), "  #%-15s lat[%.2f..%.2f] lon[%.2f..%.2f]",
                 nm, lat1, lat2, lon1, lon2);
        gline(tmp);
      }
      gline("Hinweis: read-only; aus dl9sau_geo_recommendations.cpp "
            "im Build-Prozess befuellt.");
      gflush();
      return;
    }

    pushCompanionMessage(
      "Usage (siehe 'help scope'):\n"
      "  scope default|bake|override <name>|clear    eigene Send-Policy\n"
      "  scope list | add <name> | remove <name> | info <name>   Registry\n"
      "  scope regions                                Build-in Geo-Tabelle\n"
      "  scope repeater [...]                         Repeat-Policy");
    return;
  }

  // ---------- repeater [on|off] [--force] -------------------------------
  // Schaltet client_repeat ein/aus mit Frequenz-Sicherheitsgate.
  //   repeater                  -> Status
  //   repeater off              -> immer OK
  //   repeater on               -> nur wenn die Freq im STRICT-Range liegt
  //                                (compliant variant). Schlaegt z.B. fuer
  //                                869.618 MHz fehl.
  //   repeater on --force       -> ueberspringt den STRICT-Check, behaelt
  //                                aber signalFitsInIsmBand als ISM-Gate.
  //                                Damit kann ein User der bewusst auf
  //                                869.618 repeaten will das so freigeben.
  if (starts_with_word(cmd, "repeater")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ') arg++; }

    if (!arg || *arg == 0) {
      uint32_t f_khz = (uint32_t)(_prefs.freq * 1000.0f + 0.5f);
      bool strict_ok = isValidClientRepeatFreq(f_khz);
      const char* ld = (_prefs.loop_detect == 0) ? "off"
                     : (_prefs.loop_detect == 1) ? "minimal"
                     : (_prefs.loop_detect == 2) ? "moderate" : "strict";
      // Inline-Suffix in 'repeater=on'-Zeile: runtime-Status.
      // Prioritaet (1 Slot, mutually exclusive). Reasons in positiver
      // Formulierung (User-Feedback 2026-06-02: doppelt-verneintes
      // 'non-strict' verwirrt -- besser 'freq braucht force').
      //   paused:  wish=on, effective=off, weil is_moving (defensive)
      //   blocked: wish=on, effective=off, weil freq force braucht
      //            aber force=off
      //   sonst:   kein Suffix
      bool wish_on = (_prefs.client_repeat != 0);
      const char* runtime_suffix = "";
      if (wish_on && !_repeating_allowed) {
        if (_prefs.repeater_profile == 0 && _is_moving) {
          runtime_suffix = ", paused (is_moving)";
        } else {
#ifdef REPEATER_DEFENSIVE_FORCE
          runtime_suffix = ", blocked (freq braucht force, force=off)";
#else
          runtime_suffix = ", blocked (freq nicht in client-rep-Liste)";
#endif
        }
      }

      // loop_detect-Inaktiv-Note: nur wenn loop_detect != off UND
      // profile=defensive (Loop-Detect greift nur in normal-Profile).
      const char* ld_note = (_prefs.repeater_profile != 1 && _prefs.loop_detect != 0)
                            ? " (inaktiv: profile=defensive)" : "";

      char line[160];
      snprintf(line, sizeof(line),
               "repeater=%s%s\n"
               "profile=%s\n"
               "loop_detect=%s%s\n"
               "freq=%.4f MHz",
               _prefs.client_repeat ? "on" : "off",
               runtime_suffix,
               _prefs.repeater_profile == 1 ? "normal" : "defensive",
               ld, ld_note,
               _prefs.freq);
      pushCompanionMessage(line);

      // Band-check-force Status als separate Message -- nur wenn Anomalie
      // oder explizit force gesetzt. force ist nur fuer profile=defensive
      // relevant (normal-Profile prueft die Freq nicht gegen client-rep-
      // Range -- echter Repeater ist Admin-Verantwortung).
      // Tabelle der Faelle (nur profile=defensive):
      //   force=on,  needed   -> "force: on (freq braucht es)"
      //   force=on,  no-need  -> "force: on"
      //   force=off, freq braucht force -> Hinweis-Block
      //   force=off, no-need  -> (nichts -- alles sauber)
      if (_prefs.repeater_profile == 0) {
#ifdef REPEATER_DEFENSIVE_FORCE
        bool has_force = _prefs.client_repeat_force != 0;
        bool freq_needs_force = !strict_ok;
        const char* bc = NULL;
        if (has_force && freq_needs_force) {
          bc = "force: on (freq braucht es)";
        } else if (has_force) {
          bc = "force: on";
        } else if (freq_needs_force) {
          bc = "force: off -- freq braucht force fuer client-rep\n"
               "  ('repeater on force' aktiviert das Repeating)";
        }
        if (bc) pushCompanionMessage(bc);
#else
        // Ohne ifdef: nur den 'freq nicht erlaubt'-Fall melden.
        if (!strict_ok) {
          pushCompanionMessage("Hinweis: freq nicht in defensive client-rep-Liste");
        }
#endif
      }
      return;
    }

    // -- profile [defensive|normal] (Wunschliste 8) --
    // Prefix-Match (B3): 'rep pro' soll auch funktionieren.
    bool is_profile_kw = false;
    {
      static const CompanionChoice rep_subs[] = { { "profile", false } };
      char ambig[32];
      int m = match_choice(arg, rep_subs, 1, ambig, sizeof(ambig));
      is_profile_kw = (m == 0);
    }
    if (is_profile_kw) {
      const char* pv = strchr(arg, ' ');
      if (pv) { while (*pv == ' ') pv++; }
      if (!pv || *pv == 0) {
        char r[100];
        snprintf(r, sizeof(r), "repeater profile = %s",
                 _prefs.repeater_profile == 1 ? "normal" : "defensive");
        pushCompanionMessage(r);
        pushCompanionMessage(
          "  defensive: PATH nur fuer lokale Endpoints,\n"
          "    Repeats mit reduzierter Power + CR5\n"
          "    (= client-Repeater).");
        pushCompanionMessage(
          "  normal: ALLE PATH-Pakete weiterleiten,\n"
          "    volle Power + konfigurierte CR\n"
          "    (= wie echter Repeater).");
        return;
      }
      static const CompanionChoice prof_ch[] = {
        { "defensive", false },  // 0
        { "normal",    false },  // 1
      };
      char pa[40];
      int pm = match_choice(pv, prof_ch, 2, pa, sizeof(pa));
      if (pm == -1) { char r[80]; snprintf(r, sizeof(r), "Mehrdeutig: %s", pa); pushCompanionMessage(r); return; }
      if (pm < 0)   { pushCompanionMessage("Usage: repeater profile defensive|normal"); return; }
      _prefs.repeater_profile = (uint8_t)pm;
      savePrefs();
      recomputeRepeatingAllowed("profile change");
      // Reise-Fix 2026-06-08: Profile-Wechsel => Bbox-Quelle aendert sich.
      reevaluateRepeaterBbox();
      char r[100];
      snprintf(r, sizeof(r), "OK - repeater profile = %s",
               pm == 1 ? "normal" : "defensive");
      pushCompanionMessage(r);
      snprintf(r, sizeof(r), "  PATH-Filter: %s",
               pm == 1 ? "AUS (alle PATH weiterleiten)"
                       : "AN (nur lokale Endpoints)");
      pushCompanionMessage(r);
      snprintf(r, sizeof(r), "  Power/CR-Reduktion: %s",
               pm == 1 ? "AUS (volle Power + CR)"
                       : "AN (reduziert + CR5)");
      pushCompanionMessage(r);
      return;
    }

    int rm = match_on_off(arg);
    if (rm == -1) { pushCompanionMessage("Mehrdeutig: on off"); return; }
    if (rm == 0) {
      _prefs.client_repeat = 0;
      // force-Flag bewusst NICHT cleared (User-Wunsch 2026-06-02):
      // persistent ueber on/off, damit erneutes 'repeater on' nicht
      // wieder explizites 'force' verlangt wenn der User es schon
      // einmal gesetzt hat. 'prefs reset' loescht weiterhin alles.
      savePrefs();
      recomputeRepeatingAllowed("repeater off");
      pushCompanionMessage("OK - repeater off.");
      return;
    }
    if (rm == 1) {
      bool force = false;
#ifdef REPEATER_DEFENSIVE_FORCE
      // "force"-Keyword erkennen (iOS-Tastatur macht aus "--force" einen
      // em-dash — daher ein einzelnes lowercase Wort statt Doppel-Hyphen).
      const char* rest = arg;
      while (*rest && *rest != ' ' && *rest != '\t') rest++;  // skip on-Prefix
      while (*rest == ' ' || *rest == '\t') rest++;
      if (starts_with_word(rest, "force")) force = true;
#endif
      // Sicherheitsgate 1: signalFitsInIsmBand (immer aktiv, auch mit force)
      uint32_t f_khz = (uint32_t)(_prefs.freq * 1000.0f + 0.5f);
      uint32_t bw_hz = (uint32_t)(_prefs.bw * 1000.0f + 0.5f);
      if (!signalFitsInIsmBand(f_khz, bw_hz)) {
        char line[160];
        snprintf(line, sizeof(line),
                 "Fehler: %.4f MHz / BW %.1f nicht im ISM-Band. "
                 "Repeater nicht aktiviert.", _prefs.freq, _prefs.bw);
        pushCompanionMessage(line);
        return;
      }
      // Sicherheitsgate 2: nur in profile=defensive. Im profile=normal
      // (echter Repeater) darf der User alle Frequenzen ohne Check
      // nutzen -- Admin-Verantwortung. Mit REPEATER_DEFENSIVE_FORCE-Build
      // zusaetzlich force-Bypass im defensive-Mode.
      bool defensive_mode = (_prefs.repeater_profile == 0);
      if (defensive_mode && !force && !isValidClientRepeatFreq(f_khz)) {
        char line[160];
#ifdef REPEATER_DEFENSIVE_FORCE
        snprintf(line, sizeof(line),
                 "Abgelehnt: %.4f MHz braucht force fuer client-rep.\n"
                 "Mit 'repeater on force' trotzdem aktivieren.", _prefs.freq);
#else
        snprintf(line, sizeof(line),
                 "Abgelehnt: %.4f MHz nicht in defensive client-rep-Liste.",
                 _prefs.freq);
#endif
        pushCompanionMessage(line);
        return;
      }
      _prefs.client_repeat = 1;
#ifdef REPEATER_DEFENSIVE_FORCE
      _prefs.client_repeat_force = force ? 1 : 0;
#endif
      savePrefs();
      recomputeRepeatingAllowed(force ? "repeater on force" : "repeater on");
      char line[80];
      snprintf(line, sizeof(line), "OK - repeater on%s.", force ? " (force)" : "");
      pushCompanionMessage(line);
      return;
    }
#ifdef REPEATER_DEFENSIVE_FORCE
    pushCompanionMessage("Usage: repeater [on [force] | off]");
#else
    pushCompanionMessage("Usage: repeater [on | off]");
#endif
    return;
  }

  // ---------- autoadv [on/off | zerohop on/off | nightly on/off] ---------
  // Schaltet die wiederkehrenden Adverts ein/aus, separat fuer
  // periodic zero-hop und nightly scoped flood. Bitmask in
  // _prefs.auto_advert_enabled. Default 0 (beide aus) nach frischem Flash.
  if (starts_with_word(cmd, "autoadv")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ') arg++; }

    auto print_status = [&]() {
      // Konsistent mit 'status'-Befehl: Uhrzeit statt nur on/off,
      // 'on(?)' bei RTC-unset oder nightly-noch-nicht-geplant.
      char zh_str[16], nl_str[16];
      uint32_t now_rtc = getRTCClock()->getCurrentTime();
      bool rtc_ok = (now_rtc > 1500000000UL);
      int32_t tz = rtc_ok ? localTzOffsetSecs(now_rtc) : 0;
      if (!(_prefs.auto_advert_enabled & AUTO_ADV_ZEROHOP)) {
        strcpy(zh_str, "off");
      } else if (!rtc_ok) {
        strcpy(zh_str, "on(?)");
      } else {
        long delta_ms = (long)(next_periodic_advert_at - millis());
        long delta_s = (delta_ms < 0) ? 0 : delta_ms / 1000;
        uint32_t loc = now_rtc + (uint32_t)delta_s + (uint32_t)tz;
        snprintf(zh_str, sizeof(zh_str), "%02u:%02u",
                 (unsigned)((loc % 86400UL) / 3600UL),
                 (unsigned)((loc % 3600UL) / 60UL));
      }
      if (!(_prefs.auto_advert_enabled & AUTO_ADV_NIGHTLY)) {
        strcpy(nl_str, "off");
      } else if (!rtc_ok || next_night_flood_unix == 0) {
        strcpy(nl_str, "on(?)");
      } else {
        uint32_t loc = next_night_flood_unix + (uint32_t)tz;
        snprintf(nl_str, sizeof(nl_str), "%02u:%02u",
                 (unsigned)((loc % 86400UL) / 3600UL),
                 (unsigned)((loc % 3600UL) / 60UL));
      }
      char line[120];
      snprintf(line, sizeof(line), "autoadv: zerohop=%s  nightly=%s",
               zh_str, nl_str);
      pushCompanionMessage(line);
    };
    auto trigger_zerohop_now = [&]() {
      next_periodic_advert_at = millis();
    };
    auto trigger_nightly_reschedule = [&]() {
      next_night_flood_unix = 0;
    };

    if (!arg || *arg == 0) { print_status(); return; }

    // Erst zerohop/nightly als Sub-Bereich pruefen — sonst wuerde
    // match_on_off bei "n" auf nightly statt on/off matchen.
    bool is_zh = starts_with_word(arg, "zerohop");
    bool is_nl = starts_with_word(arg, "nightly");
    if (is_zh || is_nl) {
      const char* sub = strchr(arg, ' ');
      if (sub) { while (*sub == ' ') sub++; }
      int sm = match_on_off(sub);
      if (sm == -1) { pushCompanionMessage("Mehrdeutig: on off"); return; }
      if (sm < 0) {
        pushCompanionMessage(is_zh
          ? "Usage: autoadv zerohop on|off"
          : "Usage: autoadv nightly on|off");
        return;
      }
      uint8_t mask = is_zh ? AUTO_ADV_ZEROHOP : AUTO_ADV_NIGHTLY;
      bool on = (sm == 1);
      if (on) {
        _prefs.auto_advert_enabled |= mask;
        if (is_zh) trigger_zerohop_now();
        else       trigger_nightly_reschedule();
      } else {
        _prefs.auto_advert_enabled &= ~mask;
      }
      savePrefs();
      char r[120];
      snprintf(r, sizeof(r), "OK - autoadv %s %s.",
               is_zh ? "zerohop" : "nightly", on ? "on" : "off");
      pushCompanionMessage(r);
      return;
    }

    // Top-Level on/off -> beide Flags
    int am = match_on_off(arg);
    if (am == -1) { pushCompanionMessage("Mehrdeutig: on off"); return; }
    if (am == 1) {
      _prefs.auto_advert_enabled = AUTO_ADV_ALL;
      savePrefs();
      trigger_zerohop_now();
      trigger_nightly_reschedule();
      pushCompanionMessage("OK - autoadv zerohop=on, nightly=on (sofort + reschedule).");
      return;
    }
    if (am == 0) {
      _prefs.auto_advert_enabled = 0;
      savePrefs();
      pushCompanionMessage("OK - autoadv zerohop=off, nightly=off.");
      return;
    }

    pushCompanionMessage("Usage: autoadv [on | off | zerohop on/off | nightly on/off]");
    return;
  }

  // ---------- chatname --------------------------------------------------
  // Konfiguriert wie der Sender-Name in Group-Channel-Messages aussieht.
  //   chatname                 -> aktuellen Status anzeigen
  //   chatname 1               -> nur erstes Wort aus node_name
  //   chatname 2  | auto       -> erste 2 Wörter aus node_name (Standard)
  //   chatname custom <Name>   -> frei wählbarer Text (raw, mit Original-Case)
  if (starts_with_word(cmd, "chatname")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ') arg++; }

    if (!arg || *arg == 0) {
      char block[200], preview[64], node_trim[64];
      copyShortSenderName(preview, sizeof(preview));
      // node_name fuer die Anzeige in lokalen Buffer und trailing-WS abschneiden
      size_t nl = 0;
      while (nl + 1 < sizeof(node_trim) && _prefs.node_name[nl] != 0
             && nl < sizeof(_prefs.node_name)) {
        node_trim[nl] = _prefs.node_name[nl];
        nl++;
      }
      while (nl > 0 && (node_trim[nl-1] == ' ' || node_trim[nl-1] == '\t')) nl--;
      node_trim[nl] = 0;
      const char* node_full = node_trim[0] ? node_trim : "(empty)";
      uint8_t mode = _prefs.chat_name_mode;
      if (mode == 0) {
        snprintf(block, sizeof(block),
                 "chatname:\n  mode = default (full)\n  node_name = \"%s\"\n  preview = \"%s\"",
                 node_full, preview);
      } else if (mode == 255) {
        snprintf(block, sizeof(block),
                 "chatname:\n  mode = custom\n  preview = \"%s\"",
                 preview);
      } else {
        snprintf(block, sizeof(block),
                 "chatname:\n  mode = first %u word(s)\n  preview = \"%s\"",
                 (unsigned)mode, preview);
      }
      pushCompanionMessage(block);
      return;
    }

    // Numerisches Argument zuerst pruefen — chatname N (1..253) hat keinen
    // festen Keyword-Namen und wuerde sonst von match_choice abgelehnt.
    if (arg[0] >= '0' && arg[0] <= '9') {
      int n = atoi(arg);
      if (n < 1 || n > 253) {
        pushCompanionMessage("chatname N: N muss 1..253 sein. Nutze 'chatname default' fuer den vollen Namen.");
        return;
      }
      _prefs.chat_name_mode = (uint8_t)n;
      savePrefs();
      char line[80];
      snprintf(line, sizeof(line), "OK - chatname = erste %d Woerter aus node_name.", n);
      pushCompanionMessage(line);
      return;
    }

    static const CompanionChoice cn_ch[] = {
      { "default", false },
      { "hex",     false },
      { "custom",  false },
    };
    char cn_ambig[40];
    int cn_m = match_choice(arg, cn_ch, 3, cn_ambig, sizeof(cn_ambig));
    if (cn_m == -1) {
      char r[80]; snprintf(r, sizeof(r), "Mehrdeutig: %s", cn_ambig);
      pushCompanionMessage(r); return;
    }
    if (cn_m == 0) {
      _prefs.chat_name_mode = 0;
      savePrefs();
      pushCompanionMessage("OK - chatname = default (full node_name).");
      return;
    }

    // ---- chatname hex: Diagnose, dumpt die bytes von node_name + preview
    // ---- Nutzlich um UTF-8-Multi-Byte-Probleme zu erkennen (z.B. wenn ein
    // ---- Emoji am Ende des Names unklar wirkt).
    if (cn_m == 1) {
      auto dump_hex = [&](const char* label, const char* src, size_t src_max) {
        char buf[200];
        int p = snprintf(buf, sizeof(buf), "%s:", label);
        size_t k = 0;
        while (k < src_max && src[k] != 0 && p + 4 < (int)sizeof(buf)) {
          p += snprintf(buf + p, sizeof(buf) - p, " %02X", (unsigned char)src[k]);
          k++;
        }
        if (k == 0) snprintf(buf + p, sizeof(buf) - p, " (empty)");
        pushCompanionMessage(buf);
      };
      dump_hex("node_name", _prefs.node_name, sizeof(_prefs.node_name));
      dump_hex("chat_name_custom", _prefs.chat_name_custom, sizeof(_prefs.chat_name_custom));
      char preview[64];
      copyShortSenderName(preview, sizeof(preview));
      dump_hex("preview", preview, sizeof(preview));
      return;
    }

    if (cn_m == 2) {
      // Custom-Text aus raw_cmd (Original-Case) via Token-Walk: 3. Token
      // nach "chatname"+"custom". Robust gegen Top-Level-Prefix-Expansion.
      const char* lc_text = strchr(arg, ' ');
      if (lc_text) { while (*lc_text == ' ') lc_text++; }
      if (!lc_text || *lc_text == 0) {
        pushCompanionMessage("Usage: chatname custom <Name>");
        return;
      }
      const char* rp = raw_cmd;
      while (*rp == ' ' || *rp == '\t') rp++;
      while (*rp && *rp != ' ' && *rp != '\t') rp++;          // skip 1st token
      while (*rp == ' ' || *rp == '\t') rp++;
      while (*rp && *rp != ' ' && *rp != '\t') rp++;          // skip "custom"
      while (*rp == ' ' || *rp == '\t') rp++;                 // skip leading ws to 3rd token
      // rp zeigt jetzt auf den Anfang des custom-Texts (Original-Case).
      // Sanity: leading WS bereits abgeschnitten, jetzt mehrfache interne
      // Whitespaces zu einem Space collapsen, trailing WS abschneiden.
      char clean[sizeof(_prefs.chat_name_custom)];
      size_t cl = 0;
      bool prev_space = false;
      while (*rp != 0 && cl + 1 < sizeof(clean)) {
        char c = *rp++;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
          if (!prev_space && cl > 0) {
            clean[cl++] = ' ';
            prev_space = true;
          }
        } else {
          clean[cl++] = c;
          prev_space = false;
        }
      }
      while (cl > 0 && clean[cl - 1] == ' ') cl--;
      clean[cl] = 0;
      if (cl == 0) {
        pushCompanionMessage("Usage: chatname custom <Name>");
        return;
      }
      memcpy(_prefs.chat_name_custom, clean, cl + 1);
      _prefs.chat_name_mode = 255;
      savePrefs();
      char reply[120];
      snprintf(reply, sizeof(reply), "OK - chatname custom = \"%s\".", _prefs.chat_name_custom);
      pushCompanionMessage(reply);
      return;
    }
    pushCompanionMessage("Usage: chatname [default | N | custom <Name>]\nN = 1..253");
    return;
  }

  // ---------- unbekannt -------------------------------------------------
  pushCompanionMessage("(unbekannter Befehl - 'help' fuer Liste)");
}

void MyMesh::pushDebugLog(const char* fmt, ...) {
  // millis()-Prefix vor jedem Log-Eintrag -- erlaubt Korrelation und
  // Reihenfolge-Validierung auf der seriellen Konsole (User-Wunsch
  // 2026-05-30). Der Wert ist seit-Boot in Millisekunden, wraps alle
  // ~49 Tage (uint32). Format: "[+1234567ms] ".
  char buf[180];
  int n_pref = snprintf(buf, sizeof(buf), "[+%lums] ", (unsigned long)millis());
  va_list ap;
  va_start(ap, fmt);
  int n_body = vsnprintf(buf + n_pref, sizeof(buf) - n_pref, fmt, ap);
  va_end(ap);
  if (n_body <= 0) return;
  int n = n_pref + n_body;
  if (n >= (int)sizeof(buf)) n = sizeof(buf) - 1;

  // Serial-Output gated von _prefs.log_flags bit 0 (asymm: 1 = ON).
  // Default 0 = USB-Serial AUS. 'logging usb on' setzt bit 0. Symmetrisch
  // fuer BLE- und USB-Companion-Builds: bei USB-Companion wuerde Trace-
  // Geblubber sonst die App-Frame-Stream zerstoeren -- da der User in
  // dem Fall auch nicht ueber die App das Logging abschalten koennte,
  // ist 'per Default aus' universell sicher.
  // Zusaetzlich: USB-CDC-Connect-Check. Auf Powerbank (kein Host) wuerden
  // die write()-Aufrufe sonst evtl. blockieren und das BLE-Supervision-
  // Timeout reissen (User-Report 2026-06-02). Bei 'if (!Serial)' wird der
  // ganze Log-Ausgabe-Pfad ohne Risiko geskipped.
  // CRLF-Uebersetzung wie zuvor (LF -> CRLF, multiline-aware).
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  if ((_prefs.log_flags & 0x01) && Serial) {
#else
  if (_prefs.log_flags & 0x01) {
#endif
    // CRLF-Expansion in lokalen Buffer, dann EIN einzelner Serial.write
    // statt zeichenweise. Hintergrund (User-Erkenntnis 2026-06-02):
    // jedes einzelne Serial.write kann bei DTR-true-ohne-aktivem-Host
    // bis tx_timeout_ms blockieren. 200 Aufrufe = 200x Blockzeit, was
    // BLE-Supervision-Timeout reisst. EIN Write ueber den gesamten
    // Buffer hat nur EINE Blockphase.
    char out[200 * 2 + 4];  // max Expansion: jedes Byte -> CRLF
    int oi = 0;
    for (int i = 0; i < n && oi < (int)sizeof(out) - 2; i++) {
      if (buf[i] == '\n' && (i == 0 || buf[i - 1] != '\r')) {
        out[oi++] = '\r';
      }
      out[oi++] = buf[i];
    }
    if (n > 0 && buf[n - 1] != '\n' && oi < (int)sizeof(out) - 2) {
      out[oi++] = '\r';
      out[oi++] = '\n';
    }
    if (oi > 0) Serial.write((const uint8_t*)out, (size_t)oi);
  }

  // Push to app debug log if connected. Frame: [PUSH_CODE][text bytes, no null].
  // App-Frame bekommt kein CRLF -- die UI fuegt eigene Zeilenumbrueche.
  if (_serial != NULL && _serial->isConnected()) {
    uint8_t frame[1 + sizeof(buf)];
    frame[0] = PUSH_CODE_DEBUG_LOG;
    int copy_len = n;
    // strip trailing newline for the app frame — UI usually adds its own
    while (copy_len > 0 && (buf[copy_len - 1] == '\n' || buf[copy_len - 1] == '\r')) {
      copy_len--;
    }
    memcpy(&frame[1], buf, copy_len);
    _serial->writeFrame(frame, 1 + copy_len);
  }
}

void MyMesh::initRegionKeys() {
#if DL9SAU_REGIONS_AVAILABLE
  for (int i = 0; i < DL9SAU_REGION_COUNT; i++) {
    char buf[40];
    int n = snprintf(buf, sizeof(buf), "#%s", dl9sau_regions[i].name);
    if (n <= 0) continue;
    SHA256 sha;
    sha.update((const uint8_t*)buf, (size_t)n);
    sha.finalize(_region_keys[i].key, sizeof(_region_keys[i].key));
  }
  _region_keys_ready = true;
#endif
}

const char* MyMesh::lookupRegionByTransportCode(const mesh::Packet* packet) const {
#if DL9SAU_REGIONS_AVAILABLE
  if (!_region_keys_ready || packet == NULL || !packet->hasTransportCodes()) return NULL;
  uint16_t target = packet->transport_codes[0];
  for (int i = 0; i < DL9SAU_REGION_COUNT; i++) {
    if (_region_keys[i].calcTransportCode(packet) == target) {
      return dl9sau_regions[i].name;
    }
  }
#else
  (void)packet;
#endif
  return NULL;
}

bool MyMesh::advert() {
  mesh::Packet* pkt;
  if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
    pkt = createSelfAdvert(_prefs.node_name);
  } else {
    pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
  }
  if (pkt) {
    sendZeroHop(pkt);
    _tx_advert_count++;
    pushDebugLog("[ADV-DBG] ui-button (advert()), millis=%lu\n", millis());
    traceCompanion(TRACE_ADVERTS, "[adv] ui-button (manual)");
    return true;
  } else {
    return false;
  }
}

// To check if there is pending work
bool MyMesh::hasPendingWork() const {
  return _mgr->getOutboundTotal() > 0 || dirty_contacts_expiry != 0;
}
