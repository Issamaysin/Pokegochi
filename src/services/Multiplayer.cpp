#include "services/Multiplayer.h"
#include "services/MultiplayerProtocol.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
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
              BattleSyncAck == PokegochiMultiplayerProtocol::BattleSyncAck,
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
struct ActivityPayload { uint8_t activity; };
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
#pragma pack(pop)

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
#endif
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
bool Multiplayer::enabled() const { return enabled_; }
Multiplayer::State Multiplayer::state() const { return state_; }
Multiplayer::Activity Multiplayer::activity() const { return activity_; }
bool Multiplayer::connected() const {
  return transportConnected_ && peerId_ && state_ >= State::Connected && state_ != State::Error;
}
uint32_t Multiplayer::peerId() const { return peerId_; }
const char* Multiplayer::statusText() const { return status_; }
uint8_t Multiplayer::peerCount() const { return peerCount_; }
Multiplayer::Peer Multiplayer::peer(uint8_t index) const {
  return index < peerCount_ ? peers_[index].publicData : Peer{};
}
const Multiplayer::TradeOffer& Multiplayer::localTradeOffer() const { return localOffer_; }
const Multiplayer::TradeOffer& Multiplayer::remoteTradeOffer() const { return remoteOffer_; }
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

bool Multiplayer::setEnabled(bool enable) {
  if (enable == enabled_) return true;
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("[BLE] toggle %s heap=%u block=%u\n", enable ? "ON" : "OFF",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()));
#endif
  if (!enable) {
    disconnect();
#if defined(ARDUINO_ARCH_ESP32)
    if (initialized_) NimBLEDevice::deinit(true);
    if (gTransportQueue) { vQueueDelete(gTransportQueue); gTransportQueue = nullptr; }
    portENTER_CRITICAL(&gDiscoveryMux);
    gDiscoveryPending = false; gLatestDiscovery = {}; gCriticalTransportDrops = 0;
    portEXIT_CRITICAL(&gDiscoveryMux);
    gServer = nullptr; gServerTx = nullptr; gClient = nullptr; gRemoteRx = nullptr;
    gOutboundPhysicalLink = false; gOutboundConnectPending = false;
#endif
    initialized_ = false; enabled_ = false; transportConnected_ = false;
    setState(State::Off, "TURN BLUETOOTH ON TO PLAY.");
    return true;
  }
  enabled_ = true;
  if (!startTransport()) {
    enabled_ = false;
    setState(State::Error, "BLUETOOTH COULD NOT START.");
#if defined(ARDUINO_ARCH_ESP32)
    Serial.printf("[BLE] start failed heap=%u block=%u\n",
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getMaxAllocHeap()));
#endif
    return false;
  }
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("[BLE] radio ready heap=%u block=%u\n",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()));
#endif
  setState(State::Advertising, "READY. FIND A NEARBY PLAYER.");
  // Advertising and scanning are independent BLE roles. Start both so peer
  // discovery does not depend on a narrow race between scan windows.
  startScan();
  return true;
}

