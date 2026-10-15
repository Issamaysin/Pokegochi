#pragma once

#include <cstddef>
#include <cstdint>
#include "services/WirelessUpdateProtocol.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <FS.h>
#include <esp_ota_ops.h>
#endif

class WirelessUpdate {
 public:
  using DeviceState = PokegochiUpdateProtocol::DeviceState;
  using ErrorCode = PokegochiUpdateProtocol::ErrorCode;
  using SaveCommitCallback = bool (*)(void* context);
  using SaveResetCallback = bool (*)(void* context);
  using DiagnosticReadCallback = size_t (*)(void* context, uint8_t section,
                                            uint16_t selector, char* output,
                                            size_t capacity);

  // Called immediately after the SD mount and before the asset pack is used.
  // It completes an interrupted atomic commit or restores the previous pack
  // when the bootloader returned to the previous application partition.
  bool recoverAtBoot(bool sdMounted);
  // Called after the normal save + asset diagnostics. A new image is accepted
  // only after both stores passed their ordinary production checks.
  void confirmBoot(bool healthy);

  bool begin(uint32_t playerId, const char* firmwareVersion,
             uint16_t currentAssetPackVersion, bool sdMounted);
  // The update transport deliberately sees the save as an opaque byte range.
  // Persistence and default initialization remain owned by PersistentSave.
  void configureSaveManagement(void* saveData, uint32_t saveBytes,
                               uint16_t saveFormatVersion,
                               SaveCommitCallback commitCallback,
                               SaveResetCallback resetCallback,
                               void* callbackContext);
  // The callback only formats read-only state into the supplied buffer. It
  // runs on the normal Arduino loop, never from a BLE callback task.
  void configureDiagnostics(DiagnosticReadCallback callback, void* context);
  void update();
  void shutdown(bool discardReceivingSession = false);
  void abort();

  bool active() const { return active_; }
  bool connected() const { return connected_; }
  bool busy() const;
  bool canAbort() const {
    return busy() && state_ != DeviceState::Committing &&
           state_ != DeviceState::Rebooting;
  }
  DeviceState state() const { return state_; }
  ErrorCode error() const { return error_; }
  uint32_t pairingCode() const { return pairingCode_; }
  uint32_t playerId() const { return playerId_; }
  uint32_t bytesDone() const { return packageBytesDone_; }
  uint32_t bytesTotal() const { return packageBytesTotal_; }
  uint8_t progressPercent() const;
  const char* statusText() const { return status_; }
  bool takeChanged();
  bool takeSynchronizedTime(uint32_t& localEpochSeconds);

  // A live log uses the authenticated updater link but is independent from
  // the updater screen. The UI can close after the host starts capture while
  // the BLE transport keeps running in the background.
  bool liveLogActive() const { return liveLogEnabled_; }
  bool takeLiveLogStarted();
  void detachLiveUi() { liveUiDetached_ = true; }
  void emitLiveLog(const char* format, ...);

  // BLE callbacks only enqueue bounded packets. All flash and SD writes run
  // later on Arduino's main loop, never inside the NimBLE host task.
  void transportConnected();
  void transportDisconnected();
  void transportControl(const uint8_t* bytes, size_t length);
  void transportData(const uint8_t* bytes, size_t length);

 private:
  enum class JournalPhase : uint8_t {
    Receiving = 1,
    Committing,
    AwaitingValidation,
    Accepted,
  };

#pragma pack(push, 1)
  struct Journal {
    char magic[4];
    uint8_t version;
    uint8_t phase;
    uint8_t verifiedAssetMask;
    uint8_t committedAssetMask;
    uint8_t firmwareReady;
    uint8_t bootAttempts;
    // Assets already installed with the exact manifest size and SHA-256 do
    // not need to cross BLE again. Persist this separately because verified
    // staged assets still need the atomic ready -> final commit.
    uint8_t reusedAssetMask;
    uint8_t reserved;
    char targetPartition[8];
    char previousPartition[8];
    uint32_t packageSequence;
    uint32_t checksum;
  };
#pragma pack(pop)

