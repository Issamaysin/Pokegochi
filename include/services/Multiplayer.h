#pragma once

#include <cstddef>
#include <cstdint>
#include "game/BattleEngine.h"

// Multiplayer stays alive while an activity-specific Party or Box selector is
// open. Closing the Bag is the session boundary and disables the BLE radio.
class Multiplayer {
 public:
  enum class TransportKind : uint8_t { Ble, Wifi };
  enum class State : uint8_t {
    Off,
    Advertising,
    Scanning,
    ConfirmOutgoingPeer,
    Connecting,
    ConfirmIncomingPeer,
    Connected,
    WaitingActivityReply,
    ConfirmIncomingActivity,
    TradeSelecting,
    TradeConfirming,
    BattlePreparing,
    BattleWaitingReady,
    BattleChoosingLead,
    BattleLaunching,
    BattleActive,
    Error,
  };

  enum class Activity : uint8_t { None, Battle, Trade };
  enum class BattleRules : uint8_t { None, Normal, Balanced };
  enum class BattleCommandKind : uint8_t { None, Move, Switch, Forfeit, Wait, Item };

  struct Peer {
    uint32_t playerId = 0;
    int8_t rssi = -127;
    uint32_t balancedWins = 0;
  };

  struct TradeOffer {
    bool valid = false;
    uint32_t ownerId = 0;
    OwnedPokemon pokemon{};
  };

  struct TradeCommit {
    bool ready = false;
    uint32_t transactionId = 0;
    uint32_t outgoingUid = 0;
    OwnedPokemon incoming{};
  };

  struct BattleRoster {
    bool valid = false;
    uint8_t count = 0;
    OwnedPokemon members[kPartyCapacity]{};
  };

  struct BattleLaunch {
    bool ready = false;
    uint32_t seed = 0;
    BattleRules rules = BattleRules::None;
    uint8_t localLeadSlot = 0;
    uint8_t remoteLeadSlot = 0;
    BattleRoster local{};
    BattleRoster remote{};
  };

  struct BattleCommand {
    bool valid = false;
    uint16_t turn = 0;
    BattleCommandKind kind = BattleCommandKind::None;
    uint8_t value = 0;
  };

  struct BattleStateSync {
    bool ready = false;
    uint16_t turn = 0;
    // Two authoritative snapshots can legitimately share one turn: the
    // attack result and the forced replacement that follows it.  The digest
    // identifies the exact snapshot so retries cannot replay the old active
    // Pokemon or acknowledge a newer state by accident.
    uint32_t snapshotChecksum = 0;
    bool active = false;
    BattleOutcome hostOutcome = BattleOutcome::None;
    uint32_t rngState = 0;
    BattleWeather weather = BattleWeather::Clear;
    uint8_t weatherTurns = 0;
    uint8_t hostSafeguardTurns = 0;
    uint8_t guestSafeguardTurns = 0;
    uint8_t hostReflectTurns = 0, guestReflectTurns = 0;
    uint8_t hostLightScreenTurns = 0, guestLightScreenTurns = 0;
    uint8_t hostMistTurns = 0, guestMistTurns = 0;
    uint8_t hostProtectChain = 0, guestProtectChain = 0;
    bool hostEndureThisTurn = false, guestEndureThisTurn = false;
    bool hostNightmare = false, guestNightmare = false;
    bool hostZMoveUsed = false, guestZMoveUsed = false;
    DelayedAttackState delayedToHost{};
    DelayedAttackState delayedToGuest{};
    uint8_t hostCount = 0;
    uint8_t guestCount = 0;
    uint8_t hostActiveRosterSlot = 0;
    uint8_t guestActiveRosterSlot = 0;
    OwnedPokemon hostParty[kPartyCapacity]{};
    OwnedPokemon guestParty[kPartyCapacity]{};
    // Indexed by the READY roster slot, not by the transient battle-array
    // index.  Sending every slot also preserves battle-local held-item state
    // across switches (THIEF/TRICK/KNOCK OFF) on both consoles.
    CombatVolatile hostVolatiles[kPartyCapacity]{};
    CombatVolatile guestVolatiles[kPartyCapacity]{};
    // Move-specific volatile state must travel with the same roster slot as
    // CombatVolatile. Without this, the guest silently loses Taunt/Yawn,
    // Rollout locks, Recycle state and the other dedicated Gen-III effects
    // whenever the host publishes the authoritative turn.
    DedicatedMoveEffectState hostMoveEffects[kPartyCapacity]{};
    DedicatedMoveEffectState guestMoveEffects[kPartyCapacity]{};
    Gen4BattlerEffectState hostGen4Effects[kPartyCapacity]{};
    Gen4BattlerEffectState guestGen4Effects[kPartyCapacity]{};
    uint8_t hostSpikesLayers = 0;
    uint8_t guestSpikesLayers = 0;
    uint8_t hostToxicSpikesLayers = 0;
    uint8_t guestToxicSpikesLayers = 0;
    bool hostStealthRock = false;
    bool guestStealthRock = false;
    uint8_t hostTailwindTurns = 0;
    uint8_t guestTailwindTurns = 0;
    uint8_t hostLuckyChantTurns = 0;
    uint8_t guestLuckyChantTurns = 0;
    uint8_t gravityTurns = 0;
    uint8_t trickRoomTurns = 0;
    MoveId lastMoveUsedGlobally = MoveId::None;
  };

