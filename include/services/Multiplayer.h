#pragma once

#include <cstddef>
#include <cstdint>
#include "game/BattleEngine.h"

// Multiplayer is a device-wide service.  Its lifetime is deliberately not
// tied to the Bag screen: a player may connect, leave the Bag, rearrange the
// Party in the Box and return to the same peer session.
class Multiplayer {
 public:
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
  enum class BattleCommandKind : uint8_t { None, Move, Switch, Forfeit, Wait };

  struct Peer {
    uint32_t playerId = 0;
    int8_t rssi = -127;
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
    uint8_t hostSpikesLayers = 0;
    uint8_t guestSpikesLayers = 0;
  };

  bool begin();
  void update();
  void shutdown();

  uint32_t playerId() const;
  bool enabled() const;
  bool setEnabled(bool enabled);
  State state() const;
  Activity activity() const;
  bool connected() const;
  uint32_t peerId() const;
  const char* statusText() const;

  void startScan();
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
  bool setTradeConfirmation(bool accept);
  bool takeTradeCommit(TradeCommit& commit);
  void completeTradeCommit(bool success);

  // Party preparation is deliberately independent from the Bag screen. A
  // snapshot is sent only when READY is pressed, after the player has had a
  // chance to leave Multiplayer, rearrange the Party in the Box and return.
  bool setBattleReady(const PokemonCollection& collection);
  bool localBattleReady() const;
  bool remoteBattleReady() const;
  const BattleRoster& localBattleRoster() const;
  const BattleRoster& remoteBattleRoster() const;
  bool submitBattleLead(uint8_t partySlot);
  bool takeBattleLaunch(BattleLaunch& launch);
  bool submitBattleCommand(BattleCommandKind kind, uint8_t value, uint16_t turn);
  bool takeBattleCommands(BattleCommand& local, BattleCommand& remote);
  bool isBattleHost() const;
  bool publishBattleState(const BattleState& battle, const PokemonCollection& collection,
                          const uint8_t remoteRosterSlotAtIndex[kPartyCapacity]);
  bool takeBattleState(BattleStateSync& sync);
  bool acknowledgeBattleState(uint16_t turn);
  bool takeBattleStateAcknowledgement(uint16_t& turn);

  // The UI uses this edge rather than polling/redrawing continuously.
  bool takeChanged();

  // Transport callbacks. They are public solely so the small platform glue in
  // Multiplayer.cpp can enqueue events without making any UI call from a BLE
  // task. Game code should not call them.
  void transportDiscovered(uint32_t id, int8_t rssi, const char* address, uint8_t addressType);
  void transportConnected(bool outbound);
  void transportDisconnected();
  void transportPacket(const uint8_t* bytes, size_t length);

 private:
  static constexpr uint8_t kMaximumPeers = 6;
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
  void resetSession(bool retainRadio);
  void processPacket(const uint8_t* bytes, size_t length);
  void maybeBeginTradeCommit();
  void maybeAdvanceBattlePreparation();
  void maybeLaunchBattle();

  uint32_t playerId_ = 0;
  uint32_t peerId_ = 0;
  uint32_t invitationToken_ = 0;
  uint32_t transactionId_ = 0;
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
  bool battleLaunchPending_ = false;
  bool battleCommandsPending_ = false;
  bool transportConnected_ = false;
  char status_[64] = "BLUETOOTH IS OFF.";
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
  uint8_t incomingBattlePokemonMask_ = 0;
  uint8_t incomingBattleVolatileMask_ = 0;
  bool incomingBattleCore_ = false;
  bool battleStateAckPending_ = false;
  uint16_t battleStateAckTurn_ = 0;
};
