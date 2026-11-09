#include "game/AlolaForms.h"
#include "game/Collection.h"
#include "game/HeldItems.h"
#include <algorithm>

namespace {
constexpr uint32_t kUltraBurstDawnBit = uint32_t{1} << 15U;
constexpr uint8_t kAshBondTimerShift = 16U;
constexpr uint32_t kAshBondTimerMask = 0xFFFFUL << kAshBondTimerShift;

uint16_t packedAshBondSeconds(const OwnedPokemon& pokemon){
  return static_cast<uint16_t>((pokemon.legacyCareCounterReserved&
      kAshBondTimerMask)>>kAshBondTimerShift);
}

void setPackedAshBondSeconds(OwnedPokemon& pokemon,uint16_t seconds){
  pokemon.legacyCareCounterReserved=
      (pokemon.legacyCareCounterReserved&~kAshBondTimerMask)|
      (static_cast<uint32_t>(seconds)<<kAshBondTimerShift);
}

constexpr bool regionalSpecies(uint16_t id) {
  switch(id){
    case 19:case 20:case 26:case 27:case 28:case 37:case 38:
    case 50:case 51:case 52:case 53:case 74:case 75:case 76:
    case 88:case 89:case 103:case 105:return true;
    default:return false;
  }
}

struct RegionalStats {uint16_t id;uint8_t hp,atk,def,spa,spd,spe;PokemonType t1,t2;};
constexpr RegionalStats kRegional[] = {
 {19,30,56,35,25,35,72,PokemonType::Dark,PokemonType::Normal},
 {20,75,71,70,40,80,77,PokemonType::Dark,PokemonType::Normal},
 {26,60,85,50,95,85,110,PokemonType::Electric,PokemonType::Psychic},
 {27,50,75,90,10,35,40,PokemonType::Ice,PokemonType::Steel},
 {28,75,100,120,25,65,65,PokemonType::Ice,PokemonType::Steel},
 {37,38,41,40,50,65,65,PokemonType::Ice,PokemonType::Ice},
 {38,73,67,75,81,100,109,PokemonType::Ice,PokemonType::Fairy},
 {50,10,55,30,35,45,90,PokemonType::Ground,PokemonType::Steel},
 {51,35,100,60,50,70,110,PokemonType::Ground,PokemonType::Steel},
 {52,40,35,35,50,40,90,PokemonType::Dark,PokemonType::Dark},
 {53,65,60,60,75,65,115,PokemonType::Dark,PokemonType::Dark},
 {74,40,80,100,30,30,20,PokemonType::Rock,PokemonType::Electric},
 {75,55,95,115,45,45,35,PokemonType::Rock,PokemonType::Electric},
 {76,80,120,130,55,65,45,PokemonType::Rock,PokemonType::Electric},
 {88,80,80,50,40,50,25,PokemonType::Poison,PokemonType::Dark},
 {89,105,105,75,65,100,50,PokemonType::Poison,PokemonType::Dark},
 {103,95,105,85,125,75,45,PokemonType::Grass,PokemonType::Dragon},
 {105,60,80,110,50,80,45,PokemonType::Fire,PokemonType::Ghost},
};
const RegionalStats* regional(const OwnedPokemon& p){
  if(p.form!=1U)return nullptr;
  for(const auto& row:kRegional)if(row.id==p.speciesId)return &row;
  return nullptr;
}

PokemonType oricorioType(uint8_t form){
  static constexpr PokemonType kTypes[]={PokemonType::Fire,PokemonType::Electric,
      PokemonType::Psychic,PokemonType::Ghost};
  return kTypes[std::min<uint8_t>(form,3U)];
}

bool containsSpecies(const PokemonCollection& collection,uint16_t species){
  for(const OwnedPokemon& pokemon:collection.box)
    if(pokemon.uid!=kEmptyPokemonUid&&pokemon.speciesId==species)return true;
  return false;
}

bool containsHeldItem(const PokemonCollection& collection,HeldItem item){
  for(const OwnedPokemon& pokemon:collection.box)
    if(pokemon.uid!=kEmptyPokemonUid&&pokemon.heldItem==item)return true;
  return false;
}

void installFusionSignatureMove(OwnedPokemon& pokemon,MoveId wanted,MoveId opposite){
  for(uint8_t slot=0;slot<kMoveSlots;++slot){
    if(pokemon.moves[slot]==wanted)return;
    if(pokemon.moves[slot]!=opposite)continue;
    pokemon.moves[slot]=wanted;
    const FullMoveData* data=findFullMove(wanted);
    pokemon.movePp[slot]=data?data->pp:0U;
    CollectionLogic::clearMovePpUps(pokemon,slot);
    return;
  }
  for(uint8_t slot=0;slot<kMoveSlots;++slot){
    if(pokemon.moves[slot]!=MoveId::None)continue;
    pokemon.moves[slot]=wanted;
    const FullMoveData* data=findFullMove(wanted);
    pokemon.movePp[slot]=data?data->pp:0U;
    CollectionLogic::clearMovePpUps(pokemon,slot);
    return;
  }
}
}

