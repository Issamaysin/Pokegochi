#pragma once

#include <cstdint>

#include "game/BattleEngine.h"
#include "game/UnovaAdventure.h"

enum class KalosArea : uint8_t {
  SantaluneForest,
  CyllageCliffs,
  GeosengeTrail,
  CoumarineCoast,
  LumioseCity,
  LaverreMarsh,
  AnistarPath,
  SnowbelleWoods,
  PokemonLeague,
  Count,
};

enum class KalosRoomKind : uint8_t {
  Trainer,
  TallGrass,
  FlowerPatch,
  Horde,
  FriendSafari,
  Item,
  Healing,
  TeamFlare,
  Leader,
};

enum class KalosBattleRole : uint8_t {
  None,
  Wild,
  Trainer,
  TeamFlare,
  Leader,
};

struct KalosRoomChoice {
  KalosRoomKind kind = KalosRoomKind::Trainer;
  uint8_t route = 0;
  bool recommended = false;
};

struct KalosRoomResult {
  KalosRoomKind kind = KalosRoomKind::Trainer;
  bool startsBattle = false;
  bool healed = false;
  bool foundItem = false;
  bool hiddenAbilityEncounter = false;
  bool legendaryEncounter = false;
  uint16_t speciesId = 0;
  uint8_t rewardKind = 0;
};

struct KalosRunState {
  uint32_t seed = 0;
  uint8_t area = 0;
  uint8_t step = 0;
  uint8_t flags = 0;
  KalosBattleRole pendingBattle = KalosBattleRole::None;
  uint8_t pendingFlags = 0;
  uint16_t pendingSpecies = 0;
  uint32_t partyUids[kPartyCapacity]{};
};

struct KalosAdventureState {
  uint32_t rngState = 0x4B414C4FUL; // "KALO"
  uint16_t unlockedAreas = 0;
  uint16_t defeatedBosses = 0;
  uint8_t flags = 0;
  KalosRunState run{};
};

class KalosAdventure {
 public:
  static constexpr uint8_t kAreaCount = static_cast<uint8_t>(KalosArea::Count);
  static constexpr uint8_t kStepsPerArea = 5U;
  static constexpr uint16_t kAllAreasMask = (1U << kAreaCount) - 1U;
  static constexpr uint16_t kLegendaryChanceDenominator = 240U;
  static constexpr uint8_t kMasterBallRewardBit = 1U << 2U;

  static bool available(const UnovaAdventureState& unova);
  static bool reconcile(KalosAdventureState& state,
                        const UnovaAdventureState& unova);
  static bool isAreaUnlocked(const KalosAdventureState& state, KalosArea area);
  static bool championCompleted(const KalosAdventureState& state);
  // Every Kalos route is a repeatable mission; defeating all nine route
  // leaders once (including Diantha) is regional 100%.
  static bool fullyCompleted(const KalosAdventureState& state);
  static bool reconcileCompletionReward(KalosAdventureState& state,
                                        Inventory& inventory);
  static bool xThemeUnlocked(const KalosAdventureState& state);
  static bool setXThemeSelected(KalosAdventureState& state, bool selected);
  static bool xThemeSelected(const KalosAdventureState& state);
  static uint8_t badgeCount(const KalosAdventureState& state);
  static const char* areaName(KalosArea area);
  static const char* areaSubtitle(KalosArea area);
  static const char* bossName(KalosArea area);

  static bool begin(KalosAdventureState& state, KalosArea area, uint32_t seed,
                    const PokemonCollection& collection);
  static bool abandon(KalosAdventureState& state);
  static bool active(const KalosAdventureState& state);
  static uint8_t choices(const KalosAdventureState& state,
                         KalosRoomChoice output[2]);
  static KalosRoomResult choose(KalosAdventureState& state, uint8_t choice,
                                PokemonCollection& collection);
  static bool startPendingBattle(KalosAdventureState& state,
                                 BattleState& battle,
                                 PokemonCollection& collection,
                                 uint32_t playerUid);
  static bool recordBattle(KalosAdventureState& state, BattleOutcome outcome);
  static bool useAreaHeal(KalosAdventureState& state,
                          PokemonCollection& collection);
};
