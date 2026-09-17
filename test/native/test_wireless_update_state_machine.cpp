#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "services/WirelessUpdate.h"

using namespace PokegochiUpdateProtocol;

static void sendManifest(WirelessUpdate& update, const Manifest& manifest) {
  ManifestBeginCommand begin{static_cast<uint8_t>(Command::ManifestBegin), sizeof(Manifest)};
  update.transportControl(reinterpret_cast<const uint8_t*>(&begin), sizeof(begin));
  assert(update.state() == DeviceState::ReceivingManifest);
  const uint8_t* source = reinterpret_cast<const uint8_t*>(&manifest);
  uint16_t offset = 0;
  while (offset < sizeof(manifest)) {
    const uint16_t count = static_cast<uint16_t>(
        std::min<size_t>(97U, sizeof(manifest) - offset));
    std::vector<uint8_t> packet(sizeof(ManifestChunkHeader) + count);
    ManifestChunkHeader header{static_cast<uint8_t>(Command::ManifestChunk), offset};
    std::memcpy(packet.data(), &header, sizeof(header));
    std::memcpy(packet.data() + sizeof(header), source + offset, count);
    update.transportControl(packet.data(), packet.size());
    offset = static_cast<uint16_t>(offset + count);
  }
  const uint8_t finish = static_cast<uint8_t>(Command::ManifestFinish);
  update.transportControl(&finish, 1);
  assert(update.state() == DeviceState::ReadyForObject);
}

static void sendObject(WirelessUpdate& update, ObjectKind kind, uint8_t assetId,
                       uint32_t size) {
  ObjectBeginCommand begin{static_cast<uint8_t>(Command::ObjectBegin),
                           static_cast<uint8_t>(kind), assetId};
  update.transportControl(reinterpret_cast<const uint8_t*>(&begin), sizeof(begin));
  assert(update.state() == DeviceState::ReceivingObject);
  uint32_t offset = 0;
  while (offset < size) {
    const uint32_t count = std::min<uint32_t>(kMaximumDataBytes, size - offset);
    std::vector<uint8_t> packet(sizeof(DataHeader) + count, 0xA5);
    DataHeader header{offset};
    std::memcpy(packet.data(), &header, sizeof(header));
    update.transportData(packet.data(), packet.size());
    offset += count;
  }
  const uint8_t finish = static_cast<uint8_t>(Command::ObjectFinish);
  update.transportControl(&finish, 1);
  assert(update.state() == DeviceState::ReadyForObject ||
         update.state() == DeviceState::ReadyToCommit);
}

int main() {
  WirelessUpdate update;
  assert(update.begin(12345, "TEST", 18, true));
  update.transportConnected();
  assert(update.state() == DeviceState::AwaitingAuthentication);

  AuthenticateCommand auth{static_cast<uint8_t>(Command::Authenticate), kVersion,
                           update.pairingCode()};
  update.transportControl(reinterpret_cast<const uint8_t*>(&auth), sizeof(auth));
  assert(update.state() == DeviceState::AwaitingManifest);

  Manifest manifest{};
  std::memcpy(manifest.magic, "PGU1", 4);
  manifest.protocolVersion = kVersion;
  manifest.deviceModel = kDeviceModel;
  manifest.structureSize = sizeof(Manifest);
  manifest.packageSequence = 42;
  manifest.firmwareSize = 367;
  manifest.assetPackVersion = 19;
  manifest.assetCount = kMaximumAssets;
  uint32_t expectedTotal = manifest.firmwareSize;
  for (uint8_t index = 0; index < kMaximumAssets; ++index) {
    manifest.assets[index].id = index + 1U;
    manifest.assets[index].size = 101U + index;
    expectedTotal += manifest.assets[index].size;
  }
  sendManifest(update, manifest);
  assert(update.bytesTotal() == expectedTotal);

  for (uint8_t index = 0; index < kMaximumAssets; ++index)
    sendObject(update, ObjectKind::Asset, index + 1U, 101U + index);
  sendObject(update, ObjectKind::Firmware, 0, manifest.firmwareSize);
  assert(update.state() == DeviceState::ReadyToCommit);
  assert(update.progressPercent() == 100U);

  const uint8_t commit = static_cast<uint8_t>(Command::Commit);
  update.transportControl(&commit, 1);
  assert(update.state() == DeviceState::Rebooting);
  assert(update.busy());

  WirelessUpdate unauthenticated;
  assert(unauthenticated.begin(54321, "TEST", 18, true));
  unauthenticated.transportConnected();
  const uint8_t abort = static_cast<uint8_t>(Command::Abort);
  unauthenticated.transportControl(&abort, 1);
  assert(unauthenticated.state() == DeviceState::Error);
  assert(unauthenticated.error() == ErrorCode::AuthenticationFailed);
  return 0;
}