  bool begin();
  // Android uses the same session state machine with a stable app-owned ID;
  // the ESP32 identity path remains unchanged.
  bool beginWithPlayerId(uint32_t playerId);
  void update();
  void shutdown();

  uint32_t playerId() const;
  void setOnlineBalancedWins(uint32_t wins);
  uint32_t onlineBalancedWins() const;
  bool enabled() const;
  bool setEnabled(bool enabled);
  bool selectTransport(TransportKind kind);
  TransportKind transportKind() const;
  bool setOnlineConfiguration(const char* host, uint16_t port,
                              const char* accessCode);
  const char* onlineHost() const;
  uint16_t onlinePort() const;
  const char* onlineAccessCode() const;
  bool setWifiCredentials(const char* ssid, const char* password);
  const char* wifiSsid() const;
  const char* wifiPassword() const;
  uint8_t savedWifiNetworkCount() const;
  const char* savedWifiNetworkName(uint8_t index) const;
  bool wifiNetworkIsSaved(const char* ssid) const;
  const char* savedWifiPassword(const char* ssid) const;
  uint8_t scanWifiNetworks();
  bool takeWifiScanRequest();
  void setWifiScanResults(const char* const* names, const int8_t* rssi,
                          uint8_t count);
  bool wifiScanInProgress() const;
  uint8_t wifiNetworkCount() const;
  const char* wifiNetworkName(uint8_t index) const;
  int8_t wifiNetworkRssi(uint8_t index) const;
  State state() const;
  Activity activity() const;
  bool connected() const;
  // The logical session initiator owns the activity and battle-rule choice.
  // The invited peer can only accept or decline that complete proposal.
  bool initiatedConnection() const;
  uint32_t peerId() const;
  const char* statusText() const;

  void startScan();
  void completeOnlinePeerScan();
  // A Wi-Fi player search is one bounded relay query. Scanning remains the
  // selection state after results arrive, so the UI needs this explicit bit
  // to distinguish an active query from a completed result list.
  bool peerSearchInProgress() const;
  uint8_t peerCount() const;
  Peer peer(uint8_t index) const;
  bool choosePeer(uint8_t index);
  void answerPeerInvitation(bool accept);
  void disconnect();

  bool requestActivity(Activity activity);
  void answerActivity(bool accept);
  bool activityAccepted();
  void clearActivity();

  bool submitTradeOffer(const OwnedPokemon& pokemon);
  const TradeOffer& localTradeOffer() const;
  const TradeOffer& remoteTradeOffer() const;
  bool localTradeConfirmed() const;
  bool remoteTradeConfirmed() const;
  bool setTradeConfirmation(bool accept);
  bool takeTradeCommit(TradeCommit& commit);
  void completeTradeCommit(bool success);

