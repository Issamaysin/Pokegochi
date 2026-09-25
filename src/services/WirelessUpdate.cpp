#include "services/WirelessUpdate.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <SD.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/sha256.h>
#include "config/UpdateSigningKey.h"
#endif

namespace {
using namespace PokegochiUpdateProtocol;

constexpr char kJournalPath[] = "/pokegochi/update/state.bin";
constexpr char kJournalTempPath[] = "/pokegochi/update/state.tmp";
constexpr uint8_t kJournalVersion = 1;

struct AssetPaths {
  AssetId id;
  const char* partial;
  const char* ready;
  const char* finalPath;
  const char* backup;
};

constexpr AssetPaths kAssetPaths[] = {
    {AssetId::Archive,
     "/pokegochi/update/archive.part", "/pokegochi/update/archive.ready",
     "/pokegochi/pokegochi.pak", "/pokegochi/update/backup/archive.old"},
    {AssetId::PackVersion,
     "/pokegochi/update/pack_version.part", "/pokegochi/update/pack_version.ready",
     "/pokegochi/assets/pack_version.txt", "/pokegochi/update/backup/pack_version.old"},
    {AssetId::AnimationCatalog,
     "/pokegochi/update/animation_catalog.part", "/pokegochi/update/animation_catalog.ready",
     "/pokegochi/assets/animation_catalog.txt", "/pokegochi/update/backup/animation_catalog.old"},
    {AssetId::ManifestJson,
     "/pokegochi/update/manifest_json.part", "/pokegochi/update/manifest_json.ready",
     "/pokegochi/assets/manifest.json", "/pokegochi/update/backup/manifest_json.old"},
};

const AssetPaths* pathsFor(AssetId id) {
  for (const auto& paths : kAssetPaths) if (paths.id == id) return &paths;
  return nullptr;
}

uint8_t assetBit(AssetId id) {
  const uint8_t value = static_cast<uint8_t>(id);
  return value >= 1U && value <= kMaximumAssets ? static_cast<uint8_t>(1U << (value - 1U)) : 0U;
}

uint32_t crc32(const void* data, size_t length) {
  uint32_t value = 0xFFFFFFFFUL;
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  while (length--) {
    value ^= *bytes++;
    for (uint8_t bit = 0; bit < 8U; ++bit)
      value = (value >> 1U) ^ (0xEDB88320UL & (0U - (value & 1U)));
  }
  return ~value;
}

#if defined(ARDUINO_ARCH_ESP32)
WirelessUpdate* gUpdater = nullptr;
NimBLEServer* gUpdateServer = nullptr;
NimBLECharacteristic* gStatusCharacteristic = nullptr;
uint16_t gConnectionHandle = BLE_HS_CONN_HANDLE_NONE;

enum class TransportEventKind : uint8_t { Connected, Disconnected, Control, Data };
struct TransportEvent {
  TransportEventKind kind = TransportEventKind::Disconnected;
  uint16_t length = 0;
  uint8_t bytes[sizeof(DataHeader) + kMaximumDataBytes]{};
};
QueueHandle_t gUpdateQueue = nullptr;

void queueEvent(TransportEventKind kind, const uint8_t* bytes = nullptr, size_t length = 0) {
  if (!gUpdateQueue) return;
  TransportEvent event{};
  event.kind = kind;
  event.length = static_cast<uint16_t>(std::min<size_t>(length, sizeof(event.bytes)));
  if (event.length && bytes) std::memcpy(event.bytes, bytes, event.length);
  // WRITE-with-response plus one outstanding Android operation means this
  // queue should never fill. If it does, the missing status ACK naturally
  // stops the sender instead of silently losing an update block.
  xQueueSend(gUpdateQueue, &event, 0);
}

class UpdateServerCallbacks final : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
    gConnectionHandle = info.getConnHandle();
    server->updateConnParams(gConnectionHandle, 6, 12, 0, 400);
    queueEvent(TransportEventKind::Connected);
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    gConnectionHandle = BLE_HS_CONN_HANDLE_NONE;
    queueEvent(TransportEventKind::Disconnected);
    if (gUpdater && gUpdater->active()) NimBLEDevice::startAdvertising();
  }
} gUpdateServerCallbacks;

class UpdateControlCallbacks final : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    const NimBLEAttValue& value = characteristic->getValue();
    queueEvent(TransportEventKind::Control, value.data(), value.size());
  }
} gUpdateControlCallbacks;

class UpdateDataCallbacks final : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    const NimBLEAttValue& value = characteristic->getValue();
    queueEvent(TransportEventKind::Data, value.data(), value.size());
  }
} gUpdateDataCallbacks;
#endif
}  // namespace

bool WirelessUpdate::busy() const {
  return state_ == DeviceState::ReceivingManifest ||
         state_ == DeviceState::ReceivingObject ||
         state_ == DeviceState::Committing ||
         state_ == DeviceState::Rebooting;
}

uint8_t WirelessUpdate::progressPercent() const {
  if (!packageBytesTotal_) return 0;
  return static_cast<uint8_t>(std::min<uint32_t>(100U,
      static_cast<uint32_t>((static_cast<uint64_t>(packageBytesDone_) * 100ULL) /
                            packageBytesTotal_)));
}

bool WirelessUpdate::takeChanged() {
  const bool result = changed_;
  changed_ = false;
  return result;
}

void WirelessUpdate::setState(DeviceState state, const char* status, ErrorCode error) {
  state_ = state;
  error_ = error;
  if (status) {
    std::strncpy(status_, status, sizeof(status_) - 1U);
    status_[sizeof(status_) - 1U] = 0;
  }
  changed_ = true;
}

void WirelessUpdate::fail(ErrorCode error, const char* status) {
  closeCurrentObject(true);
  setState(DeviceState::Error, status, error);
  sendStatus(StatusCode::Error);
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("[OTAP] error=%u %s\n", static_cast<unsigned>(error), status ? status : "");
#endif
}

bool WirelessUpdate::begin(uint32_t playerId, const char* firmwareVersion,
                           uint16_t currentAssetPackVersion, bool sdMounted) {
  if (active_) return true;
  if (!sdMounted) {
    setState(DeviceState::Error, "INSERT A WORKING MICROSD FIRST.", ErrorCode::AssetUnavailable);
    return false;
  }
  playerId_ = playerId;
  currentAssetPackVersion_ = currentAssetPackVersion;
  std::strncpy(firmwareVersion_, firmwareVersion ? firmwareVersion : "UNKNOWN",
               sizeof(firmwareVersion_) - 1U);
#if defined(ARDUINO_ARCH_ESP32)
  pairingCode_ = 100000U + esp_random() % 900000U;
#else
  pairingCode_ = 123456U;
#endif
  active_ = true;
  connected_ = authenticated_ = manifestValid_ = false;
  manifestExpectedBytes_ = manifestReceivedBytes_ = 0;
  packageBytesDone_ = packageBytesTotal_ = 0;
  error_ = ErrorCode::None;
  if (!ensureUpdateDirectories() || !startTransport()) {
    active_ = false;
    setState(DeviceState::Error, "BLUETOOTH UPDATE COULD NOT START.", ErrorCode::Busy);
    stopTransport();
    return false;
  }
  setState(DeviceState::Advertising, "OPEN THE POKEGOCHI UPDATER APP.");
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("[OTAP] advertising player=%05lu pair=%06lu heap=%u block=%u\n",
                static_cast<unsigned long>(playerId_), static_cast<unsigned long>(pairingCode_),
                static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()));
