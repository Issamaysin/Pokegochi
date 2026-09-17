#pragma once
#include <Arduino.h>
#include <cstddef>
#include <Preferences.h>
#include "game/BattleEngine.h"
#include "game/Pokedex.h"
#include "game/GymSystem.h"
#include "game/Economy.h"
#include "game/EggSystem.h"

enum class UiColorTheme : uint8_t { Sapphire = 0, Ruby = 1, Emerald = 2 };

struct UserSettings {
  uint8_t homeBackground = 0;
  uint8_t brightnessPercent = 80;
  // The timeout needs only two bits. Its high bit stores automatic battle
  // dialogue, while two otherwise unused middle bits persist the global UI
  // theme without growing or migrating the shipped save ABI.
  uint8_t screenTimeoutIndex = 1;

  static constexpr uint8_t kAutoBattleTextMask = 0x80U;
  static constexpr uint8_t kThemeMask = 0x30U;
  static constexpr uint8_t kThemeShift = 4U;
  static constexpr uint8_t kScreenTimeoutMask = 0x0FU;
  uint8_t screenTimeout() const {
    return static_cast<uint8_t>(screenTimeoutIndex & kScreenTimeoutMask);
  }
  void setScreenTimeout(uint8_t index) {
    screenTimeoutIndex = static_cast<uint8_t>(
        (screenTimeoutIndex & (kAutoBattleTextMask | kThemeMask)) |
        (index & kScreenTimeoutMask));
  }
  bool autoBattleText() const {
    return (screenTimeoutIndex & kAutoBattleTextMask) != 0;
  }
  void setAutoBattleText(bool enabled) {
    screenTimeoutIndex = enabled
        ? static_cast<uint8_t>(screenTimeoutIndex | kAutoBattleTextMask)
        : static_cast<uint8_t>(screenTimeoutIndex & ~kAutoBattleTextMask);
  }
  UiColorTheme colorTheme() const {
    const uint8_t raw = static_cast<uint8_t>((screenTimeoutIndex & kThemeMask) >> kThemeShift);
    return raw <= static_cast<uint8_t>(UiColorTheme::Emerald)
        ? static_cast<UiColorTheme>(raw) : UiColorTheme::Sapphire;
  }
  void setColorTheme(UiColorTheme theme) {
    const uint8_t raw = static_cast<uint8_t>(theme) <= static_cast<uint8_t>(UiColorTheme::Emerald)
        ? static_cast<uint8_t>(theme) : static_cast<uint8_t>(UiColorTheme::Sapphire);
    screenTimeoutIndex = static_cast<uint8_t>(
        (screenTimeoutIndex & ~kThemeMask) | (raw << kThemeShift));
  }
};
static_assert(sizeof(UserSettings) == 3, "UserSettings save ABI changed");

struct PendingMoveLearning { uint32_t pokemonUid=0; MoveId move=MoveId::None; };
constexpr uint8_t kMoveLearningOriginInventory = 0xA7U;
struct MoveLearningQueue {
  PendingMoveLearning entries[kPartyCapacity]{};
  uint8_t count=0;
  uint8_t index=0;
  // Consumes existing tail padding, so persistent-save size and offsets do
  // not change. The nontrivial marker makes old padding impossible to mistake
  // for an interrupted Bag/TM workflow in practice.
  uint8_t origin=0;
};
static_assert(sizeof(MoveLearningQueue) == 28, "Move-learning save layout changed");
struct PendingEvolution { uint32_t pokemonUid=0; uint16_t choices[kEvolutionChoiceCapacity]{}; uint8_t choiceCount=0; };
struct EvolutionQueue { PendingEvolution entries[kPartyCapacity]{}; uint8_t count=0; uint8_t index=0; };

struct PlayerStatistics {
  uint32_t trainersDefeated = 0;
  uint32_t pokemonDefeated = 0;
  uint32_t pokemonCaptured = 0;
  uint32_t pokeBallsThrown = 0;
  uint32_t eggsHatched = 0;
  uint32_t shiniesEncountered = 0;
  uint32_t shiniesCaptured = 0;
  uint32_t legendariesEncountered = 0;
  uint32_t legendariesCaptured = 0;
};