namespace AlolaForms {
bool isRegionalSpecies(uint16_t speciesId){return regionalSpecies(speciesId);}

bool isRegionalVariant(const OwnedPokemon& pokemon){
  return regionalSpecies(pokemon.speciesId)&&resolvedForm(pokemon,0U)==1U;
}

bool supports(uint16_t speciesId){
  return regionalSpecies(speciesId)||speciesId==658U||speciesId==741U||speciesId==745U||
      speciesId==746U||speciesId==773U||speciesId==774U||speciesId==778U||
      speciesId==800U||speciesId==801U;
}

uint8_t initialForm(uint16_t speciesId,uint32_t personality){
  if(regionalSpecies(speciesId))return 0U;
  if(speciesId==741U)return static_cast<uint8_t>(personality%4U);
  if(speciesId==745U)return static_cast<uint8_t>(personality%3U);
  if(speciesId==774U)return static_cast<uint8_t>(personality%7U);
  if(speciesId==801U)return static_cast<uint8_t>((personality&15U)==0U);
  return 0U;
}

uint8_t resolvedForm(const OwnedPokemon& pokemon,uint8_t fallback){
  if(regionalSpecies(pokemon.speciesId))return std::min<uint8_t>(pokemon.form,1U);
  if(pokemon.speciesId==658U)return std::min<uint8_t>(pokemon.form,1U);
  if(pokemon.speciesId==741U)return std::min<uint8_t>(pokemon.form,3U);
  if(pokemon.speciesId==745U)return std::min<uint8_t>(pokemon.form,2U);
  if(pokemon.speciesId==746U)return std::min<uint8_t>(pokemon.form,1U);
  if(pokemon.speciesId==773U){
    if(!heldItemIsMemory(pokemon.heldItem))return 0U;
    return static_cast<uint8_t>(memoryType(pokemon.heldItem));
  }
  if(pokemon.speciesId==774U)return std::min<uint8_t>(pokemon.form,13U);
  if(pokemon.speciesId==778U)return std::min<uint8_t>(pokemon.form,1U);
  if(pokemon.speciesId==800U)return std::min<uint8_t>(pokemon.form,3U);
  if(pokemon.speciesId==801U)return std::min<uint8_t>(pokemon.form,1U);
  return fallback;
}

uint8_t formCount(uint16_t speciesId){
  if(regionalSpecies(speciesId)||speciesId==658U||speciesId==746U||
     speciesId==778U||speciesId==801U)return 2U;
  if(speciesId==741U||speciesId==800U)return 4U;
  if(speciesId==745U)return 3U;
  if(speciesId==773U)return 18U;
  if(speciesId==774U)return 14U;
  return 1U;
}

const char* formName(uint16_t speciesId,uint8_t form){
  if(regionalSpecies(speciesId))return form?"ALOLAN":"KANTO";
  if(speciesId==658U)return form?"ASH":"NORMAL";
  if(speciesId==741U){static constexpr const char* k[]={"BAILE","POM-POM","PA'U","SENSU"};return k[std::min<uint8_t>(form,3U)];}
  if(speciesId==745U){static constexpr const char* k[]={"MIDDAY","MIDNIGHT","DUSK"};return k[std::min<uint8_t>(form,2U)];}
  if(speciesId==746U)return form?"SCHOOL":"SOLO";
  if(speciesId==773U){static constexpr const char* k[]={"NORMAL","FIGHTING","FLYING","POISON","GROUND","ROCK","BUG","GHOST","STEEL","FIRE","WATER","GRASS","ELECTRIC","PSYCHIC","ICE","DRAGON","DARK","FAIRY"};return k[std::min<uint8_t>(form,17U)];}
  if(speciesId==774U){static constexpr const char* k[]={"METEOR RED","METEOR ORANGE","METEOR YELLOW","METEOR GREEN","METEOR BLUE","METEOR INDIGO","METEOR VIOLET","CORE RED","CORE ORANGE","CORE YELLOW","CORE GREEN","CORE BLUE","CORE INDIGO","CORE VIOLET"};return k[std::min<uint8_t>(form,13U)];}
  if(speciesId==778U)return form?"BUSTED":"DISGUISED";
  if(speciesId==800U){static constexpr const char* k[]={"NORMAL","DUSK MANE","DAWN WINGS","ULTRA"};return k[std::min<uint8_t>(form,3U)];}
  if(speciesId==801U)return form?"ORIGINAL COLOR":"NORMAL";
  return "NORMAL";
}

bool canSelectForm(uint16_t speciesId){
  return speciesId==741U||speciesId==745U||speciesId==801U;
}

uint8_t baseHp(const OwnedPokemon& pokemon,uint8_t fallback){
  if(const auto* row=regional(pokemon))return row->hp;
  const uint8_t form=resolvedForm(pokemon,pokemon.form);
  if(pokemon.speciesId==746U&&form==1U)return 45U;
  if(pokemon.speciesId==774U)return 60U;
  if(pokemon.speciesId==800U)return 97U;
  return fallback;
}

uint8_t baseStat(const OwnedPokemon& pokemon,uint8_t statIndex,uint8_t fallback){
  if(const auto* row=regional(pokemon)){
    const uint8_t stats[]={row->atk,row->def,row->spe,row->spa,row->spd};
    return statIndex<5U?stats[statIndex]:fallback;
  }
  const uint8_t form=resolvedForm(pokemon,pokemon.form);
  if(pokemon.speciesId==658U&&form==1U){
    static constexpr uint8_t k[]={145,67,132,153,71};
    return statIndex<5U?k[statIndex]:fallback;
  }
  if(pokemon.speciesId==745U&&form>0U){
    static constexpr uint8_t k[2][5]={{115,75,82,55,75},{117,65,110,55,65}};
    return statIndex<5U?k[form-1U][statIndex]:fallback;
  }
  if(pokemon.speciesId==746U&&form==1U){static constexpr uint8_t k[]={140,130,30,140,135};return statIndex<5U?k[statIndex]:fallback;}
  if(pokemon.speciesId==774U&&form>=7U){static constexpr uint8_t k[]={100,60,120,100,60};return statIndex<5U?k[statIndex]:fallback;}
  if(pokemon.speciesId==800U&&form){
    static constexpr uint8_t k[3][5]={{157,127,77,113,109},{113,109,77,157,127},{167,97,129,167,97}};
    return statIndex<5U?k[form-1U][statIndex]:fallback;
  }
  return fallback;
}

PokemonType type1(const OwnedPokemon& pokemon,PokemonType fallback){
  if(const auto* row=regional(pokemon))return row->t1;
  const uint8_t form=resolvedForm(pokemon,pokemon.form);
  if(pokemon.speciesId==741U)return oricorioType(form);
  if(pokemon.speciesId==773U)return static_cast<PokemonType>(form);
  if(pokemon.speciesId==800U)return PokemonType::Psychic;
  return fallback;
}
PokemonType type2(const OwnedPokemon& pokemon,PokemonType fallback){
  if(const auto* row=regional(pokemon))return row->t2;
  const uint8_t form=resolvedForm(pokemon,pokemon.form);
  if(pokemon.speciesId==741U)return PokemonType::Flying;
  if(pokemon.speciesId==773U)return static_cast<PokemonType>(form);
  if(pokemon.speciesId==800U&&form==1U)return PokemonType::Steel;
  if(pokemon.speciesId==800U&&form==2U)return PokemonType::Ghost;
  if(pokemon.speciesId==800U&&form==3U)return PokemonType::Dragon;
  return fallback;
}

uint8_t abilityId(const OwnedPokemon& pokemon,uint8_t fallback){
  const uint8_t form=resolvedForm(pokemon,pokemon.form);
  if(form==1U){
    switch(pokemon.speciesId){
      case 19U:case 20U:
        return fallback==50U?83U:fallback==62U?55U:47U;
      case 27U:case 28U:return fallback==8U?82U:203U;
      case 37U:case 38U:return fallback==18U?82U:118U;
      case 50U:case 51U:return fallback==160U?160U:
          fallback==147U?147U:222U;
      case 52U:return fallback;
      case 53U:return fallback==102U?102U:fallback==156U?156U:170U;
      case 74U:case 75U:case 76U:
        return fallback==5U?5U:fallback==8U?207U:42U;
      case 88U:case 89U:
        return fallback==83U?83U:fallback==224U?224U:144U;
      case 103U:return fallback==140U?140U:120U;
      case 105U:return fallback==31U?31U:fallback==69U?69U:131U;
      default:break;
    }
  }
  if(pokemon.speciesId==658U&&form==1U)return 211U; // Battle Bond
  if(pokemon.speciesId==26U&&form==1U)return 208U; // Surge Surfer
  if(pokemon.speciesId==741U)return 217U;          // Dancer
  if(pokemon.speciesId==745U&&form==2U)return 182U;// Tough Claws
  if(pokemon.speciesId==746U)return 209U;          // Schooling
  if(pokemon.speciesId==773U)return 226U;          // RKS System
  if(pokemon.speciesId==774U)return 198U;          // Shields Down
  if(pokemon.speciesId==778U)return 210U;          // Disguise
  if(pokemon.speciesId==800U)return form==3U?234U:233U;
  return fallback;
}

PokemonType moveTypeOverride(const OwnedPokemon& pokemon,MoveId move,PokemonType fallback){
  const uint16_t id=static_cast<uint16_t>(move);
  if(pokemon.speciesId==773U&&id==672U)return type1(pokemon,PokemonType::Normal);
  if(pokemon.speciesId==741U&&id==649U)return type1(pokemon,fallback);
  return fallback;
}

bool reconcileUniqueItems(PokemonCollection& collection,
                          Gen7UniqueItemInventory& inventory){
  bool changed=false;
  const uint64_t removedBits=(inventory.bagMask|inventory.claimedMask)&
      kRemovedUltranecroziumZBit;
  if(removedBits){
    inventory.bagMask&=~kRemovedUltranecroziumZBit;
    inventory.claimedMask&=~kRemovedUltranecroziumZBit;
    changed=true;
  }
  for(OwnedPokemon& pokemon:collection.box){
    if(pokemon.uid==kEmptyPokemonUid||
       pokemon.heldItem!=HeldItem::RemovedUltranecroziumZ)continue;
    CollectionLogic::setHeldItemQuantity(pokemon,HeldItem::None,0U);
    changed=true;
  }
  if(containsSpecies(collection,773U)){
    for(uint8_t id=static_cast<uint8_t>(HeldItem::FightingMemory);
        id<=static_cast<uint8_t>(HeldItem::FairyMemory);++id){
      const int8_t index=gen7UniqueItemIndex(static_cast<HeldItem>(id));
      if(index<0)continue;
      const uint64_t bit=uint64_t{1}<<static_cast<uint8_t>(index);
      if(inventory.claimedMask&bit)continue;
      inventory.claimedMask|=bit;inventory.bagMask|=bit;changed=true;
    }
  }
  if(containsSpecies(collection,658U)){
    if(!(inventory.claimedMask&kAshsBondOwnershipBit)){
      inventory.claimedMask|=kAshsBondOwnershipBit;
      changed=true;
    }
    // Repair saves that recorded the one-time claim but lost the bag bit.
    // Do not duplicate the item while a Greninja is already holding it.
    if(!(inventory.bagMask&kAshsBondOwnershipBit)&&
       !containsHeldItem(collection,HeldItem::AshsBond)){
      inventory.bagMask|=kAshsBondOwnershipBit;
      changed=true;
    }
  }
  if(containsSpecies(collection,800U)){
    if(!(inventory.claimedMask&kNecrozmaFusersBit)){
      inventory.claimedMask|=kNecrozmaFusersBit;
      changed=true;
    }
  }
  return changed;
}

bool hasNecrozmaFusers(const Gen7UniqueItemInventory& inventory){
  return (inventory.claimedMask&kNecrozmaFusersBit)!=0U;
}

bool canFuse(const PokemonCollection& collection,const AlolaFormState& state,
             uint32_t necrozmaUid,uint32_t donorUid){
  if(state.fusedDonor.uid!=kEmptyPokemonUid||necrozmaUid==donorUid)return false;
  const OwnedPokemon* necrozma=CollectionLogic::find(collection,necrozmaUid);
  const OwnedPokemon* donor=CollectionLogic::find(collection,donorUid);
  return necrozma&&donor&&necrozma->speciesId==800U&&necrozma->form==0U&&
      (donor->speciesId==791U||donor->speciesId==792U);
}

bool fuse(PokemonCollection& collection,AlolaFormState& state,
          const Gen7UniqueItemInventory& inventory,uint32_t necrozmaUid,
          uint32_t donorUid){
  if(!hasNecrozmaFusers(inventory)||
     !canFuse(collection,state,necrozmaUid,donorUid))return false;
  OwnedPokemon* necrozma=CollectionLogic::find(collection,necrozmaUid);
  OwnedPokemon* donor=CollectionLogic::find(collection,donorUid);
  if(!necrozma||!donor)return false;
  state.fusedDonor=*donor;
  state.fusedNecrozmaUid=necrozmaUid;
  const bool dusk=donor->speciesId==791U;
  necrozma->form=dusk?1U:2U;
  installFusionSignatureMove(*necrozma,static_cast<MoveId>(dusk?667U:668U),
                             static_cast<MoveId>(dusk?668U:667U));
  for(uint8_t slot=0;slot<kPartyCapacity;++slot){
    if(collection.party[slot]!=donorUid)continue;
    bool necrozmaAlreadyActive=false;
    for(uint8_t other=0;other<kPartyCapacity;++other)
      if(collection.party[other]==necrozmaUid)necrozmaAlreadyActive=true;
    if(necrozmaAlreadyActive){
      for(uint8_t other=slot;other+1U<kPartyCapacity;++other)
        collection.party[other]=collection.party[other+1U];
      collection.party[kPartyCapacity-1U]=kEmptyPokemonUid;
    }else collection.party[slot]=necrozmaUid;
  }
  for(OwnedPokemon& candidate:collection.box)
    if(candidate.uid==donorUid){candidate=OwnedPokemon{};break;}
  CollectionLogic::refreshAbility(*necrozma,7U);
  CollectionLogic::refreshDerivedStats(*necrozma,true);
  return CollectionLogic::validate(collection);
}

bool unfuse(PokemonCollection& collection,AlolaFormState& state){
  if(state.fusedDonor.uid==kEmptyPokemonUid||!state.fusedNecrozmaUid)return false;
  OwnedPokemon* necrozma=CollectionLogic::find(collection,state.fusedNecrozmaUid);
  if(!necrozma||necrozma->speciesId!=800U)return false;
  OwnedPokemon* empty=nullptr;
  for(OwnedPokemon& candidate:collection.box)
    if(candidate.uid==kEmptyPokemonUid){empty=&candidate;break;}
  if(!empty)return false;
  *empty=state.fusedDonor;
  resetUltraBurst(*necrozma);
  necrozma->form=0U;
  necrozma->legacyCareCounterReserved&=~kUltraBurstDawnBit;
  CollectionLogic::refreshAbility(*necrozma,7U);
  CollectionLogic::refreshDerivedStats(*necrozma,true);
  state=AlolaFormState{};
  return CollectionLogic::validate(collection);
}

bool reconcileFusionState(PokemonCollection& collection,AlolaFormState& state,
                          uint32_t activeBattleUid){
  bool changed=false;
  if(state.fusedDonor.uid!=kEmptyPokemonUid&&
     state.fusedDonor.heldItem==HeldItem::RemovedUltranecroziumZ){
    CollectionLogic::setHeldItemQuantity(
        state.fusedDonor,HeldItem::None,0U);
    changed=true;
  }
  const bool hasDonor=state.fusedDonor.uid!=kEmptyPokemonUid;
  if(hasDonor&&!state.fusedNecrozmaUid){
    for(OwnedPokemon& candidate:collection.box){
      if(candidate.uid!=kEmptyPokemonUid)continue;
      candidate=state.fusedDonor;
      state=AlolaFormState{};
      changed=true;
      break;
    }
  }else if(!hasDonor&&state.fusedNecrozmaUid){
    state=AlolaFormState{};
    changed=true;
  }

  OwnedPokemon* fused=state.fusedNecrozmaUid?
      CollectionLogic::find(collection,state.fusedNecrozmaUid):nullptr;
  if(state.fusedDonor.uid!=kEmptyPokemonUid&&
     (!fused||fused->speciesId!=800U)){
    for(OwnedPokemon& candidate:collection.box){
      if(candidate.uid!=kEmptyPokemonUid)continue;
      candidate=state.fusedDonor;
      state=AlolaFormState{};
      changed=true;
      break;
    }
    fused=nullptr;
  }
  if(fused){
    const uint8_t persistentForm=state.fusedDonor.speciesId==792U?2U:1U;
    const bool validUltra=fused->form==3U&&fused->uid==activeBattleUid;
    if(!validUltra&&fused->form!=persistentForm){
      fused->form=persistentForm;
      fused->legacyCareCounterReserved&=~kUltraBurstDawnBit;
      CollectionLogic::refreshAbility(*fused,7U);
      CollectionLogic::refreshDerivedStats(*fused,true);
      changed=true;
    }
  }
  for(OwnedPokemon& pokemon:collection.box){
    if(pokemon.uid==kEmptyPokemonUid||pokemon.speciesId!=800U||
       (fused&&pokemon.uid==fused->uid))continue;
    if(pokemon.form==0U)continue;
    pokemon.form=0U;
    pokemon.legacyCareCounterReserved&=~kUltraBurstDawnBit;
    CollectionLogic::refreshAbility(pokemon,7U);
    CollectionLogic::refreshDerivedStats(pokemon,true);
    changed=true;
  }
  return changed;
}

uint16_t ashBondOutsideSecondsRemaining(const OwnedPokemon& pokemon){
  return pokemon.speciesId==658U&&pokemon.form==1U?
      packedAshBondSeconds(pokemon):0U;
}

bool resetAshBondForm(OwnedPokemon& pokemon){
  if(pokemon.speciesId!=658U)return false;
  const bool formChanged=pokemon.form!=0U;
  const bool timerChanged=packedAshBondSeconds(pokemon)!=0U;
  if(!formChanged&&!timerChanged)return false;
  pokemon.form=0U;
  setPackedAshBondSeconds(pokemon,0U);
  if(formChanged){
    CollectionLogic::refreshAbility(pokemon,7U);
    CollectionLogic::refreshDerivedStatsPreservingHpRatio(pokemon);
  }
  return formChanged;
}

bool beginAshBondBattle(OwnedPokemon& pokemon){
  if(pokemon.speciesId!=658U||pokemon.form!=1U||!pokemon.currentHp||
     pokemon.heldItem!=HeldItem::AshsBond)return false;
  const bool changed=packedAshBondSeconds(pokemon)!=0U;
  // Zero is the in-battle marker. It survives a battle checkpoint and lets
  // finishAshBondBattle distinguish a participating Greninja from another
  // transformed Greninja that merely remained in storage while time paused.
  setPackedAshBondSeconds(pokemon,0U);
  return changed;
}

bool finishAshBondBattle(PokemonCollection& collection){
  bool changed=false;
  for(OwnedPokemon& pokemon:collection.box){
    if(pokemon.uid==kEmptyPokemonUid||pokemon.speciesId!=658U)continue;
    if(pokemon.form!=1U){
      if(packedAshBondSeconds(pokemon)){
        setPackedAshBondSeconds(pokemon,0U);changed=true;
      }
      continue;
    }
    if(!pokemon.currentHp||pokemon.heldItem!=HeldItem::AshsBond){
      changed=resetAshBondForm(pokemon)||changed;
      continue;
    }
    // A nonzero value belongs to a transformed Greninja that never entered
    // this battle. Its clock was paused and must resume from the same value,
    // not be refreshed merely because another party member fought.
    if(!packedAshBondSeconds(pokemon)){
      setPackedAshBondSeconds(pokemon,kAshBondOutsideDurationSeconds);
      changed=true;
      CollectionLogic::refreshAbility(pokemon,7U);
    }
  }
  return changed;
}

bool advanceAshBondOutsideTime(PokemonCollection& collection,
                               uint32_t elapsedSeconds){
  bool visibleFormChanged=false;
  for(OwnedPokemon& pokemon:collection.box){
    if(pokemon.uid==kEmptyPokemonUid||pokemon.speciesId!=658U)continue;
    const uint16_t remaining=packedAshBondSeconds(pokemon);
    if(pokemon.form!=1U){
      if(remaining)setPackedAshBondSeconds(pokemon,0U);
      continue;
    }
    if(!pokemon.currentHp||pokemon.heldItem!=HeldItem::AshsBond||!remaining||
       elapsedSeconds>=remaining){
      visibleFormChanged=resetAshBondForm(pokemon)||visibleFormChanged;
      continue;
    }
    if(elapsedSeconds)
      setPackedAshBondSeconds(pokemon,static_cast<uint16_t>(
          remaining-elapsedSeconds));
  }
  return visibleFormChanged;
}

bool activateUltraBurst(OwnedPokemon& pokemon){
  if(pokemon.speciesId!=800U||(pokemon.form!=1U&&pokemon.form!=2U))return false;
  if(pokemon.form==2U)pokemon.legacyCareCounterReserved|=kUltraBurstDawnBit;
  else pokemon.legacyCareCounterReserved&=~kUltraBurstDawnBit;
  pokemon.form=3U;
  return true;
}

bool resetUltraBurst(OwnedPokemon& pokemon){
  if(pokemon.speciesId!=800U||pokemon.form!=3U)return false;
  pokemon.form=(pokemon.legacyCareCounterReserved&kUltraBurstDawnBit)?2U:1U;
  pokemon.legacyCareCounterReserved&=~kUltraBurstDawnBit;
  return true;
}
}