#endif
  return true;
}

void WirelessUpdate::shutdown(bool discardReceivingSession) {
  if (state_ == DeviceState::Committing || state_ == DeviceState::Rebooting) return;
  closeCurrentObject(true);
  stopTransport();
  active_ = connected_ = authenticated_ = manifestValid_ = false;
  if (discardReceivingSession) clearTransferArtifacts();
  setState(DeviceState::Advertising, "WIRELESS UPDATE IS OFF.");
}

void WirelessUpdate::abort() {
  closeCurrentObject(true);
  clearTransferArtifacts();
  authenticated_ = false;
  manifestValid_ = false;
  manifestExpectedBytes_ = manifestReceivedBytes_ = 0;
  packageBytesDone_ = packageBytesTotal_ = 0;
  setState(connected_ ? DeviceState::AwaitingAuthentication : DeviceState::Advertising,
           connected_ ? "ENTER THE NEW PAIRING CODE." : "UPDATE CANCELLED.");
  sendStatus(StatusCode::Aborted);
}

bool WirelessUpdate::startTransport() {
#if !defined(ARDUINO_ARCH_ESP32)
  initialized_ = true;
  return true;
#else
  if (initialized_) return true;
  gUpdater = this;
  if (!gUpdateQueue) gUpdateQueue = xQueueCreate(3, sizeof(TransportEvent));
  if (!gUpdateQueue) return false;
  char name[22];
  std::snprintf(name, sizeof(name), "POKEGOCHI-UP-%05lu", static_cast<unsigned long>(playerId_));
  NimBLEDevice::init(name);
  NimBLEDevice::setMTU(517);
  NimBLEDevice::setPower(3);
  gUpdateServer = NimBLEDevice::createServer();
  if (!gUpdateServer) return false;
  gUpdateServer->setCallbacks(&gUpdateServerCallbacks, false);
  NimBLEService* service = gUpdateServer->createService(kServiceUuid);
  if (!service) return false;
  NimBLECharacteristic* control = service->createCharacteristic(
      kControlUuid, NIMBLE_PROPERTY::WRITE, 192);
  NimBLECharacteristic* data = service->createCharacteristic(
      kDataUuid, NIMBLE_PROPERTY::WRITE, sizeof(DataHeader) + kMaximumDataBytes);
  gStatusCharacteristic = service->createCharacteristic(
      kStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, sizeof(StatusPacket));
  if (!control || !data || !gStatusCharacteristic) return false;
  control->setCallbacks(&gUpdateControlCallbacks);
  data->setCallbacks(&gUpdateDataCallbacks);
  // NimBLE-Arduino 2.x starts every registered service when the server is
  // started; NimBLEService::start() is intentionally a no-op.
  gUpdateServer->start();

  Advertisement advertisement{{'P', 'G', 'O', 'T'}, kVersion, kDeviceModel,
                              playerId_, kMaximumDataBytes, kCapabilityTimeSync};
  NimBLEAdvertisementData advertisementData;
  if (!advertisementData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP) ||
      !advertisementData.setManufacturerData(
          reinterpret_cast<const uint8_t*>(&advertisement), sizeof(advertisement))) return false;
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  if (!advertising->setAdvertisementData(advertisementData)) return false;
  advertising->enableScanResponse(false);
  if (!advertising->start()) return false;
  initialized_ = true;
  sendStatus(StatusCode::Hello);
  return true;
#endif
}

void WirelessUpdate::stopTransport() {
#if defined(ARDUINO_ARCH_ESP32)
  if (initialized_) NimBLEDevice::deinit(true);
  if (gUpdateQueue) { vQueueDelete(gUpdateQueue); gUpdateQueue = nullptr; }
  gUpdateServer = nullptr;
  gStatusCharacteristic = nullptr;
  gConnectionHandle = BLE_HS_CONN_HANDLE_NONE;
  gUpdater = nullptr;
#endif
  initialized_ = false;
}

void WirelessUpdate::transportConnected() {
  connected_ = true;
  authenticated_ = false;
  setState(DeviceState::AwaitingAuthentication, "PHONE CONNECTED. ENTER THE CODE.");
  sendStatus(StatusCode::Hello);
}

void WirelessUpdate::transportDisconnected() {
  connected_ = false;
  authenticated_ = false;
  if (state_ == DeviceState::ReceivingObject && objectKind_ == ObjectKind::Firmware) {
    closeCurrentObject(true);
    journal_.firmwareReady = 0;
    saveJournal(journal_);
  } else {
    closeCurrentObject(false);
  }
  if (active_ && state_ != DeviceState::Committing && state_ != DeviceState::Rebooting)
    setState(DeviceState::Advertising, "CONNECTION LOST. READY TO RESUME.");
}

void WirelessUpdate::transportControl(const uint8_t* bytes, size_t length) {
  processControl(bytes, length);
}

void WirelessUpdate::transportData(const uint8_t* bytes, size_t length) {
  processData(bytes, length);
}

void WirelessUpdate::update() {
#if defined(ARDUINO_ARCH_ESP32)
  TransportEvent event{};
  if (gUpdateQueue && xQueueReceive(gUpdateQueue, &event, 0) == pdTRUE) {
    switch (event.kind) {
      case TransportEventKind::Connected: transportConnected(); break;
      case TransportEventKind::Disconnected: transportDisconnected(); break;
      case TransportEventKind::Control: transportControl(event.bytes, event.length); break;
      case TransportEventKind::Data: transportData(event.bytes, event.length); break;
    }
  }
  if (rebootScheduled_ && static_cast<int32_t>(millis() - rebootAtMs_) >= 0) {
    Serial.println("[OTAP] rebooting into verified candidate");
    Serial.flush();
    delay(25);
    ESP.restart();
  }
#endif
}

