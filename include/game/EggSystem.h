#pragma once
#include <cstdint>
#include "game/Collection.h"
#include "game/GymSystem.h"
#include "game/Pokedex.h"

enum class EggRarity:uint8_t{Common,Uncommon,Rare,Special};
struct EggState{
  bool offerPending=false;bool active=false;
  uint16_t speciesId=0;uint32_t remainingSeconds=0;
  uint32_t dailyBonusUsedSeconds=0;uint32_t bonusWindowSeconds=86400;
  uint32_t shinyRoll=1;uint8_t region=1;EggRarity rarity=EggRarity::Common;
  uint8_t giftResolvedBits=0;
};

class EggSystem{
 public:
  static constexpr uint32_t kCommonSeconds=24U*60U*60U;
  static constexpr uint32_t kUncommonSeconds=48U*60U*60U;
  static constexpr uint32_t kRareSeconds=96U*60U*60U;
  static constexpr uint32_t kDailyBonusLimitSeconds=2U*60U*60U;
  static bool daycareUnlocked(const GymProgress&,uint8_t region);
  static bool offerRegionalGift(EggState&,const GymProgress&,uint32_t seed);
  static bool offerAfterTrainer(EggState&,const GymProgress&,uint32_t seed);
  // Battle Tower's exceptional prize bypasses the daycare offer screen and
  // starts incubating immediately.  It chooses only starters the player has
  // never owned; after all nine are registered as caught it uses the complete
  // Generation-I-to-III legendary/mythical pool instead.
  static bool grantBattleTowerSpecial(EggState&,const PokedexState&,uint32_t seed);
  static bool accept(EggState&);static void reject(EggState&);
  static void advance(EggState&,uint32_t seconds,
                      const PokemonCollection* collection=nullptr);
  static uint32_t addInteractionBonus(EggState&);
  static uint32_t addBattleBonus(EggState&);
  static bool ready(const EggState& e){return e.active&&e.remainingSeconds==0;}
  // Eggs begin at the ace level of the most recently defeated Gym. Before
  // the first Badge they retain the traditional Lv.5 start, and the result
  // is capped at Lv.89 so hatching can never bypass the final level stretch.
  static uint8_t hatchLevel(const GymProgress&);
  static bool hatch(EggState&,PokemonCollection&,const GymProgress&,
                    uint32_t* uid=nullptr);
  static const char* stageText(const EggState&);
  static const char* rarityName(EggRarity);
};
