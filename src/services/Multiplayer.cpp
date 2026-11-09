#include "services/Multiplayer.h"
#include "services/MultiplayerProtocol.h"
#include "services/OnlineMultiplayerProtocol.h"
#include "config/OnlineRelayConfig.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <Network.h>
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <esp32-hal-bt.h>
#include <esp_event.h>
#include <mdns.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <lwip/ip4_addr.h>
#include <lwip/sockets.h>
#elif defined(POKEGOCHI_APP_BUILD)
#include <Preferences.h>
#include <chrono>
#endif

#if defined(ARDUINO_ARCH_ESP32)
// The generic Arduino profile is sized for sustained high-throughput traffic:
// WiFiGeneric can request 32 dynamic RX and 32 dynamic TX buffers, AMPDU and
// a 32-entry management pool. Pokegochi has one low-rate TCP connection and a
// 54 KiB save that must coexist with it. Interpose at the IDF boundary so both
// the native scanner and every Arduino reconnect use the same bounded profile,
// without modifying or rebuilding the installed framework package.
extern "C" esp_err_t __real_esp_wifi_init(
    const wifi_init_config_t* config);
extern "C" esp_err_t __wrap_esp_wifi_init(
    const wifi_init_config_t* config) {
  if (!config) return __real_esp_wifi_init(config);
  wifi_init_config_t bounded = *config;
  bounded.static_rx_buf_num = std::min(bounded.static_rx_buf_num, 4);
  bounded.dynamic_rx_buf_num = std::min(bounded.dynamic_rx_buf_num, 8);
  bounded.dynamic_tx_buf_num = std::min(bounded.dynamic_tx_buf_num, 8);
  bounded.rx_mgmt_buf_num = std::min(bounded.rx_mgmt_buf_num, 3);
  // Six is the ESP-IDF supported minimum. This client owns one low-rate
  // station socket, so the generic eight-buffer management pool only steals
  // RAM from the 54 KiB durable save without improving link reliability.
  bounded.mgmt_sbuf_num = std::min(bounded.mgmt_sbuf_num, 6);
  // Credentials and MRU order are persisted by Multiplayer itself. Keeping a
  // second copy in the driver's NVS cache allocates another runtime context
  // while the save is being restored, exactly at the tightest point of Wi-Fi
  // startup. ESP-NOW is not part of either multiplayer transport.
  bounded.nvs_enable = 0;
  bounded.espnow_max_encrypt_num = 0;
  bounded.sta_disconnected_pm = false;
  bounded.csi_enable = 0;
  bounded.ampdu_rx_enable = 0;
  bounded.ampdu_tx_enable = 0;
  bounded.rx_ba_win = 0;
  return __real_esp_wifi_init(&bounded);
}
#endif

namespace {
// Increment whenever a packed packet changes. Devices with an older beta are
// deliberately hidden from discovery instead of exchanging incompatible
// roster/state records.
constexpr uint8_t kProtocolVersion = PokegochiMultiplayerProtocol::kVersion;
constexpr uint16_t kPacketMagic = PokegochiMultiplayerProtocol::kPacketMagic;
constexpr const char* kServiceUuid = PokegochiMultiplayerProtocol::kServiceUuid;
constexpr const char* kRxUuid = PokegochiMultiplayerProtocol::kRxUuid;
constexpr const char* kTxUuid = PokegochiMultiplayerProtocol::kTxUuid;
constexpr uint32_t kOnlinePingIntervalMs = 10000U;
constexpr uint32_t kOnlineIdleTimeoutMs = 30000U;
// A battle can legitimately spend more than thirty seconds inside authored
// move text, animation and a forced replacement picker. Keep the room alive
// while that deterministic state is still in progress; idle discovery keeps
// the shorter timeout so a dead server is still reported promptly.
constexpr uint32_t kOnlineSessionTimeoutMs = 120000U;
constexpr uint32_t kWifiCandidateTimeoutMs = 8000U;
constexpr char kWifiMruKey[] = "wifi-mru";
constexpr char kWifiCountKey[] = "wifi-count";

enum PacketType : uint8_t {
  Invite = 1,
  InviteAccept,
  InviteReject,
  SessionClose,
  ActivityRequest,
  ActivityAccept,
  ActivityReject,
  TradeOfferPacket,
  TradeConfirm,
  TradeReject,
  TradeCommitRequest,
  TradeCommitApplied,
  BattlePartyMember,
  BattlePartyReady,
  BattleLead,
  BattleStart,
  ActivityCancel,
  BattleCommandPacket,
  BattleSyncPokemon,
  BattleSyncVolatile,
  BattleSyncCore,
  BattleSyncAck,
  BattleRulesChoice,
};

static_assert(Invite == PokegochiMultiplayerProtocol::Invite &&
              InviteAccept == PokegochiMultiplayerProtocol::InviteAccept &&
              InviteReject == PokegochiMultiplayerProtocol::InviteReject &&
              SessionClose == PokegochiMultiplayerProtocol::SessionClose &&
              ActivityRequest == PokegochiMultiplayerProtocol::ActivityRequest &&
              ActivityAccept == PokegochiMultiplayerProtocol::ActivityAccept &&
              ActivityReject == PokegochiMultiplayerProtocol::ActivityReject &&
              TradeOfferPacket == PokegochiMultiplayerProtocol::TradeOffer &&
              TradeConfirm == PokegochiMultiplayerProtocol::TradeConfirm &&
              TradeReject == PokegochiMultiplayerProtocol::TradeReject &&
              TradeCommitRequest == PokegochiMultiplayerProtocol::TradeCommitRequest &&
              TradeCommitApplied == PokegochiMultiplayerProtocol::TradeCommitApplied &&
              BattlePartyMember == PokegochiMultiplayerProtocol::BattlePartyMember &&
              BattlePartyReady == PokegochiMultiplayerProtocol::BattlePartyReady &&
              BattleLead == PokegochiMultiplayerProtocol::BattleLead &&
              BattleStart == PokegochiMultiplayerProtocol::BattleStart &&
              ActivityCancel == PokegochiMultiplayerProtocol::ActivityCancel &&
              BattleCommandPacket == PokegochiMultiplayerProtocol::BattleCommand &&
              BattleSyncPokemon == PokegochiMultiplayerProtocol::BattleSyncPokemon &&
              BattleSyncVolatile == PokegochiMultiplayerProtocol::BattleSyncVolatile &&
              BattleSyncCore == PokegochiMultiplayerProtocol::BattleSyncCore &&
              BattleSyncAck == PokegochiMultiplayerProtocol::BattleSyncAck &&
              BattleRulesChoice == PokegochiMultiplayerProtocol::BattleRulesChoice,
              "Multiplayer packet type ABI changed");

#pragma pack(push, 1)
struct PacketHeader {
  uint16_t magic;
  uint8_t version;
  uint8_t type;
  uint32_t sourceId;
  uint32_t transaction;
  uint16_t payloadLength;
};
struct ActivityPayload { uint8_t activity; uint8_t rules; };
struct TradePayload { OwnedPokemon pokemon; };
struct BattlePartyPayload { uint8_t slot; OwnedPokemon pokemon; };
struct BattleReadyPayload { uint8_t count; };
struct BattleLeadPayload { uint8_t slot; };
struct BattleStartPayload { uint32_t seed; uint8_t hostLeadSlot; uint8_t guestLeadSlot; };
struct BattleCommandPayload { uint16_t turn; uint8_t kind; uint8_t value; };
struct BattleSyncPokemonPayload { uint16_t turn; uint8_t side; uint8_t slot; OwnedPokemon pokemon; };
struct BattleSyncVolatilePayload {
  uint16_t turn;
  uint8_t side;
  uint8_t slot;
  CombatVolatile state;
  DedicatedMoveEffectState moveEffects;
  Gen4BattlerEffectState gen4Effects;
};
struct BattleSyncCorePayload {
  uint16_t turn;
  uint32_t snapshotChecksum;
  uint32_t rngState;
  uint8_t active;
  uint8_t outcome;
  uint8_t weather;
  uint8_t weatherTurns;
  uint8_t hostSafeguardTurns;
  uint8_t guestSafeguardTurns;
  uint8_t hostReflectTurns;
  uint8_t guestReflectTurns;
  uint8_t hostLightScreenTurns;
  uint8_t guestLightScreenTurns;
  uint8_t hostMistTurns;
  uint8_t guestMistTurns;
  uint8_t hostProtectChain;
  uint8_t guestProtectChain;
  uint8_t hostEndureThisTurn;
  uint8_t guestEndureThisTurn;
  uint8_t hostNightmare;
  uint8_t guestNightmare;
  uint8_t hostZMoveUsed;
  uint8_t guestZMoveUsed;
  uint8_t hostSpikesLayers;
  uint8_t guestSpikesLayers;
  uint8_t hostToxicSpikesLayers;
  uint8_t guestToxicSpikesLayers;
  uint8_t hostStealthRock;
  uint8_t guestStealthRock;
  uint8_t hostTailwindTurns;
  uint8_t guestTailwindTurns;
  uint8_t hostLuckyChantTurns;
  uint8_t guestLuckyChantTurns;
  uint8_t gravityTurns;
  uint8_t trickRoomTurns;
  MoveId lastMoveUsedGlobally;
  uint8_t hostCount;
  uint8_t guestCount;
  uint8_t hostActiveRosterSlot;
  uint8_t guestActiveRosterSlot;
  DelayedAttackState delayedToHost;
  DelayedAttackState delayedToGuest;
};
struct BattleSyncAckPayload {
  uint16_t turn;
  uint32_t snapshotChecksum;
};
#pragma pack(pop)

uint32_t battleSyncChecksumUpdate(uint32_t checksum, const void* data,
                                  size_t length) {
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  for (size_t index = 0; index < length; ++index) {
    checksum ^= bytes[index];
    checksum *= 16777619UL;
  }
  return checksum;
}

BattleSyncCorePayload battleSyncCorePayload(
    const Multiplayer::BattleStateSync& sync, uint32_t snapshotChecksum) {
  return BattleSyncCorePayload{
      sync.turn, snapshotChecksum, sync.rngState,
      static_cast<uint8_t>(sync.active ? 1U : 0U),
      static_cast<uint8_t>(sync.hostOutcome),
      static_cast<uint8_t>(sync.weather), sync.weatherTurns,
      sync.hostSafeguardTurns, sync.guestSafeguardTurns,
      sync.hostReflectTurns, sync.guestReflectTurns,
      sync.hostLightScreenTurns, sync.guestLightScreenTurns,
      sync.hostMistTurns, sync.guestMistTurns,
      sync.hostProtectChain, sync.guestProtectChain,
      static_cast<uint8_t>(sync.hostEndureThisTurn),
      static_cast<uint8_t>(sync.guestEndureThisTurn),
      static_cast<uint8_t>(sync.hostNightmare),
      static_cast<uint8_t>(sync.guestNightmare),
      static_cast<uint8_t>(sync.hostZMoveUsed),
      static_cast<uint8_t>(sync.guestZMoveUsed),
      sync.hostSpikesLayers, sync.guestSpikesLayers,
      sync.hostToxicSpikesLayers, sync.guestToxicSpikesLayers,
      static_cast<uint8_t>(sync.hostStealthRock),
      static_cast<uint8_t>(sync.guestStealthRock),
      sync.hostTailwindTurns, sync.guestTailwindTurns,
      sync.hostLuckyChantTurns, sync.guestLuckyChantTurns,
      sync.gravityTurns, sync.trickRoomTurns, sync.lastMoveUsedGlobally,
      sync.hostCount, sync.guestCount, sync.hostActiveRosterSlot,
      sync.guestActiveRosterSlot, sync.delayedToHost, sync.delayedToGuest};
}

// Hash the exact packed payload sequence used on the wire. Reconstructing the
// same sequence on the receiver avoids architecture-dependent padding in the
// larger BattleStateSync object (ESP32 and Android do not share an ABI).
uint32_t battleStateSyncChecksum(const Multiplayer::BattleStateSync& sync) {
  uint32_t checksum = 2166136261UL;
  for (uint8_t slot = 0; slot < sync.hostCount; ++slot) {
    const BattleSyncPokemonPayload packet{
        sync.turn, 0U, slot, sync.hostParty[slot]};
    checksum = battleSyncChecksumUpdate(checksum, &packet, sizeof(packet));
  }
  for (uint8_t slot = 0; slot < sync.guestCount; ++slot) {
    const BattleSyncPokemonPayload packet{
        sync.turn, 1U, slot, sync.guestParty[slot]};
    checksum = battleSyncChecksumUpdate(checksum, &packet, sizeof(packet));
  }
  for (uint8_t slot = 0; slot < sync.hostCount; ++slot) {
    const BattleSyncVolatilePayload packet{
        sync.turn, 0U, slot, sync.hostVolatiles[slot],
        sync.hostMoveEffects[slot], sync.hostGen4Effects[slot]};
    checksum = battleSyncChecksumUpdate(checksum, &packet, sizeof(packet));
  }
  for (uint8_t slot = 0; slot < sync.guestCount; ++slot) {
    const BattleSyncVolatilePayload packet{
        sync.turn, 1U, slot, sync.guestVolatiles[slot],
        sync.guestMoveEffects[slot], sync.guestGen4Effects[slot]};
    checksum = battleSyncChecksumUpdate(checksum, &packet, sizeof(packet));
  }
  const BattleSyncCorePayload core = battleSyncCorePayload(sync, 0U);
  checksum = battleSyncChecksumUpdate(checksum, &core, sizeof(core));
  return checksum ? checksum : 1U;
}

uint32_t tradeTransaction(uint32_t firstPlayer, uint32_t secondPlayer,
                          const OwnedPokemon& firstPokemon, const OwnedPokemon& secondPokemon) {
  const OwnedPokemon* lowPokemon = &firstPokemon;
  const OwnedPokemon* highPokemon = &secondPokemon;
  if (firstPlayer > secondPlayer) std::swap(lowPokemon, highPokemon);
  uint32_t value = firstPlayer ^ secondPlayer ^ lowPokemon->personality ^ highPokemon->personality;
  value ^= static_cast<uint32_t>(lowPokemon->speciesId) << 16U;
  value ^= static_cast<uint32_t>(highPokemon->speciesId);
  value ^= value >> 16U; value *= 0x7FEB352DU; value ^= value >> 15U;
  return value ? value : 1U;
}

static_assert(sizeof(PacketHeader) == 14, "Multiplayer packet ABI changed");
static_assert(sizeof(BattleSyncPokemonPayload) <= 160, "Pokemon sync exceeds BLE packet budget");
static_assert(sizeof(BattleSyncVolatilePayload) <= 160, "Volatile sync exceeds BLE packet budget");
Multiplayer* gMultiplayer = nullptr;

#if defined(ARDUINO_ARCH_ESP32)
NimBLEServer* gServer = nullptr;
NimBLECharacteristic* gServerTx = nullptr;
NimBLEClient* gClient = nullptr;
NimBLERemoteCharacteristic* gRemoteRx = nullptr;
bool gOutboundPhysicalLink = false;
bool gOutboundConnectPending = false;
uint16_t gInboundConnectionHandle = BLE_HS_CONN_HANDLE_NONE;
enum class TransportEventKind : uint8_t { Discovered, Connected, Disconnected, Packet };
struct TransportEvent {
  TransportEventKind kind = TransportEventKind::Disconnected;
  uint32_t id = 0;
  int8_t rssi = -127;
  uint8_t addressType = 0;
  bool outbound = false;
  char address[20]{};
  uint16_t length = 0;
  uint8_t bytes[180]{};
};
QueueHandle_t gTransportQueue = nullptr;
uint32_t gLastAdvertisementLogMs = 0;
portMUX_TYPE gDiscoveryMux = portMUX_INITIALIZER_UNLOCKED;
TransportEvent gLatestDiscovery{};
bool gDiscoveryPending = false;
uint32_t gCriticalTransportDrops = 0;
WiFiClient gOnlineClient;
PubSubClient gOnlineMqtt(gOnlineClient);
bool gOnlineAuthenticated = false;
bool gOnlineSessionReady = false;
bool gOnlineCloudMode = false;
bool gOnlineCloudFramePending = false;
uint32_t gOnlineSessionId = 0;
uint32_t gOnlinePeerId = 0;
uint32_t gOnlineLastRxMs = 0;
uint32_t gOnlineLastPingMs = 0;
uint32_t gOnlineLastConnectAttemptMs = 0;
PokegochiOnlineProtocol::FrameHeader gOnlineIncomingHeader{};
uint16_t gOnlineHeaderBytes = 0;
uint16_t gOnlinePayloadBytes = 0;
uint8_t gOnlinePayload[PokegochiOnlineProtocol::kMaximumGamePacket]{};
constexpr size_t kOnlineMaximumFrameLength =
    sizeof(PokegochiOnlineProtocol::FrameHeader) +
    PokegochiOnlineProtocol::kMaximumGamePacket;
// Keep exactly one unsent/partially-sent relay frame. A full queue would steal
// the internal RAM that lwIP needs for pbufs on the no-PSRAM board; one frame
// is enough to make invitations, switch commands and ACKs survive temporary
// EAGAIN without ever interleaving bytes from two TCP frames.
uint8_t gOnlinePendingFrame[kOnlineMaximumFrameLength]{};
uint16_t gOnlinePendingLength = 0;
uint16_t gOnlinePendingOffset = 0;
uint32_t gOnlineCloudNonce = 0;
char gOnlineCloudPublishTopic[96]{};
char gOnlineCloudSubscribeTopic[96]{};

void receiveOnlineCloudFrame(char*, uint8_t* bytes, unsigned int length) {
  if (!bytes || gOnlineCloudFramePending ||
      length < sizeof(PokegochiOnlineProtocol::FrameHeader) ||
      length > sizeof(PokegochiOnlineProtocol::FrameHeader) +
                   PokegochiOnlineProtocol::kMaximumGamePacket) return;
  PokegochiOnlineProtocol::FrameHeader header{};
  std::memcpy(&header, bytes, sizeof(header));
  if (!PokegochiOnlineProtocol::validHeader(header) ||
      sizeof(header) + header.payloadLength != length) return;
  gOnlineIncomingHeader = header;
  gOnlineHeaderBytes = sizeof(header);
  gOnlinePayloadBytes = header.payloadLength;
  if (header.payloadLength)
    std::memcpy(gOnlinePayload, bytes + sizeof(header), header.payloadLength);
  gOnlineCloudFramePending = true;
}

bool flushPendingOnlineFrame() {
  if (!gOnlinePendingLength) return true;
  if (!gOnlineClient.connected() || gOnlineClient.fd() < 0) return false;
  const size_t remaining = static_cast<size_t>(gOnlinePendingLength -
                                                gOnlinePendingOffset);
  const int sent = lwip_send(
      gOnlineClient.fd(), gOnlinePendingFrame + gOnlinePendingOffset,
      remaining, MSG_DONTWAIT);
  if (sent > 0) {
    gOnlinePendingOffset = static_cast<uint16_t>(gOnlinePendingOffset + sent);
    if (gOnlinePendingOffset == gOnlinePendingLength) {
      Serial.printf("[WIFI] deferred frame delivered bytes=%u\n",
                    static_cast<unsigned>(gOnlinePendingLength));
      gOnlinePendingLength = gOnlinePendingOffset = 0;
      return true;
    }
    return false;
  }
  if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    Serial.printf("[WIFI] deferred frame failed errno=%d; resetting stream\n",
                  errno);
    gOnlinePendingLength = gOnlinePendingOffset = 0;
    gOnlineClient.stop();
  }
  return false;
}