  // Party preparation is deliberately independent from the Bag screen. A
  // snapshot is sent only when this player presses READY, after they have had
  // a chance to leave Multiplayer, rearrange the Party in the Box and return.
  // The remote READY never locks the local Box; local READY is the commit edge.
  bool setBattleReady(const PokemonCollection& collection);
  bool chooseBattleRules(BattleRules rules);
  BattleRules battleRules() const;
  bool battleRulesReady() const;
  bool localBattleReady() const;
  bool remoteBattleReady() const;
  const BattleRoster& localBattleRoster() const;
  const BattleRoster& remoteBattleRoster() const;
  bool submitBattleLead(uint8_t partySlot);
  bool takeBattleLaunch(BattleLaunch& launch);
  bool submitBattleCommand(BattleCommandKind kind, uint8_t value, uint16_t turn);
  bool localBattleCommandPending() const;
  bool takeBattleCommands(BattleCommand& local, BattleCommand& remote);
  bool isBattleHost() const;
  bool publishBattleState(const BattleState& battle, const PokemonCollection& collection,
                          const uint8_t remoteRosterSlotAtIndex[kPartyCapacity]);
  bool takeBattleState(BattleStateSync& sync);
  bool acknowledgeBattleState(uint16_t turn, uint32_t snapshotChecksum);
  bool takeBattleStateAcknowledgement(uint16_t& turn,
                                      uint32_t& snapshotChecksum);
  uint32_t publishedBattleStateChecksum() const;

  // The UI uses this edge rather than polling/redrawing continuously.
  bool takeChanged();
  // One-shot edge for an unplanned physical link loss. The game UI consumes
  // it on the main loop so it can leave Box/Trade/Battle safely, release the
  // BLE renderer reservation and rebuild Home. Deliberate local closes and
  // protocol-level SessionClose packets do not raise this edge.
  bool takeUnexpectedDisconnect();

  // Transport callbacks. They are public solely so the small platform glue in
  // Multiplayer.cpp can enqueue events without making any UI call from a BLE
  // task. Game code should not call them.
  void transportDiscovered(uint32_t id, int8_t rssi, const char* address,
                           uint8_t addressType, uint32_t balancedWins = 0);
  void transportConnected(bool outbound);
  void transportDisconnected();
  void transportPacket(const uint8_t* bytes, size_t length);
  bool takeOutgoingPacket(uint8_t* bytes, uint16_t& length);
  bool requestPeer(uint32_t playerId);

 private:
  // Eleven other real players plus the permanent server bot can be announced
  // when the twelve-slot online relay is full.
  static constexpr uint8_t kMaximumPeers = 12;
  struct PeerRecord {
    Peer publicData{};
    char address[20]{};
    uint8_t addressType = 0;
  };

  bool startTransport();
  bool connectTransport(const PeerRecord& peer);
  bool sendPacket(uint8_t type, const void* payload = nullptr, uint16_t length = 0,
                  uint32_t transaction = 0);
  void setState(State next, const char* status = nullptr);
  void closeTransportLink();
  void resetSession(bool retainRadio);
  void resetTradeState();
  void processPacket(const uint8_t* bytes, size_t length);
  void maybeBeginTradeCommit();
  void maybeAdvanceBattlePreparation();
  void maybeLaunchBattle();
#if defined(ARDUINO_ARCH_ESP32)
  // A Wi-Fi READY snapshot spans one packet per Party member plus the final
  // count marker.  The ESP32 TCP transport deliberately owns only one
  // deferred frame, so this transaction must advance across loop ticks
  // instead of treating temporary socket backpressure as a rejected click.
  void continueWifiBattleReadyTransmission();
  void resetWifiBattleReadyTransmission();
#endif
  void loadSavedWifiNetworks();
  void rememberSuccessfulWifiNetwork(const char* ssid, const char* password);

  static constexpr uint8_t kMaximumSavedWifiNetworks = 4;
  struct WifiCredential {
    char ssid[33]{};
    char password[65]{};
  };