bool Multiplayer::startTransport() {
#if !defined(ARDUINO_ARCH_ESP32)
  initialized_ = true; return true;
#else
  if (initialized_) return true;
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

void Multiplayer::transportDiscovered(uint32_t id, int8_t rssi, const char* address, uint8_t type) {
  for (uint8_t i = 0; i < peerCount_; ++i) {
    if (peers_[i].publicData.playerId != id) continue;
    peers_[i].publicData.rssi = rssi;
    changed_ = true;
    return;
  }
  if (peerCount_ >= kMaximumPeers) return;
  PeerRecord& record = peers_[peerCount_++];
  record.publicData = {id, rssi}; record.addressType = type;
  std::strncpy(record.address, address ? address : "", sizeof(record.address) - 1U);
  changed_ = true;
}

bool Multiplayer::choosePeer(uint8_t index) {
  if (state_ != State::Scanning || index >= peerCount_) return false;
#if defined(ARDUINO_ARCH_ESP32)
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
  transportConnected_ = true; outbound_ = outbound; changed_ = true;
}

void Multiplayer::transportDisconnected() {
  transportConnected_ = false;
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
          sendPacket(Invite, nullptr, 0, invitationToken_);
        } else {
          invitationToken_ = header.transaction;
          sendPacket(InviteAccept, nullptr, 0, invitationToken_);
          setState(State::Connected, "PLAYER CONNECTED.");
        }
        break;
      }
      peerId_ = header.sourceId; invitationToken_ = header.transaction;
      setState(State::ConfirmIncomingPeer, "A PLAYER WANTS TO CONNECT.");
      break;
    case InviteAccept:
      if (state_ == State::Connecting && header.transaction == invitationToken_) {
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
      if (!connected() || header.payloadLength != sizeof(ActivityPayload)) break;
      activity_ = static_cast<Activity>(reinterpret_cast<const ActivityPayload*>(payload)->activity);
      if (activity_ != Activity::Battle && activity_ != Activity::Trade) break;
      // A physical BLE link is intentionally reused for more than one
      // activity.  Do not let a completed/cancelled trade leave its two-phase
      // commit latch armed when the peer starts the next one.
      resetTradeState();
      setState(State::ConfirmIncomingActivity,
               activity_ == Activity::Battle ? "PLAYER REQUESTS A BATTLE." : "PLAYER REQUESTS A TRADE.");
      break;
    case ActivityAccept:
      if (state_ == State::WaitingActivityReply && header.payloadLength == sizeof(ActivityPayload) &&
          reinterpret_cast<const ActivityPayload*>(payload)->activity == static_cast<uint8_t>(activity_)) {
        activityAccepted_ = true;
        setState(activity_ == Activity::Trade ? State::TradeSelecting : State::BattlePreparing,
                 activity_ == Activity::Trade ? "CHOOSE A POKEMON IN THE BOX." : "PREPARE YOUR PARTY, THEN START.");
      }
      break;
    case ActivityReject:
      activity_ = Activity::None; setState(State::Connected, "REQUEST DECLINED."); break;
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
    case BattlePartyMember:
      if (activity_ != Activity::Battle || header.payloadLength != sizeof(BattlePartyPayload)) break;
      {
        const BattlePartyPayload& incoming = *reinterpret_cast<const BattlePartyPayload*>(payload);
        if (incoming.slot >= kPartyCapacity || !incoming.pokemon.uid || !incoming.pokemon.speciesId) break;
        remoteRoster_.members[incoming.slot] = incoming.pokemon;
        remoteRoster_.count = std::max<uint8_t>(remoteRoster_.count, static_cast<uint8_t>(incoming.slot + 1U));
        changed_ = true;
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
        if (!complete) break;
        remoteRoster_.count = count; remoteRoster_.valid = true; remoteBattleReady_ = true;
        maybeAdvanceBattlePreparation();
      }
      break;
    case BattleLead:
      if (activity_ != Activity::Battle || header.payloadLength != sizeof(BattleLeadPayload) ||
          !remoteBattleReady_) break;
      remoteLeadSlot_ = reinterpret_cast<const BattleLeadPayload*>(payload)->slot;
      if (remoteLeadSlot_ >= remoteRoster_.count || !remoteRoster_.members[remoteLeadSlot_].currentHp) break;
      remoteLeadReady_ = true; maybeLaunchBattle(); changed_ = true;
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
        localBattleReady_ = remoteBattleReady_ = localLeadReady_ = remoteLeadReady_ = false;
        battleLaunchPending_ = false; std::memset(&localRoster_, 0, sizeof(localRoster_));
        std::memset(&remoteRoster_, 0, sizeof(remoteRoster_));
        std::memset(&pendingBattleLaunch_, 0, sizeof(pendingBattleLaunch_));
        battleCommandsPending_ = false; localBattleCommand_ = BattleCommand{};
        remoteBattleCommand_ = BattleCommand{};
        std::memset(&incomingBattleState_, 0, sizeof(incomingBattleState_));
        incomingBattlePokemonMask_ = incomingBattleVolatileMask_ = 0; incomingBattleCore_ = false;
        battleStateAckPending_ = false; battleStateAckTurn_ = 0;
        setState(State::Connected, "PLAYER CANCELLED THE ACTIVITY.");
      }
      break;
    case BattleCommandPacket:
      if (activity_ != Activity::Battle || state_ != State::BattleActive ||
          header.payloadLength != sizeof(BattleCommandPayload)) break;
      {
        const BattleCommandPayload& command = *reinterpret_cast<const BattleCommandPayload*>(payload);
        if (command.kind < static_cast<uint8_t>(BattleCommandKind::Move) ||
            command.kind > static_cast<uint8_t>(BattleCommandKind::Wait)) break;
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
        incomingBattlePokemonMask_ |= static_cast<uint8_t>(1U << (packet.slot + packet.side * kPartyCapacity));
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
        incomingBattleVolatileMask_ |= static_cast<uint8_t>(1U << (packet.slot + packet.side * kPartyCapacity));
      }
      break;
    case BattleSyncCore:
      if (activity_ != Activity::Battle || state_ != State::BattleActive || playerId_ < peerId_ ||
          header.payloadLength != sizeof(BattleSyncCorePayload)) break;
      {
        const BattleSyncCorePayload& packet = *reinterpret_cast<const BattleSyncCorePayload*>(payload);
        if (incomingBattleState_.turn != packet.turn || !packet.hostCount || !packet.guestCount ||
            packet.hostCount > kPartyCapacity || packet.guestCount > kPartyCapacity) break;
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
        const uint8_t requiredPokemonMask = static_cast<uint8_t>(
            ((1U << packet.hostCount) - 1U) | (((1U << packet.guestCount) - 1U) << kPartyCapacity));
        incomingBattleState_.ready = (incomingBattlePokemonMask_ & requiredPokemonMask) == requiredPokemonMask &&
                                     (incomingBattleVolatileMask_ & requiredPokemonMask) == requiredPokemonMask;
        changed_ = true;
      }
      break;
    case BattleSyncAck:
      if (activity_ != Activity::Battle || state_ != State::BattleActive || playerId_ > peerId_ ||
          header.payloadLength != sizeof(uint16_t)) break;
      battleStateAckTurn_ = *reinterpret_cast<const uint16_t*>(payload);
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
  if (enabled_) setState(State::Advertising, "READY. FIND A NEARBY PLAYER.");
}

