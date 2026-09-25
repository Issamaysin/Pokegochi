#include "game/HeldItems.h"
#include "game/Collection.h"

namespace {
constexpr HeldItemData kItems[] = {
  {HeldItem::OranBerry,"ORAN BERRY",20,0,20,true,false,PokemonType::Normal},
  {HeldItem::SitrusBerry,"SITRUS BERRY",200,2,12,true,false,PokemonType::Normal},
  {HeldItem::LumBerry,"LUM BERRY",800,7,2,true,false,PokemonType::Normal},
  {HeldItem::PersimBerry,"PERSIM BERRY",80,1,12,true,false,PokemonType::Normal},
  {HeldItem::CheriBerry,"CHERI BERRY",80,0,15,true,false,PokemonType::Normal},
  {HeldItem::ChestoBerry,"CHESTO BERRY",80,0,15,true,false,PokemonType::Normal},
  {HeldItem::PechaBerry,"PECHA BERRY",80,0,15,true,false,PokemonType::Normal},
  {HeldItem::RawstBerry,"RAWST BERRY",80,1,15,true,false,PokemonType::Normal},
  {HeldItem::AspearBerry,"ASPEAR BERRY",80,1,15,true,false,PokemonType::Normal},
  {HeldItem::WhiteHerb,"WHITE HERB",600,3,5,true,false,PokemonType::Normal},
  {HeldItem::Leftovers,"LEFTOVERS",4000,6,1,false,false,PokemonType::Normal},
  {HeldItem::ShellBell,"SHELL BELL",3000,4,2,false,false,PokemonType::Normal},
  {HeldItem::ChoiceBand,"CHOICE BAND",5000,8,1,false,false,PokemonType::Normal},
  {HeldItem::QuickClaw,"QUICK CLAW",2500,6,2,false,false,PokemonType::Normal},
  {HeldItem::ScopeLens,"SCOPE LENS",2500,5,2,false,false,PokemonType::Normal},
  {HeldItem::BrightPowder,"BRIGHTPOWDER",3000,6,2,false,false,PokemonType::Normal},
  {HeldItem::FocusBand,"FOCUS BAND",3000,7,1,false,false,PokemonType::Normal},
  {HeldItem::KingsRock,"KING'S ROCK",3000,5,2,false,false,PokemonType::Normal},
  {HeldItem::AmuletCoin,"AMULET COIN",4000,6,1,false,false,PokemonType::Normal},
  {HeldItem::SmokeBall,"SMOKE BALL",1000,6,3,false,false,PokemonType::Normal},
  {HeldItem::SilkScarf,"SILK SCARF",1000,1,5,false,true,PokemonType::Normal},
  {HeldItem::BlackBelt,"BLACK BELT",1000,3,4,false,true,PokemonType::Fighting},
  {HeldItem::SharpBeak,"SHARP BEAK",1000,2,4,false,true,PokemonType::Flying},
  {HeldItem::PoisonBarb,"POISON BARB",1000,3,4,false,true,PokemonType::Poison},
  {HeldItem::SoftSand,"SOFT SAND",1000,3,4,false,true,PokemonType::Ground},
  {HeldItem::HardStone,"HARD STONE",1000,3,4,false,true,PokemonType::Rock},
  {HeldItem::SilverPowder,"SILVERPOWDER",1000,2,4,false,true,PokemonType::Bug},
  {HeldItem::SpellTag,"SPELL TAG",1000,4,4,false,true,PokemonType::Ghost},
  {HeldItem::MetalCoat,"METAL COAT",1500,5,3,false,true,PokemonType::Steel},
  {HeldItem::Charcoal,"CHARCOAL",1000,2,4,false,true,PokemonType::Fire},
  {HeldItem::MysticWater,"MYSTIC WATER",1000,2,4,false,true,PokemonType::Water},
  {HeldItem::MiracleSeed,"MIRACLE SEED",1000,2,4,false,true,PokemonType::Grass},
  {HeldItem::Magnet,"MAGNET",1000,4,4,false,true,PokemonType::Electric},
  {HeldItem::TwistedSpoon,"TWISTEDSPOON",1000,4,4,false,true,PokemonType::Psychic},
  {HeldItem::NeverMeltIce,"NEVERMELTICE",1000,4,4,false,true,PokemonType::Ice},
  {HeldItem::DragonFang,"DRAGON FANG",1500,5,3,false,true,PokemonType::Dragon},
  {HeldItem::BlackGlasses,"BLACKGLASSES",1000,5,3,false,true,PokemonType::Dark},
  {HeldItem::MegaStone,"MEGA STONE",0,255,0,false,false,PokemonType::Normal},
  {HeldItem::BerryJuice,"BERRY JUICE",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::MachoBrace,"MACHO BRACE",0,255,0,false,false,PokemonType::Normal},
  {HeldItem::SoulDew,"SOUL DEW",0,255,0,false,false,PokemonType::Normal},
  {HeldItem::DeepSeaScale,"DEEPSEASCALE",0,255,0,false,false,PokemonType::Normal},
  {HeldItem::DeepSeaTooth,"DEEPSEATOOTH",0,255,0,false,false,PokemonType::Normal},
  {HeldItem::LightBall,"LIGHT BALL",0,255,0,false,false,PokemonType::Normal},
  {HeldItem::LeppaBerry,"LEPPA BERRY",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::MentalHerb,"MENTAL HERB",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::DragonScale,"DRAGON SCALE",0,255,0,false,false,PokemonType::Normal},
  {HeldItem::IapapaBerry,"IAPAPA BERRY",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::WikiBerry,"WIKI BERRY",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::SeaIncense,"SEA INCENSE",0,255,0,false,true,PokemonType::Water},
  {HeldItem::SalacBerry,"SALAC BERRY",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::LansatBerry,"LANSAT BERRY",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::ApicotBerry,"APICOT BERRY",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::StarfBerry,"STARF BERRY",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::LiechiBerry,"LIECHI BERRY",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::Stick,"STICK",0,255,0,false,false,PokemonType::Normal},
  {HeldItem::LaxIncense,"LAX INCENSE",0,255,0,false,false,PokemonType::Normal},
  {HeldItem::AguavBerry,"AGUAV BERRY",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::FigyBerry,"FIGY BERRY",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::ThickClub,"THICK CLUB",0,255,0,false,false,PokemonType::Normal},
  {HeldItem::MagoBerry,"MAGO BERRY",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::MetalPowder,"METAL POWDER",0,255,0,false,false,PokemonType::Normal},
  {HeldItem::PetayaBerry,"PETAYA BERRY",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::LuckyPunch,"LUCKY PUNCH",0,255,0,false,false,PokemonType::Normal},
  {HeldItem::GanlonBerry,"GANLON BERRY",0,255,0,true,false,PokemonType::Normal},
  {HeldItem::LuckyEgg,"LUCKY EGG",0,255,0,false,false,PokemonType::Normal},
  {HeldItem::QuickPowder,"QUICK POWDER",0,255,0,false,false,PokemonType::Normal},
};
static_assert(sizeof(kItems)/sizeof(kItems[0]) == static_cast<uint8_t>(HeldItem::Count)-1U,
              "Held item catalog must cover every item");
}

