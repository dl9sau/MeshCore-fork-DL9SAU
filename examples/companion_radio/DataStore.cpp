#include <Arduino.h>
#include "DataStore.h"

#if defined(EXTRAFS) || defined(QSPIFLASH)
  #define MAX_BLOBRECS 100
#else
  #define MAX_BLOBRECS 20
#endif

DataStore::DataStore(FILESYSTEM& fs, mesh::RTCClock& clock) : _fs(&fs), _fsExtra(nullptr), _clock(&clock),
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
    identity_store(fs, "")
#elif defined(RP2040_PLATFORM)
    identity_store(fs, "/identity")
#else
    identity_store(fs, "/identity")
#endif
{
}

#if defined(EXTRAFS) || defined(QSPIFLASH)
DataStore::DataStore(FILESYSTEM& fs, FILESYSTEM& fsExtra, mesh::RTCClock& clock) : _fs(&fs), _fsExtra(&fsExtra), _clock(&clock),
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
    identity_store(fs, "")
#elif defined(RP2040_PLATFORM)
    identity_store(fs, "/identity")
#else
    identity_store(fs, "/identity")
#endif
{
}
#endif

static File openWrite(FILESYSTEM* fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  fs->remove(filename);
  return fs->open(filename, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return fs->open(filename, "w");
#else
  return fs->open(filename, "w", true);
#endif
}

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  static uint32_t _ContactsChannelsTotalBlocks = 0;
#endif

void DataStore::begin() {
#if defined(RP2040_PLATFORM)
  identity_store.begin();
#endif

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  _ContactsChannelsTotalBlocks = _getContactsChannelsFS()->_getFS()->cfg->block_count;
  checkAdvBlobFile();
  #if defined(EXTRAFS) || defined(QSPIFLASH)
  migrateToSecondaryFS();
  #endif
#else
  // init 'blob store' support
  _fs->mkdir("/bl");
#endif
}

#if defined(ESP32)
  #include <SPIFFS.h>
  #include <nvs_flash.h>
#elif defined(RP2040_PLATFORM)
  #include <LittleFS.h>
#elif defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  #if defined(QSPIFLASH)
    #include <CustomLFS_QSPIFlash.h>
  #elif defined(EXTRAFS)
    #include <CustomLFS.h>
  #else 
    #include <InternalFileSystem.h>
  #endif
#endif

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
int _countLfsBlock(void *p, lfs_block_t block){
      if (block > _ContactsChannelsTotalBlocks) {
        MESH_DEBUG_PRINTLN("ERROR: Block %d exceeds filesystem bounds - CORRUPTION DETECTED!", block);
        return LFS_ERR_CORRUPT;  // return error to abort lfs_traverse() gracefully
    }
  lfs_size_t *size = (lfs_size_t*) p;
  *size += 1;
    return 0;
}

lfs_ssize_t _getLfsUsedBlockCount(FILESYSTEM* fs) {
  lfs_size_t size = 0;
  int err = lfs_traverse(fs->_getFS(), _countLfsBlock, &size);
  if (err) {
    MESH_DEBUG_PRINTLN("ERROR: lfs_traverse() error: %d", err);
    return 0;
  }
  return size;
}
#endif

uint32_t DataStore::getStorageUsedKb() const {
#if defined(ESP32)
  return SPIFFS.usedBytes() / 1024;
#elif defined(RP2040_PLATFORM)
  FSInfo info;
  info.usedBytes = 0;
  _fs->info(info);
  return info.usedBytes / 1024;
#elif defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  const lfs_config* config = _getContactsChannelsFS()->_getFS()->cfg;
  int usedBlockCount = _getLfsUsedBlockCount(_getContactsChannelsFS());
  int usedBytes = config->block_size * usedBlockCount;
  return usedBytes / 1024;
#else
  return 0;
#endif
}

uint32_t DataStore::getStorageTotalKb() const {
#if defined(ESP32)
  return SPIFFS.totalBytes() / 1024;
#elif defined(RP2040_PLATFORM)
  FSInfo info;
  info.totalBytes = 0;
  _fs->info(info);
  return info.totalBytes / 1024;
#elif defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  const lfs_config* config = _getContactsChannelsFS()->_getFS()->cfg;
  int totalBytes = config->block_size * config->block_count;
  return totalBytes / 1024;
#else
  return 0;
#endif
}

// DL9SAU 2026-07-12: Belegung beider FS in Bytes -- fuer 'fsinfo' + die
// temp+rename-Platzentscheidung (temp braucht temporaer 2x die groesste Datei).
void DataStore::getFsInfo(uint32_t& int_total, uint32_t& int_used,
                          uint32_t& ext_total, uint32_t& ext_used) const {
  int_total = int_used = ext_total = ext_used = 0;
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  {
    const lfs_config* c = _fs->_getFS()->cfg;
    int_total = (uint32_t)(c->block_size) * (uint32_t)(c->block_count);
    int_used  = (uint32_t)(c->block_size) * (uint32_t)_getLfsUsedBlockCount(_fs);
  }
  if (_fsExtra) {
    const lfs_config* c = _fsExtra->_getFS()->cfg;
    ext_total = (uint32_t)(c->block_size) * (uint32_t)(c->block_count);
    ext_used  = (uint32_t)(c->block_size) * (uint32_t)_getLfsUsedBlockCount(_fsExtra);
  }
#endif
}

File DataStore::openRead(const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return _fs->open(filename, FILE_O_READ);
#elif defined(RP2040_PLATFORM)
  return _fs->open(filename, "r");
#else
  return _fs->open(filename, "r", false);
#endif
}

File DataStore::openRead(FILESYSTEM* fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return fs->open(filename, FILE_O_READ);
#elif defined(RP2040_PLATFORM)
  return fs->open(filename, "r");
#else
  return fs->open(filename, "r", false);
#endif
}

// Oeffnet eine Datei zum Schreiben (truncate). Wird von MyMesh fuer
// Offline-Message-Bucket-Persistenz verwendet (Wunschliste 19 Phase C).
File DataStore::openWriteFile(const char* filename) {
  return openWrite(_fs, filename);
}

File DataStore::openWriteFile(FILESYSTEM* fs, const char* filename) {
  return openWrite(fs, filename);
}

bool DataStore::removeFile(const char* filename) {
  return _fs->remove(filename);
}

bool DataStore::removeFile(FILESYSTEM* fs, const char* filename) {
  return fs->remove(filename);
}

bool DataStore::renameFile(const char* from, const char* to) {
  return _fs->rename(from, to);
}

// DL9SAU 2026-07-15: In-Place-Write ohne remove (siehe .h). Caller: seek(0) +
// nach dem Schreiben truncate().
File DataStore::openWriteFileInPlace(const char* filename) {
  return openWriteFileInPlace(_fs, filename);
}

// DL9SAU 2026-07-17 (#86): Append-Open. Es gibt in Adafruit_LittleFS KEIN
// O_APPEND-Flag (nur FILE_O_READ/FILE_O_WRITE). FILE_O_WRITE = LFS_O_RDWR|
// LFS_O_CREAT truncatet NICHT und die Library seekt beim Open bereits selbst
// ans Datei-Ende (Adafruit_LittleFS_File.cpp: 'if FILE_O_WRITE lfs_file_seek
// ..LFS_SEEK_END'). Das explizite seek(size()) ist daher redundant, bleibt aber
// als selbstdokumentierende Absicht + Schutz gegen kuenftige Library-Aenderungen.
File DataStore::openAppendFile(FILESYSTEM* fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  File f = fs->open(filename, FILE_O_WRITE);  // kein remove; Lib steht schon am Ende
  if (f) f.seek(f.size());                    // -> explizit ans Datei-Ende (append)
  return f;
#elif defined(RP2040_PLATFORM)
  return fs->open(filename, "a");
#else
  return fs->open(filename, "a", true);
#endif
}

File DataStore::openWriteFileInPlace(FILESYSTEM* fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return fs->open(filename, FILE_O_WRITE);   // KEIN remove -> Bloecke bleiben
#elif defined(RP2040_PLATFORM)
  return fs->open(filename, "w");            // "w" truncatet bereits
#else
  return fs->open(filename, "w", true);
#endif
}

// DL9SAU 2026-07-12: Existenz-Check ohne Oeffnen/Parsen -- fuer den
// /shutdown_pending-Datei-Sentinel (touch=pending / rm=gecleart).
bool DataStore::fileExists(const char* filename) const {
  return _fs->exists(filename);
}

bool DataStore::formatFileSystem() {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  if (_fsExtra == nullptr) {
    return _fs->format();
  } else {
    return _fs->format() && _fsExtra->format();
  }
#elif defined(RP2040_PLATFORM)
  return LittleFS.format();
#elif defined(ESP32)
  bool fs_success = ((fs::SPIFFSFS *)_fs)->format();
  esp_err_t nvs_err = nvs_flash_erase(); // no need to reinit, will be done by reboot
  return fs_success && (nvs_err == ESP_OK);
#else
  #error "need to implement format()"
#endif
}

// DL9SAU 2026-07-15: nur die InternalFS (Key/Prefs/Bonds) formatieren.
bool DataStore::formatInternalFS() {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return _fs->format();
#elif defined(RP2040_PLATFORM)
  return LittleFS.format();
#elif defined(ESP32)
  bool fs_success = ((fs::SPIFFSFS *)_fs)->format();
  esp_err_t nvs_err = nvs_flash_erase();
  return fs_success && (nvs_err == ESP_OK);
#else
  return false;
#endif
}

// DL9SAU 2026-07-15: nur die ExtraFS (Channels/Contacts) formatieren. Gibt es
// nur auf NRF52/STM32 (zweite littlefs-Partition); sonst false.
bool DataStore::formatExtraFS() {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  if (_fsExtra == nullptr) return false;
  return _fsExtra->format();
#else
  return false;
#endif
}

bool DataStore::loadMainIdentity(mesh::LocalIdentity &identity) {
  return identity_store.load("_main", identity);
}

bool DataStore::saveMainIdentity(const mesh::LocalIdentity &identity) {
  return identity_store.save("_main", identity);
}

void DataStore::loadPrefs(NodePrefs& prefs, double& node_lat, double& node_lon) {
  // DL9SAU 2026-07-12: (RCV-remove-Recovery hier wieder entfernt -- littlefs
  // remove() failt auf einer datenblock-korrupten Datei, half nicht. Der
  // funktionierende Weg ist Format+Identity-Rettung, siehe MyMesh::begin
  // NRF52_RECOVER_FORMAT_INTERNALFS.)
  // DL9SAU 2026-07-11: NRF52_REMOVE_PREFS_ONCE (boot-time One-Shot-Loeschen von
  // /new_prefs + /node_prefs) entfernt -- abgeloest durch den CLI-Befehl
  // 'reformat yes'. War gefaehrlich (bei versehentlich aktivem Flag Prefs-
  // Wipe bei JEDEM Boot).
#if defined(NRF52_PLATFORM) && defined(NRF52_BOOT_TRACE)
  Serial.println("\r\n# [T1000-E diag] LP1 in loadPrefs"); Serial.flush();
  bool ex_new = _fs->exists("/new_prefs");
  Serial.print("# [T1000-E diag] LP2 /new_prefs exists="); Serial.println(ex_new ? "y" : "n"); Serial.flush();
  bool ex_old = _fs->exists("/node_prefs");
  Serial.print("# [T1000-E diag] LP3 /node_prefs exists="); Serial.println(ex_old ? "y" : "n"); Serial.flush();
#endif
  if (_fs->exists("/new_prefs")) {
#if defined(NRF52_PLATFORM) && defined(NRF52_BOOT_TRACE)
    Serial.println("# [T1000-E diag] LP4 pre loadPrefsInt(/new_prefs)"); Serial.flush();
#endif
    loadPrefsInt("/new_prefs", prefs, node_lat, node_lon); // new filename
#if defined(NRF52_PLATFORM) && defined(NRF52_BOOT_TRACE)
    Serial.println("# [T1000-E diag] LP5 post loadPrefsInt"); Serial.flush();
#endif
  } else if (_fs->exists("/node_prefs")) {
    loadPrefsInt("/node_prefs", prefs, node_lat, node_lon);
    savePrefs(prefs, node_lat, node_lon);                // save to new filename
    _fs->remove("/node_prefs"); // remove old
  }
}

void DataStore::loadPrefsInt(const char *filename, NodePrefs& _prefs, double& node_lat, double& node_lon) {
  File file = openRead(_fs, filename);
#if defined(NRF52_PLATFORM) && defined(NRF52_BOOT_TRACE)
  Serial.print("\r\n# [T1000-E diag] LP5 post openRead file=");
  Serial.print(((bool)file) ? "ok" : "null");
  if (file) { Serial.print(" size="); Serial.print((unsigned long)file.size()); }
  Serial.println(); Serial.flush();
#endif
  if (file) {
    uint8_t pad[8];

    file.read((uint8_t *)&_prefs.airtime_factor, sizeof(float));                           // 0
#if defined(NRF52_PLATFORM) && defined(NRF52_BOOT_TRACE)
    Serial.println("# [T1000-E diag] LP6 post first field read"); Serial.flush();
#endif
    file.read((uint8_t *)_prefs.node_name, sizeof(_prefs.node_name));                      // 4
    file.read(pad, 4);                                                                     // 36
    file.read((uint8_t *)&node_lat, sizeof(node_lat));                                     // 40
    file.read((uint8_t *)&node_lon, sizeof(node_lon));                                     // 48
    file.read((uint8_t *)&_prefs.freq, sizeof(_prefs.freq));                               // 56
    file.read((uint8_t *)&_prefs.sf, sizeof(_prefs.sf));                                   // 60
    file.read((uint8_t *)&_prefs.cr, sizeof(_prefs.cr));                                   // 61
    file.read((uint8_t *)&_prefs.client_repeat, sizeof(_prefs.client_repeat));             // 62
    file.read((uint8_t *)&_prefs.manual_add_contacts, sizeof(_prefs.manual_add_contacts)); // 63
    file.read((uint8_t *)&_prefs.bw, sizeof(_prefs.bw));                                   // 64
    file.read((uint8_t *)&_prefs.tx_power_dbm, sizeof(_prefs.tx_power_dbm));               // 68
    file.read((uint8_t *)&_prefs.telemetry_mode_base, sizeof(_prefs.telemetry_mode_base)); // 69
    file.read((uint8_t *)&_prefs.telemetry_mode_loc, sizeof(_prefs.telemetry_mode_loc));   // 70
    file.read((uint8_t *)&_prefs.telemetry_mode_env, sizeof(_prefs.telemetry_mode_env));   // 71
    file.read((uint8_t *)&_prefs.rx_delay_base, sizeof(_prefs.rx_delay_base));             // 72
    file.read((uint8_t *)&_prefs.advert_loc_policy, sizeof(_prefs.advert_loc_policy));     // 76
    file.read((uint8_t *)&_prefs.multi_acks, sizeof(_prefs.multi_acks));                   // 77
    file.read((uint8_t *)&_prefs.path_hash_mode, sizeof(_prefs.path_hash_mode));           // 78
    file.read(pad, 1);                                                                     // 79
    file.read((uint8_t *)&_prefs.ble_pin, sizeof(_prefs.ble_pin));                         // 80
    file.read((uint8_t *)&_prefs.buzzer_quiet, sizeof(_prefs.buzzer_quiet));               // 84
    file.read((uint8_t *)&_prefs.gps_enabled, sizeof(_prefs.gps_enabled));                 // 85
    file.read((uint8_t *)&_prefs.gps_interval, sizeof(_prefs.gps_interval));               // 86
    file.read((uint8_t *)&_prefs.autoadd_config, sizeof(_prefs.autoadd_config));           // 87
    file.read((uint8_t *)&_prefs.autoadd_max_hops, sizeof(_prefs.autoadd_max_hops));       // 88
    file.read((uint8_t *)&_prefs.rx_boosted_gain, sizeof(_prefs.rx_boosted_gain));         // 89
    file.read((uint8_t *)_prefs.default_scope_name, sizeof(_prefs.default_scope_name));    // 90
    file.read((uint8_t *)_prefs.default_scope_key, sizeof(_prefs.default_scope_key));     // 121
    // Optional/Neuere Felder — bei alten Files liest file.read 0 Bytes und
    // die memset(0)-Defaults aus dem MyMesh-Konstruktor bleiben stehen.
    file.read((uint8_t *)&_prefs.chat_name_mode, sizeof(_prefs.chat_name_mode));           // 137
    file.read((uint8_t *)_prefs.chat_name_custom, sizeof(_prefs.chat_name_custom));        // 138
    file.read((uint8_t *)&_prefs.auto_advert_enabled, sizeof(_prefs.auto_advert_enabled)); // 170
    file.read((uint8_t *)&_prefs.client_repeat_force, sizeof(_prefs.client_repeat_force)); // 171
    file.read((uint8_t *)&_prefs.duty_soft_pct, sizeof(_prefs.duty_soft_pct));             // 172
    file.read((uint8_t *)&_prefs.duty_hard_pct, sizeof(_prefs.duty_hard_pct));             // 173
    file.read((uint8_t *)_prefs.bake_scope_name, sizeof(_prefs.bake_scope_name));          // 174
    file.read((uint8_t *)_prefs.bake_scope_key,  sizeof(_prefs.bake_scope_key));           // 205
    file.read((uint8_t *)_prefs.override_scope_name, sizeof(_prefs.override_scope_name));  // 221
    file.read((uint8_t *)_prefs.override_scope_key,  sizeof(_prefs.override_scope_key));   // 252
    file.read((uint8_t *)&_prefs.override_expiry, sizeof(_prefs.override_expiry));         // 268
    file.read((uint8_t *)&_prefs.trace_flags_persistent, sizeof(_prefs.trace_flags_persistent)); // 272
    file.read((uint8_t *)&_prefs.gps_power_mode, sizeof(_prefs.gps_power_mode));                   // 274
    file.read((uint8_t *)&_prefs.gps_lead_min, sizeof(_prefs.gps_lead_min));                       // 275
    file.read((uint8_t *)&_prefs.tx_delay_factor, sizeof(_prefs.tx_delay_factor));                 // 276
    file.read((uint8_t *)&_prefs.direct_tx_delay_factor, sizeof(_prefs.direct_tx_delay_factor));   // 280
    file.read((uint8_t *)&_prefs.repeat_scope_mode, sizeof(_prefs.repeat_scope_mode));             // 284
    // Scope-Architektur (Wunschliste 11): Build-in-Status-Array sparse-
    // indexiert via FNV-1a Hash + User-Extras-Liste. Bei alten Files
    // liefert file.read 0 Bytes und memset(0)-Defaults bleiben stehen.
    file.read((uint8_t *)&_prefs.scope_buildin_status_count, sizeof(_prefs.scope_buildin_status_count)); // 285
    file.read((uint8_t *)_prefs.scope_buildin_status, sizeof(_prefs.scope_buildin_status));         // 286 (8*32=256 Byte)
    file.read((uint8_t *)&_prefs.scope_extras_count, sizeof(_prefs.scope_extras_count));            // 542
    file.read((uint8_t *)_prefs.scope_extras, sizeof(_prefs.scope_extras));                          // 543 (52*16=832 Byte)
    file.read((uint8_t *)&_prefs.flood_max_scope_region, sizeof(_prefs.flood_max_scope_region));                  // 1375
    file.read((uint8_t *)&_prefs.flood_max, sizeof(_prefs.flood_max));                              // 1376
    file.read((uint8_t *)&_prefs.scope_advert_auto, sizeof(_prefs.scope_advert_auto));              // 1377
    file.read((uint8_t *)&_prefs.repeater_profile, sizeof(_prefs.repeater_profile));                // 1378
    file.read((uint8_t *)&_prefs.scope_repeater_auto, sizeof(_prefs.scope_repeater_auto));          // 1379
    file.read((uint8_t *)_prefs.owner_info, sizeof(_prefs.owner_info));                              // 1380 (120 byte)
    file.read((uint8_t *)&_prefs.advert_role, sizeof(_prefs.advert_role));                          // 1500
    file.read((uint8_t *)&_prefs.loop_detect, sizeof(_prefs.loop_detect));                          // 1501
    // Wunschliste 19 Phase C: Offline-Queue-Speicher-Konfig. Bei aelteren
    // Files erreicht file.read() EOF und liefert 0 zurueck -> Felder bleiben
    // auf 0 (entspricht Default: alles RAM-only mit type-spezifischen
    // Default-Limits). Reihenfolge im File ist append-only stabil.
    file.read((uint8_t *)&_prefs.msg_store_flash, sizeof(_prefs.msg_store_flash));                  // 1502
    file.read((uint8_t *)_prefs.msg_store_limit, sizeof(_prefs.msg_store_limit));                   // 1503 (5 byte)
    file.read((uint8_t *)&_prefs.log_flags, sizeof(_prefs.log_flags));                              // 1508
    file.read((uint8_t *)&_prefs.flood_max_infra, sizeof(_prefs.flood_max_infra));          // 1509
    file.read((uint8_t *)&_prefs.flood_max_req_resp, sizeof(_prefs.flood_max_req_resp));    // 1510
    // Wunschliste 32 v2: per-Channel-Hops jetzt als Name-Hash-Liste
    // (statt frueher Slot-Index-Array). Format-Change -> existing
    // user-ch.hops gehen beim Firmware-Upgrade verloren (User-Hinweis
    // in der Commit-Message; betrifft Dev-Branch).
    // Offsets verschieben sich -- alle nachfolgenden Felder ebenfalls.
    file.read((uint8_t *)&_prefs.flood_max_unknown_chan, sizeof(_prefs.flood_max_unknown_chan)); // 1511
    file.read((uint8_t *)&_prefs.channel_hops_count, sizeof(_prefs.channel_hops_count));         // 1512
    file.read((uint8_t *)_prefs.channel_hops_list, sizeof(_prefs.channel_hops_list));            // 1513 (MAX_GROUP_CHANNELS*5)
    // Wunschliste 31: time-sync mode + sources (10 byte)
    file.read((uint8_t *)&_prefs.time_sync_mode, sizeof(_prefs.time_sync_mode));            // 1512+MAX_GROUP_CHANNELS
    file.read((uint8_t *)_prefs.time_sync_sources, sizeof(_prefs.time_sync_sources));       // +1
    // Wunschliste 39: unscoped-companions cap. Bei alten Files liefert
    // file.read 0 Bytes -> Pre-Init in begin() bleibt stehen (default
    // CH_HOPS_OFF = follow flood_max_scope_region).
    file.read((uint8_t *)&_prefs.flood_max_unscoped_companions,
              sizeof(_prefs.flood_max_unscoped_companions));
    // Reise-Wunsch 2026-06-08: messages_append_scope_to_name. Bei alten
    // Files liefert file.read 0 Bytes -> Pre-Init in begin() (default 1).
    file.read((uint8_t *)&_prefs.messages_append_scope_to_name,
              sizeof(_prefs.messages_append_scope_to_name));
    // Reise-Wunsch 2026-06-09 (Wunschliste 52): admin/guest passwords.
    // Bei alten Files leer (memset 0 ist Default).
    file.read((uint8_t *)_prefs.passwd_admin, sizeof(_prefs.passwd_admin));
    file.read((uint8_t *)_prefs.passwd_guest, sizeof(_prefs.passwd_guest));
    // Reise-Wunsch 2026-06-09 (Wunschliste 46): Filter-Listen
    file.read((uint8_t *)&_prefs.filter_sender_drop_count,
              sizeof(_prefs.filter_sender_drop_count));
    file.read((uint8_t *)_prefs.filter_sender_drop,
              sizeof(_prefs.filter_sender_drop));
    file.read((uint8_t *)&_prefs.filter_text_drop_count,
              sizeof(_prefs.filter_text_drop_count));
    file.read((uint8_t *)_prefs.filter_text_drop,
              sizeof(_prefs.filter_text_drop));
    // Reise-Wunsch 2026-06-09 (Wunschliste 45): LBT + AGC-Reset
    file.read((uint8_t *)&_prefs.interference_threshold,
              sizeof(_prefs.interference_threshold));
    file.read((uint8_t *)&_prefs.agc_reset_interval,
              sizeof(_prefs.agc_reset_interval));
    // Wunschliste 46 Phase 2 (2026-06-10): channel-filter Masks.
    file.read((uint8_t *)&_prefs.filter_sender_drop_on_channel_mask,
              sizeof(_prefs.filter_sender_drop_on_channel_mask));
    file.read((uint8_t *)&_prefs.filter_sender_drop_exempt_mask,
              sizeof(_prefs.filter_sender_drop_exempt_mask));
    file.read((uint8_t *)&_prefs.filter_text_drop_on_channel_mask,
              sizeof(_prefs.filter_text_drop_on_channel_mask));
    file.read((uint8_t *)&_prefs.filter_text_drop_exempt_mask,
              sizeof(_prefs.filter_text_drop_exempt_mask));
    // Wunschliste 46 Phase 3 (2026-06-10): keep-Listen.
    file.read((uint8_t *)&_prefs.filter_sender_keep_count,
              sizeof(_prefs.filter_sender_keep_count));
    file.read((uint8_t *)_prefs.filter_sender_keep,
              sizeof(_prefs.filter_sender_keep));
    file.read((uint8_t *)&_prefs.filter_text_keep_count,
              sizeof(_prefs.filter_text_keep_count));
    file.read((uint8_t *)_prefs.filter_text_keep,
              sizeof(_prefs.filter_text_keep));
    // Wunschliste 46 Phase 2 v2 (2026-06-10): pro-Pattern channel-filter.
    file.read((uint8_t *)_prefs.filter_sender_drop_chan_on,
              sizeof(_prefs.filter_sender_drop_chan_on));
    file.read((uint8_t *)_prefs.filter_sender_drop_chan_ex,
              sizeof(_prefs.filter_sender_drop_chan_ex));
    file.read((uint8_t *)_prefs.filter_sender_keep_chan_on,
              sizeof(_prefs.filter_sender_keep_chan_on));
    file.read((uint8_t *)_prefs.filter_sender_keep_chan_ex,
              sizeof(_prefs.filter_sender_keep_chan_ex));
    file.read((uint8_t *)_prefs.filter_text_drop_chan_on,
              sizeof(_prefs.filter_text_drop_chan_on));
    file.read((uint8_t *)_prefs.filter_text_drop_chan_ex,
              sizeof(_prefs.filter_text_drop_chan_ex));
    file.read((uint8_t *)_prefs.filter_text_keep_chan_on,
              sizeof(_prefs.filter_text_keep_chan_on));
    file.read((uint8_t *)_prefs.filter_text_keep_chan_ex,
              sizeof(_prefs.filter_text_keep_chan_ex));
    // Wunschliste 46 Phase 5 (2026-06-10): scope-Filter + Repeat-Achse.
    file.read((uint8_t *)&_prefs.filter_scope_drop_count,
              sizeof(_prefs.filter_scope_drop_count));
    file.read((uint8_t *)_prefs.filter_scope_drop,
              sizeof(_prefs.filter_scope_drop));
    file.read((uint8_t *)_prefs.filter_scope_drop_chan_on,
              sizeof(_prefs.filter_scope_drop_chan_on));
    file.read((uint8_t *)_prefs.filter_scope_drop_chan_ex,
              sizeof(_prefs.filter_scope_drop_chan_ex));
    file.read((uint8_t *)&_prefs.filter_scope_keep_count,
              sizeof(_prefs.filter_scope_keep_count));
    file.read((uint8_t *)_prefs.filter_scope_keep,
              sizeof(_prefs.filter_scope_keep));
    file.read((uint8_t *)_prefs.filter_scope_keep_chan_on,
              sizeof(_prefs.filter_scope_keep_chan_on));
    file.read((uint8_t *)_prefs.filter_scope_keep_chan_ex,
              sizeof(_prefs.filter_scope_keep_chan_ex));
    file.read((uint8_t *)&_prefs.filter_unknown_channel_repeat,
              sizeof(_prefs.filter_unknown_channel_repeat));
    // Wunschliste 43 (2026-06-10, refactored 2026-06-11): BLE-Power
    // profile + active. Legacy: 1 byte power_mode wird hier in profile
    // gelesen; active liest EOF (= 0xFF sentinel) wenn alte Datei.
    // Migration in MyMesh::begin() POST-load.
    file.read((uint8_t *)&_prefs.bluetooth_profile,
              sizeof(_prefs.bluetooth_profile));
    file.read((uint8_t *)&_prefs.bluetooth_active,
              sizeof(_prefs.bluetooth_active));
    // Wunschliste 50 Phase 1 (2026-06-11): TZ-Override
    file.read((uint8_t *)&_prefs.tz_mode,
              sizeof(_prefs.tz_mode));
    file.read((uint8_t *)&_prefs.tz_offset_min,
              sizeof(_prefs.tz_offset_min));

    // Wunschliste 46 Phase 4 (2026-06-11): Advert/Pubkey Filter.
    file.read((uint8_t *)&_prefs.filter_advert_drop_name,
              sizeof(_prefs.filter_advert_drop_name));
    file.read((uint8_t *)&_prefs.filter_advert_drop_name_count,
              sizeof(_prefs.filter_advert_drop_name_count));
    file.read((uint8_t *)&_prefs.filter_advert_drop_pubkey,
              sizeof(_prefs.filter_advert_drop_pubkey));
    file.read((uint8_t *)&_prefs.filter_advert_drop_pubkey_count,
              sizeof(_prefs.filter_advert_drop_pubkey_count));
    file.read((uint8_t *)&_prefs.filter_sender_drop_pubkey,
              sizeof(_prefs.filter_sender_drop_pubkey));
    file.read((uint8_t *)&_prefs.filter_sender_drop_pubkey_count,
              sizeof(_prefs.filter_sender_drop_pubkey_count));

    // Wunschliste 10 (2026-06-11): Serial-CLI Persistent.
    file.read((uint8_t *)&_prefs.serial_cli_persist_on,
              sizeof(_prefs.serial_cli_persist_on));

    // Wunschliste 58 Phase B (2026-06-13): CPU-Clock-Frequenz.
    file.read((uint8_t *)&_prefs.cpu_clock_mhz,
              sizeof(_prefs.cpu_clock_mhz));
    // 0xFF (Legacy/EOF Sentinel) -> 0 (= "Default benutzen")
    if (_prefs.cpu_clock_mhz == 0xFF) _prefs.cpu_clock_mhz = 0;

    // Wunschliste 58 Phase A (2026-06-14): Display Wake-Mode.
    file.read((uint8_t *)&_prefs.display_wake_mode,
              sizeof(_prefs.display_wake_mode));
    // 0xFF (Legacy/EOF Sentinel) -> 2 (= "on-at-new-messages")
    if (_prefs.display_wake_mode == 0xFF) _prefs.display_wake_mode = 2;

    // Wunschliste 53 Phase 1+2 (2026-06-14): Hardware-Watchdog Pref.
    // Pre-Init in begin() setzt 0xFF -- wenn file.read 0 Bytes liefert
    // (alte Datei ohne dieses Byte), bleibt 0xFF -> Migration zu 0 (off).
    file.read((uint8_t *)&_prefs.watchdog_mode,
              sizeof(_prefs.watchdog_mode));
    if (_prefs.watchdog_mode == 0xFF) _prefs.watchdog_mode = 0;

    // DL9SAU 2026-06-17 (Wunschliste 75): Buzzer-Profile Bitmask.
    file.read((uint8_t *)&_prefs.buzzer_profile,
              sizeof(_prefs.buzzer_profile));
    if (_prefs.buzzer_profile == 0xFF) _prefs.buzzer_profile = 0x0F;

    // DL9SAU 2026-06-17 (Wunschliste 81): GPS-Profile.
    file.read((uint8_t *)&_prefs.gps_profile,
              sizeof(_prefs.gps_profile));
    if (_prefs.gps_profile == 0xFF) _prefs.gps_profile = 0;  // full

    // DL9SAU 2026-06-18 (Wunschliste 83): RX-Disabled (Companion-Stromsparen).
    file.read((uint8_t *)&_prefs.rx_disabled,
              sizeof(_prefs.rx_disabled));
    if (_prefs.rx_disabled == 0xFF) _prefs.rx_disabled = 0;  // rx an

    // DL9SAU 2026-06-18 (Wunschliste 81 Phase 3): gps_lead_secs.
    file.read((uint8_t *)&_prefs.gps_lead_secs,
              sizeof(_prefs.gps_lead_secs));
    // 0xFFFFFFFF = EOF/Legacy -> Migration aus gps_lead_min (oder 0 falls
    // auch dort nichts gesetzt). 0 selbst ist gueltiger "noch-nicht-CLI-
    // gesetzt"-Sentinel, fuer den begin() den Default 5 min wieder herstellt.
    if (_prefs.gps_lead_secs == 0xFFFFFFFFUL) {
      _prefs.gps_lead_secs = 0;  // -> begin() migriert aus gps_lead_min
    }

    // DL9SAU 2026-06-18 (Wunschliste 89/90/91): Battery + USB-Power.
    file.read((uint8_t *)&_prefs.batt_chemistry,
              sizeof(_prefs.batt_chemistry));
    if (_prefs.batt_chemistry == 0xFF) _prefs.batt_chemistry = 0;  // disabled
    file.read((uint8_t *)&_prefs.batt_min_mv,
              sizeof(_prefs.batt_min_mv));
    if (_prefs.batt_min_mv == 0xFFFF) _prefs.batt_min_mv = 0;       // -> Default
    file.read((uint8_t *)&_prefs.usb_loss_shutdown_min,
              sizeof(_prefs.usb_loss_shutdown_min));
    if (_prefs.usb_loss_shutdown_min == 0xFF) _prefs.usb_loss_shutdown_min = 0;
    // DL9SAU 2026-06-20: usb_wake_action entfernt (Sentinel macht das).
    // Pref-Byte bleibt als reserved damit File-Layout stabil ist.
    _prefs._reserved_usb_wake_action = 0xFF;
    file.read((uint8_t *)&_prefs._reserved_usb_wake_action,
              sizeof(_prefs._reserved_usb_wake_action));
    // DL9SAU 2026-06-20: shutdown_pending Sentinel.
    _prefs.shutdown_pending = 0xFF;
    file.read((uint8_t *)&_prefs.shutdown_pending,
              sizeof(_prefs.shutdown_pending));
    if (_prefs.shutdown_pending == 0xFF) _prefs.shutdown_pending = 0;

    // DL9SAU 2026-07-02: channel_no_scope_behavior (loest runtime-only
    // '_unscoped_channel_direct' ab). EOF-Sentinel 0xFF -> Migration.
    _prefs.channel_no_scope_behavior = 0xFF;
    file.read((uint8_t *)&_prefs.channel_no_scope_behavior,
              sizeof(_prefs.channel_no_scope_behavior));
    // 0xFF (Pref-Byte fehlt) oder 0 (zufaellig Zero) -> Default 1 (direct).
    if (_prefs.channel_no_scope_behavior == 0xFF
        || _prefs.channel_no_scope_behavior == 0
        || _prefs.channel_no_scope_behavior > 2) {
      _prefs.channel_no_scope_behavior = 1;  // direct
    }

    // 2026-07-06 Upstream-Merge: cad_enabled. EOF-Sentinel 0xFF -> Default 1
    // (an). Migration-Pattern wie channel_no_scope_behavior.
    _prefs.cad_enabled = 0xFF;
    file.read((uint8_t *)&_prefs.cad_enabled, sizeof(_prefs.cad_enabled));
    if (_prefs.cad_enabled == 0xFF || _prefs.cad_enabled > 1) {
      _prefs.cad_enabled = 1;  // an per Default
    }
    // DL9SAU 2026-07-12: button_press_allow_shutdown. EOF-Sentinel 0xFF ->
    // Default 1 (erlaubt); der >1-Clamp faengt Altdaten ab (inkl. eines
    // evtl. Magic-Rest-Bytes aus /new_prefs von Commit 3735122a).
    _prefs.button_press_allow_shutdown = 0xFF;
    file.read((uint8_t *)&_prefs.button_press_allow_shutdown,
              sizeof(_prefs.button_press_allow_shutdown));
    if (_prefs.button_press_allow_shutdown == 0xFF
        || _prefs.button_press_allow_shutdown > 1) {
      _prefs.button_press_allow_shutdown = 1;
    }
    // DL9SAU 2026-07-12: batt_min_mv_boot (uint16, EOF-Sentinel 0xFFFF ->
    // Default je Chemie). APPEND ans Ende -- NICHT bei batt_min_mv einfuegen.
    _prefs.batt_min_mv_boot = 0xFFFF;
    file.read((uint8_t *)&_prefs.batt_min_mv_boot, sizeof(_prefs.batt_min_mv_boot));
    if (_prefs.batt_min_mv_boot == 0xFFFF) _prefs.batt_min_mv_boot = 0;

#ifdef ESP_PLATFORM
    // DL9SAU 2026-06-16: Reboot-into-OTA Pending-Flag (ESP32-only,
    // NRF52 nutzt DFU-Pfad statt WiFi-OTA).
    // 'start ota' setzt das Flag + savePrefs + reboot. setup() liest
    // hier, clear's es sofort wieder via savePrefs, und routed in den
    // OTA-Boot-Mode. Alte Datei ohne dieses Byte: file.read = 0 -> 0.
    file.read((uint8_t *)&_prefs.ota_pending,
              sizeof(_prefs.ota_pending));
    if (_prefs.ota_pending == 0xFF) _prefs.ota_pending = 0;
#endif

    // DL9SAU 2026-07-13 (Level-Umbau): trace_levels (uint64, 2 Bit/Kategorie).
    // APPEND ans Datei-Ende -- auf beiden Plattformen letztes Feld (nach dem
    // ESP-only ota_pending). Alte Datei ohne dieses Feld: file.read liefert
    // < 8 Bytes -> Migration aus trace_flags_persistent (jedes gesetzte Bit
    // -> Level 1). Kein 0xFF..-Sentinel, weil ein voll auf Level 3 gesetztes
    // 32-Kategorien-File tatsaechlich all-FF waere -- die Lese-Laenge ist der
    // eindeutige "Feld vorhanden?"-Marker.
    {
      uint64_t tl = 0;
      if (file.read((uint8_t *)&tl, sizeof(tl)) == sizeof(tl)) {
        _prefs.trace_levels = tl;
      } else {
        // Migration: alte an/aus-Bits -> Level 1.
        uint64_t mig = 0;
        for (uint8_t k = 0; k < 16; k++) {
          if (_prefs.trace_flags_persistent & (1u << k)) mig |= ((uint64_t)1 << (2 * k));
        }
        _prefs.trace_levels = mig;
      }
    }

    // DL9SAU 2026-07-14: auto_on_when_charging (uint8, EOF 0xFF -> Default 1).
    // APPEND ans Ende (nach trace_levels).
    _prefs.auto_on_when_charging = 0xFF;
    file.read((uint8_t *)&_prefs.auto_on_when_charging, sizeof(_prefs.auto_on_when_charging));
    if (_prefs.auto_on_when_charging == 0xFF) _prefs.auto_on_when_charging = 1;

    // DL9SAU 2026-07-21 (#3/#4): Advert-Scope (periodic/nightly) + Mindest-
    // Intervall. APPEND ans Ende (nach auto_on_when_charging). Default 0
    // (zero-hop/follow/kein Floor) via Pre-Init in begin() -- kurze Alt-Datei
    // laesst die Felder auf 0. Clamp faengt Altdaten-Muell ab.
    file.read((uint8_t *)&_prefs.advert_periodic_scope, sizeof(_prefs.advert_periodic_scope));
    if (_prefs.advert_periodic_scope > 2) _prefs.advert_periodic_scope = 0;
    file.read((uint8_t *)&_prefs.advert_nightly_scope, sizeof(_prefs.advert_nightly_scope));
    if (_prefs.advert_nightly_scope > 2) _prefs.advert_nightly_scope = 0;
    file.read((uint8_t *)&_prefs.advert_periodic_min_min, sizeof(_prefs.advert_periodic_min_min));
    if (_prefs.advert_periodic_min_min > 1440) _prefs.advert_periodic_min_min = 0;

    file.close();
  }
}

void DataStore::savePrefs(const NodePrefs& _prefs, double node_lat, double node_lon) {
  // DL9SAU 2026-07-12: temp+rename statt in-place-Write. Der Body (unten,
  // unveraendert) steckt in einer Lambda, damit er zweimal aufrufbar ist:
  //   1) nach /new_prefs.tmp -> bei VOLLSTAENDIGEM Write atomar rename ueber
  //      /new_prefs. Ein WDT-Reset/Absturz MITTEN im Write laesst nur die tmp
  //      zurueck; die echte Datei bleibt intakt -- das war die Config-Verlust-
  //      Ursache.
  //   2) Fallback (FS voll -> ein write kurz / rename scheitert): tmp weg +
  //      Direkt-Write wie frueher (best-guess-Degradierung).
  // Vollstaendigkeit NICHT ueber einen Marker im File (der wuerde den tolerant
  // wachsenden Parser irritieren), sondern ueber die write()-Rueckgabe ==
  // erwartete Laenge. CountingWriter kapselt file.write() und merkt sich, ob
  // je ein Write weniger lieferte (Adafruit kappt lfs-Fehler auf 0). close()
  // ist void -> nicht pruefbar, aber littlefs schreibt Bloecke schon WAEHREND
  // write() -> FS-voll schlaegt bereits dort zu.
  struct CountingWriter {
    File&  f;
    bool   ok;
    size_t write(const uint8_t* p, size_t n) {
      size_t w = f.write(p, n);
      if (w != n) ok = false;
      return w;
    }
  };
  auto writeBody = [&](CountingWriter& file) {
    uint8_t pad[8];
    memset(pad, 0, sizeof(pad));

    file.write((uint8_t *)&_prefs.airtime_factor, sizeof(float));                           // 0
    file.write((uint8_t *)_prefs.node_name, sizeof(_prefs.node_name));                      // 4
    file.write(pad, 4);                                                                     // 36
    file.write((uint8_t *)&node_lat, sizeof(node_lat));                                     // 40
    file.write((uint8_t *)&node_lon, sizeof(node_lon));                                     // 48
    file.write((uint8_t *)&_prefs.freq, sizeof(_prefs.freq));                               // 56
    file.write((uint8_t *)&_prefs.sf, sizeof(_prefs.sf));                                   // 60
    file.write((uint8_t *)&_prefs.cr, sizeof(_prefs.cr));                                   // 61
    file.write((uint8_t *)&_prefs.client_repeat, sizeof(_prefs.client_repeat));             // 62
    file.write((uint8_t *)&_prefs.manual_add_contacts, sizeof(_prefs.manual_add_contacts)); // 63
    file.write((uint8_t *)&_prefs.bw, sizeof(_prefs.bw));                                   // 64
    file.write((uint8_t *)&_prefs.tx_power_dbm, sizeof(_prefs.tx_power_dbm));               // 68
    file.write((uint8_t *)&_prefs.telemetry_mode_base, sizeof(_prefs.telemetry_mode_base)); // 69
    file.write((uint8_t *)&_prefs.telemetry_mode_loc, sizeof(_prefs.telemetry_mode_loc));   // 70
    file.write((uint8_t *)&_prefs.telemetry_mode_env, sizeof(_prefs.telemetry_mode_env));   // 71
    file.write((uint8_t *)&_prefs.rx_delay_base, sizeof(_prefs.rx_delay_base));             // 72
    file.write((uint8_t *)&_prefs.advert_loc_policy, sizeof(_prefs.advert_loc_policy));     // 76
    file.write((uint8_t *)&_prefs.multi_acks, sizeof(_prefs.multi_acks));                   // 77
    file.write((uint8_t *)&_prefs.path_hash_mode, sizeof(_prefs.path_hash_mode));           // 78
    file.write(pad, 1);                                                                     // 79
    file.write((uint8_t *)&_prefs.ble_pin, sizeof(_prefs.ble_pin));                         // 80
    file.write((uint8_t *)&_prefs.buzzer_quiet, sizeof(_prefs.buzzer_quiet));               // 84
    file.write((uint8_t *)&_prefs.gps_enabled, sizeof(_prefs.gps_enabled));                 // 85
    file.write((uint8_t *)&_prefs.gps_interval, sizeof(_prefs.gps_interval));               // 86
    file.write((uint8_t *)&_prefs.autoadd_config, sizeof(_prefs.autoadd_config));           // 87
    file.write((uint8_t *)&_prefs.autoadd_max_hops, sizeof(_prefs.autoadd_max_hops));       // 88
    file.write((uint8_t *)&_prefs.rx_boosted_gain, sizeof(_prefs.rx_boosted_gain));         // 89
    file.write((uint8_t *)_prefs.default_scope_name, sizeof(_prefs.default_scope_name));    // 90
    file.write((uint8_t *)_prefs.default_scope_key, sizeof(_prefs.default_scope_key));     // 121
    file.write((uint8_t *)&_prefs.chat_name_mode, sizeof(_prefs.chat_name_mode));           // 137
    file.write((uint8_t *)_prefs.chat_name_custom, sizeof(_prefs.chat_name_custom));        // 138
    file.write((uint8_t *)&_prefs.auto_advert_enabled, sizeof(_prefs.auto_advert_enabled)); // 170
    file.write((uint8_t *)&_prefs.client_repeat_force, sizeof(_prefs.client_repeat_force)); // 171
    file.write((uint8_t *)&_prefs.duty_soft_pct, sizeof(_prefs.duty_soft_pct));             // 172
    file.write((uint8_t *)&_prefs.duty_hard_pct, sizeof(_prefs.duty_hard_pct));             // 173
    file.write((uint8_t *)_prefs.bake_scope_name, sizeof(_prefs.bake_scope_name));          // 174
    file.write((uint8_t *)_prefs.bake_scope_key,  sizeof(_prefs.bake_scope_key));           // 205
    file.write((uint8_t *)_prefs.override_scope_name, sizeof(_prefs.override_scope_name));  // 221
    file.write((uint8_t *)_prefs.override_scope_key,  sizeof(_prefs.override_scope_key));   // 252
    file.write((uint8_t *)&_prefs.override_expiry, sizeof(_prefs.override_expiry));         // 268
    file.write((uint8_t *)&_prefs.trace_flags_persistent, sizeof(_prefs.trace_flags_persistent)); // 272
    file.write((uint8_t *)&_prefs.gps_power_mode, sizeof(_prefs.gps_power_mode));                   // 274
    file.write((uint8_t *)&_prefs.gps_lead_min, sizeof(_prefs.gps_lead_min));                       // 275
    file.write((uint8_t *)&_prefs.tx_delay_factor, sizeof(_prefs.tx_delay_factor));                 // 276
    file.write((uint8_t *)&_prefs.direct_tx_delay_factor, sizeof(_prefs.direct_tx_delay_factor));   // 280
    file.write((uint8_t *)&_prefs.repeat_scope_mode, sizeof(_prefs.repeat_scope_mode));             // 284
    file.write((uint8_t *)&_prefs.scope_buildin_status_count, sizeof(_prefs.scope_buildin_status_count)); // 285
    file.write((uint8_t *)_prefs.scope_buildin_status, sizeof(_prefs.scope_buildin_status));         // 286 (8*32=256 Byte)
    file.write((uint8_t *)&_prefs.scope_extras_count, sizeof(_prefs.scope_extras_count));            // 542
    file.write((uint8_t *)_prefs.scope_extras, sizeof(_prefs.scope_extras));                          // 543 (52*16=832 Byte)
    file.write((uint8_t *)&_prefs.flood_max_scope_region, sizeof(_prefs.flood_max_scope_region));                  // 1375
    file.write((uint8_t *)&_prefs.flood_max, sizeof(_prefs.flood_max));                              // 1376
    file.write((uint8_t *)&_prefs.scope_advert_auto, sizeof(_prefs.scope_advert_auto));              // 1377
    file.write((uint8_t *)&_prefs.repeater_profile, sizeof(_prefs.repeater_profile));                // 1378
    file.write((uint8_t *)&_prefs.scope_repeater_auto, sizeof(_prefs.scope_repeater_auto));          // 1379
    file.write((uint8_t *)_prefs.owner_info, sizeof(_prefs.owner_info));                              // 1380 (120 byte)
    file.write((uint8_t *)&_prefs.advert_role, sizeof(_prefs.advert_role));                          // 1500
    file.write((uint8_t *)&_prefs.loop_detect, sizeof(_prefs.loop_detect));                          // 1501
    file.write((uint8_t *)&_prefs.msg_store_flash, sizeof(_prefs.msg_store_flash));                  // 1502
    file.write((uint8_t *)_prefs.msg_store_limit, sizeof(_prefs.msg_store_limit));                   // 1503
    file.write((uint8_t *)&_prefs.log_flags, sizeof(_prefs.log_flags));                              // 1508
    file.write((uint8_t *)&_prefs.flood_max_infra, sizeof(_prefs.flood_max_infra));          // 1509
    file.write((uint8_t *)&_prefs.flood_max_req_resp, sizeof(_prefs.flood_max_req_resp));    // 1510
    // Wunschliste 32 v2
    file.write((uint8_t *)&_prefs.flood_max_unknown_chan, sizeof(_prefs.flood_max_unknown_chan)); // 1511
    file.write((uint8_t *)&_prefs.channel_hops_count, sizeof(_prefs.channel_hops_count));         // 1512
    file.write((uint8_t *)_prefs.channel_hops_list, sizeof(_prefs.channel_hops_list));            // 1513 (MAX_GROUP_CHANNELS*5)
    // Wunschliste 31
    file.write((uint8_t *)&_prefs.time_sync_mode, sizeof(_prefs.time_sync_mode));            // 1512+MAX_GROUP_CHANNELS
    file.write((uint8_t *)_prefs.time_sync_sources, sizeof(_prefs.time_sync_sources));       // +1
    // Wunschliste 39
    file.write((uint8_t *)&_prefs.flood_max_unscoped_companions,
               sizeof(_prefs.flood_max_unscoped_companions));
    // Reise-Wunsch 2026-06-08: messages_append_scope_to_name
    file.write((uint8_t *)&_prefs.messages_append_scope_to_name,
               sizeof(_prefs.messages_append_scope_to_name));
    // Reise-Wunsch 2026-06-09: passwd_admin/passwd_guest
    file.write((uint8_t *)_prefs.passwd_admin, sizeof(_prefs.passwd_admin));
    file.write((uint8_t *)_prefs.passwd_guest, sizeof(_prefs.passwd_guest));
    // Reise-Wunsch 2026-06-09 (Wunschliste 46): Filter-Listen
    file.write((uint8_t *)&_prefs.filter_sender_drop_count,
               sizeof(_prefs.filter_sender_drop_count));
    file.write((uint8_t *)_prefs.filter_sender_drop,
               sizeof(_prefs.filter_sender_drop));
    file.write((uint8_t *)&_prefs.filter_text_drop_count,
               sizeof(_prefs.filter_text_drop_count));
    file.write((uint8_t *)_prefs.filter_text_drop,
               sizeof(_prefs.filter_text_drop));
    // Reise-Wunsch 2026-06-09 (Wunschliste 45): LBT + AGC-Reset
    file.write((uint8_t *)&_prefs.interference_threshold,
               sizeof(_prefs.interference_threshold));
    file.write((uint8_t *)&_prefs.agc_reset_interval,
               sizeof(_prefs.agc_reset_interval));
    // Wunschliste 46 Phase 2 (2026-06-10): channel-filter Masks.
    file.write((uint8_t *)&_prefs.filter_sender_drop_on_channel_mask,
               sizeof(_prefs.filter_sender_drop_on_channel_mask));
    file.write((uint8_t *)&_prefs.filter_sender_drop_exempt_mask,
               sizeof(_prefs.filter_sender_drop_exempt_mask));
    file.write((uint8_t *)&_prefs.filter_text_drop_on_channel_mask,
               sizeof(_prefs.filter_text_drop_on_channel_mask));
    file.write((uint8_t *)&_prefs.filter_text_drop_exempt_mask,
               sizeof(_prefs.filter_text_drop_exempt_mask));
    // Wunschliste 46 Phase 3 (2026-06-10): keep-Listen.
    file.write((uint8_t *)&_prefs.filter_sender_keep_count,
               sizeof(_prefs.filter_sender_keep_count));
    file.write((uint8_t *)_prefs.filter_sender_keep,
               sizeof(_prefs.filter_sender_keep));
    file.write((uint8_t *)&_prefs.filter_text_keep_count,
               sizeof(_prefs.filter_text_keep_count));
    file.write((uint8_t *)_prefs.filter_text_keep,
               sizeof(_prefs.filter_text_keep));
    // Wunschliste 46 Phase 2 v2 (2026-06-10): pro-Pattern channel-filter.
    file.write((uint8_t *)_prefs.filter_sender_drop_chan_on,
               sizeof(_prefs.filter_sender_drop_chan_on));
    file.write((uint8_t *)_prefs.filter_sender_drop_chan_ex,
               sizeof(_prefs.filter_sender_drop_chan_ex));
    file.write((uint8_t *)_prefs.filter_sender_keep_chan_on,
               sizeof(_prefs.filter_sender_keep_chan_on));
    file.write((uint8_t *)_prefs.filter_sender_keep_chan_ex,
               sizeof(_prefs.filter_sender_keep_chan_ex));
    file.write((uint8_t *)_prefs.filter_text_drop_chan_on,
               sizeof(_prefs.filter_text_drop_chan_on));
    file.write((uint8_t *)_prefs.filter_text_drop_chan_ex,
               sizeof(_prefs.filter_text_drop_chan_ex));
    file.write((uint8_t *)_prefs.filter_text_keep_chan_on,
               sizeof(_prefs.filter_text_keep_chan_on));
    file.write((uint8_t *)_prefs.filter_text_keep_chan_ex,
               sizeof(_prefs.filter_text_keep_chan_ex));
    // Wunschliste 46 Phase 5 (2026-06-10): scope-Filter + Repeat-Achse.
    file.write((uint8_t *)&_prefs.filter_scope_drop_count,
               sizeof(_prefs.filter_scope_drop_count));
    file.write((uint8_t *)_prefs.filter_scope_drop,
               sizeof(_prefs.filter_scope_drop));
    file.write((uint8_t *)_prefs.filter_scope_drop_chan_on,
               sizeof(_prefs.filter_scope_drop_chan_on));
    file.write((uint8_t *)_prefs.filter_scope_drop_chan_ex,
               sizeof(_prefs.filter_scope_drop_chan_ex));
    file.write((uint8_t *)&_prefs.filter_scope_keep_count,
               sizeof(_prefs.filter_scope_keep_count));
    file.write((uint8_t *)_prefs.filter_scope_keep,
               sizeof(_prefs.filter_scope_keep));
    file.write((uint8_t *)_prefs.filter_scope_keep_chan_on,
               sizeof(_prefs.filter_scope_keep_chan_on));
    file.write((uint8_t *)_prefs.filter_scope_keep_chan_ex,
               sizeof(_prefs.filter_scope_keep_chan_ex));
    file.write((uint8_t *)&_prefs.filter_unknown_channel_repeat,
               sizeof(_prefs.filter_unknown_channel_repeat));
    // Wunschliste 43 (refactored 2026-06-11): BLE-Power profile + active
    file.write((uint8_t *)&_prefs.bluetooth_profile,
               sizeof(_prefs.bluetooth_profile));
    file.write((uint8_t *)&_prefs.bluetooth_active,
               sizeof(_prefs.bluetooth_active));
    // Wunschliste 50 Phase 1 (2026-06-11): TZ-Override
    file.write((uint8_t *)&_prefs.tz_mode,
               sizeof(_prefs.tz_mode));
    file.write((uint8_t *)&_prefs.tz_offset_min,
               sizeof(_prefs.tz_offset_min));

    // Wunschliste 46 Phase 4 (2026-06-11): Advert/Pubkey Filter.
    file.write((uint8_t *)&_prefs.filter_advert_drop_name,
               sizeof(_prefs.filter_advert_drop_name));
    file.write((uint8_t *)&_prefs.filter_advert_drop_name_count,
               sizeof(_prefs.filter_advert_drop_name_count));
    file.write((uint8_t *)&_prefs.filter_advert_drop_pubkey,
               sizeof(_prefs.filter_advert_drop_pubkey));
    file.write((uint8_t *)&_prefs.filter_advert_drop_pubkey_count,
               sizeof(_prefs.filter_advert_drop_pubkey_count));
    file.write((uint8_t *)&_prefs.filter_sender_drop_pubkey,
               sizeof(_prefs.filter_sender_drop_pubkey));
    file.write((uint8_t *)&_prefs.filter_sender_drop_pubkey_count,
               sizeof(_prefs.filter_sender_drop_pubkey_count));

    // Wunschliste 10 (2026-06-11): Serial-CLI Persistent.
    file.write((uint8_t *)&_prefs.serial_cli_persist_on,
               sizeof(_prefs.serial_cli_persist_on));

    // Wunschliste 58 Phase B (2026-06-13): CPU-Clock-Frequenz.
    file.write((uint8_t *)&_prefs.cpu_clock_mhz,
               sizeof(_prefs.cpu_clock_mhz));

    // Wunschliste 58 Phase A (2026-06-14): Display Wake-Mode.
    file.write((uint8_t *)&_prefs.display_wake_mode,
               sizeof(_prefs.display_wake_mode));

    // Wunschliste 53 Phase 1+2 (2026-06-14): Hardware-Watchdog Pref.
    file.write((uint8_t *)&_prefs.watchdog_mode,
               sizeof(_prefs.watchdog_mode));

    // DL9SAU 2026-06-17 (Wunschliste 75): Buzzer-Profile.
    file.write((uint8_t *)&_prefs.buzzer_profile,
               sizeof(_prefs.buzzer_profile));

    // DL9SAU 2026-06-17 (Wunschliste 81): GPS-Profile.
    file.write((uint8_t *)&_prefs.gps_profile,
               sizeof(_prefs.gps_profile));

    // DL9SAU 2026-06-18 (Wunschliste 83): RX-Disabled.
    file.write((uint8_t *)&_prefs.rx_disabled,
               sizeof(_prefs.rx_disabled));

    // DL9SAU 2026-06-18 (Wunschliste 81 Phase 3): gps_lead_secs.
    file.write((uint8_t *)&_prefs.gps_lead_secs,
               sizeof(_prefs.gps_lead_secs));

    // DL9SAU 2026-06-18 (Wunschliste 89/90/91): Battery + USB.
    file.write((uint8_t *)&_prefs.batt_chemistry,
               sizeof(_prefs.batt_chemistry));
    file.write((uint8_t *)&_prefs.batt_min_mv,
               sizeof(_prefs.batt_min_mv));
    file.write((uint8_t *)&_prefs.usb_loss_shutdown_min,
               sizeof(_prefs.usb_loss_shutdown_min));
    // DL9SAU 2026-06-20: usb_wake_action entfernt, reserved-Byte bleibt.
    file.write((uint8_t *)&_prefs._reserved_usb_wake_action,
               sizeof(_prefs._reserved_usb_wake_action));
    // DL9SAU 2026-06-20: shutdown_pending Sentinel.
    file.write((uint8_t *)&_prefs.shutdown_pending,
               sizeof(_prefs.shutdown_pending));
    // DL9SAU 2026-07-02: channel_no_scope_behavior.
    file.write((uint8_t *)&_prefs.channel_no_scope_behavior,
               sizeof(_prefs.channel_no_scope_behavior));
    // 2026-07-06 Upstream-Merge: cad_enabled.
    file.write((uint8_t *)&_prefs.cad_enabled, sizeof(_prefs.cad_enabled));
    // DL9SAU 2026-07-12: button_press_allow_shutdown.
    file.write((uint8_t *)&_prefs.button_press_allow_shutdown,
               sizeof(_prefs.button_press_allow_shutdown));
    // DL9SAU 2026-07-12: batt_min_mv_boot (uint16). APPEND ans Ende.
    file.write((uint8_t *)&_prefs.batt_min_mv_boot,
               sizeof(_prefs.batt_min_mv_boot));

#ifdef ESP_PLATFORM
    // DL9SAU 2026-06-16: Reboot-into-OTA Flag (ESP32-only).
    file.write((uint8_t *)&_prefs.ota_pending,
               sizeof(_prefs.ota_pending));
#endif
    // DL9SAU 2026-07-13 (Level-Umbau): trace_levels (uint64). APPEND ans Ende
    // -- letztes Feld auf beiden Plattformen (siehe loadPrefsInt).
    file.write((uint8_t *)&_prefs.trace_levels,
               sizeof(_prefs.trace_levels));
    // DL9SAU 2026-07-14: auto_on_when_charging (uint8). APPEND ans Ende.
    file.write((uint8_t *)&_prefs.auto_on_when_charging,
               sizeof(_prefs.auto_on_when_charging));
    // DL9SAU 2026-07-21 (#3/#4): Advert-Scope + Mindest-Intervall. APPEND ans
    // Ende, gleiche Reihenfolge wie in loadPrefsInt.
    file.write((uint8_t *)&_prefs.advert_periodic_scope,
               sizeof(_prefs.advert_periodic_scope));
    file.write((uint8_t *)&_prefs.advert_nightly_scope,
               sizeof(_prefs.advert_nightly_scope));
    file.write((uint8_t *)&_prefs.advert_periodic_min_min,
               sizeof(_prefs.advert_periodic_min_min));
  };  // Ende writeBody-Lambda

  const char* TMP  = "/new_prefs.tmp";
  const char* REAL = "/new_prefs";
  bool ok = false;
  File file = openWrite(_fs, TMP);
  if (file) {
    CountingWriter cw{ file, true };
    writeBody(cw);
    file.close();
    ok = cw.ok;   // jeder write() lieferte die erwartete Laenge
  }
  if (ok) {
    ok = _fs->rename(TMP, REAL);   // littlefs: atomar, ersetzt existierende
  }
  if (!ok) {
    // Write unvollstaendig oder rename gescheitert (FS voll o.ae.) -> tmp weg,
    // Direkt-Write in die echte Datei wie frueher. openWrite(REAL) gibt den
    // alten 8KB-Block frei -> genug Platz fuer den Direkt-Write (best guess).
    _fs->remove(TMP);
    File f = openWrite(_fs, REAL);
    if (f) {
      CountingWriter cw{ f, true };
      writeBody(cw);
      f.close();
    }
  }
}

void DataStore::loadContacts(DataStoreHost* host) {
#if defined(NRF52_PLATFORM) && defined(NRF52_BOOT_TRACE)
  Serial.println("\r\n# [T1000-E diag] LC1 pre openRead /contacts3"); Serial.flush();
#endif
File file = openRead(_getContactsChannelsFS(), "/contacts3");
#if defined(NRF52_PLATFORM) && defined(NRF52_BOOT_TRACE)
  bool file_ok = (bool)file;
  Serial.print("\r\n# [T1000-E diag] LC2 post openRead, file="); Serial.println(file_ok ? "ok" : "null"); Serial.flush();
  Serial.print("# [T1000-E diag] LC2b file.size="); Serial.println((unsigned long)file.size()); Serial.flush();
#endif
    if (file) {
#if defined(NRF52_PLATFORM) && defined(NRF52_BOOT_TRACE)
      Serial.println("# [T1000-E diag] LC2c entered if(file)"); Serial.flush();
#endif
      bool full = false;
      int rec = 0;
      while (!full) {
#if defined(NRF52_PLATFORM) && defined(NRF52_BOOT_TRACE)
        if (rec < 5) { Serial.print("# [T1000-E diag] LC3 rec="); Serial.println(rec); Serial.flush(); }
        rec++;
#endif
        ContactInfo c;
        uint8_t pub_key[32];
        uint8_t unused;

#if defined(NRF52_PLATFORM) && defined(NRF52_BOOT_TRACE)
        if (rec <= 1) { Serial.println("# [T1000-E diag] LC4 pre file.read(pub_key,32)"); Serial.flush(); }
#endif
        bool success = (file.read(pub_key, 32) == 32);
#if defined(NRF52_PLATFORM) && defined(NRF52_BOOT_TRACE)
        if (rec <= 1) { Serial.print("# [T1000-E diag] LC5 post file.read pub_key success="); Serial.println(success ? "y" : "n"); Serial.flush(); }
#endif
        success = success && (file.read((uint8_t *)&c.name, 32) == 32);
        success = success && (file.read(&c.type, 1) == 1);
        success = success && (file.read(&c.flags, 1) == 1);
        success = success && (file.read(&unused, 1) == 1);
        success = success && (file.read((uint8_t *)&c.sync_since, 4) == 4); // was 'reserved'
        success = success && (file.read((uint8_t *)&c.out_path_len, 1) == 1);
        success = success && (file.read((uint8_t *)&c.last_advert_timestamp, 4) == 4);
        success = success && (file.read(c.out_path, 64) == 64);
        success = success && (file.read((uint8_t *)&c.lastmod, 4) == 4);
        success = success && (file.read((uint8_t *)&c.gps_lat, 4) == 4);
        success = success && (file.read((uint8_t *)&c.gps_lon, 4) == 4);

        if (!success) break; // EOF

        c.id = mesh::Identity(pub_key);
        if (!host->onContactLoaded(c)) full = true;
      }
      file.close();
    }
}

void DataStore::saveContacts(DataStoreHost* host, bool (*filter)(const ContactInfo& c)) {
  File file = openWrite(_getContactsChannelsFS(), "/contacts3");
  if (file) {
    uint32_t idx = 0;
    ContactInfo c;
    uint8_t unused = 0;

    while (host->getContactForSave(idx, c)) {
      if (filter && !filter(c)) {
        idx++;  // advance to next contact
        continue;
      }
      bool success = (file.write(c.id.pub_key, 32) == 32);
      success = success && (file.write((uint8_t *)&c.name, 32) == 32);
      success = success && (file.write(&c.type, 1) == 1);
      success = success && (file.write(&c.flags, 1) == 1);
      success = success && (file.write(&unused, 1) == 1);
      success = success && (file.write((uint8_t *)&c.sync_since, 4) == 4);
      success = success && (file.write((uint8_t *)&c.out_path_len, 1) == 1);
      success = success && (file.write((uint8_t *)&c.last_advert_timestamp, 4) == 4);
      success = success && (file.write(c.out_path, 64) == 64);
      success = success && (file.write((uint8_t *)&c.lastmod, 4) == 4);
      success = success && (file.write((uint8_t *)&c.gps_lat, 4) == 4);
      success = success && (file.write((uint8_t *)&c.gps_lon, 4) == 4);

      if (!success) break; // write failed

      idx++;  // advance to next contact
    }
    file.close();
  }
}

void DataStore::loadChannels(DataStoreHost* host) {
    File file = openRead(_getContactsChannelsFS(), "/channels2");
    if (file) {
      bool full = false;
      uint8_t channel_idx = 0;
      while (!full) {
        ChannelDetails ch;
        uint8_t unused[4];

        bool success = (file.read(unused, 4) == 4);
        success = success && (file.read((uint8_t *)ch.name, 32) == 32);
        success = success && (file.read((uint8_t *)ch.channel.secret, 32) == 32);

        if (!success) break; // EOF

        if (host->onChannelLoaded(channel_idx, ch)) {
          channel_idx++;
        } else {
          full = true;
        }
      }
      file.close();
    }
}

void DataStore::saveChannels(DataStoreHost* host) {
  // DL9SAU 2026-07-12: temp+rename statt in-place (analog savePrefs). Ein
  // WDT-Reset/Absturz MITTEN im Write laesst nur die tmp zurueck -> die echte
  // /channels2 bleibt intakt. Channels ist klein (68B/Record) und liegt auf
  // dem ExtraFS (39.5KB frei) -> 2x passt problemlos. Der Record-Writer
  // trackt bereits den Erfolg (success &=) -> kein Magic-Trailer noetig, der
  // Loop-Erfolg gated den rename. Fallback (FS voll/Fehler): tmp weg +
  // Direkt-Write wie zuvor. writeBody ist idempotent (getChannelForSave liest
  // per Index aus der RAM-Tabelle) -> im Fallback zweiter Aufruf ok.
  FILESYSTEM* fs = _getContactsChannelsFS();
  const char* TMP  = "/channels2.tmp";
  const char* REAL = "/channels2";
  auto writeBody = [&](File& file) -> bool {
    uint8_t channel_idx = 0;
    ChannelDetails ch;
    uint8_t unused[4];
    memset(unused, 0, 4);
    while (host->getChannelForSave(channel_idx, ch)) {
      bool success = (file.write(unused, 4) == 4);
      success = success && (file.write((uint8_t *)ch.name, 32) == 32);
      success = success && (file.write((uint8_t *)ch.channel.secret, 32) == 32);
      if (!success) return false; // write failed (FS voll)
      channel_idx++;
    }
    return true;
  };

  bool ok = false;
  File file = openWrite(fs, TMP);
  if (file) {
    ok = writeBody(file);
    file.close();
  }
  if (ok) {
    ok = fs->rename(TMP, REAL);   // littlefs: atomar, ersetzt existierende
  }
  if (!ok) {
    fs->remove(TMP);
    File f = openWrite(fs, REAL);
    if (f) { writeBody(f); f.close(); }
  }
}

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)

