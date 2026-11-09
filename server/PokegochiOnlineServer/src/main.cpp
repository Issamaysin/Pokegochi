#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "services/OnlineMultiplayerProtocol.h"
#include "config/OnlineRelayConfig.h"
#include "game/BattleEngine.h"
#include "ServerBattleBot.h"
#include "ServerLocalConfig.h"

namespace {
using namespace PokegochiOnlineProtocol;

constexpr uint32_t kClientIdleTimeoutMs = 30000U;
constexpr uint32_t kClientSessionTimeoutMs = 120000U;
constexpr uint32_t kWifiRetryMs = 10000U;
constexpr uint32_t kWifiRestartMs = 10U * 60U * 1000U;
constexpr uint32_t kCloudRetryMs = 5000U;
constexpr uint8_t kBotActivityBattle = 1U;
constexpr uint8_t kBotActivityTrade = 2U;
constexpr uint8_t kBotRulesNormal = 1U;
constexpr uint8_t kBotRulesBalanced = 2U;
constexpr uint8_t kBotCommandMove = 1U;
constexpr uint8_t kBotCommandSwitch = 2U;
constexpr uint8_t kBotCommandForfeit = 3U;
constexpr uint8_t kBotCommandWait = 4U;
constexpr uint8_t kBotCommandItem = 5U;
constexpr uint8_t kBotMoveZFlag = 0x80U;

#pragma pack(push, 1)
struct BotActivityPayload { uint8_t activity; uint8_t rules; };
struct BotPartyPayload { uint8_t slot; OwnedPokemon pokemon; };
struct BotReadyPayload { uint8_t count; };
struct BotLeadPayload { uint8_t slot; };
struct BotStartPayload {
  uint32_t seed;
  uint8_t hostLeadSlot;
  uint8_t guestLeadSlot;
};
struct BotCommandPayload { uint16_t turn; uint8_t kind; uint8_t value; };
struct BotSyncPokemonPayload {
  uint16_t turn;
  uint8_t side;
  uint8_t slot;
  OwnedPokemon pokemon;
};
struct BotSyncVolatilePayload {
  uint16_t turn;
  uint8_t side;
  uint8_t slot;
  CombatVolatile state;
  DedicatedMoveEffectState moveEffects;
  Gen4BattlerEffectState gen4Effects;
};
struct BotSyncAckPayload { uint16_t turn; uint32_t snapshotChecksum; };
#pragma pack(pop)

static_assert(sizeof(BotPartyPayload) == 65U,
              "Server bot party ABI changed");
static_assert(sizeof(BotSyncPokemonPayload) == 68U,
              "Server bot sync ABI changed");
static_assert(sizeof(BotSyncVolatilePayload) <= 160U,
              "Server bot volatile sync exceeds packet budget");

enum class ClientTransport : uint8_t { LocalTcp, CloudMqtt };

struct ClientSlot {
  WiFiClient socket{};
  bool occupied = false;
  bool authenticated = false;
  ClientTransport transport = ClientTransport::LocalTcp;
  uint32_t cloudNonce = 0;
  uint8_t protocolVersion = 0;
  uint32_t playerId = 0;
  uint32_t balancedWins = 0;
  uint32_t peerId = 0;
  uint32_t sessionId = 0;
  uint32_t lastSeenMs = 0;
  FrameHeader incomingHeader{};
  uint16_t headerBytes = 0;
  uint16_t payloadBytes = 0;
  uint8_t payload[kMaximumGamePacket]{};
};

struct BotRuntime {
  bool busy = false;
  bool activityAccepted = false;
  bool battleActive = false;
  bool zMoveRequested = false;
  bool balancedWinRecorded = false;
  uint32_t opponentId = 0U;
  uint32_t sessionId = 0U;
  uint32_t decisionState = 0x50474F42U;
  uint16_t battleSerial = 0U;
  uint16_t lastCommandTurn = 0xFFFFU;
  uint16_t lastForcedTurn = 0xFFFFU;
  uint32_t lastForcedSnapshotChecksum = 0U;
  bool forcedCommandPending = false;
  uint16_t lastPivotMoveTurn = 0xFFFFU;
  uint8_t lastPivotMoveSlot = 0xFFU;
  uint8_t lastPivotMoveRosterSlot = 0xFFU;
  uint8_t lastPivotMovePp = 0U;
  bool pivotMovePending = false;
  bool pivotMoveExecuted = false;
  uint8_t rules = 0U;
  uint8_t activeSlot = 0U;
  uint8_t hostActiveSlot = 0U;
  uint8_t hostCount = 0U;
  uint8_t guestCount = 0U;
  uint16_t hostHp[PokegochiServerBot::kTeamSize]{};
  OwnedPokemon hostTeam[PokegochiServerBot::kTeamSize]{};
  CombatVolatile hostVolatiles[PokegochiServerBot::kTeamSize]{};
  CombatVolatile guestVolatiles[PokegochiServerBot::kTeamSize]{};
  DedicatedMoveEffectState
      hostMoveEffects[PokegochiServerBot::kTeamSize]{};
  DedicatedMoveEffectState
      guestMoveEffects[PokegochiServerBot::kTeamSize]{};
  Gen4BattlerEffectState
      hostGen4Effects[PokegochiServerBot::kTeamSize]{};
  Gen4BattlerEffectState
      guestGen4Effects[PokegochiServerBot::kTeamSize]{};
  PokegochiServerBot::BattleBag bag{};
  PokegochiServerBot::Team team{};
};

Preferences preferences;
WiFiServer relayServer(kDefaultPort);
WiFiClient cloudSocket;
PubSubClient cloudMqtt(cloudSocket);
ClientSlot clients[kMaximumClients]{};
BotRuntime battleBot{};
uint32_t botBalancedWins = 0U;
uint32_t accessCode = 0;
bool relayListening = false;
bool discoveryAdvertising = false;
uint32_t lastWifiAttemptMs = 0;
uint32_t wifiUnavailableSinceMs = 0;
uint32_t lastStatusMs = 0;
uint32_t lastCloudAttemptMs = 0;
bool cloudReconnectDeferredForSession = false;
char cloudServerTopic[96]{};

bool cloudClientConnected(const ClientSlot& client) {
  return client.transport == ClientTransport::CloudMqtt &&
      client.cloudNonce != 0U && cloudMqtt.connected();
}

void formatCloudClientTopic(char* output, size_t capacity,
                            uint32_t playerId, uint32_t nonce) {
  std::snprintf(output, capacity, "%s/client/%05lu/%08lx",
                PokegochiOnlineRelayConfig::kTopicRoot,
                static_cast<unsigned long>(playerId),
                static_cast<unsigned long>(nonce));
}

ClientSlot* findPlayer(uint32_t playerId) {
  for (ClientSlot& client : clients)
    if (client.occupied && client.authenticated && client.playerId == playerId)
      return &client;
  return nullptr;
}

uint8_t authenticatedCount() {
  uint8_t count = 0;
  for (const ClientSlot& client : clients)
    if (client.occupied && client.authenticated) ++count;
  return count;
}

uint8_t sessionCount() {
  uint8_t count = 0;
  for (const ClientSlot& client : clients)
    if (client.occupied && client.sessionId &&
        client.peerId != PokegochiServerBot::kPlayerId &&
        client.playerId < client.peerId) ++count;
  return static_cast<uint8_t>(count + (battleBot.busy ? 1U : 0U));
}

bool sendFrame(ClientSlot& client, MessageType type, uint32_t sourceId = 0,
               uint32_t targetId = 0, uint32_t sessionId = 0,
               const void* payload = nullptr, uint16_t payloadLength = 0) {
  if (!client.occupied || payloadLength > kMaximumGamePacket) return false;
  FrameHeader header = makeHeader(type, payloadLength, sourceId, targetId,
                                  sessionId);
  // Reply in the envelope version used by this client. PGON v1 and v2 share
  // the same frame header and all payloads except HELLO/PEER_ENTRY.
  if (client.protocolVersion == kLegacyVersion)
    header.version = kLegacyVersion;
  if (client.transport == ClientTransport::CloudMqtt) {
    if (!cloudClientConnected(client)) return false;
    uint8_t frame[sizeof(FrameHeader) + kMaximumGamePacket]{};
    std::memcpy(frame, &header, sizeof(header));
    if (payloadLength && payload)
      std::memcpy(frame + sizeof(header), payload, payloadLength);
    char topic[96]{};
    formatCloudClientTopic(topic, sizeof(topic), client.playerId,
                           client.cloudNonce);
    return cloudMqtt.publish(topic, frame,
                             static_cast<unsigned int>(sizeof(header) +
                                                       payloadLength), false);
  }
  if (!client.socket.connected()) return false;
  // A PGON envelope owns the TCP stream until its payload is sent. Splitting
  // the header and payload into separate writes creates a burst of tiny TCP
  // segments and can leave the no-PSRAM handheld waiting behind delayed ACKs.
  // Keep every ordinary frame atomic; the bot preparation below goes one step
  // further and batches its six ordered frames into a single sub-MSS write.
  uint8_t frame[sizeof(FrameHeader) + kMaximumGamePacket]{};
  std::memcpy(frame, &header, sizeof(header));
  if (payloadLength && payload)
    std::memcpy(frame + sizeof(header), payload, payloadLength);
  const size_t frameLength = sizeof(header) + payloadLength;
  const size_t written = client.socket.write(frame, frameLength);
  if (written != frameLength) {
    Serial.printf("[SERVER] short TCP frame write=%u/%u; closing stream\n",
                  static_cast<unsigned>(written),
                  static_cast<unsigned>(frameLength));
    client.socket.stop();
    return false;
  }
  return true;
}

uint32_t nextBotRandom() {
  uint32_t& state = battleBot.decisionState;
  if (!state) state = 0x50474F42U;
  state ^= state << 13U;
  state ^= state >> 17U;
  state ^= state << 5U;
  return state;
}

void resetBotActivity() {
  battleBot.activityAccepted = false;
  battleBot.battleActive = false;
  battleBot.zMoveRequested = false;
  battleBot.balancedWinRecorded = false;
  battleBot.rules = 0U;
  battleBot.activeSlot = 0U;
  battleBot.hostActiveSlot = 0U;
  battleBot.hostCount = 0U;
  battleBot.guestCount = 0U;
  battleBot.lastCommandTurn = 0xFFFFU;
  battleBot.lastForcedTurn = 0xFFFFU;
  battleBot.lastForcedSnapshotChecksum = 0U;
  battleBot.forcedCommandPending = false;
  battleBot.lastPivotMoveTurn = 0xFFFFU;
  battleBot.lastPivotMoveSlot = 0xFFU;
  battleBot.lastPivotMoveRosterSlot = 0xFFU;
  battleBot.lastPivotMovePp = 0U;
  battleBot.pivotMovePending = false;
  battleBot.pivotMoveExecuted = false;
  std::memset(battleBot.hostHp, 0, sizeof(battleBot.hostHp));
  std::memset(battleBot.hostTeam, 0, sizeof(battleBot.hostTeam));
  std::memset(battleBot.hostVolatiles, 0,
              sizeof(battleBot.hostVolatiles));
  std::memset(battleBot.guestVolatiles, 0,
              sizeof(battleBot.guestVolatiles));
  std::memset(battleBot.hostMoveEffects, 0,
              sizeof(battleBot.hostMoveEffects));
  std::memset(battleBot.guestMoveEffects, 0,
              sizeof(battleBot.guestMoveEffects));
  std::memset(battleBot.hostGen4Effects, 0,
              sizeof(battleBot.hostGen4Effects));
  std::memset(battleBot.guestGen4Effects, 0,
              sizeof(battleBot.guestGen4Effects));
  battleBot.bag = PokegochiServerBot::BattleBag{};
  battleBot.team = PokegochiServerBot::Team{};
}

void releaseBotSession(uint32_t opponentId, uint32_t sessionId) {
  if (!battleBot.busy || battleBot.opponentId != opponentId ||
      battleBot.sessionId != sessionId) return;
  const uint16_t serial = battleBot.battleSerial;
  battleBot = BotRuntime{};
  battleBot.battleSerial = serial;
  Serial.printf("[BOT] player=%05lu session=%08lX released\n",
                static_cast<unsigned long>(opponentId),
                static_cast<unsigned long>(sessionId));
}

bool sendBotGamePacket(ClientSlot& client, uint8_t type,
                       const void* payload = nullptr,
                       uint16_t payloadLength = 0U,
                       uint32_t transaction = 0U) {
  if (!battleBot.busy || battleBot.opponentId != client.playerId ||
      battleBot.sessionId != client.sessionId ||
      payloadLength > PokegochiMultiplayerProtocol::kMaximumPayload)
    return false;
  uint8_t packet[kMaximumGamePacket]{};
  const PokegochiMultiplayerProtocol::PacketHeader header{
      PokegochiMultiplayerProtocol::kPacketMagic,
      PokegochiMultiplayerProtocol::kVersion,
      type,
      PokegochiServerBot::kPlayerId,
      transaction,
      payloadLength};
  std::memcpy(packet, &header, sizeof(header));
  if (payloadLength && payload)
    std::memcpy(packet + sizeof(header), payload, payloadLength);
  return sendFrame(client, MessageType::Relay,
                   PokegochiServerBot::kPlayerId, client.playerId,
                   client.sessionId, packet,
                   static_cast<uint16_t>(sizeof(header) + payloadLength));
}

bool sendBotCommand(ClientSlot& client, uint16_t turn, uint8_t kind,
                    uint8_t value = 0U) {
  const BotCommandPayload command{turn, kind, value};
  if (!sendBotGamePacket(client, PokegochiMultiplayerProtocol::BattleCommand,
                         &command, sizeof(command), turn))
    return false;
  battleBot.lastCommandTurn = turn;
  return true;
}

bool sendBotRoster(ClientSlot& client) {
  for (uint8_t slot = 0; slot < PokegochiServerBot::kTeamSize; ++slot) {
    const BotPartyPayload member{slot, battleBot.team.members[slot]};
    if (!sendBotGamePacket(client,
                           PokegochiMultiplayerProtocol::BattlePartyMember,
                           &member, sizeof(member)))
      return false;
  }
  const BotReadyPayload ready{PokegochiServerBot::kTeamSize};
  if (!sendBotGamePacket(client,
                         PokegochiMultiplayerProtocol::BattlePartyReady,
                         &ready, sizeof(ready)))
    return false;
  const BotLeadPayload lead{0U};
  return sendBotGamePacket(client, PokegochiMultiplayerProtocol::BattleLead,
                           &lead, sizeof(lead));
}

bool sendBotPreparation(ClientSlot& client, const BotActivityPayload& request,
                        uint32_t transaction) {
  // MQTT messages contain exactly one PGON frame, so retain the ordinary
  // publish sequence for Internet clients. The broker preserves publication
  // order on this connection.
  if (client.transport == ClientTransport::CloudMqtt) {
    return sendBotGamePacket(
               client, PokegochiMultiplayerProtocol::ActivityAccept,
               &request, sizeof(request), transaction) &&
        sendBotRoster(client);
  }
  if (!client.socket.connected() || !battleBot.busy ||
      battleBot.opponentId != client.playerId ||
      battleBot.sessionId != client.sessionId)
    return false;

  // ActivityAccept + three Party members + READY + lead fit below one TCP MSS.
  // Sending them in one write makes the bot lock-in visible immediately and
  // prevents Nagle/delayed-ACK interaction between six tiny server writes.
  constexpr size_t kPreparationBatchCapacity = 768U;
  uint8_t batch[kPreparationBatchCapacity]{};
  size_t batchLength = 0U;
  auto appendGameFrame = [&](uint8_t type, const void* payload,
                             uint16_t payloadLength,
                             uint32_t packetTransaction = 0U) {
    if (payloadLength > PokegochiMultiplayerProtocol::kMaximumPayload)
      return false;
    const uint16_t gameLength = static_cast<uint16_t>(
        sizeof(PokegochiMultiplayerProtocol::PacketHeader) + payloadLength);
    const size_t frameLength = sizeof(FrameHeader) + gameLength;
    if (batchLength + frameLength > sizeof(batch)) return false;

    FrameHeader frameHeader = makeHeader(
        MessageType::Relay, gameLength, PokegochiServerBot::kPlayerId,
        client.playerId, client.sessionId);
    if (client.protocolVersion == kLegacyVersion)
      frameHeader.version = kLegacyVersion;
    const PokegochiMultiplayerProtocol::PacketHeader packetHeader{
        PokegochiMultiplayerProtocol::kPacketMagic,
        PokegochiMultiplayerProtocol::kVersion,
        type,
        PokegochiServerBot::kPlayerId,
        packetTransaction,
        payloadLength};
    std::memcpy(batch + batchLength, &frameHeader, sizeof(frameHeader));
    batchLength += sizeof(frameHeader);
    std::memcpy(batch + batchLength, &packetHeader, sizeof(packetHeader));
    batchLength += sizeof(packetHeader);
    if (payloadLength && payload) {
      std::memcpy(batch + batchLength, payload, payloadLength);
      batchLength += payloadLength;
    }
    return true;
  };

  if (!appendGameFrame(PokegochiMultiplayerProtocol::ActivityAccept,
                       &request, sizeof(request), transaction))
    return false;
  for (uint8_t slot = 0; slot < PokegochiServerBot::kTeamSize; ++slot) {
    const BotPartyPayload member{slot, battleBot.team.members[slot]};
    if (!appendGameFrame(PokegochiMultiplayerProtocol::BattlePartyMember,
                         &member, sizeof(member)))
      return false;
  }
  const BotReadyPayload ready{PokegochiServerBot::kTeamSize};
  const BotLeadPayload lead{0U};
  if (!appendGameFrame(PokegochiMultiplayerProtocol::BattlePartyReady,
                       &ready, sizeof(ready)) ||
      !appendGameFrame(PokegochiMultiplayerProtocol::BattleLead,
                       &lead, sizeof(lead)))
    return false;

  const size_t written = client.socket.write(batch, batchLength);
  if (written != batchLength) {
    Serial.printf("[BOT] preparation short write=%u/%u; closing stream\n",
                  static_cast<unsigned>(written),
                  static_cast<unsigned>(batchLength));
    client.socket.stop();
    return false;
  }
  Serial.printf("[BOT] preparation batch bytes=%u frames=6\n",
                static_cast<unsigned>(batchLength));
  return true;
}

struct BotActionChoice {
  uint8_t kind = kBotCommandMove;
  uint8_t value = 0U;
  PokegochiServerBot::MoveChoice move{};
  PokegochiServerBot::ItemChoice item{};
};

BotActionChoice chooseBotAction() {
  BotActionChoice action{};
  if (battleBot.activeSlot >= PokegochiServerBot::kTeamSize) return action;
  const OwnedPokemon& active = battleBot.team.members[battleBot.activeSlot];
  PokegochiServerBot::MoveChoiceContext context{};
  if (battleBot.hostActiveSlot < battleBot.hostCount &&
      battleBot.hostTeam[battleBot.hostActiveSlot].uid)
    context.opponent = &battleBot.hostTeam[battleBot.hostActiveSlot];
  context.userVolatile = &battleBot.guestVolatiles[battleBot.activeSlot];
  context.userMoveEffects =
      &battleBot.guestMoveEffects[battleBot.activeSlot];
  context.userGen4Effects =
      &battleBot.guestGen4Effects[battleBot.activeSlot];
  if (battleBot.hostActiveSlot < PokegochiServerBot::kTeamSize) {
    context.opponentVolatile =
        &battleBot.hostVolatiles[battleBot.hostActiveSlot];
    context.opponentMoveEffects =
        &battleBot.hostMoveEffects[battleBot.hostActiveSlot];
  }
  for (uint8_t slot = 0U; slot < battleBot.guestCount; ++slot)
    if (slot != battleBot.activeSlot && battleBot.team.members[slot].currentHp)
      ++context.healthyReserves;
  context.zMoveAvailable = !battleBot.zMoveRequested &&
      battleBot.activeSlot == battleBot.team.zSlot;
  context.zMoveSlot = battleBot.team.zMoveSlot;
  context.randomValue = nextBotRandom();
  action.move =
      PokegochiServerBot::chooseMove(active, context);

  PokegochiServerBot::ItemChoiceContext itemContext{};
  itemContext.team = battleBot.team.members;
  itemContext.volatiles = battleBot.guestVolatiles;
  itemContext.bag = &battleBot.bag;
  itemContext.teamCount = battleBot.guestCount;
  itemContext.activeSlot = battleBot.activeSlot;
  action.item = PokegochiServerBot::chooseItem(itemContext);
  // A tie goes to the move. This both conserves finite inventory and keeps a
  // useful authored recovery/status move preferable to an equivalent item.
  if (action.item.valid && action.item.score > action.move.score) {
    action.kind = kBotCommandItem;
    action.value = static_cast<uint8_t>(
        (static_cast<uint8_t>(action.item.item) & 0x1FU) |
        ((action.item.targetSlot & 0x03U) << 5U));
    Serial.printf("[BOT] choose item=%u target=%u score=%d hp=%u/%u bag=%u/%u/%u/%u\n",
                  static_cast<unsigned>(action.item.item),
                  static_cast<unsigned>(action.item.targetSlot),
                  static_cast<int>(action.item.score),
                  static_cast<unsigned>(active.currentHp),
                  static_cast<unsigned>(active.maximumHp),
                  static_cast<unsigned>(battleBot.bag.hyperPotions),
                  static_cast<unsigned>(battleBot.bag.fullHeals),
                  static_cast<unsigned>(battleBot.bag.maxRevives),
                  static_cast<unsigned>(battleBot.bag.maxElixirs));
    return action;
  }
  if (action.move.slot > kMoveSlots) action.move.slot = kMoveSlots;
  action.value = static_cast<uint8_t>(action.move.slot |
      (action.move.useZMove ? kBotMoveZFlag : 0U));
  Serial.printf("[BOT] choose species=%u move=%u slot=%u score=%d z=%u hp=%u/%u\n",
                static_cast<unsigned>(active.speciesId),
                static_cast<unsigned>(action.move.slot == kMoveSlots
                    ? MoveId::Struggle : active.moves[action.move.slot]),
                static_cast<unsigned>(action.move.slot),
                static_cast<int>(action.move.score),
                static_cast<unsigned>(action.move.useZMove),
                static_cast<unsigned>(active.currentHp),
                static_cast<unsigned>(active.maximumHp));
  return action;
}

bool botMoveRequiresChosenReplacement(const OwnedPokemon& pokemon,
                                      uint8_t commandValue) {
  // Z-Moves based on U-turn/Volt Switch use their Z effect and do not pivot.
  if (commandValue & kBotMoveZFlag) return false;
  const uint8_t slot = static_cast<uint8_t>(commandValue & ~kBotMoveZFlag);
  if (slot >= kMoveSlots) return false;
  const uint16_t move = static_cast<uint16_t>(pokemon.moves[slot]);
  return move == 369U || move == 521U || move == 575U;  // U-turn, Volt Switch, Parting Shot.
}

void rememberBotMove(uint16_t turn, uint8_t commandValue) {
  battleBot.lastPivotMoveTurn = turn;
  battleBot.lastPivotMoveSlot = static_cast<uint8_t>(commandValue & ~kBotMoveZFlag);
  battleBot.lastPivotMoveRosterSlot = battleBot.activeSlot;
  battleBot.lastPivotMovePp = 0U;
  battleBot.pivotMovePending = false;
  battleBot.pivotMoveExecuted = false;
  if (battleBot.activeSlot >= battleBot.guestCount ||
      battleBot.lastPivotMoveSlot >= kMoveSlots) return;
  const OwnedPokemon& active = battleBot.team.members[battleBot.activeSlot];
  if (!botMoveRequiresChosenReplacement(active, commandValue)) return;
  battleBot.lastPivotMovePp = active.movePp[battleBot.lastPivotMoveSlot];
  battleBot.pivotMovePending = true;
}

void clearBotPivotMove() {
  battleBot.lastPivotMoveTurn = 0xFFFFU;
  battleBot.lastPivotMoveSlot = 0xFFU;
  battleBot.lastPivotMoveRosterSlot = 0xFFU;
  battleBot.lastPivotMovePp = 0U;
  battleBot.pivotMovePending = false;
  battleBot.pivotMoveExecuted = false;
}

int8_t chooseBotReplacement() {
  uint8_t choices[PokegochiServerBot::kTeamSize]{};
  uint8_t count = 0U;
  for (uint8_t slot = 0; slot < battleBot.guestCount; ++slot) {
    if (slot == battleBot.activeSlot ||
        !battleBot.team.members[slot].currentHp) continue;
    choices[count++] = slot;
  }
  return count ? static_cast<int8_t>(choices[nextBotRandom() % count]) : -1;
}

bool sendBotPivotReplacement(ClientSlot& client, uint16_t turn) {
  const int8_t replacement = chooseBotReplacement();
  if (replacement < 0) return false;
  if (!sendBotCommand(client, turn, kBotCommandSwitch,
                      static_cast<uint8_t>(replacement))) return false;
  battleBot.lastForcedTurn = turn;
  battleBot.forcedCommandPending = true;
  clearBotPivotMove();
  Serial.printf("[BOT] pivot switch turn=%u slot=%u\n",
                static_cast<unsigned>(turn),
                static_cast<unsigned>(replacement));
  return true;
}

void handleBotSyncCore(ClientSlot& client, const uint8_t* payload,
                       uint16_t length) {
  // The final sixteen bytes are the stable host/guest counts, active slots,
  // and the two six-byte delayed-attack records. This is also asserted by the
  // protocol peer used in hardware regression tests.
  if (!payload || length < 16U) return;
  uint16_t turn = 0U;
  uint32_t checksum = 0U;
  std::memcpy(&turn, payload, sizeof(turn));
  std::memcpy(&checksum, payload + sizeof(turn), sizeof(checksum));
  const uint16_t countsOffset = static_cast<uint16_t>(length - 16U);
  battleBot.hostCount = payload[countsOffset];
  battleBot.guestCount = payload[countsOffset + 1U];
  battleBot.hostActiveSlot = payload[countsOffset + 2U];
  battleBot.activeSlot = payload[countsOffset + 3U];
  battleBot.battleActive = payload[10U] != 0U;
  const uint8_t hostOutcome = payload[11U];
  const BotSyncAckPayload acknowledgement{turn, checksum};
  sendBotGamePacket(client, PokegochiMultiplayerProtocol::BattleSyncAck,
                    &acknowledgement, sizeof(acknowledgement), turn);
  // BattleOutcome::Defeat (3) is the authoritative host losing, hence a bot
  // victory. Commit it once, only for Balanced battles, before the room can
  // be closed or the same final snapshot retried.
  if (!battleBot.battleActive && battleBot.rules == kBotRulesBalanced &&
      hostOutcome == 3U && !battleBot.balancedWinRecorded) {
    battleBot.balancedWinRecorded = true;
    if (botBalancedWins != UINT32_MAX) ++botBalancedWins;
    preferences.putULong("bot-bal-wins", botBalancedWins);
    Serial.printf("[BOT] balanced wins=%lu\n",
                  static_cast<unsigned long>(botBalancedWins));
  }
  if (!battleBot.battleActive ||
      battleBot.activeSlot >= battleBot.guestCount ||
      battleBot.hostActiveSlot >= battleBot.hostCount)
    return;

  const bool guestFainted =
      battleBot.team.members[battleBot.activeSlot].currentHp == 0U;
  const bool hostFainted =
      battleBot.hostHp[battleBot.hostActiveSlot] == 0U;
  // A forced replacement is a separate command pair but deliberately keeps
  // the battle turn number. Once the host publishes the post-switch snapshot,
  // re-arm normal move selection even though `turn` is unchanged. The former
  // turn-only guard discarded that next legal move and made the bot appear to
  // freeze after its first Pokemon fainted.
  if (battleBot.forcedCommandPending && !guestFainted && !hostFainted) {
    battleBot.forcedCommandPending = false;
    battleBot.lastCommandTurn = 0xFFFFU;
    Serial.printf("[BOT] forced switch confirmed turn=%u active=%u/%u\n",
                  static_cast<unsigned>(turn),
                  static_cast<unsigned>(battleBot.hostActiveSlot),
                  static_cast<unsigned>(battleBot.activeSlot));
  }
  // Retries of one frozen authoritative snapshot are idempotent. A genuinely
  // new same-turn snapshot has a different checksum and may require another
  // switch (for example when entry hazards also faint the replacement).
  if ((!guestFainted && !hostFainted) ||
      battleBot.lastForcedSnapshotChecksum == checksum)
    return;
  if (guestFainted) {
    for (uint8_t slot = 0; slot < battleBot.guestCount; ++slot) {
      if (slot == battleBot.activeSlot ||
          !battleBot.team.members[slot].currentHp) continue;
      if (!sendBotCommand(client, turn, kBotCommandSwitch, slot)) return;
      battleBot.lastForcedTurn = turn;
      battleBot.lastForcedSnapshotChecksum = checksum;
      battleBot.forcedCommandPending = true;
      Serial.printf("[BOT] forced switch turn=%u slot=%u\n",
                    static_cast<unsigned>(turn), static_cast<unsigned>(slot));
      return;
    }
  } else if (hostFainted) {
    // Volt Switch/U-turn can knock out the host and still requires the bot to
    // choose its own recipient. Both sides then submit SWITCH in one barrier.
    if (battleBot.pivotMovePending && battleBot.pivotMoveExecuted) {
      if (sendBotPivotReplacement(client, turn)) {
        battleBot.lastForcedSnapshotChecksum = checksum;
        return;
      }
      clearBotPivotMove();
    }
    if (!sendBotCommand(client, turn, kBotCommandWait)) return;
    battleBot.lastForcedTurn = turn;
    battleBot.lastForcedSnapshotChecksum = checksum;
    battleBot.forcedCommandPending = true;
  }
}

void handleBotGamePacket(ClientSlot& client, const uint8_t* bytes,
                         uint16_t length) {
  if (!bytes || length < sizeof(PokegochiMultiplayerProtocol::PacketHeader))
    return;
  PokegochiMultiplayerProtocol::PacketHeader header{};
  std::memcpy(&header, bytes, sizeof(header));
  if (header.sourceId != client.playerId ||
      sizeof(header) + header.payloadLength != length)
    return;
  const uint8_t* payload = bytes + sizeof(header);
  switch (header.type) {
    case PokegochiMultiplayerProtocol::Invite:
      sendBotGamePacket(client, PokegochiMultiplayerProtocol::InviteAccept,
                        nullptr, 0U, header.transaction);
      break;
    case PokegochiMultiplayerProtocol::ActivityRequest: {
      if (header.payloadLength != sizeof(BotActivityPayload)) break;
      BotActivityPayload request{};
      std::memcpy(&request, payload, sizeof(request));
      if (request.activity == kBotActivityTrade) {
        sendBotGamePacket(client, PokegochiMultiplayerProtocol::ActivityReject,
                          &request, sizeof(request), header.transaction);
        Serial.printf("[BOT] trade refused player=%05lu\n",
                      static_cast<unsigned long>(client.playerId));
        break;
      }
      if (request.activity != kBotActivityBattle ||
          (request.rules != kBotRulesNormal &&
           request.rules != kBotRulesBalanced)) {
        sendBotGamePacket(client, PokegochiMultiplayerProtocol::ActivityReject,
                          &request, sizeof(request), header.transaction);
        break;
      }
      resetBotActivity();
      battleBot.activityAccepted = true;
      battleBot.rules = request.rules;
      battleBot.decisionState = esp_random() ^ client.playerId ^
          battleBot.sessionId;
      if (!battleBot.decisionState) battleBot.decisionState = 0x50474F42U;
      ++battleBot.battleSerial;
      const uint8_t level = request.rules == kBotRulesBalanced ? 70U : 100U;
      battleBot.team = PokegochiServerBot::buildTeam(
          nextBotRandom(), level, battleBot.battleSerial);
      battleBot.guestCount = PokegochiServerBot::kTeamSize;
      if (sendBotPreparation(client, request, header.transaction)) {
        Serial.printf("[BOT] battle accepted player=%05lu team=%u/%u/%u "
                      "mega=%d z=%d rules=%u\n",
                      static_cast<unsigned long>(client.playerId),
                      static_cast<unsigned>(battleBot.team.members[0].speciesId),
                      static_cast<unsigned>(battleBot.team.members[1].speciesId),
                      static_cast<unsigned>(battleBot.team.members[2].speciesId),
                      static_cast<int>(battleBot.team.megaSlot),
                      static_cast<int>(battleBot.team.zSlot),
                      static_cast<unsigned>(request.rules));
      } else {
        Serial.printf("[BOT] preparation failed player=%05lu\n",
                      static_cast<unsigned long>(client.playerId));
        resetBotActivity();
      }
      break;
    }
    case PokegochiMultiplayerProtocol::TradeOffer:
    case PokegochiMultiplayerProtocol::TradeConfirm:
    case PokegochiMultiplayerProtocol::TradeCommitRequest:
      sendBotGamePacket(client, PokegochiMultiplayerProtocol::TradeReject,
                        nullptr, 0U, header.transaction);
      break;
    case PokegochiMultiplayerProtocol::BattlePartyMember: {
      if (!battleBot.activityAccepted ||
          header.payloadLength != sizeof(BotPartyPayload)) break;
      BotPartyPayload member{};
      std::memcpy(&member, payload, sizeof(member));
      if (member.slot >= PokegochiServerBot::kTeamSize) break;
      battleBot.hostTeam[member.slot] = member.pokemon;
      battleBot.hostHp[member.slot] = member.pokemon.currentHp;
      battleBot.hostCount = std::max<uint8_t>(
          battleBot.hostCount, static_cast<uint8_t>(member.slot + 1U));
      break;
    }
    case PokegochiMultiplayerProtocol::BattlePartyReady: {
      if (!battleBot.activityAccepted ||
          header.payloadLength != sizeof(BotReadyPayload)) break;
      BotReadyPayload ready{};
      std::memcpy(&ready, payload, sizeof(ready));
      if (ready.count && ready.count <= PokegochiServerBot::kTeamSize)
        battleBot.hostCount = ready.count;
      break;
    }
    case PokegochiMultiplayerProtocol::BattleLead: {
      if (!battleBot.activityAccepted ||
          header.payloadLength != sizeof(BotLeadPayload)) break;
      BotLeadPayload lead{};
      std::memcpy(&lead, payload, sizeof(lead));
      if (lead.slot < PokegochiServerBot::kTeamSize)
        battleBot.hostActiveSlot = lead.slot;
      break;
    }
    case PokegochiMultiplayerProtocol::BattleStart: {
      if (!battleBot.activityAccepted ||
          header.payloadLength != sizeof(BotStartPayload)) break;
      BotStartPayload start{};
      std::memcpy(&start, payload, sizeof(start));
      if (start.hostLeadSlot >= PokegochiServerBot::kTeamSize ||
          start.guestLeadSlot >= PokegochiServerBot::kTeamSize) break;
      battleBot.hostActiveSlot = start.hostLeadSlot;
      battleBot.activeSlot = start.guestLeadSlot;
      battleBot.battleActive = true;
      battleBot.lastCommandTurn = 0xFFFFU;
      battleBot.lastForcedTurn = 0xFFFFU;
      clearBotPivotMove();
      break;
    }
    case PokegochiMultiplayerProtocol::BattleCommand: {
      if (!battleBot.battleActive ||
          header.payloadLength != sizeof(BotCommandPayload)) break;
      BotCommandPayload command{};
      std::memcpy(&command, payload, sizeof(command));
      if (battleBot.forcedCommandPending) break;
      if (battleBot.pivotMovePending &&
          command.turn != battleBot.lastPivotMoveTurn)
        clearBotPivotMove();
      // A successful bot pivot creates a second command pair in the same
      // battle turn. The host submits WAIT (or SWITCH if it also fainted).
      if (battleBot.pivotMovePending && battleBot.pivotMoveExecuted &&
          command.turn == battleBot.lastPivotMoveTurn) {
        if (sendBotPivotReplacement(client, command.turn)) break;
        clearBotPivotMove();
      }
      if (battleBot.lastCommandTurn == command.turn) {
        // The host used Volt Switch/U-turn first. Its selected replacement is
        // paired with WAIT while the already-selected bot move resumes.
        if (command.kind == kBotCommandSwitch &&
            sendBotCommand(client, command.turn, kBotCommandWait)) {
          battleBot.lastForcedTurn = command.turn;
          battleBot.forcedCommandPending = true;
        }
        break;
      }
      if (command.kind == kBotCommandForfeit)
        sendBotCommand(client, command.turn, kBotCommandWait);
      else if (battleBot.activeSlot < PokegochiServerBot::kTeamSize &&
               battleBot.team.members[battleBot.activeSlot].currentHp) {
        const BotActionChoice action = chooseBotAction();
        if (sendBotCommand(client, command.turn, action.kind, action.value)) {
          if (action.kind == kBotCommandItem) {
            PokegochiServerBot::consumeItem(battleBot.bag,
                                            action.item.item);
          } else {
            if (action.move.useZMove) battleBot.zMoveRequested = true;
            rememberBotMove(command.turn, action.value);
          }
        }
      }
      break;
    }
    case PokegochiMultiplayerProtocol::BattleSyncPokemon: {
      if (header.payloadLength != sizeof(BotSyncPokemonPayload)) break;
      BotSyncPokemonPayload sync{};
      std::memcpy(&sync, payload, sizeof(sync));
      if (sync.slot >= PokegochiServerBot::kTeamSize) break;
      if (sync.side == 0U) {
        battleBot.hostTeam[sync.slot] = sync.pokemon;
        battleBot.hostHp[sync.slot] = sync.pokemon.currentHp;
      } else if (sync.side == 1U) {
        if (battleBot.pivotMovePending &&
            sync.slot == battleBot.lastPivotMoveRosterSlot &&
            battleBot.lastPivotMoveSlot < kMoveSlots &&
            sync.pokemon.movePp[battleBot.lastPivotMoveSlot] <
                battleBot.lastPivotMovePp)
          battleBot.pivotMoveExecuted = true;
        battleBot.team.members[sync.slot] = sync.pokemon;
      }
      break;
    }
    case PokegochiMultiplayerProtocol::BattleSyncVolatile: {
      if (header.payloadLength != sizeof(BotSyncVolatilePayload)) break;
      BotSyncVolatilePayload sync{};
      std::memcpy(&sync, payload, sizeof(sync));
      if (sync.side > 1U ||
          sync.slot >= PokegochiServerBot::kTeamSize) break;
      if (sync.side == 0U) {
        battleBot.hostVolatiles[sync.slot] = sync.state;
        battleBot.hostMoveEffects[sync.slot] = sync.moveEffects;
        battleBot.hostGen4Effects[sync.slot] = sync.gen4Effects;
      } else {
        battleBot.guestVolatiles[sync.slot] = sync.state;
        battleBot.guestMoveEffects[sync.slot] = sync.moveEffects;
        battleBot.guestGen4Effects[sync.slot] = sync.gen4Effects;
      }
      break;
    }
    case PokegochiMultiplayerProtocol::BattleSyncCore:
      handleBotSyncCore(client, payload, header.payloadLength);
      break;
    case PokegochiMultiplayerProtocol::ActivityCancel:
      resetBotActivity();
      break;
    case PokegochiMultiplayerProtocol::SessionClose:
      // PGON CLOSE normally follows this logical close. Keeping this packet
      // idempotent lets old clients finish either order without a ghost bot.
      sendBotGamePacket(client, PokegochiMultiplayerProtocol::SessionClose,
                        nullptr, 0U, header.transaction);
      break;
    default:
      break;
  }
}

void sendError(ClientSlot& client, ErrorCode code, uint32_t targetId = 0) {
  const ErrorPayload payload{static_cast<uint8_t>(code)};
  sendFrame(client, MessageType::Error, 0, targetId, client.sessionId,
            &payload, sizeof(payload));
}

void clearSession(ClientSlot& client, bool notifyPeer) {
  if (!client.sessionId) return;
  if (client.peerId == PokegochiServerBot::kPlayerId) {
    const uint32_t session = client.sessionId;
    const uint32_t player = client.playerId;
    client.peerId = 0U;
    client.sessionId = 0U;
    releaseBotSession(player, session);
    return;
  }
  ClientSlot* peer = findPlayer(client.peerId);
  const uint32_t session = client.sessionId;
  const uint32_t player = client.playerId;
  client.peerId = 0;
  client.sessionId = 0;
  if (peer && peer->sessionId == session && peer->peerId == player) {
    peer->peerId = 0;
    peer->sessionId = 0;
    if (notifyPeer)
      sendFrame(*peer, MessageType::SessionClosed, player, peer->playerId,
                session);
  }
}

void dropClient(ClientSlot& client, const char* reason) {
  if (!client.occupied) return;
  Serial.printf("[SERVER] disconnect player=%05lu auth=%u reason=%s\n",
                static_cast<unsigned long>(client.playerId),
                client.authenticated ? 1U : 0U, reason ? reason : "unknown");
  clearSession(client, true);
  if (client.transport == ClientTransport::LocalTcp) client.socket.stop();
  client = ClientSlot{};
}

void sendPeerList(ClientSlot& destination) {
  uint8_t delivered = 0U;
  for (const ClientSlot& candidate : clients) {
    if (!candidate.occupied || !candidate.authenticated || candidate.sessionId ||
        candidate.playerId == destination.playerId) continue;
    const PeerEntryPayload entry{candidate.balancedWins};
    if (sendFrame(destination, MessageType::PeerEntry, candidate.playerId,
                  destination.playerId, 0,
                  destination.protocolVersion == kLegacyVersion ? nullptr : &entry,
                  destination.protocolVersion == kLegacyVersion ? 0U : sizeof(entry)))
      ++delivered;
  }
  if (!battleBot.busy && destination.playerId != PokegochiServerBot::kPlayerId) {
    const PeerEntryPayload entry{botBalancedWins};
    if (sendFrame(destination, MessageType::PeerEntry,
                  PokegochiServerBot::kPlayerId, destination.playerId,
                  0, destination.protocolVersion == kLegacyVersion ? nullptr : &entry,
                  destination.protocolVersion == kLegacyVersion ? 0U : sizeof(entry)))
      ++delivered;
  }
  sendFrame(destination, MessageType::PeerListComplete, 0,
            destination.playerId);
  Serial.printf("[SERVER] peer-list player=%05lu pgon=v%u entries=%u bot-busy=%u\n",
                static_cast<unsigned long>(destination.playerId),
                static_cast<unsigned>(destination.protocolVersion),
                static_cast<unsigned>(delivered), battleBot.busy ? 1U : 0U);
}

void handleHello(ClientSlot& client, const FrameHeader& header,
                 const uint8_t* payload) {
  const bool legacy = header.version == kLegacyVersion;
  const uint16_t expectedLength = legacy ? sizeof(LegacyHelloPayload)
                                         : sizeof(HelloPayload);
  if (client.authenticated || header.payloadLength != expectedLength ||
      header.sourceId < 10000U || header.sourceId > 99999U ||
      header.sourceId == PokegochiServerBot::kPlayerId) {
    sendError(client, ErrorCode::BadFrame);
    return;
  }
  HelloPayload hello{};
  if (legacy) {
    LegacyHelloPayload previous{};
    std::memcpy(&previous, payload, sizeof(previous));
    hello.gameProtocolVersion = previous.gameProtocolVersion;
    hello.accessCode = previous.accessCode;
    hello.balancedWins = 0U;
  } else {
    std::memcpy(&hello, payload, sizeof(hello));
  }
  if (hello.gameProtocolVersion != PokegochiMultiplayerProtocol::kVersion) {
    sendError(client, ErrorCode::WrongVersion);
    return;
  }
  if (hello.accessCode != accessCode) {
    sendError(client, ErrorCode::AccessDenied);
    return;
  }
  if (findPlayer(header.sourceId)) {
    sendError(client, ErrorCode::DuplicatePlayer);
    return;
  }
  client.authenticated = true;
  client.protocolVersion = header.version;
  client.playerId = header.sourceId;
  client.balancedWins = hello.balancedWins;
  client.lastSeenMs = millis();
  sendFrame(client, MessageType::Welcome, 0, client.playerId);
  Serial.printf("[SERVER] online player=%05lu pgon=v%u clients=%u\n",
                static_cast<unsigned long>(client.playerId),
                static_cast<unsigned>(client.protocolVersion),
                static_cast<unsigned>(authenticatedCount()));
}

bool validServerHeader(const FrameHeader& header) {
  if (header.magic != kMagic ||
      (header.version != kVersion && header.version != kLegacyVersion))
    return false;
  const auto type = static_cast<MessageType>(header.type);
  return type >= MessageType::Hello && type <= MessageType::Error &&
      header.payloadLength <= kMaximumGamePacket;
}

void handleOpenSession(ClientSlot& client, const FrameHeader& header) {
  if (client.sessionId || !header.targetId || header.targetId == client.playerId) {
    sendError(client, ErrorCode::PeerUnavailable, header.targetId);
    return;
  }
  if (header.targetId == PokegochiServerBot::kPlayerId) {
    if (battleBot.busy || sessionCount() >= kMaximumSessions) {
      sendFrame(client, MessageType::SessionBusy,
                PokegochiServerBot::kPlayerId, client.playerId);
      return;
    }
    uint32_t session = esp_random();
    if (!session) session = 1U;
    client.peerId = PokegochiServerBot::kPlayerId;
    client.sessionId = session;
    battleBot.busy = true;
    battleBot.opponentId = client.playerId;
    battleBot.sessionId = session;
    resetBotActivity();
    const SessionReadyPayload outgoing{1U};
    if (!sendFrame(client, MessageType::SessionReady,
                   PokegochiServerBot::kPlayerId, client.playerId, session,
                   &outgoing, sizeof(outgoing))) {
      client.peerId = 0U;
      client.sessionId = 0U;
      releaseBotSession(client.playerId, session);
      return;
    }
    Serial.printf("[BOT] session=%08lX player=%05lu active=%u\n",
                  static_cast<unsigned long>(session),
                  static_cast<unsigned long>(client.playerId),
                  static_cast<unsigned>(sessionCount()));
    return;
  }
  ClientSlot* peer = findPlayer(header.targetId);
  if (!peer || peer->sessionId || sessionCount() >= kMaximumSessions) {
    sendFrame(client, MessageType::SessionBusy, header.targetId,
              client.playerId);
    return;
  }
  uint32_t session = esp_random();
  if (!session) session = 1U;
  client.peerId = peer->playerId;
  client.sessionId = session;
  peer->peerId = client.playerId;
  peer->sessionId = session;
  const SessionReadyPayload outgoing{1U};
  const SessionReadyPayload incoming{0U};
  const bool outgoingReady = sendFrame(
      client, MessageType::SessionReady, peer->playerId, client.playerId,
      session, &outgoing, sizeof(outgoing));
  const bool incomingReady = sendFrame(
      *peer, MessageType::SessionReady, client.playerId, peer->playerId,
      session, &incoming, sizeof(incoming));
  if (!outgoingReady || !incomingReady) {
    // Room creation is atomic. Never leave one endpoint believing a room is
    // active when the other endpoint did not receive SESSION_READY.
    client.peerId = 0;
    client.sessionId = 0;
    peer->peerId = 0;
    peer->sessionId = 0;
    if (outgoingReady)
      sendFrame(client, MessageType::SessionClosed, peer->playerId,
                client.playerId, session);
    if (incomingReady)
      sendFrame(*peer, MessageType::SessionClosed, client.playerId,
                peer->playerId, session);
    Serial.printf("[SERVER] session=%08lX rolled back ready=%u/%u\n",
                  static_cast<unsigned long>(session), outgoingReady ? 1U : 0U,
                  incomingReady ? 1U : 0U);
    return;
  }
  Serial.printf("[SERVER] session=%08lX players=%05lu/%05lu active=%u\n",
                static_cast<unsigned long>(session),
                static_cast<unsigned long>(client.playerId),
                static_cast<unsigned long>(peer->playerId),
                static_cast<unsigned>(sessionCount()));
}

void handleRelay(ClientSlot& client, const FrameHeader& header,
                 const uint8_t* payload) {
  if (!client.sessionId || header.sessionId != client.sessionId ||
      header.targetId != client.peerId ||
      !validGamePacket(payload, header.payloadLength)) {
    sendError(client, ErrorCode::SessionMismatch, header.targetId);
    return;
  }
  if (client.peerId == PokegochiServerBot::kPlayerId) {
    if (!battleBot.busy || battleBot.opponentId != client.playerId ||
        battleBot.sessionId != client.sessionId) {
      clearSession(client, false);
      sendFrame(client, MessageType::SessionClosed,
                PokegochiServerBot::kPlayerId, client.playerId,
                header.sessionId);
      return;
    }
    handleBotGamePacket(client, payload, header.payloadLength);
    return;
  }
  ClientSlot* peer = findPlayer(client.peerId);
  if (!peer || peer->sessionId != client.sessionId ||
      peer->peerId != client.playerId) {
    clearSession(client, false);
    sendFrame(client, MessageType::SessionClosed, header.targetId,
              client.playerId, header.sessionId);
    return;
  }
  if (!sendFrame(*peer, MessageType::Relay, client.playerId, peer->playerId,
                 client.sessionId, payload, header.payloadLength))
    dropClient(*peer, "relay write failed");
}

void processFrame(ClientSlot& client) {
  const FrameHeader header = client.incomingHeader;
  client.lastSeenMs = millis();
  const MessageType type = static_cast<MessageType>(header.type);
  if (!client.authenticated && type != MessageType::Hello) {
    sendError(client, ErrorCode::AccessDenied);
    return;
  }
  if (client.authenticated && header.version != client.protocolVersion) {
    sendError(client, ErrorCode::WrongVersion);
    return;
  }
  if (client.authenticated && header.sourceId != client.playerId) {
    sendError(client, ErrorCode::BadFrame);
    return;
  }
  switch (type) {
    case MessageType::Hello:
      handleHello(client, header, client.payload);
      break;
    case MessageType::RefreshPeers:
      sendPeerList(client);
      break;
    case MessageType::OpenSession:
      handleOpenSession(client, header);
      break;
    case MessageType::CloseSession:
      if (!client.sessionId) {
        // Repeated closes are harmless and acknowledged, which lets clients
        // recover if the first acknowledgement was lost.
        sendFrame(client, MessageType::SessionClosed, header.targetId,
                  client.playerId, header.sessionId);
      } else if (header.sessionId == client.sessionId &&
                 header.targetId == client.peerId) {
        const uint32_t peerId = client.peerId;
        const uint32_t sessionId = client.sessionId;
        clearSession(client, true);
        sendFrame(client, MessageType::SessionClosed, peerId,
                  client.playerId, sessionId);
      } else {
        // A delayed CLOSE from an old room must never tear down the current
        // room after either player has already reconnected.
        sendError(client, ErrorCode::SessionMismatch, header.targetId);
      }
      break;
    case MessageType::Relay:
      handleRelay(client, header, client.payload);
      break;
    case MessageType::Ping:
      sendFrame(client, MessageType::Pong, 0, client.playerId);
      break;
    case MessageType::Pong:
      break;
    default:
      sendError(client, ErrorCode::BadFrame);
      break;
  }
}

bool readClient(ClientSlot& client) {
  while (client.socket.available()) {
    if (client.headerBytes < sizeof(FrameHeader)) {
      uint8_t* destination = reinterpret_cast<uint8_t*>(&client.incomingHeader) +
                             client.headerBytes;
      const int read = client.socket.read(destination,
          static_cast<size_t>(sizeof(FrameHeader) - client.headerBytes));
      if (read <= 0) break;
      client.headerBytes = static_cast<uint16_t>(client.headerBytes + read);
      if (client.headerBytes != sizeof(FrameHeader)) continue;
      if (!validServerHeader(client.incomingHeader)) return false;
      if (!client.authenticated &&
          static_cast<MessageType>(client.incomingHeader.type) ==
              MessageType::Hello)
        client.protocolVersion = client.incomingHeader.version;
      client.payloadBytes = 0;
      if (!client.incomingHeader.payloadLength) {
        processFrame(client);
        client.headerBytes = 0;
        client.incomingHeader = {};
      }
      continue;
    }
    const uint16_t remaining = static_cast<uint16_t>(
        client.incomingHeader.payloadLength - client.payloadBytes);
    const int read = client.socket.read(client.payload + client.payloadBytes,
                                        remaining);
    if (read <= 0) break;
    client.payloadBytes = static_cast<uint16_t>(client.payloadBytes + read);
    if (client.payloadBytes == client.incomingHeader.payloadLength) {
      processFrame(client);
      client.headerBytes = 0;
      client.payloadBytes = 0;
      client.incomingHeader = {};
    }
  }
  return true;
}

ClientSlot* allocateCloudClient(uint32_t playerId, uint32_t nonce) {
  for (ClientSlot& existing : clients) {
    if (!existing.occupied || existing.transport != ClientTransport::CloudMqtt ||
        existing.playerId != playerId || existing.cloudNonce != nonce) continue;
    return &existing;
  }
  // A phone changing between Wi-Fi and mobile data reconnects with a new
  // MQTT nonce. Replace its stale route immediately instead of waiting for a
  // 30-second timeout that would leave the other player in a dead room.
  if (ClientSlot* previous = findPlayer(playerId))
    dropClient(*previous, "player route replaced");
  for (ClientSlot& client : clients) {
    if (client.occupied) continue;
    client = ClientSlot{};
    client.occupied = true;
    client.transport = ClientTransport::CloudMqtt;
    client.playerId = playerId;
    client.cloudNonce = nonce;
    client.lastSeenMs = millis();
    Serial.printf("[CLOUD] route player=%05lu nonce=%08lx\n",
                  static_cast<unsigned long>(playerId),
                  static_cast<unsigned long>(nonce));
    return &client;
  }
  return nullptr;
}

void receiveCloudFrame(char* topic, uint8_t* bytes, unsigned int length) {
  if (!topic || !bytes || length < sizeof(FrameHeader) ||
      length > sizeof(FrameHeader) + kMaximumGamePacket) return;
  const size_t prefixLength = std::strlen(cloudServerTopic) - 1U;
  if (std::strncmp(topic, cloudServerTopic, prefixLength) != 0) return;
  const char* route = topic + prefixLength;
  if (*route == '/') ++route;
  char* separator = nullptr;
  const uint32_t playerId = std::strtoul(route, &separator, 10);
  if (!separator || *separator != '/') return;
  const uint32_t nonce = std::strtoul(separator + 1, nullptr, 16);
  if (playerId < 10000U || playerId > 99999U || !nonce) return;
  FrameHeader header{};
  std::memcpy(&header, bytes, sizeof(header));
  if (!validServerHeader(header) || header.sourceId != playerId ||
      sizeof(header) + header.payloadLength != length) return;
  ClientSlot* client = allocateCloudClient(playerId, nonce);
  if (!client) return;
  if (!client->authenticated &&
      static_cast<MessageType>(header.type) == MessageType::Hello)
    client->protocolVersion = header.version;
  client->incomingHeader = header;
  client->headerBytes = sizeof(header);
  client->payloadBytes = header.payloadLength;
  if (header.payloadLength)
    std::memcpy(client->payload, bytes + sizeof(header), header.payloadLength);
  processFrame(*client);
  client->incomingHeader = {};
  client->headerBytes = client->payloadBytes = 0;
}

void serviceCloudTransport(uint32_t now) {
  if (cloudMqtt.connected()) {
    cloudReconnectDeferredForSession = false;
    cloudMqtt.loop();
    return;
  }
  for (ClientSlot& client : clients)
    if (client.occupied && client.transport == ClientTransport::CloudMqtt)
      dropClient(client, "cloud broker unavailable");
  // PubSubClient performs a synchronous DNS/TCP/MQTT handshake. A broker
  // reconnect used to stop servicing the local socket for tens of seconds in
  // the middle of READY, filling the Pokegochi TCP window and leaving only
  // the first Party member at the bot. Preserve an active LAN/bot battle and
  // reconnect the optional Internet route as soon as that room closes.
  if (sessionCount()) {
    if (!cloudReconnectDeferredForSession) {
      cloudReconnectDeferredForSession = true;
      Serial.println("[CLOUD] reconnect deferred until active session ends");
    }
    return;
  }
  cloudReconnectDeferredForSession = false;
  if (now - lastCloudAttemptMs < kCloudRetryMs) return;
  lastCloudAttemptMs = now;
  char clientId[48]{};
  std::snprintf(clientId, sizeof(clientId), "pokegochi-server-%08lx",
                static_cast<unsigned long>(ESP.getEfuseMac()));
  if (!cloudMqtt.connect(clientId)) {
    Serial.printf("[CLOUD] broker retry state=%d\n", cloudMqtt.state());
    return;
  }
  if (!cloudMqtt.subscribe(cloudServerTopic, 0)) {
    Serial.println("[CLOUD] subscribe failed");
    cloudMqtt.disconnect();
    return;
  }
  Serial.printf("[CLOUD] internet relay ready broker=%s\n",
                PokegochiOnlineRelayConfig::kBrokerHost);
}

void acceptClients() {
  WiFiClient incoming = relayServer.accept();
  if (!incoming) return;
  for (ClientSlot& client : clients) {
    if (client.occupied) continue;
    incoming.setNoDelay(true);
    client = ClientSlot{};
    client.socket = incoming;
    client.occupied = true;
    client.lastSeenMs = millis();
    Serial.printf("[SERVER] TCP client from %s\n",
                  incoming.remoteIP().toString().c_str());
    return;
  }
  const FrameHeader header = makeHeader(MessageType::Error,
      sizeof(ErrorPayload));
  const ErrorPayload payload{static_cast<uint8_t>(ErrorCode::ServerFull)};
  incoming.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
  incoming.write(reinterpret_cast<const uint8_t*>(&payload), sizeof(payload));
  incoming.stop();
}

void serviceClients() {
  acceptClients();
  const uint32_t now = millis();
  for (ClientSlot& client : clients) {
    if (!client.occupied) continue;
    if (client.transport == ClientTransport::LocalTcp &&
        !client.socket.connected()) {
      dropClient(client, "socket closed");
      continue;
    }
    const uint32_t timeoutMs = client.sessionId
        ? kClientSessionTimeoutMs : kClientIdleTimeoutMs;
    if (now - client.lastSeenMs >= timeoutMs) {
      dropClient(client, "heartbeat timeout");
      continue;
    }
    if (client.transport == ClientTransport::LocalTcp && !readClient(client))
      dropClient(client, "invalid frame");
  }
}

void loadConfiguration() {
  accessCode = PokegochiOnlineRelayConfig::kAccessCode;
  if (preferences.begin("pokegochi-on", false))
    botBalancedWins = preferences.getULong("bot-bal-wins", 0U);
}

void beginStation() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(PokegochiServerLocalConfig::kWifiSsid,
             PokegochiServerLocalConfig::kWifiPassword);
  lastWifiAttemptMs = millis();
  wifiUnavailableSinceMs = lastWifiAttemptMs;
  Serial.printf("[WIFI] connecting ssid=%s\n",
                PokegochiServerLocalConfig::kWifiSsid);
}