void Multiplayer::closeTransportLink() {
#if defined(ARDUINO_ARCH_ESP32)
  if (gClient && gClient->isConnected()) {
    Serial.println("[BLE] closing outbound physical link");
    gClient->disconnect();
  } else if (gServer && gInboundConnectionHandle != BLE_HS_CONN_HANDLE_NONE) {
    Serial.println("[BLE] closing inbound physical link");
    gServer->disconnect(gInboundConnectionHandle);
  }
#endif
  transportConnected_ = false;
  outbound_ = false;
#if defined(ARDUINO_ARCH_ESP32)
  gOutboundConnectPending = false;
#endif
}

void Multiplayer::resetSession(bool) {
  peerId_ = invitationToken_ = 0; activity_ = Activity::None;
  activityAccepted_ = false;
  resetTradeState();
  localBattleReady_ = remoteBattleReady_ = localLeadReady_ = remoteLeadReady_ = false;
  battleLaunchPending_ = false; std::memset(&localRoster_, 0, sizeof(localRoster_));
  std::memset(&remoteRoster_, 0, sizeof(remoteRoster_));
  std::memset(&pendingBattleLaunch_, 0, sizeof(pendingBattleLaunch_));
  localLeadSlot_ = remoteLeadSlot_ = 0; battleSeed_ = 0;
  battleCommandsPending_ = false; localBattleCommand_ = BattleCommand{};
  remoteBattleCommand_ = BattleCommand{};
  std::memset(&incomingBattleState_, 0, sizeof(incomingBattleState_));
  incomingBattlePokemonMask_ = incomingBattleVolatileMask_ = 0; incomingBattleCore_ = false;
  battleStateAckPending_ = false; battleStateAckTurn_ = 0;
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
  if (!connected() || (activity != Activity::Battle && activity != Activity::Trade)) return false;
  resetTradeState();
  activity_ = activity; activityAccepted_ = false;
  const ActivityPayload payload{static_cast<uint8_t>(activity)};
  if (!sendPacket(ActivityRequest, &payload, sizeof(payload))) return false;
  setState(State::WaitingActivityReply, "WAITING FOR THE OTHER PLAYER..."); return true;
}

