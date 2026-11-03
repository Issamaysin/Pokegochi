#include <cassert>
#include <cstring>
#include <cstdint>
#include <iostream>

#include "game/BattleAnimationData.h"
#include "game/BattleEngine.h"
#include "game/Collection.h"
#include "game/PokemonData.h"

namespace {
constexpr uint16_t kFirst=622U,kLast=675U;

bool saw(const BattleActionResult& result,BattleEventType type,MoveId move,
         BattleSide side){
  for(uint8_t i=0;i<result.eventCount;++i){const BattleEvent& event=result.events[i];
    if(event.type==type&&event.move==move&&event.side==side)return true;}
  return false;
}
bool failed(const BattleActionResult& result,MoveId move){
  for(uint8_t i=0;i<result.eventCount;++i){const BattleEvent& event=result.events[i];
    if(event.type==BattleEventType::MoveEffect&&event.move==move&&
       event.value==static_cast<uint16_t>(BattleMoveEffect::Failed))return true;}
  return false;
}

struct Fixture {
  PokemonCollection collection{};BattleState battle{};MoveId move;bool enemy;
  Fixture(MoveId selected,bool foe):move(selected),enemy(foe){
    CollectionLogic::initialize(collection);
    assert(CollectionLogic::chooseStarter(collection,1U,0x77000001U));
    OwnedPokemon* player=CollectionLogic::active(collection,0U);assert(player);
    const uint32_t uid=player->uid;
    *player=CollectionLogic::createPokemon(uid,151U,100U,false,0x77000002U,7U);
    player->maximumHp=30000U;player->currentHp=15000U;
    player->moves[0]=enemy?static_cast<MoveId>(33U):move;
    player->movePp[0]=findFullMove(player->moves[0])->pp;

    BattleEngine::clear(battle);battle.active=true;battle.kind=BattleKind::Pvp;
    battle.outcome=BattleOutcome::Ongoing;battle.playerUid=uid;
    battle.opponentCount=1U;battle.unlockedGeneration=7U;battle.rngState=0x77000003U;
    battle.opponents[0]=CollectionLogic::createPokemon(
        0xF7000001U,151U,100U,false,0x77000004U,7U);
    OwnedPokemon& opponent=battle.opponents[0];
    opponent.maximumHp=30000U;opponent.currentHp=15000U;
    opponent.moves[0]=enemy?move:static_cast<MoveId>(33U);
    opponent.movePp[0]=findFullMove(opponent.moves[0])->pp;
    OwnedPokemon& user=enemy?opponent:*player;
    OwnedPokemon& target=enemy?*player:opponent;
    CombatVolatile& userState=enemy?battle.opponentVolatiles[0]:battle.playerVolatile;
    CombatVolatile& targetState=enemy?battle.playerVolatile:battle.opponentVolatiles[0];
    user.level=100U;target.level=1U;userState.accuracyStage=kMaximumBattleStatStage;
    targetState.evasionStage=kMinimumBattleStatStage;
    const uint16_t id=static_cast<uint16_t>(move);
    if(id==637U)user.abilityId=57U; // PLUS makes Gear Up useful in singles.
    if(id==645U)user.speciesId=4U; // Burn Up requires the Fire type.
    if(id==648U)target.status=StatusCondition::Poison;
    if(id==652U){targetState.lastMoveUsed=static_cast<MoveId>(33U);
      target.moves[0]=static_cast<MoveId>(33U);target.movePp[0]=35U;}
    if(id==657U){battle.weather=BattleWeather::Hail;battle.weatherTurns=5U;}
    if(id==632U){battle.weather=BattleWeather::Sun;battle.weatherTurns=5U;}
    if(id==658U){ // Shell Trap must be struck physically before its user acts.
      user.level=1U;target.level=100U;
      target.moves[0]=static_cast<MoveId>(33U);target.movePp[0]=35U;
    }
  }
  BattleActionResult use(){
    const bool bothAct=static_cast<uint16_t>(move)==658U;
    return BattleEngine::fightPvp(battle,collection,0U,enemy||bothAct?0U:0xFEU);
  }
  OwnedPokemon& target(){
    if(enemy){OwnedPokemon* p=CollectionLogic::find(collection,battle.playerUid);assert(p);return *p;}
    return battle.opponents[0];
  }
};
}

