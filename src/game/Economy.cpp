#include "game/Economy.h"
#include <algorithm>
#include <cstdio>

namespace {
constexpr MartOffer catalog[]={
  {MartItem::GreatBall,HeldItem::None,600,0},{MartItem::UltraBall,HeldItem::None,1200,0},
  {MartItem::SuperPotion,HeldItem::None,700,0},{MartItem::HyperPotion,HeldItem::None,1200,0},
  {MartItem::FullHeal,HeldItem::None,600,0},{MartItem::Antidote,HeldItem::None,100,0},
  {MartItem::ParalyzeHeal,HeldItem::None,200,0},{MartItem::Awakening,HeldItem::None,250,0},
  {MartItem::BurnHeal,HeldItem::None,250,0},{MartItem::IceHeal,HeldItem::None,250,0},
  {MartItem::XAttack,HeldItem::None,500,0},{MartItem::XDefense,HeldItem::None,550,0},
  {MartItem::XSpeed,HeldItem::None,350,0},{MartItem::XAccuracy,HeldItem::None,950,0},
  {MartItem::DireHit,HeldItem::None,650,0},
  {MartItem::Revive,HeldItem::None,1500,0},{MartItem::MaxRevive,HeldItem::None,3000,0}
};
// MartOffer keeps its compact, already-persisted 16-bit price field. This
// reserved token represents the one legitimate price above that range; every
// money calculation goes through Economy::price().
constexpr uint16_t kMasterBallPriceToken=0xFFFFU;
constexpr MartOffer ppRecoveryCatalog[]={
  {MartItem::Ether,HeldItem::None,1200,0},
  {MartItem::Ether,HeldItem::None,1200,0},
  {MartItem::Ether,HeldItem::None,1200,0},
  {MartItem::Ether,HeldItem::None,1200,0},
  {MartItem::MaxEther,HeldItem::None,2500,0},
  {MartItem::Elixir,HeldItem::None,3000,0},
  {MartItem::Elixir,HeldItem::None,3000,0},
  {MartItem::MaxElixir,HeldItem::None,5000,0},
};
uint32_t next(uint32_t& x){x^=x<<13U;x^=x>>17U;x^=x<<5U;return x;}

#include "MachineDataGenerated.inc"
constexpr MartItem statusMedicines[] = {
  MartItem::FullHeal, MartItem::Antidote, MartItem::ParalyzeHeal,
  MartItem::Awakening, MartItem::BurnHeal, MartItem::IceHeal
};

HeldItem chooseHeldItem(MartState& mart,uint8_t badgeCount){
  uint16_t total=0;
  for(uint8_t raw=1;raw<kHeldItemInventorySlots;++raw){const HeldItemData* data=heldItemData(static_cast<HeldItem>(raw));if(data&&data->unlockBadges<=badgeCount)total+=data->martWeight;}
  if(!total)return HeldItem::None;
  uint16_t roll=static_cast<uint16_t>(next(mart.rng)%total);
  for(uint8_t raw=1;raw<kHeldItemInventorySlots;++raw){const HeldItemData* data=heldItemData(static_cast<HeldItem>(raw));if(!data||data->unlockBadges>badgeCount)continue;if(roll<data->martWeight)return data->id;roll-=data->martWeight;}
  return HeldItem::OranBerry;
}

bool sameOffer(const MartOffer& left,const MartOffer& right){
  if(left.isMachine()||right.isMachine())
    return left.isMachine()&&right.isMachine()&&left.machineId()==right.machineId();
  if(left.isHeldItem()||right.isHeldItem())
    return left.isHeldItem()&&right.isHeldItem()&&left.heldItem==right.heldItem;
  return left.item==right.item;
}

bool alreadyOffered(const MartState& mart,uint8_t count,const MartOffer& candidate){
  for(uint8_t i=0;i<count;++i)if(sameOffer(mart.offers[i],candidate))return true;
  return false;
}

bool chooseMachineOffer(MartState& mart,uint64_t ownedMachines,uint8_t offeredCount,
                        MartOffer& offer){
  uint8_t availableCount=0;
  for(uint8_t id=0;id<kMachineCount;++id){
    if(Economy::ownsMachine(ownedMachines,id))continue;
    const MartOffer candidate=MartOffer::machine(id,id<50?3000:5000);
    if(!alreadyOffered(mart,offeredCount,candidate))++availableCount;
  }
  if(!availableCount)return false;
  uint8_t selected=static_cast<uint8_t>(next(mart.rng)%availableCount);
  for(uint8_t id=0;id<kMachineCount;++id){
    if(Economy::ownsMachine(ownedMachines,id))continue;
    const MartOffer candidate=MartOffer::machine(id,id<50?3000:5000);
    if(alreadyOffered(mart,offeredCount,candidate))continue;
    if(!selected){offer=candidate;return true;}
    --selected;
  }
  return false;
}

MartOffer randomOffer(MartState& mart,uint8_t badgeCount,uint64_t ownedMachines,
                      uint8_t offeredCount){
  const uint8_t roll=static_cast<uint8_t>(next(mart.rng)%100U);
  if(roll<26U){
    MartOffer offer=ppRecoveryCatalog[next(mart.rng)%(sizeof(ppRecoveryCatalog)/sizeof(ppRecoveryCatalog[0]))];
    offer.remaining=static_cast<uint8_t>(2U+next(mart.rng)%5U);return offer;
  }
  if(roll<31U)return {MartItem::PpUp,HeldItem::None,9800,1};
  // The expanded fourth page leans a little more toward equipment than the
  // old rotation (27% instead of 22%), while still respecting Badge unlocks.
  if(roll<58U){
    const HeldItem item=chooseHeldItem(mart,badgeCount);const HeldItemData* data=heldItemData(item);
    return {MartItem::PokeBall,item,static_cast<uint16_t>(data?data->price:20U),
            static_cast<uint8_t>(1U+next(mart.rng)%3U)};
  }
  // Eight percent of random slots may add a second (or later) unique TM/HM.
  // This is separate from the guaranteed machine below and disappears
  // naturally once the player owns every machine.
  if(roll<66U){
    MartOffer machine{};
    if(chooseMachineOffer(mart,ownedMachines,offeredCount,machine))return machine;
  }
  MartOffer offer=catalog[next(mart.rng)%(sizeof(catalog)/sizeof(catalog[0]))];
  if(offer.item==MartItem::UltraBall&&
     next(mart.rng)%Economy::kMasterBallReplacementDenominator==0U)
    return {MartItem::MasterBall,HeldItem::None,kMasterBallPriceToken,1};
  offer.remaining=static_cast<uint8_t>(2U+next(mart.rng)%7U);return offer;
}

void sortOffersByCategory(MartState& mart){
  // Stable insertion sort needs no heap and preserves the random order inside
  // each family. Selection is complete before this presentation ordering.
  for(uint8_t i=1;i<kMartOfferCount;++i){
    const MartOffer selected=mart.offers[i];uint8_t destination=i;
    while(destination&&static_cast<uint8_t>(Economy::category(mart.offers[destination-1U]))>
        static_cast<uint8_t>(Economy::category(selected))){
      mart.offers[destination]=mart.offers[destination-1U];--destination;
    }
    mart.offers[destination]=selected;
  }
}
}
// The stocked offers remain stable for six hours, including while the screen
// is asleep; the next elapsed-time update performs one fresh rotation.
bool Economy::refreshIfDue(MartState& mart,uint8_t badgeCount,uint64_t ownedMachines){
  if(mart.elapsedSeconds<kRotationSeconds)return false;
  rotate(mart,badgeCount,ownedMachines);return true;
}
void Economy::advance(MartState& mart,uint32_t seconds,uint8_t badgeCount,uint64_t ownedMachines){mart.elapsedSeconds+=seconds;refreshIfDue(mart,badgeCount,ownedMachines);}
void Economy::rotate(MartState& mart,uint8_t badgeCount,uint64_t ownedMachines){
  mart.elapsedSeconds%=kRotationSeconds;++mart.day;
  mart.offers[0]={MartItem::PokeBall,HeldItem::None,200,12};
  mart.offers[1]={MartItem::Potion,HeldItem::None,300,8};
  const MartItem status=statusMedicines[next(mart.rng)%(sizeof(statusMedicines)/sizeof(statusMedicines[0]))];
  uint16_t statusPrice=100;
  switch(status){case MartItem::FullHeal:statusPrice=600;break;case MartItem::ParalyzeHeal:statusPrice=200;break;case MartItem::Awakening:case MartItem::BurnHeal:case MartItem::IceHeal:statusPrice=250;break;default:break;}
  mart.offers[2]={status,HeldItem::None,statusPrice,static_cast<uint8_t>(3U+next(mart.rng)%6U)};

  uint8_t randomStart=3;
  MartOffer guaranteedMachine{};
  if(chooseMachineOffer(mart,ownedMachines,3U,guaranteedMachine)){
    mart.offers[3]=guaranteedMachine;randomStart=4;
  }
  for(uint8_t i=randomStart;i<kMartOfferCount;++i){
    // Twenty slots are useful only when they represent twenty distinct
    // products. Re-roll exact duplicates while retaining the original family
    // probabilities; the bounded fallback still guarantees completion.
    MartOffer candidate{};bool unique=false;
    for(uint8_t attempt=0;attempt<32U;++attempt){
      candidate=randomOffer(mart,badgeCount,ownedMachines,i);
      if(!alreadyOffered(mart,i,candidate)){unique=true;break;}
    }
    // A deterministic fallback makes uniqueness a contract rather than a
    // probability, even for an adversarial RNG state.
    if(!unique){
      for(const MartOffer& catalogOffer:catalog){
        if(alreadyOffered(mart,i,catalogOffer))continue;
        candidate=catalogOffer;candidate.remaining=static_cast<uint8_t>(2U+next(mart.rng)%7U);
        unique=true;break;
      }
    }
    mart.offers[i]=candidate;
  }
  sortOffersByCategory(mart);
}
uint8_t Economy::maximumPurchasable(const MartState& mart,uint8_t offer,uint32_t money,
                                    const Inventory& inventory,uint64_t ownedMachines,
                                    const PpItemInventory* ppItems){
  if(offer>=kMartOfferCount)return 0;
  const MartOffer& selected=mart.offers[offer];
  const uint32_t unitPrice=price(selected);
  if(!selected.remaining||!unitPrice)return 0;
  if(selected.isMachine())return ownsMachine(ownedMachines,selected.machineId())||money<unitPrice?0:1;
  uint16_t owned=0;
  if(selected.isHeldItem()){
    const uint8_t index=static_cast<uint8_t>(selected.heldItem);
    if(index>=kHeldItemInventorySlots)return 0;
    owned=inventory.heldItems[index];
  }else if(selected.item==MartItem::MasterBall){
    owned=inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)];
  }else if(selected.item<=MartItem::UltraBall){
    owned=inventory.balls[static_cast<uint8_t>(selected.item)];
  }else if(selected.item<MartItem::Ether){
    const uint8_t index=static_cast<uint8_t>(selected.item)-3U;
    if(index>=kStandardMedicineCount)return 0;
    owned=inventory.medicine[index];
  }else{
    const uint8_t index=static_cast<uint8_t>(selected.item)-static_cast<uint8_t>(MartItem::Ether);
    if(!ppItems||index>=kPpItemCount)return 0;
    owned=ppItems->quantities[index];
  }
  if(owned>=kInventoryStackLimit)return 0;
  const uint32_t affordable=money/unitPrice;
  const uint16_t room=static_cast<uint16_t>(kInventoryStackLimit-owned);
  return static_cast<uint8_t>(std::min<uint32_t>(selected.remaining,
      std::min<uint32_t>(affordable,room)));
}