bool sendOnlineFrame(PokegochiOnlineProtocol::MessageType type,
                     uint32_t sourceId, uint32_t targetId = 0,
                     uint32_t sessionId = 0, const void* payload = nullptr,
                     uint16_t payloadLength = 0) {
  if (!gOnlineClient.connected() ||
      payloadLength > PokegochiOnlineProtocol::kMaximumGamePacket) return false;
  const PokegochiOnlineProtocol::FrameHeader header =
      PokegochiOnlineProtocol::makeHeader(type, payloadLength, sourceId,
                                          targetId, sessionId);
  const size_t frameLength = sizeof(header) + payloadLength;
  // Stage one complete sub-MSS relay frame on the roomy loop-task stack.
  // Keeping this scratch buffer off .bss preserves scarce internal DRAM for
  // lwIP pbufs while Wi-Fi, the save and the retained renderer coexist.
  uint8_t onlineTxFrame[kOnlineMaximumFrameLength]{};
  std::memcpy(onlineTxFrame, &header, sizeof(header));
  if (payloadLength && payload)
    std::memcpy(onlineTxFrame + sizeof(header), payload, payloadLength);
  if (gOnlineCloudMode) {
    if (!gOnlineMqtt.connected()) return false;
    return gOnlineMqtt.publish(
        gOnlineCloudPublishTopic, onlineTxFrame,
        static_cast<unsigned int>(frameLength), false);
  }
  // Never call NetworkClient::write() here: its ten one-second EAGAIN retries
  // freeze the UI exactly while a faint/switch is synchronizing. Use one
  // non-blocking send and retain the remainder as the sole stream owner.
  if (gOnlinePendingLength) return false;
  const int sent = lwip_send(gOnlineClient.fd(), onlineTxFrame, frameLength,
                             MSG_DONTWAIT);
  if (sent == static_cast<int>(frameLength)) return true;
  if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    Serial.printf("[WIFI] relay frame failed errno=%d; resetting stream\n",
                  errno);
    gOnlineClient.stop();
    return false;
  }
  const size_t accepted = sent > 0 ? static_cast<size_t>(sent) : 0U;
  const bool heartbeat =
      type == PokegochiOnlineProtocol::MessageType::Ping ||
      type == PokegochiOnlineProtocol::MessageType::Pong;
  // A heartbeat with no accepted bytes is disposable. Critical frames are
  // copied whole and resume at the exact byte offset accepted by lwIP.
  if (heartbeat && accepted == 0U) return false;
  std::memcpy(gOnlinePendingFrame, onlineTxFrame, frameLength);
  gOnlinePendingLength = static_cast<uint16_t>(frameLength);
  gOnlinePendingOffset = static_cast<uint16_t>(accepted);
  Serial.printf("[WIFI] relay backpressure queued type=%u offset=%u/%u\n",
                static_cast<unsigned>(type),
                static_cast<unsigned>(gOnlinePendingOffset),
                static_cast<unsigned>(gOnlinePendingLength));
  return true;
}

void resetOnlineTransportState(bool closeSocket) {
  if (closeSocket) {
    if (gOnlineMqtt.connected()) gOnlineMqtt.disconnect();
    gOnlineClient.stop();
  }
  gOnlineAuthenticated = false;
  gOnlineSessionReady = false;
  gOnlineSessionId = 0;
  gOnlinePeerId = 0;
  gOnlineHeaderBytes = 0;
  gOnlinePayloadBytes = 0;
  gOnlineCloudFramePending = false;
  gOnlinePendingLength = gOnlinePendingOffset = 0;
  gOnlineIncomingHeader = {};
  gOnlineLastRxMs = gOnlineLastPingMs = 0;
  gOnlineLastConnectAttemptMs = 0;
}

void releaseArduinoNetworkRuntime() {
  // WiFi.mode(WIFI_OFF) deinitializes the driver and its netifs, but Arduino's
  // global Network object intentionally keeps a 4 KiB event task, queue,
  // mutex and the ESP default event loop alive forever. On this no-PSRAM
  // board those first-use allocations are made between GameSave and the scene
  // arena. They survive shutdown and split the only block large enough for
  // the full menu renderer even though aggregate free RAM is ample.
  //
  // There are no callbacks or interfaces left after WiFiGeneric::mode(OFF),
  // so end the event runtime and reconstruct the empty global object in place.
  // The next Wi-Fi start calls Network.begin() normally and recreates it while
  // GameSave/renderer are parked. esp-netif itself is deliberately reserved
  // before the renderer at boot because this ESP-IDF target cannot deinit it.
  Network.~NetworkManager();
  ::new (static_cast<void*>(&Network)) NetworkManager();
  const esp_err_t loopResult = esp_event_loop_delete_default();
  Serial.printf("[WIFI] network runtime released loop=%s heap=%u block=%u\n",
                esp_err_to_name(loopResult),
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()));
}

// Arduino's WiFi.scanNetworks() forwards every driver event through a second
// heap-allocated NetworkEvents queue. On the no-PSRAM board the scan's own
// peak can leave too little internal RAM for that copied event and libstdc++
// aborts from operator new. A short-lived native scan has no Arduino event
// bridge, returns the same strongest-first records and deinitializes the
// driver before the renderer or TCP stack is restored.
uint8_t scanPhysicalWifiNetworks(char names[][33], int8_t* rssi,
                                 uint8_t capacity) {
  if (!names || !rssi || !capacity) return 0;
  const uint8_t limit = std::min<uint8_t>(capacity, 6U);
  std::memset(names, 0, static_cast<size_t>(capacity) * 33U);
  std::memset(rssi, 0, capacity);
  bool defaultLoopCreated = false;
  bool wifiInitialized = false;
  bool wifiStarted = false;
  uint8_t kept = 0;
  const esp_err_t netifResult = esp_netif_init();
  if (netifResult != ESP_OK && netifResult != ESP_ERR_INVALID_STATE) {
    Serial.printf("[WIFI] native scan netif failed: %s\n",
                  esp_err_to_name(netifResult));
    return 0;
  }
  const esp_err_t loopResult = esp_event_loop_create_default();
  if (loopResult == ESP_OK) defaultLoopCreated = true;
  else if (loopResult != ESP_ERR_INVALID_STATE) {
    Serial.printf("[WIFI] native scan event loop failed: %s\n",
                  esp_err_to_name(loopResult));
    return 0;
  }
  wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  // The scan has no data socket and needs only a few management frames at a
  // time. Match the production low-throughput profile explicitly so this
  // temporary driver cannot recreate the large generic Arduino buffer pool.
  init.static_rx_buf_num = 4;
  init.dynamic_rx_buf_num = 8;
  init.dynamic_tx_buf_num = 8;
  init.rx_mgmt_buf_num = 3;
  init.mgmt_sbuf_num = 6;
  init.nvs_enable = 0;
  init.espnow_max_encrypt_num = 0;
  init.sta_disconnected_pm = false;
  init.csi_enable = 0;
  init.ampdu_rx_enable = 0;
  init.ampdu_tx_enable = 0;
  init.rx_ba_win = 0;
  esp_err_t result = esp_wifi_init(&init);
  if (result == ESP_OK) wifiInitialized = true;
  if (result == ESP_OK) result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if (result == ESP_OK) result = esp_wifi_set_mode(WIFI_MODE_STA);
  if (result == ESP_OK) {
    result = esp_wifi_start();
    wifiStarted = result == ESP_OK;
  }
  wifi_scan_config_t config{};
  config.show_hidden = true;
  config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  if (result == ESP_OK) result = esp_wifi_scan_start(&config, true);
  wifi_ap_record_t records[6]{};
  uint16_t found = limit;
  if (result == ESP_OK) result = esp_wifi_scan_get_ap_records(&found, records);
  if (result == ESP_OK) {
    for (uint16_t index = 0; index < found && kept < limit; ++index) {
      const char* name = reinterpret_cast<const char*>(records[index].ssid);
      if (!name[0]) continue;
      bool duplicate = false;
      for (uint8_t existing = 0; existing < kept; ++existing)
        duplicate = duplicate || std::strncmp(names[existing], name, 32U) == 0;
      if (duplicate) continue;
      std::strncpy(names[kept], name, 32U);
      names[kept][32] = 0;
      rssi[kept] = static_cast<int8_t>(
          std::max<int32_t>(-127, std::min<int32_t>(0, records[index].rssi)));
      ++kept;
    }
  } else {
    Serial.printf("[WIFI] native scan failed: %s\n", esp_err_to_name(result));
  }
  if (wifiStarted) esp_wifi_stop();
  if (wifiInitialized) esp_wifi_deinit();
  if (defaultLoopCreated) esp_event_loop_delete_default();
  // Let the IDF event task release final STOP records before its loop is
  // reused by Arduino WiFi for a real station connection.
  delay(100);
  return kept;
}

bool discoverOnlineRelay(uint32_t playerId, char* host, size_t hostCapacity,
                         uint16_t& port, char* accessCode,
                         size_t accessCodeCapacity) {
  if (!host || hostCapacity < 8U || !accessCode || accessCodeCapacity < 7U)
    return false;
  host[0] = 0;
  accessCode[0] = 0;
  char clientName[32]{};
  std::snprintf(clientName, sizeof(clientName), "pokegochi-%05lu",
                static_cast<unsigned long>(playerId));
  if (mdns_init() != ESP_OK) {
    Serial.println("[WIFI] mDNS client could not start");
    return false;
  }
  if (mdns_hostname_set(clientName) != ESP_OK) {
    mdns_free();
    Serial.println("[WIFI] mDNS hostname could not be set");
    return false;
  }
  mdns_result_t* results = nullptr;
  // The native IDF API expects DNS-SD labels exactly as they appear on the
  // wire. Arduino's MDNS.queryService() used to add these underscores for us;
  // keeping the protocol constants human-readable therefore requires doing
  // the same conversion explicitly here.
  char serviceLabel[24]{};
  char transportLabel[12]{};
  std::snprintf(serviceLabel, sizeof(serviceLabel), "_%s",
                PokegochiOnlineProtocol::kMdnsServiceName);
  std::snprintf(transportLabel, sizeof(transportLabel), "_%s",
                PokegochiOnlineProtocol::kMdnsTransport);
  const esp_err_t queryResult = mdns_query_ptr(
      serviceLabel, transportLabel, 3000U, 4U, &results);
  bool resolved = false;
  uint32_t selectedAddressRank = UINT32_MAX;
  for (mdns_result_t* result = queryResult == ESP_OK ? results : nullptr;
       result; result = result->next) {
    const esp_ip4_addr_t* address = nullptr;
    for (mdns_ip_addr_t* item = result->addr; item && !address;
         item = item->next) {
      if (item->addr.type == ESP_IPADDR_TYPE_V4)
        address = &item->addr.u_addr.ip4;
    }
    const uint8_t* code = nullptr;
    size_t codeLength = 0U;
    for (size_t index = 0; index < result->txt_count; ++index) {
      if (!result->txt[index].key ||
          std::strcmp(result->txt[index].key,
                      PokegochiOnlineProtocol::kMdnsAccessCodeKey) != 0)
        continue;
      code = reinterpret_cast<const uint8_t*>(result->txt[index].value);
      codeLength = result->txt_value_len
          ? result->txt_value_len[index]
          : (code ? std::strlen(reinterpret_cast<const char*>(code)) : 0U);
      break;
    }
    if (!address || !result->port || !code || codeLength != 6U) continue;
    bool numeric = true;
    for (size_t digit = 0; digit < 6U; ++digit)
      numeric = numeric && code[digit] >= '0' && code[digit] <= '9';
    if (!numeric) continue;
    char candidateHost[16]{};
    if (!esp_ip4addr_ntoa(address, candidateHost, sizeof(candidateHost)))
      continue;
    // PTR answer order is explicitly unspecified. Selecting the first answer
    // made the same device alternate between two valid relays after reconnect,
    // which split peers across rooms and looked like a dead second battle.
    // Rank IPv4 addresses numerically so every client with the same answer set
    // deterministically joins the same local relay.
    const uint32_t candidateRank = lwip_ntohl(address->addr);
    if (resolved && candidateRank >= selectedAddressRank) continue;
    std::strncpy(host, candidateHost, hostCapacity - 1U);
    host[hostCapacity - 1U] = 0;
    std::memcpy(accessCode, code, 6U);
    accessCode[6] = 0;
    port = result->port;
    selectedAddressRank = candidateRank;
    resolved = true;
  }
  // ESPmDNS' Arduino wrapper keeps its last result list after end().  Free
  // the IDF query explicitly before restoring the 54 KiB save, then stop the
  // task and all discovery-only allocations.
  if (results) mdns_query_results_free(results);
  mdns_free();
  Serial.printf("[WIFI] discovery cleanup heap=%u block=%u\n",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()));
  if (resolved)
    Serial.printf("[WIFI] relay discovered automatically host=%s port=%u\n",
                  host, static_cast<unsigned>(port));
  else
    Serial.println("[WIFI] automatic relay discovery found no server");
  return resolved;
}

void queueTransportEvent(const TransportEvent& event) {
  // Advertisements can arrive dozens of times per second. They are only a
  // latest-value observation and must never occupy the queue used by link,
  // packet and disconnect events.
  if (event.kind == TransportEventKind::Discovered) {
    portENTER_CRITICAL(&gDiscoveryMux);
    gLatestDiscovery = event;
    gDiscoveryPending = true;
    portEXIT_CRITICAL(&gDiscoveryMux);
    return;
  }
  if (gTransportQueue && xQueueSend(gTransportQueue, &event, 0) == pdTRUE) return;
  portENTER_CRITICAL(&gDiscoveryMux);
  ++gCriticalTransportDrops;
  portEXIT_CRITICAL(&gDiscoveryMux);
}

class ServerCallbacks final : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
    if (gOutboundConnectPending || gOutboundPhysicalLink) {
      Serial.printf("[BLE] rejecting competing inbound link handle=%u\n",
                    static_cast<unsigned>(info.getConnHandle()));
      if (gServer) gServer->disconnect(info.getConnHandle());
      return;
    }
    if (gServer) gServer->updateConnParams(info.getConnHandle(), 12, 24, 0, 240);
    if (NimBLEDevice::getScan()) NimBLEDevice::getScan()->stop();
    gOutboundPhysicalLink = false;
    gInboundConnectionHandle = info.getConnHandle();
    TransportEvent event{}; event.kind = TransportEventKind::Connected; event.outbound = false;
    Serial.printf("[BLE] inbound physical link connected handle=%u\n",
                  static_cast<unsigned>(info.getConnHandle()));
    queueTransportEvent(event);
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo& info, int) override {
    const bool trackedInbound = gInboundConnectionHandle == info.getConnHandle();
    if (trackedInbound) gInboundConnectionHandle = BLE_HS_CONN_HANDLE_NONE;
    if (trackedInbound && !gOutboundPhysicalLink && !gOutboundConnectPending) {
      TransportEvent event{}; event.kind = TransportEventKind::Disconnected; queueTransportEvent(event);
    }
    Serial.printf("[BLE] inbound physical link disconnected handle=%u tracked=%u\n",
                  static_cast<unsigned>(info.getConnHandle()), trackedInbound ? 1U : 0U);
    if (gMultiplayer && gMultiplayer->enabled() && !gOutboundPhysicalLink &&
        !gOutboundConnectPending) NimBLEDevice::startAdvertising();
  }
} gServerCallbacks;

class RxCallbacks final : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    const NimBLEAttValue& value = characteristic->getValue();
    TransportEvent event{}; event.kind = TransportEventKind::Packet;
    event.length = static_cast<uint16_t>(std::min<size_t>(value.size(), sizeof(event.bytes)));
    std::memcpy(event.bytes, value.data(), event.length); queueTransportEvent(event);
  }
} gRxCallbacks;

class ClientCallbacks final : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient*) override {
    if (NimBLEDevice::getScan()) NimBLEDevice::getScan()->stop();
    Serial.println("[BLE] outbound physical link connected");
  }
  void onDisconnect(NimBLEClient*, int) override {
    if (gOutboundPhysicalLink) {
      TransportEvent event{}; event.kind = TransportEventKind::Disconnected; queueTransportEvent(event);
    }
    gOutboundPhysicalLink = false;
    Serial.println("[BLE] outbound physical link disconnected");
  }
} gClientCallbacks;

void notificationCallback(NimBLERemoteCharacteristic*, uint8_t* bytes, size_t length, bool) {
  TransportEvent event{}; event.kind = TransportEventKind::Packet;
  event.length = static_cast<uint16_t>(std::min<size_t>(length, sizeof(event.bytes)));
  std::memcpy(event.bytes, bytes, event.length); queueTransportEvent(event);
}

class ScanCallbacks final : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* device) override {
    if (!gMultiplayer) return;
    // The compact manufacturer record is the discovery contract. Advertising
    // this plus a 128-bit service UUID exceeded the 31-byte legacy BLE limit,
    // so NimBLE silently omitted this record and every Pokegochi was rejected
    // by the scanner. The full UUID is still verified after connecting.
    const std::string data = device->getManufacturerData();
    const std::string address = device->getAddress().toString();
    const uint32_t now = millis();
    if (now - gLastAdvertisementLogMs >= 500U) {
      gLastAdvertisementLogMs = now;
      char hex[37]{};
      const size_t shown = std::min<size_t>(data.size(), 18U);
      for (size_t i = 0; i < shown; ++i)
        std::snprintf(hex + i * 2U, sizeof(hex) - i * 2U, "%02X",
                      static_cast<unsigned>(static_cast<uint8_t>(data[i])));
      Serial.printf("[BLE] adv rssi=%d address=%s type=%u mfg_len=%u mfg=%s\n",
                    static_cast<int>(device->getRSSI()), address.c_str(),
                    static_cast<unsigned>(device->getAddressType()),
                    static_cast<unsigned>(data.size()), hex);
    }
    PokegochiMultiplayerProtocol::Advertisement advertisement{};
    if (!PokegochiMultiplayerProtocol::decodeManufacturerAdvertisement(
            reinterpret_cast<const uint8_t*>(data.data()), data.size(), advertisement) ||
        advertisement.playerId == gMultiplayer->playerId()) return;
    TransportEvent event{}; event.kind = TransportEventKind::Discovered;
    event.id = advertisement.playerId; event.rssi = device->getRSSI();
    event.addressType = device->getAddressType();
    std::strncpy(event.address, address.c_str(), sizeof(event.address) - 1U);
    Serial.printf("[BLE] discovered player=%05lu rssi=%d address=%s framing=%u\n",
                  static_cast<unsigned long>(event.id), static_cast<int>(event.rssi), event.address,
                  static_cast<unsigned>(data.size()));
    queueTransportEvent(event);
  }
} gScanCallbacks;
#endif
}  // namespace