// One manually arranged Home layout is kept with the save. The collision
// grid uses only 19x8 cells, so byte coordinates are sufficient and avoid
// attaching UI-only data to every Pokemon in the 386-slot collection.
struct HomePetPlacementState {
  uint8_t background = 0xFF;
  uint8_t validMask = 0;
  uint8_t gridX[kPartyCapacity]{};
  uint8_t gridY[kPartyCapacity]{};
};
static_assert(sizeof(HomePetPlacementState) == 8, "Home placement save layout changed");

struct GameSave {
  uint32_t playTimeSeconds = 0;
  uint32_t bootCount = 0;
  uint8_t flags = 0;
  uint8_t activePetSlot = 0;
  PokemonCollection collection{};
  Inventory inventory{};
  EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};
  BattleState battle{};
  PokedexState pokedex{};
  GymProgress gymProgress{};
  uint32_t money = 3000;
  MartState mart{};
  MoveLearningQueue moveLearning{};
  EvolutionQueue evolutionQueue{};
  EggState egg{};
  UserSettings settings{};
  // One ownership bit for TM01-TM50 and HM01-HM08. Machines are unique and
  // therefore disappear permanently from the Mart rotation after purchase.
  uint64_t ownedMachines = 0;
  PlayerStatistics statistics{};
  uint32_t martSeenDay = 0;
  PpItemInventory ppItems{};
  HomePetPlacementState homePetPlacement{};
};

enum class SaveLoadResult : uint8_t {
  Loaded,
  FirstStart,
  Corrupted,
  StorageError,
};