bool Economy::buyQuantity(MartState& mart,uint8_t offer,uint8_t quantity,uint32_t& money,
                          Inventory& inventory,uint64_t& ownedMachines,PpItemInventory* ppItems){
  if(!quantity||quantity>maximumPurchasable(mart,offer,money,inventory,ownedMachines,ppItems))return false;
  MartOffer& selected=mart.offers[offer];
  const uint32_t total=price(selected)*quantity;
  if(selected.isMachine())ownedMachines|=uint64_t{1}<<selected.machineId();
  else if(selected.isHeldItem()){
    const uint8_t index=static_cast<uint8_t>(selected.heldItem);
    if(index>=kHeldItemInventorySlots)return false;
    inventory.heldItems[index]+=quantity;
  }
  else if(selected.item==MartItem::MasterBall)
    inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)]+=quantity;
  else if(selected.item<=MartItem::UltraBall)inventory.balls[static_cast<uint8_t>(selected.item)]+=quantity;
  else if(selected.item<MartItem::Ether)inventory.medicine[static_cast<uint8_t>(selected.item)-3U]+=quantity;
  else ppItems->quantities[static_cast<uint8_t>(selected.item)-static_cast<uint8_t>(MartItem::Ether)]+=quantity;
  money-=total;
  selected.remaining=static_cast<uint8_t>(selected.remaining-quantity);
  return true;
}

