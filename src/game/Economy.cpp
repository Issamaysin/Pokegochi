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

uint8_t greatBallStock(uint8_t badgeCount){
  if(badgeCount<4U)return 0U;
  if(badgeCount<8U)return 8U;
  if(badgeCount<12U)return 12U;
  return 16U;
}

uint8_t ultraBallStock(uint8_t badgeCount,bool sinnohUnlocked){
  if(sinnohUnlocked)return 32U;
  if(badgeCount<12U)return 0U;
  if(badgeCount<16U)return 8U;
  if(badgeCount<20U)return 12U;
  if(badgeCount<24U)return 18U;
  return 24U;
}

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

bool machineIsAvailable(uint16_t id, bool sinnohUnlocked, bool unovaUnlocked,
                        bool kalosUnlocked) {
  if(id<kLegacyMachineCount)return true;
  if(id<kLegacyMachineCount+kSinnohMachineCount)return sinnohUnlocked;
  if(id<kLegacyMachineCount+kSinnohMachineCount+kUnovaMachineCount)
    return unovaUnlocked;
  return kalosUnlocked&&id<kMachineCount;
}

bool martHasLockedRegionalOffer(const MartState& mart,bool sinnohUnlocked,
                                bool unovaUnlocked,bool kalosUnlocked) {
  for (const MartOffer& offer : mart.offers) {
    if(!offer.isMachine())continue;
    if(!machineIsAvailable(offer.machineId(),sinnohUnlocked,unovaUnlocked,
                           kalosUnlocked))return true;
  }
  return false;
}

uint16_t machinePrice(uint16_t id){return id<kMachineCount?machinePrices[id]:0U;}

bool chooseMachineOffer(MartState& mart,uint64_t ownedMachines,uint64_t ownedSinnohMachines,
                        uint32_t ownedUnovaMachines,uint8_t offeredCount,
                        bool sinnohUnlocked,bool unovaUnlocked,bool kalosUnlocked,
                        MartOffer& offer){
  uint8_t availableCount=0;
  for(uint16_t id=0;id<kMachineCount;++id){
    if(!machineIsAvailable(id,sinnohUnlocked,unovaUnlocked,kalosUnlocked))continue;
    if(Economy::ownsMachine(ownedMachines,ownedSinnohMachines,ownedUnovaMachines,id))continue;
    const MartOffer candidate=MartOffer::machine(id,machinePrice(id));
    if(!alreadyOffered(mart,offeredCount,candidate))++availableCount;
  }
  if(!availableCount)return false;
  uint8_t selected=static_cast<uint8_t>(next(mart.rng)%availableCount);
  for(uint16_t id=0;id<kMachineCount;++id){
    if(!machineIsAvailable(id,sinnohUnlocked,unovaUnlocked,kalosUnlocked))continue;
    if(Economy::ownsMachine(ownedMachines,ownedSinnohMachines,ownedUnovaMachines,id))continue;
    const MartOffer candidate=MartOffer::machine(id,machinePrice(id));
    if(alreadyOffered(mart,offeredCount,candidate))continue;
    if(!selected){offer=candidate;return true;}
    --selected;
  }
  return false;
}