void WirelessUpdate::processControl(const uint8_t* bytes, size_t length) {
  if (!bytes || !length) return;
  const Command command = static_cast<Command>(bytes[0]);
  if (command == Command::QueryStatus) {
    if (length != 1U) { fail(ErrorCode::BadCommand, "INVALID STATUS REQUEST."); return; }
    sendStatus(StatusCode::Hello);
    return;
  }

  if (command == Command::Authenticate) {
    if (length != sizeof(AuthenticateCommand)) { fail(ErrorCode::BadCommand, "INVALID AUTH REQUEST."); return; }
    if (busy()) { fail(ErrorCode::Busy, "AN UPDATE OPERATION IS ALREADY RUNNING."); return; }
    AuthenticateCommand request{};
    std::memcpy(&request, bytes, sizeof(request));
    if (request.protocolVersion != kVersion || request.pairingCode != pairingCode_) {
      fail(ErrorCode::AuthenticationFailed, "PAIRING CODE WAS REJECTED.");
      return;
    }
    authenticated_ = true;
    setState(DeviceState::AwaitingManifest, "PHONE AUTHENTICATED.");
    sendStatus(StatusCode::Authenticated);
    return;
  }
  if (!authenticated_) { fail(ErrorCode::AuthenticationFailed, "PAIR THE PHONE FIRST."); return; }
  if (command == Command::Abort) {
    if (length != 1U) { fail(ErrorCode::BadCommand, "INVALID ABORT REQUEST."); return; }
    abort();
    return;
  }
  if (command == Command::SynchronizeTime) {
    if (length != sizeof(TimeSyncCommand)) {
      fail(ErrorCode::BadCommand, "INVALID CLOCK REQUEST."); return;
    }
    TimeSyncCommand request{};
    std::memcpy(&request, bytes, sizeof(request));
    // 2020-01-01 through 2100-01-01, including practical timezone shifts.
    if (request.localEpochSeconds < 1577793600UL ||
        request.localEpochSeconds > 4102495200UL) {
      fail(ErrorCode::BadCommand, "PHONE CLOCK IS OUT OF RANGE."); return;
    }
    synchronizedTime_ = request.localEpochSeconds;
    synchronizedTimePending_ = true;
    sendStatus(StatusCode::TimeSynchronized);
    return;
  }

  switch (command) {
    case Command::ManifestBegin: {
      if (length != sizeof(ManifestBeginCommand)) { fail(ErrorCode::BadCommand, "INVALID MANIFEST REQUEST."); return; }
      ManifestBeginCommand request{};
      std::memcpy(&request, bytes, sizeof(request));
      if (request.totalBytes != sizeof(Manifest) || request.totalBytes > sizeof(manifestBuffer_)) {
        fail(ErrorCode::ManifestSize, "UPDATE MANIFEST HAS THE WRONG SIZE."); return;
      }
      manifestExpectedBytes_ = request.totalBytes;
      manifestReceivedBytes_ = 0;
      std::memset(manifestBuffer_, 0, sizeof(manifestBuffer_));
      setState(DeviceState::ReceivingManifest, "READING SIGNED UPDATE MANIFEST...");
      sendStatus(StatusCode::DataAccepted);
      return;
    }
    case Command::ManifestChunk: {
      if (state_ != DeviceState::ReceivingManifest || length <= sizeof(ManifestChunkHeader)) {
        fail(ErrorCode::ObjectOrder, "MANIFEST DATA IS OUT OF ORDER."); return;
      }
      ManifestChunkHeader header{};
      std::memcpy(&header, bytes, sizeof(header));
      const size_t payload = length - sizeof(header);
      if (header.offset != manifestReceivedBytes_ ||
          manifestReceivedBytes_ + payload > manifestExpectedBytes_) {
        fail(ErrorCode::ObjectRange, "MANIFEST OFFSET DOES NOT MATCH."); return;
      }
      std::memcpy(manifestBuffer_ + manifestReceivedBytes_, bytes + sizeof(header), payload);
      manifestReceivedBytes_ = static_cast<uint16_t>(manifestReceivedBytes_ + payload);
      sendStatus(StatusCode::DataAccepted);
      return;
    }
    case Command::ManifestFinish:
      if (state_ != DeviceState::ReceivingManifest || manifestReceivedBytes_ != manifestExpectedBytes_ ||
          !verifyManifest()) return;
      setState(DeviceState::ReadyForObject, "SIGNED PACKAGE ACCEPTED.");
      sendStatus(StatusCode::ManifestAccepted);
      return;
    case Command::ObjectBegin: {
      if (!manifestValid_ || length != sizeof(ObjectBeginCommand)) {
        fail(ErrorCode::BadCommand, "INVALID OBJECT REQUEST."); return;
      }
      ObjectBeginCommand request{};
      std::memcpy(&request, bytes, sizeof(request));
      if (request.kind > static_cast<uint8_t>(ObjectKind::Asset) ||
          (request.kind == static_cast<uint8_t>(ObjectKind::Firmware) && request.assetId != 0U)) {
        fail(ErrorCode::BadCommand, "UNKNOWN UPDATE OBJECT TYPE."); return;
      }
      if (!beginObject(static_cast<ObjectKind>(request.kind), static_cast<AssetId>(request.assetId))) return;
      sendStatus(StatusCode::ObjectReady);
      return;
    }
    case Command::ObjectFinish:
      if (length != 1U) { fail(ErrorCode::BadCommand, "INVALID OBJECT FINISH REQUEST."); return; }
      if (!finishObject()) return;
      sendStatus(StatusCode::ObjectVerified);
      return;
    case Command::Commit:
      if (length != 1U) { fail(ErrorCode::BadCommand, "INVALID COMMIT REQUEST."); return; }
      if (!commit()) return;
      sendStatus(StatusCode::CommitAccepted);
      return;
    default:
      fail(ErrorCode::BadCommand, "UNKNOWN UPDATE COMMAND.");
      return;
  }
}

bool WirelessUpdate::takeSynchronizedTime(uint32_t& localEpochSeconds) {
  if (!synchronizedTimePending_) return false;
  synchronizedTimePending_ = false;
  localEpochSeconds = synchronizedTime_;
  return true;
}

void WirelessUpdate::processData(const uint8_t* bytes, size_t length) {
  if (state_ != DeviceState::ReceivingObject || !bytes || length <= sizeof(DataHeader)) {
    fail(ErrorCode::ObjectOrder, "UPDATE DATA ARRIVED OUT OF ORDER."); return;
  }
  DataHeader header{};
  std::memcpy(&header, bytes, sizeof(header));
  const size_t payloadBytes = length - sizeof(header);
  if (payloadBytes > kMaximumDataBytes || header.offset != objectOffset_ ||
      objectOffset_ + payloadBytes > objectSize_) {
    fail(ErrorCode::ObjectRange, "UPDATE DATA OFFSET DOES NOT MATCH."); return;
  }
  const uint8_t* payload = bytes + sizeof(header);
#if defined(ARDUINO_ARCH_ESP32)
  bool written = false;
  if (objectKind_ == ObjectKind::Firmware) {
    written = otaHandle_ && esp_ota_write(otaHandle_, payload, payloadBytes) == ESP_OK;
  } else {
    written = assetFile_ && assetFile_.write(payload, payloadBytes) == payloadBytes;
  }
  if (!written) { fail(ErrorCode::ObjectWrite, "THE UPDATE COULD NOT BE WRITTEN."); return; }
#endif
  objectOffset_ += static_cast<uint32_t>(payloadBytes);
  recalculatePackageProgress();
  sendStatus(StatusCode::DataAccepted);
}

