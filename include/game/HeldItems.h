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
  // Generation-IV signature items. These are appended so every previously
  // serialized held-item value keeps its meaning in old Pokemon records.
  FistPlate, SkyPlate, ToxicPlate, StonePlate, InsectPlate, SpookyPlate,
  DracoPlate, IronPlate, FlamePlate, SplashPlate, MeadowPlate, ZapPlate,
  MindPlate, IciclePlate, EarthPlate, PixiePlate, DreadPlate,
  AdamantOrb, LustrousOrb, GriseousOrb,
  // Generation-V Drives are unique Genesect form items. Appending keeps all
  // previously serialized HeldItem values byte-for-byte stable.
  DouseDrive, ShockDrive, BurnDrive, ChillDrive,
  Count,
};

constexpr uint8_t kHeldItemInventorySlots = static_cast<uint8_t>(HeldItem::MegaStone);
constexpr uint64_t kMegaStoneOwnershipBit = uint64_t{1} << 63U;
constexpr uint64_t kLuckyEggOwnershipBit = uint64_t{1} << 62U;
constexpr uint8_t kSpecialHeldItemCount = 9;
constexpr uint8_t kGen4UniqueItemCount = 20;
constexpr HeldItem kFirstGen4UniqueItem = HeldItem::FistPlate;
constexpr HeldItem kLastGen4UniqueItem = HeldItem::GriseousOrb;
constexpr uint8_t kGen5DriveCount = 4;
constexpr HeldItem kFirstGen5Drive = HeldItem::DouseDrive;
constexpr HeldItem kLastGen5Drive = HeldItem::ChillDrive;

struct SpecialHeldItemInventory {
  uint16_t quantities[kSpecialHeldItemCount]{};
  // One bit per reward prevents releasing/trading a matching species from
  // producing unlimited copies when the collection is reconciled again.
  uint16_t claimedMask = 0;
};

// Gen 4 species rewards are kept in a new sidecar so the frozen V38--V43
// save records retain their exact SpecialHeldItemInventory ABI. Each entry is
// normally one copy, but quantities make equip/take-item swaps lossless.
struct Gen4UniqueItemInventory {
  uint16_t quantities[kGen4UniqueItemCount]{};
  uint32_t claimedMask = 0;
};
static_assert(sizeof(Gen4UniqueItemInventory) == 44,
              "Gen 4 unique-item inventory layout changed");

struct Gen5UniqueItemInventory {
  uint16_t driveQuantities[kGen5DriveCount]{};
  // Bit 0 stores DNA Splicers. Bit 1 is reserved for compatibility with saves
  // created while Reveal Glass was a separate item. Claimed bits prevent a
  // released legendary from manufacturing another copy during reconciliation.
  uint8_t keyItems = 0;
  uint8_t claimedMask = 0;
};
static_assert(sizeof(Gen5UniqueItemInventory) == 10,
              "Gen 5 unique-item inventory layout changed");

constexpr uint8_t kDnaSplicersBit = 1U << 0U;
// Legacy save bit: intentionally preserved in the serialized layout, but no
// longer granted, shown or required by gameplay.
constexpr uint8_t kRevealGlassBit = 1U << 1U;

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
uint8_t heldItemBoostPercent(HeldItem item, PokemonType type);
bool heldItemIsArceusPlate(HeldItem item);
PokemonType arceusPlateType(HeldItem item);
bool heldItemIsTransferLocked(HeldItem item);
uint8_t heldItemCount();
int8_t specialHeldItemIndex(HeldItem item);
uint16_t specialHeldItemQuantity(const SpecialHeldItemInventory& inventory,
                                 HeldItem item);
int8_t gen4UniqueItemIndex(HeldItem item);
uint16_t gen4UniqueItemQuantity(const Gen4UniqueItemInventory& inventory,
                                HeldItem item);
int8_t gen5DriveIndex(HeldItem item);
uint16_t gen5DriveQuantity(const Gen5UniqueItemInventory& inventory,
                           HeldItem item);

struct OwnedPokemon;
bool equipHeldItem(OwnedPokemon& pokemon, HeldItem selected,
                   uint16_t inventory[], SpecialHeldItemInventory& special,
                   uint64_t& ownedMachines);
bool unequipHeldItem(OwnedPokemon& pokemon, uint16_t inventory[],
                     SpecialHeldItemInventory& special,
                     uint64_t& ownedMachines);
bool equipHeldItem(OwnedPokemon& pokemon, HeldItem selected,
                   uint16_t inventory[], SpecialHeldItemInventory& special,
                   Gen4UniqueItemInventory& gen4, uint64_t& ownedMachines);
bool unequipHeldItem(OwnedPokemon& pokemon, uint16_t inventory[],
                     SpecialHeldItemInventory& special,
                     Gen4UniqueItemInventory& gen4, uint64_t& ownedMachines);
bool equipHeldItem(OwnedPokemon& pokemon, HeldItem selected,
                   uint16_t inventory[], SpecialHeldItemInventory& special,
                   Gen4UniqueItemInventory& gen4,
                   Gen5UniqueItemInventory& gen5, uint64_t& ownedMachines);
bool unequipHeldItem(OwnedPokemon& pokemon, uint16_t inventory[],
                     SpecialHeldItemInventory& special,
                     Gen4UniqueItemInventory& gen4,
                     Gen5UniqueItemInventory& gen5, uint64_t& ownedMachines);

// Swaps an item from the Bag with the Pokemon's current item. Selecting None
// takes the current item back. A consumed Berry never returns to the Bag.
bool equipHeldItem(HeldItem& equipped, HeldItem selected, uint16_t inventory[],
                   uint64_t& ownedMachines);
// Returns the current item to its proper persistent inventory. Equipable
// unique rewards such as Mega Stone live in ownedMachines, not the counted array.
bool unequipHeldItem(HeldItem& equipped, uint16_t inventory[], uint64_t& ownedMachines);