bool Multiplayer::begin() {
  gMultiplayer = this;
#if defined(ARDUINO_ARCH_ESP32)
  const uint64_t mac = ESP.getEfuseMac();
  const auto derivePlayerId = [mac]() {
    uint32_t mixed = static_cast<uint32_t>(mac) ^ static_cast<uint32_t>(mac >> 24U) ^ 0x50474F43U;
    mixed ^= mixed >> 16U; mixed *= 0x7FEB352DU; mixed ^= mixed >> 15U;
    return 10000U + mixed % 90000U;
  };
  Preferences prefs;
  if (prefs.begin("pokegochi-mp", false)) {
    playerId_ = prefs.getUInt("player-id", 0);
    if (!playerId_) {
      playerId_ = derivePlayerId();
      prefs.putUInt("player-id", playerId_);
    }
    prefs.getBytes("online-host", onlineHost_, sizeof(onlineHost_));
    prefs.getBytes("online-code", onlineAccessCode_, sizeof(onlineAccessCode_));
    prefs.getBytes("online-port", &onlinePort_, sizeof(onlinePort_));
    onlineHost_[sizeof(onlineHost_) - 1U] = 0;
    onlineAccessCode_[sizeof(onlineAccessCode_) - 1U] = 0;
    if (!onlinePort_) onlinePort_ = PokegochiOnlineProtocol::kDefaultPort;
    // Radio power is deliberately session-only. Restoring ON before the
    // renderer can release its scene arena caused a PHY allocation abort and
    // an endless reboot loop after one successful tap.
    if (prefs.getBool("enabled", false)) prefs.putBool("enabled", false);
    prefs.end();
  } else {
    // The save service deliberately keeps its own NVS handle open. On a
    // fragmented low-memory boot a second Preferences handle can fail; the
    // identity is derived from the immutable chip MAC, so multiplayer remains
    // usable even when its convenience preference cannot be opened.
    playerId_ = derivePlayerId();
    Serial.println("[BLE] preferences unavailable; using MAC identity and OFF default");
  }
#else
  playerId_ = 12345;
#if defined(POKEGOCHI_APP_BUILD)
  Preferences prefs;
  if (prefs.begin("pokegochi-mp", false)) {
    playerId_ = prefs.getUInt("player-id", 0);
    if (playerId_ < 10000U || playerId_ > 99999U) {
      uint64_t seed = static_cast<uint64_t>(
          std::chrono::high_resolution_clock::now().time_since_epoch().count());
      uint32_t mixed = static_cast<uint32_t>(seed) ^
          static_cast<uint32_t>(seed >> 32U);
      mixed ^= mixed >> 16U; mixed *= 0x7FEB352DU; mixed ^= mixed >> 15U;
      playerId_ = 10000U + mixed % 90000U;
      prefs.putUInt("player-id", playerId_);
    }
    prefs.getBytes("online-host", onlineHost_, sizeof(onlineHost_));
    prefs.getBytes("online-code", onlineAccessCode_, sizeof(onlineAccessCode_));
    prefs.getBytes("online-port", &onlinePort_, sizeof(onlinePort_));
    prefs.end();
  }
#endif
  onlineHost_[sizeof(onlineHost_) - 1U] = 0;
  onlineAccessCode_[sizeof(onlineAccessCode_) - 1U] = 0;
  if (!onlinePort_) onlinePort_ = PokegochiOnlineProtocol::kDefaultPort;
#endif
  // Distribution builds always carry the outbound Internet route. Local
  // DNS-SD may replace it for the current session, but stale LAN addresses in
  // NVS must never make mobile/remote play depend on the home subnet.
  std::strncpy(onlineHost_, PokegochiOnlineRelayConfig::kBrokerHost,
               sizeof(onlineHost_) - 1U);
  onlineHost_[sizeof(onlineHost_) - 1U] = 0;
  std::snprintf(onlineAccessCode_, sizeof(onlineAccessCode_), "%06lu",
                static_cast<unsigned long>(
                    PokegochiOnlineRelayConfig::kAccessCode));
  onlinePort_ = PokegochiOnlineRelayConfig::kBrokerPort;
  loadSavedWifiNetworks();
  setState(State::Off, "TURN BLUETOOTH ON TO PLAY.");
  return true;
}

bool Multiplayer::beginWithPlayerId(uint32_t playerId) {
  if (!begin()) return false;
#if !defined(ARDUINO_ARCH_ESP32)
  if (playerId) playerId_ = playerId;
#else
  (void)playerId;
#endif
  return true;
}

void Multiplayer::shutdown() { setEnabled(false); }

uint32_t Multiplayer::playerId() const { return playerId_; }
void Multiplayer::setOnlineBalancedWins(uint32_t wins) {
  onlineBalancedWins_ = wins;
}
uint32_t Multiplayer::onlineBalancedWins() const { return onlineBalancedWins_; }
bool Multiplayer::enabled() const { return enabled_; }
bool Multiplayer::selectTransport(TransportKind kind) {
  if (enabled_ && kind != transportKind_) return false;
  transportKind_ = kind;
  onlinePeerSearchPending_ = false;
  setState(State::Off, kind == TransportKind::Wifi
      ? "TURN WIFI MULTIPLAYER ON TO PLAY."
      : "TURN BLUETOOTH ON TO PLAY.");
  return true;
}
Multiplayer::TransportKind Multiplayer::transportKind() const { return transportKind_; }

bool Multiplayer::setOnlineConfiguration(const char* host, uint16_t port,
                                         const char* accessCode) {
  if (enabled_ || !host || !host[0] || !accessCode || std::strlen(accessCode) != 6U)
    return false;
  for (size_t index = 0; index < 6U; ++index)
    if (accessCode[index] < '0' || accessCode[index] > '9') return false;
  std::strncpy(onlineHost_, host, sizeof(onlineHost_) - 1U);
  onlineHost_[sizeof(onlineHost_) - 1U] = 0;
  std::memcpy(onlineAccessCode_, accessCode, 6U);
  onlineAccessCode_[6] = 0;
  onlinePort_ = port ? port : PokegochiOnlineProtocol::kDefaultPort;
  onlineConfigurationForced_ = true;
#if defined(ARDUINO_ARCH_ESP32) || defined(POKEGOCHI_APP_BUILD)
  Preferences prefs;
  if (prefs.begin("pokegochi-mp", false)) {
    prefs.putBytes("online-host", onlineHost_, sizeof(onlineHost_));
    prefs.putBytes("online-code", onlineAccessCode_, sizeof(onlineAccessCode_));
    prefs.putBytes("online-port", &onlinePort_, sizeof(onlinePort_));
    prefs.end();
  }
#endif
  changed_ = true;
  return true;
}

const char* Multiplayer::onlineHost() const { return onlineHost_; }
uint16_t Multiplayer::onlinePort() const { return onlinePort_; }
const char* Multiplayer::onlineAccessCode() const { return onlineAccessCode_; }

bool Multiplayer::setWifiCredentials(const char* ssid, const char* password) {
  if (enabled_ || !ssid || !ssid[0] || !password) return false;
  std::strncpy(wifiSsid_, ssid, sizeof(wifiSsid_) - 1U);
  wifiSsid_[sizeof(wifiSsid_) - 1U] = 0;
  std::strncpy(wifiPassword_, password, sizeof(wifiPassword_) - 1U);
  wifiPassword_[sizeof(wifiPassword_) - 1U] = 0;
  // A typed password is only a candidate until association succeeds. This
  // prevents a typo from evicting a working saved network.
  wifiManualCandidate_ = true;
  changed_ = true;
  return true;
}
const char* Multiplayer::wifiSsid() const { return wifiSsid_; }
const char* Multiplayer::wifiPassword() const { return wifiPassword_; }
uint8_t Multiplayer::savedWifiNetworkCount() const {
  return savedWifiNetworkCount_;
}
const char* Multiplayer::savedWifiNetworkName(uint8_t index) const {
  return index < savedWifiNetworkCount_ ? savedWifiNetworks_[index].ssid : "";
}
bool Multiplayer::wifiNetworkIsSaved(const char* ssid) const {
  if (!ssid || !ssid[0]) return false;
  for (uint8_t index = 0; index < savedWifiNetworkCount_; ++index)
    if (std::strncmp(savedWifiNetworks_[index].ssid, ssid,
                     sizeof(savedWifiNetworks_[index].ssid)) == 0) return true;
  return false;
}
const char* Multiplayer::savedWifiPassword(const char* ssid) const {
  if (!ssid || !ssid[0]) return "";
  for (uint8_t index = 0; index < savedWifiNetworkCount_; ++index)
    if (std::strncmp(savedWifiNetworks_[index].ssid, ssid,
                     sizeof(savedWifiNetworks_[index].ssid)) == 0)
      return savedWifiNetworks_[index].password;
  return "";
}

void Multiplayer::loadSavedWifiNetworks() {
  savedWifiNetworkCount_ = 0;
  std::memset(savedWifiNetworks_, 0, sizeof(savedWifiNetworks_));
#if defined(ARDUINO_ARCH_ESP32) || defined(POKEGOCHI_APP_BUILD)
  Preferences prefs;
  if (prefs.begin("pokegochi-mp", false)) {
    const uint32_t storedCount = prefs.getUInt(kWifiCountKey, 0U);
    if (storedCount <= kMaximumSavedWifiNetworks && storedCount > 0U &&
        prefs.getBytesLength(kWifiMruKey) == sizeof(savedWifiNetworks_) &&
        prefs.getBytes(kWifiMruKey, savedWifiNetworks_,
                       sizeof(savedWifiNetworks_)) == sizeof(savedWifiNetworks_)) {
      savedWifiNetworkCount_ = static_cast<uint8_t>(storedCount);
      for (uint8_t index = 0; index < savedWifiNetworkCount_; ++index) {
        savedWifiNetworks_[index].ssid[
            sizeof(savedWifiNetworks_[index].ssid) - 1U] = 0;
        savedWifiNetworks_[index].password[
            sizeof(savedWifiNetworks_[index].password) - 1U] = 0;
        if (!savedWifiNetworks_[index].ssid[0]) {
          savedWifiNetworkCount_ = index;
          break;
        }
      }
    }
    // One-time migration from the original single-network layout. Keep the
    // old keys too, so installing an older firmware does not destroy access.
    if (!savedWifiNetworkCount_) {
      WifiCredential legacy{};
      if (prefs.getBytesLength("wifi-ssid") == sizeof(legacy.ssid) &&
          prefs.getBytes("wifi-ssid", legacy.ssid, sizeof(legacy.ssid)) ==
              sizeof(legacy.ssid)) {
        prefs.getBytes("wifi-pass", legacy.password, sizeof(legacy.password));
        legacy.ssid[sizeof(legacy.ssid) - 1U] = 0;
        legacy.password[sizeof(legacy.password) - 1U] = 0;
        if (legacy.ssid[0]) {
          savedWifiNetworks_[0] = legacy;
          savedWifiNetworkCount_ = 1;
          prefs.putBytes(kWifiMruKey, savedWifiNetworks_,
                         sizeof(savedWifiNetworks_));
          prefs.putUInt(kWifiCountKey, savedWifiNetworkCount_);
        }
      }
    }
    prefs.end();
  }
#endif
  if (savedWifiNetworkCount_) {
    std::strncpy(wifiSsid_, savedWifiNetworks_[0].ssid,
                 sizeof(wifiSsid_) - 1U);
    std::strncpy(wifiPassword_, savedWifiNetworks_[0].password,
                 sizeof(wifiPassword_) - 1U);
  }
  wifiSsid_[sizeof(wifiSsid_) - 1U] = 0;
  wifiPassword_[sizeof(wifiPassword_) - 1U] = 0;
  wifiManualCandidate_ = false;
}

void Multiplayer::rememberSuccessfulWifiNetwork(const char* ssid,
                                                 const char* password) {
  if (!ssid || !ssid[0] || !password) return;
  WifiCredential connected{};
  std::strncpy(connected.ssid, ssid, sizeof(connected.ssid) - 1U);
  std::strncpy(connected.password, password,
               sizeof(connected.password) - 1U);
  WifiCredential reordered[kMaximumSavedWifiNetworks]{};
  reordered[0] = connected;
  uint8_t count = 1;
  for (uint8_t index = 0;
       index < savedWifiNetworkCount_ && count < kMaximumSavedWifiNetworks;
       ++index) {
    if (std::strncmp(savedWifiNetworks_[index].ssid, connected.ssid,
                     sizeof(connected.ssid)) == 0) continue;
    reordered[count++] = savedWifiNetworks_[index];
  }
  std::memcpy(savedWifiNetworks_, reordered, sizeof(savedWifiNetworks_));
  savedWifiNetworkCount_ = count;
  std::strncpy(wifiSsid_, connected.ssid, sizeof(wifiSsid_) - 1U);
  std::strncpy(wifiPassword_, connected.password,
               sizeof(wifiPassword_) - 1U);
  wifiSsid_[sizeof(wifiSsid_) - 1U] = 0;
  wifiPassword_[sizeof(wifiPassword_) - 1U] = 0;
  wifiManualCandidate_ = false;
#if defined(ARDUINO_ARCH_ESP32) || defined(POKEGOCHI_APP_BUILD)
  Preferences prefs;
  if (prefs.begin("pokegochi-mp", false)) {
    prefs.putBytes(kWifiMruKey, savedWifiNetworks_, sizeof(savedWifiNetworks_));
    prefs.putUInt(kWifiCountKey, savedWifiNetworkCount_);
    prefs.putBytes("wifi-ssid", wifiSsid_, sizeof(wifiSsid_));
    prefs.putBytes("wifi-pass", wifiPassword_, sizeof(wifiPassword_));
    prefs.end();
  }
#endif
  changed_ = true;
}
uint8_t Multiplayer::scanWifiNetworks() {
  wifiNetworkCount_ = 0;
  std::memset(wifiNetworks_, 0, sizeof(wifiNetworks_));
  std::memset(wifiNetworkRssi_, 0, sizeof(wifiNetworkRssi_));
#if defined(ARDUINO_ARCH_ESP32)
  if (enabled_) return 0;
  Serial.printf("[WIFI] scan init heap=%u block=%u\n",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()));
  wifiNetworkCount_ = scanPhysicalWifiNetworks(
      wifiNetworks_, wifiNetworkRssi_, 6U);
  Serial.printf("[WIFI] native scan complete kept=%u heap=%u block=%u\n",
                static_cast<unsigned>(wifiNetworkCount_),
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()));
#elif defined(POKEGOCHI_APP_BUILD)
  wifiScanRequestPending_ = true;
  wifiScanInProgress_ = true;
#endif
  changed_ = true;
  return wifiNetworkCount_;
}
bool Multiplayer::takeWifiScanRequest() {
  const bool pending = wifiScanRequestPending_;
  wifiScanRequestPending_ = false;
  return pending;
}
void Multiplayer::setWifiScanResults(const char* const* names,
                                     const int8_t* rssi, uint8_t count) {
  wifiNetworkCount_ = 0;
  std::memset(wifiNetworks_, 0, sizeof(wifiNetworks_));
  std::memset(wifiNetworkRssi_, 0, sizeof(wifiNetworkRssi_));
  if (names && rssi) {
    for (uint8_t index = 0; index < count && wifiNetworkCount_ < 6U; ++index) {
      if (!names[index] || !names[index][0]) continue;
      bool duplicate = false;
      for (uint8_t existing = 0; existing < wifiNetworkCount_; ++existing)
        duplicate = duplicate ||
            std::strncmp(names[index], wifiNetworks_[existing], 32U) == 0;
      if (duplicate) continue;
      std::strncpy(wifiNetworks_[wifiNetworkCount_], names[index], 32U);
      wifiNetworks_[wifiNetworkCount_][32] = 0;
      wifiNetworkRssi_[wifiNetworkCount_] = rssi[index];
      ++wifiNetworkCount_;
    }
  }
  wifiScanRequestPending_ = false;
  wifiScanInProgress_ = false;
  changed_ = true;
}
bool Multiplayer::wifiScanInProgress() const { return wifiScanInProgress_; }
uint8_t Multiplayer::wifiNetworkCount() const { return wifiNetworkCount_; }
const char* Multiplayer::wifiNetworkName(uint8_t index) const {
  return index < wifiNetworkCount_ ? wifiNetworks_[index] : "";
}
int8_t Multiplayer::wifiNetworkRssi(uint8_t index) const {
  return index < wifiNetworkCount_ ? wifiNetworkRssi_[index] : -127;
}
Multiplayer::State Multiplayer::state() const { return state_; }
Multiplayer::Activity Multiplayer::activity() const { return activity_; }
bool Multiplayer::connected() const {
  return transportConnected_ && peerId_ && state_ >= State::Connected && state_ != State::Error;
}
bool Multiplayer::initiatedConnection() const { return connected() && outbound_; }
uint32_t Multiplayer::peerId() const { return peerId_; }
const char* Multiplayer::statusText() const { return status_; }
bool Multiplayer::peerSearchInProgress() const {
  return state_ == State::Scanning &&
      (transportKind_ != TransportKind::Wifi || onlinePeerSearchPending_);
}
uint8_t Multiplayer::peerCount() const { return peerCount_; }
Multiplayer::Peer Multiplayer::peer(uint8_t index) const {
  return index < peerCount_ ? peers_[index].publicData : Peer{};
}
const Multiplayer::TradeOffer& Multiplayer::localTradeOffer() const { return localOffer_; }
const Multiplayer::TradeOffer& Multiplayer::remoteTradeOffer() const { return remoteOffer_; }
bool Multiplayer::localTradeConfirmed() const { return localTradeConfirmed_; }
bool Multiplayer::remoteTradeConfirmed() const { return remoteTradeConfirmed_; }
bool Multiplayer::activityAccepted() { return activityAccepted_; }

