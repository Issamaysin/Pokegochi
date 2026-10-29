#pragma once
#include <cstdint>
#include "game/BattleEngine.h"

enum class MartItem : uint8_t { PokeBall, GreatBall, UltraBall, Potion, SuperPotion, HyperPotion, FullHeal, Antidote, ParalyzeHeal, Awakening, BurnHeal, IceHeal, XAttack, XDefense, XSpeed, XAccuracy, DireHit, Revive, MaxRevive, Ether, MaxEther, Elixir, MaxElixir, PpUp, MasterBall, Count };
constexpr uint8_t kLegacyMachineCount = 58;
constexpr uint8_t kSinnohMachineCount = 42;
constexpr uint8_t kUnovaMachineCount = 25;
constexpr uint8_t kKalosMachineCount = 4;
constexpr uint16_t kMachineCount = kLegacyMachineCount + kSinnohMachineCount +
                                   kUnovaMachineCount + kKalosMachineCount;
constexpr uint8_t kMartOfferCount = 20;
constexpr uint16_t kInventoryStackLimit = 999;
struct MartOffer {
  MartItem item=MartItem::PokeBall;
  HeldItem heldItem=HeldItem::None;
  uint16_t price=0;
  uint8_t remaining=0;
  bool isMachine() const { return (static_cast<uint8_t>(heldItem) & 0x80U) != 0; }
  // The original persisted encoding had seven ID bits. Kalos brings the
  // catalog to 129 entries, so MartItem::Count acts as the high ID bit only
  // for machine offers. This keeps sizeof(MartOffer), old saves and every
  // existing machine ID byte-for-byte stable.
  uint16_t machineId() const {
    return static_cast<uint16_t>(static_cast<uint8_t>(heldItem) & 0x7FU) +
           (item == MartItem::Count ? 128U : 0U);
  }
  bool isHeldItem() const { return heldItem != HeldItem::None && !isMachine(); }
  static MartOffer machine(uint16_t id, uint16_t price) {
    return {id >= 128U ? MartItem::Count : MartItem::PokeBall,
            static_cast<HeldItem>(0x80U | (id & 0x7FU)), price, 1};
  }
};
enum class MartOfferCategory : uint8_t {
  PokeBalls, HpRecovery, StatusRecovery, PpRecovery, BattleItems, HeldItems, Machines
};
struct MartState { MartOffer offers[kMartOfferCount]{}; uint32_t elapsedSeconds=86400; uint32_t day=0; uint32_t rng=0x4D415254U; };

class Economy {
 public:
  static constexpr uint32_t kRotationSeconds = 6U * 60U * 60U;
  static constexpr uint32_t kEndgameRotationSeconds = 4U * 60U * 60U;
  static constexpr uint32_t kMasterBallPrice = 99999U;
  static constexpr uint32_t kMasterBallReplacementDenominator = 100000U;
  // Opening the Mart may initialize/refresh offers only when the normal
  // six-hour timer, or the endgame four-hour timer, is due. Sold-out stock is
  // valid state and must not rotate early.
  static bool refreshIfDue(MartState& mart, uint8_t badgeCount = 0,
                           uint64_t ownedMachines = 0,
                           uint64_t ownedSinnohMachines = 0,
                           bool sinnohUnlocked = false,
                           uint32_t ownedUnovaMachines = 0,
                           bool unovaUnlocked = false,
                           bool kalosUnlocked = false);
  static void advance(MartState& mart, uint32_t seconds, uint8_t badgeCount = 0,
                      uint64_t ownedMachines = 0,
                      uint64_t ownedSinnohMachines = 0,
                      bool sinnohUnlocked = false,
                      uint32_t ownedUnovaMachines = 0,
                      bool unovaUnlocked = false,
                      bool kalosUnlocked = false);
  static void rotate(MartState& mart, uint8_t badgeCount = 0, uint64_t ownedMachines = 0,
                     uint64_t ownedSinnohMachines = 0,
                     bool sinnohUnlocked = false,
                     uint32_t ownedUnovaMachines = 0,
                     bool unovaUnlocked = false,
                     bool kalosUnlocked = false);
  static bool buy(MartState& mart, uint8_t offer, uint32_t& money, Inventory& inventory,
                  uint64_t& ownedMachines, PpItemInventory* ppItems = nullptr);
  static bool buy(MartState& mart, uint8_t offer, uint32_t& money, Inventory& inventory,
                  uint64_t& ownedMachines, uint64_t& ownedSinnohMachines,
                  PpItemInventory* ppItems = nullptr);
  static bool buy(MartState& mart, uint8_t offer, uint32_t& money, Inventory& inventory,
                  uint64_t& ownedMachines, uint64_t& ownedSinnohMachines,
                  uint32_t& ownedUnovaMachines, PpItemInventory* ppItems = nullptr);
  static uint8_t maximumPurchasable(const MartState& mart, uint8_t offer, uint32_t money,
                                    const Inventory& inventory, uint64_t ownedMachines,
                                    const PpItemInventory* ppItems = nullptr);
  static uint8_t maximumPurchasable(const MartState& mart, uint8_t offer, uint32_t money,
                                    const Inventory& inventory, uint64_t ownedMachines,
                                    uint64_t ownedSinnohMachines,
                                    const PpItemInventory* ppItems = nullptr);
  static uint8_t maximumPurchasable(const MartState& mart, uint8_t offer, uint32_t money,
                                    const Inventory& inventory, uint64_t ownedMachines,
                                    uint64_t ownedSinnohMachines,
                                    uint32_t ownedUnovaMachines,
                                    const PpItemInventory* ppItems = nullptr);
  static bool buyQuantity(MartState& mart, uint8_t offer, uint8_t quantity, uint32_t& money,
                          Inventory& inventory, uint64_t& ownedMachines,
                          PpItemInventory* ppItems = nullptr);
  static bool buyQuantity(MartState& mart, uint8_t offer, uint8_t quantity, uint32_t& money,
                          Inventory& inventory, uint64_t& ownedMachines,
                          uint64_t& ownedSinnohMachines,
                          PpItemInventory* ppItems = nullptr);
  static bool buyQuantity(MartState& mart, uint8_t offer, uint8_t quantity, uint32_t& money,
                          Inventory& inventory, uint64_t& ownedMachines,
                          uint64_t& ownedSinnohMachines,
                          uint32_t& ownedUnovaMachines,
                          PpItemInventory* ppItems = nullptr);
  static const char* name(MartItem item);
  static const char* name(const MartOffer& offer);
  static uint32_t price(const MartOffer& offer);
  static MartOfferCategory category(const MartOffer& offer);
  static MoveId machineMove(uint16_t machineId);
  static bool canLearnMachine(uint16_t speciesId, uint16_t machineId);
  // Form-aware entry point. Platinum's supported forms inherit the species
  // TM/HM table unchanged; keeping the Pokemon overload prevents callers
  // from accidentally dropping form rules as new form data is added.
  static bool canLearnMachine(const OwnedPokemon& pokemon, uint16_t machineId);
  static bool ownsMachine(uint64_t ownedMachines, uint16_t machineId);
  static bool ownsMachine(uint64_t ownedMachines, uint64_t ownedSinnohMachines,
                          uint16_t machineId);
  static bool ownsMachine(uint64_t ownedMachines, uint64_t ownedSinnohMachines,
                          uint32_t ownedUnovaMachines, uint16_t machineId);
  static bool isHiddenMachine(uint16_t machineId);
  static uint8_t machineNumber(uint16_t machineId);
};
