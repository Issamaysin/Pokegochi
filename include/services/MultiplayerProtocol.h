#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// Stable wire constants shared by the firmware transport and the Android app.
// Packet payload structures remain in Multiplayer.cpp until the ABI extraction
// is complete; these constants are already the single source of truth for
// discovery, framing and protocol versioning.
namespace PokegochiMultiplayerProtocol {

constexpr uint8_t kVersion = 7;
constexpr uint16_t kPacketMagic = 0x4750; // bytes "PG" in little-endian order
constexpr uint16_t kMaximumPayload = 160;
// Android's BLE API separates the mandatory two-byte manufacturer id from
// its value, while NimBLE exposes the complete manufacturer field.  The
// original PGCH wire signature already starts with bytes P,G, so those bytes
// are the stable manufacturer id and Android advertises the remaining seven.
constexpr uint16_t kManufacturerId = 0x4750; // bytes "PG"
// 0xffff was used by the first Android transport, followed by all nine PGCH
// bytes. Keep accepting it so already-installed beta apps remain discoverable.
constexpr uint16_t kLegacyAndroidManufacturerId = 0xffff;
constexpr char kServiceUuid[] = "70564743-4849-4d50-8000-504f4b45474f";
constexpr char kRxUuid[] = "70564743-4849-4d50-8001-504f4b45474f";
constexpr char kTxUuid[] = "70564743-4849-4d50-8002-504f4b45474f";

enum PacketType : uint8_t {
  Invite = 1,
  InviteAccept,
  InviteReject,
  SessionClose,
  ActivityRequest,
  ActivityAccept,
  ActivityReject,
  TradeOffer,
  TradeConfirm,
  TradeReject,
  TradeCommitRequest,
  TradeCommitApplied,
  BattlePartyMember,
  BattlePartyReady,
  BattleLead,
  BattleStart,
  ActivityCancel,
  BattleCommand,
  BattleSyncPokemon,
  BattleSyncVolatile,
  BattleSyncCore,
  BattleSyncAck,
};

#pragma pack(push, 1)
struct PacketHeader {
  uint16_t magic;
  uint8_t version;
  uint8_t type;
  uint32_t sourceId;
  uint32_t transaction;
  uint16_t payloadLength;
};

struct Advertisement {
  char signature[4];
  uint8_t version;
  uint32_t playerId;
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 14, "Multiplayer packet header ABI changed");
static_assert(sizeof(Advertisement) == 9, "Multiplayer advertisement ABI changed");

inline Advertisement makeAdvertisement(uint32_t playerId) {
  return Advertisement{{'P', 'G', 'C', 'H'}, kVersion, playerId};
}

// Decodes the bytes returned by NimBLE's getManufacturerData(). Canonical
// packets are the nine-byte PGCH record. The eleven-byte variant is the old
// Android framing (ffff + PGCH) and is accepted only for compatibility.
inline bool decodeManufacturerAdvertisement(const uint8_t* bytes, size_t length,
                                            Advertisement& advertisement) {
  if (!bytes) return false;
  size_t offset = 0;
  if (length == sizeof(Advertisement) + sizeof(uint16_t) &&
      bytes[0] == static_cast<uint8_t>(kLegacyAndroidManufacturerId & 0xffU) &&
      bytes[1] == static_cast<uint8_t>(kLegacyAndroidManufacturerId >> 8U)) {
    offset = sizeof(uint16_t);
  } else if (length != sizeof(Advertisement)) {
    return false;
  }
  std::memcpy(&advertisement, bytes + offset, sizeof(advertisement));
  return std::memcmp(advertisement.signature, "PGCH", 4) == 0 &&
         advertisement.version == kVersion && advertisement.playerId != 0;
}

}  // namespace PokegochiMultiplayerProtocol
