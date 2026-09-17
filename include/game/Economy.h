#pragma once
#include <cstdint>
#include "game/BattleEngine.h"

enum class MartItem : uint8_t { PokeBall, GreatBall, UltraBall, Potion, SuperPotion, HyperPotion, FullHeal, Antidote, ParalyzeHeal, Awakening, BurnHeal, IceHeal, XAttack, XDefense, XSpeed, XAccuracy, DireHit, Revive, MaxRevive, Ether, MaxEther, Elixir, MaxElixir, PpUp, MasterBall, Count };
constexpr uint8_t kMachineCount = 58;
constexpr uint8_t kMartOfferCount = 20;
constexpr uint16_t kInventoryStackLimit = 999;
struct MartOffer {
  MartItem item=MartItem::PokeBall;
  HeldItem heldItem=HeldItem::None;
  uint16_t price=0;
  uint8_t remaining=0;
  bool isMachine() const { return (static_cast<uint8_t>(heldItem) & 0x80U) != 0; }
  uint8_t machineId() const { return static_cast<uint8_t>(heldItem) & 0x7FU; }
  bool isHeldItem() const { return heldItem != HeldItem::None && !isMachine(); }
  static MartOffer machine(uint8_t id, uint16_t price) {
    return {MartItem::PokeBall, static_cast<HeldItem>(0x80U | id), price, 1};
  }
};
enum class MartOfferCategory : uint8_t {
  PokeBalls, HpRecovery, StatusRecovery, PpRecovery, BattleItems, HeldItems, Machines
};
struct MartState { MartOffer offers[kMartOfferCount]{}; uint32_t elapsedSeconds=86400; uint32_t day=0; uint32_t rng=0x4D415254U; };

class Economy {
 public:
  static constexpr uint32_t kRotationSeconds = 6U * 60U * 60U;
  static constexpr uint32_t kMasterBallPrice = 99999U;
  static constexpr uint32_t kMasterBallReplacementDenominator = 100000U;
  static void advance(MartState& mart, uint32_t seconds, uint8_t badgeCount = 0,
                      uint64_t ownedMachines = 0);
  static void rotate(MartState& mart, uint8_t badgeCount = 0, uint64_t ownedMachines = 0);
  static bool buy(MartState& mart, uint8_t offer, uint32_t& money, Inventory& inventory,
                  uint64_t& ownedMachines, PpItemInventory* ppItems = nullptr);
  static uint8_t maximumPurchasable(const MartState& mart, uint8_t offer, uint32_t money,
                                    const Inventory& inventory, uint64_t ownedMachines,
                                    const PpItemInventory* ppItems = nullptr);
  static bool buyQuantity(MartState& mart, uint8_t offer, uint8_t quantity, uint32_t& money,
                          Inventory& inventory, uint64_t& ownedMachines,
                          PpItemInventory* ppItems = nullptr);
  static const char* name(MartItem item);
  static const char* name(const MartOffer& offer);
  static uint32_t price(const MartOffer& offer);
  static MartOfferCategory category(const MartOffer& offer);
  static MoveId machineMove(uint8_t machineId);
  static bool canLearnMachine(uint16_t speciesId, uint8_t machineId);
  static bool ownsMachine(uint64_t ownedMachines, uint8_t machineId);
};