bool Economy::buy(MartState& mart,uint8_t offer,uint32_t& money,Inventory& inventory,uint64_t& ownedMachines,
                  PpItemInventory* ppItems){
  return buyQuantity(mart,offer,1,money,inventory,ownedMachines,ppItems);
}
const char* Economy::name(MartItem item){static const char* names[]={"POKE BALL","GREAT BALL","ULTRA BALL","POTION","SUPER POTION","HYPER POTION","FULL HEAL","ANTIDOTE","PARALYZE HEAL","AWAKENING","BURN HEAL","ICE HEAL","X ATTACK","X DEFENSE","X SPEED","X ACCURACY","DIRE HIT","REVIVE","MAX REVIVE","ETHER","MAX ETHER","ELIXIR","MAX ELIXIR","PP UP","MASTER BALL"};const uint8_t index=static_cast<uint8_t>(item);return index<static_cast<uint8_t>(MartItem::Count)?names[index]:"UNKNOWN";}
const char* Economy::name(const MartOffer& offer){if(offer.isMachine()){static char label[8];const uint8_t id=offer.machineId();std::snprintf(label,sizeof(label),id<50?"TM%02u":"HM%02u",id<50?id+1U:id-49U);return label;}return offer.isHeldItem()?heldItemName(offer.heldItem):name(offer.item);}
uint32_t Economy::price(const MartOffer& offer){return offer.item==MartItem::MasterBall&&!offer.isHeldItem()&&!offer.isMachine()?kMasterBallPrice:offer.price;}
MartOfferCategory Economy::category(const MartOffer& offer){
  if(offer.isMachine())return MartOfferCategory::Machines;
  if(offer.isHeldItem())return MartOfferCategory::HeldItems;
  if(offer.item<=MartItem::UltraBall||offer.item==MartItem::MasterBall)return MartOfferCategory::PokeBalls;
  if(offer.item==MartItem::Potion||offer.item==MartItem::SuperPotion||
     offer.item==MartItem::HyperPotion||offer.item==MartItem::Revive||
     offer.item==MartItem::MaxRevive)return MartOfferCategory::HpRecovery;
  if(offer.item>=MartItem::FullHeal&&offer.item<=MartItem::IceHeal)
    return MartOfferCategory::StatusRecovery;
  if(offer.item>=MartItem::Ether)return MartOfferCategory::PpRecovery;
  return MartOfferCategory::BattleItems;
}
MoveId Economy::machineMove(uint8_t machineId){return machineId<kMachineCount?static_cast<MoveId>(machineMoves[machineId]):MoveId::None;}
bool Economy::canLearnMachine(uint16_t speciesId,uint8_t machineId){
  return speciesId>0U&&speciesId<387U&&machineId<kMachineCount&&
      (machineLearnsets[speciesId]&(uint64_t{1}<<machineId))!=0U;
}
bool Economy::ownsMachine(uint64_t ownedMachines,uint8_t machineId){return machineId<kMachineCount&&(ownedMachines&(uint64_t{1}<<machineId));}

// Emerald TM/HM catalog: 0d5937e85f4c1bfe
