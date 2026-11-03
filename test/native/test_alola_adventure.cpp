#include <cassert>
#include <cstring>
#include <cstdint>
#include <iostream>

#include "game/AlolaAdventure.h"
#include "game/AlolaForms.h"
#include "game/BattleAnimationData.h"
#include "game/Collection.h"
#include "game/HeldItems.h"
#include "game/Pokedex.h"

namespace {
uint64_t itemBit(HeldItem item){
  const int8_t index=gen7UniqueItemIndex(item);
  assert(index>=0);
  return uint64_t{1}<<static_cast<uint8_t>(index);
}
}

int main(){
  // Alolan regional variants are permanent identities, never selectable
  // transformations. Kanto and Alolan individuals may share a National Dex
  // number, but neither one can be toggled into the other from the Box.
  assert(AlolaForms::isRegionalSpecies(19U));
  assert(AlolaForms::isRegionalSpecies(105U));
  assert(!AlolaForms::isRegionalSpecies(741U));
  OwnedPokemon kantoMarowak=CollectionLogic::createPokemon(
      0U,105U,30U,false,0x10500001U,7U);
  assert(kantoMarowak.form==0U&&!AlolaForms::isRegionalVariant(kantoMarowak));
  assert(!CollectionLogic::canChangeForm(kantoMarowak));
  assert(!CollectionLogic::setForm(kantoMarowak,1U));
  OwnedPokemon alolanMarowak=kantoMarowak;
  alolanMarowak.form=1U;
  CollectionLogic::refreshAbility(alolanMarowak,7U);
  CollectionLogic::refreshDerivedStats(alolanMarowak,true);
  assert(AlolaForms::isRegionalVariant(alolanMarowak));
  assert(!CollectionLogic::canChangeForm(alolanMarowak));
  assert(!CollectionLogic::setForm(alolanMarowak,0U));

  PokemonCollection regionalEvolution{};
  CollectionLogic::initialize(regionalEvolution);
  OwnedPokemon rattata=CollectionLogic::createPokemon(
      0U,19U,20U,false,0x01900001U,7U);
  rattata.form=1U;
  uint32_t rattataUid=0U;
  assert(CollectionLogic::add(regionalEvolution,rattata,&rattataUid));
  assert(CollectionLogic::evolve(regionalEvolution,rattataUid,20U,nullptr,7U));
  const OwnedPokemon* raticate=CollectionLogic::find(regionalEvolution,rattataUid);
  assert(raticate&&raticate->speciesId==20U&&raticate->form==1U&&
         AlolaForms::isRegionalVariant(*raticate));

  PokemonCollection regionalTrade{};
  CollectionLogic::initialize(regionalTrade);
  assert(CollectionLogic::chooseStarter(regionalTrade,1U,0xA1070001U));
  uint32_t receivedUid=0U;
  assert(CollectionLogic::tradeReplace(regionalTrade,regionalTrade.party[0],
                                       alolanMarowak,&receivedUid));
  const OwnedPokemon* received=CollectionLogic::find(regionalTrade,receivedUid);
  assert(received&&received->speciesId==105U&&received->form==1U&&
         AlolaForms::isRegionalVariant(*received)&&
         !CollectionLogic::canChangeForm(*received));

  PokemonCollection collection{};
  CollectionLogic::initialize(collection);
  assert(CollectionLogic::chooseStarter(collection,1U,0xA107A001U));
  OwnedPokemon* player=CollectionLogic::active(collection,0U);assert(player);
  player->level=85U;player->currentHp=player->maximumHp;

  KalosAdventureState kalos{};
  kalos.flags=1U; // Kalos champion complete.
  AlolaAdventureState alola{};
  Gen7UniqueItemInventory items{};
  assert(AlolaAdventure::reconcile(alola,kalos));
  assert(AlolaAdventure::isAreaUnlocked(alola,AlolaArea::VerdantCavern));
  assert(!AlolaAdventure::sunThemeUnlocked(alola));
  assert(!AlolaAdventure::moonThemeUnlocked(alola));
  assert(!AlolaAdventure::setMoonThemeSelected(alola,true));

  // Every authored island/postgame route unlocks in sequence, has a Totem
  // encounter, and permanently awards its primary and optional bonus crystal.
  for(uint8_t area=0;area<AlolaAdventure::kAreaCount;++area){
    const AlolaArea selected=static_cast<AlolaArea>(area);
    assert(AlolaAdventure::isAreaUnlocked(alola,selected));
    assert(AlolaAdventure::begin(alola,selected,0xA1071000U+area,collection));
    alola.run.step=AlolaAdventure::kStepsPerArea;
    AlolaRoomChoice choice[2]{};
    assert(AlolaAdventure::choices(alola,choice)==1U);
    assert(choice[0].kind==AlolaRoomKind::Trial);
    const AlolaRoomResult room=AlolaAdventure::choose(alola,0U,collection);
    assert(room.startsBattle&&alola.run.pendingBattle==AlolaBattleRole::Totem);
    BattleState totemBattle{};
    assert(AlolaAdventure::startPendingBattle(
        alola,totemBattle,collection,player->uid));
    bool foundFeaturedTotem=false;
    for(uint8_t slot=0;slot<totemBattle.opponentCount;++slot){
      if(totemBattle.opponents[slot].speciesId!=
          (area==0U?734U:area==2U?751U:area==3U?105U:area==4U?753U:
           area==6U?738U:area==7U?769U:area==9U?782U:area==12U?793U:0U))continue;
      foundFeaturedTotem=true;
      const CombatVolatile& aura=totemBattle.opponentVolatiles[slot];
      if(area==0U||area==6U)assert(aura.defenseStage==1);
      if(area==2U)assert(aura.speedStage==1);
      if(area==3U||area==4U)assert(aura.speedStage==2);
      if(area==7U||area==9U||area==12U)
        assert(aura.attackStage==1&&aura.defenseStage==1&&
               aura.spAttackStage==1&&aura.spDefenseStage==1&&
               aura.speedStage==1);
    }
    const bool expectsAura=area==0U||area==2U||area==3U||area==4U||
        area==6U||area==7U||area==9U||area==12U;
    assert(foundFeaturedTotem==expectsAura);
    BattleEngine::clear(totemBattle);
    assert(AlolaAdventure::recordBattle(alola,BattleOutcome::Victory,items));
    assert(items.bagMask&itemBit(AlolaAdventure::areaZCrystal(selected)));
    const HeldItem bonus=AlolaAdventure::areaBonusZCrystal(selected);
    if(bonus!=HeldItem::None)assert(items.bagMask&itemBit(bonus));
    if(area==11U){
      assert(AlolaAdventure::sunThemeUnlocked(alola));
      assert(AlolaAdventure::moonThemeUnlocked(alola));
    }
  }
  assert(AlolaAdventure::championCompleted(alola));
  assert(AlolaAdventure::fullyCompleted(alola));
  assert(AlolaAdventure::trialCount(alola)==12U);
  for(uint8_t raw=static_cast<uint8_t>(HeldItem::NormaliumZ);
      raw<=static_cast<uint8_t>(HeldItem::FairiumZ);++raw)
    assert(items.bagMask&itemBit(static_cast<HeldItem>(raw)));

  Inventory inventory{};
  const uint16_t masterBefore=
      inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)];
  assert(AlolaAdventure::reconcileCompletionReward(alola,inventory));
  assert(inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)]==
         masterBefore+1U);
  assert(!AlolaAdventure::reconcileCompletionReward(alola,inventory));
  assert(AlolaAdventure::setSunThemeSelected(alola,true));
  assert(AlolaAdventure::sunThemeSelected(alola));
  assert(!AlolaAdventure::moonThemeSelected(alola));
  assert(AlolaAdventure::setMoonThemeSelected(alola,true));
  assert(!AlolaAdventure::sunThemeSelected(alola));
  assert(AlolaAdventure::moonThemeSelected(alola));
  assert(AlolaAdventure::setSunThemeSelected(alola,true));
  assert(AlolaAdventure::sunThemeSelected(alola));
  assert(!AlolaAdventure::moonThemeSelected(alola));

  // Trial healing restores living partners but never revives a fainted one.
  assert(AlolaAdventure::begin(alola,AlolaArea::VerdantCavern,
                               0xA1072000U,collection));
  player->currentHp=0U;player->recoverySecondsRemaining=9U;
  assert(AlolaAdventure::useAreaHeal(alola,collection));
  assert(player->currentHp==0U&&player->recoverySecondsRemaining==9U);
  assert(!AlolaAdventure::useAreaHeal(alola,collection));
  assert(AlolaAdventure::abandon(alola));
  player->currentHp=player->maximumHp;player->recoverySecondsRemaining=0U;

  // SOS paths deliberately source a missing hidden Ability and preserve it
  // on the battle opponent.
  bool foundSos=false;
  for(uint32_t seed=1U;seed<2000U&&!foundSos;++seed){
    assert(AlolaAdventure::begin(alola,AlolaArea::VerdantCavern,seed,collection));
    AlolaRoomChoice choices[2]{};
    const uint8_t count=AlolaAdventure::choices(alola,choices);
    if(count==2U&&choices[1].kind==AlolaRoomKind::SosChain){
      const AlolaRoomResult room=AlolaAdventure::choose(alola,1U,collection);
      assert(room.startsBattle&&room.hiddenAbilityEncounter);
      BattleState battle{};
      assert(AlolaAdventure::startPendingBattle(
          alola,battle,collection,player->uid));
      assert(CollectionLogic::hasHiddenAbility(battle.opponents[0]));
      foundSos=true;
    }
    AlolaAdventure::abandon(alola);
  }
  assert(foundSos);

  // Area rare rolls contain only Alola's own eleven Legendaries and three
  // Mythicals. Ultra Beasts are capturable through explicit wormhole rooms,
  // never through the ordinary per-site Legendary roll.
  assert(PokedexLogic::isLegendaryOrMythical(772U));
  assert(PokedexLogic::isLegendaryOrMythical(773U));
  static_assert(AlolaAdventure::kLegendaryChanceDenominator==240U,
                "Alola's authored rare chance must remain 1/240");
  constexpr uint16_t alolaRareSpecies[]={
    772U,773U,785U,786U,787U,788U,789U,790U,791U,792U,
    800U,801U,802U,807U,
  };
  constexpr uint16_t ultraBeastSpecies[]={
    793U,794U,795U,796U,797U,798U,799U,803U,804U,805U,806U,
  };
  const auto isAlolaRare=[&](uint16_t species){
    for(uint16_t candidate:alolaRareSpecies)
      if(candidate==species)return true;
    return false;
  };
  const auto isUltraBeast=[&](uint16_t species){
    for(uint16_t candidate:ultraBeastSpecies)
      if(candidate==species)return true;
    return false;
  };
  bool encountered[808]{};
  uint32_t regularCommonEncounters=0U,ultraCommonEncounters=0U;
  uint32_t rainbowCommonEncounters=0U;
  constexpr uint16_t rainbowCommonSpecies[]={
    34U,53U,105U,20U,89U,110U,229U,430U,553U,625U,687U,758U,
  };
  for(uint8_t area=0;area<AlolaAdventure::kAreaCount;++area){
    const uint8_t expectedInArea=
        area==static_cast<uint8_t>(AlolaArea::RainbowRocket)?14U:2U;
    uint8_t foundInArea=0U;
    uint16_t areaSpecies[14]{};
    for(uint32_t seed=1U;seed<500000U&&foundInArea<expectedInArea;++seed){
      AlolaAdventureState hunt{};
      hunt.unlockedAreas=AlolaAdventure::kAllAreasMask;
      hunt.run.seed=seed;hunt.run.area=area;hunt.run.flags=1U;
      AlolaRoomChoice choices[2]{};
      if(AlolaAdventure::choices(hunt,choices)!=2U||
         choices[1].kind==AlolaRoomKind::SosChain||
         choices[1].kind==AlolaRoomKind::UltraWormhole||
         choices[1].kind==AlolaRoomKind::Item||
         choices[1].kind==AlolaRoomKind::Healing)continue;
      const AlolaRoomResult room=AlolaAdventure::choose(hunt,1U,collection);
      assert(room.startsBattle&&room.speciesId<808U);
      if(!room.legendaryEncounter){
        if(area==static_cast<uint8_t>(AlolaArea::UltraWormhole)){
          assert(room.speciesId!=800U&&room.speciesId!=803U);
          ++ultraCommonEncounters;
        }else{
          assert(!PokedexLogic::isLegendaryOrMythical(room.speciesId));
          ++regularCommonEncounters;
        }
      }
      if(area==static_cast<uint8_t>(AlolaArea::RainbowRocket)&&
         !room.legendaryEncounter){
        bool isAuthoredCommon=false;
        for(uint16_t species:rainbowCommonSpecies)
          isAuthoredCommon|=room.speciesId==species;
        assert(isAuthoredCommon);
        assert(!PokedexLogic::isLegendaryOrMythical(room.speciesId));
        ++rainbowCommonEncounters;
      }
      if(!room.legendaryEncounter)continue;
      assert(isAlolaRare(room.speciesId));
      assert(!isUltraBeast(room.speciesId));
      encountered[room.speciesId]=true;
      bool alreadyFound=false;
      for(uint8_t i=0U;i<foundInArea;++i)
        alreadyFound|=room.speciesId==areaSpecies[i];
      if(alreadyFound)continue;
      areaSpecies[foundInArea++]=room.speciesId;
    }
    assert(foundInArea==expectedInArea);
  }
  assert(regularCommonEncounters>0U&&ultraCommonEncounters>0U&&
         rainbowCommonEncounters>0U);
  for(uint16_t species:alolaRareSpecies)
    assert(encountered[species]);

  // Explicit wormhole rooms use the ten-species common Ultra Beast pool and
  // apply the same 1/240 rare roll for Necrozma and Poipole.
  bool wormholeSpecies[808]{};
  constexpr uint16_t commonWormholeSpecies[]={
    793U,794U,795U,796U,797U,798U,799U,804U,805U,806U,
  };
  uint8_t foundWormholeCommon=0U,foundWormholeRare=0U;
  uint32_t wormholeEncounters=0U,wormholeRareEncounters=0U;
  for(uint32_t seed=1U;seed<500000U;++seed){
    AlolaAdventureState hunt{};
    hunt.unlockedAreas=AlolaAdventure::kAllAreasMask;
    hunt.run.seed=seed;
    hunt.run.area=static_cast<uint8_t>(AlolaArea::RainbowRocket);
    hunt.run.flags=1U;
    AlolaRoomChoice choices[2]{};
    if(AlolaAdventure::choices(hunt,choices)!=2U||
       choices[1].kind!=AlolaRoomKind::UltraWormhole)continue;
    const AlolaRoomResult room=AlolaAdventure::choose(hunt,1U,collection);
    assert(room.startsBattle&&room.legendaryEncounter);
    ++wormholeEncounters;
    const bool rare=room.speciesId==800U||room.speciesId==803U;
    if(rare){
      ++wormholeRareEncounters;
      if(!wormholeSpecies[room.speciesId])++foundWormholeRare;
    }else{
      bool authoredCommon=false;
      for(uint16_t species:commonWormholeSpecies)
        authoredCommon|=room.speciesId==species;
      assert(authoredCommon);
      if(!wormholeSpecies[room.speciesId])++foundWormholeCommon;
    }
    if(!wormholeSpecies[room.speciesId]){
      wormholeSpecies[room.speciesId]=true;
    }
  }
  assert(foundWormholeCommon==10U&&foundWormholeRare==2U);
  for(uint16_t species:ultraBeastSpecies)assert(wormholeSpecies[species]);
  assert(wormholeSpecies[800U]);
  assert(wormholeRareEncounters>0U&&wormholeEncounters>0U);
  assert(wormholeRareEncounters*100U<wormholeEncounters);

  // Owning Necrozma grants the paired fusion key function, but no private
  // crystal. Legacy Ultranecrozium inventory/equipment is removed while
  // Solgaleo/Lunala remain byte-exact in the fusion sidecar.
  uint32_t necrozmaUid=0U,solgaleoUid=0U,lunalaUid=0U;
  OwnedPokemon necrozma=CollectionLogic::createPokemon(
      0U,800U,100U,false,0x80000001U,7U);
  necrozma.moves[0]=static_cast<MoveId>(675U); // Photon Geyser is preserved.
  necrozma.movePp[0]=findFullMove(necrozma.moves[0])->pp;
  necrozma.moves[1]=static_cast<MoveId>(14U);  // Any Z-Move may Ultra Burst.
  necrozma.movePp[1]=findFullMove(necrozma.moves[1])->pp;
  assert(CollectionLogic::add(collection,necrozma,&necrozmaUid));
  assert(CollectionLogic::add(collection,CollectionLogic::createPokemon(
      0U,791U,70U,false,0x79100001U,7U),&solgaleoUid));
  assert(CollectionLogic::add(collection,CollectionLogic::createPokemon(
      0U,792U,70U,false,0x79200001U,7U),&lunalaUid));
  OwnedPokemon* collectedNecrozma=CollectionLogic::find(collection,necrozmaUid);
  assert(collectedNecrozma);
  collectedNecrozma->heldItem=HeldItem::RemovedUltranecroziumZ;
  items.bagMask|=kRemovedUltranecroziumZBit;
  items.claimedMask|=kRemovedUltranecroziumZBit;
  assert(AlolaForms::reconcileUniqueItems(collection,items));
  assert(AlolaForms::hasNecrozmaFusers(items));
  assert(!(items.bagMask&kRemovedUltranecroziumZBit));
  assert(!(items.claimedMask&kRemovedUltranecroziumZBit));
  assert(collectedNecrozma->heldItem==HeldItem::None);
  assert(gen7UniqueItemIndex(HeldItem::RemovedUltranecroziumZ)<0);
  assert(!heldItemIsZCrystal(HeldItem::RemovedUltranecroziumZ));
  assert(!AlolaForms::reconcileUniqueItems(collection,items));

  AlolaFormState formState{};
  assert(AlolaForms::fuse(collection,formState,items,necrozmaUid,solgaleoUid));
  OwnedPokemon* fused=CollectionLogic::find(collection,necrozmaUid);assert(fused);
  assert(fused->form==1U&&CollectionLogic::find(collection,solgaleoUid)==nullptr);
  assert(fused->moves[0]==static_cast<MoveId>(675U));
  assert(AlolaForms::unfuse(collection,formState));
  assert(CollectionLogic::find(collection,solgaleoUid));
  assert(AlolaForms::fuse(collection,formState,items,necrozmaUid,lunalaUid));
  fused=CollectionLogic::find(collection,necrozmaUid);assert(fused&&fused->form==2U);
  assert(CollectionLogic::find(collection,lunalaUid)==nullptr);
  formState.fusedDonor.heldItem=HeldItem::RemovedUltranecroziumZ;
  assert(AlolaForms::reconcileFusionState(collection,formState,necrozmaUid));
  assert(formState.fusedDonor.heldItem==HeldItem::None);
  assert(!AlolaForms::reconcileFusionState(collection,formState,necrozmaUid));
  MoveId relearn[64]{};
  const uint8_t relearnCount=relearnableMovesForForm(
      800U,2U,100U,relearn,64U,7U);
  bool foundMoongeist=false;
  for(uint8_t i=0;i<relearnCount;++i)
    foundMoongeist|=relearn[i]==static_cast<MoveId>(668U);
  assert(foundMoongeist);

  Inventory ultraInventory{};
  SpecialHeldItemInventory specialItems{};
  Gen4UniqueItemInventory gen4Items{};
  Gen5UniqueItemInventory gen5Items{};
  uint64_t machines=0U;
  // Even if a save reaches this UI before its boot repair is persisted, the
  // normal replace path must discard the retired tombstone instead of trying
  // to return an item which has no inventory slot.
  fused->heldItem=HeldItem::RemovedUltranecroziumZ;
  assert(std::strcmp(heldItemName(fused->heldItem),"NONE")==0);
  assert(equipHeldItem(*fused,HeldItem::NormaliumZ,
                       ultraInventory.heldItems,specialItems,gen4Items,
                       gen5Items,items,machines));
  assert(fused->heldItem==HeldItem::NormaliumZ);
  assert(CollectionLogic::setPartySlot(collection,1U,necrozmaUid));
  const uint32_t starterUid=collection.party[0];
  BattleState ultraBattle{};BattleEngine::clear(ultraBattle);
  ultraBattle.active=true;ultraBattle.kind=BattleKind::Pvp;
  ultraBattle.outcome=BattleOutcome::Ongoing;
  ultraBattle.playerUid=necrozmaUid;ultraBattle.opponentCount=1U;
  ultraBattle.unlockedGeneration=7U;ultraBattle.rngState=0x80000002U;
  ultraBattle.opponents[0]=CollectionLogic::createPokemon(
      0xF8000001U,242U,100U,false,0x80000003U,7U);
  ultraBattle.opponents[0].maximumHp=60000U;
  ultraBattle.opponents[0].currentHp=60000U;
  ultraBattle.opponents[0].moves[0]=static_cast<MoveId>(150U);
  ultraBattle.opponents[0].movePp[0]=40U;
  assert(BattleEngine::armPlayerZMove(
      ultraBattle,collection,static_cast<MoveId>(14U)));
  const BattleActionResult burst=BattleEngine::fightPvp(
      ultraBattle,collection,1U,0xFEU);
  assert(burst.accepted&&!ultraBattle.playerZMoveUsed&&
         ultraBattle.playerLegacyUltraBurstBonusGranted&&fused->form==3U);
  int16_t formEvent=-1,zEvent=-1;
  for(uint8_t i=0;i<burst.eventCount;++i){
    const BattleEvent& event=burst.events[i];
    if(event.type!=BattleEventType::MoveEffect)continue;
    if(event.value==static_cast<uint16_t>(BattleMoveEffect::FormChanged)){
      formEvent=i;assert(event.before==2U&&event.after==3U);
    }
    if(event.value==static_cast<uint16_t>(BattleMoveEffect::ZMoveUsed))zEvent=i;
  }
  assert(formEvent>=0&&zEvent>formEvent);
  OwnedPokemon* starter=CollectionLogic::find(collection,starterUid);assert(starter);
  starter->heldItem=HeldItem::NormaliumZ;
  starter->moves[0]=static_cast<MoveId>(33U);
  starter->movePp[0]=findFullMove(starter->moves[0])->pp;
  assert(!BattleEngine::canUseZMove(
      ultraBattle,*starter,starter->moves[0],BattleSide::Player));
  assert(BattleEngine::armPlayerZMove(
      ultraBattle,collection,static_cast<MoveId>(14U)));
  const BattleActionResult secondZ=BattleEngine::fightPvp(
      ultraBattle,collection,1U,0xFEU);
  assert(secondZ.accepted&&ultraBattle.playerZMoveUsed&&
         ultraBattle.playerLegacyUltraBurstBonusGranted&&fused->form==3U);
  assert(!BattleEngine::armPlayerZMove(
      ultraBattle,collection,static_cast<MoveId>(14U)));
  const BattleActionResult switched=BattleEngine::switchToPokemon(
      ultraBattle,collection,starterUid);
  assert(switched.accepted&&fused->form==2U);
  assert(formState.fusedDonor.uid==lunalaUid);

  // A fresh battle with Psychium Z turns Photon Geyser into Light That Burns
  // the Sky while activating Ultra Burst. That action refunds exactly one
  // charge reserved for this Ultra Necrozma.
  assert(equipHeldItem(*fused,HeldItem::PsychiumZ,
                       ultraInventory.heldItems,specialItems,gen4Items,
                       gen5Items,items,machines));
  BattleState photonBattle{};BattleEngine::clear(photonBattle);
  photonBattle.active=true;photonBattle.kind=BattleKind::Pvp;
  photonBattle.outcome=BattleOutcome::Ongoing;
  photonBattle.playerUid=necrozmaUid;photonBattle.opponentCount=1U;
  photonBattle.unlockedGeneration=7U;photonBattle.rngState=0x80000004U;
  photonBattle.opponents[0]=CollectionLogic::createPokemon(
      0xF8000002U,242U,100U,false,0x80000005U,7U);
  photonBattle.opponents[0].maximumHp=60000U;
  photonBattle.opponents[0].currentHp=60000U;
  photonBattle.opponents[0].moves[0]=static_cast<MoveId>(150U);
  photonBattle.opponents[0].movePp[0]=40U;
  assert(BattleEngine::armPlayerZMove(
      photonBattle,collection,static_cast<MoveId>(675U)));
  const BattleActionResult ultraPhoton=BattleEngine::fightPvp(
      photonBattle,collection,0U,0xFEU);
  assert(ultraPhoton.accepted&&!photonBattle.playerZMoveUsed&&
         photonBattle.playerLegacyUltraBurstBonusGranted&&fused->form==3U);
  assert(photonBattle.opponents[0].currentHp<60000U);
  bool signatureAnnounced=false,signaturePower=false;
  for(uint8_t i=0;i<ultraPhoton.eventCount;++i){
    const BattleEvent& event=ultraPhoton.events[i];
    if(event.type==BattleEventType::MoveUsed&&event.move==static_cast<MoveId>(675U))
      signaturePower|=event.after==200U;
    if(event.type==BattleEventType::MoveEffect&&
       event.value==static_cast<uint16_t>(BattleMoveEffect::ZMoveUsed)){
      const char* name=BattleEngine::signatureZMoveName(event.after);
      signatureAnnounced|=name&&std::strcmp(name,"LIGHT THAT BURNS THE SKY")==0;
      assert(BattleEngine::signatureZMoveAnimationMoveId(
          event.after,static_cast<MoveId>(675U))==
          kLightThatBurnsTheSkyAnimationMoveId);
    }
  }
  assert(signaturePower&&signatureAnnounced);
  assert(BattleEngine::armPlayerZMove(
      photonBattle,collection,static_cast<MoveId>(675U)));

  std::cout<<"Alola adventure passed: 14 regional Legendary/Mythical hunts, "
              "18 Z-Crystals, Necrozma fusion/Ultra Burst, SOS, Sun/Moon themes "
              "and Master Ball verified.\n";
}
