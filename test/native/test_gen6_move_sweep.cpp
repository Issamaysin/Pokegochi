#include <cassert>
#include <cstdint>
#include <iostream>

#include "game/BattleEngine.h"
#include "game/Collection.h"
#include "game/MegaEvolution.h"
#include "game/PokemonData.h"

namespace {
constexpr uint16_t kFirstGen6Move=560U;
constexpr uint16_t kLastGen6Move=617U;

struct Fixture {
  PokemonCollection collection{};
  BattleState battle{};

  Fixture(MoveId move,bool enemy){
    CollectionLogic::initialize(collection);
    assert(CollectionLogic::chooseStarter(collection,1U,0x61000001U));
    OwnedPokemon* player=CollectionLogic::active(collection,0U);assert(player);
    const uint32_t uid=player->uid;
    *player=CollectionLogic::createPokemon(uid,151U,100U,false,0x61000002U,6U);
    player->maximumHp=player->currentHp=30000U;
    player->moves[0]=enemy?static_cast<MoveId>(150U):move;
    player->movePp[0]=findFullMove(player->moves[0])->pp;

    BattleEngine::clear(battle);battle.active=true;battle.kind=BattleKind::Pvp;
    battle.outcome=BattleOutcome::Ongoing;battle.playerUid=uid;
    battle.opponentCount=1U;battle.unlockedGeneration=6U;
    battle.rngState=0x61000003U;
    battle.opponents[0]=CollectionLogic::createPokemon(
        0xF6000001U,151U,100U,false,0x61000004U,6U);
    OwnedPokemon& opponent=battle.opponents[0];
    opponent.maximumHp=opponent.currentHp=30000U;
    opponent.moves[0]=enemy?move:static_cast<MoveId>(150U);
    opponent.movePp[0]=findFullMove(opponent.moves[0])->pp;

    OwnedPokemon& user=enemy?opponent:*player;
    OwnedPokemon& target=enemy?*player:opponent;
    CombatVolatile& userState=enemy?battle.opponentVolatiles[0]:battle.playerVolatile;
    CombatVolatile& targetState=enemy?battle.playerVolatile:battle.opponentVolatiles[0];
    user.level=100U;target.level=1U;
    userState.accuracyStage=kMaximumBattleStatStage;
    targetState.evasionStage=kMinimumBattleStatStage;

    const uint16_t id=static_cast<uint16_t>(move);
    if(id==562U) // BELCH requires a Berry consumed earlier in this battle.
      (enemy?battle.opponentMoveEffects[0]:battle.playerMoveEffects).recyclableItem=
          HeldItem::OranBerry;
    if(id==563U||id==579U){ // ROTOTILLER/FLOWER SHIELD need a Grass battler.
      user.speciesId=1U;target.speciesId=1U;
      CollectionLogic::refreshDerivedStats(user,false);
      CollectionLogic::refreshDerivedStats(target,false);
    }
    if(id==599U)target.status=StatusCondition::Poison; // VENOM DRENCH
    if(id==602U)user.abilityId=57U; // PLUS enables MAGNETIC FLUX in singles.
    if(id==565U)target.currentHp=1U; // FELL STINGER's KO branch.
  }

  BattleActionResult use(bool enemy){
    return BattleEngine::fightPvp(battle,collection,0U,enemy?0U:0xFEU);
  }
  OwnedPokemon& player(){
    OwnedPokemon* value=CollectionLogic::find(collection,battle.playerUid);
    assert(value);return *value;
  }
  OwnedPokemon& target(bool enemy){return enemy?player():battle.opponents[0];}
  CombatVolatile& userVolatile(bool enemy){
    return enemy?battle.opponentVolatiles[0]:battle.playerVolatile;
  }
};

bool saw(const BattleActionResult& result,BattleEventType type,MoveId move,
         BattleSide side){
  for(uint8_t i=0;i<result.eventCount;++i){const BattleEvent& event=result.events[i];
    if(event.type==type&&event.move==move&&event.side==side)return true;}
  return false;
}
bool sawEffect(const BattleActionResult& result,MoveId move,BattleMoveEffect effect){
  for(uint8_t i=0;i<result.eventCount;++i){const BattleEvent& event=result.events[i];
    if(event.type==BattleEventType::MoveEffect&&event.move==move&&
       event.value==static_cast<uint16_t>(effect))return true;}
  return false;
}
bool unavailable(MoveId move){
  const uint16_t id=static_cast<uint16_t>(move);
  return id==597U||id==607U;
}
}

