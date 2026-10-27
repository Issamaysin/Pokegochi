#include "game/UnovaForms.h"

#include <algorithm>
#include <iterator>

namespace {
bool containsSpecies(const PokemonCollection& collection,uint16_t species){
  for(const auto& pokemon:collection.box)
    if(pokemon.uid!=kEmptyPokemonUid&&pokemon.speciesId==species)return true;
  return false;
}
void replaceMove(OwnedPokemon& pokemon,MoveId from,MoveId to){
  for(uint8_t slot=0;slot<kMoveSlots;++slot){
    if(pokemon.moves[slot]!=from)continue;
    pokemon.moves[slot]=to;
    const FullMoveData* data=findFullMove(to);
    pokemon.movePp[slot]=data?data->pp:0U;
    CollectionLogic::clearMovePpUps(pokemon,slot);
  }
}
}

namespace UnovaForms {
bool reconcileUniqueItems(const PokemonCollection& collection,
                          Gen5UniqueItemInventory& items,bool unovaUnlocked){
  if(!unovaUnlocked)return false;
  bool changed=false;
  if(containsSpecies(collection,646U)&&!(items.claimedMask&kDnaSplicersBit)){
    items.claimedMask|=kDnaSplicersBit;items.keyItems|=kDnaSplicersBit;changed=true;
  }
  if(containsSpecies(collection,649U)&&!(items.claimedMask&0x04U)){
    items.claimedMask|=0x04U;
    for(auto& quantity:items.driveQuantities)if(!quantity)quantity=1U;
    changed=true;
  }
  return changed;
}
bool hasDnaSplicers(const Gen5UniqueItemInventory& items){return(items.keyItems&kDnaSplicersBit)!=0U;}
bool canFuse(const PokemonCollection& collection,const UnovaFormState& state,
             uint32_t kyuremUid,uint32_t donorUid){
  if(state.fusedDonor.uid!=kEmptyPokemonUid||kyuremUid==donorUid)return false;
  const OwnedPokemon* kyurem=CollectionLogic::find(collection,kyuremUid);
  const OwnedPokemon* donor=CollectionLogic::find(collection,donorUid);
  return kyurem&&donor&&kyurem->speciesId==646U&&kyurem->form==0U&&
      (donor->speciesId==643U||donor->speciesId==644U);
}
bool fuse(PokemonCollection& collection,UnovaFormState& state,
          const Gen5UniqueItemInventory& items,uint32_t kyuremUid,uint32_t donorUid){
  if(!hasDnaSplicers(items)||!canFuse(collection,state,kyuremUid,donorUid))return false;
  OwnedPokemon* kyurem=CollectionLogic::find(collection,kyuremUid);
  OwnedPokemon* donor=CollectionLogic::find(collection,donorUid);
  if(!kyurem||!donor)return false;
  state.fusedDonor=*donor;state.fusedKyuremUid=kyuremUid;
  std::copy(std::begin(kyurem->moves),std::end(kyurem->moves),state.kyuremOriginalMoves);
  std::copy(std::begin(kyurem->movePp),std::end(kyurem->movePp),state.kyuremOriginalPp);
  const bool white=donor->speciesId==643U;
  kyurem->form=white?2U:1U;
  replaceMove(*kyurem,static_cast<MoveId>(184U),
              static_cast<MoveId>(white?558U:559U));
  replaceMove(*kyurem,static_cast<MoveId>(549U),
              static_cast<MoveId>(white?554U:553U));
  for(uint8_t slot=0;slot<kPartyCapacity;++slot){
    if(collection.party[slot]!=donorUid)continue;
    bool kyuremAlreadyActive=false;
    for(uint8_t other=0;other<kPartyCapacity;++other)
      if(collection.party[other]==kyuremUid)kyuremAlreadyActive=true;
    if(kyuremAlreadyActive){
      for(uint8_t other=slot;other+1U<kPartyCapacity;++other)
        collection.party[other]=collection.party[other+1U];
      collection.party[kPartyCapacity-1U]=kEmptyPokemonUid;
    }else collection.party[slot]=kyuremUid;
  }
  for(auto& candidate:collection.box)
    if(candidate.uid==donorUid){candidate=OwnedPokemon{};break;}
  CollectionLogic::refreshAbility(*kyurem,5U);
  CollectionLogic::refreshDerivedStats(*kyurem,true);
  return CollectionLogic::validate(collection);
}
bool unfuse(PokemonCollection& collection,UnovaFormState& state){
  if(state.fusedDonor.uid==kEmptyPokemonUid||!state.fusedKyuremUid)return false;
  OwnedPokemon* kyurem=CollectionLogic::find(collection,state.fusedKyuremUid);
  if(!kyurem||kyurem->speciesId!=646U)return false;
  OwnedPokemon* empty=nullptr;
  for(auto& candidate:collection.box)if(candidate.uid==kEmptyPokemonUid){empty=&candidate;break;}
  if(!empty)return false;
  *empty=state.fusedDonor;
  const uint8_t fusedForm=CollectionLogic::resolvedForm(*kyurem);
  kyurem->form=0U;
  // Revert only the two form-bound moves. Restoring the complete pre-fusion
  // snapshot discarded TMs and Summary edits made while Kyurem was fused.
  // The sidecar fields remain serialized for backward save compatibility.
  if(fusedForm==1U){
    replaceMove(*kyurem,static_cast<MoveId>(559U),static_cast<MoveId>(184U));
    replaceMove(*kyurem,static_cast<MoveId>(553U),static_cast<MoveId>(549U));
  }else if(fusedForm==2U){
    replaceMove(*kyurem,static_cast<MoveId>(558U),static_cast<MoveId>(184U));
    replaceMove(*kyurem,static_cast<MoveId>(554U),static_cast<MoveId>(549U));
  }
  CollectionLogic::refreshAbility(*kyurem,5U);
  CollectionLogic::refreshDerivedStats(*kyurem,true);
  state=UnovaFormState{};
  return CollectionLogic::validate(collection);
}
}
