#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "services/MultiplayerProtocol.h"

// TCP control protocol used between every internet-capable Pokegochi client
// and the small dedicated ESP32 relay. The battle/trade payload remains the
// exact current MultiplayerProtocol packet; this envelope only provides presence,
// virtual sessions and routing through NAT.
namespace PokegochiOnlineProtocol {

constexpr uint32_t kMagic = 0x4E4F4750UL;  // bytes "PGON" on little endian
constexpr uint8_t kLegacyVersion = 1;
constexpr uint8_t kVersion = 2;
constexpr uint16_t kDefaultPort = 32145;
constexpr char kMdnsHostName[] = "pokegochi-server";
constexpr char kMdnsServiceName[] = "pokegochi";
constexpr char kMdnsTransport[] = "tcp";
constexpr char kMdnsAccessCodeKey[] = "code";
constexpr char kMdnsGameProtocolKey[] = "game-protocol";
// The ESP32 build exposes 16 lwIP sockets. Twelve local clients plus the TCP
// listener, MQTT uplink and mDNS socket use fifteen at peak; cloud clients are
// multiplexed through that single MQTT socket. Keep one descriptor in reserve
// for recovery/reconnect work instead of advertising a fragile theoretical 13.
constexpr uint8_t kMaximumClients = 12;
constexpr uint8_t kMaximumSessions = 6;
static_assert(kMaximumSessions * 2U <= kMaximumClients,
              "Every player-vs-player session needs two client slots");
constexpr uint16_t kMaximumGamePacket =
    static_cast<uint16_t>(sizeof(PokegochiMultiplayerProtocol::PacketHeader) +
                          PokegochiMultiplayerProtocol::kMaximumPayload);

enum class MessageType : uint8_t {
  Hello = 1,
  Welcome,
  RefreshPeers,
  PeerEntry,
  PeerListComplete,
  OpenSession,
  SessionReady,
  SessionBusy,
  CloseSession,
  SessionClosed,
  Relay,
  Ping,
  Pong,
  Error,
};

enum class ErrorCode : uint8_t {
  None = 0,
  BadFrame,
  WrongVersion,
  AccessDenied,
  DuplicatePlayer,
  ServerFull,
  PeerUnavailable,
  SessionMismatch,
};

#pragma pack(push, 1)
struct FrameHeader {
  uint32_t magic;
  uint8_t version;
  uint8_t type;
  uint16_t payloadLength;
  uint32_t sourceId;
  uint32_t targetId;
  uint32_t sessionId;
};

struct HelloPayload {
  uint8_t gameProtocolVersion;
  uint8_t reserved[3];
  uint32_t accessCode;
  uint32_t balancedWins;
};

// PGON v1 shipped before the Balanced victory counter. The current clients
// never emit it, but the home server accepts it so updating the server cannot
// strand an older physical Pokegochi or APK.
struct LegacyHelloPayload {
  uint8_t gameProtocolVersion;
  uint8_t reserved[3];
  uint32_t accessCode;
};

struct PeerEntryPayload {
  uint32_t balancedWins;
};

struct SessionReadyPayload {
  uint8_t outbound;
};

struct ErrorPayload {
  uint8_t code;
};
#pragma pack(pop)

static_assert(sizeof(FrameHeader) == 20, "Online frame header ABI changed");
static_assert(sizeof(HelloPayload) == 12, "Online hello ABI changed");
static_assert(sizeof(LegacyHelloPayload) == 8, "Legacy online hello ABI changed");
static_assert(sizeof(PeerEntryPayload) == 4, "Online peer entry ABI changed");
static_assert(kMaximumGamePacket == 174, "Game packet budget changed");

inline FrameHeader makeHeader(MessageType type, uint16_t payloadLength,
                              uint32_t sourceId = 0, uint32_t targetId = 0,
                              uint32_t sessionId = 0) {
  return FrameHeader{kMagic, kVersion, static_cast<uint8_t>(type), payloadLength,
                     sourceId, targetId, sessionId};
}

inline bool validHeader(const FrameHeader& header) {
  if (header.magic != kMagic || header.version != kVersion) return false;
  const auto type = static_cast<MessageType>(header.type);
  if (type < MessageType::Hello || type > MessageType::Error) return false;
  return header.payloadLength <= kMaximumGamePacket;
}

inline bool validGamePacket(const uint8_t* bytes, size_t length) {
  if (!bytes || length < sizeof(PokegochiMultiplayerProtocol::PacketHeader) ||
      length > kMaximumGamePacket) return false;
  PokegochiMultiplayerProtocol::PacketHeader packet{};
  std::memcpy(&packet, bytes, sizeof(packet));
  return packet.magic == PokegochiMultiplayerProtocol::kPacketMagic &&
         packet.version == PokegochiMultiplayerProtocol::kVersion &&
         packet.payloadLength <= PokegochiMultiplayerProtocol::kMaximumPayload &&
         sizeof(packet) + packet.payloadLength == length;
}

}  // namespace PokegochiOnlineProtocol