class PersistentSave {
 public:
  bool begin(void* migrationScratch = nullptr, size_t migrationScratchBytes = 0);
  SaveLoadResult loadOrCreate(GameSave& save);
  bool commit(const GameSave& save);
#if defined(POKEGOCHI_DEV_ALLOW_FACTORY_RESET)
 bool factoryResetForDevelopment();
#endif
 private:
  struct Record;
  static constexpr uint32_t kMagic = 0x504F4B45;
  static constexpr uint16_t kFormatVersion = 37;
  struct MartStateLegacy7 { MartOffer offers[7]{}; uint32_t elapsedSeconds=86400; uint32_t day=0; uint32_t rng=0x4D415254U; };
  struct MartStateLegacy10 { MartOffer offers[10]{}; uint32_t elapsedSeconds=86400; uint32_t day=0; uint32_t rng=0x4D415254U; };
  struct MartStateLegacy15 { MartOffer offers[15]{}; uint32_t elapsedSeconds=86400; uint32_t day=0; uint32_t rng=0x4D415254U; };
  struct BattleStateV9 {
    bool active; BattleKind kind; BattleOutcome outcome; uint32_t playerUid;
    OwnedPokemon opponents[kOpponentTeamCapacity]; uint8_t opponentCount; uint8_t opponentIndex;
    uint8_t trainerProfileId; uint8_t gymId; CombatVolatile playerVolatile;
    CombatVolatile opponentVolatiles[kOpponentTeamCapacity]; uint32_t rngState; uint16_t turn; uint32_t rewardMoney;
  };
  struct GameSaveV9 {
    uint32_t playTimeSeconds; uint32_t bootCount; uint8_t flags; uint8_t activePetSlot;
    PokemonCollection collection; Inventory inventory; EncounterCharges encounterCharges;
    WildEncounterClock wildEncounterClock; BattleStateV9 battle; PokedexState pokedex;
    GymProgress gymProgress; uint32_t money; MartStateLegacy7 mart;
  };
  struct RecordV9 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV9 payload; uint32_t crc; };
  struct BattleStateV10 {
    bool active; BattleKind kind; BattleOutcome outcome; uint32_t playerUid;
    OwnedPokemon opponents[kOpponentTeamCapacity]; uint8_t opponentCount; uint8_t opponentIndex;
    uint8_t trainerProfileId; uint8_t gymId; CombatVolatile playerVolatile;
    CombatVolatile opponentVolatiles[kOpponentTeamCapacity]; uint32_t rngState; uint16_t turn;
    uint32_t rewardMoney; uint8_t opponentItemUses;
  };
  struct GameSaveV10 {
    uint32_t playTimeSeconds; uint32_t bootCount; uint8_t flags; uint8_t activePetSlot;
    PokemonCollection collection; Inventory inventory; EncounterCharges encounterCharges;
    WildEncounterClock wildEncounterClock; BattleStateV10 battle; PokedexState pokedex;
    GymProgress gymProgress; uint32_t money; MartStateLegacy7 mart;
  };
  struct RecordV10 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV10 payload; uint32_t crc; };
  struct GameSaveV11 {
    uint32_t playTimeSeconds; uint32_t bootCount; uint8_t flags; uint8_t activePetSlot;
    PokemonCollection collection; Inventory inventory; EncounterCharges encounterCharges;
    WildEncounterClock wildEncounterClock; BattleStateV10 battle; PokedexState pokedex;
    GymProgress gymProgress; uint32_t money; MartStateLegacy7 mart; MoveLearningQueue moveLearning;
  };
  struct RecordV11 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV11 payload; uint32_t crc; };
  struct CombatVolatileLegacy {
    int8_t attackStage = 0, defenseStage = 0, spAttackStage = 0, spDefenseStage = 0;
    int8_t speedStage = 0, accuracyStage = 0, evasionStage = 0;
    uint8_t confusionTurns = 0, trappedTurns = 0, criticalStage = 0;
    bool flinched = false, protectedThisTurn = false, recharging = false;
    uint16_t substituteHp = 0;
    MoveId disabledMove = MoveId::None, encoreMove = MoveId::None, chargingMove = MoveId::None;
    MoveId choiceMove = MoveId::None; HeldItem heldItemOverride = HeldItem::None;
    bool hasHeldItemOverride = false, heldItemSuppressed = false;
  };
  // Version 21 was the last format before individual Pokémon carried their
  // six persistent Gen-III EV bytes. Keep its exact record layout so beta
  // saves are upgraded instead of being treated as a reset/corruption.
  struct OwnedPokemonV21 {
    uint32_t uid = kEmptyPokemonUid;
    uint32_t personality = 0;
    uint16_t speciesId = 0;
    uint32_t experience = 0;
    uint16_t currentHp = 0;
    uint16_t maximumHp = 0;
    uint8_t level = 1;
    StatusCondition status = StatusCondition::None;
    uint8_t fullness = 80;
    uint8_t happiness = 80;
    uint32_t careRemainderSeconds = 0;
    uint32_t careTickCounter = 0;
    uint32_t recoverySecondsRemaining = 0;
    MoveId moves[kMoveSlots] = {MoveId::None, MoveId::None, MoveId::None, MoveId::None};
    uint8_t movePp[kMoveSlots]{};
    uint8_t abilityId = 0;
    IndividualValues ivs{};
    PokemonNature nature = PokemonNature::Hardy;
    HeldItem heldItem = HeldItem::None;
    bool shiny = false;
  };
  struct PokemonCollectionV21 {
    OwnedPokemonV21 box[kBoxCapacity]{};
    uint32_t party[kPartyCapacity]{};
    uint32_t nextUid = 1;
  };
  struct BattleStateV21 {
    bool active = false;
    BattleKind kind = BattleKind::None;
    BattleOutcome outcome = BattleOutcome::None;
    uint32_t playerUid = 0;
    OwnedPokemonV21 opponents[kOpponentTeamCapacity]{};
    uint8_t opponentCount = 0;
    uint8_t opponentIndex = 0;
    uint8_t trainerProfileId = 0xFF;
    uint8_t gymId = 0xFF;
    CombatVolatileLegacy playerVolatile{};
    BattleHeldItemState playerHeldItems[kPartyCapacity]{};
    CombatVolatileLegacy opponentVolatiles[kOpponentTeamCapacity]{};
    uint32_t rngState = 0xA341316CU;
    uint16_t turn = 0;
    uint32_t rewardMoney = 0;
    uint8_t opponentItemUses = 0;
    uint8_t gymStage = 0;
    uint8_t leagueRegion = 0;
    uint8_t leagueStage = 0;
    uint8_t unlockedGeneration = 1;
  };
  struct GameSaveV21 {
    uint32_t playTimeSeconds = 0;
    uint32_t bootCount = 0;
    uint8_t flags = 0;
    uint8_t activePetSlot = 0;
    PokemonCollectionV21 collection{};
    Inventory inventory{};
    EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{};
    BattleStateV21 battle{};
    PokedexState pokedex{};
    GymProgress gymProgress{};
    uint32_t money = 3000;
    MartStateLegacy7 mart{};
    MoveLearningQueue moveLearning{};
    EvolutionQueue evolutionQueue{};
    EggState egg{};
    UserSettings settings{};
  };
  struct RecordV21 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV21 payload; uint32_t crc; };
  using CombatVolatileV22 = CombatVolatileLegacy;
  struct BattleStateV22 {
    bool active = false; BattleKind kind = BattleKind::None; BattleOutcome outcome = BattleOutcome::None;
    uint32_t playerUid = 0; OwnedPokemon opponents[kOpponentTeamCapacity]{};
    uint8_t opponentCount = 0, opponentIndex = 0, trainerProfileId = 0xFF, gymId = 0xFF;
    CombatVolatileV22 playerVolatile{}; BattleHeldItemState playerHeldItems[kPartyCapacity]{};
    CombatVolatileV22 opponentVolatiles[kOpponentTeamCapacity]{};
    uint32_t rngState = 0xA341316CU; uint16_t turn = 0; uint32_t rewardMoney = 0;
    uint8_t opponentItemUses = 0, gymStage = 0, leagueRegion = 0, leagueStage = 0, unlockedGeneration = 1;
  };
  struct GameSaveV22 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleStateV22 battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy7 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
  };
  struct RecordV22 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV22 payload; uint32_t crc; };
  // Exact battle layout used by save versions 23-25, before persistent
  // weather was added to BattleState in V26.
  struct BattleStateV25 {
    bool active = false; BattleKind kind = BattleKind::None; BattleOutcome outcome = BattleOutcome::None;
    uint32_t playerUid = 0; OwnedPokemon opponents[kOpponentTeamCapacity]{};
    uint8_t opponentCount = 0, opponentIndex = 0, trainerProfileId = 0xFF, gymId = 0xFF;
    CombatVolatile playerVolatile{}; BattleHeldItemState playerHeldItems[kPartyCapacity]{};
    CombatVolatile opponentVolatiles[kOpponentTeamCapacity]{};
    uint32_t rngState = 0xA341316CU; uint16_t turn = 0; uint32_t rewardMoney = 0;
    uint8_t opponentItemUses = 0, gymStage = 0, leagueRegion = 0, leagueStage = 0, unlockedGeneration = 1;
  };
  struct GameSaveV23 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleStateV25 battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy7 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
  };
  struct RecordV23 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV23 payload; uint32_t crc; };
  struct GameSaveV24 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleStateV25 battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy10 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
    uint64_t ownedMachines = 0;
  };
  struct RecordV24 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV24 payload; uint32_t crc; };
  struct GameSaveV25 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleStateV25 battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy10 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
    uint64_t ownedMachines = 0; PlayerStatistics statistics{}; uint32_t martSeenDay = 0;
  };
  struct RecordV25 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV25 payload; uint32_t crc; };
  // Exact V26 layout, before delayed Future Sight/Doom Desire state became
  // persistent in V27.
  struct BattleStateV26 {
    bool active = false; BattleKind kind = BattleKind::None; BattleOutcome outcome = BattleOutcome::None;
    uint32_t playerUid = 0; OwnedPokemon opponents[kOpponentTeamCapacity]{};
    uint8_t opponentCount = 0, opponentIndex = 0, trainerProfileId = 0xFF, gymId = 0xFF;
    CombatVolatile playerVolatile{}; BattleHeldItemState playerHeldItems[kPartyCapacity]{};
    CombatVolatile opponentVolatiles[kOpponentTeamCapacity]{};
    uint32_t rngState = 0xA341316CU; uint16_t turn = 0; uint32_t rewardMoney = 0;
    uint8_t opponentItemUses = 0, gymStage = 0, leagueRegion = 0, leagueStage = 0, unlockedGeneration = 1;
    BattleWeather weather = BattleWeather::Clear; uint8_t weatherTurns = 0;
    bool playerTransformed = false; uint16_t playerOriginalSpeciesId = 0; uint8_t playerOriginalAbilityId = 0;
    MoveId playerOriginalMoves[kMoveSlots]{}; uint8_t playerOriginalMovePp[kMoveSlots]{};
    bool playerAbilityTraced = false; uint8_t playerPreTraceAbilityId = 0;
  };
  static_assert(sizeof(BattleStateV26) == offsetof(BattleState, delayedToPlayer) + 2U,
                "V26 battle migration layout no longer matches the former save ABI");
  struct GameSaveV26 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleStateV26 battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy10 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
    uint64_t ownedMachines = 0; PlayerStatistics statistics{}; uint32_t martSeenDay = 0;
  };
  struct RecordV26 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV26 payload; uint32_t crc; };
  // Exact V27 battle layout, before the two side-wide Safeguard counters were
  // added. It must remain available so an update never discards the one
  // permanent save merely because a battle was in progress.
  struct BattleStateV27 {
    bool active = false; BattleKind kind = BattleKind::None; BattleOutcome outcome = BattleOutcome::None;
    uint32_t playerUid = 0; OwnedPokemon opponents[kOpponentTeamCapacity]{};
    uint8_t opponentCount = 0, opponentIndex = 0, trainerProfileId = 0xFF, gymId = 0xFF;
    CombatVolatile playerVolatile{}; BattleHeldItemState playerHeldItems[kPartyCapacity]{};
    CombatVolatile opponentVolatiles[kOpponentTeamCapacity]{};
    uint32_t rngState = 0xA341316CU; uint16_t turn = 0; uint32_t rewardMoney = 0;
    uint8_t opponentItemUses = 0, gymStage = 0, leagueRegion = 0, leagueStage = 0, unlockedGeneration = 1;
    BattleWeather weather = BattleWeather::Clear; uint8_t weatherTurns = 0;
    bool playerTransformed = false; uint16_t playerOriginalSpeciesId = 0; uint8_t playerOriginalAbilityId = 0;
    MoveId playerOriginalMoves[kMoveSlots]{}; uint8_t playerOriginalMovePp[kMoveSlots]{};
    bool playerAbilityTraced = false; uint8_t playerPreTraceAbilityId = 0;
    DelayedAttackState delayedToPlayer{}; DelayedAttackState delayedToOpponent{};
  };
  struct GameSaveV27 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleStateV27 battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy10 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
    uint64_t ownedMachines = 0; PlayerStatistics statistics{}; uint32_t martSeenDay = 0;
  };
  struct RecordV27 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV27 payload; uint32_t crc; };
  // V28 added Safeguard but predates Reflect, Light Screen and Mist side
  // counters. Keep its byte-exact layout for permanent-save migration.
  struct BattleStateV28 {
    bool active = false; BattleKind kind = BattleKind::None; BattleOutcome outcome = BattleOutcome::None;
    uint32_t playerUid = 0; OwnedPokemon opponents[kOpponentTeamCapacity]{};
    uint8_t opponentCount = 0, opponentIndex = 0, trainerProfileId = 0xFF, gymId = 0xFF;
    CombatVolatile playerVolatile{}; BattleHeldItemState playerHeldItems[kPartyCapacity]{};
    CombatVolatile opponentVolatiles[kOpponentTeamCapacity]{};
    uint32_t rngState = 0xA341316CU; uint16_t turn = 0; uint32_t rewardMoney = 0;
    uint8_t opponentItemUses = 0, gymStage = 0, leagueRegion = 0, leagueStage = 0, unlockedGeneration = 1;
    BattleWeather weather = BattleWeather::Clear; uint8_t weatherTurns = 0;
    bool playerTransformed = false; uint16_t playerOriginalSpeciesId = 0; uint8_t playerOriginalAbilityId = 0;
    MoveId playerOriginalMoves[kMoveSlots]{}; uint8_t playerOriginalMovePp[kMoveSlots]{};
    bool playerAbilityTraced = false; uint8_t playerPreTraceAbilityId = 0;
    DelayedAttackState delayedToPlayer{}; DelayedAttackState delayedToOpponent{};
    uint8_t playerSafeguardTurns = 0, opponentSafeguardTurns = 0;
  };
  struct GameSaveV28 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleStateV28 battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy10 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
    uint64_t ownedMachines = 0; PlayerStatistics statistics{}; uint32_t martSeenDay = 0;
  };
  struct RecordV28 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV28 payload; uint32_t crc; };
  // Early V29 records predate STOCKPILE's counter. CombatVolatile was 32
  // bytes; adding the byte grew every persisted battle by eight bytes (four
  // volatile records, each rounded to uint16 alignment). Keep this byte-exact
  // schema because those saves were already written to beta hardware before
  // the format number was corrected to V30.
  struct CombatVolatileV29 {
    int8_t attackStage = 0, defenseStage = 0, spAttackStage = 0, spDefenseStage = 0;
    int8_t speedStage = 0, accuracyStage = 0, evasionStage = 0;
    uint8_t confusionTurns = 0, trappedTurns = 0, criticalStage = 0;
    bool flinched = false, protectedThisTurn = false, recharging = false;
    uint16_t substituteHp = 0;
    MoveId disabledMove = MoveId::None, encoreMove = MoveId::None, chargingMove = MoveId::None;
    MoveId lastMoveUsed = MoveId::None, choiceMove = MoveId::None;
    HeldItem heldItemOverride = HeldItem::None;
    bool hasHeldItemOverride = false, heldItemSuppressed = false, seeded = false;
    uint8_t toxicCounter = 0, sleepTurns = 0;
  };
  struct BattleStateV29 {
    bool active = false; BattleKind kind = BattleKind::None; BattleOutcome outcome = BattleOutcome::None;
    uint32_t playerUid = 0; OwnedPokemon opponents[kOpponentTeamCapacity]{};
    uint8_t opponentCount = 0, opponentIndex = 0, trainerProfileId = 0xFF, gymId = 0xFF;
    CombatVolatileV29 playerVolatile{}; BattleHeldItemState playerHeldItems[kPartyCapacity]{};
    CombatVolatileV29 opponentVolatiles[kOpponentTeamCapacity]{};
    uint32_t rngState = 0xA341316CU; uint16_t turn = 0; uint32_t rewardMoney = 0;
    uint8_t opponentItemUses = 0, gymStage = 0, leagueRegion = 0, leagueStage = 0, unlockedGeneration = 1;
    BattleWeather weather = BattleWeather::Clear; uint8_t weatherTurns = 0;
    bool playerTransformed = false; uint16_t playerOriginalSpeciesId = 0; uint8_t playerOriginalAbilityId = 0;
    MoveId playerOriginalMoves[kMoveSlots]{}; uint8_t playerOriginalMovePp[kMoveSlots]{};
    bool playerAbilityTraced = false; uint8_t playerPreTraceAbilityId = 0;
    DelayedAttackState delayedToPlayer{}; DelayedAttackState delayedToOpponent{};
    uint8_t playerSafeguardTurns = 0, opponentSafeguardTurns = 0;
    uint8_t playerReflectTurns = 0, opponentReflectTurns = 0;
    uint8_t playerLightScreenTurns = 0, opponentLightScreenTurns = 0;
    uint8_t playerMistTurns = 0, opponentMistTurns = 0;
    uint8_t playerProtectChain = 0, opponentProtectChain = 0;
    bool playerEndureThisTurn = false, opponentEndureThisTurn = false;
    bool playerNightmare = false; bool opponentNightmares[kOpponentTeamCapacity]{};
  };
  struct GameSaveV29 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleStateV29 battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy10 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
    uint64_t ownedMachines = 0; PlayerStatistics statistics{}; uint32_t martSeenDay = 0;
  };
  struct RecordV29 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV29 payload; uint32_t crc; };
  // V30 is the last format before Bide and Encore gained their complete,
  // persistent multi-turn state. Keep its exact layout so an installed beta
  // upgrades without resetting an active battle or the permanent save.
  struct BattleStateV30 {
    bool active = false; BattleKind kind = BattleKind::None; BattleOutcome outcome = BattleOutcome::None;
    uint32_t playerUid = 0; OwnedPokemon opponents[kOpponentTeamCapacity]{};
    uint8_t opponentCount = 0, opponentIndex = 0, trainerProfileId = 0xFF, gymId = 0xFF;
    CombatVolatile playerVolatile{}; BattleHeldItemState playerHeldItems[kPartyCapacity]{};
    CombatVolatile opponentVolatiles[kOpponentTeamCapacity]{};
    uint32_t rngState = 0xA341316CU; uint16_t turn = 0; uint32_t rewardMoney = 0;
    uint8_t opponentItemUses = 0, gymStage = 0, leagueRegion = 0, leagueStage = 0, unlockedGeneration = 1;
    BattleWeather weather = BattleWeather::Clear; uint8_t weatherTurns = 0;
    bool playerTransformed = false; uint16_t playerOriginalSpeciesId = 0; uint8_t playerOriginalAbilityId = 0;
    MoveId playerOriginalMoves[kMoveSlots]{}; uint8_t playerOriginalMovePp[kMoveSlots]{};
    bool playerAbilityTraced = false; uint8_t playerPreTraceAbilityId = 0;
    DelayedAttackState delayedToPlayer{}; DelayedAttackState delayedToOpponent{};
    uint8_t playerSafeguardTurns = 0, opponentSafeguardTurns = 0;
    uint8_t playerReflectTurns = 0, opponentReflectTurns = 0;
    uint8_t playerLightScreenTurns = 0, opponentLightScreenTurns = 0;
    uint8_t playerMistTurns = 0, opponentMistTurns = 0;
    uint8_t playerProtectChain = 0, opponentProtectChain = 0;
    bool playerEndureThisTurn = false, opponentEndureThisTurn = false;
    bool playerNightmare = false; bool opponentNightmares[kOpponentTeamCapacity]{};
  };
  struct GameSaveV30 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleStateV30 battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy10 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
    uint64_t ownedMachines = 0; PlayerStatistics statistics{}; uint32_t martSeenDay = 0;
  };
  struct RecordV30 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV30 payload; uint32_t crc; };
  // V31 is the exact byte prefix of BattleState before the dedicated
  // Teleport/Conversion/Perish/etc. state was appended. Keeping it opaque
  // avoids duplicating another large battle schema while preserving every
  // installed beta save and any battle already in progress.
  struct BattleStateV31 {
    alignas(BattleState) uint8_t bytes[offsetof(BattleState, playerMoveEffects)]{};
  };
  static_assert(sizeof(BattleStateV31)==offsetof(BattleState,playerMoveEffects),
                "V31 battle prefix changed");
  struct GameSaveV31 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleStateV31 battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy10 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
    uint64_t ownedMachines = 0; PlayerStatistics statistics{}; uint32_t martSeenDay = 0;
  };
  struct RecordV31 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV31 payload; uint32_t crc; };
  // V33 ended immediately before playerMimicActive. Mimic was added later to
  // the current V34 BattleState, but inserting it before the Transform
  // snapshots grew offsetof(playerTransformSnapshot) by 16 bytes. Basing a
  // legacy ABI on that moving offset made valid installed V33 saves appear
  // corrupt. Keep the shipped 684-byte prefix frozen permanently.
  struct BattleStateV33 {
    alignas(BattleState) uint8_t bytes[684]{};
  };
  static_assert(offsetof(BattleState, playerMimicActive) == sizeof(BattleStateV33),
                "Fields before the frozen V33 battle prefix changed");
  // V32 is the complete battle/save layout immediately before the
  // five PP-item stacks were appended. It is otherwise byte-for-byte equal to
  // GameSave, so migration only copies these fields and initializes ppItems.
  struct GameSaveV32 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleStateV33 battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy10 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
    uint64_t ownedMachines = 0; PlayerStatistics statistics{}; uint32_t martSeenDay = 0;
  };
  struct RecordV32 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV32 payload; uint32_t crc; };
  // V33 adds the five PP-item stacks but predates complete Transform state.
  struct GameSaveV33 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleStateV33 battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy10 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
    uint64_t ownedMachines = 0; PlayerStatistics statistics{}; uint32_t martSeenDay = 0;
    PpItemInventory ppItems{};
  };
  struct RecordV33 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV33 payload; uint32_t crc; };
  static_assert(sizeof(GameSaveV33) == 25912, "Frozen V33 payload ABI changed");
  static_assert(offsetof(RecordV33, crc) == 25928, "Frozen V33 CRC offset changed");
  static_assert(sizeof(RecordV33) == 25936, "Frozen V33 record ABI changed");
  // V34 is the last format with ten Mart offers. BattleState was already the
  // complete 768-byte implementation, so freeze it as opaque bytes and grow
  // only the Mart when migrating to V35.
  struct BattleStateV34 { alignas(BattleState) uint8_t bytes[768]{}; };
  static_assert(sizeof(BattleState)==sizeof(BattleStateV34),
                "Fields were added to BattleState without freezing a new save ABI");
  struct GameSaveV34 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleStateV34 battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy10 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
    uint64_t ownedMachines = 0; PlayerStatistics statistics{}; uint32_t martSeenDay = 0;
    PpItemInventory ppItems{};
  };
  struct RecordV34 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV34 payload; uint32_t crc; };
  static_assert(sizeof(GameSaveV34)==26000,"Frozen V34 payload ABI changed");
  static_assert(offsetof(RecordV34,crc)==26016,"Frozen V34 CRC offset changed");
  static_assert(sizeof(RecordV34)==26024,"Frozen V34 record ABI changed");
  // V35 enlarged the Mart to fifteen offers and is otherwise the exact
  // current save prefix before persistent Home placement was introduced.
  struct GameSaveV35 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleState battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy15 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
    uint64_t ownedMachines = 0; PlayerStatistics statistics{}; uint32_t martSeenDay = 0;
    PpItemInventory ppItems{};
  };
  struct RecordV35 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV35 payload; uint32_t crc; };
  static_assert(sizeof(GameSaveV35)==26032,"Frozen V35 payload ABI changed");
  static_assert(offsetof(RecordV35,crc)==26048,"Frozen V35 CRC offset changed");
  static_assert(sizeof(RecordV35)==26056,"Frozen V35 record ABI changed");
  // V36 added persistent Home placement and was the final fifteen-offer Mart
  // format. V37 expands only that rotation to twenty products.
  struct GameSaveV36 {
    uint32_t playTimeSeconds = 0, bootCount = 0; uint8_t flags = 0, activePetSlot = 0;
    PokemonCollection collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
    WildEncounterClock wildEncounterClock{}; BattleState battle{}; PokedexState pokedex{};
    GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy15 mart{};
    MoveLearningQueue moveLearning{}; EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
    uint64_t ownedMachines = 0; PlayerStatistics statistics{}; uint32_t martSeenDay = 0;
    PpItemInventory ppItems{}; HomePetPlacementState homePetPlacement{};
  };
  struct RecordV36 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV36 payload; uint32_t crc; };
  static_assert(sizeof(GameSaveV36)==26040,"Frozen V36 payload ABI changed");
  static_assert(offsetof(RecordV36,crc)==26056,"Frozen V36 CRC offset changed");
  static_assert(sizeof(RecordV36)==26064,"Frozen V36 record ABI changed");
  struct Record { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSave payload; uint32_t crc; };
  Preferences preferences_;
  // Kept for the lifetime of the firmware.  A save record is ~28 KiB, so a
  // late allocation after graphics have cached sprites can fail despite the
  // total free heap being sufficient. Reserving it before the renderer starts
  // makes every subsequent persistent save independent of heap fragmentation.
  Record* scratch_ = nullptr;
  uint32_t sequence_ = 0;
  bool nextSlotA_ = true;
  bool readRecord(const char* key, Record& record);
  bool readRecordV9(const char* key, RecordV9& record);
  bool readRecordV10(const char* key, RecordV10& record);
  bool readRecordV11(const char* key, RecordV11& record);
  bool readRecordV21(const char* key, RecordV21& record);
  bool readRecordV22(const char* key, RecordV22& record);
  bool readRecordV23(const char* key, RecordV23& record);
  bool readRecordV24(const char* key, RecordV24& record);
  bool readRecordV25(const char* key, RecordV25& record);
  bool readRecordV26(const char* key, RecordV26& record);
  bool readRecordV27(const char* key, RecordV27& record);
  bool readRecordV28(const char* key, RecordV28& record);
  bool readRecordV29(const char* key, RecordV29& record);
  bool readRecordV30(const char* key, RecordV30& record);
  bool readRecordV31(const char* key, RecordV31& record);
  bool readRecordV32(const char* key, RecordV32& record);
  bool readRecordV33(const char* key, RecordV33& record);
  bool readRecordV34(const char* key, RecordV34& record);
  bool readRecordV35(const char* key, RecordV35& record);
  bool readRecordV36(const char* key, RecordV36& record);
  bool valid(const Record& record) const;
  uint32_t crc32(const uint8_t* data, size_t length) const;
  uint32_t crc32Update(uint32_t state, const uint8_t* data, size_t length) const;
#if defined(ARDUINO_ARCH_ESP32)
  bool inspectCurrentRecord(const char* key, uint32_t& sequence) const;
  bool loadCurrentRecord(const char* key, GameSave& save, uint32_t& sequence) const;
#endif
  bool isNewer(uint32_t a, uint32_t b) const;
};
