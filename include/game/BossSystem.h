#pragma once

#include <cstdint>
#include "game/EggSystem.h"
#include "game/GymSystem.h"
#include "game/MegaEvolution.h"

enum class BossRewardKind : uint8_t {
  None,
  SpecialEgg,
  MasterBall,
  StorageFull,
};

struct BossRewardResult {
  BossRewardKind kind = BossRewardKind::None;
  uint16_t eggSpeciesId = 0;
};

// Compact persistent identity for the current raid-style encounter. Rebuilding
// the combat Pokemon from these authored inputs keeps the save small while
// preserving the exact species, form, Mega variant and remaining HP between
// attempts.
struct BossEncounterState {
  uint32_t personality = 0;
  uint16_t speciesId = 0;
  uint32_t currentHp = 0;
  uint8_t form = 0;
  MegaVariant megaVariant = MegaVariant::None;
  uint16_t reserved = 0;
};
static_assert(sizeof(BossEncounterState) == 16,
              "Boss encounter save layout changed");

// Repeatable endgame encounter unlocked by defeating the Mega Challenge.
// Bosses use ordinary legal battle stats at level 100; only their HP pool is
// replaced by the explicit raid-style value requested by the game design.
class BossSystem {
 public:
  static constexpr uint32_t kMaximumHp = 200000UL;
  static constexpr uint16_t kBattleHpLayer = UINT16_MAX;
  static constexpr uint32_t kCooldownSeconds = 12U * 60U * 60U;
  static constexpr uint8_t kRewardBucketCount = 10U;
  static constexpr uint8_t kMasterBallRewardBuckets = 1U;
  static constexpr uint8_t kSpecialEggRewardBuckets = 9U;

  static bool available(const GymProgress& progress);
  static bool ready(const GymProgress& progress, uint32_t cooldownSecondsRemaining);
  static void beginCooldown(uint32_t& cooldownSecondsRemaining);
  static void advanceCooldown(uint32_t& cooldownSecondsRemaining,
                              uint32_t elapsedSeconds);
  static bool hasEncounter(const BossEncounterState& encounter);
  static void ensureEncounter(BossEncounterState& encounter, uint32_t seed);
  static OwnedPokemon encounterPokemon(const BossEncounterState& encounter);
  static uint32_t battleCurrentHp(const BattleState& battle);
  static void recordAttempt(BossEncounterState& encounter,
                            const BattleState& battle, bool defeated);
  // With both destinations available, one of ten equally sized buckets gives
  // a Master Ball and the other nine grant the same Special Egg used by the
  // Battle Tower's 0.5% prize. A blocked destination falls back to the other
  // prize instead of destroying an existing Egg or overflowing an item stack.
  static BossRewardResult awardVictoryReward(
      Inventory& inventory, EggInventory& eggs, const PokedexState& pokedex,
      uint8_t unlockedGeneration, uint32_t seed);
  static bool start(BattleState& battle, PokemonCollection& collection,
                    uint32_t playerUid, const GymProgress& progress,
                    uint32_t cooldownSecondsRemaining,
                    BossEncounterState& encounter, uint32_t seed);

  static bool isLegendaryOrMythical(uint16_t speciesId);
  static bool moveAllowed(MoveId move);
};