MartOffer randomOffer(MartState& mart,uint8_t badgeCount,uint64_t ownedMachines,
                      uint64_t ownedSinnohMachines,uint32_t ownedUnovaMachines,
                      uint8_t offeredCount,bool sinnohUnlocked,bool unovaUnlocked,
                      bool kalosUnlocked){
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
    if(chooseMachineOffer(mart,ownedMachines,ownedSinnohMachines,ownedUnovaMachines,
                          offeredCount,sinnohUnlocked,unovaUnlocked,kalosUnlocked,
                          machine))return machine;
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
// The stocked offers remain stable for six hours during the campaign and four
// hours after the final endgame expedition is unlocked, including while the
// screen is asleep; the next elapsed-time update performs one fresh rotation.
bool Economy::refreshIfDue(MartState& mart,uint8_t badgeCount,uint64_t ownedMachines,
                           uint64_t ownedSinnohMachines,bool sinnohUnlocked,
                           uint32_t ownedUnovaMachines,bool unovaUnlocked,
                           bool kalosUnlocked){
  // A save produced by an older build may already contain a Sinnoh machine
  // while the region is still locked. Replace that incompatible rotation
  // before it can be displayed or bought.
  const uint32_t rotationSeconds = sinnohUnlocked ? kEndgameRotationSeconds :
                                                     kRotationSeconds;
  if(martHasLockedRegionalOffer(mart,sinnohUnlocked,unovaUnlocked,kalosUnlocked) ||
     mart.elapsedSeconds>=rotationSeconds){
    rotate(mart,badgeCount,ownedMachines,ownedSinnohMachines,sinnohUnlocked,
           ownedUnovaMachines,unovaUnlocked,kalosUnlocked);return true;
  }
  return false;
}
void Economy::advance(MartState& mart,uint32_t seconds,uint8_t badgeCount,uint64_t ownedMachines,
                      uint64_t ownedSinnohMachines,bool sinnohUnlocked,
                      uint32_t ownedUnovaMachines,bool unovaUnlocked,
                      bool kalosUnlocked){
  mart.elapsedSeconds+=seconds;
  refreshIfDue(mart,badgeCount,ownedMachines,ownedSinnohMachines,sinnohUnlocked,
               ownedUnovaMachines,unovaUnlocked,kalosUnlocked);
}
void Economy::rotate(MartState& mart,uint8_t badgeCount,uint64_t ownedMachines,
                     uint64_t ownedSinnohMachines,bool sinnohUnlocked,
                     uint32_t ownedUnovaMachines,bool unovaUnlocked,
                     bool kalosUnlocked){
  const uint32_t rotationSeconds = sinnohUnlocked ? kEndgameRotationSeconds :
                                                     kRotationSeconds;
  mart.elapsedSeconds%=rotationSeconds;++mart.day;
  uint8_t guaranteedCount=0;
  mart.offers[guaranteedCount++]={MartItem::PokeBall,HeldItem::None,200,12};
  // Better Balls become dependable supplies as capture difficulty rises.
  // They remain one unique product each, so progression raises stock without
  // consuming several rows or changing the six-hour rotation cadence.
  const uint8_t greatStock=greatBallStock(badgeCount);
  if(greatStock)
    mart.offers[guaranteedCount++]={MartItem::GreatBall,HeldItem::None,600,greatStock};
  const uint8_t ultraStock=ultraBallStock(badgeCount,sinnohUnlocked);
  if(ultraStock)
    mart.offers[guaranteedCount++]={MartItem::UltraBall,HeldItem::None,1200,ultraStock};
  mart.offers[guaranteedCount++]={MartItem::Potion,HeldItem::None,300,8};
  const MartItem status=statusMedicines[next(mart.rng)%(sizeof(statusMedicines)/sizeof(statusMedicines[0]))];
  uint16_t statusPrice=100;
  switch(status){case MartItem::FullHeal:statusPrice=600;break;case MartItem::ParalyzeHeal:statusPrice=200;break;case MartItem::Awakening:case MartItem::BurnHeal:case MartItem::IceHeal:statusPrice=250;break;default:break;}
  mart.offers[guaranteedCount++]={status,HeldItem::None,statusPrice,
                                 static_cast<uint8_t>(3U+next(mart.rng)%6U)};

  uint8_t randomStart=guaranteedCount;
  MartOffer guaranteedMachine{};
  if(chooseMachineOffer(mart,ownedMachines,ownedSinnohMachines,ownedUnovaMachines,
                        randomStart,sinnohUnlocked,unovaUnlocked,kalosUnlocked,
                        guaranteedMachine)){
    mart.offers[randomStart++]=guaranteedMachine;
  }
  for(uint8_t i=randomStart;i<kMartOfferCount;++i){
    // Twenty slots are useful only when they represent twenty distinct
    // products. Re-roll exact duplicates while retaining the original family
    // probabilities; the bounded fallback still guarantees completion.
    MartOffer candidate{};bool unique=false;
    for(uint8_t attempt=0;attempt<32U;++attempt){
      candidate=randomOffer(mart,badgeCount,ownedMachines,ownedSinnohMachines,
                            ownedUnovaMachines,i,sinnohUnlocked,unovaUnlocked,
                            kalosUnlocked);
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
  return maximumPurchasable(mart,offer,money,inventory,ownedMachines,0U,ppItems);
}
uint8_t Economy::maximumPurchasable(const MartState& mart,uint8_t offer,uint32_t money,
                                    const Inventory& inventory,uint64_t ownedMachines,
                                    uint64_t ownedSinnohMachines,
                                    const PpItemInventory* ppItems){
  return maximumPurchasable(mart,offer,money,inventory,ownedMachines,
                            ownedSinnohMachines,0U,ppItems);
}
uint8_t Economy::maximumPurchasable(const MartState& mart,uint8_t offer,uint32_t money,
                                    const Inventory& inventory,uint64_t ownedMachines,
                                    uint64_t ownedSinnohMachines,
                                    uint32_t ownedUnovaMachines,
                                    const PpItemInventory* ppItems){
  if(offer>=kMartOfferCount)return 0;
  const MartOffer& selected=mart.offers[offer];
  const uint32_t unitPrice=price(selected);
  if(!selected.remaining||!unitPrice)return 0;
  if(selected.isMachine())return ownsMachine(ownedMachines,ownedSinnohMachines,
      ownedUnovaMachines,selected.machineId())||money<unitPrice?0:1;
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
  uint64_t ignoredSinnohMachines=0;
  return buyQuantity(mart,offer,quantity,money,inventory,ownedMachines,
                     ignoredSinnohMachines,ppItems);
}
bool Economy::buyQuantity(MartState& mart,uint8_t offer,uint8_t quantity,uint32_t& money,
                          Inventory& inventory,uint64_t& ownedMachines,
                          uint64_t& ownedSinnohMachines,PpItemInventory* ppItems){
  uint32_t ignoredUnovaMachines=0U;
  return buyQuantity(mart,offer,quantity,money,inventory,ownedMachines,
                     ownedSinnohMachines,ignoredUnovaMachines,ppItems);
}
bool Economy::buyQuantity(MartState& mart,uint8_t offer,uint8_t quantity,uint32_t& money,
                          Inventory& inventory,uint64_t& ownedMachines,
                          uint64_t& ownedSinnohMachines,uint32_t& ownedUnovaMachines,
                          PpItemInventory* ppItems){
  if(!quantity||quantity>maximumPurchasable(mart,offer,money,inventory,ownedMachines,
                                            ownedSinnohMachines,ownedUnovaMachines,
                                            ppItems))return false;
  MartOffer& selected=mart.offers[offer];
  const uint32_t total=price(selected)*quantity;
  if(selected.isMachine()){
    const uint16_t id=selected.machineId();
    if(id<kLegacyMachineCount)ownedMachines|=uint64_t{1}<<id;
    else if(id<kLegacyMachineCount+kSinnohMachineCount)
      ownedSinnohMachines|=uint64_t{1}<<(id-kLegacyMachineCount);
    else if(id<kMachineCount)
      ownedUnovaMachines|=uint32_t{1}<<(id-kLegacyMachineCount-kSinnohMachineCount);
  }
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
  uint64_t ignoredSinnohMachines=0;
  return buy(mart,offer,money,inventory,ownedMachines,ignoredSinnohMachines,ppItems);
}
bool Economy::buy(MartState& mart,uint8_t offer,uint32_t& money,Inventory& inventory,uint64_t& ownedMachines,
                  uint64_t& ownedSinnohMachines,PpItemInventory* ppItems){
  return buyQuantity(mart,offer,1,money,inventory,ownedMachines,ownedSinnohMachines,ppItems);
}
bool Economy::buy(MartState& mart,uint8_t offer,uint32_t& money,Inventory& inventory,uint64_t& ownedMachines,
                  uint64_t& ownedSinnohMachines,uint32_t& ownedUnovaMachines,
                  PpItemInventory* ppItems){
  return buyQuantity(mart,offer,1,money,inventory,ownedMachines,
                     ownedSinnohMachines,ownedUnovaMachines,ppItems);
}
const char* Economy::name(MartItem item){static const char* names[]={"POKE BALL","GREAT BALL","ULTRA BALL","POTION","SUPER POTION","HYPER POTION","FULL HEAL","ANTIDOTE","PARALYZE HEAL","AWAKENING","BURN HEAL","ICE HEAL","X ATTACK","X DEFENSE","X SPEED","X ACCURACY","DIRE HIT","REVIVE","MAX REVIVE","ETHER","MAX ETHER","ELIXIR","MAX ELIXIR","PP UP","MASTER BALL"};const uint8_t index=static_cast<uint8_t>(item);return index<static_cast<uint8_t>(MartItem::Count)?names[index]:"UNKNOWN";}
const char* Economy::name(const MartOffer& offer){
  if(offer.isMachine()){
    static char label[8];
    const uint16_t id=offer.machineId();
    std::snprintf(label,sizeof(label),isHiddenMachine(id)?"HM%02u":"TM%02u",
                  machineNumber(id));
    return label;
  }
  return offer.isHeldItem()?heldItemName(offer.heldItem):name(offer.item);
}
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
MoveId Economy::machineMove(uint16_t machineId){return machineId<kMachineCount?static_cast<MoveId>(machineMoves[machineId]):MoveId::None;}
bool Economy::canLearnMachine(uint16_t speciesId,uint16_t machineId){
  if(speciesId==0U||speciesId>=722U||machineId>=kMachineCount)return false;
  if(machineId<kLegacyMachineCount)
    return (machineLearnsets[speciesId]&(uint64_t{1}<<machineId))!=0U;
  if(machineId<kLegacyMachineCount+kSinnohMachineCount){
    const uint8_t sinnohBit = static_cast<uint8_t>(machineId-kLegacyMachineCount);
    return (sinnohMachineLearnsets[speciesId][sinnohBit>>3U] &
            static_cast<uint8_t>(1U<<(sinnohBit&7U))) != 0U;
  }
  const uint8_t unovaBit=static_cast<uint8_t>(
      machineId-kLegacyMachineCount-kSinnohMachineCount);
  return (unovaMachineLearnsets[speciesId][unovaBit>>3U]&
          static_cast<uint8_t>(1U<<(unovaBit&7U)))!=0U;
}
bool Economy::canLearnMachine(const OwnedPokemon& pokemon,uint16_t machineId){
  // Platinum's form JSON deliberately repeats the base species TM/HM list
  // for Rotom, Giratina and Shaymin. Their form-exclusive moves are learned
  // by form change/relearn, not by making a TM compatible with one appliance.
  return canLearnMachine(pokemon.speciesId,machineId);
}
bool Economy::ownsMachine(uint64_t ownedMachines,uint16_t machineId){
  return machineId<kLegacyMachineCount&&(ownedMachines&(uint64_t{1}<<machineId));
}
bool Economy::ownsMachine(uint64_t ownedMachines,uint64_t ownedSinnohMachines,uint16_t machineId){
  return ownsMachine(ownedMachines,ownedSinnohMachines,0U,machineId);
}
bool Economy::ownsMachine(uint64_t ownedMachines,uint64_t ownedSinnohMachines,
                          uint32_t ownedUnovaMachines,uint16_t machineId){
  if(machineId<kLegacyMachineCount)return (ownedMachines&(uint64_t{1}<<machineId))!=0U;
  if(machineId<kLegacyMachineCount+kSinnohMachineCount)return
      (ownedSinnohMachines&(uint64_t{1}<<(machineId-kLegacyMachineCount)))!=0U;
  return machineId<kMachineCount&&
      (ownedUnovaMachines&(uint32_t{1}<<(
          machineId-kLegacyMachineCount-kSinnohMachineCount)))!=0U;
}
bool Economy::isHiddenMachine(uint16_t machineId){
  return machineId>=50U&&machineId<kLegacyMachineCount;
}
uint8_t Economy::machineNumber(uint16_t machineId){
  if(machineId<50U)return static_cast<uint8_t>(machineId+1U);
  if(machineId<kLegacyMachineCount)return static_cast<uint8_t>(machineId-49U);
  if(machineId<kLegacyMachineCount+kSinnohMachineCount)
    return static_cast<uint8_t>(machineId-7U);
  const uint16_t offset=static_cast<uint16_t>(
      machineId-kLegacyMachineCount-kSinnohMachineCount);
  if(offset<kUnovaMachineCount)return unovaMachineNumbers[offset];
  const uint16_t kalosOffset=offset-kUnovaMachineCount;
  return kalosOffset<kKalosMachineCount?kalosMachineNumbers[kalosOffset]:0U;
}

// Emerald/Platinum TM/HM catalog: b7958c9613d74c34