bool WirelessUpdate::verifyManifest() {
  std::memcpy(&manifest_, manifestBuffer_, sizeof(manifest_));
  if (std::memcmp(manifest_.magic, "PGU1", 4) || manifest_.protocolVersion != kVersion ||
      manifest_.deviceModel != kDeviceModel || manifest_.structureSize != sizeof(Manifest) ||
      manifest_.assetCount != kMaximumAssets || !manifest_.packageSequence ||
      !manifest_.firmwareSize) {
    fail(ErrorCode::ManifestFormat, "THIS PACKAGE IS NOT FOR THIS DEVICE."); return false;
  }
  uint8_t seen = 0;
  uint64_t assetBytes = 0;
  for (uint8_t index = 0; index < manifest_.assetCount; ++index) {
    const AssetId id = static_cast<AssetId>(manifest_.assets[index].id);
    const uint8_t bit = assetBit(id);
    if (!bit || (seen & bit) || !manifest_.assets[index].size) {
      fail(ErrorCode::ManifestFormat, "THE ASSET LIST IS INVALID."); return false;
    }
    seen |= bit;
    assetBytes += manifest_.assets[index].size;
  }
  if (seen != 0x0FU || assetBytes > 64ULL * 1024ULL * 1024ULL) {
    fail(ErrorCode::ManifestFormat, "THE ASSET LIST IS INCOMPLETE."); return false;
  }
  if (!signatureValid(manifest_)) {
    fail(ErrorCode::ManifestSignature, "PACKAGE SIGNATURE IS INVALID."); return false;
  }

  Journal existing{};
  const bool resumable = loadJournal(existing) &&
      existing.phase == static_cast<uint8_t>(JournalPhase::Receiving) &&
      existing.packageSequence == manifest_.packageSequence;
  const uint32_t accepted = acceptedPackageSequence();
  if (!resumable && manifest_.packageSequence <= accepted) {
    fail(ErrorCode::PackageRollback, "AN OLDER UPDATE WAS REJECTED."); return false;
  }
  uint8_t reusableAssets = 0;
  uint64_t reusableBytes = 0;
#if defined(ARDUINO_ARCH_ESP32)
  const esp_partition_t* candidate = esp_ota_get_next_update_partition(nullptr);
  if (!candidate || manifest_.firmwareSize > candidate->size) {
    fail(ErrorCode::FirmwareBegin, "FIRMWARE DOES NOT FIT THE OTA SLOT."); return false;
  }
  // Most releases change firmware while retaining the same large asset pack.
  // Verify installed files against the signed manifest once and count exact
  // matches as complete instead of retransmitting them over BLE.
  for (uint8_t index = 0; index < manifest_.assetCount; ++index) {
    const AssetRecord& record = manifest_.assets[index];
    const AssetId id = static_cast<AssetId>(record.id);
    const AssetPaths* paths = pathsFor(id);
    if (!paths) continue;
    File installed = SD.open(paths->finalPath, FILE_READ);
    const bool sameSize = installed && installed.size() == record.size;
    if (installed) installed.close();
    uint8_t digest[32]{};
    if (sameSize && hashFile(paths->finalPath, digest) &&
        !std::memcmp(digest, record.sha256, sizeof(digest))) {
      reusableAssets |= assetBit(id);
      reusableBytes += record.size;
    }
  }

  const uint64_t freeBytes = SD.totalBytes() > SD.usedBytes() ? SD.totalBytes() - SD.usedBytes() : 0;
  uint64_t stagedAlready = 0;
  if (resumable) {
    for (const auto& paths : kAssetPaths) {
      if (reusableAssets & assetBit(paths.id)) continue;
      File partial = SD.open(paths.partial, FILE_READ);
      if (partial) { stagedAlready += partial.size(); partial.close(); }
      File ready = SD.open(paths.ready, FILE_READ);
      if (ready) { stagedAlready += ready.size(); ready.close(); }
    }
  }
  const uint64_t bytesToStage = assetBytes > reusableBytes ? assetBytes - reusableBytes : 0;
  const uint64_t stillRequired = bytesToStage > stagedAlready ? bytesToStage - stagedAlready : 0;
  if (freeBytes < stillRequired + 512ULL * 1024ULL) {
    fail(ErrorCode::NotEnoughSpace, "MICROSD NEEDS MORE FREE SPACE."); return false;
  }
#endif
  if (resumable) {
    journal_ = existing;
    journal_.verifiedAssetMask &= static_cast<uint8_t>(~journal_.reusedAssetMask);
    journal_.reusedAssetMask = reusableAssets;
    journal_.verifiedAssetMask |= reusableAssets;
    if (!saveJournal(journal_)) {
      fail(ErrorCode::ObjectWrite, "UPDATE JOURNAL COULD NOT BE RESUMED."); return false;
    }
  } else {
    clearTransferArtifacts();
    std::memset(&journal_, 0, sizeof(journal_));
    std::memcpy(journal_.magic, "PGUS", 4);
    journal_.version = kJournalVersion;
    journal_.phase = static_cast<uint8_t>(JournalPhase::Receiving);
    journal_.packageSequence = manifest_.packageSequence;
    journal_.reusedAssetMask = reusableAssets;
    journal_.verifiedAssetMask = reusableAssets;
    if (!saveJournal(journal_)) {
      fail(ErrorCode::ObjectWrite, "UPDATE JOURNAL COULD NOT BE CREATED."); return false;
    }
  }
  manifestValid_ = true;
  packageBytesTotal_ = manifest_.firmwareSize + static_cast<uint32_t>(assetBytes);
  recalculatePackageProgress();
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("[OTAP] manifest seq=%lu firmware=%lu assets=%llu resume=%u reused=%u (%llu bytes)\n",
                static_cast<unsigned long>(manifest_.packageSequence),
                static_cast<unsigned long>(manifest_.firmwareSize),
                static_cast<unsigned long long>(assetBytes), resumable ? 1U : 0U,
                static_cast<unsigned>(reusableAssets),
                static_cast<unsigned long long>(reusableBytes));
#endif
  return true;
}

bool WirelessUpdate::beginObject(ObjectKind kind, AssetId assetId) {
  if (state_ != DeviceState::ReadyForObject && state_ != DeviceState::ReadyToCommit) {
    fail(ErrorCode::ObjectOrder, "FINISH THE CURRENT OBJECT FIRST."); return false;
  }
  closeCurrentObject(false);
  objectKind_ = kind;
  assetId_ = assetId;
  objectOffset_ = objectSize_ = 0;
  objectHash_ = nullptr;
  const bool okay = kind == ObjectKind::Firmware ? beginFirmware() : beginAsset(assetId);
  if (!okay) return false;
  setState(DeviceState::ReceivingObject,
           kind == ObjectKind::Firmware ? "RECEIVING FIRMWARE..." : "RECEIVING MICROSD ASSETS...");
  recalculatePackageProgress();
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("[OTAP] object kind=%u id=%u offset=%lu size=%lu\n",
                static_cast<unsigned>(kind), static_cast<unsigned>(assetId),
                static_cast<unsigned long>(objectOffset_),
                static_cast<unsigned long>(objectSize_));
#endif
  return true;
}