void Multiplayer::setState(State next, const char* status) {
  state_ = next;
  if (status) {
    std::strncpy(status_, status, sizeof(status_) - 1U);
    status_[sizeof(status_) - 1U] = 0;
  }
  changed_ = true;
}

bool Multiplayer::takeChanged() { const bool value = changed_; changed_ = false; return value; }

bool Multiplayer::takeUnexpectedDisconnect() {
  const bool value = unexpectedDisconnectPending_;
  unexpectedDisconnectPending_ = false;
  return value;
}

bool Multiplayer::setEnabled(bool enable) {
  if (enable == enabled_) return true;
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("[%s] toggle %s heap=%u block=%u\n",
                transportKind_ == TransportKind::Wifi ? "WIFI" : "BLE",
                enable ? "ON" : "OFF",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()));
#endif
  if (!enable) {
    onlinePeerSearchPending_ = false;
    disconnect();
#if defined(ARDUINO_ARCH_ESP32)
    if (transportKind_ == TransportKind::Ble) {
      if (initialized_) NimBLEDevice::deinit(true);
      if (gTransportQueue) { vQueueDelete(gTransportQueue); gTransportQueue = nullptr; }
      portENTER_CRITICAL(&gDiscoveryMux);
      gDiscoveryPending = false; gLatestDiscovery = {}; gCriticalTransportDrops = 0;
      portEXIT_CRITICAL(&gDiscoveryMux);
      gServer = nullptr; gServerTx = nullptr; gClient = nullptr; gRemoteRx = nullptr;
      gOutboundPhysicalLink = false; gOutboundConnectPending = false;
    } else {
      resetOnlineTransportState(true);
      WiFi.disconnect(true, false);
      WiFi.mode(WIFI_OFF);
      releaseArduinoNetworkRuntime();
    }
#endif
    initialized_ = false; enabled_ = false; transportConnected_ = false;
    localTransportClosePending_ = false;
    unexpectedDisconnectPending_ = false;
    setState(State::Off, transportKind_ == TransportKind::Wifi
        ? "TURN WIFI MULTIPLAYER ON TO PLAY."
        : "TURN BLUETOOTH ON TO PLAY.");
    return true;
  }
  enabled_ = true;
  localTransportClosePending_ = false;
  unexpectedDisconnectPending_ = false;
  if (!startTransport()) {
    enabled_ = false;
#if defined(ARDUINO_ARCH_ESP32)
    // A failed station or relay connection can occur after the Wi-Fi driver
    // has already claimed its runtime buffers. Return the radio to the exact
    // OFF state expected by the renderer before the UI restores Home's arena.
    if (transportKind_ == TransportKind::Wifi) {
      resetOnlineTransportState(true);
      WiFi.disconnect(true, false);
      WiFi.mode(WIFI_OFF);
      releaseArduinoNetworkRuntime();
    }
#endif
    setState(State::Error, transportKind_ == TransportKind::Wifi
        ? "WIFI MULTIPLAYER COULD NOT START."
        : "BLUETOOTH COULD NOT START.");
#if defined(ARDUINO_ARCH_ESP32)
    Serial.printf("[%s] start failed heap=%u block=%u\n",
                  transportKind_ == TransportKind::Wifi ? "WIFI" : "BLE",
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getMaxAllocHeap()));
#endif
    return false;
  }
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("[%s] radio ready heap=%u block=%u\n",
                transportKind_ == TransportKind::Wifi ? "WIFI" : "BLE",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()));
#endif
  setState(State::Advertising, transportKind_ == TransportKind::Wifi
      ? "ONLINE SERVER CONNECTED. FIND A PLAYER."
      : "READY. FIND A NEARBY PLAYER.");
  // Advertising and scanning are independent BLE roles. Start both so peer
  // discovery does not depend on a narrow race between scan windows.
  if (transportKind_ == TransportKind::Ble) startScan();
  return true;
}

bool Multiplayer::startTransport() {
#if !defined(ARDUINO_ARCH_ESP32)
  initialized_ = true; return true;
#else
  if (initialized_) return true;
  if (transportKind_ == TransportKind::Wifi) {
    if (!wifiManualCandidate_ && !savedWifiNetworkCount_) {
      Serial.println("[WIFI] no saved networks or selected candidate");
      return false;
    }
    // The caller parks GameSave and the scene arena before station startup and
    // may lend the mutually-exclusive BLE controller pool to Wi-Fi. Arduino's
    // long-lived Network event runtime is reserved at boot, so shutdown can
    // reconstruct the normal UI allocation map without fragmenting its scene
    // workspace. A later BLE request uses the controlled BLE-resume path.
    resetOnlineTransportState(true);
    char visibleNetworks[6][33]{};
    int8_t visibleRssi[6]{};
    const uint8_t visibleCount = scanPhysicalWifiNetworks(
        visibleNetworks, visibleRssi, 6U);
    // esp_wifi_init is wrapped by this firmware, so Arduino's generic
    // 32-RX/32-TX request is bounded before the driver can allocate it.
    // Apply RAM-only credential storage before low-level initialization. The
    // previous order changed this flag only after esp_wifi_init had already
    // created its NVS-backed runtime context.
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    // PvP has long authored animations and replacement menus. Keep the
    // station awake so a quiet turn does not let modem power-save defer the
    // small TCP heartbeat behind TFT/SD traffic.
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    const auto isVisible = [&](const char* ssid) {
      for (uint8_t index = 0; index < visibleCount; ++index)
        if (std::strncmp(visibleNetworks[index], ssid, 33U) == 0) return true;
      return false;
    };
    const auto tryNetwork = [&](const char* ssid, const char* password) {
      if (!ssid || !ssid[0] || !isVisible(ssid)) return false;
      Serial.printf("[WIFI] trying saved network ssid=%s\n", ssid);
      WiFi.disconnect(false, false);
      delay(40);
      WiFi.begin(ssid, password ? password : "");
      const uint32_t deadline = millis() + kWifiCandidateTimeoutMs;
      while (WiFi.status() != WL_CONNECTED &&
             static_cast<int32_t>(millis() - deadline) < 0) delay(40);
      return WiFi.status() == WL_CONNECTED;
    };
    bool stationConnected = false;
    if (wifiManualCandidate_)
      stationConnected = tryNetwork(wifiSsid_, wifiPassword_);
    for (uint8_t index = 0;
         !stationConnected && index < savedWifiNetworkCount_; ++index) {
      const bool exactCandidate = wifiManualCandidate_ &&
          std::strncmp(savedWifiNetworks_[index].ssid, wifiSsid_,
                       sizeof(wifiSsid_)) == 0 &&
          std::strncmp(savedWifiNetworks_[index].password, wifiPassword_,
                       sizeof(wifiPassword_)) == 0;
      if (!exactCandidate)
        stationConnected = tryNetwork(savedWifiNetworks_[index].ssid,
                                      savedWifiNetworks_[index].password);
      if (stationConnected) {
        std::strncpy(wifiSsid_, savedWifiNetworks_[index].ssid,
                     sizeof(wifiSsid_) - 1U);
        std::strncpy(wifiPassword_, savedWifiNetworks_[index].password,
                     sizeof(wifiPassword_) - 1U);
      }
    }
    if (!stationConnected) {
      Serial.printf("[WIFI] no saved network connected visible=%u status=%d\n",
                    static_cast<unsigned>(visibleCount), WiFi.status());
      WiFi.disconnect(false, false);
      return false;
    }
    rememberSuccessfulWifiNetwork(wifiSsid_, wifiPassword_);
    char relayHost[sizeof(onlineHost_)]{};
    char relayAccessCode[sizeof(onlineAccessCode_)]{};
    uint16_t relayPort = PokegochiOnlineProtocol::kDefaultPort;
    bool relayResolved = onlineConfigurationForced_;
    if (relayResolved) {
      std::strncpy(relayHost, onlineHost_, sizeof(relayHost) - 1U);
      std::strncpy(relayAccessCode, onlineAccessCode_,
                   sizeof(relayAccessCode) - 1U);
      relayPort = onlinePort_;
      Serial.printf("[WIFI] using cabled diagnostic relay host=%s port=%u\n",
                    relayHost, static_cast<unsigned>(relayPort));
    } else {
      relayResolved = discoverOnlineRelay(
          playerId_, relayHost, sizeof(relayHost), relayPort,
          relayAccessCode, sizeof(relayAccessCode));
    }
    gOnlineCloudMode = !relayResolved;
    if (gOnlineCloudMode) {
      std::strncpy(relayHost, PokegochiOnlineRelayConfig::kBrokerHost,
                   sizeof(relayHost) - 1U);
      std::snprintf(relayAccessCode, sizeof(relayAccessCode), "%06lu",
                    static_cast<unsigned long>(
                        PokegochiOnlineRelayConfig::kAccessCode));
      relayPort = PokegochiOnlineRelayConfig::kBrokerPort;
      Serial.println("[WIFI] local relay absent; using Internet route");
    }
    // Discovery data is runtime state too: reconnects after a dropped relay
    // socket must use the endpoint that was actually found, not stale NVS.
    std::strncpy(onlineHost_, relayHost, sizeof(onlineHost_) - 1U);
    onlineHost_[sizeof(onlineHost_) - 1U] = 0;
    std::strncpy(onlineAccessCode_, relayAccessCode,
                 sizeof(onlineAccessCode_) - 1U);
    onlineAccessCode_[sizeof(onlineAccessCode_) - 1U] = 0;
    onlinePort_ = relayPort;
    if (gOnlineCloudMode) {
      gOnlineCloudNonce = esp_random();
      if (!gOnlineCloudNonce) gOnlineCloudNonce = 1U;
      std::snprintf(gOnlineCloudPublishTopic, sizeof(gOnlineCloudPublishTopic),
                    "%s/server/%05lu/%08lx",
                    PokegochiOnlineRelayConfig::kTopicRoot,
                    static_cast<unsigned long>(playerId_),
                    static_cast<unsigned long>(gOnlineCloudNonce));
      std::snprintf(gOnlineCloudSubscribeTopic, sizeof(gOnlineCloudSubscribeTopic),
                    "%s/client/%05lu/%08lx",
                    PokegochiOnlineRelayConfig::kTopicRoot,
                    static_cast<unsigned long>(playerId_),
                    static_cast<unsigned long>(gOnlineCloudNonce));
      // PubSubClient retains this pointer for every later reconnect. relayHost
      // is a stack buffer and becomes invalid as soon as radio startup
      // returns; onlineHost_ is the durable endpoint copied just above.
      gOnlineMqtt.setServer(onlineHost_, onlinePort_);
      gOnlineMqtt.setBufferSize(PokegochiOnlineRelayConfig::kMqttBufferBytes);
      gOnlineMqtt.setKeepAlive(
          PokegochiOnlineRelayConfig::kMqttKeepAliveSeconds);
      gOnlineMqtt.setSocketTimeout(5);
      gOnlineMqtt.setCallback(receiveOnlineCloudFrame);
      char clientId[48]{};
      std::snprintf(clientId, sizeof(clientId), "pokegochi-%05lu-%08lx",
                    static_cast<unsigned long>(playerId_),
                    static_cast<unsigned long>(gOnlineCloudNonce));
      if (!gOnlineMqtt.connect(clientId) ||
          !gOnlineMqtt.subscribe(gOnlineCloudSubscribeTopic, 0)) {
        Serial.printf("[WIFI] Internet relay connection failed state=%d\n",
                      gOnlineMqtt.state());
        resetOnlineTransportState(true);
        return false;
      }
    } else if (!gOnlineClient.connect(relayHost, relayPort, 8000)) {
        Serial.printf("[WIFI] local relay connection failed host=%s port=%u\n",
                      relayHost, static_cast<unsigned>(relayPort));
        return false;
    } else {
      // Keep Nagle enabled on the no-PSRAM handheld. Party and authoritative
      // snapshots are short bursts of several sub-200-byte frames; TCP_NODELAY
      // makes lwIP reserve a full-MSS pbuf for each one and exhausts the small
      // internal heap before READY/faint-switch completes. They now share one
      // segment, while isolated player commands still transmit immediately.
      gOnlineClient.setNoDelay(false);
    }
    const PokegochiOnlineProtocol::HelloPayload hello{
        PokegochiMultiplayerProtocol::kVersion, {0, 0, 0},
        static_cast<uint32_t>(std::strtoul(relayAccessCode, nullptr, 10)),
        onlineBalancedWins_};
    if (!sendOnlineFrame(PokegochiOnlineProtocol::MessageType::Hello,
                         playerId_, 0, 0, &hello, sizeof(hello))) {
      resetOnlineTransportState(true);
      return false;
    }
    gOnlineLastRxMs = gOnlineLastPingMs = millis();
    initialized_ = true;
    Serial.printf("[WIFI] relay connected host=%s port=%u ip=%s\n",
                  relayHost, static_cast<unsigned>(relayPort),
                  WiFi.localIP().toString().c_str());
    return true;
  }
  // Discovery uses a coalescing mailbox, while this queue is reserved for
  // critical link and packet events. Eight entries remain below the largest
  // free block available after FAT/renderer/save startup and absorb short BLE
  // bursts without silently losing battle state.
  if (!gTransportQueue) gTransportQueue = xQueueCreate(8, sizeof(TransportEvent));
  if (!gTransportQueue) { Serial.println("[BLE] event queue allocation failed"); return false; }
  portENTER_CRITICAL(&gDiscoveryMux);
  gDiscoveryPending = false; gLatestDiscovery = {}; gCriticalTransportDrops = 0;
  portEXIT_CRITICAL(&gDiscoveryMux);
  char name[20]; std::snprintf(name, sizeof(name), "POKEGOCHI-%05lu", static_cast<unsigned long>(playerId_));
  NimBLEDevice::init(name);
  NimBLEDevice::setMTU(200);
  NimBLEDevice::setPower(3);
  gServer = NimBLEDevice::createServer();
  if (!gServer) return false;
  gServer->setCallbacks(&gServerCallbacks, false);
  NimBLEService* service = gServer->createService(kServiceUuid);
  if (!service) return false;
  NimBLECharacteristic* rx = service->createCharacteristic(kRxUuid,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, 192);
  gServerTx = service->createCharacteristic(kTxUuid, NIMBLE_PROPERTY::NOTIFY, 192);
  if (!rx || !gServerTx) return false;
  rx->setCallbacks(&gRxCallbacks);
  gServer->start();

  const PokegochiMultiplayerProtocol::Advertisement payload =
      PokegochiMultiplayerProtocol::makeAdvertisement(playerId_);
  NimBLEAdvertisementData data;
  const bool flagsReady = data.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  // Keep the legacy advertisement comfortably below 31 bytes. The PGCH
  // signature, protocol version and player id are sufficient to hide every
  // unrelated BLE device from the UI.
  const bool identityReady = data.setManufacturerData(
      reinterpret_cast<const uint8_t*>(&payload), sizeof(payload));
  if (!flagsReady || !identityReady) {
    Serial.printf("[BLE] advertisement payload failed flags=%u identity=%u\n",
                  flagsReady ? 1U : 0U, identityReady ? 1U : 0U);
    return false;
  }
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  if (!advertising->setAdvertisementData(data)) {
    Serial.println("[BLE] controller rejected advertisement payload");
    return false;
  }
  advertising->enableScanResponse(false);
  if (!advertising->start()) {
    Serial.println("[BLE] advertising could not start");
    return false;
  }
  Serial.printf("[BLE] advertising player=%05lu payload=%u bytes\n",
                static_cast<unsigned long>(playerId_),
                static_cast<unsigned>(data.getPayload().size()));

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&gScanCallbacks, false);
  scan->setActiveScan(false);
  scan->setInterval(90);
  scan->setWindow(45);
  scan->setMaxResults(0);
  initialized_ = true;
  return true;
#endif
}

void Multiplayer::startScan() {
  const bool link = connected();
  if (transportKind_ == TransportKind::Wifi) {
    if (!enabled_ || link) return;
    peerCount_ = 0;
    onlinePeerSearchPending_ = true;
    setState(State::Scanning, "SEARCHING FOR ONLINE PLAYERS...");
#if defined(ARDUINO_ARCH_ESP32)
    if (!gOnlineAuthenticated ||
        !sendOnlineFrame(PokegochiOnlineProtocol::MessageType::RefreshPeers,
                         playerId_)) {
      onlinePeerSearchPending_ = false;
      setState(State::Error, "ONLINE SERVER IS NOT READY.");
    }
#endif
    return;
  }
  onlinePeerSearchPending_ = false;
#if defined(ARDUINO_ARCH_ESP32)
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (!enabled_ || link || !scan) {
    Serial.printf("[BLE] scan skipped enabled=%u connected=%u transport=%u peer=%05lu state=%u scan=%u\n",
                  enabled_ ? 1U : 0U, link ? 1U : 0U, transportConnected_ ? 1U : 0U,
                  static_cast<unsigned long>(peerId_), static_cast<unsigned>(state_),
                  scan ? 1U : 0U);
    return;
  }
#else
  if (!enabled_ || link) return;
#endif
  peerCount_ = 0;
  setState(State::Scanning, "SEARCHING FOR POKEGOCHIS...");
#if defined(ARDUINO_ARCH_ESP32)
  const bool started = scan->start(5000, false, true);
  Serial.printf("[BLE] scan started window=5000ms result=%u active=%u\n",
                started ? 1U : 0U, scan->isScanning() ? 1U : 0U);
#endif
}

void Multiplayer::completeOnlinePeerScan() {
  if (!enabled_ || transportKind_ != TransportKind::Wifi ||
      state_ != State::Scanning || !onlinePeerSearchPending_) return;
  onlinePeerSearchPending_ = false;
  if (peerCount_)
    setState(State::Scanning, "SELECT AN ONLINE PLAYER.");
  else
    // Android uses Advertising as the completed-empty boundary. That stops
    // its one-shot relay query and requires a fresh FIND PLAYER tap; the
    // physical relay path has its own PeerListComplete handler below.
    setState(State::Advertising, "NO ONLINE PLAYERS FOUND. TAP FIND PLAYER.");
}

void Multiplayer::transportDiscovered(uint32_t id, int8_t rssi, const char* address,
                                      uint8_t type, uint32_t balancedWins) {
  // Relay results belong to the currently open query only. A delayed frame
  // after PeerListComplete must not make a player mysteriously appear while
  // the screen says that the search has already finished.
  if (transportKind_ == TransportKind::Wifi &&
      (state_ != State::Scanning || !onlinePeerSearchPending_)) return;
  for (uint8_t i = 0; i < peerCount_; ++i) {
    if (peers_[i].publicData.playerId != id) continue;
    peers_[i].publicData.rssi = rssi;
    peers_[i].publicData.balancedWins = balancedWins;
    changed_ = true;
    return;
  }
  if (peerCount_ >= kMaximumPeers) return;
  PeerRecord& record = peers_[peerCount_++];
  record.publicData = {id, rssi, balancedWins}; record.addressType = type;
  std::strncpy(record.address, address ? address : "", sizeof(record.address) - 1U);
  changed_ = true;
}