int main(){
  uint16_t audited=0U,executed=0U,excluded=0U;
  for(uint16_t raw=kFirst;raw<=kLast;++raw){
    const MoveId move=static_cast<MoveId>(raw);const FullMoveData* data=findFullMove(move);
    assert(data&&data->id==raw);
    const BattleAnimationData* descriptor=findBattleAnimation(raw);
    const BattleAnimationProgram* program=findBattleAnimationProgram(raw);
    assert(descriptor&&descriptor->moveId==raw);
    assert(program&&program->moveId==raw&&program->commandCount>0U);++audited;
    if(raw==629U||raw==634U){assert(!isMoveAvailableInSingleBattle(move));++excluded;continue;}
    assert(isMoveAvailableInSingleBattle(move));
    for(bool enemy:{false,true}){
      Fixture fixture(move,enemy);const uint16_t before=fixture.target().currentHp;
      const BattleActionResult result=fixture.use();
      const BattleSide side=enemy?BattleSide::Opponent:BattleSide::Player;
      if(failed(result,move))std::cerr<<"Unexpected Gen-VII failure "<<raw<<" "
          <<data->name<<" side="<<(enemy?"foe":"player")<<"\n";
      assert(result.accepted);assert(saw(result,BattleEventType::MoveUsed,move,side));
      assert(!saw(result,BattleEventType::MoveMissed,move,side));assert(!failed(result,move));
      if(data->power)assert(fixture.target().currentHp<before);
      ++executed;
    }
  }
  assert(audited==54U&&executed==104U&&excluded==2U);
  assert(zStatusEffectForMove(static_cast<MoveId>(14U))==ZStatusEffect::ResetStats);
  assert(zStatusEffectForMove(static_cast<MoveId>(100U))==ZStatusEffect::RecoverHp);

  // Every signature Z-Move resolves to the romhack's own authored program.
  // The presentation-code order is BattleEngine's serialized journal ABI;
  // the animation IDs are pokeemerald-expansion's canonical 866..882 rows.
  constexpr uint16_t signaturePrograms[]={
    866U,867U,868U,869U,870U,871U,872U,873U,874U,
    878U,882U,876U,875U,877U,879U,880U,881U,
  };
  for(uint16_t code=1U;code<=17U;++code){
    const uint16_t animation=BattleEngine::signatureZMoveAnimationMoveId(
        code,static_cast<MoveId>(33U));
    assert(animation==signaturePrograms[code-1U]);
    const BattleAnimationData* descriptor=findBattleAnimation(animation);
    const BattleAnimationProgram* program=findBattleAnimationProgram(animation);
    assert(descriptor&&descriptor->moveId==animation);
    assert(program&&program->moveId==animation&&program->commandCount>0U);
    assert(BattleEngine::signatureZMoveName(code));
  }
  assert(BattleEngine::signatureZMoveAnimationMoveId(
      0U,static_cast<MoveId>(33U))==33U);
  assert(BattleEngine::signatureZMoveName(0U)==nullptr);
  assert(findBattleAnimationProgram(kLightThatBurnsTheSkyAnimationMoveId)->
      commandCount>=150U);

  // All type crystals use the expansion's canonical Normal..Fairy block.
  // These IDs are stored directly in the transient presentation journal.
  for(uint16_t animation=kFirstGenericZMoveAnimationMoveId;
      animation<=kLastGenericZMoveAnimationMoveId;++animation){
    assert(BattleEngine::zMovePresentationAnimationMoveId(
        animation,static_cast<MoveId>(33U))==animation);
    assert(BattleEngine::zMovePresentationName(animation));
    const BattleAnimationData* descriptor=findBattleAnimation(animation);
    const BattleAnimationProgram* program=findBattleAnimationProgram(animation);
    assert(descriptor&&descriptor->moveId==animation);
    assert(program&&program->moveId==animation&&program->commandCount>0U);
  }

  // Exact reported regression: Psycho Cut + Psychium Z must become
  // Shattered Psyche in presentation while mechanics retain move 427.
  {
    Fixture fixture(static_cast<MoveId>(427U),false);
    OwnedPokemon* mew=CollectionLogic::find(
        fixture.collection,fixture.battle.playerUid);assert(mew);
    mew->heldItem=HeldItem::PsychiumZ;
    assert(BattleEngine::armPlayerZMove(
        fixture.battle,fixture.collection,static_cast<MoveId>(427U)));
    const BattleActionResult result=fixture.use();
    bool shatteredPsyche=false;
    for(uint8_t i=0;i<result.eventCount;++i){
      const BattleEvent& event=result.events[i];
      if(event.type==BattleEventType::MoveEffect&&
         event.value==static_cast<uint16_t>(BattleMoveEffect::ZMoveUsed)){
        assert(event.move==static_cast<MoveId>(427U));
        assert(event.after==861U);
        shatteredPsyche=true;
      }
    }
    assert(shatteredPsyche&&fixture.battle.playerZMoveUsed);
    assert(std::strcmp(BattleEngine::zMovePresentationName(861U),
                       "SHATTERED PSYCHE")==0);
  }

  // Dancer copies a dance immediately without spending the holder's turn.
  {
    Fixture fixture(static_cast<MoveId>(14U),false);
    fixture.battle.opponents[0].abilityId=217U;
    const BattleActionResult result=fixture.use();
    assert(fixture.battle.opponentVolatiles[0].attackStage==2);
    bool activated=false;
    for(uint8_t i=0;i<result.eventCount;++i)
      activated|=result.events[i].type==BattleEventType::AbilityActivated&&
          result.events[i].side==BattleSide::Opponent&&
          result.events[i].value==217U;
    assert(activated);
  }

  // Signature Z-Moves resolve from the compatible type crystal. Extreme
  // Evoboost bypasses Last Resort's ordinary use condition and raises all
  // five battle stats by two stages.
  {
    Fixture fixture(static_cast<MoveId>(387U),false);
    OwnedPokemon* eevee=CollectionLogic::find(
        fixture.collection,fixture.battle.playerUid);assert(eevee);
    eevee->speciesId=133U;eevee->heldItem=HeldItem::NormaliumZ;
    assert(BattleEngine::armPlayerZMove(
        fixture.battle,fixture.collection,static_cast<MoveId>(387U)));
    const BattleActionResult result=fixture.use();
    assert(result.accepted&&fixture.battle.playerZMoveUsed);
    const CombatVolatile& stages=fixture.battle.playerVolatile;
    assert(stages.attackStage==2&&stages.defenseStage==2&&
           stages.spAttackStage==2&&stages.spDefenseStage==2&&
           stages.speedStage==2);
  }

  // Guardian of Alola removes 75% of current HP and cannot knock out a
  // target from one HP.
  {
    Fixture fixture(static_cast<MoveId>(671U),false);
    OwnedPokemon* tapu=CollectionLogic::find(
        fixture.collection,fixture.battle.playerUid);assert(tapu);
    tapu->speciesId=785U;tapu->heldItem=HeldItem::FairiumZ;
    fixture.battle.opponents[0].currentHp=12000U;
    assert(BattleEngine::armPlayerZMove(
        fixture.battle,fixture.collection,static_cast<MoveId>(671U)));
    fixture.use();
    assert(fixture.battle.opponents[0].currentHp==3000U);
  }
  std::cout<<"Gen-VII move sweep passed: 54/54 data+animation contracts, "
              "52 singles moves executed on both sides; Dancer, Psycho Cut + "
              "Psychium Z, and all 35 authored Z-Move animations verified.\n";
}