bool WirelessUpdate::beginFirmware() {
  objectSize_ = manifest_.firmwareSize;
  objectHash_ = manifest_.firmwareSha256;
#if defined(ARDUINO_ARCH_ESP32)
  if (journal_.firmwareReady && journal_.targetPartition[0]) {
    targetPartition_ = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, journal_.targetPartition);
    uint8_t digest[32]{};
    if (targetPartition_ && hashPartition(targetPartition_, objectSize_, digest) &&
        !std::memcmp(digest, objectHash_, sizeof(digest))) {
      objectOffset_ = objectSize_;
      return true;
    }
    journal_.firmwareReady = 0;
    saveJournal(journal_);
  }
  targetPartition_ = esp_ota_get_next_update_partition(nullptr);
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (!targetPartition_ || objectSize_ > targetPartition_->size ||
      esp_ota_begin(targetPartition_, objectSize_, &otaHandle_) != ESP_OK) {
    fail(ErrorCode::FirmwareBegin, "THE OTA PARTITION COULD NOT START."); return false;
  }
  std::strncpy(journal_.targetPartition, targetPartition_->label,
               sizeof(journal_.targetPartition) - 1U);
  if (running) std::strncpy(journal_.previousPartition, running->label,
                            sizeof(journal_.previousPartition) - 1U);
  journal_.firmwareReady = 0;
  saveJournal(journal_);
#endif
  return true;
}

bool WirelessUpdate::beginAsset(AssetId id) {
  const AssetRecord* record = findAsset(manifest_, id);
  const AssetPaths* paths = pathsFor(id);
  if (!record || !paths) { fail(ErrorCode::ManifestFormat, "UNKNOWN ASSET OBJECT."); return false; }
  objectSize_ = record->size;
  objectHash_ = record->sha256;
#if defined(ARDUINO_ARCH_ESP32)
  const uint8_t bit = assetBit(id);
  if ((journal_.reusedAssetMask & bit) && (journal_.verifiedAssetMask & bit)) {
    objectOffset_ = objectSize_;
    return true;
  }
  if (journal_.verifiedAssetMask & bit) {
    File ready = SD.open(paths->ready, FILE_READ);
    const bool complete = ready && ready.size() == objectSize_;
    if (ready) ready.close();
    if (complete) { objectOffset_ = objectSize_; return true; }
    journal_.verifiedAssetMask &= static_cast<uint8_t>(~bit);
    saveJournal(journal_);
  }
  File partial = SD.open(paths->partial, FILE_READ);
  if (partial) { objectOffset_ = partial.size(); partial.close(); }
  if (objectOffset_ > objectSize_) {
    SD.remove(paths->partial);
    objectOffset_ = 0;
  }
  assetFile_ = SD.open(paths->partial, objectOffset_ ? FILE_APPEND : FILE_WRITE);
  if (!assetFile_) { fail(ErrorCode::ObjectWrite, "MICROSD STAGING FILE COULD NOT OPEN."); return false; }
#endif
  return true;
}

bool WirelessUpdate::finishObject() {
  if (state_ != DeviceState::ReceivingObject || objectOffset_ != objectSize_) {
    fail(ErrorCode::ObjectRange, "THE CURRENT OBJECT IS INCOMPLETE."); return false;
  }
  const bool okay = objectKind_ == ObjectKind::Firmware ? finishFirmware() : finishAsset();
  if (!okay) return false;
  const bool allAssets = (journal_.verifiedAssetMask & 0x0FU) == 0x0FU;
  setState(journal_.firmwareReady && allAssets ? DeviceState::ReadyToCommit : DeviceState::ReadyForObject,
           journal_.firmwareReady && allAssets ? "READY TO INSTALL." : "OBJECT VERIFIED.");
  recalculatePackageProgress();
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("[OTAP] object verified kind=%u id=%u total=%lu/%lu\n",
                static_cast<unsigned>(objectKind_), static_cast<unsigned>(assetId_),
                static_cast<unsigned long>(packageBytesDone_),
                static_cast<unsigned long>(packageBytesTotal_));
#endif
  return true;
}

bool WirelessUpdate::finishFirmware() {
  if (!journal_.firmwareReady) {
#if defined(ARDUINO_ARCH_ESP32)
    if (!otaHandle_ || esp_ota_end(otaHandle_) != ESP_OK) {
      otaHandle_ = 0;
      fail(ErrorCode::FirmwareValidation, "FIRMWARE IMAGE VALIDATION FAILED."); return false;
    }
    otaHandle_ = 0;
    uint8_t digest[32]{};
    if (!hashPartition(targetPartition_, objectSize_, digest) ||
        std::memcmp(digest, objectHash_, sizeof(digest))) {
      fail(ErrorCode::ObjectHash, "FIRMWARE SHA-256 DID NOT MATCH."); return false;
    }
#endif
    journal_.firmwareReady = 1;
    if (!saveJournal(journal_)) {
      fail(ErrorCode::ObjectWrite, "FIRMWARE STATE COULD NOT BE SAVED."); return false;
    }
  }
  return true;
}

bool WirelessUpdate::finishAsset() {
  const AssetPaths* paths = pathsFor(assetId_);
  if (!paths) return false;
  const uint8_t bit = assetBit(assetId_);
  if ((journal_.reusedAssetMask & bit) && (journal_.verifiedAssetMask & bit))
    return saveJournal(journal_);
#if defined(ARDUINO_ARCH_ESP32)
  if (assetFile_) { assetFile_.flush(); assetFile_.close(); }
  const char* source = SD.exists(paths->partial) ? paths->partial : paths->ready;
  uint8_t digest[32]{};
  if (!hashFile(source, digest) || std::memcmp(digest, objectHash_, sizeof(digest))) {
    SD.remove(paths->partial);
    SD.remove(paths->ready);
    journal_.verifiedAssetMask &= static_cast<uint8_t>(~bit);
    saveJournal(journal_);
    fail(ErrorCode::ObjectHash, "ASSET SHA-256 DID NOT MATCH."); return false;
  }
  if (std::strcmp(source, paths->ready)) {
    SD.remove(paths->ready);
    if (!SD.rename(paths->partial, paths->ready)) {
      fail(ErrorCode::ObjectWrite, "VERIFIED ASSET COULD NOT BE STAGED."); return false;
    }
  }
#endif
  journal_.verifiedAssetMask |= bit;
  if (!saveJournal(journal_)) {
    fail(ErrorCode::ObjectWrite, "ASSET STATE COULD NOT BE SAVED."); return false;
  }
  return true;
}

void WirelessUpdate::closeCurrentObject(bool abortFirmware) {
#if defined(ARDUINO_ARCH_ESP32)
  if (assetFile_) { assetFile_.flush(); assetFile_.close(); }
  if (otaHandle_) {
    if (abortFirmware) esp_ota_abort(otaHandle_);
    otaHandle_ = 0;
  }
#else
  (void)abortFirmware;
#endif
  objectOffset_ = objectSize_ = 0;
  objectHash_ = nullptr;
}

