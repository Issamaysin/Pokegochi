#pragma once

#include <cstddef>
#include <cstdint>

namespace PokegochiUpdateProtocol {

constexpr uint8_t kVersion = 1;
constexpr uint8_t kDeviceModel = 1;  // ESP32-2432S028R.
constexpr uint8_t kMaximumAssets = 4;
constexpr uint16_t kMaximumManifestBytes = 384;
// A 517-byte ATT MTU carries a 512-byte characteristic value.  Four bytes
// belong to DataHeader, leaving 508 useful bytes per acknowledged write.
// Older updater apps remain compatible: they keep sending 184-byte blocks.
constexpr uint16_t kMaximumDataBytes = 508;
constexpr uint8_t kCapabilityTimeSync = 0x01U;
constexpr uint8_t kCapabilitySaveManagement = 0x02U;
// The updater transport can also remain connected as a read-only live
// renderer trace while the game is running. It is deliberately advertised so
// a desktop diagnostic tool can distinguish this firmware from older builds.
constexpr uint8_t kCapabilityLiveDiagnostics = 0x04U;
constexpr uint16_t kSaveBackupVersion = 1;
constexpr uint32_t kMaximumSaveBackupBytes = 64U * 1024U;

constexpr char kServiceUuid[] = "70564743-4849-5550-8000-504f4b45474f";
constexpr char kControlUuid[] = "70564743-4849-5550-8001-504f4b45474f";
constexpr char kDataUuid[] = "70564743-4849-5550-8002-504f4b45474f";
constexpr char kStatusUuid[] = "70564743-4849-5550-8003-504f4b45474f";
constexpr char kLiveLogUuid[] = "70564743-4849-5550-8004-504f4b45474f";

enum class Command : uint8_t {
  Authenticate = 1,
  ManifestBegin,
  ManifestChunk,
  ManifestFinish,
  ObjectBegin,
  ObjectFinish,
  Commit,
  Abort,
  QueryStatus,
  SynchronizeTime,
  SaveExportBegin,
  SaveExportChunk,
  SaveExportFinish,
  SaveImportBegin,
  SaveImportFinish,
  SaveReset,
  DiagnosticRead,
  LiveLogStart,
  LiveLogStop,
};

enum class ObjectKind : uint8_t { Firmware = 0, Asset = 1, Save = 2 };

enum class AssetId : uint8_t {
  Archive = 1,
  PackVersion,
  AnimationCatalog,
  ManifestJson,
};

enum class DeviceState : uint8_t {
  Advertising = 0,
  AwaitingAuthentication,
  AwaitingManifest,
  ReceivingManifest,
  ReadyForObject,
  ReceivingObject,
  ReadyToCommit,
  Committing,
  Rebooting,
  Error,
  ExportingSave,
  ImportingSave,
};

enum class StatusCode : uint8_t {
  Hello = 1,
  Authenticated,
  ManifestAccepted,
  ObjectReady,
  DataAccepted,
  ObjectVerified,
  CommitAccepted,
  Complete,
  Aborted,
  Error,
  TimeSynchronized,
  SaveExportReady,
  SaveExportComplete,
  SaveImportReady,
  SaveImported,
  SaveReset,
  DiagnosticReady,
  LiveLogReady,
  LiveLogStopped,
};

enum class ErrorCode : uint8_t {
  None = 0,
  BadCommand,
  AuthenticationFailed,
  ManifestSize,
  ManifestFormat,
  ManifestSignature,
  PackageRollback,
  AssetUnavailable,
  NotEnoughSpace,
  ObjectOrder,
  ObjectRange,
  ObjectWrite,
  ObjectHash,
  FirmwareBegin,
  FirmwareValidation,
  CommitFailed,
  Busy,
  SaveUnavailable,
  SaveFormat,
  SaveChecksum,
  DiagnosticUnavailable,
};

#pragma pack(push, 1)
struct Advertisement {
  char signature[4];  // "PGOT"
  uint8_t protocolVersion;
  uint8_t deviceModel;
  uint32_t playerId;
  // Optional extension.  Legacy firmware advertised only the first 10 bytes;
  // a new Android client therefore falls back to 184 when this is absent.
  uint16_t maximumDataBytes;
  // Optional second extension. Unknown bits are ignored by phone clients.
  uint8_t capabilities;
};

struct AssetRecord {
  uint8_t id;
  uint8_t reserved[3];
  uint32_t size;
  uint8_t sha256[32];
};

// The signature is the raw P-256 (r || s) signature over every byte before
// `signature`. A fixed binary manifest keeps parsing deterministic on the
// memory-constrained ESP32 and is equally easy for the Android client.
struct Manifest {
  char magic[4];  // "PGU1"
  uint8_t protocolVersion;
  uint8_t deviceModel;
  uint16_t structureSize;
  uint32_t packageSequence;
  uint32_t firmwareSize;
  uint16_t assetPackVersion;
  uint8_t assetCount;
  uint8_t flags;
  uint8_t firmwareSha256[32];
  AssetRecord assets[kMaximumAssets];
  uint8_t signature[64];
};

struct AuthenticateCommand {
  uint8_t command;
  uint8_t protocolVersion;
  uint32_t pairingCode;
};

struct TimeSyncCommand {
  uint8_t command;
  // Unix seconds shifted by the phone's current UTC offset. This lets the
  // console render local wall time without maintaining a timezone database.
  uint32_t localEpochSeconds;
};

struct SaveExportChunkCommand {
  uint8_t command;
  uint32_t offset;
  uint16_t maximumBytes;
};

struct SaveImportBeginCommand {
  uint8_t command;
  uint32_t totalBytes;
  uint32_t fileCrc32;
  uint16_t saveFormatVersion;
};

// Read-only request used by the desktop diagnostic tool. `selector` is a
// move id for AnimationMove and is ignored by Summary.
struct DiagnosticReadCommand {
  uint8_t command;
  uint8_t section;
  uint16_t selector;
};

// Portable, versioned save container exchanged with the Android app. The
// payload is the exact current GameSave ABI; both checksums use standard
// CRC-32/ISO-HDLC (the same representation as java.util.zip.CRC32).
struct SaveBackupHeader {
  char magic[4];  // "PGS1"
  uint16_t backupVersion;
  uint16_t saveFormatVersion;
  uint32_t payloadBytes;
  uint32_t sourcePlayerId;
  uint32_t payloadCrc32;
  uint32_t headerCrc32;
};

struct ManifestBeginCommand {
  uint8_t command;
  uint16_t totalBytes;
};

struct ManifestChunkHeader {
  uint8_t command;
  uint16_t offset;
};

struct ObjectBeginCommand {
  uint8_t command;
  uint8_t kind;
  uint8_t assetId;
};

struct DataHeader {
  uint32_t offset;
};

// Diagnostic responses use the existing data characteristic as a notify-only
// stream. Keeping the header independent from DataHeader means an updater
// client can never mistake a read-back packet for an OTA write.
struct DiagnosticPacket {
  char magic[2];  // "DG"
  uint8_t protocolVersion;
  uint8_t section;
  uint16_t selector;
  uint16_t offset;
  uint16_t totalBytes;
};

// Live renderer telemetry is sent on its own notify characteristic so it can
// never be confused with a diagnostic readback chunk or an OTA data block.
// The payload is UTF-8 text on purpose: the capture remains useful even if
// the board resets before a complete binary trace is assembled.
struct LiveLogPacket {
  char magic[2];  // "LG"
  uint8_t protocolVersion;
  uint8_t kind;  // 1 = UTF-8 event line
  uint32_t sequence;
  uint32_t uptimeMs;
  uint16_t payloadBytes;
};

struct StatusPacket {
  char magic[2];  // "UP"
  uint8_t protocolVersion;
  uint8_t code;
  uint8_t state;
  uint8_t error;
  uint8_t objectKind;
  uint8_t assetId;
  uint32_t nextOffset;
  uint32_t objectSize;
  uint32_t packageBytesDone;
  uint32_t packageBytesTotal;
};
#pragma pack(pop)

static_assert(sizeof(Advertisement) == 13, "Update advertisement ABI changed");
static_assert(sizeof(TimeSyncCommand) == 5, "Time-sync command ABI changed");
static_assert(sizeof(SaveExportChunkCommand) == 7, "Save-export command ABI changed");
static_assert(sizeof(SaveImportBeginCommand) == 11, "Save-import command ABI changed");
static_assert(sizeof(DiagnosticReadCommand) == 4, "Diagnostic-read command ABI changed");
static_assert(sizeof(DiagnosticPacket) == 10, "Diagnostic packet ABI changed");
static_assert(sizeof(LiveLogPacket) == 14, "Live-log packet ABI changed");
static_assert(sizeof(SaveBackupHeader) == 24, "Save-backup header ABI changed");
static_assert(sizeof(AssetRecord) == 40, "Update asset record ABI changed");
static_assert(sizeof(Manifest) == 276, "Update manifest ABI changed");
static_assert(sizeof(StatusPacket) == 24, "Update status ABI changed");
static_assert(offsetof(Manifest, signature) == 212, "Update signature range changed");

inline const AssetRecord* findAsset(const Manifest& manifest, AssetId id) {
  for (uint8_t index = 0; index < manifest.assetCount && index < kMaximumAssets; ++index)
    if (manifest.assets[index].id == static_cast<uint8_t>(id)) return &manifest.assets[index];
  return nullptr;
}

}  // namespace PokegochiUpdateProtocol
