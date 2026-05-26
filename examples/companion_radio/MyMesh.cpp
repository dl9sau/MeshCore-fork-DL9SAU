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

void MyMesh::addToOfflineQueue(const uint8_t frame[], int len) {
  if (offline_queue_len >= OFFLINE_QUEUE_SIZE) {
    MESH_DEBUG_PRINTLN("WARN: offline_queue is full!");
    int pos = 0;
    while (pos < offline_queue_len) {
      if (offline_queue[pos].isChannelMsg()) {
        for (int i = pos; i < offline_queue_len - 1; i++) { // delete oldest channel msg from queue
          offline_queue[i] = offline_queue[i + 1];
        }
        MESH_DEBUG_PRINTLN("INFO: removed oldest channel message from queue.");
        offline_queue[offline_queue_len - 1].len = len;
        memcpy(offline_queue[offline_queue_len - 1].buf, frame, len);
        return;
      }
      pos++;
    }
    MESH_DEBUG_PRINTLN("INFO: no channel messages to remove from queue.");
  } else {
    offline_queue[offline_queue_len].len = len;
    memcpy(offline_queue[offline_queue_len].buf, frame, len);
    offline_queue_len++;
  }
}

int MyMesh::getFromOfflineQueue(uint8_t frame[]) {
  if (offline_queue_len > 0) {         // check offline queue
    size_t len = offline_queue[0].len; // take from top of queue
    memcpy(frame, offline_queue[0].buf, len);

    offline_queue_len--;
    for (int i = 0; i < offline_queue_len; i++) { // delete top item from queue
      offline_queue[i] = offline_queue[i + 1];
    }
    return len;
  }
  return 0; // queue is empty
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
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * 0.5f);
  return getRNG()->nextInt(0, 5*t + 1);
}
uint32_t MyMesh::getDirectRetransmitDelay(const mesh::Packet *packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * 0.2f);
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
bool MyMesh::shouldReduceFloodRetransmit(const mesh::Packet* packet, uint8_t n) const {
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
    _ui->newMsg(path_len, from.name, text, offline_queue_len);
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

  // path-length cap (hop count, not byte length)
  if (packet->getPathHashCount() > CR_MAX_REPEAT_PATH_LEN) {
    reject_reason = "path-too-long";
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
  } else if (ptype == PAYLOAD_TYPE_PATH) {
    // PATH discovery: only repeat for local nodes (heard < 48h OR known contact < 48h)
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
  TransportKey default_scope;
  memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));

  auto scope = send_scope.isNull() ? &default_scope : &send_scope;
  sendFloodScoped(*scope, pkt, delay_millis);
}
void MyMesh::sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis) {
  // TODO: have per-channel send_scope
  TransportKey default_scope;
  memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));

  auto scope = send_scope.isNull() ? &default_scope : &send_scope;
  sendFloodScoped(*scope, pkt, delay_millis);
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
  if (_ui) _ui->newMsg(path_len, channel_name, effective_text, offline_queue_len);
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

uint8_t MyMesh::onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                                 uint8_t len, uint8_t *reply) {
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
  offline_queue_len = 0;
  app_target_ver = 0;
  clearPendingReqs();
  next_ack_idx = 0;
  sign_data = NULL;
  dirty_contacts_expiry = 0;
  memset(advert_paths, 0, sizeof(advert_paths));
  memset(send_scope.key, 0, sizeof(send_scope.key));
  memset(heard_list, 0, sizeof(heard_list));
  heard_next_idx = 0;
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
        TransportKey default_scope;
        memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));
        sendFloodScoped(default_scope, pkt, delay_millis);
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
      if (_ui) _ui->msgRead(offline_queue_len);
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

// Geo bounding boxes (inclusive). Adjust per-build with #defines if needed.
#ifndef CR_BBOX_BEBB_LAT_MIN
#define CR_BBOX_BEBB_LAT_MIN  51.40
#define CR_BBOX_BEBB_LAT_MAX  53.60
#define CR_BBOX_BEBB_LON_MIN  11.20
#define CR_BBOX_BEBB_LON_MAX  14.80
#endif
#ifndef CR_BBOX_OSTFR_LAT_MIN
#define CR_BBOX_OSTFR_LAT_MIN 53.10
#define CR_BBOX_OSTFR_LAT_MAX 53.80
#define CR_BBOX_OSTFR_LON_MIN  6.50
#define CR_BBOX_OSTFR_LON_MAX  8.50
#endif