bool WirelessUpdate::commit() {
  if (!manifestValid_ || state_ != DeviceState::ReadyToCommit ||
      !journal_.firmwareReady || (journal_.verifiedAssetMask & 0x0FU) != 0x0FU) {
    fail(ErrorCode::ObjectOrder, "THE PACKAGE IS NOT COMPLETE."); return false;
  }
  setState(DeviceState::Committing, "INSTALLING VERIFIED ASSETS...");
  journal_.phase = static_cast<uint8_t>(JournalPhase::Committing);
  if (!saveJournal(journal_) || !commitAssets(journal_) || !verifyInstalledAssets()) {
    rollbackAssets(journal_);
    clearTransferArtifacts();
    fail(ErrorCode::CommitFailed, "INSTALLED ASSETS FAILED FINAL CHECK; BACKUP RESTORED."); return false;
  }
  journal_.phase = static_cast<uint8_t>(JournalPhase::AwaitingValidation);
  if (!saveJournal(journal_)) {
    rollbackAssets(journal_);
    clearTransferArtifacts();
    fail(ErrorCode::CommitFailed, "FINAL UPDATE STATE COULD NOT BE SAVED."); return false;
  }
#if defined(ARDUINO_ARCH_ESP32)
  targetPartition_ = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, journal_.targetPartition);
  if (!targetPartition_ ||
      !storePendingBoot(journal_.targetPartition, journal_.previousPartition,
                        journal_.packageSequence, 0U) ||
      esp_ota_set_boot_partition(targetPartition_) != ESP_OK) {
    clearPendingBoot();
    rollbackAssets(journal_);
    clearTransferArtifacts();
    fail(ErrorCode::CommitFailed, "FIRMWARE COULD NOT BE SELECTED."); return false;
  }
#endif
  setState(DeviceState::Rebooting, "UPDATE VERIFIED. RESTARTING...");
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("[OTAP] commit complete; candidate=%s previous=%s\n",
                journal_.targetPartition, journal_.previousPartition);
#endif
#if defined(ARDUINO_ARCH_ESP32)
  rebootScheduled_ = true;
  rebootAtMs_ = millis() + 1200U;
#endif
  return true;
}

bool WirelessUpdate::commitAssets(Journal& journal) {
#if !defined(ARDUINO_ARCH_ESP32)
  (void)journal;
  return true;
#else
  if (!ensureUpdateDirectories()) return false;
  for (const auto& paths : kAssetPaths) {
    const uint8_t bit = assetBit(paths.id);
    if (journal.committedAssetMask & bit) continue;
    if (journal.reusedAssetMask & bit) {
      journal.committedAssetMask |= bit;
      if (!saveJournal(journal)) return false;
      continue;
    }
    const bool hasReady = SD.exists(paths.ready);
    const bool hasFinal = SD.exists(paths.finalPath);
    const bool hasBackup = SD.exists(paths.backup);

    // Power can disappear between either rename and the journal write.  Each
    // combination below is therefore a resumable transaction state, not an
    // error that destroys the only known-good copy.
    if (!hasReady) {
      if (!hasFinal || !hasBackup) return false;
      journal.committedAssetMask |= bit;  // ready -> final already happened.
      if (!saveJournal(journal)) return false;
      continue;
    }
    if (hasFinal && !hasBackup && !SD.rename(paths.finalPath, paths.backup)) return false;
    if (!SD.rename(paths.ready, paths.finalPath)) {
      if (!SD.exists(paths.finalPath) && SD.exists(paths.backup))
        SD.rename(paths.backup, paths.finalPath);
      return false;
    }
    journal.committedAssetMask |= bit;
    if (!saveJournal(journal)) return false;
  }
  return true;
#endif
}

bool WirelessUpdate::verifyInstalledAssets() const {
#if !defined(ARDUINO_ARCH_ESP32)
  return true;
#else
  for (uint8_t index = 0; index < manifest_.assetCount; ++index) {
    const AssetRecord& record = manifest_.assets[index];
    const AssetPaths* paths = pathsFor(static_cast<AssetId>(record.id));
    if (!paths) return false;
    File installed = SD.open(paths->finalPath, FILE_READ);
    const bool sizeMatches = installed && installed.size() == record.size;
    if (installed) installed.close();
    uint8_t digest[32]{};
    if (!sizeMatches || !hashFile(paths->finalPath, digest) ||
        std::memcmp(digest, record.sha256, sizeof(digest))) {
      Serial.printf("[OTAP] final asset verification failed id=%u path=%s\n",
                    static_cast<unsigned>(record.id), paths->finalPath);
      return false;
    }
  }
  Serial.println("[OTAP] installed firmware companion assets verified");
  return true;
#endif
}

bool WirelessUpdate::rollbackAssets(Journal& journal) {
#if !defined(ARDUINO_ARCH_ESP32)
  (void)journal;
  return true;
#else
  bool okay = true;
  for (const auto& paths : kAssetPaths) {
    const uint8_t bit = assetBit(paths.id);
    // A backup can exist before committedAssetMask was persisted.  Its
    // presence is authoritative: restore it regardless of the mask.
    if (SD.exists(paths.backup)) {
      SD.remove(paths.finalPath);
      if (!SD.rename(paths.backup, paths.finalPath)) okay = false;
    }
    journal.committedAssetMask &= static_cast<uint8_t>(~bit);
  }
  saveJournal(journal);
  return okay;
#endif
}

