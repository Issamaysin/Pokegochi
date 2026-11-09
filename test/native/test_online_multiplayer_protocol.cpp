#include <cassert>
#include <cstdint>
#include <cstring>

#include "services/OnlineMultiplayerProtocol.h"

int main() {
  using namespace PokegochiOnlineProtocol;
  static_assert(kMaximumClients == 12U);
  static_assert(kMaximumSessions == 6U);
  static_assert(kMaximumSessions * 2U <= kMaximumClients);
  const FrameHeader hello = makeHeader(MessageType::Hello, sizeof(HelloPayload),
                                       12345U);
  assert(validHeader(hello));
  const auto* bytes = reinterpret_cast<const uint8_t*>(&hello);
  assert(bytes[0] == 'P' && bytes[1] == 'G' && bytes[2] == 'O' && bytes[3] == 'N');
  assert(bytes[4] == kVersion && bytes[5] == static_cast<uint8_t>(MessageType::Hello));
  const HelloPayload helloPayload{
      PokegochiMultiplayerProtocol::kVersion, {0, 0, 0}, 123456U, 27U};
  assert(helloPayload.accessCode == 123456U);
  assert(helloPayload.balancedWins == 27U);
  const PeerEntryPayload entry{27U};
  assert(entry.balancedWins == helloPayload.balancedWins);

  FrameHeader invalid = hello;
  invalid.version++;
  assert(!validHeader(invalid));
  invalid = hello;
  invalid.payloadLength = kMaximumGamePacket + 1U;
  assert(!validHeader(invalid));

  uint8_t packet[kMaximumGamePacket]{};
  const PokegochiMultiplayerProtocol::PacketHeader game{
      PokegochiMultiplayerProtocol::kPacketMagic,
      PokegochiMultiplayerProtocol::kVersion,
      PokegochiMultiplayerProtocol::BattleCommand,
      12345U, 77U, 3U};
  std::memcpy(packet, &game, sizeof(game));
  assert(validGamePacket(packet, sizeof(game) + 3U));
  assert(!validGamePacket(packet, sizeof(game) + 2U));
  packet[2] = 0;
  assert(!validGamePacket(packet, sizeof(game) + 3U));
  return 0;
}