  bool startTransport();
  void stopTransport();
  void processControl(const uint8_t* bytes, size_t length);
  void processData(const uint8_t* bytes, size_t length);
  void sendStatus(PokegochiUpdateProtocol::StatusCode code);
  bool sendDiagnostic(uint8_t section, uint16_t selector);
  void setState(DeviceState state, const char* status,
                ErrorCode error = ErrorCode::None);
  void fail(ErrorCode error, const char* status);
  bool verifyManifest();
  bool beginObject(PokegochiUpdateProtocol::ObjectKind kind,
                   PokegochiUpdateProtocol::AssetId assetId);
  bool finishObject();
  bool beginFirmware();
  bool beginAsset(PokegochiUpdateProtocol::AssetId id);
  bool finishFirmware();
  bool finishAsset();
  bool commit();
  bool commitAssets(Journal& journal);
  bool verifyInstalledAssets() const;
  bool rollbackAssets(Journal& journal);
  void finishSuccessfulUpdate();
  void closeCurrentObject(bool abortFirmware);
  void clearTransferArtifacts(bool keepBackups = false);
  bool ensureUpdateDirectories();
  bool loadJournal(Journal& journal) const;
  bool saveJournal(Journal& journal) const;
  void removeJournal() const;
  bool hashFile(const char* path, uint8_t output[32]) const;
  bool hashPartition(const void* partition, uint32_t bytes, uint8_t output[32]) const;
  bool signatureValid(const PokegochiUpdateProtocol::Manifest& manifest) const;
  uint32_t acceptedPackageSequence() const;
  void storeAcceptedPackageSequence(uint32_t sequence) const;
  bool loadPendingBoot(char target[8], char previous[8], uint32_t& sequence,
                       uint8_t& attempts) const;
  bool storePendingBoot(const char* target, const char* previous,
                        uint32_t sequence, uint8_t attempts) const;
  void clearPendingBoot() const;
  // `esp_ota_set_boot_partition` only requests a slot change. Read the
  // bootloader's authoritative choice back before telling the phone that a
  // candidate can reboot, otherwise a stale OTA record can silently retain
  // the older application slot.
  bool selectBootPartition(const char* label) const;
  void recalculatePackageProgress();
  bool prepareSaveExport();
  bool sendSaveExportChunk(const PokegochiUpdateProtocol::SaveExportChunkCommand& request);
  bool finishSaveExport();
  bool beginSaveImport(const PokegochiUpdateProtocol::SaveImportBeginCommand& request);
  bool finishSaveImport();
  bool resetSave();
  void closeSaveTransfer(bool removeFiles);

  bool active_ = false;
  bool initialized_ = false;
  bool connected_ = false;
  bool authenticated_ = false;
  bool manifestValid_ = false;
  bool changed_ = true;
  bool bootValidationPending_ = false;
  bool rebootScheduled_ = false;
  uint32_t rebootAtMs_ = 0;
  uint32_t playerId_ = 0;
  uint32_t pairingCode_ = 0;
  uint32_t synchronizedTime_ = 0;
  bool synchronizedTimePending_ = false;
  bool liveLogEnabled_ = false;
  bool liveLogStartPending_ = false;
  bool liveUiDetached_ = false;
  uint32_t liveLogSequence_ = 0;
  uint32_t liveLogLastHeartbeatMs_ = 0;
  uint16_t currentAssetPackVersion_ = 0;
  char firmwareVersion_[24]{};
  char status_[72] = "WIRELESS UPDATE IS OFF.";
  DeviceState state_ = DeviceState::Advertising;
  ErrorCode error_ = ErrorCode::None;

  uint8_t manifestBuffer_[PokegochiUpdateProtocol::kMaximumManifestBytes]{};
  uint16_t manifestExpectedBytes_ = 0;
  uint16_t manifestReceivedBytes_ = 0;
  PokegochiUpdateProtocol::Manifest manifest_{};
  Journal journal_{};

  PokegochiUpdateProtocol::ObjectKind objectKind_ =
      PokegochiUpdateProtocol::ObjectKind::Firmware;
  PokegochiUpdateProtocol::AssetId assetId_ =
      PokegochiUpdateProtocol::AssetId::Archive;
  uint32_t objectOffset_ = 0;
  uint32_t objectSize_ = 0;
  const uint8_t* objectHash_ = nullptr;
  uint32_t packageBytesDone_ = 0;
  uint32_t packageBytesTotal_ = 0;
  void* saveData_ = nullptr;
  uint32_t saveBytes_ = 0;
  uint16_t saveFormatVersion_ = 0;
  uint32_t saveImportFileCrc32_ = 0;
  SaveCommitCallback saveCommitCallback_ = nullptr;
  SaveResetCallback saveResetCallback_ = nullptr;
  void* saveCallbackContext_ = nullptr;
  DiagnosticReadCallback diagnosticReadCallback_ = nullptr;
  void* diagnosticCallbackContext_ = nullptr;
  // Keep the readback buffer below the ESP32's tight internal DRAM budget.
  // The transport chunks it over BLE, and the report deliberately truncates
  // only after the useful header/resource checks have been written.
  char diagnosticBuffer_[2048]{};

#if defined(ARDUINO_ARCH_ESP32)
  File assetFile_{};
  File saveTransferFile_{};
  esp_ota_handle_t otaHandle_ = 0;
  const esp_partition_t* targetPartition_ = nullptr;
#endif
};