bool WirelessUpdate::recoverAtBoot(bool sdMounted) {
  bootValidationPending_ = false;
  char pendingTarget[8]{};
  char pendingPrevious[8]{};
  uint32_t pendingSequence = 0;
  uint8_t pendingAttempts = 0;
  const bool hasPendingBoot = loadPendingBoot(
      pendingTarget, pendingPrevious, pendingSequence, pendingAttempts);
#if defined(ARDUINO_ARCH_ESP32)
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (hasPendingBoot && running &&
      !std::strncmp(running->label, pendingTarget, sizeof(pendingTarget))) {
    std::memset(&journal_, 0, sizeof(journal_));
    std::strncpy(journal_.targetPartition, pendingTarget,
                 sizeof(journal_.targetPartition) - 1U);
    std::strncpy(journal_.previousPartition, pendingPrevious,
                 sizeof(journal_.previousPartition) - 1U);
    journal_.packageSequence = pendingSequence;
    ++pendingAttempts;
    storePendingBoot(pendingTarget, pendingPrevious, pendingSequence, pendingAttempts);
    bootValidationPending_ = true;
  }
#endif
  if (!sdMounted) return !hasPendingBoot;
  Journal journal{};
  if (!loadJournal(journal)) return !hasPendingBoot;
  journal_ = journal;
#if defined(ARDUINO_ARCH_ESP32)
  if (journal.phase == static_cast<uint8_t>(JournalPhase::Committing)) {
    Serial.println("[OTAP] recovering interrupted asset commit");
    if (!commitAssets(journal_)) {
      rollbackAssets(journal_);
      removeJournal();
      return false;
    }
    const esp_partition_t* target = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, journal_.targetPartition);
    if (!target ||
        !storePendingBoot(journal_.targetPartition, journal_.previousPartition,
                          journal_.packageSequence, 0U) ||
        esp_ota_set_boot_partition(target) != ESP_OK) {
      clearPendingBoot();
      rollbackAssets(journal_);
      removeJournal();
      return false;
    }
    journal_.phase = static_cast<uint8_t>(JournalPhase::AwaitingValidation);
    if (!saveJournal(journal_)) {
      clearPendingBoot();
      rollbackAssets(journal_);
      const esp_partition_t* previous = esp_partition_find_first(
          ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, journal_.previousPartition);
      if (previous) esp_ota_set_boot_partition(previous);
      removeJournal();
      return false;
    }
    ESP.restart();
    return true;
  }
  if (journal.phase == static_cast<uint8_t>(JournalPhase::Accepted)) {
    Serial.println("[OTAP] finishing accepted update cleanup");
    bootValidationPending_ = false;
    storeAcceptedPackageSequence(journal_.packageSequence);
    clearPendingBoot();
    finishSuccessfulUpdate();
    return true;
  }
  if (journal.phase == static_cast<uint8_t>(JournalPhase::AwaitingValidation)) {
    if (running && !std::strncmp(running->label, journal.targetPartition,
                                 sizeof(journal.targetPartition))) {
      if (hasPendingBoot)
        journal_.bootAttempts = std::max<uint8_t>(journal_.bootAttempts, pendingAttempts);
      else
        ++journal_.bootAttempts;
      saveJournal(journal_);
      storePendingBoot(journal_.targetPartition, journal_.previousPartition,
                       journal_.packageSequence, journal_.bootAttempts);
      if (journal_.bootAttempts > 1U) {
        Serial.println("[OTAP] candidate rebooted before validation; rolling back");
        rollbackAssets(journal_);
        const esp_partition_t* previous = esp_partition_find_first(
            ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, journal_.previousPartition);
        removeJournal();
        clearPendingBoot();
        if (previous) esp_ota_set_boot_partition(previous);
        ESP.restart();
        return false;
      }
      bootValidationPending_ = true;
      Serial.printf("[OTAP] candidate %s awaits save/asset self-test\n", running->label);
    } else {
      Serial.println("[OTAP] bootloader returned to previous image; restoring asset backup");
      rollbackAssets(journal_);
      clearTransferArtifacts();
      clearPendingBoot();
    }
  }
#endif
  return true;
}

void WirelessUpdate::confirmBoot(bool healthy) {
  if (!bootValidationPending_) return;
#if defined(ARDUINO_ARCH_ESP32)
  if (healthy) {
    const esp_err_t marked = esp_ota_mark_app_valid_cancel_rollback();
    Serial.printf("[OTAP] candidate accepted result=%s\n", esp_err_to_name(marked));
    journal_.phase = static_cast<uint8_t>(JournalPhase::Accepted);
    if (!saveJournal(journal_)) {
      Serial.println("[OTAP] acceptance journal failed; retaining rollback marker");
      return;
    }
    storeAcceptedPackageSequence(journal_.packageSequence);
    clearPendingBoot();
    finishSuccessfulUpdate();
    bootValidationPending_ = false;
    return;
  }
  Serial.println("[OTAP] candidate self-test failed; restoring application and assets");
  rollbackAssets(journal_);
  const esp_partition_t* previous = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, journal_.previousPartition);
  removeJournal();
  clearPendingBoot();
  if (previous) esp_ota_set_boot_partition(previous);
  ESP.restart();
#else
  (void)healthy;
#endif
}

void WirelessUpdate::finishSuccessfulUpdate() {
#if defined(ARDUINO_ARCH_ESP32)
  for (const auto& paths : kAssetPaths) {
    SD.remove(paths.backup);
    SD.remove(paths.partial);
    SD.remove(paths.ready);
  }
  removeJournal();
#endif
}

bool WirelessUpdate::ensureUpdateDirectories() {
#if !defined(ARDUINO_ARCH_ESP32)
  return true;
#else
  if (!SD.exists("/pokegochi") && !SD.mkdir("/pokegochi")) return false;
  if (!SD.exists("/pokegochi/update") && !SD.mkdir("/pokegochi/update")) return false;
  if (!SD.exists("/pokegochi/update/backup") && !SD.mkdir("/pokegochi/update/backup")) return false;
  return true;
#endif
}

void WirelessUpdate::clearTransferArtifacts(bool keepBackups) {
#if defined(ARDUINO_ARCH_ESP32)
  for (const auto& paths : kAssetPaths) {
    SD.remove(paths.partial);
    SD.remove(paths.ready);
    if (!keepBackups) SD.remove(paths.backup);
  }
  removeJournal();
#else
  (void)keepBackups;
#endif
  std::memset(&journal_, 0, sizeof(journal_));
}

bool WirelessUpdate::loadJournal(Journal& journal) const {
#if !defined(ARDUINO_ARCH_ESP32)
  (void)journal;
  return false;
#else
  const auto readValid = [&](const char* path, Journal& candidate) {
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    const bool complete = file.size() == sizeof(Journal) &&
        file.read(reinterpret_cast<uint8_t*>(&candidate), sizeof(candidate)) == sizeof(candidate);
    file.close();
    if (!complete || std::memcmp(candidate.magic, "PGUS", 4) ||
        candidate.version != kJournalVersion) return false;
    const uint32_t expected = candidate.checksum;
    candidate.checksum = 0;
    const uint32_t actual = crc32(&candidate, sizeof(candidate));
    candidate.checksum = expected;
    return expected == actual;
  };
  // state.tmp is created and flushed after state.bin, so a valid temporary
  // file is always the newer side of an interrupted atomic journal update.
  if (readValid(kJournalTempPath, journal)) return true;
  return readValid(kJournalPath, journal);
#endif
}

bool WirelessUpdate::saveJournal(Journal& journal) const {
#if !defined(ARDUINO_ARCH_ESP32)
  (void)journal;
  return true;
#else
  journal.checksum = 0;
  journal.checksum = crc32(&journal, sizeof(journal));
  SD.remove(kJournalTempPath);
  File file = SD.open(kJournalTempPath, FILE_WRITE);
  if (!file) return false;
  const bool complete = file.write(reinterpret_cast<const uint8_t*>(&journal), sizeof(journal)) == sizeof(journal);
  file.flush();
  file.close();
  if (!complete) { SD.remove(kJournalTempPath); return false; }
  SD.remove(kJournalPath);
  if (!SD.rename(kJournalTempPath, kJournalPath)) return false;
  return true;
#endif
}

void WirelessUpdate::removeJournal() const {
#if defined(ARDUINO_ARCH_ESP32)
  SD.remove(kJournalTempPath);
  SD.remove(kJournalPath);
#endif
}

bool WirelessUpdate::hashFile(const char* path, uint8_t output[32]) const {
#if !defined(ARDUINO_ARCH_ESP32)
  (void)path; std::memset(output, 0, 32); return true;
#else
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  bool okay = mbedtls_sha256_starts(&context, 0) == 0;
  constexpr size_t kHashBufferBytes = 4096;
  uint8_t* buffer = static_cast<uint8_t*>(std::malloc(kHashBufferBytes));
  if (!buffer) {
    mbedtls_sha256_free(&context);
    file.close();
    return false;
  }
  while (okay && file.available()) {
    const size_t count = file.read(buffer, kHashBufferBytes);
    if (!count) { okay = false; break; }
    okay = mbedtls_sha256_update(&context, buffer, count) == 0;
    yield();
  }
  file.close();
  if (okay) okay = mbedtls_sha256_finish(&context, output) == 0;
  std::free(buffer);
  mbedtls_sha256_free(&context);
  return okay;
#endif
}

