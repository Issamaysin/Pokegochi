#pragma once

#include <cstdint>
#include "game/GymSystem.h"
#include "game/EggSystem.h"
#include "game/Pokedex.h"

enum class BattleTowerRewardKind : uint8_t {
  None,
  RareCandy,
  SpecialEgg,
  MasterBall,
};

struct BattleTowerRewardResult {
  BattleTowerRewardKind kind = BattleTowerRewardKind::None;
  uint16_t eggSpeciesId = 0;
};

struct BattleTowerMonTemplate {
  uint16_t speciesId;
  uint16_t moves[kMoveSlots];
  HeldItem heldItem;
  uint8_t evSpread;
  PokemonNature nature;
};

struct BattleTowerTrainerDefinition {
  const char* trainerClass;
  const char* name;
  const char* frontAsset;
  const char* introLine1;
  const char* introLine2;
  uint16_t poolOffset;
  uint16_t poolCount;
  uint8_t fixedIv;
};

class BattleTowerSystem {
 public:
  static constexpr uint8_t kStageCount = 3;
  static constexpr uint16_t kTrainerCount = 300;
  static constexpr uint16_t kMonTemplateCount = 882;
  static constexpr uint16_t kSpecialEggOddsDenominator = 200;
  static constexpr uint16_t kMasterBallOddsDenominator = 200;

  // Battle Tower is the repeatable epilogue. It remains locked until the
  // one-time post-Hoenn Mega Stone challenge has been defeated.
  static bool available(const GymProgress& progress);
  // Once available, the Tower is a permanent Home destination. It never uses
  // a timer, invitation roll, Center charge, or VS Seeker charge.
  // Rare Candy is the fallback prize after the third opponent. Separate
  // non-overlapping 1-in-200 buckets replace it with a Master Ball or the
  // Special Egg (when the one-egg incubator is free).
  // The result object makes future percentage rewards an explicit table
  // instead of scattering their rolls through the UI.
  static bool awardCompletionReward(Inventory& inventory);
  static BattleTowerRewardResult awardCompletionReward(
      Inventory& inventory,EggState& egg,const PokedexState& pokedex,uint32_t seed);
  static bool start(BattleState& battle, PokemonCollection& collection, uint32_t playerUid,
                    const GymProgress& progress, uint32_t seed);
  static bool advance(BattleState& battle, PokemonCollection& collection);
  static uint16_t trainerId(const BattleState& battle);
  static const BattleTowerTrainerDefinition* trainer(const BattleState& battle);
  static const BattleTowerTrainerDefinition* trainer(uint16_t id);
  static const BattleTowerMonTemplate* monTemplate(uint16_t id);
};