#define MAX_ADVERT_PKT_LEN   (2 + 32 + PUB_KEY_SIZE + 4 + SIGNATURE_SIZE + MAX_ADVERT_DATA_SIZE)

struct BlobRec {
  uint32_t timestamp;
  uint8_t  key[7];
  uint8_t  len;
  uint8_t  data[MAX_ADVERT_PKT_LEN];
};

void DataStore::checkAdvBlobFile() {
  if (!_getContactsChannelsFS()->exists("/adv_blobs")) {
    File file = openWrite(_getContactsChannelsFS(), "/adv_blobs");
    if (file) {
      BlobRec zeroes;
      memset(&zeroes, 0, sizeof(zeroes));
      for (int i = 0; i < MAX_BLOBRECS; i++) {     // pre-allocate to fixed size
        file.write((uint8_t *) &zeroes, sizeof(zeroes));
      }
      file.close();
    }
  }
}

void DataStore::migrateToSecondaryFS() {
  // migrate old adv_blobs, contacts3 and channels2 files to secondary FS if they don't already exist
  if (!_fsExtra->exists("/adv_blobs")) {
    if (_fs->exists("/adv_blobs")) {
    File oldAdvBlobs = openRead(_fs, "/adv_blobs");
    File newAdvBlobs = openWrite(_fsExtra, "/adv_blobs");

    if (oldAdvBlobs && newAdvBlobs) {
      BlobRec rec;
      size_t count = 0;

      // Copy 20 BlobRecs from old to new
      while (count < 20 && oldAdvBlobs.read((uint8_t *)&rec, sizeof(rec)) == sizeof(rec)) {
        newAdvBlobs.seek(count * sizeof(BlobRec));
        newAdvBlobs.write((uint8_t *)&rec, sizeof(rec));
        count++;
      }
    }
    if (oldAdvBlobs) oldAdvBlobs.close();
    if (newAdvBlobs) newAdvBlobs.close();
    _fs->remove("/adv_blobs");
    }
  }
  if (!_fsExtra->exists("/contacts3")) {
    if (_fs->exists("/contacts3")) {
      File oldFile = openRead(_fs, "/contacts3");
      File newFile = openWrite(_fsExtra, "/contacts3");

      if (oldFile && newFile) {
        uint8_t buf[64];
        int n;
        while ((n = oldFile.read(buf, sizeof(buf))) > 0) {
          newFile.write(buf, n);
        }
      }
      if (oldFile) oldFile.close();
      if (newFile) newFile.close();
      _fs->remove("/contacts3");
    }
  }
  if (!_fsExtra->exists("/channels2")) {
    if (_fs->exists("/channels2")) {
      File oldFile = openRead(_fs, "/channels2");
      File newFile = openWrite(_fsExtra, "/channels2");

      if (oldFile && newFile) {
        uint8_t buf[64];
        int n;
        while ((n = oldFile.read(buf, sizeof(buf))) > 0) {
          newFile.write(buf, n);
        }
      }
      if (oldFile) oldFile.close();
      if (newFile) newFile.close();
      _fs->remove("/channels2");
    }
  }
  // cleanup nodes which have been testing the extra fs, copy _main.id and new_prefs back to primary
  if (_fsExtra->exists("/_main.id")) {
      if (_fs->exists("/_main.id")) {_fs->remove("/_main.id");}
      File oldFile = openRead(_fsExtra, "/_main.id");
      File newFile = openWrite(_fs, "/_main.id");

      if (oldFile && newFile) {
        uint8_t buf[64];
        int n;
        while ((n = oldFile.read(buf, sizeof(buf))) > 0) {
          newFile.write(buf, n);
        }
      }
      if (oldFile) oldFile.close();
      if (newFile) newFile.close();
      _fsExtra->remove("/_main.id");
  }
  if (_fsExtra->exists("/new_prefs")) {
    if (_fs->exists("/new_prefs")) {_fs->remove("/new_prefs");}
      File oldFile = openRead(_fsExtra, "/new_prefs");
      File newFile = openWrite(_fs, "/new_prefs");

      if (oldFile && newFile) {
        uint8_t buf[64];
        int n;
        while ((n = oldFile.read(buf, sizeof(buf))) > 0) {
          newFile.write(buf, n);
        }
      }
      if (oldFile) oldFile.close();
      if (newFile) newFile.close();
      _fsExtra->remove("/new_prefs");
  }
  // remove files from where they should not be anymore
  if (_fs->exists("/adv_blobs")) {
    _fs->remove("/adv_blobs");
  }
  if (_fs->exists("/contacts3")) {
    _fs->remove("/contacts3");
  }
  if (_fs->exists("/channels2")) {
    _fs->remove("/channels2");
  }
  if (_fsExtra->exists("/_main.id")) {
    _fsExtra->remove("/_main.id");
  }
  if (_fsExtra->exists("/new_prefs")) {
    _fsExtra->remove("/new_prefs");
  }
}

