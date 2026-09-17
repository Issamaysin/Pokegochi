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

  // Called immediately after the SD mount and before the asset pack is used.
  // It completes an interrupted atomic commit or restores the previous pack
  // when the bootloader returned to the previous application partition.
  bool recoverAtBoot(bool sdMounted);
  // Called after the normal save + asset diagnostics. A new image is accepted
  // only after both stores passed their ordinary production checks.
  void confirmBoot(bool healthy);

  bool begin(uint32_t playerId, const char* firmwareVersion,
             uint16_t currentAssetPackVersion, bool sdMounted);
  void update();
  void shutdown(bool discardReceivingSession = false);
  void abort();

  bool active() const { return active_; }
  bool connected() const { return connected_; }
  bool busy() const;
  DeviceState state() const { return state_; }
  ErrorCode error() const { return error_; }
  uint32_t pairingCode() const { return pairingCode_; }
  uint32_t playerId() const { return playerId_; }
  uint32_t bytesDone() const { return packageBytesDone_; }
  uint32_t bytesTotal() const { return packageBytesTotal_; }
  uint8_t progressPercent() const;
  const char* statusText() const { return status_; }
  bool takeChanged();

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
  void recalculatePackageProgress();

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

#if defined(ARDUINO_ARCH_ESP32)
  File assetFile_{};
  esp_ota_handle_t otaHandle_ = 0;
  const esp_partition_t* targetPartition_ = nullptr;
#endif
};