bool Multiplayer::choosePeer(uint8_t index) {
  if (state_ != State::Scanning || index >= peerCount_) return false;
  onlinePeerSearchPending_ = false;
#if defined(ARDUINO_ARCH_ESP32)
  if (transportKind_ == TransportKind::Ble && NimBLEDevice::getScan())
    NimBLEDevice::getScan()->stop();
#endif
  peerId_ = peers_[index].publicData.playerId;
  peers_[0] = peers_[index];
  setState(State::ConfirmOutgoingPeer, "CONNECT TO THIS PLAYER?");
  return true;
}

void Multiplayer::answerPeerInvitation(bool accept) {
  if (state_ == State::ConfirmOutgoingPeer) {
    if (!accept) { peerId_ = 0; setState(State::Advertising, "CONNECTION CANCELLED."); return; }
    setState(State::Connecting, "CONNECTING...");
    if (!connectTransport(peers_[0])) setState(State::Error, "PLAYER CONNECTION FAILED.");
    return;
  }
  if (state_ != State::ConfirmIncomingPeer) return;
  if (!accept) {
    sendPacket(InviteReject, nullptr, 0, invitationToken_);
    closeTransportLink();
    resetSession(true); setState(State::Advertising, "INVITATION DECLINED.");
  } else {
    sendPacket(InviteAccept, nullptr, 0, invitationToken_);
    setState(State::Connected, "PLAYER CONNECTED.");
  }
}

bool Multiplayer::connectTransport(const PeerRecord& peer) {
  if (transportKind_ == TransportKind::Wifi) {
    invitationToken_ = playerId_ ^ (peer.publicData.playerId << 1U) ^ 0x57494649U;
    if (!invitationToken_) invitationToken_ = 1U;
#if defined(ARDUINO_ARCH_ESP32)
    if (!gOnlineAuthenticated || !gOnlineClient.connected()) return false;
    return sendOnlineFrame(PokegochiOnlineProtocol::MessageType::OpenSession,
                           playerId_, peer.publicData.playerId);
#else
    // Android's socket bridge opens the virtual room and holds this Invite
    // until SessionReady arrives from the relay.
    (void)peer;
    return sendPacket(Invite, nullptr, 0, invitationToken_);
#endif
  }
#if !defined(ARDUINO_ARCH_ESP32)
  // The Android bridge can queue the invitation before GATT is ready, but the
  // physical transport only becomes connected after service discovery/CCCD.
  (void)peer; return sendPacket(Invite, nullptr, 0, invitationToken_);
#else
  NimBLEDevice::getScan()->stop();
  NimBLEDevice::stopAdvertising();
  gOutboundConnectPending = true;
  Serial.printf("[BLE] connecting player=%05lu address=%s type=%u\n",
                static_cast<unsigned long>(peer.publicData.playerId), peer.address,
                static_cast<unsigned>(peer.addressType));
  if (!gClient) {
    gClient = NimBLEDevice::createClient();
    if (!gClient) {
      gOutboundConnectPending = false;
      NimBLEDevice::startAdvertising();
      Serial.println("[BLE] client allocation failed");
      return false;
    }
    gClient->setClientCallbacks(&gClientCallbacks, false);
    gClient->setConnectionParams(12, 24, 0, 240);
    gClient->setConnectTimeout(8 * 1000);
  }
  const NimBLEAddress address(std::string(peer.address), peer.addressType);
  for (uint8_t attempt = 1; attempt <= 2; ++attempt) {
    Serial.printf("[BLE] GATT attempt=%u/2\n", static_cast<unsigned>(attempt));
    if (!gClient->connect(address, true, false, true)) {
      Serial.printf("[BLE] physical connection failed error=%d\n", gClient->getLastError());
    } else {
      NimBLERemoteService* service = gClient->getService(kServiceUuid);
      gRemoteRx = service ? service->getCharacteristic(kRxUuid) : nullptr;
      NimBLERemoteCharacteristic* remoteTx = service ? service->getCharacteristic(kTxUuid) : nullptr;
      if (gRemoteRx && remoteTx && remoteTx->subscribe(true, notificationCallback)) {
        gOutboundConnectPending = false;
        gOutboundPhysicalLink = true;
        outbound_ = true;
        transportConnected(true);
        invitationToken_ = static_cast<uint32_t>(micros()) ^ playerId_ ^ peerId_;
        return sendPacket(Invite, nullptr, 0, invitationToken_);
      }
      Serial.printf("[BLE] service/subscribe failed service=%u rx=%u tx=%u error=%d\n",
                    service ? 1U : 0U, gRemoteRx ? 1U : 0U, remoteTx ? 1U : 0U,
                    gClient->getLastError());
      gRemoteRx = nullptr;
      gClient->disconnect();
    }
    if (attempt < 2) delay(300);
  }
  gOutboundConnectPending = false;
  gOutboundPhysicalLink = false;
  NimBLEDevice::startAdvertising();
  return false;
#endif
}

void Multiplayer::transportConnected(bool outbound) {
  transportConnected_ = true; outbound_ = outbound;
  localTransportClosePending_ = false;
  // Do not erase an unconsumed drop edge when the TCP station reconnects
  // quickly. The game loop must still restore the disposable PvP Party and
  // leave the old battle; otherwise it continues as a ghost session inside a
  // newly-created room.
  changed_ = true;
}

void Multiplayer::transportDisconnected() {
  const bool hadConnection = transportConnected_;
  transportConnected_ = false;
  if (localTransportClosePending_) {
    localTransportClosePending_ = false;
    return;
  }
  unexpectedDisconnectPending_ = enabled_ && hadConnection;
  if (enabled_) { resetSession(true); setState(State::Advertising, "PLAYER DISCONNECTED."); }
}

bool Multiplayer::sendPacket(uint8_t type, const void* payload, uint16_t length, uint32_t transaction) {
  if (length > PokegochiMultiplayerProtocol::kMaximumPayload) return false;
  uint8_t bytes[sizeof(PacketHeader) + PokegochiMultiplayerProtocol::kMaximumPayload]{};
  const PacketHeader header{kPacketMagic, kProtocolVersion, type, playerId_, transaction, length};
  std::memcpy(bytes, &header, sizeof(header));
  if (length && payload) std::memcpy(bytes + sizeof(header), payload, length);
#if !defined(ARDUINO_ARCH_ESP32)
  if (outgoingPacketCount_ >= kOutgoingPacketCapacity) return false;
  OutgoingPacket& queued = outgoingPackets_[outgoingPacketCount_++];
  queued.length = static_cast<uint16_t>(sizeof(header) + length);
  std::memcpy(queued.bytes, bytes, queued.length);
  return true;
#else
  if (transportKind_ == TransportKind::Wifi) {
    if (!gOnlineSessionReady || !gOnlineSessionId || !gOnlinePeerId) return false;
    return sendOnlineFrame(PokegochiOnlineProtocol::MessageType::Relay,
                           playerId_, gOnlinePeerId, gOnlineSessionId,
                           bytes, static_cast<uint16_t>(sizeof(header) + length));
  }
  if (outbound_) return gRemoteRx && gRemoteRx->writeValue(bytes, sizeof(header) + length, true);
  return gServerTx && gServerTx->notify(bytes, sizeof(header) + length);
#endif
}

bool Multiplayer::takeOutgoingPacket(uint8_t* bytes, uint16_t& length) {
#if !defined(ARDUINO_ARCH_ESP32)
  if (!bytes || !outgoingPacketCount_) { length = 0; return false; }
  const OutgoingPacket& queued = outgoingPackets_[0];
  length = queued.length;
  std::memcpy(bytes, queued.bytes, length);
  for (uint8_t index = 1; index < outgoingPacketCount_; ++index)
    outgoingPackets_[index - 1] = outgoingPackets_[index];
  outgoingPackets_[--outgoingPacketCount_] = OutgoingPacket{};
  return true;
#else
  (void)bytes; length = 0; return false;
#endif
}

bool Multiplayer::requestPeer(uint32_t playerId) {
  if (!enabled_ || !playerId || playerId == playerId_) return false;
  peerId_ = playerId;
  invitationToken_ = playerId_ ^ (playerId << 1U) ^ 0x50474F43U;
  transportConnected_ = true;
  outbound_ = true;
  setState(State::Connecting, "CONNECTING...");
  return sendPacket(Invite, nullptr, 0, invitationToken_);
}

void Multiplayer::transportPacket(const uint8_t* bytes, size_t length) { processPacket(bytes, length); }