  uint32_t playerId_ = 0;
  uint32_t peerId_ = 0;
  uint32_t invitationToken_ = 0;
  uint32_t transactionId_ = 0;
  TransportKind transportKind_ = TransportKind::Ble;
  State state_ = State::Off;
  Activity activity_ = Activity::None;
  bool enabled_ = false;
  bool initialized_ = false;
  bool outbound_ = false;
  bool changed_ = true;
  bool activityAccepted_ = false;
  bool localTradeConfirmed_ = false;
  bool remoteTradeConfirmed_ = false;
  bool tradeCommitOffered_ = false;
  bool tradeCommitPending_ = false;
  bool localBattleReady_ = false;
  bool remoteBattleReady_ = false;
  bool localLeadReady_ = false;
  bool remoteLeadReady_ = false;
#if defined(ARDUINO_ARCH_ESP32)
  bool wifiBattleReadyTransmissionPending_ = false;
  uint8_t wifiBattleReadyNextSlot_ = 0;
#endif
  BattleRules localBattleRules_ = BattleRules::None;
  BattleRules remoteBattleRules_ = BattleRules::None;
  bool battleLaunchPending_ = false;
  bool battleCommandsPending_ = false;
  bool transportConnected_ = false;
  bool localTransportClosePending_ = false;
  bool unexpectedDisconnectPending_ = false;
  char status_[64] = "BLUETOOTH IS OFF.";
  char onlineHost_[64]{};
  char onlineAccessCode_[7]{};
  // A cabled diagnostic may force a specific LAN relay for the current boot.
  // Normal player startup leaves this false and keeps relay selection automatic.
  bool onlineConfigurationForced_ = false;
  char wifiSsid_[33]{};
  char wifiPassword_[65]{};
  WifiCredential savedWifiNetworks_[kMaximumSavedWifiNetworks]{};
  uint8_t savedWifiNetworkCount_ = 0;
  bool wifiManualCandidate_ = false;
  uint16_t onlinePort_ = 32145;
  char wifiNetworks_[6][33]{};
  int8_t wifiNetworkRssi_[6]{};
  uint8_t wifiNetworkCount_ = 0;
  bool wifiScanRequestPending_ = false;
  bool wifiScanInProgress_ = false;
  bool onlinePeerSearchPending_ = false;
  PeerRecord peers_[kMaximumPeers]{};
  uint8_t peerCount_ = 0;
  TradeOffer localOffer_{};
  TradeOffer remoteOffer_{};
  TradeCommit pendingCommit_{};
  BattleRoster localRoster_{};
  BattleRoster remoteRoster_{};
  BattleLaunch pendingBattleLaunch_{};
  uint8_t localLeadSlot_ = 0;
  uint8_t remoteLeadSlot_ = 0;
  uint32_t battleSeed_ = 0;
  BattleCommand localBattleCommand_{};
  BattleCommand remoteBattleCommand_{};
  BattleStateSync incomingBattleState_{};
  // Keep enough room for every roster bit from both sides without relying on
  // the current three-member Party limit. Faint synchronization is a barrier:
  // a truncated mask would leave the replacement flow waiting indefinitely.
  uint16_t incomingBattlePokemonMask_ = 0;
  uint16_t incomingBattleVolatileMask_ = 0;
  bool incomingBattleCore_ = false;
  bool battleStateAckPending_ = false;
  uint16_t battleStateAckTurn_ = 0;
  uint32_t battleStateAckChecksum_ = 0;
  uint32_t publishedBattleStateChecksum_ = 0;
  // The authoritative host must retry the exact same bytes until the guest
  // acknowledges them. Rebuilding from the live Party lets clock-driven
  // friendship/form changes alter the checksum during a faint barrier.
  BattleStateSync* publishedBattleState_ = nullptr;
  uint32_t onlineBalancedWins_ = 0;
#if !defined(ARDUINO_ARCH_ESP32)
  // One authoritative three-vs-three state contains 13 packets. Native
  // regression clients must model the Android queue faithfully or they hide
  // the exact faint/switch barrier that the tests exercise.
  static constexpr uint8_t kOutgoingPacketCapacity = 32;
  struct OutgoingPacket {
    uint16_t length = 0;
    uint8_t bytes[14 + 160]{};
  };
  OutgoingPacket outgoingPackets_[kOutgoingPacketCapacity]{};
  uint8_t outgoingPacketCount_ = 0;
#endif
  void clearPublishedBattleStateCache();
};