void stopRelay() {
  for (ClientSlot& client : clients) dropClient(client, "Wi-Fi unavailable");
  cloudMqtt.disconnect();
  if (relayListening) relayServer.end();
  if (discoveryAdvertising) MDNS.end();
  relayListening = false;
  discoveryAdvertising = false;
}

void startDiscoveryAdvertisement() {
  if (discoveryAdvertising) return;
  if (!MDNS.begin(kMdnsHostName)) {
    Serial.println("[SERVER] mDNS advertisement failed");
    return;
  }
  char code[7]{};
  char gameProtocol[4]{};
  std::snprintf(code, sizeof(code), "%06lu",
                static_cast<unsigned long>(accessCode));
  std::snprintf(gameProtocol, sizeof(gameProtocol), "%u",
                static_cast<unsigned>(PokegochiMultiplayerProtocol::kVersion));
  MDNS.setInstanceName("Pokegochi Online Server");
  MDNS.addService(kMdnsServiceName, kMdnsTransport, kDefaultPort);
  MDNS.addServiceTxt(kMdnsServiceName, kMdnsTransport,
                     kMdnsAccessCodeKey, code);
  MDNS.addServiceTxt(kMdnsServiceName, kMdnsTransport,
                     kMdnsGameProtocolKey, gameProtocol);
  discoveryAdvertising = true;
  Serial.printf("[SERVER] automatic discovery ready as %s.local\n",
                kMdnsHostName);
}