void Multiplayer::processPacket(const uint8_t* bytes, size_t length) {
  if (!bytes || length < sizeof(PacketHeader)) return;
  PacketHeader header{}; std::memcpy(&header, bytes, sizeof(header));
  if (header.magic != kPacketMagic || header.version != kProtocolVersion ||
      sizeof(header) + header.payloadLength != length || header.sourceId == playerId_) return;
  const uint8_t* payload = bytes + sizeof(header);
  switch (header.type) {
    case Invite:
      if (!enabled_) break;
      // A repeated invitation means the acceptance was lost; acknowledge it
      // again instead of rejecting an otherwise healthy session.
      if (connected()) {
        if (header.sourceId == peerId_ && header.transaction == invitationToken_)
          sendPacket(InviteAccept, nullptr, 0, header.transaction);
        else
          sendPacket(InviteReject, nullptr, 0, header.transaction);
        break;
      }
      if (state_ == State::ConfirmIncomingPeer && header.sourceId == peerId_ &&
          header.transaction == invitationToken_) break;
      // Simultaneous outgoing invitations are resolved without another prompt:
      // the lower player id remains the initiator and the higher id accepts.
      if (state_ == State::Connecting && header.sourceId == peerId_) {
        if (playerId_ < header.sourceId) {
          outbound_ = true;
          sendPacket(Invite, nullptr, 0, invitationToken_);
        } else {
          outbound_ = false;
          invitationToken_ = header.transaction;
          sendPacket(InviteAccept, nullptr, 0, invitationToken_);
          setState(State::Connected, "PLAYER CONNECTED.");
        }
        break;
      }
      outbound_ = false;
      peerId_ = header.sourceId; invitationToken_ = header.transaction;
      setState(State::ConfirmIncomingPeer, "A PLAYER WANTS TO CONNECT.");
      break;
    case InviteAccept:
      if (state_ == State::Connecting && header.transaction == invitationToken_) {
        outbound_ = true;
        peerId_ = header.sourceId; setState(State::Connected, "PLAYER CONNECTED.");
      }
      break;
    case InviteReject:
      if (state_ == State::Connecting) {
        closeTransportLink();
        resetSession(true); setState(State::Advertising, "PLAYER DECLINED.");
      }
      break;
    case SessionClose:
      // SessionClose is a logical packet, but the old implementation left the
      // GATT link alive.  Both peers then looked idle while their BLE stacks
      // still occupied the only connection, so the next connection failed.
      closeTransportLink();
      resetSession(true); setState(State::Advertising, "PLAYER ENDED THE SESSION.");
      break;
    case ActivityRequest:
      if (!connected() || outbound_ || state_ != State::Connected ||
          header.sourceId != peerId_ || header.payloadLength != sizeof(ActivityPayload)) break;
      {
      const ActivityPayload& request = *reinterpret_cast<const ActivityPayload*>(payload);
      activity_ = static_cast<Activity>(request.activity);
      const BattleRules requestedRules = static_cast<BattleRules>(request.rules);
      if (activity_ != Activity::Battle && activity_ != Activity::Trade) break;
      if ((activity_ == Activity::Battle && requestedRules != BattleRules::Normal &&
           requestedRules != BattleRules::Balanced) ||
          (activity_ == Activity::Trade && requestedRules != BattleRules::None)) {
        activity_ = Activity::None;
        break;
      }
      // A physical BLE link is intentionally reused for more than one
      // activity.  Do not let a completed/cancelled trade leave its two-phase
      // commit latch armed when the peer starts the next one.
      resetTradeState();
      if (activity_ == Activity::Battle) {
        // Rules are part of the invitation. The recipient never makes a
        // second, potentially conflicting selection.
        localBattleRules_ = remoteBattleRules_ = requestedRules;
        localBattleReady_ = remoteBattleReady_ = false;
      }
      activityAccepted_ = false;
      char requestStatus[64]{};
      if (activity_ == Activity::Battle)
        std::snprintf(requestStatus, sizeof(requestStatus), "PLAYER REQUESTS A %s BATTLE.",
                      requestedRules == BattleRules::Balanced ? "BALANCED" : "NORMAL");
      else
        std::strncpy(requestStatus, "PLAYER REQUESTS A TRADE.", sizeof(requestStatus) - 1U);
      setState(State::ConfirmIncomingActivity,
               requestStatus);
      }
      break;
    case ActivityAccept:
      if (outbound_ && state_ == State::WaitingActivityReply && header.sourceId == peerId_ &&
          header.payloadLength == sizeof(ActivityPayload)) {
        const ActivityPayload& accepted = *reinterpret_cast<const ActivityPayload*>(payload);
        const uint8_t expectedRules = activity_ == Activity::Battle
            ? static_cast<uint8_t>(localBattleRules_)
            : static_cast<uint8_t>(BattleRules::None);
        if (accepted.activity != static_cast<uint8_t>(activity_) ||
            accepted.rules != expectedRules) break;
        activityAccepted_ = true;
        setState(activity_ == Activity::Trade ? State::TradeSelecting : State::BattlePreparing,
                 activity_ == Activity::Trade ? "CHOOSE A POKEMON IN THE BOX." : "PREPARE YOUR PARTY, THEN START.");
#if defined(ARDUINO_ARCH_ESP32)
        Serial.printf("[LINK] activity accepted peer=%05lu rules=%u\n",
                      static_cast<unsigned long>(header.sourceId),
                      static_cast<unsigned>(accepted.rules));
#endif
      }
      break;
    case ActivityReject:
      if (outbound_ && state_ == State::WaitingActivityReply && header.sourceId == peerId_ &&
          header.payloadLength == sizeof(ActivityPayload)) {
        const ActivityPayload& rejected = *reinterpret_cast<const ActivityPayload*>(payload);
        const uint8_t expectedRules = activity_ == Activity::Battle
            ? static_cast<uint8_t>(localBattleRules_)
            : static_cast<uint8_t>(BattleRules::None);
        if (rejected.activity != static_cast<uint8_t>(activity_) ||
            rejected.rules != expectedRules) break;
        activity_ = Activity::None;
        localBattleRules_ = remoteBattleRules_ = BattleRules::None;
        setState(State::Connected, "REQUEST DECLINED.");
      }
      break;
    case TradeOfferPacket:
      if (activity_ != Activity::Trade || header.payloadLength != sizeof(TradePayload)) break;
      // Never accept a unique reward item from the wire. This protects current
      // firmware from an older or modified peer which omitted the UI check.
      if (heldItemIsTransferLocked(reinterpret_cast<const TradePayload*>(payload)->pokemon.heldItem)) {
        sendPacket(TradeReject,nullptr,0,header.transaction);
        setState(State::TradeSelecting,"THEIR POKEMON HOLDS A UNIQUE ITEM.");
        break;
      }
      remoteOffer_.valid = true; remoteOffer_.ownerId = header.sourceId;
      remoteOffer_.pokemon = reinterpret_cast<const TradePayload*>(payload)->pokemon;
      remoteOffer_.pokemon.uid = 0;
      if (localOffer_.valid)
        transactionId_ = tradeTransaction(playerId_, peerId_, localOffer_.pokemon, remoteOffer_.pokemon);
      setState(State::TradeConfirming, "REVIEW THE POKEMON TRADE.");
      break;
    case TradeConfirm:
      if (activity_ == Activity::Trade && header.transaction == transactionId_) {
        remoteTradeConfirmed_ = true; maybeBeginTradeCommit(); changed_ = true;
      }
      break;
    case TradeReject:
      resetTradeState();
      setState(State::TradeSelecting, "TRADE CANCELLED. CHOOSE AGAIN.");
      break;
    case TradeCommitRequest:
      if (activity_ == Activity::Trade && localTradeConfirmed_ && remoteTradeConfirmed_ &&
          header.transaction == transactionId_) {
        tradeCommitPending_ = true;
        pendingCommit_ = {true, transactionId_, localOffer_.pokemon.uid, remoteOffer_.pokemon};
        changed_ = true;
      }
      break;
    case TradeCommitApplied:
      if (tradeCommitOffered_ && header.transaction == transactionId_) {
        pendingCommit_ = {true, transactionId_, localOffer_.pokemon.uid, remoteOffer_.pokemon};
        tradeCommitPending_ = true; changed_ = true;
      }
      break;
    case BattleRulesChoice:
      // Reserved legacy packet. Protocol v9 carries the initiator's rule in
      // ActivityRequest so the invited peer only accepts or declines.
      break;
    case BattlePartyMember:
      if (activity_ != Activity::Battle || header.payloadLength != sizeof(BattlePartyPayload)) break;
      {
        const BattlePartyPayload& incoming = *reinterpret_cast<const BattlePartyPayload*>(payload);
        if (incoming.slot >= kPartyCapacity || !incoming.pokemon.uid || !incoming.pokemon.speciesId) break;
        remoteRoster_.members[incoming.slot] = incoming.pokemon;
        remoteRoster_.count = std::max<uint8_t>(remoteRoster_.count, static_cast<uint8_t>(incoming.slot + 1U));
        changed_ = true;
#if defined(ARDUINO_ARCH_ESP32)
        Serial.printf("[LINK] remote party slot=%u species=%u\n",
                      static_cast<unsigned>(incoming.slot),
                      static_cast<unsigned>(incoming.pokemon.speciesId));
#endif
      }
      break;
    case BattlePartyReady:
      if (activity_ != Activity::Battle || header.payloadLength != sizeof(BattleReadyPayload)) break;
      {
        const uint8_t count = reinterpret_cast<const BattleReadyPayload*>(payload)->count;
        if (!count || count > kPartyCapacity) break;
        bool complete = true;
        for (uint8_t slot = 0; slot < count; ++slot)
          complete = complete && remoteRoster_.members[slot].uid && remoteRoster_.members[slot].speciesId;
        if (!complete) {
#if defined(ARDUINO_ARCH_ESP32)
          Serial.printf("[LINK] remote READY incomplete count=%u have=%u\n",
                        static_cast<unsigned>(count),
                        static_cast<unsigned>(remoteRoster_.count));
#endif
          break;
        }
        remoteRoster_.count = count; remoteRoster_.valid = true; remoteBattleReady_ = true;
#if defined(ARDUINO_ARCH_ESP32)
        Serial.printf("[LINK] remote READY count=%u\n",
                      static_cast<unsigned>(count));
#endif
        maybeAdvanceBattlePreparation();
      }
      break;
    case BattleLead:
      if (activity_ != Activity::Battle || header.payloadLength != sizeof(BattleLeadPayload) ||
          !remoteBattleReady_) break;
      remoteLeadSlot_ = reinterpret_cast<const BattleLeadPayload*>(payload)->slot;
      if (remoteLeadSlot_ >= remoteRoster_.count || !remoteRoster_.members[remoteLeadSlot_].currentHp) break;
      remoteLeadReady_ = true; maybeLaunchBattle(); changed_ = true;
#if defined(ARDUINO_ARCH_ESP32)
      Serial.printf("[LINK] remote lead slot=%u\n",
                    static_cast<unsigned>(remoteLeadSlot_));
#endif
      break;
    case BattleStart:
      if (activity_ != Activity::Battle || header.payloadLength != sizeof(BattleStartPayload) ||
          playerId_ < peerId_) break;
      {
        const BattleStartPayload& start = *reinterpret_cast<const BattleStartPayload*>(payload);
        battleSeed_ = start.seed;
        remoteLeadSlot_ = start.hostLeadSlot;
        localLeadSlot_ = start.guestLeadSlot;
        std::memset(&pendingBattleLaunch_, 0, sizeof(pendingBattleLaunch_));
        pendingBattleLaunch_.ready = true; pendingBattleLaunch_.seed = battleSeed_;
        pendingBattleLaunch_.rules = localBattleRules_;
        pendingBattleLaunch_.localLeadSlot = localLeadSlot_;
        pendingBattleLaunch_.remoteLeadSlot = remoteLeadSlot_;
        pendingBattleLaunch_.local = localRoster_; pendingBattleLaunch_.remote = remoteRoster_;
        battleLaunchPending_ = true;
        setState(State::BattleLaunching, "STARTING LINK BATTLE...");
      }
      break;
    case ActivityCancel:
      if (connected()) {
        activity_ = Activity::None; activityAccepted_ = false;
        resetTradeState();
#if defined(ARDUINO_ARCH_ESP32)
        resetWifiBattleReadyTransmission();
#endif
        localBattleReady_ = remoteBattleReady_ = localLeadReady_ = remoteLeadReady_ = false;
        localBattleRules_ = remoteBattleRules_ = BattleRules::None;
        battleLaunchPending_ = false; std::memset(&localRoster_, 0, sizeof(localRoster_));
        std::memset(&remoteRoster_, 0, sizeof(remoteRoster_));
        std::memset(&pendingBattleLaunch_, 0, sizeof(pendingBattleLaunch_));
        battleCommandsPending_ = false; localBattleCommand_ = BattleCommand{};
        remoteBattleCommand_ = BattleCommand{};
        std::memset(&incomingBattleState_, 0, sizeof(incomingBattleState_));
        incomingBattlePokemonMask_ = incomingBattleVolatileMask_ = 0; incomingBattleCore_ = false;
        battleStateAckPending_ = false; battleStateAckTurn_ = 0;
        battleStateAckChecksum_ = publishedBattleStateChecksum_ = 0;
        clearPublishedBattleStateCache();
        setState(State::Connected, "PLAYER CANCELLED THE ACTIVITY.");
      }
      break;
    case BattleCommandPacket:
      if (activity_ != Activity::Battle || state_ != State::BattleActive ||
          header.payloadLength != sizeof(BattleCommandPayload)) break;
      {
        const BattleCommandPayload& command = *reinterpret_cast<const BattleCommandPayload*>(payload);
        if (command.kind < static_cast<uint8_t>(BattleCommandKind::Move) ||
            command.kind > static_cast<uint8_t>(BattleCommandKind::Item)) break;
        remoteBattleCommand_ = {true, command.turn, static_cast<BattleCommandKind>(command.kind), command.value};
        // A forfeit is unilateral. A tiny WAIT acknowledgement lets both
        // peers resolve the same turn immediately without requiring the
        // winning player to choose a meaningless move.
        if (remoteBattleCommand_.kind == BattleCommandKind::Forfeit && !localBattleCommand_.valid) {
          const BattleCommandPayload acknowledgement{command.turn,
              static_cast<uint8_t>(BattleCommandKind::Wait), 0};
          if (sendPacket(BattleCommandPacket, &acknowledgement, sizeof(acknowledgement), command.turn))
            localBattleCommand_ = {true, command.turn, BattleCommandKind::Wait, 0};
        }
        battleCommandsPending_ = localBattleCommand_.valid &&
            localBattleCommand_.turn == remoteBattleCommand_.turn;
        changed_ = true;
      }
      break;
    case BattleSyncPokemon:
      if (activity_ != Activity::Battle || state_ != State::BattleActive || playerId_ < peerId_ ||
          header.payloadLength != sizeof(BattleSyncPokemonPayload)) break;
      {
        const BattleSyncPokemonPayload& packet = *reinterpret_cast<const BattleSyncPokemonPayload*>(payload);
        if (packet.side > 1U || packet.slot >= kPartyCapacity) break;
        if (incomingBattleState_.turn != packet.turn) {
          std::memset(&incomingBattleState_, 0, sizeof(incomingBattleState_));
          incomingBattleState_.turn = packet.turn;
          incomingBattlePokemonMask_ = incomingBattleVolatileMask_ = 0; incomingBattleCore_ = false;
        }
        (packet.side ? incomingBattleState_.guestParty : incomingBattleState_.hostParty)[packet.slot] = packet.pokemon;
        incomingBattlePokemonMask_ |= static_cast<uint16_t>(1U << (packet.slot + packet.side * kPartyCapacity));
      }
      break;
    case BattleSyncVolatile:
      if (activity_ != Activity::Battle || state_ != State::BattleActive || playerId_ < peerId_ ||
          header.payloadLength != sizeof(BattleSyncVolatilePayload)) break;
      {
        const BattleSyncVolatilePayload& packet = *reinterpret_cast<const BattleSyncVolatilePayload*>(payload);
        if (packet.side > 1U || incomingBattleState_.turn != packet.turn) break;
        if (packet.slot >= kPartyCapacity) break;
        (packet.side ? incomingBattleState_.guestVolatiles : incomingBattleState_.hostVolatiles)[packet.slot] = packet.state;
        (packet.side ? incomingBattleState_.guestMoveEffects : incomingBattleState_.hostMoveEffects)[packet.slot] = packet.moveEffects;
        (packet.side ? incomingBattleState_.guestGen4Effects : incomingBattleState_.hostGen4Effects)[packet.slot] = packet.gen4Effects;
        incomingBattleVolatileMask_ |= static_cast<uint16_t>(1U << (packet.slot + packet.side * kPartyCapacity));
      }
      break;
    case BattleSyncCore:
      if (activity_ != Activity::Battle || state_ != State::BattleActive || playerId_ < peerId_ ||
          header.payloadLength != sizeof(BattleSyncCorePayload)) break;
      {
        const BattleSyncCorePayload& packet = *reinterpret_cast<const BattleSyncCorePayload*>(payload);
        if (incomingBattleState_.turn != packet.turn || !packet.hostCount || !packet.guestCount ||
            packet.hostCount > kPartyCapacity || packet.guestCount > kPartyCapacity) break;
        incomingBattleState_.snapshotChecksum = packet.snapshotChecksum;
        incomingBattleState_.active = packet.active != 0;
        incomingBattleState_.hostOutcome = static_cast<BattleOutcome>(packet.outcome);
        incomingBattleState_.rngState = packet.rngState;
        incomingBattleState_.weather = static_cast<BattleWeather>(packet.weather);
        incomingBattleState_.weatherTurns = packet.weatherTurns;
        incomingBattleState_.hostSafeguardTurns = packet.hostSafeguardTurns;
        incomingBattleState_.guestSafeguardTurns = packet.guestSafeguardTurns;
        incomingBattleState_.hostReflectTurns = packet.hostReflectTurns;
        incomingBattleState_.guestReflectTurns = packet.guestReflectTurns;
        incomingBattleState_.hostLightScreenTurns = packet.hostLightScreenTurns;
        incomingBattleState_.guestLightScreenTurns = packet.guestLightScreenTurns;
        incomingBattleState_.hostMistTurns = packet.hostMistTurns;
        incomingBattleState_.guestMistTurns = packet.guestMistTurns;
        incomingBattleState_.hostProtectChain = packet.hostProtectChain;
        incomingBattleState_.guestProtectChain = packet.guestProtectChain;
        incomingBattleState_.hostEndureThisTurn = packet.hostEndureThisTurn != 0;
        incomingBattleState_.guestEndureThisTurn = packet.guestEndureThisTurn != 0;
        incomingBattleState_.hostNightmare = packet.hostNightmare != 0;
        incomingBattleState_.guestNightmare = packet.guestNightmare != 0;
        incomingBattleState_.hostZMoveUsed = packet.hostZMoveUsed != 0;
        incomingBattleState_.guestZMoveUsed = packet.guestZMoveUsed != 0;
        incomingBattleState_.hostSpikesLayers = packet.hostSpikesLayers;
        incomingBattleState_.guestSpikesLayers = packet.guestSpikesLayers;
        incomingBattleState_.hostToxicSpikesLayers = packet.hostToxicSpikesLayers;
        incomingBattleState_.guestToxicSpikesLayers = packet.guestToxicSpikesLayers;
        incomingBattleState_.hostStealthRock = packet.hostStealthRock != 0;
        incomingBattleState_.guestStealthRock = packet.guestStealthRock != 0;
        incomingBattleState_.hostTailwindTurns = packet.hostTailwindTurns;
        incomingBattleState_.guestTailwindTurns = packet.guestTailwindTurns;
        incomingBattleState_.hostLuckyChantTurns = packet.hostLuckyChantTurns;
        incomingBattleState_.guestLuckyChantTurns = packet.guestLuckyChantTurns;
        incomingBattleState_.gravityTurns = packet.gravityTurns;
        incomingBattleState_.trickRoomTurns = packet.trickRoomTurns;
        incomingBattleState_.lastMoveUsedGlobally = packet.lastMoveUsedGlobally;
        incomingBattleState_.delayedToHost = packet.delayedToHost;
        incomingBattleState_.delayedToGuest = packet.delayedToGuest;
        incomingBattleState_.hostCount = packet.hostCount; incomingBattleState_.guestCount = packet.guestCount;
        incomingBattleState_.hostActiveRosterSlot = packet.hostActiveRosterSlot;
        incomingBattleState_.guestActiveRosterSlot = packet.guestActiveRosterSlot;
        incomingBattleCore_ = true;
        const uint16_t requiredPokemonMask = static_cast<uint16_t>(
            ((1U << packet.hostCount) - 1U) | (((1U << packet.guestCount) - 1U) << kPartyCapacity));
        incomingBattleState_.ready = (incomingBattlePokemonMask_ & requiredPokemonMask) == requiredPokemonMask &&
                                     (incomingBattleVolatileMask_ & requiredPokemonMask) == requiredPokemonMask;
        if (incomingBattleState_.ready) {
          const uint32_t actualChecksum =
              battleStateSyncChecksum(incomingBattleState_);
          if (!packet.snapshotChecksum ||
              actualChecksum != packet.snapshotChecksum) {
            incomingBattleState_.ready = false;
            incomingBattleCore_ = false;
            std::printf("[LINK] rejected mixed snapshot turn=%u checksum=%08lx/%08lx\n",
                        static_cast<unsigned>(packet.turn),
                        static_cast<unsigned long>(actualChecksum),
                        static_cast<unsigned long>(packet.snapshotChecksum));
          }
        }
        changed_ = true;
      }
      break;
    case BattleSyncAck:
      if (activity_ != Activity::Battle || state_ != State::BattleActive || playerId_ > peerId_ ||
          header.payloadLength != sizeof(BattleSyncAckPayload)) break;
      battleStateAckTurn_ =
          reinterpret_cast<const BattleSyncAckPayload*>(payload)->turn;
      battleStateAckChecksum_ =
          reinterpret_cast<const BattleSyncAckPayload*>(payload)->snapshotChecksum;
      battleStateAckPending_ = true;
      changed_ = true;
      break;
    default: break;
  }
}

void Multiplayer::disconnect() {
  if (transportConnected_ && peerId_) sendPacket(SessionClose);
  closeTransportLink();
  resetSession(enabled_);
  if (enabled_) setState(State::Advertising, transportKind_ == TransportKind::Wifi
      ? "ONLINE SERVER CONNECTED. FIND A PLAYER."
      : "READY. FIND A NEARBY PLAYER.");
}

void Multiplayer::closeTransportLink() {
  // Mark the close before invoking NimBLE: an outbound disconnect callback may
  // run synchronously from disconnect(). Its later event must not be mistaken
  // for the peer disappearing and force the UI through the recovery path.
  if (transportConnected_) localTransportClosePending_ = true;
#if defined(ARDUINO_ARCH_ESP32)
  if (transportKind_ == TransportKind::Wifi) {
    if (gOnlineSessionReady)
      sendOnlineFrame(PokegochiOnlineProtocol::MessageType::CloseSession,
                      playerId_, gOnlinePeerId, gOnlineSessionId);
    gOnlineSessionReady = false;
    gOnlineSessionId = 0;
    gOnlinePeerId = 0;
  } else {
    if (gClient && gClient->isConnected()) {
      Serial.println("[BLE] closing outbound physical link");
      gClient->disconnect();
    } else if (gServer && gInboundConnectionHandle != BLE_HS_CONN_HANDLE_NONE) {
      Serial.println("[BLE] closing inbound physical link");
      gServer->disconnect(gInboundConnectionHandle);
    }
  }
#endif
  transportConnected_ = false;
  outbound_ = false;
#if defined(ARDUINO_ARCH_ESP32)
  if (transportKind_ == TransportKind::Ble) gOutboundConnectPending = false;
#endif
}

void Multiplayer::clearPublishedBattleStateCache() {
  delete publishedBattleState_;
  publishedBattleState_ = nullptr;
}

void Multiplayer::resetSession(bool) {
  onlinePeerSearchPending_ = false;
  peerId_ = invitationToken_ = 0; activity_ = Activity::None;
  activityAccepted_ = false;
  resetTradeState();
#if defined(ARDUINO_ARCH_ESP32)
  resetWifiBattleReadyTransmission();
#endif
  localBattleReady_ = remoteBattleReady_ = localLeadReady_ = remoteLeadReady_ = false;
  localBattleRules_ = remoteBattleRules_ = BattleRules::None;
  battleLaunchPending_ = false; std::memset(&localRoster_, 0, sizeof(localRoster_));
  std::memset(&remoteRoster_, 0, sizeof(remoteRoster_));
  std::memset(&pendingBattleLaunch_, 0, sizeof(pendingBattleLaunch_));
  localLeadSlot_ = remoteLeadSlot_ = 0; battleSeed_ = 0;
  battleCommandsPending_ = false; localBattleCommand_ = BattleCommand{};
  remoteBattleCommand_ = BattleCommand{};
  std::memset(&incomingBattleState_, 0, sizeof(incomingBattleState_));
  incomingBattlePokemonMask_ = incomingBattleVolatileMask_ = 0; incomingBattleCore_ = false;
  battleStateAckPending_ = false; battleStateAckTurn_ = 0;
  battleStateAckChecksum_ = publishedBattleStateChecksum_ = 0;
  clearPublishedBattleStateCache();
}

void Multiplayer::resetTradeState() {
  transactionId_ = 0;
  localTradeConfirmed_ = false;
  remoteTradeConfirmed_ = false;
  tradeCommitOffered_ = false;
  tradeCommitPending_ = false;
  localOffer_ = {};
  remoteOffer_ = {};
  pendingCommit_ = {};
}

bool Multiplayer::requestActivity(Activity activity) {
  if (!connected() || !outbound_ || state_ != State::Connected ||
      (activity != Activity::Battle && activity != Activity::Trade)) return false;
  resetTradeState();
  if (activity == Activity::Battle) {
    localBattleRules_ = remoteBattleRules_ = BattleRules::None;
    localBattleReady_ = remoteBattleReady_ = false;
  }
  activity_ = activity; activityAccepted_ = false;
  if (activity == Activity::Battle) {
    // Keep this choice local until a complete NORMAL/BALANCED proposal exists.
    setState(State::BattlePreparing, "CHOOSE NORMAL OR BALANCED.");
    return true;
  }
  const ActivityPayload payload{static_cast<uint8_t>(activity),
                                static_cast<uint8_t>(BattleRules::None)};
  if (!sendPacket(ActivityRequest, &payload, sizeof(payload))) return false;
  setState(State::WaitingActivityReply, "WAITING FOR THE OTHER PLAYER..."); return true;
}

void Multiplayer::answerActivity(bool accept) {
  if (outbound_ || state_ != State::ConfirmIncomingActivity) return;
  const ActivityPayload payload{static_cast<uint8_t>(activity_),
      static_cast<uint8_t>(activity_ == Activity::Battle ? localBattleRules_ : BattleRules::None)};
  sendPacket(accept ? ActivityAccept : ActivityReject, &payload, sizeof(payload));
  if (!accept) { activity_ = Activity::None; setState(State::Connected, "REQUEST DECLINED."); return; }
  activityAccepted_ = true;
  setState(activity_ == Activity::Trade ? State::TradeSelecting : State::BattlePreparing,
           activity_ == Activity::Trade ? "CHOOSE A POKEMON IN THE BOX." : "PREPARE YOUR PARTY, THEN START.");
}

void Multiplayer::clearActivity() {
  const bool proposalNotSent = outbound_ && activity_ == Activity::Battle &&
      state_ == State::BattlePreparing && !activityAccepted_ &&
      localBattleRules_ == BattleRules::None;
  if (connected() && activity_ != Activity::None && !proposalNotSent) sendPacket(ActivityCancel);
  activity_ = Activity::None; activityAccepted_ = false;
  resetTradeState();
#if defined(ARDUINO_ARCH_ESP32)
  resetWifiBattleReadyTransmission();
#endif
  localBattleReady_ = remoteBattleReady_ = localLeadReady_ = remoteLeadReady_ = false;
  localBattleRules_ = remoteBattleRules_ = BattleRules::None;
  battleLaunchPending_ = false; std::memset(&localRoster_, 0, sizeof(localRoster_));
  std::memset(&remoteRoster_, 0, sizeof(remoteRoster_));
  std::memset(&pendingBattleLaunch_, 0, sizeof(pendingBattleLaunch_));
  battleCommandsPending_ = false; localBattleCommand_ = BattleCommand{};
  remoteBattleCommand_ = BattleCommand{};
  std::memset(&incomingBattleState_, 0, sizeof(incomingBattleState_));
  incomingBattlePokemonMask_ = incomingBattleVolatileMask_ = 0; incomingBattleCore_ = false;
  battleStateAckPending_ = false; battleStateAckTurn_ = 0;
  battleStateAckChecksum_ = publishedBattleStateChecksum_ = 0;
  clearPublishedBattleStateCache();
  if (connected()) setState(State::Connected, "PLAYER CONNECTED.");
}