const HeldItemData* heldItemData(HeldItem item) {
  const uint8_t index=static_cast<uint8_t>(item);
  return index>0&&index<static_cast<uint8_t>(HeldItem::Count)?&kItems[index-1U]:nullptr;
}
const char* heldItemName(HeldItem item) { const HeldItemData* data=heldItemData(item);return data?data->name:"NONE"; }
const char* heldItemDescription(HeldItem item) {
  switch (item) {
    case HeldItem::OranBerry: return "RESTORES 10 HP AUTOMATICALLY WHEN HP FALLS BELOW HALF.";
    case HeldItem::SitrusBerry: return "RESTORES 30 HP AUTOMATICALLY WHEN HP FALLS BELOW HALF.";
    case HeldItem::LumBerry: return "CURES ANY STATUS PROBLEM OR CONFUSION AUTOMATICALLY.";
    case HeldItem::PersimBerry: return "CURES CONFUSION AUTOMATICALLY.";
    case HeldItem::CheriBerry: return "CURES PARALYSIS AUTOMATICALLY.";
    case HeldItem::ChestoBerry: return "WAKES A SLEEPING POKEMON AUTOMATICALLY.";
    case HeldItem::PechaBerry: return "CURES POISONING AUTOMATICALLY.";
    case HeldItem::RawstBerry: return "HEALS A BURN AUTOMATICALLY.";
    case HeldItem::AspearBerry: return "THAWS A FROZEN POKEMON AUTOMATICALLY.";
    case HeldItem::WhiteHerb: return "RESTORES LOWERED STATS ONCE DURING BATTLE.";
    case HeldItem::Leftovers: return "RESTORES 1/16 OF MAX HP AT THE END OF EACH TURN.";
    case HeldItem::ShellBell: return "RESTORES HP EQUAL TO 1/8 OF DAMAGE DEALT.";
    case HeldItem::ChoiceBand: return "BOOSTS PHYSICAL ATTACK BY 50%, BUT LOCKS ONE MOVE.";
    case HeldItem::QuickClaw: return "MAY LET ITS HOLDER MOVE FIRST.";
    case HeldItem::ScopeLens: return "RAISES THE HOLDER'S CRITICAL-HIT CHANCE.";
    case HeldItem::BrightPowder: return "MAKES MOVES USED AGAINST THE HOLDER LESS ACCURATE.";
    case HeldItem::FocusBand: return "MAY SURVIVE A KNOCKOUT HIT WITH 1 HP.";
    case HeldItem::KingsRock: return "DAMAGING MOVES MAY MAKE THE TARGET FLINCH.";
    case HeldItem::AmuletCoin: return "DOUBLES PRIZE MONEY WHILE ITS HOLDER IS IN THE PARTY.";
    case HeldItem::SmokeBall: return "GUARANTEES ESCAPE FROM A WILD POKEMON.";
    case HeldItem::SilkScarf: return "BOOSTS NORMAL-TYPE MOVES BY 10%.";
    case HeldItem::BlackBelt: return "BOOSTS FIGHTING-TYPE MOVES BY 10%.";
    case HeldItem::SharpBeak: return "BOOSTS FLYING-TYPE MOVES BY 10%.";
    case HeldItem::PoisonBarb: return "BOOSTS POISON-TYPE MOVES BY 10%.";
    case HeldItem::SoftSand: return "BOOSTS GROUND-TYPE MOVES BY 10%.";
    case HeldItem::HardStone: return "BOOSTS ROCK-TYPE MOVES BY 10%.";
    case HeldItem::SilverPowder: return "BOOSTS BUG-TYPE MOVES BY 10%.";
    case HeldItem::SpellTag: return "BOOSTS GHOST-TYPE MOVES BY 10%.";
    case HeldItem::MetalCoat: return "BOOSTS STEEL-TYPE MOVES BY 10%.";
    case HeldItem::Charcoal: return "BOOSTS FIRE-TYPE MOVES BY 10%.";
    case HeldItem::MysticWater: return "BOOSTS WATER-TYPE MOVES BY 10%.";
    case HeldItem::MiracleSeed: return "BOOSTS GRASS-TYPE MOVES BY 10%.";
    case HeldItem::Magnet: return "BOOSTS ELECTRIC-TYPE MOVES BY 10%.";
    case HeldItem::TwistedSpoon: return "BOOSTS PSYCHIC-TYPE MOVES BY 10%.";
    case HeldItem::NeverMeltIce: return "BOOSTS ICE-TYPE MOVES BY 10%.";
    case HeldItem::DragonFang: return "BOOSTS DRAGON-TYPE MOVES BY 10%.";
    case HeldItem::BlackGlasses: return "BOOSTS DARK-TYPE MOVES BY 10%.";
    case HeldItem::MegaStone: return "AWAKENS AN OFFICIAL MEGA OR PRIMAL FORM WHILE HELD.";
    case HeldItem::BerryJuice: return "RESTORES 20 HP WHEN THE HOLDER FALLS BELOW HALF HP.";
    case HeldItem::MachoBrace: return "HALVES SPEED. RETAINED FOR ORIGINAL BATTLE TOWER SETS.";
    case HeldItem::SoulDew: return "BOOSTS LATIAS OR LATIOS SP. ATK AND SP. DEF BY 50%.";
    case HeldItem::DeepSeaScale: return "DOUBLES CLAMPERL'S SP. DEF.";
    case HeldItem::DeepSeaTooth: return "DOUBLES CLAMPERL'S SP. ATK.";
    case HeldItem::LightBall: return "DOUBLES PIKACHU'S SP. ATK.";
    case HeldItem::LeppaBerry: return "RESTORES 10 PP TO THE FIRST MOVE THAT REACHES ZERO.";
    case HeldItem::MentalHerb: return "CURES INFATUATION ONCE.";
    case HeldItem::DragonScale: return "A RARE SCALE WITH NO DIRECT BATTLE EFFECT.";
    case HeldItem::IapapaBerry: case HeldItem::WikiBerry: case HeldItem::AguavBerry:
    case HeldItem::FigyBerry: case HeldItem::MagoBerry:
      return "RESTORES ONE EIGHTH OF MAX HP IN A PINCH; MAY CAUSE CONFUSION.";
    case HeldItem::SeaIncense: return "BOOSTS WATER-TYPE MOVES BY 5%.";
    case HeldItem::SalacBerry: return "RAISES SPEED IN A PINCH.";
    case HeldItem::LansatBerry: return "RAISES THE CRITICAL-HIT RATIO IN A PINCH.";
    case HeldItem::ApicotBerry: return "RAISES SP. DEF IN A PINCH.";
    case HeldItem::StarfBerry: return "SHARPLY RAISES A RANDOM STAT IN A PINCH.";
    case HeldItem::LiechiBerry: return "RAISES ATTACK IN A PINCH.";
    case HeldItem::Stick: return "RAISES FARFETCH'D'S CRITICAL-HIT RATIO.";
    case HeldItem::LaxIncense: return "LOWERS THE ACCURACY OF MOVES USED AGAINST THE HOLDER.";
    case HeldItem::ThickClub: return "DOUBLES CUBONE OR MAROWAK'S ATTACK.";
    case HeldItem::MetalPowder: return "BOOSTS DITTO'S DEFENSE AND SP. DEF BY 50%.";
    case HeldItem::PetayaBerry: return "RAISES SP. ATK IN A PINCH.";
    case HeldItem::LuckyPunch: return "RAISES CHANSEY'S CRITICAL-HIT RATIO.";
    case HeldItem::GanlonBerry: return "RAISES DEFENSE IN A PINCH.";
    case HeldItem::LuckyEgg: return "A PERMANENT ACCOUNT REWARD THAT DOUBLES PARTY EXP.";
    case HeldItem::QuickPowder: return "DOUBLES DITTO'S SPEED BEFORE IT TRANSFORMS.";
    default: return "NO BATTLE EFFECT.";
  }
}
bool heldItemIsConsumable(HeldItem item) { const HeldItemData* data=heldItemData(item);return data&&data->consumable; }
bool heldItemIsBerry(HeldItem item) {
  switch(item){
    case HeldItem::OranBerry:case HeldItem::SitrusBerry:case HeldItem::LumBerry:
    case HeldItem::PersimBerry:case HeldItem::CheriBerry:case HeldItem::ChestoBerry:
    case HeldItem::PechaBerry:case HeldItem::RawstBerry:case HeldItem::AspearBerry:
    case HeldItem::LeppaBerry:case HeldItem::IapapaBerry:case HeldItem::WikiBerry:
    case HeldItem::SalacBerry:case HeldItem::LansatBerry:case HeldItem::ApicotBerry:
    case HeldItem::StarfBerry:case HeldItem::LiechiBerry:case HeldItem::AguavBerry:
    case HeldItem::FigyBerry:case HeldItem::MagoBerry:case HeldItem::PetayaBerry:
    case HeldItem::GanlonBerry:return true;
    default:return false;
  }
}
bool heldItemBoostsType(HeldItem item,PokemonType type) { const HeldItemData* data=heldItemData(item);return data&&data->boostsType&&data->boostedType==type; }
bool heldItemIsTransferLocked(HeldItem item) { return uniqueHeldItemOwnershipBit(item)!=0U; }
uint8_t heldItemCount(){return kHeldItemInventorySlots-1U;}

