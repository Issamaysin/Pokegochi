#pragma once
#include <cstdint>
#include "game/BattleEngine.h"

enum class MartItem : uint8_t { PokeBall, GreatBall, UltraBall, Potion, SuperPotion, HyperPotion, FullHeal, Antidote, ParalyzeHeal, Awakening, BurnHeal, IceHeal, XAttack, XDefense, XSpeed, XAccuracy, DireHit, Count };
constexpr uint8_t kMartOfferCount = 7;
struct MartOffer { MartItem item=MartItem::PokeBall; uint16_t price=0; uint8_t remaining=0; };
struct MartState { MartOffer offers[kMartOfferCount]{}; uint32_t elapsedSeconds=86400; uint32_t day=0; uint32_t rng=0x4D415254U; };

class Economy {
 public:
  static void advance(MartState& mart, uint32_t seconds);
  static void rotate(MartState& mart);
  static bool buy(MartState& mart, uint8_t offer, uint32_t& money, Inventory& inventory);
  static const char* name(MartItem item);
};
