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

constexpr char kServiceUuid[] = "70564743-4849-5550-8000-504f4b45474f";
constexpr char kControlUuid[] = "70564743-4849-5550-8001-504f4b45474f";
constexpr char kDataUuid[] = "70564743-4849-5550-8002-504f4b45474f";
constexpr char kStatusUuid[] = "70564743-4849-5550-8003-504f4b45474f";

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
};

enum class ObjectKind : uint8_t { Firmware = 0, Asset = 1 };

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

static_assert(sizeof(Advertisement) == 12, "Update advertisement ABI changed");
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