namespace {
constexpr HeldItem kSpecialItems[kSpecialHeldItemCount] = {
  HeldItem::LightBall, HeldItem::LuckyPunch, HeldItem::ThickClub,
  HeldItem::Stick, HeldItem::MetalPowder, HeldItem::QuickPowder,
  HeldItem::DeepSeaTooth, HeldItem::DeepSeaScale, HeldItem::SoulDew,
};
uint16_t* countedQuantity(HeldItem item,uint16_t inventory[],
                          SpecialHeldItemInventory& special){
  const uint8_t raw=static_cast<uint8_t>(item);
  if(raw<kHeldItemInventorySlots)return inventory?&inventory[raw]:nullptr;
  const int8_t specialIndex=specialHeldItemIndex(item);
  return specialIndex>=0?&special.quantities[static_cast<uint8_t>(specialIndex)]:nullptr;
}
}

int8_t specialHeldItemIndex(HeldItem item){
  for(uint8_t index=0;index<kSpecialHeldItemCount;++index)
    if(kSpecialItems[index]==item)return static_cast<int8_t>(index);
  return -1;
}
uint16_t specialHeldItemQuantity(const SpecialHeldItemInventory& inventory,HeldItem item){
  const int8_t index=specialHeldItemIndex(item);
  return index>=0?inventory.quantities[static_cast<uint8_t>(index)]:0U;
}