int main(){
  uint16_t audited=0U,executed=0U,excluded=0U;
  for(uint16_t raw=kFirstGen6Move;raw<=kLastGen6Move;++raw){
    const MoveId move=static_cast<MoveId>(raw);
    const FullMoveData* data=findFullMove(move);assert(data&&data->id==raw);++audited;
    if(unavailable(move)){
      assert(!isMoveAvailableInSingleBattle(move));++excluded;continue;
    }
    assert(isMoveAvailableInSingleBattle(move));
    for(bool enemy:{false,true}){
      Fixture fixture(move,enemy);
      const uint16_t hpBefore=fixture.target(enemy).currentHp;
      BattleActionResult result=fixture.use(enemy);
      const BattleSide side=enemy?BattleSide::Opponent:BattleSide::Player;
      assert(result.accepted);
      assert(saw(result,BattleEventType::MoveUsed,move,side));
      assert(!saw(result,BattleEventType::MoveMissed,move,side));
      if(sawEffect(result,move,BattleMoveEffect::Failed))
      {
        std::cerr<<"Unexpected failure "<<raw<<" "<<data->name
                 <<" side="<<(enemy?"foe":"player")<<"\n";
        for(uint8_t i=0;i<result.eventCount;++i)
          std::cerr<<" event "<<static_cast<unsigned>(i)<<" type="
                   <<static_cast<unsigned>(result.events[i].type)<<" side="
                   <<static_cast<unsigned>(result.events[i].side)<<" move="
                   <<static_cast<unsigned>(result.events[i].move)<<" value="
                   <<result.events[i].value<<"\n";
      }
      assert(!sawEffect(result,move,BattleMoveEffect::Failed));
      if(data->power){
        if(raw==566U){ // PHANTOM FORCE
          assert(fixture.userVolatile(enemy).chargingMove==move);
          result=fixture.use(enemy);
          assert(result.accepted&&!sawEffect(result,move,BattleMoveEffect::Failed));
        }
        assert(fixture.target(enemy).currentHp<hpBefore);
      }else if(raw==601U){ // GEOMANCY applies its boosts on turn two.
        assert(fixture.userVolatile(enemy).chargingMove==move);
        result=fixture.use(enemy);
        assert(result.accepted&&!sawEffect(result,move,BattleMoveEffect::Failed));
        assert(fixture.userVolatile(enemy).spAttackStage==2);
        assert(fixture.userVolatile(enemy).spDefenseStage==2);
        assert(fixture.userVolatile(enemy).speedStage==2);
      }
      ++executed;
    }
  }
  assert(audited==58U&&executed==112U&&excluded==2U);

  // X/Y's accuracy sentinel 101 means "never accuracy-check", including at
  // the worst possible Accuracy/Evasion stages.
  for(uint16_t raw=kFirstGen6Move;raw<=kLastGen6Move;++raw){
    const MoveId move=static_cast<MoveId>(raw);const FullMoveData* data=findFullMove(move);
    if(!data||data->accuracy!=101U||unavailable(move))continue;
    for(bool enemy:{false,true}){
      Fixture fixture(move,enemy);
      CombatVolatile& user=enemy?fixture.battle.opponentVolatiles[0]:fixture.battle.playerVolatile;
      CombatVolatile& target=enemy?fixture.battle.playerVolatile:fixture.battle.opponentVolatiles[0];
      user.accuracyStage=kMinimumBattleStatStage;target.evasionStage=kMaximumBattleStatStage;
      fixture.battle.rngState=1U;
      const BattleActionResult result=fixture.use(enemy);
      assert(result.accepted);
      assert(!saw(result,BattleEventType::MoveMissed,move,
                  enemy?BattleSide::Opponent:BattleSide::Player));
      assert(!sawEffect(result,move,BattleMoveEffect::Failed));
    }
  }

  // Misty Terrain prevents confusion for grounded battlers, including older
  // confusion moves routed through the shared Gen-VI engine.
  {
    Fixture fixture(static_cast<MoveId>(109U),false); // CONFUSE RAY
    fixture.battle.gen6Terrain=Gen6Terrain::Misty;
    fixture.battle.gen6TerrainTurns=5U;
    const BattleActionResult result=fixture.use(false);
    assert(result.accepted);
    assert(fixture.battle.opponentVolatiles[0].confusionTurns==0U);
  }

  // Composite Gen-VI drops must use the ordinary stat-change pipeline:
  // Flower Veil blocks both Parting Shot drops on a Grass target, while
  // Competitive reacts once for every stat Venom Drench actually lowers.
  {
    Fixture fixture(static_cast<MoveId>(575U),false); // PARTING SHOT
    fixture.battle.opponents[0].speciesId=1U;
    fixture.battle.opponents[0].abilityId=167U; // FLOWER VEIL
    const BattleActionResult result=fixture.use(false);
    assert(result.accepted);
    assert(fixture.battle.opponentVolatiles[0].attackStage==0);
    assert(fixture.battle.opponentVolatiles[0].spAttackStage==0);
  }
  {
    Fixture fixture(static_cast<MoveId>(599U),false); // VENOM DRENCH
    fixture.battle.opponents[0].abilityId=173U; // COMPETITIVE
    const BattleActionResult result=fixture.use(false);
    assert(result.accepted);
    const CombatVolatile& target=fixture.battle.opponentVolatiles[0];
    assert(target.attackStage==-1&&target.speedStage==-1&&
           target.spAttackStage==5);
  }

  // X/Y's table ends at #617, but the form catalog also exposes the four
  // ORAS signatures and Mega Zygarde's Core Enforcer/Nihil Light conversion.
  for(uint16_t raw=618U;raw<=620U;++raw){
    for(bool enemy:{false,true}){
      Fixture fixture(static_cast<MoveId>(raw),enemy);
      const uint16_t before=fixture.target(enemy).currentHp;
      const BattleActionResult result=fixture.use(enemy);
      assert(result.accepted&&!sawEffect(
          result,static_cast<MoveId>(raw),BattleMoveEffect::Failed));
      assert(fixture.target(enemy).currentHp<before);
      if(raw==620U){
        const CombatVolatile& user=fixture.userVolatile(enemy);
        assert(user.defenseStage==-1&&user.spDefenseStage==-1);
      }
    }
  }
  {
    Fixture fixture(static_cast<MoveId>(621U),false); // HYPERSPACE FURY
    fixture.player().speciesId=720U;
    fixture.player().form=1U;
    fixture.battle.opponentVolatiles[0].protectedThisTurn=true;
    const uint16_t before=fixture.battle.opponents[0].currentHp;
    const BattleActionResult result=fixture.use(false);
    assert(result.accepted&&!sawEffect(
        result,static_cast<MoveId>(621U),BattleMoveEffect::Failed));
    assert(fixture.battle.opponents[0].currentHp<before);
  }
  {
    Fixture fixture(static_cast<MoveId>(687U),false); // CORE ENFORCER
    const uint16_t before=fixture.battle.opponents[0].currentHp;
    const BattleActionResult result=fixture.use(false);
    assert(result.accepted&&!sawEffect(
        result,static_cast<MoveId>(687U),BattleMoveEffect::Failed));
    assert(fixture.battle.opponents[0].currentHp<before);
  }

  // Nihil Light is executable only by Mega Zygarde, crosses Protect and a
  // Fairy immunity, ignores target Evasion and deals identical damage through
  // positive or negative target Sp. Defense stages.
  uint16_t nihilDamage[2]{};
  for(uint8_t pass=0U;pass<2U;++pass){
    Fixture fixture(static_cast<MoveId>(920U),false);
    OwnedPokemon& zygarde=fixture.player();
    zygarde.speciesId=718U;
    zygarde.form=2U;
    zygarde.heldItem=HeldItem::MegaStone;
    assert(MegaEvolution::setVariantChoice(zygarde,MegaVariant::Mega));
    CollectionLogic::refreshAbility(zygarde,6U);
    CollectionLogic::refreshDerivedStats(zygarde,true);
    OwnedPokemon& fairy=fixture.battle.opponents[0];
    fairy.speciesId=35U;
    CombatVolatile& target=fixture.battle.opponentVolatiles[0];
    target.protectedThisTurn=true;
    target.evasionStage=kMaximumBattleStatStage;
    target.spDefenseStage=pass?kMinimumBattleStatStage:kMaximumBattleStatStage;
    fixture.battle.playerVolatile.accuracyStage=0;
    const uint16_t before=fairy.currentHp;
    const BattleActionResult result=fixture.use(false);
    assert(result.accepted&&!saw(
        result,BattleEventType::MoveMissed,static_cast<MoveId>(920U),
        BattleSide::Player));
    assert(!sawEffect(result,static_cast<MoveId>(920U),BattleMoveEffect::Failed));
    assert(fairy.currentHp<before);
    nihilDamage[pass]=static_cast<uint16_t>(before-fairy.currentHp);
  }
  assert(nihilDamage[0]==nihilDamage[1]);
  {
    Fixture fixture(static_cast<MoveId>(920U),false);
    fixture.player().speciesId=718U;
    const uint16_t before=fixture.battle.opponents[0].currentHp;
    const BattleActionResult result=fixture.use(false);
    assert(result.accepted&&sawEffect(
        result,static_cast<MoveId>(920U),BattleMoveEffect::Failed));
    assert(fixture.battle.opponents[0].currentHp==before);
  }

  std::cout<<"Gen-VI move sweep passed: 58/58 audited; 56 usable moves "
              "executed on both sides; Aromatic Mist and Hold Hands excluded.\n";
}
