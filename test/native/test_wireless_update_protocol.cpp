#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "services/WirelessUpdateProtocol.h"

using namespace PokegochiUpdateProtocol;

int main() {
  static_assert(sizeof(Advertisement) == 13);
  static_assert(sizeof(TimeSyncCommand) == 5);
  static_assert(kCapabilityTimeSync == 1);
  static_assert(kMaximumDataBytes == 508);
  static_assert(sizeof(AssetRecord) == 40);
  static_assert(sizeof(Manifest) == 276);
  static_assert(offsetof(Manifest, signature) == 212);
  static_assert(sizeof(StatusPacket) == 24);

  Manifest manifest{};
  std::memcpy(manifest.magic, "PGU1", 4);
  manifest.protocolVersion = kVersion;
  manifest.deviceModel = kDeviceModel;
  manifest.structureSize = sizeof(Manifest);
  manifest.assetCount = kMaximumAssets;
  for (uint8_t index = 0; index < kMaximumAssets; ++index) {
    manifest.assets[index].id = index + 1U;
    manifest.assets[index].size = 100U + index;
  }
  assert(findAsset(manifest, AssetId::Archive)->size == 100U);
  assert(findAsset(manifest, AssetId::ManifestJson)->size == 103U);
  assert(findAsset(manifest, static_cast<AssetId>(99)) == nullptr);

  StatusPacket status{{'U', 'P'}, kVersion,
      static_cast<uint8_t>(StatusCode::DataAccepted),
      static_cast<uint8_t>(DeviceState::ReceivingObject),
      static_cast<uint8_t>(ErrorCode::None),
      static_cast<uint8_t>(ObjectKind::Asset),
      static_cast<uint8_t>(AssetId::Archive), 184U, 1000U, 184U, 2000U};
  const uint8_t* raw = reinterpret_cast<const uint8_t*>(&status);
  assert(raw[0] == 'U' && raw[1] == 'P' && raw[2] == kVersion);
  assert(status.nextOffset == 184U && status.packageBytesTotal == 2000U);
  return 0;
}