bool Multiplayer::setBattleReady(const PokemonCollection& collection) {
#if defined(ARDUINO_ARCH_ESP32)
  if (transportKind_ == TransportKind::Wifi &&
      wifiBattleReadyTransmissionPending_)
    return true;
#endif
  if (!connected() || !activityAccepted_ || activity_ != Activity::Battle ||
      (state_ != State::BattlePreparing && state_ != State::BattleWaitingReady)) return false;
  if (!battleRulesReady()) {
    setState(State::BattlePreparing, "CHOOSE NORMAL OR BALANCED FIRST.");
    return false;
  }
  BattleRoster roster{};
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) {
    const OwnedPokemon* pokemon = CollectionLogic::find(collection, collection.party[slot]);
    if (!pokemon) continue;
    // Multiplayer uses disposable snapshots. Base-game HP, status and PP are
    // never healed or consumed by the link battle.
    OwnedPokemon prepared = *pokemon;
    if (localBattleRules_ == BattleRules::Balanced) {
      prepared.level = 70;
      CollectionLogic::refreshDerivedStats(prepared, false);
    }
    prepared.currentHp = prepared.maximumHp;
    prepared.status = StatusCondition::None;
    prepared.recoverySecondsRemaining = 0;
    for (uint8_t move = 0; move < kMoveSlots; ++move)
      prepared.movePp[move] = CollectionLogic::maximumMovePp(prepared, move);
    roster.members[roster.count++] = prepared;
  }
  if (!roster.count) { setState(State::BattlePreparing, "NO POKEMON IN PARTY."); return false; }
  roster.valid = true;
  localRoster_ = roster; localLeadReady_ = false;
#if defined(ARDUINO_ARCH_ESP32)
  if (transportKind_ == TransportKind::Wifi) {
    // Accept the UI commit immediately, but publish the snapshot one packet
    // at a time. sendOnlineFrame() may retain one whole Relay frame when lwIP
    // returns EAGAIN; sending the next member in the same call used to fail,
    // roll READY back and leave the server with an incomplete roster.
    localBattleReady_ = false;
    wifiBattleReadyTransmissionPending_ = true;
    wifiBattleReadyNextSlot_ = 0;
    setState(State::BattleWaitingReady, "SENDING PARTY TO PLAYER...");
    Serial.printf("[WIFI] READY transaction started members=%u\n",
                  static_cast<unsigned>(roster.count));
    continueWifiBattleReadyTransmission();
    return true;
  }
#endif
  localBattleReady_ = true;
  for (uint8_t slot = 0; slot < roster.count; ++slot) {
    const BattlePartyPayload payload{slot, roster.members[slot]};
    if (!sendPacket(BattlePartyMember, &payload, sizeof(payload))) {
      localBattleReady_ = false; std::memset(&localRoster_, 0, sizeof(localRoster_));
      setState(State::BattlePreparing, "PARTY COULD NOT BE SENT."); return false;
    }
  }
  const BattleReadyPayload ready{roster.count};
  if (!sendPacket(BattlePartyReady, &ready, sizeof(ready))) {
    localBattleReady_ = false; std::memset(&localRoster_, 0, sizeof(localRoster_));
    setState(State::BattlePreparing, "PARTY COULD NOT BE SENT."); return false;
  }
  maybeAdvanceBattlePreparation();
  return true;
}

#if defined(ARDUINO_ARCH_ESP32)
void Multiplayer::resetWifiBattleReadyTransmission() {
  wifiBattleReadyTransmissionPending_ = false;
  wifiBattleReadyNextSlot_ = 0;
}

void Multiplayer::continueWifiBattleReadyTransmission() {
  if (!wifiBattleReadyTransmissionPending_ ||
      transportKind_ != TransportKind::Wifi) return;
  if (!connected() || !activityAccepted_ || activity_ != Activity::Battle ||
      !localRoster_.valid || !localRoster_.count) {
    resetWifiBattleReadyTransmission();
    return;
  }
  // The retained frame remains the sole owner of the TCP stream until every
  // byte is accepted.  Waiting here is non-blocking and keeps UI/touch alive.
  if (gOnlinePendingLength) return;

  if (wifiBattleReadyNextSlot_ < localRoster_.count) {
    const uint8_t slot = wifiBattleReadyNextSlot_;
    const BattlePartyPayload payload{slot, localRoster_.members[slot]};
    if (!sendPacket(BattlePartyMember, &payload, sizeof(payload))) {
      if (gOnlinePendingLength) return;
      resetWifiBattleReadyTransmission();
      std::memset(&localRoster_, 0, sizeof(localRoster_));
      setState(State::BattlePreparing, "PARTY COULD NOT BE SENT.");
      return;
    }
    ++wifiBattleReadyNextSlot_;
    Serial.printf("[WIFI] READY member queued slot=%u/%u\n",
                  static_cast<unsigned>(wifiBattleReadyNextSlot_),
                  static_cast<unsigned>(localRoster_.count));
    return;
  }

  const BattleReadyPayload ready{localRoster_.count};
  if (!sendPacket(BattlePartyReady, &ready, sizeof(ready))) {
    if (gOnlinePendingLength) return;
    resetWifiBattleReadyTransmission();
    std::memset(&localRoster_, 0, sizeof(localRoster_));
    setState(State::BattlePreparing, "PARTY COULD NOT BE SENT.");
    return;
  }
  resetWifiBattleReadyTransmission();
  localBattleReady_ = true;
  Serial.println("[WIFI] READY transaction complete");
  maybeAdvanceBattlePreparation();
}
#endif

bool Multiplayer::chooseBattleRules(BattleRules rules) {
  if (!connected() || !outbound_ || activityAccepted_ || activity_ != Activity::Battle ||
      localBattleReady_ || state_ != State::BattlePreparing ||
      (rules != BattleRules::Normal && rules != BattleRules::Balanced)) return false;
  localBattleRules_ = remoteBattleRules_ = rules;
  const ActivityPayload payload{static_cast<uint8_t>(Activity::Battle),
                                static_cast<uint8_t>(rules)};
  if (!sendPacket(ActivityRequest, &payload, sizeof(payload))) {
    localBattleRules_ = remoteBattleRules_ = BattleRules::None;
    return false;
  }
  setState(State::WaitingActivityReply, "WAITING FOR THE OTHER PLAYER...");
  return true;
}

Multiplayer::BattleRules Multiplayer::battleRules() const {
  return battleRulesReady() ? localBattleRules_ : BattleRules::None;
}

bool Multiplayer::battleRulesReady() const {
  return localBattleRules_ != BattleRules::None && localBattleRules_ == remoteBattleRules_;
}

bool Multiplayer::localBattleReady() const { return localBattleReady_; }
bool Multiplayer::remoteBattleReady() const { return remoteBattleReady_; }
const Multiplayer::BattleRoster& Multiplayer::localBattleRoster() const { return localRoster_; }
const Multiplayer::BattleRoster& Multiplayer::remoteBattleRoster() const { return remoteRoster_; }

void Multiplayer::maybeAdvanceBattlePreparation() {
  if (localBattleReady_ && remoteBattleReady_)
    setState(State::BattleChoosingLead, "CHOOSE YOUR FIRST POKEMON.");
  else if (localBattleReady_)
    // READY is the local commit boundary. Only after this player has sent the
    // Party snapshot may the UI leave preparation and lock OPEN BOX.
    setState(State::BattleWaitingReady, "PARTY LOCKED. WAITING FOR PLAYER...");
  else
    // A remote READY is information, not permission to commit this player's
    // Party. Keep preparation/Box available until local READY is pressed.
    setState(State::BattlePreparing, remoteBattleReady_
        ? "OTHER PLAYER READY. PREPARE YOUR PARTY."
        : "PREPARE YOUR PARTY, THEN START.");
}

bool Multiplayer::submitBattleLead(uint8_t partySlot) {
  if (state_ != State::BattleChoosingLead || !localRoster_.valid || partySlot >= localRoster_.count ||
      !localRoster_.members[partySlot].currentHp) return false;
  const BattleLeadPayload payload{partySlot};
  if (!sendPacket(BattleLead, &payload, sizeof(payload))) return false;
  localLeadSlot_ = partySlot; localLeadReady_ = true;
  setState(State::BattleChoosingLead, remoteLeadReady_ ? "STARTING LINK BATTLE..." :
           "WAITING FOR THEIR FIRST POKEMON...");
  maybeLaunchBattle(); return true;
}

void Multiplayer::maybeLaunchBattle() {
  if (!localLeadReady_ || !remoteLeadReady_ || battleLaunchPending_) return;
  if (playerId_ > peerId_) return;  // The lower ID is the deterministic host.
#if defined(ARDUINO_ARCH_ESP32)
  battleSeed_ = static_cast<uint32_t>(esp_random()) ^ playerId_ ^ (peerId_ << 1U);
#else
  battleSeed_ = playerId_ ^ (peerId_ << 1U) ^ 0x50475056U;
#endif
  if (!battleSeed_) battleSeed_ = 1;
  const BattleStartPayload start{battleSeed_, localLeadSlot_, remoteLeadSlot_};
  if (!sendPacket(BattleStart, &start, sizeof(start))) {
    setState(State::BattleChoosingLead, "BATTLE START COULD NOT BE SENT."); return;
  }
  std::memset(&pendingBattleLaunch_, 0, sizeof(pendingBattleLaunch_));
  pendingBattleLaunch_.ready = true; pendingBattleLaunch_.seed = battleSeed_;
  pendingBattleLaunch_.rules = localBattleRules_;
  pendingBattleLaunch_.localLeadSlot = localLeadSlot_;
  pendingBattleLaunch_.remoteLeadSlot = remoteLeadSlot_;
  pendingBattleLaunch_.local = localRoster_; pendingBattleLaunch_.remote = remoteRoster_;
  battleLaunchPending_ = true;
  setState(State::BattleLaunching, "STARTING LINK BATTLE...");
}

bool Multiplayer::takeBattleLaunch(BattleLaunch& launch) {
  if (!battleLaunchPending_ || !pendingBattleLaunch_.ready) return false;
  launch = pendingBattleLaunch_; battleLaunchPending_ = false;
  setState(State::BattleActive, "LINK BATTLE IN PROGRESS.");
  return true;
}

bool Multiplayer::submitBattleCommand(BattleCommandKind kind, uint8_t value, uint16_t turn) {
  // FORFEIT is not an ordinary turn choice. It must be able to replace a
  // command that is still waiting for the peer, otherwise a stalled link
  // makes RUN unusable at the exact time the player needs an escape hatch.
  const bool replacingPendingCommand = kind == BattleCommandKind::Forfeit &&
      localBattleCommand_.valid;
  if (!connected() || activity_ != Activity::Battle || state_ != State::BattleActive ||
      kind == BattleCommandKind::None ||
      (localBattleCommand_.valid && !replacingPendingCommand)) return false;
  const BattleCommandPayload payload{turn, static_cast<uint8_t>(kind), value};
  if (!sendPacket(BattleCommandPacket, &payload, sizeof(payload), turn)) return false;
  localBattleCommand_ = {true, turn, kind, value};
  // Surrender is unilateral. Resolve it locally as soon as the FORFEIT frame
  // has entered the transport queue; the peer independently pairs the same
  // frame with WAIT when it arrives. The conceding player never depends on a
  // round trip or on the opponent pressing RUN too.
  if (kind == BattleCommandKind::Forfeit &&
      (!remoteBattleCommand_.valid || remoteBattleCommand_.turn != turn))
    remoteBattleCommand_ = {true, turn, BattleCommandKind::Wait, 0};
  battleCommandsPending_ = remoteBattleCommand_.valid && remoteBattleCommand_.turn == turn;
  std::snprintf(status_, sizeof(status_), "WAITING FOR PLAYER:%05lu...", static_cast<unsigned long>(peerId_));
  changed_ = true; return true;
}

bool Multiplayer::localBattleCommandPending() const {
  return localBattleCommand_.valid;
}

bool Multiplayer::takeBattleCommands(BattleCommand& local, BattleCommand& remote) {
  if (!battleCommandsPending_ || !localBattleCommand_.valid || !remoteBattleCommand_.valid ||
      localBattleCommand_.turn != remoteBattleCommand_.turn) return false;
  local = localBattleCommand_; remote = remoteBattleCommand_;
  localBattleCommand_ = BattleCommand{}; remoteBattleCommand_ = BattleCommand{};
  battleCommandsPending_ = false;
  std::strncpy(status_, "LINK BATTLE IN PROGRESS.", sizeof(status_) - 1U);
  changed_ = true; return true;
}

bool Multiplayer::isBattleHost() const {
  return connected() && activity_ == Activity::Battle && playerId_ < peerId_;
}

bool Multiplayer::publishBattleState(const BattleState& battle, const PokemonCollection& collection,
                                     const uint8_t remoteRosterSlotAtIndex[kPartyCapacity]) {
  if (!isBattleHost() || state_ != State::BattleActive || battle.kind != BattleKind::Pvp) return false;
#if !defined(ARDUINO_ARCH_ESP32)
  // A canonical three-vs-three snapshot is thirteen packets. Reserving the
  // complete batch before its first packet prevents Android from filling its
  // native queue with two-and-a-half retries and then publishing a partial
  // state that can never be acknowledged after a faint.
  const uint8_t snapshotPackets = static_cast<uint8_t>(
      localRoster_.count + battle.opponentCount + localRoster_.count +
      battle.opponentCount + 1U);
  if (snapshotPackets > kOutgoingPacketCapacity - outgoingPacketCount_) {
    std::printf("[LINK] snapshot queue busy turn=%u queued=%u need=%u\n",
                static_cast<unsigned>(battle.turn),
                static_cast<unsigned>(outgoingPacketCount_),
                static_cast<unsigned>(snapshotPackets));
    return false;
  }
#endif
  BattleStateSync candidate{};
  if (!publishedBattleState_) {
  BattleStateSync& sync = candidate;
  sync.ready = true;
  sync.turn = battle.turn;
  sync.active = battle.active;
  sync.hostOutcome = battle.outcome;
  sync.rngState = battle.rngState;
  sync.weather = battle.weather;
  sync.weatherTurns = battle.weatherTurns;
  sync.hostSafeguardTurns = battle.playerSafeguardTurns;
  sync.guestSafeguardTurns = battle.opponentSafeguardTurns;
  sync.hostReflectTurns = battle.playerReflectTurns;
  sync.guestReflectTurns = battle.opponentReflectTurns;
  sync.hostLightScreenTurns = battle.playerLightScreenTurns;
  sync.guestLightScreenTurns = battle.opponentLightScreenTurns;
  sync.hostMistTurns = battle.playerMistTurns;
  sync.guestMistTurns = battle.opponentMistTurns;
  sync.hostProtectChain = battle.playerProtectChain;
  sync.guestProtectChain = battle.opponentProtectChain;
  sync.hostEndureThisTurn = battle.playerEndureThisTurn;
  sync.guestEndureThisTurn = battle.opponentEndureThisTurn;
  sync.hostNightmare = battle.playerNightmare;
  sync.guestNightmare = battle.opponentNightmares[battle.opponentIndex];
  sync.hostZMoveUsed = battle.playerZMoveUsed;
  sync.guestZMoveUsed = battle.opponentZMoveUsed;
  sync.hostSpikesLayers = battle.playerSpikesLayers;
  sync.guestSpikesLayers = battle.opponentSpikesLayers;
  sync.hostToxicSpikesLayers = battle.playerToxicSpikesLayers;
  sync.guestToxicSpikesLayers = battle.opponentToxicSpikesLayers;
  sync.hostStealthRock = battle.playerStealthRock;
  sync.guestStealthRock = battle.opponentStealthRock;
  sync.hostTailwindTurns = battle.playerTailwindTurns;
  sync.guestTailwindTurns = battle.opponentTailwindTurns;
  sync.hostLuckyChantTurns = battle.playerLuckyChantTurns;
  sync.guestLuckyChantTurns = battle.opponentLuckyChantTurns;
  sync.gravityTurns = battle.gravityTurns;
  sync.trickRoomTurns = battle.trickRoomTurns;
  sync.lastMoveUsedGlobally = battle.lastMoveUsedGlobally;
  sync.delayedToHost = battle.delayedToPlayer;
  sync.delayedToGuest = battle.delayedToOpponent;
  sync.hostCount = localRoster_.count;
  sync.guestCount = battle.opponentCount;

  uint8_t hostActiveSlot = 0xFFU;
  for (uint8_t slot = 0; slot < localRoster_.count; ++slot) {
    const OwnedPokemon* pokemon = CollectionLogic::find(collection, localRoster_.members[slot].uid);
    if (!pokemon) return false;
    sync.hostParty[slot] = *pokemon;
    if (pokemon->uid == battle.playerUid) hostActiveSlot = slot;
  }
  uint8_t guestActiveSlot = 0xFFU;
  for (uint8_t index = 0; index < battle.opponentCount; ++index) {
    const uint8_t rosterSlot = remoteRosterSlotAtIndex[index];
    if (rosterSlot >= remoteRoster_.count) return false;
    sync.guestParty[rosterSlot] = battle.opponents[index];
    if (index == battle.opponentIndex) guestActiveSlot = rosterSlot;
  }
  if (hostActiveSlot >= localRoster_.count || guestActiveSlot >= remoteRoster_.count) return false;
  sync.hostActiveRosterSlot = hostActiveSlot;
  sync.guestActiveRosterSlot = guestActiveSlot;
  for (uint8_t slot = 0; slot < localRoster_.count; ++slot) {
    CombatVolatile state{};
    DedicatedMoveEffectState moveEffects{};
    Gen4BattlerEffectState gen4Effects{};
    if (slot == hostActiveSlot) state = battle.playerVolatile;
    else {
      const uint32_t uid = localRoster_.members[slot].uid;
      for (const BattleHeldItemState& held : battle.playerHeldItems) if (held.pokemonUid == uid) {
        state.heldItemOverride = held.heldItemOverride;
        state.hasHeldItemOverride = held.hasHeldItemOverride;
        state.heldItemSuppressed = held.heldItemSuppressed;
        break;
      }
    }
    if (slot == hostActiveSlot) moveEffects = battle.playerMoveEffects;
    if (slot == hostActiveSlot) gen4Effects = battle.playerGen4Effects;
    sync.hostVolatiles[slot] = state;
    sync.hostMoveEffects[slot] = moveEffects;
    sync.hostGen4Effects[slot] = gen4Effects;
  }
  for (uint8_t index = 0; index < battle.opponentCount; ++index) {
    const uint8_t rosterSlot = remoteRosterSlotAtIndex[index];
    sync.guestVolatiles[rosterSlot] = battle.opponentVolatiles[index];
    sync.guestMoveEffects[rosterSlot] = battle.opponentMoveEffects[index];
    sync.guestGen4Effects[rosterSlot] = battle.opponentGen4Effects[index];
  }

  sync.snapshotChecksum = battleStateSyncChecksum(sync);
  BattleStateSync* frozen = new (std::nothrow) BattleStateSync{};
  if (!frozen) return false;
  *frozen = sync;
  publishedBattleState_ = frozen;
  publishedBattleStateChecksum_ = frozen->snapshotChecksum;
  }
  const BattleStateSync& sync = *publishedBattleState_;
  for (uint8_t slot = 0; slot < sync.hostCount; ++slot) {
    const BattleSyncPokemonPayload packet{
        sync.turn, 0U, slot, sync.hostParty[slot]};
    if (!sendPacket(BattleSyncPokemon, &packet, sizeof(packet), sync.turn)) return false;
  }
  for (uint8_t slot = 0; slot < sync.guestCount; ++slot) {
    const BattleSyncPokemonPayload packet{
        sync.turn, 1U, slot, sync.guestParty[slot]};
    if (!sendPacket(BattleSyncPokemon, &packet, sizeof(packet), sync.turn)) return false;
  }
  for (uint8_t slot = 0; slot < sync.hostCount; ++slot) {
    const BattleSyncVolatilePayload packet{
        sync.turn, 0U, slot, sync.hostVolatiles[slot],
        sync.hostMoveEffects[slot], sync.hostGen4Effects[slot]};
    if (!sendPacket(BattleSyncVolatile, &packet, sizeof(packet), sync.turn)) return false;
  }
  for (uint8_t slot = 0; slot < sync.guestCount; ++slot) {
    const BattleSyncVolatilePayload packet{
        sync.turn, 1U, slot, sync.guestVolatiles[slot],
        sync.guestMoveEffects[slot], sync.guestGen4Effects[slot]};
    if (!sendPacket(BattleSyncVolatile, &packet, sizeof(packet), sync.turn)) return false;
  }
  const BattleSyncCorePayload core =
      battleSyncCorePayload(sync, sync.snapshotChecksum);
  return sendPacket(BattleSyncCore, &core, sizeof(core), sync.turn);
}

