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
#define CMD_DEVICE_QEURY              22
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
#define RESP_CODE_DEVICE_INFO         13 // a reply to CMD_DEVICE_QEURY
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
      slot++;
    }
    f.close();
  }
  if (max_seq + 1 > _msg_seq_next) _msg_seq_next = max_seq;  // ++ in addToOfflineQueue erhoeht auf max_seq+1
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
  if (_companion_channel_idx != 0xFF && channel_idx == _companion_channel_idx) {
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

int MyMesh::getInterferenceThreshold() const {
  return 0; // disabled for now, until currentRSSI() problem is resolved
}

int MyMesh::calcRxDelay(float score, uint32_t air_time) const {
  if (_prefs.rx_delay_base <= 0.0f) return 0;
  return (int)((pow(_prefs.rx_delay_base, 0.85f - score) - 1.0) * air_time);
}

uint32_t MyMesh::getRetransmitDelay(const mesh::Packet *packet) {
  // Faktor analog zum simple_repeater (_prefs.tx_delay_factor). 0.5f bleibt
  // der bisherige hartcodierte Default — wird in begin() als Fallback gesetzt.
  float f = _prefs.tx_delay_factor;
  uint32_t t = (uint32_t)(_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * f);
  return getRNG()->nextInt(0, 5*t + 1);
}
uint32_t MyMesh::getDirectRetransmitDelay(const mesh::Packet *packet) {
  float f = _prefs.direct_tx_delay_factor;
  uint32_t t = (uint32_t)(_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * f);
  return getRNG()->nextInt(0, 5*t + 1);
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
  if (is_new) {
    traceCompanion(TRACE_HEARD, "[heard] neuer Direct-Node hash=0x%02X", hash);
  }
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
  BaseChatMesh::onAdvertRecv(packet, id, timestamp, app_data, app_data_len);

  // Wunschliste 15: bei zero-hop heard (path_len==0) zur Runtime-
  // Neighbour-Tabelle hinzufuegen. 'liberal' Filter: alle adv_types
  // werden aufgenommen (chat/repeater/sensor/room).
  if (packet != NULL && packet->path_len == 0
      && app_data != NULL && app_data_len > 0) {
    AdvertDataParser parser(app_data, app_data_len);
    if (parser.isValid()) {
      putRuntimeNeighbour(id, timestamp, _last_advert_snr_q4, parser.getType());
    }
  }
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
  // Track only adverts received directly (zero-hop, no repeater in the path).
  if ((path_len & 63) == 0) {
    markHeardDirect(contact.id.pub_key[0]);
    // Stats: zero-hop direkt empfangene Adverts pro Node-Typ
    if (contact.type < 5 && _heard_direct[contact.type] < 0xFFFF) {
      _heard_direct[contact.type]++;
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

bool MyMesh::filterRecvFloodPacket(mesh::Packet* packet) {
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
  if (ptype_raw < 16 && _rx_flood_by_ptype[ptype_raw] < 0xFFFF) {
    _rx_flood_by_ptype[ptype_raw]++;
  }

  if (_prefs.client_repeat == 0) {
    // Kein Trace hier - bei deaktiviertem Repeater wuerde JEDES Paket einen
    // filter-trace generieren, das ist nur Laerm.
    return false;
  }

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
  // ADVERTs and ACKs: forward only if the packet is scoped (transport-coded)
  else if (ptype == PAYLOAD_TYPE_ADVERT || ptype == PAYLOAD_TYPE_ACK ||
      ptype == PAYLOAD_TYPE_GRP_TXT || ptype == PAYLOAD_TYPE_GRP_DATA ||
      ptype == PAYLOAD_TYPE_MULTIPART || ptype == PAYLOAD_TYPE_CONTROL ||
      ptype == PAYLOAD_TYPE_RAW_CUSTOM || ptype == PAYLOAD_TYPE_TRACE ||
      ptype == PAYLOAD_TYPE_REQ || ptype == PAYLOAD_TYPE_RESPONSE ||
      ptype == PAYLOAD_TYPE_TXT_MSG || ptype == PAYLOAD_TYPE_ANON_REQ) {
    decision = packet->hasTransportCodes();
    if (!decision) reject_reason = "unscoped";
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
      // region/regional: konfigurierbares Hop-Limit (scope_regional_hop_limit).
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
      auto codeMatches = [&](int idx) -> bool {
        return idx >= 0 && idx < _buildin_keys_count
               && _buildin_keys[idx].calcTransportCode(packet) == target;
      };
      bool is_local         = codeMatches(idx_local) || codeMatches(idx_lokal);
      bool is_region        = codeMatches(idx_region) || codeMatches(idx_regional);
      bool is_local_discard = codeMatches(idx_local_discard);

      if (is_local_discard) {
        // SENTINEL: NIE weiterleiten. Gilt auch im 'repeat all'-Modus.
        decision = false;
        reject_reason = "local-discard";
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
        if (hops >= _prefs.scope_regional_hop_limit) {
          decision = false;
          reject_reason = "region-hop-limit";
        }
      }
    }
  } else if (ptype == PAYLOAD_TYPE_PATH) {
    // PATH discovery. Wunschliste 8:
    //   defensive (Default): nur fuer lokale Nodes repeaten (heard < 48h
    //     ODER known contact < 48h).
    //   normal: alle PATH-Pakete weiterleiten ("wie echter Repeater").
    if (_prefs.repeater_profile == 1) {
      decision = true;  // normal-Profil: kein lokaler Endpoint-Filter
    } else if (packet->payload_len >= 2) {
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

      decision = matchHash(dest_hash) || matchHash(src_hash);
      if (!decision) reject_reason = "path-endpoint-unknown";
    } else {
      reject_reason = "path-payload-too-short";
    }
  } else {
    reject_reason = "unknown-ptype";
  }

  if (decision) {
    _tx_digi_count++;
    if (ptype < 16 && _repeat_by_ptype[ptype] < 0xFFFF) {
      _repeat_by_ptype[ptype]++;
    }
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
  TransportKey eff_scope;
  if (!send_scope.isNull()) {
    eff_scope = send_scope;
  } else if (!resolveDefaultOrGeo(eff_scope)) {
    memset(eff_scope.key, 0, sizeof(eff_scope.key));
  }
  sendFloodScoped(eff_scope, pkt, delay_millis);
}

void MyMesh::onMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                           const char *text) {
  markConnectionActive(from); // in case this is from a server, and we have a connection
  queueMessage(from, TXT_TYPE_PLAIN, pkt, sender_timestamp, NULL, 0, text);
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

void MyMesh::onChannelMessageRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint32_t timestamp,
                                  const char *text) {
  // Build an augmented text that exposes the packet's scope (region) name
  // to the app, using a text-convention "Sender (#scope): msg". No
  // wire-protocol change required.
  //   scoped + known region   -> (#name)
  //   scoped + unknown region -> (#?)  (raw transport_code is payload-
  //                             dependent and not a stable identifier)
  //   unscoped                -> (#*)  (borrowed from repeater allowf-*
  //                             notation: wildcard / no scope)
  const char* effective_text = text;
  char augmented[MAX_TEXT_LEN + 32];
  const char* sep = strstr(text, ": ");
  if (sep) {
    const char* scope_label = NULL;
    char buf[36];
    if (pkt->hasTransportCodes()) {
      const char* scope_name = lookupRegionByTransportCode(pkt);
      if (scope_name) {
        snprintf(buf, sizeof(buf), "#%s", scope_name);
      } else {
        snprintf(buf, sizeof(buf), "#?");
      }
      scope_label = buf;
    } else {
      scope_label = "#*";
    }
    size_t prefix_len = (size_t)(sep - text);
    int n = snprintf(augmented, sizeof(augmented), "%.*s (%s)%s",
                     (int)prefix_len, text, scope_label, sep);
    if (n > 0 && n < (int)sizeof(augmented)) {
      effective_text = augmented;
    }
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
  } else {
    return;  // unbekannt oder per role gegated
  }

  // Reply zurueck. Wenn Request flood war: createPathReturn (analog
  // simple_repeater). Wenn direct + path bekannt: sendDirect; sonst
  // sendFlood.
  if (packet->isRouteFlood()) {
    mesh::Packet* path = createPathReturn(sender, secret, packet->path, packet->path_len,
                                          PAYLOAD_TYPE_RESPONSE, reply_data, reply_len);
    if (path) sendFlood(path, 300 /* ms reply-delay */);
  } else {
    mesh::Packet* reply = createDatagram(PAYLOAD_TYPE_RESPONSE, sender, secret,
                                          reply_data, reply_len);
    if (!reply) return;
    if (reply_path_len > 0) {
      uint8_t path_meta = ((hash_size - 1) << 6) | (reply_path_len & 63);
      sendDirect(reply, (uint8_t*)reply_path, path_meta, 300 /* ms reply-delay */);
    } else {
      sendFlood(reply, 300 /* ms reply-delay */);
    }
  }
}

uint8_t MyMesh::onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                                 uint8_t len, uint8_t *reply) {
  uint8_t role = effectiveAdvertRole();

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
  } else if (len > 4 && tag == pending_req) {  // check for matching response tag
    pending_req = 0;

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

void MyMesh::onControlDataRecv(mesh::Packet *packet) {
  if (packet->payload_len + 4 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onControlDataRecv(), payload_len too long: %d", packet->payload_len);
    return;
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
  _pending_reboot_at = 0;
  memset(_heard_direct,       0, sizeof(_heard_direct));
  memset(_rx_advert_total,    0, sizeof(_rx_advert_total));
  memset(_rx_flood_by_ptype,  0, sizeof(_rx_flood_by_ptype));
  memset(_repeat_by_ptype,    0, sizeof(_repeat_by_ptype));
  memset(_tx_total_by_ptype,  0, sizeof(_tx_total_by_ptype));
  memset(_heard_quality,      0, sizeof(_heard_quality));
  _tx_repeat_airtime_ms = 0;
  _last_advert_snr_q4 = 0;
  memset(_duty_air_ms_per_minute, 0, sizeof(_duty_air_ms_per_minute));
  _duty_slot_idx = 0;
  _duty_slot_start_ms = 0;
  _duty_last_total_ms = 0;
  _duty_blocked_count = 0;

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
  // tx_delay_factor / direct_tx_delay_factor: 0 wird als uninitialisiert
  // gewertet (bisher hartcodiert in MyMesh::getRetransmitDelay) und
  // einmalig auf den Companion-Default gesetzt. Obergrenze wie simple_repeater.
  if (_prefs.tx_delay_factor <= 0.0f || _prefs.tx_delay_factor > 2.0f) {
    _prefs.tx_delay_factor = 0.5f;
  }
  if (_prefs.direct_tx_delay_factor <= 0.0f || _prefs.direct_tx_delay_factor > 2.0f) {
    _prefs.direct_tx_delay_factor = 0.2f;
  }
  if (_prefs.repeat_scope_mode > REPEAT_SCOPE_MODE_ALLOWLIST) {
    _prefs.repeat_scope_mode = REPEAT_SCOPE_MODE_ALL;
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
    TransportKeyStore tmp;
    for (size_t i = 0; i < n; i++) {
      const char* name = NULL;
      if (!dl9sau_get_region(i, &name, NULL, NULL, NULL, NULL)) continue;
      char tag[40];
      snprintf(tag, sizeof(tag), "#%s", name);
      tmp.getAutoKeyFor(0, tag, _buildin_keys[i]);
    }
  }

  // flood_max: 0 = uninitialisiert -> Default 16 (analog dem alten
  // hartcodierten Wert). Range 1..64 analog CommonCLI flood.max.
  if (_prefs.flood_max == 0 || _prefs.flood_max > 64) {
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
  // scope_regional_hop_limit: 0 = uninitialisiert -> Companion-Default 3.
  // Range 1..flood_max (sonst widerspruechlich — flood_max ist die harte
  // Obergrenze, regional muss drunter liegen).
  if (_prefs.scope_regional_hop_limit == 0
      || _prefs.scope_regional_hop_limit > _prefs.flood_max) {
    _prefs.scope_regional_hop_limit = (_prefs.flood_max < 3) ? _prefs.flood_max : 3;
  }
  _prefs.airtime_factor = constrain(_prefs.airtime_factor, 0, 9.0f);
  _prefs.freq = constrain(_prefs.freq, 150.0f, 2500.0f);
  _prefs.bw = constrain(_prefs.bw, 7.8f, 500.0f);
  _prefs.sf = constrain(_prefs.sf, 5, 12);
  _prefs.cr = constrain(_prefs.cr, 5, 8);
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

  resetContacts();
  _store->loadContacts(this);
  bootstrapRTCfromContacts();
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

  // Boot-Geo-Push (Channel ist jetzt vorhanden, Output erscheint im Chat):
  if (_boot_pos_known) {
    maybePushGeoRecommendation(_boot_lat, _boot_lon);
  }

  applyRadioPolicy();
  radio_set_tx_power(_prefs.tx_power_dbm);
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
  { 869400, 869587 },   // EU 869 narrow OHNE 869.618 (compliant)
  { 902000, 928000 }    // US 915
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
  radio_set_params(freq, _prefs.bw, _prefs.sf, _prefs.cr);
}

void MyMesh::applyPacketTxOverrides(const mesh::Packet* packet) {
  if (packet == NULL) return;
  // Stats: zentraler TX-Hook — laeuft fuer JEDEN ausgehenden Packet (eigene
  // UND repeated). Eigene Pakete = _tx_total_by_ptype - _repeat_by_ptype.
  uint8_t pt = packet->getPayloadType();
  if (pt < 16 && _tx_total_by_ptype[pt] < 0xFFFF) {
    _tx_total_by_ptype[pt]++;
  }
  uint8_t flags = packet->tx_flags;
  if (flags == 0) return;

  if (flags & PKT_TX_REDUCE_POWER) {
    int reduced = (int)_prefs.tx_power_dbm - CR_TX_POWER_REDUCTION_DB;
    if (reduced < CR_TX_POWER_FLOOR_DBM) reduced = CR_TX_POWER_FLOOR_DBM;
    if (reduced > _prefs.tx_power_dbm) reduced = _prefs.tx_power_dbm;  // never *raise* power
    radio_set_tx_power((int8_t)reduced);
  }
  if ((flags & PKT_TX_FORCE_CR5) && _prefs.cr != CR_REPEATER_CR) {
    float freq = _prefs.freq;
    if (fabsf(freq - CR_NARROW_FREQ_TRIGGER) < 0.0005f) freq = CR_NARROW_FREQ_ACTUAL;
    radio_set_params(freq, _prefs.bw, _prefs.sf, CR_REPEATER_CR);
  }
}

void MyMesh::restorePacketTxDefaults() {
  // Bring radio back to the user-configured CR and full TX power. Cheap if
  // nothing was overridden (radio_set_params and radio_set_tx_power are
  // light register writes on SX126x).
  applyRadioPolicy();
  radio_set_tx_power(_prefs.tx_power_dbm);
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
  if (cmd_frame[0] == CMD_DEVICE_QEURY && len >= 2) { // sent when app establishes connection
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
                        : ERR_CODE_UNSUPPORTED_CMD); // unknown recipient, or unsuported TXT_TYPE_*
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
    } else if (channel_idx == _companion_channel_idx && _companion_channel_idx != 0xFF) {
      // Lokaler Companion-Channel: NICHT senden, sondern als Befehl parsen.
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
      if (success && sendGroupMessage(msg_timestamp, channel.channel, short_sender, text, len - i)) {
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
      getRTCClock()->setCurrentTime(secs);
      // Any RTC-driven schedule made before this point used the stale time;
      // invalidate so the next loop tick re-picks a slot with the corrected RTC.
      next_night_flood_unix = 0;
      pushDebugLog("[ADV-DBG] CMD_SET_DEVICE_TIME: rtc %lu -> %lu, nightly slot invalidated\n",
                    (unsigned long)curr, (unsigned long)secs);
      writeOKFrame();
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

    if (repeat && !_prefs.client_repeat_force && !isValidClientRepeatFreq(freq)) {
      // App will Repeater aktivieren auf einer Freq die ausserhalb des
      // strict-Range liegt UND der Force-Flag wurde nicht gesetzt (siehe
      // Companion-Befehl "repeater on force"). Ablehnen.
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
      // Force-Flag wird gecleared sobald der Repeater per App deaktiviert
      // wird. Damit muss er erneut per Companion "repeater on force"
      // aktiviert werden — verhindert dass jemand per App ein/aus toggelt
      // und dabei den force-Modus stillschweigend reaktiviert.
      if (!repeat) _prefs.client_repeat_force = 0;
      savePrefs();

      applyRadioPolicy();
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
      radio_set_tx_power(_prefs.tx_power_dbm);
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
    uint8_t *data = &cmd_frame[1 + PUB_KEY_SIZE];
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
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
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
    if (setChannel(channel_idx, channel)) {
      saveChannels();
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
      memcpy(send_scope.key, &cmd_frame[2], sizeof(send_scope.key));  // set curr scope TransportKey
    } else {
      memset(send_scope.key, 0, sizeof(send_scope.key));  // set scope to null
    }
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
    int i = 0;
    out_frame[i++] = RESP_ALLOWED_REPEAT_FREQ;
    for (int k = 0; k < sizeof(repeat_freq_ranges)/sizeof(repeat_freq_ranges[0]) && i + 8 < sizeof(out_frame); k++) {
      auto r = &repeat_freq_ranges[k];
      memcpy(&out_frame[i], &r->lower_freq, 4); i += 4;
      memcpy(&out_frame[i], &r->upper_freq, 4); i += 4;
    }
    _serial->writeFrame(out_frame, i);
  } else {
    writeErrFrame(ERR_CODE_UNSUPPORTED_CMD);
    MESH_DEBUG_PRINTLN("ERROR: unknown command: %02X", cmd_frame[0]);
  }
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
    if (_iter.hasNext(this, contact)) {
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
    next_periodic_advert_at = futureMillis(computeNextAdvertIntervalMs());
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
      if (delta > 300 || delta < -300) {   // 5 min jump in either direction
        traceCompanion(TRACE_RTC, "[rtc] Sprung %ld sec erkannt", (long)delta);
        if (next_night_flood_unix != 0) {
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
  // shift into local time
  uint32_t local_now = now + (uint32_t)LOCAL_TZ_OFFSET_SECS;
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
  next_night_flood_unix = pick_local - (uint32_t)LOCAL_TZ_OFFSET_SECS;
  uint32_t now_rtc = getRTCClock()->getCurrentTime();
  long until_s = (long)next_night_flood_unix - (long)now_rtc;
  traceCompanion(TRACE_NIGHT, "[night] scheduled in %ld min", until_s / 60);
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
    evaluateScopeBboxes(cur_lat, cur_lon);
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
      traceCompanion(TRACE_MOTION, "[motion] %s (Anker-Distanz %d m) pos=%s",
                     _is_moving ? "moving" : "static", (int)d_m, ll);
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
    evaluateScopeBboxes(cur_lat, cur_lon);
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
  if (!_gps_had_fix_ever) return;    // still in initial boot search — keep GPS on

  unsigned long now = millis();
  bool gps_is_on = false;
  const char* cur = sensors.getSettingByKey("gps");
  if (cur != NULL) gps_is_on = (cur[0] == '1');

  // always-on Modus: nicht cyceln, GPS dauerhaft an
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
  char chat[256];
  snprintf(chat, sizeof(chat), "GEO-SCOPE @ %s: %s", ll, buf);
  pushCompanionMessage(chat);
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

void MyMesh::setupCompanionChannel() {
  ChannelDetails ch;

  // Schon vorhanden (aus persistiertem /channels2 geladen)?
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
    if (getChannel(i, ch) &&
        strncmp(ch.name, COMPANION_CHANNEL_NAME, sizeof(ch.name)) == 0) {
      _companion_channel_idx = (uint8_t)i;
      return;
    }
  }
  // Nicht gefunden — ersten freien Slot suchen und Channel manuell anlegen.
  int target = -1;
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
    if (getChannel(i, ch) && ch.name[0] == 0) { target = i; break; }
  }
  if (target < 0) {
    _companion_channel_idx = 0xFF; // kein Slot frei
    return;
  }
  // Neue ChannelDetails komponieren und über setChannel() schreiben — das
  // berechnet auch den Hash und respektiert die internen Datenstrukturen.
  ChannelDetails nch;
  memset(&nch, 0, sizeof(nch));
  StrHelper::strncpy(nch.name, COMPANION_CHANNEL_NAME, sizeof(nch.name));
  // 16 Null-Bytes als Pseudo-Key. Über diesen Channel wird NIE transmittet,
  // der Key dient nur als Channel-Identität für die App-Liste.
  // setChannel berechnet den hash aus secret automatisch (siehe BaseChatMesh).
  setChannel(target, nch);
  _companion_channel_idx = (uint8_t)target;
  saveChannels(); // persistieren, damit der Index über Reboots stabil bleibt
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
  _tx_repeat_airtime_ms = 0;
  memset(_rx_flood_by_ptype, 0, sizeof(_rx_flood_by_ptype));
  memset(_repeat_by_ptype,   0, sizeof(_repeat_by_ptype));
  memset(_tx_total_by_ptype, 0, sizeof(_tx_total_by_ptype));
  memset(_heard_direct,      0, sizeof(_heard_direct));
  memset(_heard_quality,     0, sizeof(_heard_quality));
  memset(_rx_advert_total,   0, sizeof(_rx_advert_total));
  // Duty-Sliding-Window — symmetrisch zur simple_repeater-Logik. Wirkt
  // wie ein 'Duty-Reset bei Stats-Clear', der User hat damit nach
  // 'clear stats' wieder volle 10%/h verfuegbar (was auch unfair sein
  // koennte gegenueber dem Mesh, aber explizite User-Aktion).
  memset(_duty_air_ms_per_minute, 0, sizeof(_duty_air_ms_per_minute));
}

void MyMesh::pushCompanionMessage(const char* text) {
  if (_companion_channel_idx == 0xFF) return;
  if (text == NULL || text[0] == 0) return;

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
    time_t lt = (time_t)(now_rtc + (uint32_t)LOCAL_TZ_OFFSET_SECS);
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
  { "adverts", TRACE_ADVERTS, "eigene Adverts (periodic/nightly/manual)" },
  { "repeat",  TRACE_REPEAT,  "durchgereichte Packets" },
  { "scope",   TRACE_SCOPE,   "scope override/default/bake Wechsel" },
  { "motion",  TRACE_MOTION,  "_is_moving Uebergaenge" },
  { "heard",   TRACE_HEARD,   "neue Direct-heard Nodes (HeardList)" },
  { "rtc",     TRACE_RTC,     "detektierte RTC-Spruenge" },
  { "connect", TRACE_CONNECT, "BLE-App-Connect Events" },
  { "filter",  TRACE_FILTER,  "NICHT-repeatete Pakete + Grund (kann viele Zeilen erzeugen)" },
  { "night",    TRACE_NIGHT,    "Nightly-Flood Schedule + Scope-Auswahl" },
  { "duty",     TRACE_DUTY,     "Duty-Cycle Drops (Soft/Hard) ueber 10% TX/h" },
  { "msgstore", TRACE_MSGSTORE, "Offline-Queue Bucket-Save zu Flash ($companion ausgenommen)" },
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
    "help", "?", "status", "stats", "uptime", "advert", "autoadv",
    "repeater", "gps", "trace", "chatname", "reboot", "duty", "scope",
    "prefs", "neighbors", "tempradio", "set", "get", "clock", "date", "time",
    "messages", "clear",
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
      if (topic_prefix_match(topic, "neighbors")) {
        pushCompanionMessage(
          "neighbors: Liste der Contacts die in den letzten 48h via Advert "
          "gehoert wurden. Zeigt Typ (rep/cmp/room/sns), Name, Alter, Hops."
        );
        return;
      }
      if (topic_prefix_match(topic, "set")) {
        pushCompanionMessage(
          "set <key> <value>: persistente Settings setzen.");
        pushCompanionMessage(
          "Radio:  freq sf bw cr tx_power\n"
          "Position: lat lon gps gps_interval advert_loc_policy");
        pushCompanionMessage(
          "Repeat:   repeat flood_max scope_regional_hops\n"
          "          loop_detect (off|minimal|moderate|strict)\n"
          "Delays:   rxdelay txdelay direct_txdelay\n"
          "Telemetry: telemetry_mode_base loc env\n"
          "          airtime_factor rx_boosted_gain");
        pushCompanionMessage(
          "App:    name manual_add_contacts multi_acks autoadd_config\n"
          "        autoadd_max_hops path_hash_mode buzzer_quiet\n"
          "        owner_info (free-form, max 119 Zeichen, '|' -> Newline)");
        pushCompanionMessage(
          "Hinweise: Sued/West negativ (lat -10.5). freq MHz, bw kHz. "
          "Delays = Faktor*Airtime (tx/direct 0..2, rx 0..20). "
          "Aenderungen sofort applied + savePrefs.");
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
          "Keys: identisch zu 'set' (siehe 'help set'). Plus:\n"
          "  flood.max (alias flood_max), scope_regional_hops");
        return;
      }
      if (topic_prefix_match(topic, "clock") || topic_prefix_match(topic, "date")
          || topic_prefix_match(topic, "time")) {
        pushCompanionMessage(
          "clock / date / time (ohne Arg):\n"
          "  zeigt RTC (Unix-sec, UTC, lokal)."
        );
        pushCompanionMessage(
          "time <epoch>: setzt RTC.\n"
          "  Sanity-Check 1500000000..4000000000."
        );
        return;
      }
      if (topic_prefix_match(topic, "clear")) {
        pushCompanionMessage(
          "clear stats:\n"
          "  Setzt alle RAM-Statistik-Counter zurueck."
        );
        pushCompanionMessage(
          "'stats' muss voll ausgeschrieben werden (no_abbrev)\n"
          "-- Tippfehler wuerden Tests killen."
        );
        return;
      }
      if (topic_prefix_match(topic, "messages")) {
        pushCompanionMessage(
          "messages (no arg): Status pro Bucket.\n"
          "Typen: public, hashtag, private, dm, companion."
        );
        pushCompanionMessage(
          "messages flash <type> on|off\n"
          "  Flash-Persistenz toggle (default off)."
        );
        pushCompanionMessage(
          "messages limit <type> <N>\n"
          "  Slot-Limit setzen. 0 = type-Default.\n"
          "  Max: 16 fuer alle ausser DM (32)."
        );
        pushCompanionMessage(
          "messages clear <type|all>\n"
          "  RAM + Flash leeren."
        );
        return;
      }
      if (topic_prefix_match(topic, "tempradio")) {
        pushCompanionMessage(
          "tempradio <freq> <sf> <bw> <cr> [<tx_dbm>]: temporaere "
          "Funkparameter (nicht persistent, weg nach Reboot)."
        );
        pushCompanionMessage(
          "Range-Tests ohne savePrefs. Caveat: andere CLI-Befehle die "
          "savePrefs() machen wuerden die temp-Werte ins File schreiben."
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
        pushCompanionMessage(
          "repeater [on [force] | off]: schaltet client_repeat ein/aus. "
          "'on' prueft die Freq gegen einen strict-Range (z.B. 869.618 MHz "
          "NICHT enthalten)."
        );
        pushCompanionMessage(
          "'force' ueberspringt diesen Check; signalFitsInIsmBand "
          "bleibt aktiv. Force-Flag wird persistiert und beim App-'aus' "
          "gecleared. Ohne Arg -> Status."
        );
        pushCompanionMessage(
          "repeater profile [defensive | normal]: Filter-Tiefe.");
        pushCompanionMessage(
          "  defensive (Default):\n"
          "    PATH nur fuer lokale Endpoints,\n"
          "    Repeats mit reduzierter Power + CR5.");
        pushCompanionMessage(
          "  normal:\n"
          "    ALLE PATH-Pakete repeaten, volle Power +\n"
          "    konfigurierte CR (= echter Repeater).");
        return;
      }
      if (topic_prefix_match(topic, "status")) {
        pushCompanionMessage(
          "status: zeigt Firmware-Version, Uptime, GPS-Status, Position, "
          "Advert-Counter."
        );
        return;
      }
      if (topic_prefix_match(topic, "stats")) {
        pushCompanionMessage(
          "stats: detaillierte RX/Repeat/Airtime-Statistik. Aufschluesselung "
          "nach Node-Typ und Payload-Typ, plus /h und /d hochgerechnet."
        );
        pushCompanionMessage(
          "Direct-Pakete sind NICHT erfasst (nur Flood, der Hauptanteil "
          "des Mesh-Hintergrundtraffics). Counter sind RAM-only."
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
      pushCompanionMessage("(kein Help-Eintrag fuer dieses Topic)");
      return;
    }
    pushCompanionMessage(
      "Befehle: help [topic], status, stats, uptime, neighbors, advert, "
      "autoadv, repeater, duty, scope, gps, trace, chatname, prefs, "
      "set, get, clock, time, messages, tempradio, clear, reboot."
    );
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
    const char* zh = (_prefs.auto_advert_enabled & AUTO_ADV_ZEROHOP) ? "on" : "off";
    const char* nl = (_prefs.auto_advert_enabled & AUTO_ADV_NIGHTLY) ? "on" : "off";
    snprintf(line, sizeof(line),
             "autoadv: zerohop=%s nightly=%s  repeater=%s%s",
             zh, nl,
             _prefs.client_repeat ? "on" : "off",
             _prefs.client_repeat_force ? " (force)" : "");
    pushCompanionMessage(line);
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
        "    Type fest pinnen -- beeinflusst createSelfAdvert UND\n"
        "    Discovery-Query-Antworten (siehe Wunschliste 7).");
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
        if (!tv || *tv == 0) { pushCompanionMessage("Usage: advert role fixed chat|repeater|sensor|room"); return; }
        static const CompanionChoice rch[] = {
          { "chat",     false },  // -> 1
          { "repeater", false },  // -> 2
          { "sensor",   false },  // -> 3
          { "room",     false },  // -> 4
        };
        char rambig[40];
        int ri = match_choice(tv, rch, 4, rambig, sizeof(rambig));
        if (ri == -1) { char r[80]; snprintf(r, sizeof(r), "Mehrdeutig: %s", rambig); pushCompanionMessage(r); return; }
        if (ri < 0)   { pushCompanionMessage("Usage: advert role fixed chat|repeater|sensor|room"); return; }
        _prefs.advert_role = (uint8_t)(ri + 1);
        savePrefs();
        char r[120]; snprintf(r, sizeof(r),
          "OK - advert role = fixed %s",
          roleName(_prefs.advert_role));
        pushCompanionMessage(r);
        return;
      }
      pushCompanionMessage("Usage: advert role [auto | fixed chat|repeater|sensor|room]");
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
        time_t lt = (time_t)(now_rtc + (uint32_t)LOCAL_TZ_OFFSET_SECS);
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
      pushCompanionMessage("Usage: gps power [always-on | cycle | lead <N> | reset]");
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
      pushCompanionMessage("Usage: gps [on | off | sync | setloc | power ...]\n"
                           "ohne Arg -> Status");
    }
    return;
  }

  // ---------- tempradio <freq> <sf> <bw> <cr> [<tx_dbm>] ----------------
  // Temporaere Funk-Parameter (nicht persistent). Ueberschreibt _prefs
  // OHNE savePrefs - beim Reboot werden _prefs aus File geladen und die
  // tempradio-Werte sind weg. Caveat: andere CLI-Befehle die savePrefs()
  // aufrufen wuerden die tempradio-Werte ins File schreiben (-> Test
  // beenden bevor anderes geaendert wird, oder einfach rebooten).
  if (starts_with_word(cmd, "tempradio")) {
    // Argumente parsen: brauchen min. freq sf bw cr.
    const char* p = strchr(cmd, ' ');
    if (!p) { pushCompanionMessage("Usage: tempradio <freq_MHz> <sf> <bw_kHz> <cr> [<tx_dbm>]"); return; }
    while (*p == ' ') p++;
    // freq (float in MHz)
    float freq = atof(p);
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    // sf (int)
    int sf = atoi(p);
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    // bw (float in kHz)
    float bw = atof(p);
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    // cr (int)
    int cr = atoi(p);
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    // tx_dbm optional
    int tx = (*p) ? atoi(p) : (int)_prefs.tx_power_dbm;

    if (freq < 150.0f || freq > 2500.0f) {
      pushCompanionMessage("freq ausserhalb 150..2500 MHz");
      return;
    }
    if (sf < 5 || sf > 12) { pushCompanionMessage("sf ausserhalb 5..12"); return; }
    if (bw < 7.0f || bw > 500.0f) { pushCompanionMessage("bw ausserhalb 7..500 kHz"); return; }
    if (cr < 5 || cr > 8) { pushCompanionMessage("cr ausserhalb 5..8"); return; }
    if (tx < -9 || tx > MAX_LORA_TX_POWER) {
      char e[80]; snprintf(e, sizeof(e), "tx_dbm ausserhalb -9..%d", (int)MAX_LORA_TX_POWER);
      pushCompanionMessage(e);
      return;
    }

    // _prefs direkt ueberschreiben (KEIN savePrefs)
    _prefs.freq = freq;
    _prefs.sf = (uint8_t)sf;
    _prefs.bw = bw;
    _prefs.cr = (uint8_t)cr;
    _prefs.tx_power_dbm = (int8_t)tx;
    applyRadioPolicy();
    radio_set_tx_power((int8_t)tx);

    char line[160];
    snprintf(line, sizeof(line),
             "OK - tempradio: f=%.4f sf=%d bw=%.1f cr=%d tx=%d dBm "
             "(NICHT persistent, weg nach Reboot)",
             freq, sf, bw, cr, tx);
    pushCompanionMessage(line);
    return;
  }

  // ---------- neighbors -------------------------------------------------
  // Listet alle bekannten Contacts die wir in den letzten 48h ueber einen
  // Advert gehoert haben (direkt oder ueber Repeats). Sortierung in der
  // Reihenfolge wie die contacts[]-Tabelle aufgebaut ist (kein Sort um
  // Speicher/Zeit zu sparen).
  if (starts_with_word(cmd, "neighbors")) {
    uint32_t now = getRTCClock()->getCurrentTime();
    int num = getNumContacts();
    int shown = 0;

    // Akkumulierender Buffer wie bei prefs (mehrzeilig pro push).
    char buf[200];
    size_t buf_used = 0;
    auto flush = [&](bool force) {
      if (buf_used == 0) return;
      if (!force && buf_used < 130) return;
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

    add_line("neighbors (heard < 48h):");

    for (int i = 0; i < num; i++) {
      ContactInfo c;
      if (!getContactByIdx(i, c)) continue;
      if (c.lastmod == 0) continue;
      if (now - c.lastmod > CR_HEARD_MAX_AGE_SECS) continue;

      // Age formatieren (RTC-relativ).
      char age[16];
      uint32_t s = now - c.lastmod;
      if      (s < 60)     snprintf(age, sizeof(age), "%us", (unsigned)s);
      else if (s < 3600)   snprintf(age, sizeof(age), "%um", (unsigned)(s / 60));
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

      char line[160];
      snprintf(line, sizeof(line), "  %s %-18.18s %6s hops=%s",
               tname, c.name, age, hop);
      add_line(line);
      shown++;
    }

    char summary[80];
    snprintf(summary, sizeof(summary),
             "total: %d within 48h, %d known", shown, num);
    add_line(summary);
    flush(true);
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
      // repeat_scope_mode auf ALL (Default). Scope-User-Customizations
      // (scope_buildin_status + scope_extras) bleiben erhalten — User
      // soll sie nicht durch einen prefs-reset verlieren. Wer das auch
      // los werden will: 'scope <name> off|delete' pro Eintrag,
      // bzw. 'scope remove <name>' fuer Extras.
      _prefs.repeat_scope_mode = REPEAT_SCOPE_MODE_ALL;
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
    // client_repeat_force
    if (show_all || _prefs.client_repeat_force != 0) {
      snprintf(tmp, sizeof(tmp), "  client_repeat_force = %u%s",
               (unsigned)_prefs.client_repeat_force,
               _prefs.client_repeat_force == 0 ? " [default]" : " (default: 0)");
      add_line(tmp);
      if (_prefs.client_repeat_force != 0) non_default_count++;
    }
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
    // repeat_scope_mode (Liste B Policy)
    if (show_all || _prefs.repeat_scope_mode != REPEAT_SCOPE_MODE_ALL) {
      const char* mode = (_prefs.repeat_scope_mode == REPEAT_SCOPE_MODE_ALLOWLIST)
                         ? "allowlist" : "all";
      snprintf(tmp, sizeof(tmp), "  repeat_scope_mode = %s%s", mode,
               _prefs.repeat_scope_mode == REPEAT_SCOPE_MODE_ALL ? " [default]" : " (default: all)");
      add_line(tmp);
      if (_prefs.repeat_scope_mode != REPEAT_SCOPE_MODE_ALL) non_default_count++;
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
    // Lokal: now + LOCAL_TZ_OFFSET_SECS
    time_t local_t = (time_t)(now + (uint32_t)LOCAL_TZ_OFFSET_SECS);
    struct tm loc;
    gmtime_r(&local_t, &loc);
    char loc_str[40];
    snprintf(loc_str, sizeof(loc_str), "%04d-%02d-%02d %02d:%02d:%02d (TZ+%lds)",
             loc.tm_year + 1900, loc.tm_mon + 1, loc.tm_mday,
             loc.tm_hour, loc.tm_min, loc.tm_sec, (long)LOCAL_TZ_OFFSET_SECS);
    char block[200];
    snprintf(block, sizeof(block),
             "clock:\n  unix = %lu\n  utc  = %s\n  loc  = %s",
             (unsigned long)now, utc_str, loc_str);
    pushCompanionMessage(block);
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
      // Status-Output -- 5 Buckets, je eine Zeile, in 2 BLE-Messages
      // gesplittet (max 145 Zeichen).
      static const int caps[BUCKET_COUNT] = {
        BUCKET_CAP_PUBLIC, BUCKET_CAP_HASHTAG, BUCKET_CAP_PRIVATE,
        BUCKET_CAP_DM,     BUCKET_CAP_COMPANION
      };
      char head[200];
      snprintf(head, sizeof(head),
               "messages (offline-queue):\n  bucket  used limit cap flash");
      pushCompanionMessage(head);
      for (int b = 0; b < BUCKET_COUNT; b++) {
        Frame* arr = NULL;
        int cap_unused = 0;
        getBucket((MsgBucket)b, arr, cap_unused);
        int used = 0;
        for (int i = 0; i < caps[b]; i++) if (arr[i].seq_no) used++;
        char line[120];
        snprintf(line, sizeof(line),
                 "  %-9s %3d %5d %3d %s",
                 bucketName((MsgBucket)b),
                 used, getBucketLimit((MsgBucket)b), caps[b],
                 getBucketFlash((MsgBucket)b) ? "on" : "off");
        pushCompanionMessage(line);
      }
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
          pushCompanionMessage("Usage: messages flash <type> on|off");
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
    char key[24];
    if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
    memcpy(key, key_start, key_len);
    key[key_len] = 0;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) {
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

    // -- set owner_info <text> (case-sensitiv, raw_cmd, lange Strings) --
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
      radio_set_tx_power(_prefs.tx_power_dbm);
      char r[40]; snprintf(r, sizeof(r), "OK - tx_power = %d dBm", v);
      pushCompanionMessage(r);
      return;
    }

    // Repeater-style Delay-Knöpfe (analog simple_repeater CommonCLI).
    // txdelay/rxdelay sind Faktoren multipliziert mit Pkt-Airtime; siehe
    // getRetransmitDelay() / calcRxDelay().
    if (strcmp(key, "txdelay") == 0 || strcmp(key, "rxdelay") == 0
        || strcmp(key, "direct_txdelay") == 0) {
      float v = (float)atof(value_lc);
      if (v < 0.0f || v > 20.0f) {
        pushCompanionMessage("Wert ausserhalb 0..20.0");
        return;
      }
      if (strcmp(key, "rxdelay") == 0) {
        _prefs.rx_delay_base = v;
        savePrefs();
        char r[60]; snprintf(r, sizeof(r), "OK - rxdelay = %.3f", v);
        pushCompanionMessage(r);
      } else if (strcmp(key, "txdelay") == 0) {
        if (v > 2.0f) { pushCompanionMessage("txdelay max 2.0"); return; }
        if (v == 0.0f) v = 0.5f;   // 0 wird als uninit gewertet, siehe begin()
        _prefs.tx_delay_factor = v;
        savePrefs();
        char r[60]; snprintf(r, sizeof(r), "OK - txdelay = %.3f", v);
        pushCompanionMessage(r);
      } else {  // direct_txdelay
        if (v > 2.0f) { pushCompanionMessage("direct_txdelay max 2.0"); return; }
        if (v == 0.0f) v = 0.2f;
        _prefs.direct_tx_delay_factor = v;
        savePrefs();
        char r[60]; snprintf(r, sizeof(r), "OK - direct_txdelay = %.3f", v);
        pushCompanionMessage(r);
      }
      return;
    }

    // Hop-Cap fuer #region / #regional. Range 1..flood_max. 0 wird in
    // begin() als uninitialisiert auf 3 normalisiert.
    if (strcmp(key, "scope_regional_hops") == 0) {
      int v = atoi(value_lc);
      if (v < 1 || v > _prefs.flood_max) {
        char r[80]; snprintf(r, sizeof(r),
          "Wert ausserhalb 1..%u (flood_max-Cap)", (unsigned)_prefs.flood_max);
        pushCompanionMessage(r);
        return;
      }
      _prefs.scope_regional_hop_limit = (uint8_t)v;
      savePrefs();
      char r[60]; snprintf(r, sizeof(r), "OK - scope_regional_hops = %d", v);
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

    // Globale Repeat-Hop-Obergrenze. Range 1..64 analog CommonCLI flood.max.
    // 'flood.max' (CommonCLI-Stil) als Alias erlaubt.
    if (strcmp(key, "flood_max") == 0 || strcmp(key, "flood.max") == 0) {
      int v = atoi(value_lc);
      if (v < 1 || v > 64) {
        pushCompanionMessage("Wert ausserhalb 1..64");
        return;
      }
      _prefs.flood_max = (uint8_t)v;
      // Wenn scope_regional_hop_limit jetzt drueber liegt: nach unten ziehen
      // damit die Beziehung gilt (regional <= flood_max).
      if (_prefs.scope_regional_hop_limit > _prefs.flood_max) {
        _prefs.scope_regional_hop_limit = _prefs.flood_max;
      }
      savePrefs();
      char r[80]; snprintf(r, sizeof(r),
        "OK - flood_max = %d (scope_regional_hops auf %u gecapped falls drueber)",
        v, (unsigned)_prefs.scope_regional_hop_limit);
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
    char key[24];
    key[0] = 0;
    if (p) {
      while (*p == ' ') p++;
      if (*p) {
        size_t klen = 0;
        while (p[klen] && p[klen] != ' ' && p[klen] != '\t' && klen < 23) klen++;
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
      emit_float ("rxdelay",             _prefs.rx_delay_base,         0.0f,                   "",     3);
      emit_float ("txdelay",             _prefs.tx_delay_factor,       0.5f,                   "",     3);
      emit_float ("direct_txdelay",      _prefs.direct_tx_delay_factor,0.2f,                   "",     3);
      emit_uint  ("scope_regional_hops", _prefs.scope_regional_hop_limit, 3);
      emit_uint  ("flood_max",           _prefs.flood_max,             16);
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
    else if (strcmp(key, "rxdelay") == 0)           snprintf(r, sizeof(r), "rxdelay = %.3f", _prefs.rx_delay_base);
    else if (strcmp(key, "txdelay") == 0)           snprintf(r, sizeof(r), "txdelay = %.3f", _prefs.tx_delay_factor);
    else if (strcmp(key, "direct_txdelay") == 0)    snprintf(r, sizeof(r), "direct_txdelay = %.3f", _prefs.direct_tx_delay_factor);
    else if (strcmp(key, "scope_regional_hops") == 0) snprintf(r, sizeof(r), "scope_regional_hops = %u", (unsigned)_prefs.scope_regional_hop_limit);
    else if (strcmp(key, "flood_max") == 0 || strcmp(key, "flood.max") == 0) snprintf(r, sizeof(r), "flood_max = %u", (unsigned)_prefs.flood_max);
    else if (strcmp(key, "owner_info") == 0 || strcmp(key, "owner.info") == 0) snprintf(r, sizeof(r), "owner_info = %s", _prefs.owner_info[0] ? _prefs.owner_info : "(leer)");
    else if (strcmp(key, "loop_detect") == 0 || strcmp(key, "loop.detect") == 0) {
      const char* nm = (_prefs.loop_detect == 0) ? "off" : (_prefs.loop_detect == 1) ? "minimal" : (_prefs.loop_detect == 2) ? "moderate" : "strict";
      snprintf(r, sizeof(r), "loop_detect = %s", nm);
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
  // ---------- clear stats ----------------------------------------------
  // Setzt RAM-Statistik-Counter zurueck. Analog zu simple_repeater
  // clearStats(). 'stats' ist no_abbrev — Tippfehler wuerden alle
  // Test-Counter killen.
  if (starts_with_word(cmd, "clear")) {
    const char* arg = strchr(cmd, ' ');
    if (arg) { while (*arg == ' ') arg++; }
    if (!arg || strcmp(arg, "stats") != 0) {
      pushCompanionMessage("Usage: clear stats\n"
                           "no_abbrev - 'stats' muss voll ausgeschrieben sein");
      return;
    }
    clearStats();
    pushCompanionMessage("OK - alle Statistik-Counter zurueckgesetzt.");
    return;
  }

  // D5: 'stat' (Abbr) als Alias zu 'stats' akzeptieren. 'status' ist ein
  // anderer Befehl (siehe oben), aber 'stat' faellt aus dessen Match
  // raus (text[4]=NUL/space) und kommt erst hier vorbei -- spart
  // Tipparbeit.
  if (starts_with_word(cmd, "stats") || starts_with_word(cmd, "stat")) {
    uint64_t total_ms = (uint64_t)_millis_wraps * 4294967296ULL + (uint64_t)millis();
    if (total_ms == 0) total_ms = 1;
    uint64_t uptime_s = total_ms / 1000ULL;
    if (uptime_s == 0) uptime_s = 1;
    char block[200];
    int p;

    // ---- Msg 1: heard direct nodes ----
    uint32_t hd_total = 0;
    for (int t = 0; t < 5; t++) hd_total += _heard_direct[t];
    p = snprintf(block, sizeof(block),
                 "heard direct nodes:\n"
                 "  rep=%u cmp=%u room=%u sns=%u\n"
                 "  total=%lu",
                 (unsigned)_heard_direct[ADV_TYPE_REPEATER],
                 (unsigned)_heard_direct[ADV_TYPE_CHAT],
                 (unsigned)_heard_direct[ADV_TYPE_ROOM],
                 (unsigned)_heard_direct[ADV_TYPE_SENSOR],
                 (unsigned long)hd_total);
    append_rate_hint(block + p, sizeof(block) - p, hd_total, uptime_s);
    pushCompanionMessage(block);

    // ---- Msg 2: heard direct qual (SNR gut/mittel/schlecht) ----
    // Alle 4 Typen werden immer angezeigt (auch mit 0/0/0), damit klar ist
    // dass die Auswertung greift selbst wenn ein Typ noch nicht aufgetaucht
    // ist. Schwellen Q4: gut >= 0 dB, mittel >= -8 dB, schlecht < -8 dB.
    snprintf(block, sizeof(block),
             "heard direct qual good/med/bad:\n"
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

    // ---- Msg 3: rx adv (all hops) ----
    uint32_t ad_total = 0;
    for (int t = 0; t < 5; t++) ad_total += _rx_advert_total[t];
    p = snprintf(block, sizeof(block),
                 "rx adv:\n"
                 "  all: rep=%u cmp=%u room=%u sns=%u\n"
                 "  total=%lu",
                 (unsigned)_rx_advert_total[ADV_TYPE_REPEATER],
                 (unsigned)_rx_advert_total[ADV_TYPE_CHAT],
                 (unsigned)_rx_advert_total[ADV_TYPE_ROOM],
                 (unsigned)_rx_advert_total[ADV_TYPE_SENSOR],
                 (unsigned long)ad_total);
    append_rate_hint(block + p, sizeof(block) - p, ad_total, uptime_s);
    pushCompanionMessage(block);

    // ---- Msg 4: rx flood (alle Pakettypen, nur Flood-Forward-Pfad) ----
    uint32_t rxf_total = 0;
    for (int pp = 0; pp < 16; pp++) rxf_total += _rx_flood_by_ptype[pp];
    p = snprintf(block, sizeof(block),
                 "rx flood:\n"
                 "  adv=%u path=%u txt=%u grp=%u ack=%u req=%u rsp=%u trc=%u\n"
                 "  total=%lu",
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_ADVERT],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_PATH],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_TXT_MSG],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_GRP_TXT],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_ACK],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_REQ],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_RESPONSE],
                 (unsigned)_rx_flood_by_ptype[PAYLOAD_TYPE_TRACE],
                 (unsigned long)rxf_total);
    append_rate_hint(block + p, sizeof(block) - p, rxf_total, uptime_s);
    pushCompanionMessage(block);

    // ---- Msg 5: tx own (eigene = total - repeated pro Pakettyp) ----
    auto own_of = [&](uint8_t pp) -> unsigned {
      uint16_t tot = _tx_total_by_ptype[pp];
      uint16_t rep = _repeat_by_ptype[pp];
      return (tot > rep) ? (unsigned)(tot - rep) : 0u;
    };
    uint32_t own_total = 0;
    for (int pp = 0; pp < 16; pp++) own_total += own_of((uint8_t)pp);
    p = snprintf(block, sizeof(block),
                 "tx own packets:\n"
                 "  adv=%u path=%u txt=%u grp=%u ack=%u req=%u rsp=%u trc=%u\n"
                 "  total=%lu",
                 own_of(PAYLOAD_TYPE_ADVERT),
                 own_of(PAYLOAD_TYPE_PATH),
                 own_of(PAYLOAD_TYPE_TXT_MSG),
                 own_of(PAYLOAD_TYPE_GRP_TXT),
                 own_of(PAYLOAD_TYPE_ACK),
                 own_of(PAYLOAD_TYPE_REQ),
                 own_of(PAYLOAD_TYPE_RESPONSE),
                 own_of(PAYLOAD_TYPE_TRACE),
                 (unsigned long)own_total);
    append_rate_hint(block + p, sizeof(block) - p, own_total, uptime_s);
    pushCompanionMessage(block);

    // ---- Msg 6+7: tx own repeated + tx own total — nur wenn aktiv ----
    uint32_t rep_total = 0;
    for (int pp = 0; pp < 16; pp++) rep_total += _repeat_by_ptype[pp];
    if (_prefs.client_repeat != 0) {
      p = snprintf(block, sizeof(block),
                   "tx own repeated:\n"
                   "  adv=%u path=%u txt=%u grp=%u ack=%u req=%u rsp=%u trc=%u\n"
                   "  total=%lu",
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_ADVERT],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_PATH],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_TXT_MSG],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_GRP_TXT],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_ACK],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_REQ],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_RESPONSE],
                   (unsigned)_repeat_by_ptype[PAYLOAD_TYPE_TRACE],
                   (unsigned long)rep_total);
      append_rate_hint(block + p, sizeof(block) - p, rep_total, uptime_s);
      pushCompanionMessage(block);

      uint32_t tx_grand_total = own_total + rep_total;
      p = snprintf(block, sizeof(block),
                   "tx own total:\n"
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
             "  tx=%s (%.2f%%) own=%s (%.2f%%) repeat=%s (%.2f%%)",
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
      savePrefs();
      char r[80]; snprintf(r, sizeof(r), "OK - trace %s an.", trace_cats[tc_idx].name);
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
      const char* active_val;
      const char* active_legend;   // NULL = kein Legende-Zeile noetig
      if (override_active)         { active_val = "override";     active_legend = NULL; }
      else if (bake_set)           { active_val = "bake";         active_legend = "flooded advert, nightly"; }
      else if (geo_wins_default)   { active_val = "geo-fallback"; active_legend = "Geo gewinnt vor Default (auto=prefer)"; }
      else if (default_set)        { active_val = "default";      active_legend = "configured catchall scope"; }
      else if (geo_is_fallback)    { active_val = "geo-fallback"; active_legend = "kein Default gesetzt"; }
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
      char head[200];
      int hlen = snprintf(head, sizeof(head),
                          "scope advert (send hierarchy):\n  %s\n  %s\n  %s",
                          def_line, bake_line, ovr_line);
      if (hlen < 145) {
        pushCompanionMessage(head);
      } else {
        pushCompanionMessage("scope advert (send hierarchy):");
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
    char scope_ambig[80];
    int sub_idx = match_choice(arg, scope_subs,
                               (int)(sizeof(scope_subs)/sizeof(scope_subs[0])),
                               scope_ambig, sizeof(scope_ambig));
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

      // Sentinel-Schutz: #local-discard ist ein Sentinel-Scope und darf
      // NICHT modifiziert werden. Nur 'info' ist erlaubt -- alles andere
      // (enable/disable/delete/undelete/pin/auto/off/rep/adv) wuerde die
      // Sentinel-Semantik kaputt machen (s. Wunschliste 14 +
      // allowPacketForward local-discard-Hard-Block).
      if (ref.storage == SCOPE_BUILDIN && strcmp(name, "local-discard") == 0
          && aidx != 6 /* info */) {
        pushCompanionMessage(
          "#local-discard ist Sentinel — nicht modifizierbar.\n"
          "  Erlaubt: nur 'scope local-discard info'.");
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
      if (strcmp(sub, "clear") == 0) {
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
      if (strcmp(sub, "clear") == 0) {
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
      if (strcmp(sub, "clear") == 0) {
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
        "  ! = Eigenes geo auto-Advert nimmt diesen Scope nie.");
      pushCompanionMessage(
        "      Repeat-Verhalten bleibt unberuehrt.");
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
      // Sentinel-Schutz: local-discard nicht entfernbar (Wunschliste 14).
      if (ref.storage == SCOPE_BUILDIN && strcmp(name, "local-discard") == 0) {
        pushCompanionMessage(
          "#local-discard ist Sentinel — nicht entfernbar.");
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
            "Legende: (A)=aktiv (auto Bbox),\n"
            "(A-)=inaktiv (auto Bbox),\n"
            "(P)=pin (immer aktiv), (D)=disabled\n"
            "Umschalten: scope <name> pin | auto | off\n"
            "Hilfe: 'scope repeater ?' fuer Sub-Befehle.");
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
      char line[160];
      // C1: Multi-Line statt einer langen Zeile (User-Wunsch).
      // Wunschliste 6b: loop_detect mit anzeigen wenn != off.
      const char* ld = (_prefs.loop_detect == 0) ? "off"
                     : (_prefs.loop_detect == 1) ? "minimal"
                     : (_prefs.loop_detect == 2) ? "moderate" : "strict";
      snprintf(line, sizeof(line),
               "repeater=%s%s\n"
               "profile=%s\n"
               "loop_detect=%s%s\n"
               "freq=%.4f MHz\n"
               "strict_ok=%s",
               _prefs.client_repeat ? "on" : "off",
               _prefs.client_repeat_force ? " (force)" : "",
               _prefs.repeater_profile == 1 ? "normal" : "defensive",
               ld,
               (_prefs.repeater_profile != 1 && _prefs.loop_detect != 0)
                   ? " (inaktiv -- profile=defensive)" : "",
               _prefs.freq, strict_ok ? "yes" : "no");
      pushCompanionMessage(line);
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
        char r[200];
        snprintf(r, sizeof(r),
          "repeater profile = %s\n"
          "  defensive: PATH nur fuer lokale Endpoints, Repeats mit\n"
          "             reduzierter Power + CR5 (= client-Repeater).\n"
          "  normal:    ALLE PATH-Pakete weiterleiten, volle Power +\n"
          "             konfigurierte CR (= wie echter Repeater).",
          _prefs.repeater_profile == 1 ? "normal" : "defensive");
        pushCompanionMessage(r);
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
      char r[160];
      snprintf(r, sizeof(r),
        "OK - repeater profile = %s.\n"
        "  PATH-Filter: %s\n"
        "  Power/CR-Reduktion bei Repeats: %s",
        pm == 1 ? "normal" : "defensive",
        pm == 1 ? "AUS (alle PATH weiterleiten)"
                : "AN (nur lokale Endpoints)",
        pm == 1 ? "AUS (volle Power + konfigurierte CR)"
                : "AN (reduziert + CR5)");
      pushCompanionMessage(r);
      return;
    }

    int rm = match_on_off(arg);
    if (rm == -1) { pushCompanionMessage("Mehrdeutig: on off"); return; }
    if (rm == 0) {
      _prefs.client_repeat = 0;
      _prefs.client_repeat_force = 0;  // Force-Modus mit "off" beenden
      savePrefs();
      pushCompanionMessage("OK - repeater off.");
      return;
    }
    if (rm == 1) {
      // "force"-Keyword erkennen (iOS-Tastatur macht aus "--force" einen
      // em-dash — daher ein einzelnes lowercase Wort statt Doppel-Hyphen).
      bool force = false;
      const char* rest = arg;
      while (*rest && *rest != ' ' && *rest != '\t') rest++;  // skip on-Prefix
      while (*rest == ' ' || *rest == '\t') rest++;
      if (starts_with_word(rest, "force")) force = true;
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
      // Sicherheitsgate 2: strict-Range (nur ohne force)
      if (!force && !isValidClientRepeatFreq(f_khz)) {
        char line[160];
        snprintf(line, sizeof(line),
                 "Abgelehnt: %.4f MHz nicht im strict-Range. "
                 "Mit 'repeater on force' trotzdem aktivieren.", _prefs.freq);
        pushCompanionMessage(line);
        return;
      }
      _prefs.client_repeat = 1;
      _prefs.client_repeat_force = force ? 1 : 0;
      savePrefs();
      char line[80];
      snprintf(line, sizeof(line), "OK - repeater on%s.", force ? " (force)" : "");
      pushCompanionMessage(line);
      return;
    }
    pushCompanionMessage("Usage: repeater [on [force] | off]");
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
      char line[120];
      snprintf(line, sizeof(line), "autoadv: zerohop=%s  nightly=%s",
               (_prefs.auto_advert_enabled & AUTO_ADV_ZEROHOP) ? "on" : "off",
               (_prefs.auto_advert_enabled & AUTO_ADV_NIGHTLY) ? "on" : "off");
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
  char buf[160];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n <= 0) return;
  if (n >= (int)sizeof(buf)) n = sizeof(buf) - 1;

  // Always to Serial -- jede '\n' im Buffer in '\r\n' uebersetzen, sonst
  // bleibt der Cursor in seriellen Terminals (USB-Serial, putty, screen)
  // in der vorherigen Spalte stehen und die Folge-Zeile haengt rechts
  // angeklatscht raus statt am linken Rand zu beginnen. Funktioniert
  // auch bei multiline-Logs (mehrere \n im Buffer). Doppel-CR vermeiden
  // falls vor einem \n schon ein \r steht.
  for (int i = 0; i < n; i++) {
    if (buf[i] == '\n' && (i == 0 || buf[i - 1] != '\r')) {
      Serial.write('\r');
    }
    Serial.write(buf[i]);
  }
  if (buf[n - 1] != '\n') {
    Serial.write('\r');
    Serial.write('\n');
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
