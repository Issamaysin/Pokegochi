#pragma once
#include <cstdint>
#include "game/PokemonData.h"

enum class HeldItem : uint8_t {
  None,
  OranBerry, SitrusBerry, LumBerry, PersimBerry, CheriBerry,
  ChestoBerry, PechaBerry, RawstBerry, AspearBerry, WhiteHerb,
  Leftovers, ShellBell, ChoiceBand, QuickClaw, ScopeLens,
  BrightPowder, FocusBand, KingsRock, AmuletCoin, SmokeBall,
  SilkScarf, BlackBelt, SharpBeak, PoisonBarb, SoftSand,
  HardStone, SilverPowder, SpellTag, MetalCoat, Charcoal,
  MysticWater, MiracleSeed, Magnet, TwistedSpoon, NeverMeltIce,
  DragonFang, BlackGlasses,
  // One universal stone deliberately replaces the per-species stones. It is
  // stored outside Inventory::heldItems so adding it does not resize V32
  // saves. The item is unique, cannot be sold, stolen, knocked off or traded.
  MegaStone,
  // Emerald Battle Tower opponents use the complete Generation-III held-item
  // table. These entries are battle-only: appending them after MegaStone
  // preserves every player inventory/save index and keeps them out of shops.
  BerryJuice, MachoBrace, SoulDew, DeepSeaScale, DeepSeaTooth, LightBall,
  LeppaBerry, MentalHerb, DragonScale, IapapaBerry, WikiBerry, SeaIncense,
  SalacBerry, LansatBerry, ApicotBerry, StarfBerry, LiechiBerry, Stick,
  LaxIncense, AguavBerry, FigyBerry, ThickClub, MagoBerry, MetalPowder,
  PetayaBerry, LuckyPunch, GanlonBerry,
  // Appended so every previously serialized held-item value remains stable.
  // This is a unique player reward, stored in ownedMachines like MegaStone.
  LuckyEgg,
  // Kept after every shipped value so old serialized enums remain stable.
  QuickPowder,
  Count,
};

constexpr uint8_t kHeldItemInventorySlots = static_cast<uint8_t>(HeldItem::MegaStone);
constexpr uint64_t kMegaStoneOwnershipBit = uint64_t{1} << 63U;
constexpr uint64_t kLuckyEggOwnershipBit = uint64_t{1} << 62U;
constexpr uint8_t kSpecialHeldItemCount = 9;

struct SpecialHeldItemInventory {
  uint16_t quantities[kSpecialHeldItemCount]{};
  // One bit per reward prevents releasing/trading a matching species from
  // producing unlimited copies when the collection is reconciled again.
  uint16_t claimedMask = 0;
};

constexpr uint64_t uniqueHeldItemOwnershipBit(HeldItem item) {
  return item == HeldItem::MegaStone ? kMegaStoneOwnershipBit
       : item == HeldItem::LuckyEgg ? kLuckyEggOwnershipBit : 0U;
}

struct HeldItemData {
  HeldItem id;
  const char* name;
  uint16_t price;
  uint8_t unlockBadges;
  uint8_t martWeight;
  bool consumable;
  bool boostsType;
  PokemonType boostedType;
};

const HeldItemData* heldItemData(HeldItem item);
const char* heldItemName(HeldItem item);
const char* heldItemDescription(HeldItem item);
bool heldItemIsConsumable(HeldItem item);
bool heldItemIsBerry(HeldItem item);
bool heldItemBoostsType(HeldItem item, PokemonType type);
bool heldItemIsTransferLocked(HeldItem item);
uint8_t heldItemCount();
int8_t specialHeldItemIndex(HeldItem item);
uint16_t specialHeldItemQuantity(const SpecialHeldItemInventory& inventory,
                                 HeldItem item);

struct OwnedPokemon;
bool equipHeldItem(OwnedPokemon& pokemon, HeldItem selected,
                   uint16_t inventory[], SpecialHeldItemInventory& special,
                   uint64_t& ownedMachines);
bool unequipHeldItem(OwnedPokemon& pokemon, uint16_t inventory[],
                     SpecialHeldItemInventory& special,
                     uint64_t& ownedMachines);

// Swaps an item from the Bag with the Pokemon's current item. Selecting None
// takes the current item back. A consumed Berry never returns to the Bag.
bool equipHeldItem(HeldItem& equipped, HeldItem selected, uint16_t inventory[],
                   uint64_t& ownedMachines);
// Returns the current item to its proper persistent inventory. Equipable
// unique rewards such as Mega Stone live in ownedMachines, not the counted array.
bool unequipHeldItem(HeldItem& equipped, uint16_t inventory[], uint64_t& ownedMachines);
