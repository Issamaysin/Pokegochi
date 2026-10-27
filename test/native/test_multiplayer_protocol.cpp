#include <cassert>
#include <cstdint>
#include <cstring>

#include "services/MultiplayerProtocol.h"

int main() {
  using namespace PokegochiMultiplayerProtocol;
  assert(kVersion == 7);
  assert(kPacketMagic == 0x4750);
  assert(kManufacturerId == 0x4750);
  assert(kLegacyAndroidManufacturerId == 0xffff);
  assert(kMaximumPayload == 160);
  assert(std::strcmp(kServiceUuid, "70564743-4849-4d50-8000-504f4b45474f") == 0);
  PacketHeader packet{ kPacketMagic, kVersion, BattleCommand, 0x01020304U,
                       0xAABBCCDDU, 3U };
  const auto* bytes = reinterpret_cast<const uint8_t*>(&packet);
  assert(bytes[0] == 0x50 && bytes[1] == 0x47 && bytes[2] == 7 && bytes[3] == BattleCommand);
  assert(bytes[4] == 4 && bytes[5] == 3 && bytes[6] == 2 && bytes[7] == 1);
  assert(bytes[12] == 3 && bytes[13] == 0);

  const Advertisement expected = makeAdvertisement(0x01020304U);
  const auto* advertisementBytes = reinterpret_cast<const uint8_t*>(&expected);
  const uint8_t expectedAdvertisement[] = {
      0x50, 0x47, 0x43, 0x48, 0x07, 0x04, 0x03, 0x02, 0x01};
  assert(std::memcmp(advertisementBytes, expectedAdvertisement,
                     sizeof(expectedAdvertisement)) == 0);
  Advertisement decoded{};
  assert(decodeManufacturerAdvertisement(expectedAdvertisement,
                                          sizeof(expectedAdvertisement), decoded));
  assert(decoded.playerId == 0x01020304U);

  const uint8_t legacyAndroidAdvertisement[] = {
      0xff, 0xff, 0x50, 0x47, 0x43, 0x48, 0x07, 0x04, 0x03, 0x02, 0x01};
  assert(decodeManufacturerAdvertisement(legacyAndroidAdvertisement,
                                          sizeof(legacyAndroidAdvertisement), decoded));
  assert(decoded.playerId == 0x01020304U);

  uint8_t wrongManufacturer[sizeof(legacyAndroidAdvertisement)]{};
  std::memcpy(wrongManufacturer, legacyAndroidAdvertisement, sizeof(wrongManufacturer));
  wrongManufacturer[0] = 0x34;
  assert(!decodeManufacturerAdvertisement(wrongManufacturer,
                                           sizeof(wrongManufacturer), decoded));
  uint8_t wrongVersion[sizeof(expectedAdvertisement)]{};
  std::memcpy(wrongVersion, expectedAdvertisement, sizeof(wrongVersion));
  wrongVersion[4] = 5;
  assert(!decodeManufacturerAdvertisement(wrongVersion, sizeof(wrongVersion), decoded));
  return 0;
}