void Multiplayer::answerActivity(bool accept) {
  if (state_ != State::ConfirmIncomingActivity) return;
  const ActivityPayload payload{static_cast<uint8_t>(activity_)};
  sendPacket(accept ? ActivityAccept : ActivityReject, &payload, sizeof(payload));
  if (!accept) { activity_ = Activity::None; setState(State::Connected, "REQUEST DECLINED."); return; }
  activityAccepted_ = true;
  setState(activity_ == Activity::Trade ? State::TradeSelecting : State::BattlePreparing,
           activity_ == Activity::Trade ? "CHOOSE A POKEMON IN THE BOX." : "PREPARE YOUR PARTY, THEN START.");
}

void Multiplayer::clearActivity() {
  if (connected() && activity_ != Activity::None) sendPacket(ActivityCancel);
  activity_ = Activity::None; activityAccepted_ = false;
  resetTradeState();
  localBattleReady_ = remoteBattleReady_ = localLeadReady_ = remoteLeadReady_ = false;
  battleLaunchPending_ = false; std::memset(&localRoster_, 0, sizeof(localRoster_));
  std::memset(&remoteRoster_, 0, sizeof(remoteRoster_));
  std::memset(&pendingBattleLaunch_, 0, sizeof(pendingBattleLaunch_));
  battleCommandsPending_ = false; localBattleCommand_ = BattleCommand{};
  remoteBattleCommand_ = BattleCommand{};
  std::memset(&incomingBattleState_, 0, sizeof(incomingBattleState_));
  incomingBattlePokemonMask_ = incomingBattleVolatileMask_ = 0; incomingBattleCore_ = false;
  battleStateAckPending_ = false; battleStateAckTurn_ = 0;
  if (connected()) setState(State::Connected, "PLAYER CONNECTED.");
}

bool Multiplayer::setBattleReady(const PokemonCollection& collection) {
  if (!connected() || activity_ != Activity::Battle ||
      (state_ != State::BattlePreparing && state_ != State::BattleWaitingReady)) return false;
  BattleRoster roster{};
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) {
    const OwnedPokemon* pokemon = CollectionLogic::find(collection, collection.party[slot]);
    if (!pokemon) continue;
    // Link battles may be prepared only with usable partners. Fainted or
    // recovering members remain in the ordinary Party but are not exported.
    if (!pokemon->currentHp || pokemon->recoverySecondsRemaining) continue;
    roster.members[roster.count++] = *pokemon;
  }
  if (!roster.count) { setState(State::BattlePreparing, "NO USABLE POKEMON IN PARTY."); return false; }
  roster.valid = true;
  localRoster_ = roster; localBattleReady_ = true; localLeadReady_ = false;
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

bool Multiplayer::localBattleReady() const { return localBattleReady_; }
bool Multiplayer::remoteBattleReady() const { return remoteBattleReady_; }
const Multiplayer::BattleRoster& Multiplayer::localBattleRoster() const { return localRoster_; }
const Multiplayer::BattleRoster& Multiplayer::remoteBattleRoster() const { return remoteRoster_; }