uint8_t DataStore::getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) {
  File file = openRead(_getContactsChannelsFS(), "/adv_blobs");
  uint8_t len = 0;  // 0 = not found
  if (file) {
    BlobRec tmp;
    while (file.read((uint8_t *) &tmp, sizeof(tmp)) == sizeof(tmp)) {
      if (memcmp(key, tmp.key, sizeof(tmp.key)) == 0) {  // only match by 7 byte prefix
        len = tmp.len;
        memcpy(dest_buf, tmp.data, len);
        break;
      }
    }
    file.close();
  }
  return len;
}

bool DataStore::putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], uint8_t len) {
  if (len < PUB_KEY_SIZE+4+SIGNATURE_SIZE || len > MAX_ADVERT_PKT_LEN) return false;
  checkAdvBlobFile();
  File file = _getContactsChannelsFS()->open("/adv_blobs", FILE_O_WRITE);
  if (file) {
    uint32_t pos = 0, found_pos = 0;
    uint32_t min_timestamp = 0xFFFFFFFF;

    // search for matching key OR evict by oldest timestamp
    BlobRec tmp;
    file.seek(0);
    while (file.read((uint8_t *) &tmp, sizeof(tmp)) == sizeof(tmp)) {
      if (memcmp(key, tmp.key, sizeof(tmp.key)) == 0) {  // only match by 7 byte prefix
        found_pos = pos;
        break;
      }
      if (tmp.timestamp < min_timestamp) {
        min_timestamp = tmp.timestamp;
        found_pos = pos;
      }

      pos += sizeof(tmp);
    }

    memcpy(tmp.key, key, sizeof(tmp.key));  // just record 7 byte prefix of key
    memcpy(tmp.data, src_buf, len);
    tmp.len = len;
    tmp.timestamp = _clock->getCurrentTime();

    file.seek(found_pos);
    file.write((uint8_t *) &tmp, sizeof(tmp));

    file.close();
    return true;
  }
  return false; // error
}
bool DataStore::deleteBlobByKey(const uint8_t key[], int key_len) {
  return true; // this is just a stub on NRF52/STM32 platforms
}
#else
inline void makeBlobPath(const uint8_t key[], int key_len, char* path, size_t path_size) {
  char fname[18];
  if (key_len > 8) key_len = 8; // just use first 8 bytes (prefix)
  mesh::Utils::toHex(fname, key, key_len);
  sprintf(path, "/bl/%s", fname);
}

uint8_t DataStore::getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) {
  char path[64];
  makeBlobPath(key, key_len, path, sizeof(path));

  if (_fs->exists(path)) {
    File f = openRead(_fs, path);
    if (f) {
      int len = f.read(dest_buf, 255); // currently MAX 255 byte blob len supported!!
      f.close();
      return len;
    }
  }
  return 0; // not found
}

bool DataStore::putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], uint8_t len) {
  char path[64];
  makeBlobPath(key, key_len, path, sizeof(path));

  File f = openWrite(_fs, path);
  if (f) {
    int n = f.write(src_buf, len);
    f.close();
    if (n == len) return true; // success!

    _fs->remove(path); // blob was only partially written!
  }
  return false; // error
}

bool DataStore::deleteBlobByKey(const uint8_t key[], int key_len) {
  char path[64];
  makeBlobPath(key, key_len, path, sizeof(path));

  _fs->remove(path);
  
  return true; // return true even if file did not exist
}
#endif
