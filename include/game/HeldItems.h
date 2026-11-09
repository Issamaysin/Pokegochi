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
  // Generation-VII Memories drive Silvally's RKS System. Normal is the
  // no-item form, so the compact set contains the other seventeen types.
  FightingMemory, FlyingMemory, PoisonMemory, GroundMemory, RockMemory,
  BugMemory, GhostMemory, SteelMemory, FireMemory, WaterMemory, GrassMemory,
  ElectricMemory, PsychicMemory, IceMemory, DragonMemory, DarkMemory,
  FairyMemory,
  // One generic Z-Crystal per type. Signature crystals are progression flags
  // and resolve automatically for their compatible species/move.
  NormaliumZ, FightiniumZ, FlyiniumZ, PoisoniumZ, GroundiumZ, RockiumZ,
  BuginiumZ, GhostiumZ, SteeliumZ, FiriumZ, WateriumZ, GrassiumZ,
  ElectriumZ, PsychiumZ, IciumZ, DragoniumZ, DarkiniumZ, FairiumZ,
  // Serialized tombstone for saves created by the removed custom
  // Ultranecrozium Z mechanic. It is never granted, shown or equipable;
  // keeping the numeric value prevents later enum entries from shifting.
  RemovedUltranecroziumZ,
  // Ordinary held items introduced after Emerald.  They are appended after
  // the retired value so every already-serialized Pokemon keeps its exact
  // item id.  Unlike the signature sidecars above, these are stackable Mart
  // goods and live in ModernHeldItemInventory.
  LifeOrb, FocusSash, ChoiceScarf, ChoiceSpecs, ExpertBelt, BlackSludge,
  ToxicOrb, FlameOrb,
  Eviolite, RockyHelmet, AirBalloon, RedCard, EjectButton,
  AssaultVest, WeaknessPolicy, SafetyGoggles,
  TerrainExtender, ProtectivePads, AdrenalineOrb,
  ElectricSeed, GrassySeed, MistySeed, PsychicSeed,
  // Project-specific Battle Bond catalyst. Appended after every shipped
  // value so old serialized Pokemon and item inventories keep their ids.
  AshsBond,
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
constexpr uint8_t kGen7UniqueItemCount = 36;
constexpr HeldItem kFirstGen7UniqueItem = HeldItem::FightingMemory;
constexpr HeldItem kLastGen7UniqueItem = HeldItem::FairiumZ;
// Bit 35 remains the retired Ultranecrozium Z tombstone. Ash's Bond uses the
// next free bit without changing Gen7UniqueItemInventory's serialized size.
constexpr uint8_t kAshsBondInventoryIndex = 36;
constexpr uint64_t kAshsBondOwnershipBit =
    uint64_t{1} << kAshsBondInventoryIndex;
constexpr uint8_t kModernHeldItemCount = 23;
constexpr HeldItem kFirstModernHeldItem = HeldItem::LifeOrb;
constexpr HeldItem kLastModernHeldItem = HeldItem::PsychicSeed;

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

struct Gen7UniqueItemInventory {
  // A bit is present while the unique item is in the Bag and cleared while
  // equipped. claimedMask prevents campaign rewards from duplicating it.
  uint64_t bagMask = 0;
  uint64_t claimedMask = 0;
};
static_assert(sizeof(Gen7UniqueItemInventory) == 16,
              "Gen 7 unique-item inventory layout changed");

// V51 appends one counted slot for every ordinary Gen-IV--VII held item.
// Keeping this outside Inventory preserves the frozen save prefix used by
// every installed build through V50.
struct ModernHeldItemInventory {
  uint16_t quantities[kModernHeldItemCount]{};
};
static_assert(sizeof(ModernHeldItemInventory) == 46,
              "Modern held-item inventory layout changed");

constexpr uint8_t kDnaSplicersBit = 1U << 0U;
// Kept outside the contiguous held-item bits. It represents the paired
// N-Solarizer/N-Lunarizer key-item function without growing the save record.
constexpr uint64_t kNecrozmaFusersBit = uint64_t{1} << 63U;
// Bit 35 belonged to the removed Ultranecrozium Z. Reconciliation clears it
// from both masks so old saves cannot keep displaying or cloning the item.
constexpr uint64_t kRemovedUltranecroziumZBit = uint64_t{1} << 35U;
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
// Retired serialized values remain in the enum only so old saves keep their
// byte layout. They are not real inventory items.
bool heldItemIsRetired(HeldItem item);
bool heldItemIsConsumable(HeldItem item);
bool heldItemIsBerry(HeldItem item);
bool heldItemBoostsType(HeldItem item, PokemonType type);
uint8_t heldItemBoostPercent(HeldItem item, PokemonType type);
bool heldItemIsArceusPlate(HeldItem item);
PokemonType arceusPlateType(HeldItem item);
bool heldItemIsMemory(HeldItem item);
PokemonType memoryType(HeldItem item);
bool heldItemIsZCrystal(HeldItem item);
PokemonType zCrystalType(HeldItem item);
bool heldItemIsTransferLocked(HeldItem item);
uint8_t heldItemGeneration(HeldItem item);
bool heldItemCanBeEquippedBy(HeldItem item, uint16_t speciesId);
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
int8_t gen7UniqueItemIndex(HeldItem item);
uint16_t gen7UniqueItemQuantity(const Gen7UniqueItemInventory& inventory,
                                HeldItem item);
int8_t modernHeldItemIndex(HeldItem item);
uint16_t modernHeldItemQuantity(const ModernHeldItemInventory& inventory,
                                HeldItem item);

struct OwnedPokemon;
bool discardRetiredHeldItem(OwnedPokemon& pokemon);
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
bool equipHeldItem(OwnedPokemon& pokemon, HeldItem selected,
                   uint16_t inventory[], SpecialHeldItemInventory& special,
                   Gen4UniqueItemInventory& gen4,
                   Gen5UniqueItemInventory& gen5,
                   Gen7UniqueItemInventory& gen7, uint64_t& ownedMachines);
bool unequipHeldItem(OwnedPokemon& pokemon, uint16_t inventory[],
                     SpecialHeldItemInventory& special,
                     Gen4UniqueItemInventory& gen4,
                     Gen5UniqueItemInventory& gen5,
                     Gen7UniqueItemInventory& gen7, uint64_t& ownedMachines);
bool equipHeldItem(OwnedPokemon& pokemon, HeldItem selected,
                   uint16_t inventory[], SpecialHeldItemInventory& special,
                   Gen4UniqueItemInventory& gen4,
                   Gen5UniqueItemInventory& gen5,
                   Gen7UniqueItemInventory& gen7,
                   ModernHeldItemInventory& modern, uint64_t& ownedMachines);
bool unequipHeldItem(OwnedPokemon& pokemon, uint16_t inventory[],
                     SpecialHeldItemInventory& special,
                     Gen4UniqueItemInventory& gen4,
                     Gen5UniqueItemInventory& gen5,
                     Gen7UniqueItemInventory& gen7,
                     ModernHeldItemInventory& modern, uint64_t& ownedMachines);

// Swaps an item from the Bag with the Pokemon's current item. Selecting None
// takes the current item back. A consumed Berry never returns to the Bag.
bool equipHeldItem(HeldItem& equipped, HeldItem selected, uint16_t inventory[],
                   uint64_t& ownedMachines);
// Returns the current item to its proper persistent inventory. Equipable
// unique rewards such as Mega Stone live in ownedMachines, not the counted array.
bool unequipHeldItem(HeldItem& equipped, uint16_t inventory[], uint64_t& ownedMachines);