void Multiplayer::maybeAdvanceBattlePreparation() {
  if (localBattleReady_ && remoteBattleReady_)
    setState(State::BattleChoosingLead, "CHOOSE YOUR FIRST POKEMON.");
  else
    setState(State::BattleWaitingReady, localBattleReady_ ? "WAITING FOR THE OTHER PLAYER..." :
             "OTHER PLAYER IS READY. PRESS READY.");
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
  if (!connected() || activity_ != Activity::Battle || state_ != State::BattleActive ||
      kind == BattleCommandKind::None || localBattleCommand_.valid) return false;
  const BattleCommandPayload payload{turn, static_cast<uint8_t>(kind), value};
  if (!sendPacket(BattleCommandPacket, &payload, sizeof(payload), turn)) return false;
  localBattleCommand_ = {true, turn, kind, value};
  battleCommandsPending_ = remoteBattleCommand_.valid && remoteBattleCommand_.turn == turn;
  std::snprintf(status_, sizeof(status_), "WAITING FOR PLAYER:%05lu...", static_cast<unsigned long>(peerId_));
  changed_ = true; return true;
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
  uint8_t hostActiveSlot = 0xFFU;
  for (uint8_t slot = 0; slot < localRoster_.count; ++slot) {
    const OwnedPokemon* pokemon = CollectionLogic::find(collection, localRoster_.members[slot].uid);
    if (!pokemon) return false;
    const BattleSyncPokemonPayload packet{battle.turn, 0, slot, *pokemon};
    if (!sendPacket(BattleSyncPokemon, &packet, sizeof(packet), battle.turn)) return false;
    if (pokemon->uid == battle.playerUid) hostActiveSlot = slot;
  }
  uint8_t guestActiveSlot = 0xFFU;
  for (uint8_t index = 0; index < battle.opponentCount; ++index) {
    const uint8_t rosterSlot = remoteRosterSlotAtIndex[index];
    if (rosterSlot >= remoteRoster_.count) return false;
    const BattleSyncPokemonPayload packet{battle.turn, 1, rosterSlot, battle.opponents[index]};
    if (!sendPacket(BattleSyncPokemon, &packet, sizeof(packet), battle.turn)) return false;
    if (index == battle.opponentIndex) guestActiveSlot = rosterSlot;
  }
  if (hostActiveSlot >= localRoster_.count || guestActiveSlot >= remoteRoster_.count) return false;
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
    const BattleSyncVolatilePayload packet{battle.turn, 0, slot, state, moveEffects,
                                            gen4Effects};
    if (!sendPacket(BattleSyncVolatile, &packet, sizeof(packet), battle.turn)) return false;
  }
  for (uint8_t index = 0; index < battle.opponentCount; ++index) {
    const uint8_t rosterSlot = remoteRosterSlotAtIndex[index];
    const BattleSyncVolatilePayload packet{battle.turn, 1, rosterSlot,
                                            battle.opponentVolatiles[index],
                                            battle.opponentMoveEffects[index],
                                            battle.opponentGen4Effects[index]};
    if (!sendPacket(BattleSyncVolatile, &packet, sizeof(packet), battle.turn)) return false;
  }
  const BattleSyncCorePayload core{battle.turn, battle.rngState,
      static_cast<uint8_t>(battle.active ? 1U : 0U),
      static_cast<uint8_t>(battle.outcome), static_cast<uint8_t>(battle.weather), battle.weatherTurns,
      battle.playerSafeguardTurns, battle.opponentSafeguardTurns,
      battle.playerReflectTurns, battle.opponentReflectTurns,
      battle.playerLightScreenTurns, battle.opponentLightScreenTurns,
      battle.playerMistTurns, battle.opponentMistTurns,
      battle.playerProtectChain, battle.opponentProtectChain,
      static_cast<uint8_t>(battle.playerEndureThisTurn),
      static_cast<uint8_t>(battle.opponentEndureThisTurn),
      static_cast<uint8_t>(battle.playerNightmare),
      static_cast<uint8_t>(battle.opponentNightmares[battle.opponentIndex]),
      battle.playerSpikesLayers, battle.opponentSpikesLayers,
      battle.playerToxicSpikesLayers, battle.opponentToxicSpikesLayers,
      static_cast<uint8_t>(battle.playerStealthRock),
      static_cast<uint8_t>(battle.opponentStealthRock),
      battle.playerTailwindTurns, battle.opponentTailwindTurns,
      battle.playerLuckyChantTurns, battle.opponentLuckyChantTurns,
      battle.gravityTurns, battle.trickRoomTurns, battle.lastMoveUsedGlobally,
      localRoster_.count, remoteRoster_.count, hostActiveSlot, guestActiveSlot,
      battle.delayedToPlayer, battle.delayedToOpponent};
  return sendPacket(BattleSyncCore, &core, sizeof(core), battle.turn);
}

bool Multiplayer::takeBattleState(BattleStateSync& sync) {
  if (!incomingBattleCore_ || !incomingBattleState_.ready) return false;
  std::memcpy(&sync, &incomingBattleState_, sizeof(sync));
  std::memset(&incomingBattleState_, 0, sizeof(incomingBattleState_));
  incomingBattlePokemonMask_ = incomingBattleVolatileMask_ = 0; incomingBattleCore_ = false;
  return true;
}

bool Multiplayer::acknowledgeBattleState(uint16_t turn) {
  if (!connected() || activity_ != Activity::Battle || state_ != State::BattleActive ||
      playerId_ < peerId_) return false;
  return sendPacket(BattleSyncAck, &turn, sizeof(turn), turn);
}

bool Multiplayer::takeBattleStateAcknowledgement(uint16_t& turn) {
  if (!battleStateAckPending_) return false;
  turn = battleStateAckTurn_;
  battleStateAckPending_ = false;
  return true;
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