bool Multiplayer::takeBattleState(BattleStateSync& sync) {
  if (!incomingBattleCore_ || !incomingBattleState_.ready) return false;
  std::memcpy(&sync, &incomingBattleState_, sizeof(sync));
  std::memset(&incomingBattleState_, 0, sizeof(incomingBattleState_));
  incomingBattlePokemonMask_ = incomingBattleVolatileMask_ = 0; incomingBattleCore_ = false;
  return true;
}

bool Multiplayer::acknowledgeBattleState(uint16_t turn,
                                         uint32_t snapshotChecksum) {
  if (!connected() || activity_ != Activity::Battle || state_ != State::BattleActive ||
      playerId_ < peerId_ || !snapshotChecksum) return false;
  const BattleSyncAckPayload acknowledgement{turn, snapshotChecksum};
  return sendPacket(BattleSyncAck, &acknowledgement,
                    sizeof(acknowledgement), turn);
}

bool Multiplayer::takeBattleStateAcknowledgement(
    uint16_t& turn, uint32_t& snapshotChecksum) {
  if (!battleStateAckPending_) return false;
  turn = battleStateAckTurn_;
  snapshotChecksum = battleStateAckChecksum_;
  battleStateAckPending_ = false;
  if (snapshotChecksum == publishedBattleStateChecksum_)
    clearPublishedBattleStateCache();
  return true;
}

uint32_t Multiplayer::publishedBattleStateChecksum() const {
  return publishedBattleStateChecksum_;
}

bool Multiplayer::submitTradeOffer(const OwnedPokemon& pokemon) {
  if (!connected() || activity_ != Activity::Trade || pokemon.uid == 0) return false;
  if (heldItemIsTransferLocked(pokemon.heldItem)) {
    setState(State::TradeSelecting,"REMOVE THE UNIQUE ITEM BEFORE TRADING.");
    return false;
  }
  // A new offer always starts a new two-phase transaction.  In particular,
  // this clears the coordinator latch left by an older beta after its first
  // successful trade, which otherwise made every later trade wait forever.
  tradeCommitOffered_ = false;
  tradeCommitPending_ = false;
  pendingCommit_ = {};
  localOffer_.valid = true; localOffer_.ownerId = playerId_; localOffer_.pokemon = pokemon;
  localTradeConfirmed_ = remoteTradeConfirmed_ = false;
  transactionId_ = remoteOffer_.valid
      ? tradeTransaction(playerId_, peerId_, localOffer_.pokemon, remoteOffer_.pokemon) : 0;
  const TradePayload payload{pokemon};
  if (!sendPacket(TradeOfferPacket, &payload, sizeof(payload), 0)) return false;
  setState(remoteOffer_.valid ? State::TradeConfirming : State::TradeSelecting,
           remoteOffer_.valid ? "REVIEW THE POKEMON TRADE." : "WAITING FOR THEIR POKEMON...");
  return true;
}

bool Multiplayer::setTradeConfirmation(bool accept) {
  if (activity_ != Activity::Trade || !localOffer_.valid || !remoteOffer_.valid) return false;
  if (!accept) {
    sendPacket(TradeReject, nullptr, 0, transactionId_);
    resetTradeState();
    setState(State::TradeSelecting, "TRADE CANCELLED. CHOOSE AGAIN."); return true;
  }
  localTradeConfirmed_ = true;
  sendPacket(TradeConfirm, nullptr, 0, transactionId_);
  maybeBeginTradeCommit();
  setState(State::TradeConfirming, "WAITING FOR BOTH PLAYERS...");
  return true;
}

void Multiplayer::maybeBeginTradeCommit() {
  if (!localTradeConfirmed_ || !remoteTradeConfirmed_ || tradeCommitOffered_) return;
  // The lower board ID coordinates the two-phase exchange. It applies only
  // after the peer has durably applied and acknowledged its half.
  if (playerId_ < peerId_) {
    tradeCommitOffered_ = sendPacket(TradeCommitRequest, nullptr, 0, transactionId_);
  }
}

bool Multiplayer::takeTradeCommit(TradeCommit& commit) {
  if (!tradeCommitPending_ || !pendingCommit_.ready) return false;
  commit = pendingCommit_; tradeCommitPending_ = false; return true;
}

void Multiplayer::completeTradeCommit(bool success) {
  if (!pendingCommit_.ready) return;
  if (!success) {
    sendPacket(TradeReject, nullptr, 0, transactionId_);
    resetTradeState();
    setState(State::TradeSelecting, "TRADE COULD NOT BE SAVED."); return;
  }
  // Keep the transaction id until the acknowledgement is on the wire, then
  // clear every trade-only latch before returning to the reusable session.
  sendPacket(TradeCommitApplied, nullptr, 0, transactionId_);
  resetTradeState();
  activityAccepted_ = false;
  activity_ = Activity::None;
  setState(State::Connected, "TRADE COMPLETE!");
}

void Multiplayer::update() {
#if defined(ARDUINO_ARCH_ESP32)
  if (transportKind_ == TransportKind::Wifi) {
    if (!enabled_) return;
    const uint32_t now = millis();
    if (WiFi.status() != WL_CONNECTED) {
      if (transportConnected_) transportDisconnected();
      if (gOnlineClient.connected()) gOnlineClient.stop();
      setState(State::Error, "WIFI LOST. RECONNECTING...");
      return;
    }
    if (!gOnlineCloudMode && gOnlineClient.connected())
      flushPendingOnlineFrame();
    const bool relayConnected = gOnlineCloudMode
        ? gOnlineMqtt.connected() : gOnlineClient.connected();
    if (!relayConnected) {
      if (transportConnected_) transportDisconnected();
      const uint32_t previousAttempt = gOnlineLastConnectAttemptMs;
      resetOnlineTransportState(true);
      gOnlineLastConnectAttemptMs = previousAttempt;
      if (now - gOnlineLastConnectAttemptMs < 5000U) return;
      gOnlineLastConnectAttemptMs = now;
      bool opened = false;
      if (gOnlineCloudMode) {
        char clientId[48]{};
        std::snprintf(clientId, sizeof(clientId), "pokegochi-%05lu-%08lx",
                      static_cast<unsigned long>(playerId_),
                      static_cast<unsigned long>(gOnlineCloudNonce));
        opened = gOnlineMqtt.connect(clientId) &&
            gOnlineMqtt.subscribe(gOnlineCloudSubscribeTopic, 0);
      } else {
        opened = gOnlineClient.connect(onlineHost_, onlinePort_, 5000);
        if (opened) gOnlineClient.setNoDelay(false);
      }
      if (!opened) {
        setState(State::Error, "ONLINE SERVER OFFLINE. RETRYING...");
        return;
      }
      const PokegochiOnlineProtocol::HelloPayload hello{
          PokegochiMultiplayerProtocol::kVersion, {0, 0, 0},
          static_cast<uint32_t>(std::strtoul(onlineAccessCode_, nullptr, 10)),
          onlineBalancedWins_};
      if (!sendOnlineFrame(PokegochiOnlineProtocol::MessageType::Hello,
                           playerId_, 0, 0, &hello, sizeof(hello))) {
        resetOnlineTransportState(true);
        return;
      }
      gOnlineLastRxMs = gOnlineLastPingMs = now;
      setState(State::Advertising, "CONNECTED. AUTHENTICATING SERVER...");
    }

    const auto handleOnlineFrame = [&](const PokegochiOnlineProtocol::FrameHeader& header,
                                       const uint8_t* payload) {
      using PokegochiOnlineProtocol::MessageType;
      const MessageType type = static_cast<MessageType>(header.type);
      switch (type) {
        case MessageType::Welcome:
          gOnlineAuthenticated = true;
          setState(State::Advertising, "ONLINE SERVER READY. FIND A PLAYER.");
          break;
        case MessageType::PeerEntry: {
          if (header.payloadLength != sizeof(
                  PokegochiOnlineProtocol::PeerEntryPayload)) break;
          char address[20]{};
          std::snprintf(address, sizeof(address), "ONLINE-%05lu",
                        static_cast<unsigned long>(header.sourceId));
          const auto& entry = *reinterpret_cast<const
              PokegochiOnlineProtocol::PeerEntryPayload*>(payload);
          transportDiscovered(header.sourceId, 0, address, 0,
                              entry.balancedWins);
          break;
        }
        case MessageType::PeerListComplete:
          completeOnlinePeerScan();
          break;
        case MessageType::SessionReady:
          if (header.payloadLength != sizeof(PokegochiOnlineProtocol::SessionReadyPayload)) break;
          gOnlineSessionReady = true;
          gOnlineSessionId = header.sessionId;
          gOnlinePeerId = header.sourceId;
          {
            const bool outbound = reinterpret_cast<const
                PokegochiOnlineProtocol::SessionReadyPayload*>(payload)->outbound != 0;
            transportConnected(outbound);
            if (outbound && !sendPacket(Invite, nullptr, 0, invitationToken_)) {
              closeTransportLink();
              setState(State::Error, "ONLINE INVITATION COULD NOT BE SENT.");
            }
          }
          break;
        case MessageType::SessionBusy:
          gOnlineSessionReady = false;
          transportConnected_ = false;
          resetSession(true);
          setState(State::Advertising, "PLAYER IS BUSY OR OFFLINE.");
          break;
        case MessageType::SessionClosed:
          if (gOnlineSessionReady && header.sessionId == gOnlineSessionId &&
              header.sourceId == gOnlinePeerId) {
            gOnlineSessionReady = false;
            gOnlineSessionId = 0;
            gOnlinePeerId = 0;
            if (transportConnected_) transportDisconnected();
          }
          break;
        case MessageType::Relay:
          if (gOnlineSessionReady && header.sessionId == gOnlineSessionId &&
              header.sourceId == gOnlinePeerId &&
              PokegochiOnlineProtocol::validGamePacket(payload, header.payloadLength))
            processPacket(payload, header.payloadLength);
          break;
        case MessageType::Ping:
          sendOnlineFrame(MessageType::Pong, playerId_, header.sourceId,
                          header.sessionId);
          break;
        case MessageType::Pong:
          break;
        case MessageType::Error:
          setState(State::Error, "ONLINE SERVER REJECTED THE CONNECTION.");
          resetOnlineTransportState(true);
          break;
        default:
          break;
      }
    };

    if (gOnlineCloudMode && !gOnlineCloudFramePending)
      gOnlineMqtt.loop();
    if (gOnlineCloudMode && gOnlineCloudFramePending) {
      gOnlineLastRxMs = now;
      handleOnlineFrame(gOnlineIncomingHeader, gOnlinePayload);
      gOnlineIncomingHeader = {};
      gOnlineHeaderBytes = gOnlinePayloadBytes = 0;
      gOnlineCloudFramePending = false;
    }
    while (!gOnlineCloudMode && gOnlineClient.connected() &&
           gOnlineClient.available() > 0) {
      if (gOnlineHeaderBytes < sizeof(gOnlineIncomingHeader)) {
        const int read = gOnlineClient.read(
            reinterpret_cast<uint8_t*>(&gOnlineIncomingHeader) + gOnlineHeaderBytes,
            sizeof(gOnlineIncomingHeader) - gOnlineHeaderBytes);
        if (read <= 0) break;
        gOnlineHeaderBytes = static_cast<uint16_t>(gOnlineHeaderBytes + read);
        if (gOnlineHeaderBytes < sizeof(gOnlineIncomingHeader)) continue;
        if (!PokegochiOnlineProtocol::validHeader(gOnlineIncomingHeader)) {
          setState(State::Error, "INVALID ONLINE SERVER FRAME.");
          gOnlineClient.stop();
          break;
        }
      }
      if (gOnlinePayloadBytes < gOnlineIncomingHeader.payloadLength) {
        const int read = gOnlineClient.read(
            gOnlinePayload + gOnlinePayloadBytes,
            gOnlineIncomingHeader.payloadLength - gOnlinePayloadBytes);
        if (read <= 0) break;
        gOnlinePayloadBytes = static_cast<uint16_t>(gOnlinePayloadBytes + read);
        if (gOnlinePayloadBytes < gOnlineIncomingHeader.payloadLength) continue;
      }
      gOnlineLastRxMs = now;
      handleOnlineFrame(gOnlineIncomingHeader, gOnlinePayload);
      gOnlineIncomingHeader = {};
      gOnlineHeaderBytes = gOnlinePayloadBytes = 0;
    }
    // READY is a resumable transaction on physical Wi-Fi. Advance only after
    // the previous Relay frame has drained, so a transient full TCP window
    // never turns a valid button press into a partial Party on the server.
    continueWifiBattleReadyTransmission();
    const bool stillConnected = gOnlineCloudMode
        ? gOnlineMqtt.connected() : gOnlineClient.connected();
    if (stillConnected &&
        now - gOnlineLastPingMs >= kOnlinePingIntervalMs) {
      if (!sendOnlineFrame(PokegochiOnlineProtocol::MessageType::Ping,
                           playerId_, gOnlinePeerId, gOnlineSessionId)) {
        if (!gOnlineCloudMode && gOnlineClient.connected()) {
          // EAGAIN is backpressure, not proof of a dead peer. The receive
          // timeout remains the authoritative liveness check.
          Serial.println("[WIFI] heartbeat deferred by relay backpressure");
        } else {
          Serial.println("[WIFI] heartbeat write failed; closing stale socket");
          if (transportConnected_) transportDisconnected();
          resetOnlineTransportState(true);
          setState(State::Error, "ONLINE LINK LOST. RETRYING...");
          return;
        }
      }
      gOnlineLastPingMs = now;
    }
    const uint32_t onlineTimeoutMs = gOnlineSessionReady
        ? kOnlineSessionTimeoutMs : kOnlineIdleTimeoutMs;
    if (stillConnected && now - gOnlineLastRxMs >= onlineTimeoutMs) {
      setState(State::Error, "ONLINE SERVER TIMED OUT. RETRYING...");
      resetOnlineTransportState(true);
    }
    return;
  }
  TransportEvent discovery{};
  bool hasDiscovery = false;
  uint32_t criticalDrops = 0;
  portENTER_CRITICAL(&gDiscoveryMux);
  if (gDiscoveryPending) {
    discovery = gLatestDiscovery;
    gDiscoveryPending = false;
    hasDiscovery = true;
  }
  criticalDrops = gCriticalTransportDrops;
  gCriticalTransportDrops = 0;
  portEXIT_CRITICAL(&gDiscoveryMux);
  if (hasDiscovery)
    transportDiscovered(discovery.id, discovery.rssi, discovery.address, discovery.addressType);
  if (criticalDrops) {
    Serial.printf("[BLE] critical transport queue overflow drops=%lu; resetting link\n",
                  static_cast<unsigned long>(criticalDrops));
    if (gTransportQueue) xQueueReset(gTransportQueue);
    closeTransportLink();
    resetSession(true);
    unexpectedDisconnectPending_ = true;
    setState(State::Advertising, "BLUETOOTH OVERLOADED. TRY AGAIN.");
    NimBLEDevice::startAdvertising();
    return;
  }
  TransportEvent event{};
  while (gTransportQueue && xQueueReceive(gTransportQueue, &event, 0) == pdTRUE) {
    switch (event.kind) {
      case TransportEventKind::Discovered: break;
      case TransportEventKind::Connected: transportConnected(event.outbound); break;
      case TransportEventKind::Disconnected: transportDisconnected(); break;
      case TransportEventKind::Packet: transportPacket(event.bytes, event.length); break;
    }
  }
  if (state_ == State::Scanning && !NimBLEDevice::getScan()->isScanning()) {
    const bool started = NimBLEDevice::getScan()->start(5000, false, true);
    Serial.printf("[BLE] scan window ended; restarting result=%u\n", started ? 1U : 0U);
    setState(State::Scanning, peerCount_ ? "SELECT A NEARBY PLAYER." : "NO POKEGOCHIS FOUND.");
  }
#endif
}