void serviceNetwork() {
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    stopRelay();
    if (!wifiUnavailableSinceMs) wifiUnavailableSinceMs = now;
    if (now - lastWifiAttemptMs >= kWifiRetryMs) {
      lastWifiAttemptMs = now;
      WiFi.disconnect(false, false);
      WiFi.begin(PokegochiServerLocalConfig::kWifiSsid,
                 PokegochiServerLocalConfig::kWifiPassword);
      Serial.println("[WIFI] reconnecting");
    }
    if (now - wifiUnavailableSinceMs >= kWifiRestartMs) ESP.restart();
    return;
  }
  wifiUnavailableSinceMs = 0;
  if (!relayListening) {
    relayServer.begin();
    relayServer.setNoDelay(true);
    relayListening = true;
    startDiscoveryAdvertisement();
    Serial.printf("[SERVER] listening tcp=%u ip=%s access=%06lu\n",
                  static_cast<unsigned>(kDefaultPort),
                  WiFi.localIP().toString().c_str(),
                  static_cast<unsigned long>(accessCode));
  }
  if (!discoveryAdvertising) startDiscoveryAdvertisement();
  // Consume LAN frames first. OpenSession can create a room in this same loop,
  // allowing serviceCloudTransport() to defer its blocking reconnect before
  // it can starve that newly-active socket.
  serviceClients();
  serviceCloudTransport(now);
  if (now - lastStatusMs >= 30000U) {
    lastStatusMs = now;
    Serial.printf("[SERVER] clients=%u sessions=%u heap=%u rssi=%d\n",
                  static_cast<unsigned>(authenticatedCount()),
                  static_cast<unsigned>(sessionCount()),
                  static_cast<unsigned>(ESP.getFreeHeap()), WiFi.RSSI());
  }
}