bool equipHeldItem(OwnedPokemon& pokemon,HeldItem selected,uint16_t inventory[],
                   SpecialHeldItemInventory& special,uint64_t& ownedMachines){
  if(!inventory||selected>=HeldItem::Count||selected==HeldItem::LuckyEgg)return false;
  const HeldItem equipped=pokemon.heldItem;
  const uint8_t equippedQuantity=CollectionLogic::heldItemQuantity(pokemon);
  if(selected==equipped){
    uint16_t* selectedQuantity=countedQuantity(selected,inventory,special);
    if(!heldItemIsBerry(selected)||!selectedQuantity||!*selectedQuantity||
       equippedQuantity>=CollectionLogic::kMaximumHeldBerryQuantity)return false;
    --*selectedQuantity;
    return CollectionLogic::setHeldItemQuantity(
        pokemon,selected,static_cast<uint8_t>(equippedQuantity+1U));
  }
  const uint64_t selectedBit=uniqueHeldItemOwnershipBit(selected);
  const uint64_t equippedBit=uniqueHeldItemOwnershipBit(equipped);
  uint16_t* selectedQuantity=selected==HeldItem::None?nullptr:
      countedQuantity(selected,inventory,special);
  uint16_t* equippedBagQuantity=equipped==HeldItem::None?nullptr:
      countedQuantity(equipped,inventory,special);
  if(selected!=HeldItem::None){
    if(selectedBit){if(!(ownedMachines&selectedBit))return false;}
    else if(!selectedQuantity||!*selectedQuantity)return false;
  }
  if(equipped!=HeldItem::None&&!equippedBit&&
     (!equippedBagQuantity||UINT16_MAX-*equippedBagQuantity<equippedQuantity))return false;
  if(equippedBit)ownedMachines|=equippedBit;
  else if(equippedBagQuantity)*equippedBagQuantity=static_cast<uint16_t>(
      *equippedBagQuantity+equippedQuantity);
  if(selectedBit)ownedMachines&=~selectedBit;
  else if(selectedQuantity)--*selectedQuantity;
  return CollectionLogic::setHeldItemQuantity(
      pokemon,selected,selected==HeldItem::None?0U:1U);
}

