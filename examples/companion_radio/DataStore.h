#pragma once

#include <helpers/IdentityStore.h>
#include <helpers/ContactInfo.h>
#include <helpers/ChannelDetails.h>
#include "NodePrefs.h"

class DataStoreHost {
public:
  virtual bool onContactLoaded(const ContactInfo& contact) =0;
  virtual bool getContactForSave(uint32_t idx, ContactInfo& contact) =0;
  virtual bool onChannelLoaded(uint8_t channel_idx, const ChannelDetails& ch) =0;
  virtual bool getChannelForSave(uint8_t channel_idx, ChannelDetails& ch) =0;
};

class DataStore {
  FILESYSTEM* _fs;
  FILESYSTEM* _fsExtra;
  mesh::RTCClock* _clock;
  IdentityStore identity_store;

  void loadPrefsInt(const char *filename, NodePrefs& prefs, double& node_lat, double& node_lon);
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  void checkAdvBlobFile();
#endif

public:
  DataStore(FILESYSTEM& fs, mesh::RTCClock& clock);
  DataStore(FILESYSTEM& fs, FILESYSTEM& fsExtra, mesh::RTCClock& clock);
  void begin();
  bool formatFileSystem();
  FILESYSTEM* getPrimaryFS() const { return _fs; }
  FILESYSTEM* getSecondaryFS() const { return _fsExtra; }
  bool loadMainIdentity(mesh::LocalIdentity &identity);
  bool saveMainIdentity(const mesh::LocalIdentity &identity);
  void loadPrefs(NodePrefs& prefs, double& node_lat, double& node_lon);
  void savePrefs(const NodePrefs& prefs, double node_lat, double node_lon);
  void loadContacts(DataStoreHost* host);
  void saveContacts(DataStoreHost* host, bool (*filter)(const ContactInfo& c) = NULL);
  void loadChannels(DataStoreHost* host);
  void saveChannels(DataStoreHost* host);
  void migrateToSecondaryFS();
  uint8_t getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]);
  bool putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], uint8_t len);
  bool deleteBlobByKey(const uint8_t key[], int key_len);
  File openRead(const char* filename);
  File openRead(FILESYSTEM* fs, const char* filename);
  File openWriteFile(const char* filename);   // Wunschliste 19 Phase C: msg-bucket-persist
  // DL9SAU 2026-07-15: In-Place-Write OHNE remove -> Bloecke wiederverwendet
  // (weniger Alloc/Free-Churn + weniger GC-Druck = weniger Korruptions-Risiko auf
  // der kleinen InternalFS) UND crash-sicherer (COW: unterbrochener Write laesst
  // die alte Datei intakt, kein remove-dann-weg-Fenster). Caller MUSS: seek(0)
  // vor dem Schreiben, truncate() nach dem Schreiben (alten Tail kappen).
  File openWriteFileInPlace(const char* filename);
  bool removeFile(const char* filename);
  bool removeFile(FILESYSTEM* fs, const char* filename);
  // DL9SAU 2026-07-14: Rename auf der Primary-FS (nur Directory-Eintrag, fasst
  // Datei-Datenbloecke NICHT an) -- fuer Sideline-Recovery korrupter Dateien.
  bool renameFile(const char* from, const char* to);
  // DL9SAU 2026-07-12: Existenz-Check auf der Primary-FS (InternalFS) --
  // fuer den /shutdown_pending-Datei-Sentinel (touch/rm statt Prefs-Byte).
  bool fileExists(const char* filename) const;
  uint32_t getStorageUsedKb() const;
  uint32_t getStorageTotalKb() const;
  // DL9SAU 2026-07-12: Belegung BEIDER Partitionen in Bytes (fuer 'fsinfo' +
  // die temp+rename-Platzentscheidung). ext_* = 0 wenn kein ExtraFS.
  void getFsInfo(uint32_t& int_total, uint32_t& int_used,
                 uint32_t& ext_total, uint32_t& ext_used) const;

private:
  FILESYSTEM* _getContactsChannelsFS() const { if (_fsExtra) return _fsExtra; return _fs;};
};