bool MyMesh::chooseGeoFallbackScope(TransportKey& out_key) const {
  double lat, lon;
  if (!getEffectiveLatLon(lat, lon)) return false;

  const char* tag = NULL;
  if (lat >= CR_BBOX_BEBB_LAT_MIN && lat <= CR_BBOX_BEBB_LAT_MAX &&
      lon >= CR_BBOX_BEBB_LON_MIN && lon <= CR_BBOX_BEBB_LON_MAX) {
    tag = "#bebb";
  } else if (lat >= CR_BBOX_OSTFR_LAT_MIN && lat <= CR_BBOX_OSTFR_LAT_MAX &&
             lon >= CR_BBOX_OSTFR_LON_MIN && lon <= CR_BBOX_OSTFR_LON_MAX) {
    tag = "#ostfriesland";
  }
  if (tag == NULL) return false;

  TransportKeyStore tmp;
  tmp.getAutoKeyFor(0, tag, out_key);
  return true;
}

bool MyMesh::chooseNightFloodScope(TransportKey& out_key) const {
  // Neue 4-stufige Hierarchie:
  //   1) override (persistent, expiry-basiert)
  //   2) bake-scope (persistent, explizit fuer nightly)
  //   3) default-scope (persistent, fuer regulaere Sends)
  //   4) geo-fallback (Position-basiert)
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
  // 3) default-scope
  TransportKey configured;
  memcpy(configured.key, _prefs.default_scope_key, sizeof(configured.key));
  if (!configured.isNull()) {
    out_key = configured;
    return true;
  }
  // 4) geo fallback
  return chooseGeoFallbackScope(out_key);
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

  // If we're already past today's window, schedule for tomorrow.
  if (local_now >= window_end) {
    window_start += 86400UL;
    window_end   += 86400UL;
  } else if (local_now >= window_start) {
    // we're inside the window now; pick any future second within remaining window
    window_start = local_now + 1;
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
    traceCompanion(TRACE_GPS, "[gps] first fix erkannt");
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
      traceCompanion(TRACE_MOTION, "[motion] %s (Anker-Distanz %d m)",
                     _is_moving ? "moving" : "static", (int)d_m);
    }
    // Movement just started — accelerate the next advert so a fresh
    // position goes out promptly, instead of waiting out the static
    // (1h) slot we may currently be on.
    if (!was_moving && _is_moving) {
      next_periodic_advert_at = millis();
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
    traceCompanion(TRACE_ADVERTS, "[adv] periodic zero-hop moving=%d",
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
    traceCompanion(TRACE_ADVERTS, "[adv] nightly-flood (3B path)");
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
    traceCompanion(TRACE_GPS, "[gps] wake (until_advert=%lds)", until_advert / 1000);
  } else if (!want_gps_on && gps_is_on) {
    sensors.setSettingValue("gps", "0");
    _gps_woke_at_millis = 0;
    _gps_off_at_millis = (now == 0 ? 1 : now);
    _gps_fix_seen_this_wake = false;
    pushDebugLog("[GPS-DBG] sleep at millis=%lu (until_advert=%lds, fix_was_seen=1)\n",
                  now, until_advert / 1000);
    traceCompanion(TRACE_GPS, "[gps] sleep (fix_seen=%d)", (int)_gps_fix_seen_this_wake);
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

  // Koordinaten im nautischen DM-Format "DD-MM,M N/S DDD-MM,M E/W"
  // (Komma als Dezimal-Trenner, Grad-Breite 2 für Lat, 3 für Lon mit
  // führenden Nullen). Z.B. lat=54.0767 lon=6.7467 -> "54-04,6N 006-44,8E".
  auto fmt_dm = [](char* out, size_t out_size, double v, int deg_width, char pos, char neg) {
    char hemi = (v >= 0) ? pos : neg;
    double a = fabs(v);
    int deg = (int)a;
    double rem_min = (a - deg) * 60.0;
    int min_int = (int)rem_min;
    int min_frac = (int)((rem_min - min_int) * 10.0 + 0.5);
    if (min_frac >= 10) { min_frac = 0; min_int++; }
    if (min_int >= 60)  { min_int = 0;  deg++; }
    snprintf(out, out_size, "%0*d-%02d,%d%c", deg_width, deg, min_int, min_frac, hemi);
  };
  char lat_dm[16], lon_dm[16];
  fmt_dm(lat_dm, sizeof(lat_dm), lat, 2, 'N', 'S');
  fmt_dm(lon_dm, sizeof(lon_dm), lon, 3, 'E', 'W');

  pushDebugLog("[GEO-SCOPE] %s %s -> %s", lat_dm, lon_dm, buf);
  // Zusätzlich im Companion-Channel anzeigen, damit die Info auch bei
  // verbundener App sichtbar wird (nicht nur im Debug-Protokoll-View).
  char chat[256];
  snprintf(chat, sizeof(chat), "GEO-SCOPE @ %s %s: %s", lat_dm, lon_dm, buf);
  pushCompanionMessage(chat);
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
  { "night",   TRACE_NIGHT,   "Nightly-Flood Schedule + Scope-Auswahl" },
  { "duty",    TRACE_DUTY,    "Duty-Cycle Drops (Soft/Hard) ueber 10% TX/h" },
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
    "prefs", "neighbors", "tempradio",
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
          "Ohne Arg -> Status (state, fix_ever, moving, pos)."
        );
        pushCompanionMessage(
          "gps power [always-on | cycle | lead <N> | reset]: "
          "Power-Management. Default cycle, lead=5 -> Sleep=10 im 15-min-Cycle."
        );
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
      if (topic_prefix_match(topic, "scope")) {
        pushCompanionMessage(
          "scope steuert NUR die nightly bake / 'advert flood' Reichweite, "
          "nicht regulaere Sends. Hierarchie: override > bake > default > geo."
        );
        pushCompanionMessage(
          "scope                              -> Status aller 4 Quellen. "
          "scope default <name>|clear         -> persistent (auch fuer normale Sends als Fallback)."
        );
        pushCompanionMessage(
          "scope bake <name>|clear            -> persistent, NUR nightly. "
          "Bewusst weiter als default moeglich (z.B. default=#de-be, bake=#de-bebb)."
        );
        pushCompanionMessage(
          "scope override <name> [12h|3d]|clear -> persistent ueber Reboots "
          "(default 12h, max 30d). Suffix h oder d. Hoechste Prio."
        );
        return;
      }
      if (topic_prefix_match(topic, "neighbors")) {
        pushCompanionMessage(
          "neighbors: Liste der Contacts die in den letzten 48h via Advert "
          "gehoert wurden. Zeigt Typ (rep/cmp/room/sns), Name, Alter, Hops."
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
          "prefs        -> nur Non-Default-Werte. "
          "prefs all    -> alle mit [default]-Markierung. "
          "prefs reset  -> alle DL9SAU-Vars auf Default."
        );
        return;
      }
      if (topic_prefix_match(topic, "duty")) {
        pushCompanionMessage(
          "duty: Duty-Cycle-Schutz (10% TX-Airtime pro rollendem 1h-Fenster). "
          "Ohne Arg -> Status (stats, blocked, soft/hard-Limits)."
        );
        pushCompanionMessage(
          "duty soft N (0..99): Repeats ab N% droppen. duty hard N (1..100): "
          "ALLE TX ab N% droppen. duty reset -> Default 80/100. "
          "Trace-Kategorie 'duty'."
        );
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
          "Active (RAM, reset bei Reboot) + Persistent (User-Selektion)."
        );
        pushCompanionMessage(
          "list = Kategorien-Uebersicht. <cat> on/off = setzt bit in beiden. "
          "on = active wird persistent wiederhergestellt. off = pausiert."
        );
        pushCompanionMessage(
          "all on/off = beide auf alle/keine. Ohne Arg -> Status (active + ggf. persistent). "
          "Nach Reboot: active = 0, 'trace on' aktiviert die Selektion wieder."
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
      "autoadv, repeater, duty, scope, gps, trace, chatname, prefs, reboot."
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
    snprintf(line, sizeof(line),
             "gps=%s fix_ever=%d moving=%d  pos=%.4f,%.4f",
             _prefs.gps_enabled ? "on" : "off",
             (int)_gps_had_fix_ever, (int)_is_moving,
             sensors.node_lat, sensors.node_lon);
    pushCompanionMessage(line);
    const char* zh = (_prefs.auto_advert_enabled & AUTO_ADV_ZEROHOP) ? "on" : "off";
    const char* nl = (_prefs.auto_advert_enabled & AUTO_ADV_NIGHTLY) ? "on" : "off";
    snprintf(line, sizeof(line),
             "autoadv: zerohop=%s nightly=%s  repeater=%s%s",
             zh, nl,
             _prefs.client_repeat ? "on" : "off",
             _prefs.client_repeat_force ? "(force)" : "");
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

    bool want_flood = false;
    if (arg && *arg) {
      if (strcmp(arg, "flood") == 0 || strcmp(arg, "f") == 0) {
        want_flood = true;
      } else if (strcmp(arg, "zero-hop") == 0 || strcmp(arg, "zerohop") == 0
                 || strcmp(arg, "z") == 0) {
        want_flood = false;
      } else {
        pushCompanionMessage("Usage: advert [zero-hop | flood]  (Aliase: z, f)");
        return;
      }
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
      char block[200];
      snprintf(block, sizeof(block),
               "gps=%s  fix_ever=%d  moving=%d  app-poll-interval=%s\n"
               "\n"
               "pos=%.4f,%.4f",
               state, (int)_gps_had_fix_ever, (int)_is_moving,
               interval_str,
               sensors.node_lat, sensors.node_lon);
      pushCompanionMessage(block);
      return;
    }
    // ---- gps power [...] - Power-Management-Konfig ----
    if (starts_with_word(arg, "power")) {
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
          pushCompanionMessage("Usage: gps power lead <N>  (1..14 Minuten)");
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

    if (starts_with_word(arg, "on")) {
      _prefs.gps_enabled = 1;
      savePrefs();
      pushCompanionMessage("OK - GPS enabled.");
    } else if (starts_with_word(arg, "off")) {
      _prefs.gps_enabled = 0;
      savePrefs();
      pushCompanionMessage("OK - GPS disabled.");
    } else {
      pushCompanionMessage("Usage: gps [on | off | power ...]  (ohne Arg -> Status)");
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
      _trace_flags = 0;  // RAM-only auch resetten (sonst inkonsistent)
      savePrefs();
      pushCompanionMessage("OK - DL9SAU prefs auf Defaults zurueckgesetzt.");
      return;
    }

    // Akkumulierender Buffer der bei ~140 Zeichen autom. pusht.
    char prefs_buf[200];
    size_t buf_used = 0;
    auto flush_buf = [&](bool force) {
      if (buf_used == 0) return;
      if (!force && buf_used < 130) return;
      prefs_buf[buf_used] = 0;
      pushCompanionMessage(prefs_buf);
      buf_used = 0;
    };
    auto add_line = [&](const char* line) {
      size_t len = strlen(line);
      if (buf_used + len + 2 >= sizeof(prefs_buf)) flush_buf(true);
      if (buf_used > 0) prefs_buf[buf_used++] = '\n';
      for (size_t i = 0; i < len && buf_used < sizeof(prefs_buf) - 1; i++) {
        prefs_buf[buf_used++] = line[i];
      }
      flush_buf(false);
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
    // duty
    if (show_all || _prefs.duty_soft_pct != 80) {
      snprintf(tmp, sizeof(tmp), "  duty_soft_pct = %u%%%s",
               (unsigned)_prefs.duty_soft_pct,
               _prefs.duty_soft_pct == 80 ? " [default]" : " (default: 80)");
      add_line(tmp);
      if (_prefs.duty_soft_pct != 80) non_default_count++;
    }
    if (show_all || _prefs.duty_hard_pct != 100) {
      snprintf(tmp, sizeof(tmp), "  duty_hard_pct = %u%%%s",
               (unsigned)_prefs.duty_hard_pct,
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

    if (!show_all && non_default_count == 0) {
      add_line("  (alle Werte auf Default)");
    }
    flush_buf(true);
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
  if (starts_with_word(cmd, "stats")) {
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
      snprintf(block, sizeof(block),
               "duty: last_h=%s of %s (%.1f%%)\n"
               "  soft=%u%% (%s) hard=%u%% (%s) blocked=%lu",
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
    if (!arg || *arg == 0) {
      // Zwei Zeilen: aktiv + persistent (wenn beide gleich, nur eine)
      char line[160]; int used;
      used = snprintf(line, sizeof(line), "trace active:");
      bool any_a = false;
      for (size_t k = 0; k < TRACE_CAT_COUNT; k++) {
        if (_trace_flags & trace_cats[k].flag) {
          used += snprintf(line + used, sizeof(line) - used, " %s", trace_cats[k].name);
          any_a = true;
        }
      }
      if (!any_a) snprintf(line + used, sizeof(line) - used, " (none)");
      pushCompanionMessage(line);

      if (_prefs.trace_flags_persistent != _trace_flags) {
        used = snprintf(line, sizeof(line), "trace persistent (-> 'trace on'):");
        bool any_p = false;
        for (size_t k = 0; k < TRACE_CAT_COUNT; k++) {
          if (_prefs.trace_flags_persistent & trace_cats[k].flag) {
            used += snprintf(line + used, sizeof(line) - used, " %s", trace_cats[k].name);
            any_p = true;
          }
        }
        if (!any_p) snprintf(line + used, sizeof(line) - used, " (none)");
        pushCompanionMessage(line);
      }
      return;
    }

    if (starts_with_word(arg, "list")) {
      for (size_t k = 0; k < TRACE_CAT_COUNT; k++) {
        char line[160];
        snprintf(line, sizeof(line), "  %s - %s", trace_cats[k].name, trace_cats[k].desc);
        pushCompanionMessage(line);
      }
      return;
    }

    // 'trace on' -> active = persistent (Wiederherstellen)
    if (strcmp(arg, "on") == 0) {
      _trace_flags = _prefs.trace_flags_persistent;
      pushCompanionMessage("OK - trace resumed (active = persistent).");
      return;
    }
    // 'trace off' -> active = 0, persistent BLEIBT (Pause)
    if (strcmp(arg, "off") == 0) {
      _trace_flags = 0;
      pushCompanionMessage("OK - trace paused (persistent untouched).");
      return;
    }

    // 'trace all on/off' -> active UND persistent
    if (starts_with_word(arg, "all")) {
      const char* sub = strchr(arg, ' ');
      if (sub) { while (*sub == ' ') sub++; }
      if (sub && strcmp(sub, "on") == 0) {
        _trace_flags = TRACE_ALL_MASK;
        _prefs.trace_flags_persistent = TRACE_ALL_MASK;
        savePrefs();
        pushCompanionMessage("OK - alle traces an (active + persistent).");
      } else if (sub && strcmp(sub, "off") == 0) {
        _trace_flags = 0;
        _prefs.trace_flags_persistent = 0;
        savePrefs();
        pushCompanionMessage("OK - alle traces aus (active + persistent).");
      } else {
        pushCompanionMessage("Usage: trace all on | trace all off");
      }
      return;
    }

    // 'trace <cat> on/off' -> bit in BEIDEN (User-Selektion)
    for (size_t k = 0; k < TRACE_CAT_COUNT; k++) {
      if (starts_with_word(arg, trace_cats[k].name)) {
        const char* sub = strchr(arg, ' ');
        if (sub) { while (*sub == ' ') sub++; }
        if (sub && strcmp(sub, "on") == 0) {
          _trace_flags |= trace_cats[k].flag;
          _prefs.trace_flags_persistent |= trace_cats[k].flag;
          savePrefs();
          char r[80]; snprintf(r, sizeof(r), "OK - trace %s an.", trace_cats[k].name);
          pushCompanionMessage(r);
        } else if (sub && strcmp(sub, "off") == 0) {
          _trace_flags &= ~trace_cats[k].flag;
          _prefs.trace_flags_persistent &= ~trace_cats[k].flag;
          savePrefs();
          char r[80]; snprintf(r, sizeof(r), "OK - trace %s aus.", trace_cats[k].name);
          pushCompanionMessage(r);
        } else {
          char r[80]; snprintf(r, sizeof(r), "Usage: trace %s on|off", trace_cats[k].name);
          pushCompanionMessage(r);
        }
        return;
      }
    }
    pushCompanionMessage("Unbekannte trace-Kategorie. 'trace list' fuer Uebersicht.");
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
    if (strcmp(arg, "reset") == 0) {
      // Zurueck auf die hardcoded Defaults — Counter bleibt erhalten
      // (ist nur eine Statistik, nicht Teil der Konfiguration).
      _prefs.duty_soft_pct = 80;
      _prefs.duty_hard_pct = 100;
      savePrefs();
      pushCompanionMessage("OK - duty reset: soft=80%, hard=100%.");
      return;
    }
    if (starts_with_word(arg, "soft") || starts_with_word(arg, "hard")) {
      bool is_soft = starts_with_word(arg, "soft");
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
      if (default_set) snprintf(def_line, sizeof(def_line), "default  = #%s",
               _prefs.default_scope_name[0] ? _prefs.default_scope_name : "?");
      else snprintf(def_line, sizeof(def_line), "default  = (none)");

      if (bake_set) snprintf(bake_line, sizeof(bake_line), "bake     = #%s",
               _prefs.bake_scope_name[0] ? _prefs.bake_scope_name : "?");
      else snprintf(bake_line, sizeof(bake_line), "bake     = (none)");

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

      const char* active;
      if (override_active) active = "override";
      else if (bake_set)   active = "bake";
      else if (default_set) active = "default";
      else {
        TransportKey tmp;
        active = chooseGeoFallbackScope(tmp) ? "geo-fallback" : "(none)";
      }
      char block[300];
      snprintf(block, sizeof(block),
               "scope (nightly bake hierarchy):\n"
               "  %s\n  %s\n  %s\n"
               "  active = %s",
               def_line, bake_line, ovr_line, active);
      pushCompanionMessage(block);
      return;
    }

    // -- default <name>|clear --
    if (starts_with_word(arg, "default")) {
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
    if (starts_with_word(arg, "bake")) {
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
    if (starts_with_word(arg, "override")) {
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

    pushCompanionMessage("Usage: scope [default|bake|override] <name>|clear");
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
      snprintf(line, sizeof(line),
               "repeater=%s%s  freq=%.4f MHz  strict_ok=%s",
               _prefs.client_repeat ? "on" : "off",
               _prefs.client_repeat_force ? " (force)" : "",
               _prefs.freq, strict_ok ? "yes" : "no");
      pushCompanionMessage(line);
      return;
    }
    if (strcmp(arg, "off") == 0) {
      _prefs.client_repeat = 0;
      _prefs.client_repeat_force = 0;  // Force-Modus mit "off" beenden
      savePrefs();
      pushCompanionMessage("OK - repeater off.");
      return;
    }
    if (starts_with_word(arg, "on")) {
      // "force"-Keyword erkennen (iOS-Tastatur macht aus "--force" einen
      // em-dash — daher ein einzelnes lowercase Wort statt Doppel-Hyphen).
      bool force = false;
      const char* rest = arg + 2;  // hinter "on"
      while (*rest == ' ') rest++;
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

    // "on" / "off" -> beide Flags
    if (strcmp(arg, "on") == 0) {
      _prefs.auto_advert_enabled = AUTO_ADV_ALL;
      savePrefs();
      trigger_zerohop_now();
      trigger_nightly_reschedule();
      pushCompanionMessage("OK - autoadv zerohop=on, nightly=on (sofort + reschedule).");
      return;
    }
    if (strcmp(arg, "off") == 0) {
      _prefs.auto_advert_enabled = 0;
      savePrefs();
      pushCompanionMessage("OK - autoadv zerohop=off, nightly=off.");
      return;
    }

    // "zerohop on/off" / "nightly on/off"
    bool is_zh = starts_with_word(arg, "zerohop");
    bool is_nl = starts_with_word(arg, "nightly");
    if (is_zh || is_nl) {
      const char* sub = strchr(arg, ' ');
      if (sub) { while (*sub == ' ') sub++; }
      if (!sub || (strcmp(sub, "on") != 0 && strcmp(sub, "off") != 0)) {
        pushCompanionMessage(is_zh
          ? "Usage: autoadv zerohop on|off"
          : "Usage: autoadv nightly on|off");
        return;
      }
      uint8_t mask = is_zh ? AUTO_ADV_ZEROHOP : AUTO_ADV_NIGHTLY;
      bool on = (strcmp(sub, "on") == 0);
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

    if (strcmp(arg, "default") == 0) {
      _prefs.chat_name_mode = 0;
      savePrefs();
      pushCompanionMessage("OK - chatname = default (full node_name).");
      return;
    }

    // ---- chatname hex: Diagnose, dumpt die bytes von node_name + preview
    // ---- Nutzlich um UTF-8-Multi-Byte-Probleme zu erkennen (z.B. wenn ein
    // ---- Emoji am Ende des Names unklar wirkt).
    if (strcmp(arg, "hex") == 0) {
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

    // Numerisches Argument? chatname N (N=1..253) -> erste N Woerter
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

    if (starts_with_word(arg, "custom")) {
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
    pushCompanionMessage("Usage: chatname [default | N | custom <Name>]  (N = 1..253)");
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

  // Always to Serial — direct USB users keep their stream
  Serial.print(buf);
  if (buf[n - 1] != '\n') Serial.print('\n');

  // Push to app debug log if connected. Frame: [PUSH_CODE][text bytes, no null]
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
