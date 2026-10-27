#pragma once

#include <cstdint>

#include "game/BattleEngine.h"
#include "game/SinnohExpedition.h"

enum class UnovaArea : uint8_t {
  PinwheelForest,
  DesertResort,
  ChargestoneCave,
  DriftveilPass,
  CelestialTower,
  TwistMountain,
  MoorOfIcirrus,
  ReversalMountain,
  GiantChasm,
  WhiteTreehollow,
  Count,
};

enum class UnovaRoomKind : uint8_t {
  Trainer,
  ShakingGrass,
  DustCloud,
  GroundShadow,
  BubblingWater,
  HiddenGrotto,
  ClueNpc,
  Healing,
  GateTrainer,
  Boss,
};

enum class UnovaBattleRole : uint8_t { None, Wild, Trainer, GateTrainer, Boss };
enum class UnovaTournamentMode : uint8_t { None, Pwt, Champions, RoyalUnova };

struct UnovaTournamentRunState {
  uint32_t seed = 0;
  uint32_t localDay = UINT32_MAX;
  UnovaTournamentMode mode = UnovaTournamentMode::None;
  uint8_t tier = 0;
  uint8_t round = 0;
  uint8_t rounds = 0;
  uint8_t season = 0;
};

struct UnovaRoomChoice {
  UnovaRoomKind kind = UnovaRoomKind::Trainer;
  uint8_t route = 0;
  bool recommended = false;
};

struct UnovaRoomResult {
  UnovaRoomKind kind = UnovaRoomKind::Trainer;
  bool startsBattle = false;
  bool grantedClue = false;
  bool healed = false;
  bool foundItem = false;
  bool hiddenAbilityEncounter = false;
  bool legendaryEncounter = false;
  uint16_t speciesId = 0;
  // 1 medicine, 2 PP item, 3 held item; the UI awards from its existing Bag
  // inventories after presenting the Hidden Grotto discovery.
  uint8_t rewardKind = 0;
};

struct UnovaRunState {
  uint32_t seed = 0;
  uint16_t visitedRooms = 0;
  uint8_t area = 0;
  uint8_t depth = 0;
  uint8_t gateDepth = 0;
  uint8_t gateRoute = 0;
  uint8_t clueMask = 0;
  uint8_t flags = 0;
  UnovaBattleRole pendingBattle = UnovaBattleRole::None;
  uint8_t pendingPhenomenon = 0;
  uint16_t pendingSpecies = 0;
  uint8_t season = 0;
  uint32_t partyUids[kPartyCapacity]{};
};

struct UnovaAdventureState {
  uint32_t rngState = 0x554E4F56UL; // "UNOV"
  uint16_t unlockedAreas = 0;
  uint16_t defeatedBosses = 0;
  uint8_t pwtCompletedTiers = 0;
  uint8_t flags = 0;
  uint32_t royalUnovaLastDay = UINT32_MAX;
  uint32_t unovaMachineOwnership = 0;
  UnovaRunState run{};
  UnovaTournamentRunState tournament{};
};

class UnovaAdventure {
 public:
  static constexpr uint8_t kAreaCount = static_cast<uint8_t>(UnovaArea::Count);
  static constexpr uint8_t kRoomsPerArea = 7;
  static constexpr uint16_t kAllAreasMask = (1U << kAreaCount) - 1U;
  static constexpr uint16_t kLegendaryChanceDenominator =
      SinnohExpedition::kLegendaryChanceDenominator;
  static constexpr uint16_t kSpecialLegendaryChanceDenominator = 240U;

  static bool available(const SinnohExpeditionState& sinnoh);
  static bool reconcile(UnovaAdventureState& state,
                        const SinnohExpeditionState& sinnoh);
  static bool isAreaUnlocked(const UnovaAdventureState& state, UnovaArea area);
  static bool allAreasCompleted(const UnovaAdventureState& state);
  static bool championsCompleted(const UnovaAdventureState& state);
  static bool white2ThemeUnlocked(const UnovaAdventureState& state);
  static bool royalUnovaUnlocked(const UnovaAdventureState& state);
  static bool setWhite2ThemeSelected(UnovaAdventureState& state, bool selected);
  static bool white2ThemeSelected(const UnovaAdventureState& state);

  static const char* areaName(UnovaArea area);
  static const char* areaSubtitle(UnovaArea area);
  static const char* bossName(UnovaArea area);
  // The ten areas expose two Hidden-Grotto locations for each National-Dex
  // generation. This makes the retroactive Hidden Abilities from Gen I-IV
  // intentionally discoverable instead of mixing hundreds of species into
  // one opaque pool.
  static uint8_t hiddenGrottoGeneration(UnovaArea area);
  static bool begin(UnovaAdventureState& state, UnovaArea area, uint32_t seed,
                    const PokemonCollection& collection,
                    uint8_t season = 0U);
  static bool abandon(UnovaAdventureState& state);
  static bool active(const UnovaAdventureState& state);
  static uint8_t choices(const UnovaAdventureState& state,
                         UnovaRoomChoice output[3]);
  static UnovaRoomResult choose(UnovaAdventureState& state, uint8_t choice,
                                PokemonCollection& collection);
  static bool startPendingBattle(UnovaAdventureState& state,
                                 BattleState& battle,
                                 PokemonCollection& collection,
                                 uint32_t playerUid);
  static bool recordBattle(UnovaAdventureState& state, BattleOutcome outcome);
  static bool useAreaHeal(UnovaAdventureState& state,
                          PokemonCollection& collection);

  static bool pwtTierUnlocked(const UnovaAdventureState& state, uint8_t tier);
  static bool recordPwtVictory(UnovaAdventureState& state, uint8_t tier);
  static bool recordChampionsVictory(UnovaAdventureState& state);
  static uint8_t royalTrainerCount(uint32_t localDay);
  static bool royalAvailableToday(const UnovaAdventureState& state,
                                  uint32_t localDay);
  static bool recordRoyalCompletion(UnovaAdventureState& state,
                                    uint32_t localDay);
};