void serviceSerial() {
  if (!Serial.available()) return;
  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toUpperCase();
  if (command == "RESET_CODE") {
    Serial.println("[SERVER] release access code is fixed by build");
  } else if (command == "STATUS") {
    Serial.printf("[SERVER] ip=%s port=%u access=%06lu clients=%u sessions=%u "
                  "bot=%05lu bot-balance-wins=%lu bot-busy=%u\n",
                  WiFi.localIP().toString().c_str(),
                  static_cast<unsigned>(kDefaultPort),
                  static_cast<unsigned long>(accessCode),
                  static_cast<unsigned>(authenticatedCount()),
                  static_cast<unsigned>(sessionCount()),
                  static_cast<unsigned long>(PokegochiServerBot::kPlayerId),
                  static_cast<unsigned long>(botBalancedWins),
                  battleBot.busy ? 1U : 0U);
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nPokegochi Online Server v3");
  loadConfiguration();
  Serial.printf("[BOT] online id=%05lu balanced-wins=%lu roster=%u\n",
                static_cast<unsigned long>(PokegochiServerBot::kPlayerId),
                static_cast<unsigned long>(botBalancedWins),
                static_cast<unsigned>(PokegochiServerBot::rosterSize()));
  std::snprintf(cloudServerTopic, sizeof(cloudServerTopic), "%s/server/#",
                PokegochiOnlineRelayConfig::kTopicRoot);
  cloudMqtt.setServer(PokegochiOnlineRelayConfig::kBrokerHost,
                      PokegochiOnlineRelayConfig::kBrokerPort);
  cloudMqtt.setBufferSize(PokegochiOnlineRelayConfig::kMqttBufferBytes);
  cloudMqtt.setKeepAlive(PokegochiOnlineRelayConfig::kMqttKeepAliveSeconds);
  cloudMqtt.setSocketTimeout(5);
  cloudMqtt.setCallback(receiveCloudFrame);
  beginStation();
}

void loop() {
  serviceSerial();
  serviceNetwork();
  delay(2);
}