bool WirelessUpdate::hashPartition(const void* opaquePartition, uint32_t bytes,
                                   uint8_t output[32]) const {
#if !defined(ARDUINO_ARCH_ESP32)
  (void)opaquePartition; (void)bytes; std::memset(output, 0, 32); return true;
#else
  const esp_partition_t* partition = static_cast<const esp_partition_t*>(opaquePartition);
  if (!partition || bytes > partition->size) return false;
  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  bool okay = mbedtls_sha256_starts(&context, 0) == 0;
  uint8_t buffer[1024];
  uint32_t offset = 0;
  while (okay && offset < bytes) {
    const size_t count = std::min<size_t>(sizeof(buffer), bytes - offset);
    okay = esp_partition_read(partition, offset, buffer, count) == ESP_OK &&
            mbedtls_sha256_update(&context, buffer, count) == 0;
    offset += static_cast<uint32_t>(count);
    yield();
  }
  if (okay) okay = mbedtls_sha256_finish(&context, output) == 0;
  mbedtls_sha256_free(&context);
  return okay;
#endif
}

bool WirelessUpdate::signatureValid(const Manifest& manifest) const {
#if !defined(ARDUINO_ARCH_ESP32)
  (void)manifest;
  return true;
#else
  uint8_t digest[32]{};
  if (mbedtls_sha256(reinterpret_cast<const uint8_t*>(&manifest),
                     offsetof(Manifest, signature), digest, 0) != 0) return false;
  mbedtls_ecp_group group;
  mbedtls_ecp_point publicPoint;
  mbedtls_mpi r, s;
  mbedtls_ecp_group_init(&group);
  mbedtls_ecp_point_init(&publicPoint);
  mbedtls_mpi_init(&r);
  mbedtls_mpi_init(&s);
  bool okay = mbedtls_ecp_group_load(&group, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
      mbedtls_ecp_point_read_binary(&group, &publicPoint, kPokegochiUpdatePublicKey,
                                    sizeof(kPokegochiUpdatePublicKey)) == 0 &&
      mbedtls_mpi_read_binary(&r, manifest.signature, 32) == 0 &&
      mbedtls_mpi_read_binary(&s, manifest.signature + 32, 32) == 0 &&
      mbedtls_ecdsa_verify(&group, digest, sizeof(digest), &publicPoint, &r, &s) == 0;
  mbedtls_mpi_free(&s);
  mbedtls_mpi_free(&r);
  mbedtls_ecp_point_free(&publicPoint);
  mbedtls_ecp_group_free(&group);
  return okay;
#endif
}

uint32_t WirelessUpdate::acceptedPackageSequence() const {
#if !defined(ARDUINO_ARCH_ESP32)
  return 0;
#else
  Preferences preferences;
  if (!preferences.begin("pokegochi-ota", true)) return 0;
  const uint32_t sequence = preferences.getUInt("sequence", 0);
  preferences.end();
  return sequence;
#endif
}

void WirelessUpdate::storeAcceptedPackageSequence(uint32_t sequence) const {
#if defined(ARDUINO_ARCH_ESP32)
  Preferences preferences;
  if (preferences.begin("pokegochi-ota", false)) {
    preferences.putUInt("sequence", sequence);
    preferences.end();
  }
#else
  (void)sequence;
#endif
}

bool WirelessUpdate::loadPendingBoot(char target[8], char previous[8],
                                     uint32_t& sequence, uint8_t& attempts) const {
#if !defined(ARDUINO_ARCH_ESP32)
  (void)target; (void)previous; (void)sequence; (void)attempts;
  return false;
#else
  Preferences preferences;
  if (!preferences.begin("pokegochi-ota", true)) return false;
  const bool pending = preferences.getBool("pending", false);
  if (pending) {
    preferences.getString("target", target, 8);
    preferences.getString("previous", previous, 8);
    sequence = preferences.getUInt("pkg", 0);
    attempts = preferences.getUChar("attempts", 0);
  }
  preferences.end();
  return pending && target[0] && previous[0] && sequence;
#endif
}

bool WirelessUpdate::storePendingBoot(const char* target, const char* previous,
                                      uint32_t sequence, uint8_t attempts) const {
#if !defined(ARDUINO_ARCH_ESP32)
  (void)target; (void)previous; (void)sequence; (void)attempts;
  return true;
#else
  Preferences preferences;
  if (!preferences.begin("pokegochi-ota", false)) return false;
  const bool okay = preferences.putString("target", target ? target : "") > 0U &&
      preferences.putString("previous", previous ? previous : "") > 0U &&
      preferences.putUInt("pkg", sequence) > 0U &&
      preferences.putUChar("attempts", attempts) > 0U &&
      preferences.putBool("pending", true) > 0U;
  preferences.end();
  return okay;
#endif
}

void WirelessUpdate::clearPendingBoot() const {
#if defined(ARDUINO_ARCH_ESP32)
  Preferences preferences;
  if (preferences.begin("pokegochi-ota", false)) {
    preferences.remove("pending");
    preferences.remove("target");
    preferences.remove("previous");
    preferences.remove("pkg");
    preferences.remove("attempts");
    preferences.end();
  }
#endif
}

void WirelessUpdate::recalculatePackageProgress() {
  if (!manifestValid_) { packageBytesDone_ = 0; return; }
  uint64_t done = journal_.firmwareReady ? manifest_.firmwareSize : 0U;
  for (uint8_t index = 0; index < manifest_.assetCount; ++index)
    if (journal_.verifiedAssetMask & assetBit(static_cast<AssetId>(manifest_.assets[index].id)))
      done += manifest_.assets[index].size;
  if (state_ == DeviceState::ReceivingObject) {
    const bool alreadyCounted = objectKind_ == ObjectKind::Firmware
        ? journal_.firmwareReady != 0
        : (journal_.verifiedAssetMask & assetBit(assetId_)) != 0;
    if (!alreadyCounted) done += objectOffset_;
  }
  packageBytesDone_ = static_cast<uint32_t>(std::min<uint64_t>(done, packageBytesTotal_));
}

void WirelessUpdate::sendStatus(StatusCode code) {
  changed_ = true;
#if defined(ARDUINO_ARCH_ESP32)
  if (!gStatusCharacteristic) return;
  const StatusPacket packet{{'U', 'P'}, kVersion, static_cast<uint8_t>(code),
      static_cast<uint8_t>(state_), static_cast<uint8_t>(error_),
      static_cast<uint8_t>(objectKind_), static_cast<uint8_t>(assetId_),
      objectOffset_, objectSize_, packageBytesDone_, packageBytesTotal_};
  gStatusCharacteristic->setValue(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  if (connected_) gStatusCharacteristic->notify();
#else
  (void)code;
#endif
}