bool unequipHeldItem(OwnedPokemon& pokemon,uint16_t inventory[],
                     SpecialHeldItemInventory& special,uint64_t& ownedMachines){
  if(pokemon.heldItem==HeldItem::None)return false;
  const HeldItem equipped=pokemon.heldItem;
  const uint8_t quantity=CollectionLogic::heldItemQuantity(pokemon);
  const uint64_t ownershipBit=uniqueHeldItemOwnershipBit(equipped);
  if(ownershipBit){ownedMachines|=ownershipBit;return CollectionLogic::setHeldItemQuantity(
      pokemon,HeldItem::None,0U);}
  uint16_t* bagQuantity=countedQuantity(equipped,inventory,special);
  if(!bagQuantity||UINT16_MAX-*bagQuantity<quantity)return false;
  *bagQuantity=static_cast<uint16_t>(*bagQuantity+quantity);
  return CollectionLogic::setHeldItemQuantity(pokemon,HeldItem::None,0U);
}

bool equipHeldItem(HeldItem& equipped,HeldItem selected,uint16_t inventory[],
                   uint64_t& ownedMachines) {
  if(!inventory||selected>=HeldItem::Count||selected==equipped||
     selected==HeldItem::LuckyEgg)return false;
  const uint64_t selectedBit=uniqueHeldItemOwnershipBit(selected);
  const uint64_t equippedBit=uniqueHeldItemOwnershipBit(equipped);
  const uint8_t selectedIndex=static_cast<uint8_t>(selected);
  const uint8_t equippedIndex=static_cast<uint8_t>(equipped);
  if(selected!=HeldItem::None){
    if(selectedBit){if(!(ownedMachines&selectedBit))return false;}
    else if(selectedIndex>=kHeldItemInventorySlots||!inventory[selectedIndex])return false;
  }
  if(equipped!=HeldItem::None&&!equippedBit&&
     (equippedIndex>=kHeldItemInventorySlots||inventory[equippedIndex]==UINT16_MAX))return false;

  if(equippedBit)ownedMachines|=equippedBit;
  else if(equipped!=HeldItem::None)++inventory[equippedIndex];
  if(selectedBit)ownedMachines&=~selectedBit;
  else if(selected!=HeldItem::None)--inventory[selectedIndex];
  equipped=selected;return true;
}

bool unequipHeldItem(HeldItem& equipped,uint16_t inventory[],uint64_t& ownedMachines) {
  if(equipped==HeldItem::None)return false;
  const uint64_t ownershipBit=uniqueHeldItemOwnershipBit(equipped);
  if(ownershipBit){
    ownedMachines|=ownershipBit;
    equipped=HeldItem::None;
    return true;
  }
  const uint8_t index=static_cast<uint8_t>(equipped);
  if(!inventory||index>=kHeldItemInventorySlots||inventory[index]==UINT16_MAX)return false;
  ++inventory[index];
  equipped=HeldItem::None;
  return true;
}
