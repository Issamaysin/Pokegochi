#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "game/BattleEngine.h"
#include "game/Collection.h"
#include "game/PokemonData.h"

namespace {
struct Fixture {
  PokemonCollection collection{};
  BattleState battle{};
  Fixture(MoveId playerMove,MoveId opponentMove,BattleKind kind=BattleKind::Pvp){
    CollectionLogic::initialize(collection);
    assert(CollectionLogic::chooseStarter(collection,1,0xD000U));
    OwnedPokemon* owned=CollectionLogic::active(collection,0);
    const uint32_t uid=owned->uid;
    *owned=CollectionLogic::createPokemon(uid,137,50,false,0xD001U);
    owned->moves[0]=playerMove;
    owned->movePp[0]=findFullMove(playerMove)?findFullMove(playerMove)->pp:20;
    owned->moves[1]=static_cast<MoveId>(7); // FIRE PUNCH gives Conversion a non-Normal type.
    owned->movePp[1]=15;
    BattleEngine::clear(battle);
    battle.active=true;battle.kind=kind;battle.outcome=BattleOutcome::Ongoing;
    battle.playerUid=uid;battle.opponentCount=1;battle.rngState=0xD002U;
    battle.opponents[0]=CollectionLogic::createPokemon(0xF100D001U,137,50,false,0xD003U);
    battle.opponents[0].moves[0]=opponentMove;
    battle.opponents[0].movePp[0]=findFullMove(opponentMove)?findFullMove(opponentMove)->pp:20;
    battle.opponents[0].moves[1]=static_cast<MoveId>(7);
    battle.opponents[0].movePp[1]=15;
  }
  OwnedPokemon& player(){return *CollectionLogic::find(collection,battle.playerUid);}
  OwnedPokemon& opponent(){return battle.opponents[battle.opponentIndex];}
  BattleActionResult playerUses(){return BattleEngine::fightPvp(battle,collection,0,0xFEU);}
  BattleActionResult opponentUses(){return BattleEngine::fightPvp(battle,collection,0,0);}
};

bool saw(const BattleActionResult& result,BattleMoveEffect effect,BattleSide side){
  for(uint8_t i=0;i<result.eventCount;++i)
    if(result.events[i].type==BattleEventType::MoveEffect&&result.events[i].side==side&&
       result.events[i].value==static_cast<uint16_t>(effect))return true;
  return false;
}

bool sawMove(const BattleActionResult& result,MoveId move,BattleSide side){
  for(uint8_t i=0;i<result.eventCount;++i)
    if(result.events[i].type==BattleEventType::MoveUsed&&result.events[i].side==side&&
       result.events[i].move==move)return true;
  return false;
}

uint16_t moveEventValue(const BattleActionResult& result,MoveId move,BattleSide side){
  for(uint8_t i=0;i<result.eventCount;++i)
    if(result.events[i].type==BattleEventType::MoveUsed&&result.events[i].side==side&&
       result.events[i].move==move)return result.events[i].value;
  return 0U;
}

uint8_t countEvents(const BattleActionResult& result,BattleEventType type,
                     BattleSide side){
  uint8_t count=0;
  for(uint8_t i=0;i<result.eventCount;++i)
    if(result.events[i].type==type&&result.events[i].side==side)++count;
  return count;
}

bool sawCannotMove(const BattleActionResult& result,BattleSide side,uint16_t reason){
  for(uint8_t i=0;i<result.eventCount;++i)
    if(result.events[i].type==BattleEventType::CannotMove&&
       result.events[i].side==side&&result.events[i].value==reason)return true;
  return false;
}

void symmetricFlag(MoveId move,bool DedicatedMoveEffectState::*member,
                   BattleMoveEffect event){
  Fixture player(move,static_cast<MoveId>(150));
  const BattleActionResult playerResult=player.playerUses();
  assert(playerResult.accepted&&player.battle.playerMoveEffects.*member&&
         saw(playerResult,event,BattleSide::Player));
  Fixture opponent(static_cast<MoveId>(150),move);
  const BattleActionResult opponentResult=opponent.opponentUses();
  assert(opponentResult.accepted&&opponent.battle.opponentMoveEffects[0].*member&&
         saw(opponentResult,event,BattleSide::Opponent));
}
}

int main(){
  // Every ABSORB-family move owns exactly one recovery transition. The
  // resolver used to append the explicit drain event and then repeat the
  // same before/after pair in its generic self-HP snapshot. On hardware that
  // rendered as full heal -> rewind -> heal for both battle sides.
  for (const MoveId drainMove : {static_cast<MoveId>(71),  // ABSORB
                                 static_cast<MoveId>(72),  // MEGA DRAIN
                                 static_cast<MoveId>(141), // LEECH LIFE
                                 static_cast<MoveId>(202)}) { // GIGA DRAIN
    Fixture playerDrain(drainMove, static_cast<MoveId>(150)); // SPLASH
    playerDrain.player().abilityId = 0;
    playerDrain.opponent().abilityId = 0;
    playerDrain.player().maximumHp = 400;
    playerDrain.player().currentHp = 100;
    playerDrain.opponent().maximumHp = playerDrain.opponent().currentHp = 1000;
    const uint16_t playerBefore = playerDrain.player().currentHp;
    const BattleActionResult playerResult = playerDrain.playerUses();
    assert(playerResult.accepted && playerResult.hit &&
           playerDrain.player().currentHp > playerBefore &&
           countEvents(playerResult, BattleEventType::HpChanged,
                       BattleSide::Player) == 1U);

    Fixture opponentDrain(static_cast<MoveId>(150), drainMove); // SPLASH
    opponentDrain.player().abilityId = 0;
    opponentDrain.opponent().abilityId = 0;
    opponentDrain.opponent().maximumHp = 400;
    opponentDrain.opponent().currentHp = 100;
    opponentDrain.player().maximumHp = opponentDrain.player().currentHp = 1000;
    const uint16_t opponentBefore = opponentDrain.opponent().currentHp;
    const BattleActionResult opponentResult = opponentDrain.opponentUses();
    assert(opponentResult.accepted && opponentDrain.opponent().currentHp > opponentBefore &&
           countEvents(opponentResult, BattleEventType::HpChanged,
                       BattleSide::Opponent) == 1U);
  }

  // FireRed's internal 0..12 stages map to Pokegochi's signed -6..+6.
  // Verify both battle sides, one- and two-stage changes, partial composite
  // boosts, and the no-op feedback once every affected stat reaches a cap.
  {
    const MoveId swordsDance=static_cast<MoveId>(14);
    const MoveId growl=static_cast<MoveId>(45);
    const MoveId bulkUp=static_cast<MoveId>(339);
    const MoveId splash=static_cast<MoveId>(150);
    for(bool enemy:{false,true}){
      Fixture raises(enemy?splash:swordsDance,enemy?swordsDance:splash);
      CombatVolatile& raisingState=enemy?raises.battle.opponentVolatiles[0]:
          raises.battle.playerVolatile;
      for(int8_t expected:{2,4,6}){
        const BattleActionResult result=enemy?raises.opponentUses():raises.playerUses();
        assert(result.accepted&&raisingState.attackStage==expected);
      }
      const BattleActionResult cappedRaise=enemy?raises.opponentUses():raises.playerUses();
      const BattleSide raisingSide=enemy?BattleSide::Opponent:BattleSide::Player;
      assert(cappedRaise.accepted&&raisingState.attackStage==kMaximumBattleStatStage&&
             saw(cappedRaise,BattleMoveEffect::Failed,raisingSide)&&
             countEvents(cappedRaise,BattleEventType::StatChanged,raisingSide)==0U);

      Fixture lowers(enemy?splash:growl,enemy?growl:splash);
      CombatVolatile& loweredState=enemy?lowers.battle.playerVolatile:
          lowers.battle.opponentVolatiles[0];
      for(int8_t expected=-1;expected>=kMinimumBattleStatStage;--expected){
        const BattleActionResult result=enemy?lowers.opponentUses():lowers.playerUses();
        assert(result.accepted&&loweredState.attackStage==expected);
      }
      const BattleActionResult cappedDrop=enemy?lowers.opponentUses():lowers.playerUses();
      const BattleSide loweredSide=enemy?BattleSide::Player:BattleSide::Opponent;
      assert(cappedDrop.accepted&&loweredState.attackStage==kMinimumBattleStatStage&&
             saw(cappedDrop,BattleMoveEffect::Failed,loweredSide)&&
             countEvents(cappedDrop,BattleEventType::StatChanged,loweredSide)==0U);

      Fixture composite(enemy?splash:bulkUp,enemy?bulkUp:splash);
      CombatVolatile& compositeState=enemy?composite.battle.opponentVolatiles[0]:
          composite.battle.playerVolatile;
      compositeState.attackStage=kMaximumBattleStatStage;
      compositeState.defenseStage=kMaximumBattleStatStage-1;
      const BattleActionResult partial=enemy?composite.opponentUses():composite.playerUses();
      assert(partial.accepted&&compositeState.attackStage==kMaximumBattleStatStage&&
             compositeState.defenseStage==kMaximumBattleStatStage&&
             countEvents(partial,BattleEventType::StatChanged,raisingSide)==1U&&
             !saw(partial,BattleMoveEffect::Failed,raisingSide));
      const BattleActionResult fullyCapped=enemy?composite.opponentUses():composite.playerUses();
      assert(fullyCapped.accepted&&
             saw(fullyCapped,BattleMoveEffect::Failed,raisingSide)&&
             countEvents(fullyCapped,BattleEventType::StatChanged,raisingSide)==0U);
    }

    // Seed 27 produces an accuracy roll of 30 on the first xorshift draw.
    // FireRed's -6 accuracy ratio is 33%, so GROWL must connect. Reusing the
    // ordinary-stat 25% ratio (the former bug) makes this exact case miss.
    Fixture accuracy(growl,splash);
    accuracy.battle.rngState=27U;
    accuracy.battle.playerVolatile.accuracyStage=kMinimumBattleStatStage;
    const BattleActionResult lowAccuracy=accuracy.playerUses();
    assert(lowAccuracy.accepted&&accuracy.battle.opponentVolatiles[0].attackStage==-1);
  }

  // Pinch stat berries stay equipped when their relevant stage is already
  // +6. STARF likewise searches for an eligible stat and remains held when
  // all five candidates are capped, matching FireRed's activation guards.
  {
    const MoveId splash=static_cast<MoveId>(150);
    Fixture liechi(splash,splash);
    liechi.player().heldItem=HeldItem::LiechiBerry;
    liechi.player().currentHp=liechi.player().maximumHp/4U;
    liechi.battle.playerVolatile.attackStage=kMaximumBattleStatStage;
    const BattleActionResult capped=liechi.playerUses();
    assert(capped.accepted&&liechi.player().heldItem==HeldItem::LiechiBerry&&
           !capped.heldItemActivationCount);

    liechi.battle.playerVolatile.attackStage=kMaximumBattleStatStage-1;
    const BattleActionResult activated=liechi.playerUses();
    assert(activated.accepted&&
           liechi.battle.playerVolatile.attackStage==kMaximumBattleStatStage&&
           liechi.player().heldItem==HeldItem::None&&
           activated.heldItemActivationCount==1U);

    Fixture starf(splash,splash);
    starf.player().heldItem=HeldItem::StarfBerry;
    starf.player().currentHp=starf.player().maximumHp/4U;
    CombatVolatile& stages=starf.battle.playerVolatile;
    stages.attackStage=stages.defenseStage=stages.speedStage=
        stages.spAttackStage=stages.spDefenseStage=kMaximumBattleStatStage;
    const BattleActionResult allCapped=starf.playerUses();
    assert(allCapped.accepted&&starf.player().heldItem==HeldItem::StarfBerry&&
           !allCapped.heldItemActivationCount);
  }

  // Helping Hand has priority +5 and targets the user's partner in Gen III.
  // Pokegochi battles are strictly singles, so there is no legal target: it
  // must consume the action and report failure for either battle side.
  {
    const MoveId helpingHand=static_cast<MoveId>(270);
    const MoveId followMe=static_cast<MoveId>(266);
    const FullMoveData* data=findFullMove(helpingHand);
    assert(data&&data->priority==5&&data->target==MoveTarget::User);
    assert(!isMoveAvailableInSingleBattle(helpingHand)&&
           !isMoveAvailableInSingleBattle(followMe));
    for(bool enemy:{false,true}){
      Fixture f(enemy?MoveId::Tackle:helpingHand,
                enemy?helpingHand:MoveId::Tackle);
      OwnedPokemon& user=enemy?f.opponent():f.player();
      const uint8_t ppBefore=user.movePp[0];
      const BattleActionResult used=enemy?f.opponentUses():f.playerUses();
      const BattleSide side=enemy?BattleSide::Opponent:BattleSide::Player;
      assert(used.accepted&&user.movePp[0]+1U==ppBefore&&
             sawMove(used,helpingHand,side)&&
             saw(used,BattleMoveEffect::Failed,side));
    }

    // The canonical handler remains testable, but all normal acquisition and
    // AI-selection paths exclude this doubles-only move.
    MoveId learned[4]{};
    const uint8_t learnedCount=movesForLevel(133,50,learned);
    for(uint8_t slot=0;slot<learnedCount;++slot)assert(learned[slot]!=helpingHand);
    MoveId relearnable[32]{};
    const uint8_t relearnableCount=relearnableMovesForLine(133,50,relearnable,32);
    for(uint8_t slot=0;slot<relearnableCount;++slot)
      assert(relearnable[slot]!=helpingHand);

    // Sentret's evolutionary line learns Follow Me in the imported Gen-III
    // learnset. It must be filtered from the same acquisition API.
    const uint8_t followMeCount=relearnableMovesForLine(161,100,relearnable,32);
    for(uint8_t slot=0;slot<followMeCount;++slot)
      assert(relearnable[slot]!=followMe);

    for(MoveId doublesOnly:{followMe,helpingHand}){
      for(uint8_t sample=0;sample<32;++sample){
        Fixture ai(static_cast<MoveId>(150),doublesOnly,BattleKind::Trainer);
        ai.battle.rngState=static_cast<uint32_t>(0x26600000U+
            static_cast<uint16_t>(doublesOnly)*32U+sample);
        for(uint8_t slot=0;slot<kMoveSlots;++slot){
          ai.opponent().moves[slot]=doublesOnly;
          ai.opponent().movePp[slot]=20;
        }
        ai.opponent().moves[2]=MoveId::Tackle;
        ai.opponent().movePp[2]=35;
        const BattleActionResult turn=BattleEngine::fight(ai.battle,ai.collection,0);
        assert(turn.accepted&&sawMove(turn,MoveId::Tackle,BattleSide::Opponent)&&
               !sawMove(turn,doublesOnly,BattleSide::Opponent));
      }
    }
  }

  // PSYBEAM is a damaging move with a 10% secondary confusion chance in
  // FireRed. Verify that the generated data retains that chance and that the
  // engine reports the volatile effect for either attacker without turning it
  // into an always-on effect.
  {
    const MoveId psybeam=static_cast<MoveId>(60);
    const MoveId splash=static_cast<MoveId>(150);
    const FullMoveData* data=findFullMove(psybeam);
    assert(data&&data->power==65U&&data->accuracy==100U&&
           data->effectChance==10U&&std::strcmp(data->effect,"CONFUSE_HIT")==0);
    for(bool enemy:{false,true}){
      bool sawConfusion=false;
      bool sawCleanHit=false;
      for(uint32_t seed=1;seed<=512U&&(!sawConfusion||!sawCleanHit);++seed){
        Fixture f(enemy?splash:psybeam,enemy?psybeam:splash);
        f.battle.rngState=0x50590000U+seed;
        const BattleActionResult result=enemy?f.opponentUses():f.playerUses();
        const BattleSide targetSide=enemy?BattleSide::Player:BattleSide::Opponent;
        const bool confused=saw(result,BattleMoveEffect::Confused,targetSide);
        sawConfusion|=confused;
        sawCleanHit|=!confused;
      }
      assert(sawConfusion&&sawCleanHit);
    }
  }

  // TRI ATTACK performs one 20% secondary-effect roll, then chooses equally
  // between burn, freeze and paralysis. It never applies more than one major
  // status, and the status presentation follows damage for either side.
  {
    const MoveId triAttack=static_cast<MoveId>(161);
    const MoveId splash=static_cast<MoveId>(150);
    const FullMoveData* data=findFullMove(triAttack);
    assert(data&&data->power==80U&&data->accuracy==100U&&data->effectChance==20U&&
           std::strcmp(data->effect,"TRI_ATTACK")==0);
    for(bool enemy:{false,true}){
      uint8_t statusesSeen=0;
      bool sawCleanHit=false;
      bool checkedEventOrder=false;
      for(uint32_t seed=1;seed<=2048U&&(statusesSeen!=7U||!sawCleanHit);++seed){
        Fixture f(enemy?splash:triAttack,enemy?triAttack:splash);
        f.battle.rngState=0x7A1A0000U+seed;
        const BattleActionResult result=enemy?f.opponentUses():f.playerUses();
        const OwnedPokemon& target=enemy?f.player():f.opponent();
        switch(target.status){
          case StatusCondition::Burn: statusesSeen|=1U;break;
          case StatusCondition::Frozen: statusesSeen|=2U;break;
          case StatusCondition::Paralysis: statusesSeen|=4U;break;
          case StatusCondition::None: sawCleanHit=true;break;
          default: assert(false&&"TRI ATTACK applied an invalid major status");
        }
        if(target.status!=StatusCondition::None){
          int hpEvent=-1,statusEvent=-1;
          const BattleSide targetSide=enemy?BattleSide::Player:BattleSide::Opponent;
          for(uint8_t event=0;event<result.eventCount;++event){
            if(result.events[event].side!=targetSide)continue;
            if(hpEvent<0&&result.events[event].type==BattleEventType::HpChanged)hpEvent=event;
            if(statusEvent<0&&result.events[event].type==BattleEventType::StatusApplied)statusEvent=event;
          }
          assert(hpEvent>=0&&statusEvent>hpEvent);
          checkedEventOrder=true;
        }
      }
      assert(statusesSeen==7U&&sawCleanHit&&checkedEventOrder);
    }

    // A target that already has a major status keeps it; TRI ATTACK's
    // damaging hit must not replace it or emit a bogus second status event.
    Fixture occupied(triAttack,splash);
    occupied.opponent().status=StatusCondition::Poison;
    const BattleActionResult occupiedResult=occupied.playerUses();
    assert(occupiedResult.accepted&&
           occupied.opponent().status==StatusCondition::Poison&&
           countEvents(occupiedResult,BattleEventType::StatusApplied,
                       BattleSide::Opponent)==0U);
  }

  // DISABLE follows FireRed's selection rules. It targets the last move with
  // PP, blocks normal command selection without spending a turn, makes the AI
  // choose another legal move, and falls back to Struggle when no legal move
  // remains.
  {
    const MoveId disable=static_cast<MoveId>(50),tackle=static_cast<MoveId>(33);
    const MoveId growl=static_cast<MoveId>(45),splash=static_cast<MoveId>(150);
    bool applied=false;
    for(uint32_t seed=1;seed<=128U&&!applied;++seed){
      Fixture f(disable,splash);
      f.battle.opponentVolatiles[0].lastMoveUsed=tackle;
      f.opponent().moves[0]=tackle;f.opponent().movePp[0]=35;
      f.battle.rngState=0xD15AB100U+seed;
      const BattleActionResult result=f.playerUses();
      applied=saw(result,BattleMoveEffect::Disabled,BattleSide::Opponent);
      if(applied)assert(f.battle.opponentVolatiles[0].disabledMove==tackle);
    }
    assert(applied); // DISABLE has 55% accuracy, so scan deterministic seeds.

    Fixture playerBlocked(tackle,splash);
    for(uint8_t slot=1;slot<kMoveSlots;++slot){
      playerBlocked.player().moves[slot]=MoveId::None;
      playerBlocked.player().movePp[slot]=0;
    }
    playerBlocked.battle.playerVolatile.disabledMove=tackle;
    playerBlocked.battle.playerVolatile.sureHitTurns|=0x30U;
    assert(!BattleEngine::moveIsSelectable(playerBlocked.battle,
        playerBlocked.collection,tackle,BattleSide::Player));
    const uint16_t turnBefore=playerBlocked.battle.turn;
    const uint8_t ppBefore=playerBlocked.player().movePp[0];
    const BattleActionResult rejected=playerBlocked.playerUses();
    assert(!rejected.accepted&&playerBlocked.battle.turn==turnBefore&&
           playerBlocked.player().movePp[0]==ppBefore);
    const BattleActionResult playerStruggle=BattleEngine::fightPvp(
        playerBlocked.battle,playerBlocked.collection,kMoveSlots,0xFEU);
    assert(playerStruggle.accepted&&
           sawMove(playerStruggle,MoveId::Struggle,BattleSide::Player));

    Fixture aiAlternative(splash,tackle,BattleKind::Trainer);
    aiAlternative.opponent().moves[0]=tackle;
    aiAlternative.opponent().movePp[0]=35;
    aiAlternative.opponent().moves[1]=growl;
    aiAlternative.opponent().movePp[1]=40;
    for(uint8_t slot=2;slot<kMoveSlots;++slot){
      aiAlternative.opponent().moves[slot]=MoveId::None;
      aiAlternative.opponent().movePp[slot]=0;
    }
    aiAlternative.battle.opponentVolatiles[0].disabledMove=tackle;
    aiAlternative.battle.opponentVolatiles[0].sureHitTurns|=0x30U;
    const BattleActionResult alternative=BattleEngine::fight(
        aiAlternative.battle,aiAlternative.collection,0);
    assert(alternative.accepted&&
           sawMove(alternative,growl,BattleSide::Opponent)&&
           !sawMove(alternative,tackle,BattleSide::Opponent));

    Fixture aiStruggle(splash,tackle,BattleKind::Trainer);
    for(uint8_t slot=1;slot<kMoveSlots;++slot){
      aiStruggle.opponent().moves[slot]=MoveId::None;
      aiStruggle.opponent().movePp[slot]=0;
    }
    aiStruggle.battle.opponentVolatiles[0].disabledMove=tackle;
    aiStruggle.battle.opponentVolatiles[0].sureHitTurns|=0x30U;
    const BattleActionResult enemyStruggle=BattleEngine::fight(
        aiStruggle.battle,aiStruggle.collection,0);
    assert(enemyStruggle.accepted&&
           sawMove(enemyStruggle,MoveId::Struggle,BattleSide::Opponent));
  }

  // Mud Sport and Water Sport are battler-local flags whose reduction is
  // field-wide. Both sides may establish the same effect in one turn; only a
  // repeated use by that same side fails. This is easy to get wrong by
  // treating the global damage check as the setup condition too.
  for(MoveId sport:{static_cast<MoveId>(300),static_cast<MoveId>(346)}){
    Fixture f(sport,sport);
    const BattleActionResult first=BattleEngine::fightPvp(f.battle,f.collection,0,0);
    const bool mud=sport==static_cast<MoveId>(300);
    assert(first.accepted&&
           (mud?f.battle.playerMoveEffects.mudSport:
                f.battle.playerMoveEffects.waterSport)&&
           (mud?f.battle.opponentMoveEffects[0].mudSport:
                f.battle.opponentMoveEffects[0].waterSport)&&
           !saw(first,BattleMoveEffect::Failed,BattleSide::Player)&&
           !saw(first,BattleMoveEffect::Failed,BattleSide::Opponent));
    const BattleActionResult repeated=
        BattleEngine::fightPvp(f.battle,f.collection,0,0);
    assert(repeated.accepted&&
           saw(repeated,BattleMoveEffect::Failed,BattleSide::Player)&&
           saw(repeated,BattleMoveEffect::Failed,BattleSide::Opponent));
  }

  // DIG's setup and release use different FireRed animation scripts.  The
  // resolved journal must retain that phase for both battle sides; inferring
  // it later from chargingMove is unsafe because presentation is deferred.
  for(bool enemy:{false,true}){
    const MoveId dig=static_cast<MoveId>(91),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:dig,enemy?dig:splash);
    const BattleSide side=enemy?BattleSide::Opponent:BattleSide::Player;
    const BattleActionResult setup=enemy?f.opponentUses():f.playerUses();
    assert(setup.accepted&&moveEventValue(setup,dig,side)==1U&&
           (enemy?f.battle.opponentVolatiles[0].chargingMove:
                  f.battle.playerVolatile.chargingMove)==dig);
    const BattleActionResult release=enemy?f.opponentUses():f.playerUses();
    assert(release.accepted&&moveEventValue(release,dig,side)==2U&&
           (enemy?f.battle.opponentVolatiles[0].chargingMove:
                  f.battle.playerVolatile.chargingMove)==MoveId::None);
  }

  // FURY CUTTER follows the exact Gen-III counter script: successful hits
  // progress 10/20/40/80/160, an unrelated move does not erase the chain,
  // and a no-effect result resets it. Exercise both battle sides because the
  // firmware has separate player and trainer execution paths.
  for(bool enemy:{false,true}){
    const MoveId fury=static_cast<MoveId>(210);
    const MoveId splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:fury,enemy?fury:splash);
    f.player().maximumHp=f.player().currentHp=5000;
    f.opponent().maximumHp=f.opponent().currentHp=5000;
    auto use=[&](){
      f.battle.rngState=0xFC770001U;
      return enemy?f.opponentUses():f.playerUses();
    };
    const BattleActionResult first=use();
    const uint16_t firstDamage=enemy?first.damageTaken:first.damageDealt;
    const BattleActionResult second=use();
    const uint16_t secondDamage=enemy?second.damageTaken:second.damageDealt;
    DedicatedMoveEffectState& effects=enemy?f.battle.opponentMoveEffects[0]:
        f.battle.playerMoveEffects;
    assert(first.accepted&&second.accepted&&firstDamage>0&&secondDamage>firstDamage&&
           effects.furyCutterCount==2U);

    // The original games preserve the counter when another move is used.
    if(enemy){
      f.opponent().moves[0]=splash;f.opponent().movePp[0]=40;
    }else{
      f.player().moves[0]=splash;f.player().movePp[0]=40;
    }
    use();
    assert(effects.furyCutterCount==2U);
    if(enemy){
      f.opponent().moves[0]=fury;f.opponent().movePp[0]=20;
    }else{
      f.player().moves[0]=fury;f.player().movePp[0]=20;
    }
    const BattleActionResult third=use();
    const uint16_t thirdDamage=enemy?third.damageTaken:third.damageDealt;
    assert(thirdDamage>secondDamage&&effects.furyCutterCount==3U);

    // Shedinja's Wonder Guard turns Bug damage into MOVE_RESULT_NO_EFFECT.
    OwnedPokemon& immuneTarget=enemy?f.player():f.opponent();
    const uint32_t targetUid=immuneTarget.uid;
    immuneTarget=CollectionLogic::createPokemon(targetUid,292,50,false,0xFC77U);
    immuneTarget.maximumHp=immuneTarget.currentHp=1;
    const BattleActionResult immune=use();
    assert(immune.accepted&&effects.furyCutterCount==0U);
  }

  // maxattackhalvehp clamps Belly Drum's cost to one HP before testing the
  // strict "HP > cost" condition. This only matters for tiny synthetic HP
  // totals, but keeping it exact prevents a zero-cost success.
  for(bool enemy:{false,true}){
    const MoveId drum=static_cast<MoveId>(187),splash=static_cast<MoveId>(150);
    Fixture fail(enemy?splash:drum,enemy?drum:splash);
    OwnedPokemon& failUser=enemy?fail.opponent():fail.player();
    failUser.maximumHp=failUser.currentHp=1;
    const BattleActionResult failed=enemy?fail.opponentUses():fail.playerUses();
    assert(failed.accepted&&failUser.currentHp==1U&&
           saw(failed,BattleMoveEffect::Failed,
               enemy?BattleSide::Player:BattleSide::Opponent));

    Fixture success(enemy?splash:drum,enemy?drum:splash);
    OwnedPokemon& successUser=enemy?success.opponent():success.player();
    CombatVolatile& successVolatile=enemy?success.battle.opponentVolatiles[0]:
        success.battle.playerVolatile;
    successUser.maximumHp=3;successUser.currentHp=2;
    const BattleActionResult used=enemy?success.opponentUses():success.playerUses();
    assert(used.accepted&&successUser.currentHp==1U&&successVolatile.attackStage==6);
  }

  // Move callers keep the original selection algorithms, not just their
  // candidate lists. Sleep Talk retries a random 0..3 slot; Assist scales the
  // low RNG byte into the packed party-move list.
  {
    const MoveId sleepTalk=static_cast<MoveId>(214),firePunch=static_cast<MoveId>(7);
    Fixture talk(sleepTalk,static_cast<MoveId>(150));
    talk.player().moves[1]=firePunch;talk.player().movePp[1]=15;
    talk.player().moves[2]=MoveId::Tackle;talk.player().movePp[2]=35;
    talk.player().moves[3]=MoveId::None;talk.player().movePp[3]=0;
    talk.player().status=StatusCondition::Sleep;
    talk.battle.playerVolatile.sleepTurns=2;
    talk.battle.rngState=1U; // first sampled slot is 1, not compacted index 1
    const BattleActionResult result=talk.playerUses();
    assert(result.accepted&&sawMove(result,firePunch,BattleSide::Player));
  }
  {
    const MoveId assist=static_cast<MoveId>(274),ember=static_cast<MoveId>(52),
                 waterGun=static_cast<MoveId>(55);
    Fixture call(assist,static_cast<MoveId>(150));
    OwnedPokemon partner=CollectionLogic::createPokemon(0,1,50,false,0xA55157U);
    partner.moves[0]=MoveId::Tackle;partner.movePp[0]=35;
    partner.moves[1]=ember;partner.movePp[1]=25;
    partner.moves[2]=waterGun;partner.movePp[2]=25;
    partner.moves[3]=MoveId::None;partner.movePp[3]=0;
    uint32_t partnerUid=0;
    assert(CollectionLogic::add(call.collection,partner,&partnerUid));
    assert(CollectionLogic::setPartySlot(call.collection,1,partnerUid));
    call.battle.rngState=3U; // low byte 99 => floor(99 * 3 / 256) == 1
    const BattleActionResult result=call.playerUses();
    assert(result.accepted&&sawMove(result,ember,BattleSide::Player));
  }


  // Defense Curl and Minimize set their special Gen-III bits before the
  // ordinary stat-stage command. At +6 the move fails, but the bit remains
  // armed for Rollout/Stomp interactions.
  for(bool enemy:{false,true}){
    const MoveId splash=static_cast<MoveId>(150);
    for(const MoveId setup:{static_cast<MoveId>(111),static_cast<MoveId>(107)}){
      Fixture f(enemy?splash:setup,enemy?setup:splash);
      CombatVolatile& state=enemy?f.battle.opponentVolatiles[0]:f.battle.playerVolatile;
      DedicatedMoveEffectState& effects=enemy?f.battle.opponentMoveEffects[0]:
          f.battle.playerMoveEffects;
      if(setup==static_cast<MoveId>(111))state.defenseStage=6;
      else state.evasionStage=6;
      const BattleActionResult result=enemy?f.opponentUses():f.playerUses();
      assert(result.accepted&&saw(result,BattleMoveEffect::Failed,
          enemy?BattleSide::Opponent:BattleSide::Player));
      if(setup==static_cast<MoveId>(111))assert(effects.defenseCurl);
      else assert(effects.minimized);
    }
  }

  // TRANSFORM copies the target's complete Gen-III battle-data subset on
  // both execution paths. It must reject an already transformed or
  // semi-invulnerable target instead of merely changing the sprite/moves.
  for(bool enemy:{false,true}){
    const MoveId transform=static_cast<MoveId>(144),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:transform,enemy?transform:splash);
    OwnedPokemon& user=enemy?f.opponent():f.player();
    OwnedPokemon& target=enemy?f.player():f.opponent();
    const uint32_t targetUid=target.uid;
    target=CollectionLogic::createPokemon(targetUid,213,73,false,0x7A600001U);
    target.moves[0]=splash;target.movePp[0]=40;
    CombatVolatile& userVolatile=enemy?f.battle.opponentVolatiles[0]:
        f.battle.playerVolatile;
    CombatVolatile& targetVolatile=enemy?f.battle.playerVolatile:
        f.battle.opponentVolatiles[0];
    targetVolatile.attackStage=5;targetVolatile.defenseStage=-3;
    targetVolatile.spAttackStage=2;targetVolatile.spDefenseStage=-4;
    targetVolatile.speedStage=1;targetVolatile.accuracyStage=-2;
    targetVolatile.evasionStage=3;
    userVolatile.disabledMove=static_cast<MoveId>(33);userVolatile.sureHitTurns=0x50U;
    const uint16_t copiedDefense=CollectionLogic::calculatedStat(target,PokemonStat::Defense);
    const BattleActionResult transformed=enemy?f.opponentUses():f.playerUses();
    const TransformBattleSnapshot& snapshot=enemy?f.battle.opponentTransformSnapshot:
        f.battle.playerTransformSnapshot;
    assert(transformed.accepted&&snapshot.active&&snapshot.defense==copiedDefense&&
           user.speciesId==target.speciesId&&user.moves[0]==splash&&user.movePp[0]==5U&&
           userVolatile.attackStage==5&&userVolatile.defenseStage==-3&&
           userVolatile.spAttackStage==2&&userVolatile.spDefenseStage==-4&&
           userVolatile.speedStage==1&&userVolatile.accuracyStage==-2&&
           userVolatile.evasionStage==3&&userVolatile.disabledMove==MoveId::None&&
           (userVolatile.sureHitTurns&0x70U)==0U&&
           saw(transformed,BattleMoveEffect::Transformed,
               enemy?BattleSide::Opponent:BattleSide::Player));

    Fixture already(enemy?splash:transform,enemy?transform:splash);
    (enemy?already.battle.playerMoveEffects:already.battle.opponentMoveEffects[0]).
        transformed=true;
    const BattleActionResult alreadyResult=enemy?already.opponentUses():already.playerUses();
    assert(saw(alreadyResult,BattleMoveEffect::Failed,
               enemy?BattleSide::Opponent:BattleSide::Player)&&
           !(enemy?already.battle.opponentTransformSnapshot:
                    already.battle.playerTransformSnapshot).active);

    Fixture hidden(enemy?splash:transform,enemy?transform:splash);
    CombatVolatile& hiddenUser=enemy?hidden.battle.opponentVolatiles[0]:
        hidden.battle.playerVolatile;
    CombatVolatile& hiddenTarget=enemy?hidden.battle.playerVolatile:
        hidden.battle.opponentVolatiles[0];
    OwnedPokemon& hiddenTargetPokemon=enemy?hidden.player():hidden.opponent();
    hiddenTargetPokemon.moves[2]=static_cast<MoveId>(19);
    hiddenTargetPokemon.movePp[2]=15;
    hiddenUser.speedStage=6;hiddenTarget.speedStage=-6;
    hiddenTarget.chargingMove=static_cast<MoveId>(19); // FLY
    const BattleActionResult hiddenResult=enemy?hidden.opponentUses():hidden.playerUses();
    assert(saw(hiddenResult,BattleMoveEffect::Failed,
               enemy?BattleSide::Opponent:BattleSide::Player));
  }

  // SPIT UP is not an ordinary 100/200/300-power attack in Gen III. The
  // stockpile count multiplies the complete base-damage result, the move
  // cannot crit, and it has no 85..100 random-damage roll. Protect consumes
  // the stockpile, whereas an accuracy miss does not. Check the deterministic
  // multiplier and the otherwise easy-to-miss Protect branch on both sides.
  {
    const MoveId spitUp=static_cast<MoveId>(255),splash=static_cast<MoveId>(150);
    Fixture one(spitUp,splash),two(spitUp,splash);
    one.battle.playerVolatile.stockpileCount=1;
    two.battle.playerVolatile.stockpileCount=2;
    one.battle.rngState=two.battle.rngState=0x5A170001U;
    one.opponent().maximumHp=one.opponent().currentHp=5000;
    two.opponent().maximumHp=two.opponent().currentHp=5000;
    const uint16_t oneDamage=one.playerUses().damageDealt;
    const uint16_t twoDamage=two.playerUses().damageDealt;
    assert(oneDamage>0U&&twoDamage==static_cast<uint16_t>(oneDamage*2U)&&
           !one.battle.playerVolatile.stockpileCount&&
           !two.battle.playerVolatile.stockpileCount);
  }
  for(bool enemy:{false,true}){
    const MoveId spitUp=static_cast<MoveId>(255),protect=static_cast<MoveId>(182);
    Fixture f(enemy?protect:spitUp,enemy?spitUp:protect);
    CombatVolatile& stockpiler=enemy?f.battle.opponentVolatiles[0]:
        f.battle.playerVolatile;
    stockpiler.stockpileCount=3;
    const BattleActionResult protectedSpit=BattleEngine::fightPvp(
        f.battle,f.collection,0,0);
    assert(protectedSpit.accepted&&stockpiler.stockpileCount==0U&&
           (enemy?protectedSpit.damageTaken:protectedSpit.damageDealt)==0U);
  }

  // MIMIC and SKETCH look similar in the UI but use different Gen-III copy
  // rules. Mimic rejects its own small forbidden list and grants at most
  // five PP; Sketch may copy Mimic/Metronome, grants the move's full base PP,
  // and only rejects Sketch/Struggle. Both reject an already-known move and
  // a transformed user.
  for(bool enemy:{false,true}){
    const MoveId mimic=static_cast<MoveId>(102),hydroPump=static_cast<MoveId>(56);
    const MoveId splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:mimic,enemy?mimic:splash);
    if(enemy){f.battle.opponentVolatiles[0].speedStage=6;
              f.battle.playerVolatile.speedStage=-6;}
    CombatVolatile& target=enemy?f.battle.playerVolatile:
        f.battle.opponentVolatiles[0];
    target.lastMoveUsed=hydroPump;
    const BattleActionResult copied=enemy?f.opponentUses():f.playerUses();
    OwnedPokemon& user=enemy?f.opponent():f.player();
    assert(copied.accepted&&user.moves[0]==hydroPump&&user.movePp[0]==5U&&
           saw(copied,BattleMoveEffect::MoveCopied,
               enemy?BattleSide::Opponent:BattleSide::Player));

    Fixture known(enemy?splash:mimic,enemy?mimic:splash);
    if(enemy){known.battle.opponentVolatiles[0].speedStage=6;
              known.battle.playerVolatile.speedStage=-6;}
    CombatVolatile& knownTarget=enemy?known.battle.playerVolatile:
        known.battle.opponentVolatiles[0];
    OwnedPokemon& knownUser=enemy?known.opponent():known.player();
    knownTarget.lastMoveUsed=hydroPump;
    knownUser.moves[1]=hydroPump;knownUser.movePp[1]=5;
    const BattleActionResult rejected=enemy?known.opponentUses():known.playerUses();
    assert(rejected.accepted&&knownUser.moves[0]==mimic&&
           saw(rejected,BattleMoveEffect::Failed,
               enemy?BattleSide::Opponent:BattleSide::Player));

    Fixture transformed(enemy?splash:mimic,enemy?mimic:splash);
    if(enemy){transformed.battle.opponentVolatiles[0].speedStage=6;
              transformed.battle.playerVolatile.speedStage=-6;}
    CombatVolatile& transformedTarget=enemy?transformed.battle.playerVolatile:
        transformed.battle.opponentVolatiles[0];
    transformedTarget.lastMoveUsed=hydroPump;
    DedicatedMoveEffectState& transformedState=enemy?
        transformed.battle.opponentMoveEffects[0]:transformed.battle.playerMoveEffects;
    transformedState.transformed=true;
    const BattleActionResult transformedResult=enemy?transformed.opponentUses():
        transformed.playerUses();
    assert(saw(transformedResult,BattleMoveEffect::Failed,
               enemy?BattleSide::Opponent:BattleSide::Player));
  }
  {
    const MoveId sketch=static_cast<MoveId>(166),mimic=static_cast<MoveId>(102);
    Fixture f(sketch,static_cast<MoveId>(150));
    f.battle.opponentVolatiles[0].lastMoveUsed=mimic;
    const BattleActionResult copied=f.playerUses();
    assert(copied.accepted&&f.player().moves[0]==mimic&&
           f.player().movePp[0]==findFullMove(mimic)->pp);
  }

  // SLEEP TALK uses FireRed's complete CheckMoveLimitations mask with PP as
  // the sole exception.  It therefore may call a zero-PP move, but must not
  // tunnel through Disable, Torment, Taunt, Imprison, Encore or Choice Band.
  for(bool enemy:{false,true}){
    const MoveId sleepTalk=static_cast<MoveId>(214),tackle=static_cast<MoveId>(33);
    const MoveId splash=static_cast<MoveId>(150),growl=static_cast<MoveId>(45);
    auto configure=[&](Fixture& f,MoveId candidate){
      OwnedPokemon& user=enemy?f.opponent():f.player();
      CombatVolatile& state=enemy?f.battle.opponentVolatiles[0]:f.battle.playerVolatile;
      user.moves[0]=sleepTalk;user.movePp[0]=10;
      user.moves[1]=candidate;user.movePp[1]=0; // PP is deliberately ignored.
      user.moves[2]=user.moves[3]=MoveId::None;
      user.movePp[2]=user.movePp[3]=0;
      user.status=StatusCondition::Sleep;state.sleepTurns=3;
    };
    auto use=[&](Fixture& f){return enemy?f.opponentUses():f.playerUses();};
    const BattleSide side=enemy?BattleSide::Opponent:BattleSide::Player;

    Fixture ppIgnored(enemy?splash:sleepTalk,enemy?sleepTalk:splash);
    configure(ppIgnored,tackle);
    const BattleActionResult called=use(ppIgnored);
    assert(called.accepted&&sawMove(called,tackle,side));

    Fixture disabled(enemy?splash:sleepTalk,enemy?sleepTalk:splash);
    configure(disabled,tackle);
    (enemy?disabled.battle.opponentVolatiles[0]:disabled.battle.playerVolatile).
        disabledMove=tackle;
    assert(saw(use(disabled),BattleMoveEffect::Failed,side));

    Fixture tormented(enemy?splash:sleepTalk,enemy?sleepTalk:splash);
    configure(tormented,tackle);
    CombatVolatile& tormentedVolatile=enemy?tormented.battle.opponentVolatiles[0]:
        tormented.battle.playerVolatile;
    DedicatedMoveEffectState& tormentedEffects=enemy?
        tormented.battle.opponentMoveEffects[0]:tormented.battle.playerMoveEffects;
    tormentedVolatile.lastMoveUsed=tackle;tormentedEffects.tormented=true;
    assert(saw(use(tormented),BattleMoveEffect::Failed,side));

    Fixture taunted(enemy?splash:sleepTalk,enemy?sleepTalk:splash);
    configure(taunted,growl);
    (enemy?taunted.battle.opponentMoveEffects[0]:taunted.battle.playerMoveEffects).
        tauntTurns=2;
    const BattleActionResult tauntBlocked=use(taunted);
    // Sleep Talk itself is a status move, so an ordinary selection is
    // rejected before its internal candidate mask is reached.
    assert(!sawMove(tauntBlocked,growl,side));

    Fixture imprisoned(enemy?tackle:sleepTalk,enemy?sleepTalk:tackle);
    configure(imprisoned,tackle);
    (enemy?imprisoned.battle.playerMoveEffects:
           imprisoned.battle.opponentMoveEffects[0]).imprisoned=true;
    assert(saw(use(imprisoned),BattleMoveEffect::Failed,side));

    Fixture encored(enemy?splash:sleepTalk,enemy?sleepTalk:splash);
    configure(encored,tackle);
    (enemy?encored.battle.opponentVolatiles[0]:encored.battle.playerVolatile).
        encoreMove=sleepTalk;
    assert(saw(use(encored),BattleMoveEffect::Failed,side));

    Fixture choiceLocked(enemy?splash:sleepTalk,enemy?sleepTalk:splash);
    configure(choiceLocked,growl);
    OwnedPokemon& choiceUser=enemy?choiceLocked.opponent():choiceLocked.player();
    CombatVolatile& choiceState=enemy?choiceLocked.battle.opponentVolatiles[0]:
        choiceLocked.battle.playerVolatile;
    choiceState.heldItemOverride=HeldItem::ChoiceBand;
    choiceState.hasHeldItemOverride=true;choiceState.choiceMove=tackle;
    const BattleActionResult choiceBlocked=use(choiceLocked);
    assert(!sawMove(choiceBlocked,growl,side));
  }

  // YAWN is rejected by Substitute and by an active Uproar.  When its
  // delayed sleep finally lands, FireRed also invokes CancelMultiTurnMoves;
  // the Fury Cutter chain is a compact observable of that shared reset.
  for(bool enemyTarget:{false,true}){
    const MoveId yawn=static_cast<MoveId>(281),splash=static_cast<MoveId>(150);
    Fixture substitute(enemyTarget?splash:yawn,enemyTarget?yawn:splash);
    CombatVolatile& protectedState=enemyTarget?substitute.battle.playerVolatile:
        substitute.battle.opponentVolatiles[0];
    protectedState.substituteHp=10;
    const BattleSide attackerSide=enemyTarget?BattleSide::Opponent:BattleSide::Player;
    assert(saw(enemyTarget?substitute.opponentUses():substitute.playerUses(),
               BattleMoveEffect::Failed,attackerSide));

    Fixture uproar(enemyTarget?splash:yawn,enemyTarget?yawn:splash);
    uproar.battle.playerMoveEffects.uproar=true;
    assert(saw(enemyTarget?uproar.opponentUses():uproar.playerUses(),
               BattleMoveEffect::Failed,attackerSide));

    Fixture onset(splash,splash);
    OwnedPokemon& sleeper=enemyTarget?onset.player():onset.opponent();
    CombatVolatile& sleeperVolatile=enemyTarget?onset.battle.playerVolatile:
        onset.battle.opponentVolatiles[0];
    DedicatedMoveEffectState& sleeperEffects=enemyTarget?
        onset.battle.playerMoveEffects:onset.battle.opponentMoveEffects[0];
    sleeperEffects.yawnTurns=1;sleeperEffects.furyCutterCount=4;
    const BattleActionResult elapsed=onset.playerUses();
    assert(elapsed.accepted&&sleeper.status==StatusCondition::Sleep&&
           sleeperVolatile.sleepTurns>=2U&&sleeperVolatile.sleepTurns<=5U&&
           sleeperEffects.furyCutterCount==0U);

    // UproarWakeUpCheck deliberately exempts a Soundproof target. It may be
    // made drowsy and must still fall asleep while the noise remains active.
    Fixture soundproof(enemyTarget?splash:yawn,enemyTarget?yawn:splash);
    OwnedPokemon& soundproofTarget=enemyTarget?soundproof.player():soundproof.opponent();
    soundproofTarget.abilityId=43; // SOUNDPROOF
    soundproof.battle.playerMoveEffects.uproar=true;
    const BattleActionResult drowsy=enemyTarget?soundproof.opponentUses():
        soundproof.playerUses();
    DedicatedMoveEffectState& drowsyState=enemyTarget?
        soundproof.battle.playerMoveEffects:soundproof.battle.opponentMoveEffects[0];
    assert(drowsy.accepted&&!saw(drowsy,BattleMoveEffect::Failed,attackerSide)&&
           drowsyState.yawnTurns==1U);
    if(enemyTarget){soundproof.opponent().moves[0]=splash;soundproof.opponent().movePp[0]=40;}
    else{soundproof.player().moves[0]=splash;soundproof.player().movePp[0]=40;}
    (void)(enemyTarget?soundproof.opponentUses():soundproof.playerUses());
    assert(soundproofTarget.status==StatusCondition::Sleep);
  }

  // Direct Sleep and Freeze invoke CancelMultiTurnMoves at application time,
  // rather than waiting for the affected battler to attempt its next action.
  // Soundproof also permits direct sleep through an active Uproar.
  for(bool targetIsPlayer:{false,true}){
    const MoveId hypnosis=static_cast<MoveId>(95),splash=static_cast<MoveId>(150);
    Fixture interrupted(splash,splash);
    OwnedPokemon& source=targetIsPlayer?interrupted.opponent():interrupted.player();
    OwnedPokemon& target=targetIsPlayer?interrupted.player():interrupted.opponent();
    CombatVolatile& targetVolatile=targetIsPlayer?interrupted.battle.playerVolatile:
        interrupted.battle.opponentVolatiles[0];
    DedicatedMoveEffectState& targetEffects=targetIsPlayer?
        interrupted.battle.playerMoveEffects:interrupted.battle.opponentMoveEffects[0];
    BideState& targetBide=targetIsPlayer?interrupted.battle.playerBide:
        interrupted.battle.opponentBides[0];
    targetEffects.furyCutterCount=4;targetEffects.rolloutCount=3;
    targetEffects.lockedMove=static_cast<MoveId>(205);targetEffects.lockedMoveTurns=3;
    targetVolatile.chargingMove=static_cast<MoveId>(76);targetBide.turns=2;
    BattleActionResult applied{};
    BattleEngine::applyMoveStatus(interrupted.battle,source,hypnosis,target,applied,false);
    assert(target.status==StatusCondition::Sleep&&targetEffects.furyCutterCount==0U&&
           targetEffects.rolloutCount==0U&&targetEffects.lockedMoveTurns==0U&&
           targetVolatile.chargingMove==MoveId::None&&targetBide.turns==0U);

    Fixture soundproof(splash,splash);
    OwnedPokemon& soundSource=targetIsPlayer?soundproof.opponent():soundproof.player();
    OwnedPokemon& soundTarget=targetIsPlayer?soundproof.player():soundproof.opponent();
    soundTarget.abilityId=43;soundproof.battle.playerMoveEffects.uproar=true;
    BattleActionResult soundApplied{};
    BattleEngine::applyMoveStatus(soundproof.battle,soundSource,hypnosis,
                                  soundTarget,soundApplied,false);
    assert(soundTarget.status==StatusCondition::Sleep);
  }

  // SetMoveEffect rejects Freeze under effective sunlight. Cloud Nine and
  // Air Lock suppress that weather rule; ordinary clear weather remains able
  // to freeze with the same deterministic RNG stream.
  {
    const MoveId iceBeam=static_cast<MoveId>(58),splash=static_cast<MoveId>(150);
    Fixture clear(splash,splash),sunny(splash,splash);
    clear.player().abilityId=clear.opponent().abilityId=0;
    sunny.player().abilityId=sunny.opponent().abilityId=0;
    clear.battle.rngState=sunny.battle.rngState=0xFEEE2003U;
    sunny.battle.weather=BattleWeather::Sun;
    for(uint16_t attempt=0;attempt<1000U&&clear.opponent().status==StatusCondition::None;
        ++attempt){
      BattleActionResult clearResult{},sunnyResult{};
      BattleEngine::applyMoveStatus(clear.battle,clear.player(),iceBeam,
                                    clear.opponent(),clearResult,false);
      BattleEngine::applyMoveStatus(sunny.battle,sunny.player(),iceBeam,
                                    sunny.opponent(),sunnyResult,false);
    }
    assert(clear.opponent().status==StatusCondition::Frozen&&
           sunny.opponent().status==StatusCondition::None);
  }

  // MOVE_EFFECT_AFFECTS_USER is significant for hit+boost moves. Shield Dust
  // on the defender blocks effects aimed at the defender, not Metal Claw,
  // Steel Wing, AncientPower or Silver Wind boosting their own attacker.
  for(bool enemy:{false,true}){
    const MoveId ancientPower=static_cast<MoveId>(246),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:ancientPower,enemy?ancientPower:splash);
    OwnedPokemon& target=enemy?f.player():f.opponent();
    target.abilityId=19; // SHIELD DUST
    target.maximumHp=target.currentHp=60000;
    CombatVolatile& attacker=enemy?f.battle.opponentVolatiles[0]:
        f.battle.playerVolatile;
    for(uint8_t attempt=0;attempt<40U&&!attacker.attackStage;++attempt){
      OwnedPokemon& user=enemy?f.opponent():f.player();
      user.movePp[0]=5;
      (void)(enemy?f.opponentUses():f.playerUses());
    }
    assert(attacker.attackStage>0&&attacker.defenseStage>0&&
           attacker.spAttackStage>0&&attacker.spDefenseStage>0&&
           attacker.speedStage>0);
  }

  // The retail multi-hit script recalculates and applies every hit instead
  // of multiplying a single damage roll. This matters visibly (one HP event
  // per strike) and mechanically: a hit can break Substitute without spill,
  // then the following hit can damage the Pokemon underneath it.
  for(bool enemy:{false,true}){
    const MoveId doubleKick=static_cast<MoveId>(24),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:doubleKick,enemy?doubleKick:splash);
    OwnedPokemon& target=enemy?f.player():f.opponent();
    target.maximumHp=target.currentHp=5000;
    const BattleSide targetSide=enemy?BattleSide::Player:BattleSide::Opponent;
    const BattleSide attackerSide=enemy?BattleSide::Opponent:BattleSide::Player;
    const BattleActionResult result=enemy?f.opponentUses():f.playerUses();
    assert(result.accepted&&countEvents(result,BattleEventType::HpChanged,targetSide)==2U&&
           countEvents(result,BattleEventType::MultiHitStrike,attackerSide)==1U&&
           countEvents(result,BattleEventType::MultiHitCount,attackerSide)==1U);
    bool reportedTwo=false;
    for(uint8_t event=0;event<result.eventCount;++event)
      if(result.events[event].type==BattleEventType::MultiHitCount&&
         result.events[event].side==attackerSide)
        reportedTwo=result.events[event].value==2U;
    assert(reportedTwo);

    Fixture substitute(enemy?splash:doubleKick,enemy?doubleKick:splash);
    OwnedPokemon& substituteTarget=enemy?substitute.player():substitute.opponent();
    substituteTarget.maximumHp=substituteTarget.currentHp=5000;
    CombatVolatile& targetVolatile=enemy?substitute.battle.playerVolatile:
        substitute.battle.opponentVolatiles[0];
    targetVolatile.substituteHp=1;
    const uint16_t before=substituteTarget.currentHp;
    const BattleActionResult through=enemy?substitute.opponentUses():substitute.playerUses();
    assert(through.accepted&&targetVolatile.substituteHp==0U&&
           substituteTarget.currentHp<before&&
           countEvents(through,BattleEventType::HpChanged,targetSide)==1U);
  }

  // Endure ends the retail multi-hit loop on the lethal strike. Processing
  // the remaining hits as zero damage produces the same final HP but the
  // wrong animation/count, so assert the journal as well as the state.
  for(bool enemy:{false,true}){
    const MoveId doubleKick=static_cast<MoveId>(24),endure=static_cast<MoveId>(203);
    Fixture f(enemy?endure:doubleKick,enemy?doubleKick:endure);
    OwnedPokemon& target=enemy?f.player():f.opponent();
    target.maximumHp=100;target.currentHp=1;
    const BattleActionResult result=BattleEngine::fightPvp(f.battle,f.collection,0,0);
    const BattleSide attackerSide=enemy?BattleSide::Opponent:BattleSide::Player;
    assert(result.accepted&&target.currentHp==1U&&
           countEvents(result,BattleEventType::MultiHitStrike,attackerSide)==0U);
    bool reportedOne=false;
    for(uint8_t event=0;event<result.eventCount;++event)
      if(result.events[event].type==BattleEventType::MultiHitCount&&
         result.events[event].side==attackerSide)
        reportedOne=result.events[event].value==1U;
    assert(reportedOne);
  }

  // Rage is evaluated by moveend after every strike, not once after the
  // aggregate damage. Both fixed hits must independently raise Attack.
  for(bool enemy:{false,true}){
    const MoveId doubleKick=static_cast<MoveId>(24),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:doubleKick,enemy?doubleKick:splash);
    (enemy?f.opponent():f.player()).level=100;
    OwnedPokemon& target=enemy?f.player():f.opponent();
    target.maximumHp=target.currentHp=5000;
    DedicatedMoveEffectState& targetEffects=enemy?f.battle.playerMoveEffects:
        f.battle.opponentMoveEffects[0];
    CombatVolatile& targetVolatile=enemy?f.battle.playerVolatile:
        f.battle.opponentVolatiles[0];
    targetEffects.rageActive=true;
    const BattleActionResult result=enemy?f.opponentUses():f.playerUses();
    const BattleSide targetSide=enemy?BattleSide::Player:BattleSide::Opponent;
    assert(result.accepted&&targetVolatile.attackStage==2&&
           countEvents(result,BattleEventType::StatChanged,targetSide)==2U);

    targetVolatile.attackStage=kMaximumBattleStatStage;
    const BattleActionResult capped=enemy?f.opponentUses():f.playerUses();
    assert(capped.accepted&&targetVolatile.attackStage==kMaximumBattleStatStage&&
           countEvents(capped,BattleEventType::StatChanged,targetSide)==0U);
  }

  // Skill Link fixes the count at five. Color Change is deliberately used
  // here to prove damage is recalculated between hits: after the first Grass
  // strike changes the Normal target into Grass, all later Grass strikes are
  // resisted. A precomputed batch incorrectly gives all five full damage.
  for(bool enemy:{false,true}){
    const MoveId bulletSeed=static_cast<MoveId>(331),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:bulletSeed,enemy?bulletSeed:splash);
    OwnedPokemon& attacker=enemy?f.opponent():f.player();
    OwnedPokemon& target=enemy?f.player():f.opponent();
    attacker.abilityId=138; // custom SKILL LINK
    target.abilityId=16;    // COLOR CHANGE
    target.maximumHp=target.currentHp=60000;
    const BattleActionResult result=enemy?f.opponentUses():f.playerUses();
    const BattleSide targetSide=enemy?BattleSide::Player:BattleSide::Opponent;
    const BattleSide attackerSide=enemy?BattleSide::Opponent:BattleSide::Player;
    uint16_t hits[5]{};uint8_t hitCount=0;
    for(uint8_t event=0;event<result.eventCount;++event)
      if(result.events[event].type==BattleEventType::HpChanged&&
         result.events[event].side==targetSide&&hitCount<5U)
        hits[hitCount++]=result.events[event].value;
    assert(result.accepted&&hitCount==5U&&
           countEvents(result,BattleEventType::MultiHitStrike,attackerSide)==4U&&
           hits[0]>hits[1]&&hits[0]>hits[2]&&hits[0]>hits[3]&&hits[0]>hits[4]);
  }

  // An Effect Spore sleep is processed by moveend between strikes. The next
  // loop iteration sees the sleeping attacker and terminates immediately;
  // precomputing all five Skill Link hits used to ignore that interruption.
  for(bool enemy:{false,true}){
    const MoveId doubleSlap=static_cast<MoveId>(3),splash=static_cast<MoveId>(150);
    bool foundInterruptedSleep=false;
    for(uint32_t seed=1U;seed<2000U&&!foundInterruptedSleep;++seed){
      Fixture f(enemy?splash:doubleSlap,enemy?doubleSlap:splash);
      OwnedPokemon& attacker=enemy?f.opponent():f.player();
      OwnedPokemon& target=enemy?f.player():f.opponent();
      attacker.abilityId=138; // custom SKILL LINK
      attacker.level=100;
      target.abilityId=27;    // EFFECT SPORE
      target.maximumHp=target.currentHp=60000;
      f.battle.rngState=seed;
      const BattleActionResult result=enemy?f.opponentUses():f.playerUses();
      if(attacker.status!=StatusCondition::Sleep)continue;
      const BattleSide attackerSide=enemy?BattleSide::Opponent:BattleSide::Player;
      for(uint8_t event=0;event<result.eventCount;++event){
        const BattleEvent& count=result.events[event];
        if(count.type!=BattleEventType::MultiHitCount||count.side!=attackerSide)continue;
        if(count.value<5U){
          assert(count.value>=1U&&
                 countEvents(result,BattleEventType::MultiHitStrike,attackerSide)==
                     static_cast<uint8_t>(count.value-1U));
          foundInterruptedSleep=true;
        }
      }
    }
    assert(foundInterruptedSleep);
  }

  // Triple Kick has a fresh accuracy roll and escalating 10/20/30 power.
  // No Guard removes the random misses so all three independently calculated
  // damage steps can be asserted symmetrically.
  for(bool enemy:{false,true}){
    const MoveId tripleKick=static_cast<MoveId>(167),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:tripleKick,enemy?tripleKick:splash);
    OwnedPokemon& attacker=enemy?f.opponent():f.player();
    OwnedPokemon& target=enemy?f.player():f.opponent();
    attacker.abilityId=131; // custom NO GUARD
    target.maximumHp=target.currentHp=60000;
    const BattleActionResult result=enemy?f.opponentUses():f.playerUses();
    const BattleSide targetSide=enemy?BattleSide::Player:BattleSide::Opponent;
    uint16_t hits[3]{};uint8_t hitCount=0;
    for(uint8_t event=0;event<result.eventCount;++event)
      if(result.events[event].type==BattleEventType::HpChanged&&
         result.events[event].side==targetSide&&hitCount<3U)
        hits[hitCount++]=result.events[event].value;
    assert(result.accepted&&hitCount==3U&&hits[0]<hits[1]&&hits[1]<hits[2]);
  }

  // The sequential path must still preserve Present's 52/256 healing roll,
  // which is chosen inside damage calculation rather than before the loop.
  for(bool enemy:{false,true}){
    const MoveId present=static_cast<MoveId>(217),splash=static_cast<MoveId>(150);
    bool observedHeal=false;
    for(uint32_t seed=1U;seed<1000U&&!observedHeal;++seed){
      Fixture f(enemy?splash:present,enemy?present:splash);
      (enemy?f.opponent():f.player()).level=100;
      OwnedPokemon& target=enemy?f.player():f.opponent();
      target.maximumHp=200;target.currentHp=100;
      f.battle.rngState=seed;
      const BattleActionResult result=enemy?f.opponentUses():f.playerUses();
      if(target.currentHp>100U){
        const BattleSide targetSide=enemy?BattleSide::Player:BattleSide::Opponent;
        assert(result.accepted&&target.currentHp==150U&&
               countEvents(result,BattleEventType::HpChanged,targetSide)==1U);
        observedHeal=true;
      }
    }
    assert(observedHeal);
  }

  // Thunder ignores accuracy/evasion in rain, matching AccuracyCalcHelper in
  // both decompilations.
  for(bool enemy:{false,true}){
    const MoveId thunder=static_cast<MoveId>(87),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:thunder,enemy?thunder:splash);
    f.battle.weather=BattleWeather::Rain;
    if(enemy){
      f.battle.opponentVolatiles[0].accuracyStage=-6;
      f.battle.playerVolatile.evasionStage=6;
    }else{
      f.battle.playerVolatile.accuracyStage=-6;
      f.battle.opponentVolatiles[0].evasionStage=6;
    }
    const BattleActionResult result=enemy?f.opponentUses():f.playerUses();
    assert(result.accepted&&(enemy?result.damageTaken:result.damageDealt)>0U);
  }

  // FORECAST is not merely a visual label: FireRed's
  // CastformDataTypeChange changes the battle sprite and both type slots.
  // Exercise every weather form from both execution sides and keep the form
  // event after the weather announcement in the presentation journal.
  {
    struct WeatherFormCase { MoveId move; BattleWeather weather; uint8_t form; PokemonType type; };
    const WeatherFormCase cases[] = {
      {static_cast<MoveId>(241),BattleWeather::Sun,1,PokemonType::Fire},
      {static_cast<MoveId>(240),BattleWeather::Rain,2,PokemonType::Water},
      {static_cast<MoveId>(258),BattleWeather::Hail,3,PokemonType::Ice},
    };
    for(const WeatherFormCase& weatherCase:cases)for(bool enemy:{false,true}){
      const MoveId splash=static_cast<MoveId>(150);
      Fixture f(enemy?splash:weatherCase.move,enemy?weatherCase.move:splash);
      OwnedPokemon& castform=enemy?f.opponent():f.player();
      const uint32_t uid=castform.uid;
      castform=CollectionLogic::createPokemon(uid,351,50,false,0xCA570001U);
      castform.abilityId=59; // FORECAST
      castform.moves[0]=weatherCase.move;
      castform.movePp[0]=findFullMove(weatherCase.move)->pp;
      const BattleActionResult result=enemy?f.opponentUses():f.playerUses();
      const DedicatedMoveEffectState& state=enemy?f.battle.opponentMoveEffects[0]:
          f.battle.playerMoveEffects;
      assert(result.accepted&&f.battle.weather==weatherCase.weather&&
             castform.form==weatherCase.form&&state.typeOverrideActive&&
             state.type1==weatherCase.type&&state.type2==weatherCase.type&&
             saw(result,BattleMoveEffect::FormChanged,
                 enemy?BattleSide::Opponent:BattleSide::Player));
      int weatherEvent=-1,formEvent=-1;
      for(uint8_t index=0;index<result.eventCount;++index){
        const BattleEvent& event=result.events[index];
        if(event.type!=BattleEventType::MoveEffect)continue;
        if(event.value==static_cast<uint16_t>(BattleMoveEffect::WeatherSet))weatherEvent=index;
        if(event.value==static_cast<uint16_t>(BattleMoveEffect::FormChanged)&&
           event.pokemonUid==castform.uid)formEvent=index;
      }
      assert(weatherEvent>=0&&formEvent>weatherEvent);
    }
  }

  // Sandstorm has no Castform form in Generation III. Cloud Nine and Air
  // Lock suppress Forecast even while sunlight remains stored on the field;
  // removing the suppressor immediately exposes SUNNY again.
  {
    const MoveId splash=static_cast<MoveId>(150);
    Fixture f(splash,splash);
    const uint32_t uid=f.player().uid;
    f.player()=CollectionLogic::createPokemon(uid,351,50,false,0xCA570002U);
    f.player().abilityId=59;
    f.opponent().abilityId=13; // CLOUD NINE
    f.battle.weather=BattleWeather::Sun;
    BattleEngine::applyEntryAbilities(f.battle,f.collection);
    assert(f.player().form==0&&!f.battle.playerMoveEffects.typeOverrideActive);
    f.opponent().abilityId=0;
    BattleEngine::applyEntryAbilities(f.battle,f.collection);
    assert(f.player().form==1&&f.battle.playerMoveEffects.type1==PokemonType::Fire);
    f.opponent().abilityId=77; // AIR LOCK
    BattleEngine::applyEntryAbilities(f.battle,f.collection);
    assert(f.player().form==0&&f.battle.playerMoveEffects.typeOverrideActive&&
           f.battle.playerMoveEffects.type1==PokemonType::Normal);

    f.opponent().abilityId=0;f.battle.weather=BattleWeather::Sandstorm;
    BattleEngine::applyEntryAbilities(f.battle,f.collection);
    assert(f.player().form==0&&f.battle.playerMoveEffects.type1==PokemonType::Normal);
  }

  // A five-turn weather expiry runs Forecast again, and Weather Ball receives
  // the doubled weather power plus STAB only from Castform's changed type.
  {
    const MoveId weatherBall=static_cast<MoveId>(311),splash=static_cast<MoveId>(150);
    Fixture expiry(splash,splash);
    const uint32_t expiryUid=expiry.player().uid;
    expiry.player()=CollectionLogic::createPokemon(expiryUid,351,50,false,0xCA570003U);
    expiry.player().abilityId=59;expiry.player().moves[0]=splash;expiry.player().movePp[0]=40;
    expiry.battle.weather=BattleWeather::Sun;expiry.battle.weatherTurns=1;
    BattleEngine::applyEntryAbilities(expiry.battle,expiry.collection);
    assert(expiry.player().form==1);
    const BattleActionResult expired=expiry.playerUses();
    assert(expired.accepted&&expiry.battle.weather==BattleWeather::Clear&&
           expiry.player().form==0&&
           saw(expired,BattleMoveEffect::FormChanged,BattleSide::Player));

    Fixture forecast(weatherBall,splash),plain(weatherBall,splash);
    for(Fixture* fixture:{&forecast,&plain}){
      const uint32_t castformUid=fixture->player().uid;
      fixture->player()=CollectionLogic::createPokemon(
          castformUid,351,50,false,0xCA570004U);
      fixture->player().moves[0]=weatherBall;
      fixture->player().movePp[0]=10;
      fixture->battle.weather=BattleWeather::Rain;
      fixture->battle.rngState=0xCA570005U;
      fixture->opponent().maximumHp=fixture->opponent().currentHp=5000;
    }
    forecast.player().abilityId=59;
    plain.player().abilityId=0;
    BattleEngine::applyEntryAbilities(forecast.battle,forecast.collection);
    BattleEngine::applyEntryAbilities(plain.battle,plain.collection);
    const uint16_t forecastDamage=forecast.playerUses().damageDealt;
    const uint16_t plainDamage=plain.playerUses().damageDealt;
    assert(forecast.player().form==2&&forecastDamage>plainDamage&&
           forecastDamage*100U>=plainDamage*145U&&
           forecastDamage*100U<=plainDamage*155U);
  }

  // Primary paralysis uniquely runs typecalc in the Gen-III script. Thunder
  // Wave cannot affect Ground and Glare cannot affect an unidentified Ghost;
  // Foresight removes the latter immunity. Verify both execution paths.
  for(bool enemy:{false,true}){
    const MoveId thunderWave=static_cast<MoveId>(86),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:thunderWave,enemy?thunderWave:splash);
    OwnedPokemon& target=enemy?f.player():f.opponent();
    const uint32_t targetUid=target.uid;
    target=CollectionLogic::createPokemon(targetUid,74,50,false,0x8600U); // GEODUDE
    if(enemy){target.moves[0]=splash;target.movePp[0]=40;}
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    assert(r.accepted&&target.status==StatusCondition::None);
  }
  for(bool identified:{false,true}){
    const MoveId glare=static_cast<MoveId>(137),splash=static_cast<MoveId>(150);
    Fixture f(glare,splash);
    const uint32_t targetUid=f.opponent().uid;
    f.opponent()=CollectionLogic::createPokemon(targetUid,92,50,false,0x1370U); // GASTLY
    f.battle.opponentMoveEffects[0].identified=identified;
    f.battle.playerVolatile.accuracyStage=6;
    const BattleActionResult r=f.playerUses();
    assert(r.accepted&&(identified?
        f.opponent().status==StatusCondition::Paralysis:
        f.opponent().status==StatusCondition::None));
  }

  // OHKO uses tryKO's own level-adjusted roll, not the common accuracy
  // stages/items/abilities. Extreme stages and BrightPowder therefore cannot
  // change a deterministic successful roll; Sturdy still blocks it.
  {
    const MoveId fissure=static_cast<MoveId>(90),splash=static_cast<MoveId>(150);
    Fixture f(fissure,splash);
    f.battle.playerVolatile.speedStage=6;
    f.battle.opponentVolatiles[0].speedStage=-6;
    f.battle.playerVolatile.accuracyStage=-6;
    f.battle.opponentVolatiles[0].evasionStage=6;
    f.opponent().heldItem=HeldItem::BrightPowder;
    f.battle.rngState=3U; // first xorshift roll is 8/100 (< 30).
    const BattleActionResult r=f.playerUses();
    assert(r.accepted&&r.hit&&!f.opponent().currentHp);

    Fixture sturdy(fissure,splash);
    sturdy.battle.playerVolatile.speedStage=6;
    sturdy.battle.opponentVolatiles[0].speedStage=-6;
    sturdy.opponent().abilityId=5; // STURDY
    sturdy.battle.rngState=3U;
    const uint16_t before=sturdy.opponent().currentHp;
    const BattleActionResult blocked=sturdy.playerUses();
    assert(blocked.accepted&&sturdy.opponent().currentHp==before);
  }

  // Confusion decrements before the coin flip. On the final turn the user
  // snaps out and acts; otherwise self-damage uses the typeless 40-power
  // physical formula rather than a flat fraction of maximum HP.
  for(bool enemy:{false,true}){
    const MoveId tackle=MoveId::Tackle,splash=static_cast<MoveId>(150);
    Fixture finalTurn(enemy?splash:tackle,enemy?tackle:splash);
    if(enemy){finalTurn.battle.opponentVolatiles[0].speedStage=6;
              finalTurn.battle.playerVolatile.speedStage=-6;
              finalTurn.battle.opponentVolatiles[0].confusionTurns=1;}
    else{finalTurn.battle.playerVolatile.speedStage=6;
         finalTurn.battle.opponentVolatiles[0].speedStage=-6;
         finalTurn.battle.playerVolatile.confusionTurns=1;}
    const BattleActionResult acted=enemy?finalTurn.opponentUses():finalTurn.playerUses();
    assert(acted.accepted&&sawMove(acted,tackle,enemy?BattleSide::Opponent:BattleSide::Player));

    Fixture selfHit(enemy?splash:tackle,enemy?tackle:splash);
    OwnedPokemon& user=enemy?selfHit.opponent():selfHit.player();
    CombatVolatile& volatileState=enemy?selfHit.battle.opponentVolatiles[0]:
        selfHit.battle.playerVolatile;
    if(enemy){selfHit.battle.opponentVolatiles[0].speedStage=6;
              selfHit.battle.playerVolatile.speedStage=-6;}
    else{selfHit.battle.playerVolatile.speedStage=6;
         selfHit.battle.opponentVolatiles[0].speedStage=-6;}
    volatileState.confusionTurns=3;
    selfHit.battle.rngState=2U; // even first roll: hurts itself.
    const uint16_t before=user.currentHp;
    const BattleActionResult hurt=enemy?selfHit.opponentUses():selfHit.playerUses();
    assert(hurt.accepted&&user.currentHp<before&&volatileState.confusionTurns==2U&&
           !sawMove(hurt,tackle,enemy?BattleSide::Opponent:BattleSide::Player)&&
           countEvents(hurt,BattleEventType::HpChanged,
                       enemy?BattleSide::Opponent:BattleSide::Player)==1U);
    int16_t blockedIndex=-1,hpIndex=-1;
    for(uint8_t event=0;event<hurt.eventCount;++event){
      const BattleEvent& entry=hurt.events[event];
      if(entry.side!=(enemy?BattleSide::Opponent:BattleSide::Player))continue;
      if(blockedIndex<0&&entry.type==BattleEventType::CannotMove&&entry.value==3U)
        blockedIndex=event;
      if(hpIndex<0&&entry.type==BattleEventType::HpChanged){
        hpIndex=event;assert(entry.before==before&&entry.after==user.currentHp);
      }
    }
    assert(blockedIndex>=0&&hpIndex>blockedIndex);

    // A lethal self-hit follows the same text -> HP -> faint ordering.  The
    // other battler must not execute its queued command against a Pokemon
    // that has already knocked itself out.
    Fixture lethal(enemy?splash:tackle,enemy?tackle:splash);
    OwnedPokemon& lethalUser=enemy?lethal.opponent():lethal.player();
    CombatVolatile& lethalVolatile=enemy?lethal.battle.opponentVolatiles[0]:
        lethal.battle.playerVolatile;
    if(enemy){lethal.battle.opponentVolatiles[0].speedStage=6;
              lethal.battle.playerVolatile.speedStage=-6;}
    else{lethal.battle.playerVolatile.speedStage=6;
         lethal.battle.opponentVolatiles[0].speedStage=-6;}
    lethalUser.currentHp=1U;lethalVolatile.confusionTurns=3U;
    lethal.battle.rngState=2U;
    const BattleActionResult knockedOut=enemy?lethal.opponentUses():lethal.playerUses();
    const BattleSide userSide=enemy?BattleSide::Opponent:BattleSide::Player;
    int16_t lethalBlocked=-1,lethalHp=-1,lethalFaint=-1;
    for(uint8_t event=0;event<knockedOut.eventCount;++event){
      const BattleEvent& entry=knockedOut.events[event];
      if(entry.side!=userSide)continue;
      if(lethalBlocked<0&&entry.type==BattleEventType::CannotMove&&entry.value==3U)
        lethalBlocked=event;
      else if(lethalHp<0&&entry.type==BattleEventType::HpChanged){
        lethalHp=event;assert(entry.before==1U&&entry.after==0U);
      }else if(lethalFaint<0&&entry.type==BattleEventType::Fainted)
        lethalFaint=event;
    }
    assert(knockedOut.accepted&&!lethalUser.currentHp&&lethalBlocked>=0&&
           lethalHp>lethalBlocked&&lethalFaint>lethalHp&&
           countEvents(knockedOut,BattleEventType::Fainted,userSide)==1U);
    if(enemy)assert(!sawMove(knockedOut,splash,BattleSide::Player));
  }

  // Solar Beam loses half its damage in rain/sand/hail. Use the same RNG so
  // only the weather modifier can change the result.
  {
    Fixture clearFixture(static_cast<MoveId>(76),static_cast<MoveId>(150));
    Fixture rainFixture(static_cast<MoveId>(76),static_cast<MoveId>(150));
    const FullMoveData* solar=findFullMove(static_cast<MoveId>(76));
    assert(solar&&std::strcmp(solar->effect,"SOLAR_BEAM")==0);
    clearFixture.battle.weather=BattleWeather::Clear;
    rainFixture.battle.weather=BattleWeather::Rain;
    clearFixture.battle.playerVolatile.chargingMove=static_cast<MoveId>(76);
    rainFixture.battle.playerVolatile.chargingMove=static_cast<MoveId>(76);
    clearFixture.battle.rngState=rainFixture.battle.rngState=0x50A2U;
    const uint16_t clear=clearFixture.playerUses().damageDealt;
    const uint16_t rain=rainFixture.playerUses().damageDealt;
    assert(clear>rain&&rain*2U<=clear&&clear<=rain*2U+2U);
  }

  // TELEPORT: legal from either side only in a wild encounter.
  for(bool enemy:{false,true}){
    Fixture f(enemy?static_cast<MoveId>(150):static_cast<MoveId>(100),
              enemy?static_cast<MoveId>(100):static_cast<MoveId>(150),BattleKind::Wild);
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    assert(r.accepted&&!f.battle.active&&f.battle.outcome==BattleOutcome::Escaped&&
           saw(r,BattleMoveEffect::Teleported,enemy?BattleSide::Opponent:BattleSide::Player));
  }

  // CONVERSION and CONVERSION 2: the same type-override state is used by both sides.
  for(bool enemy:{false,true}){
    const MoveId move=static_cast<MoveId>(160);
    Fixture f(enemy?static_cast<MoveId>(150):move,enemy?move:static_cast<MoveId>(150));
    OwnedPokemon& user=enemy?f.opponent():f.player();
    // Keep exactly CONVERSION + FIRE PUNCH so Fire is the only legal result;
    // createPokemon's level-up moves in slots 2/3 are irrelevant here.
    user.moves[2]=MoveId::None;user.movePp[2]=0;
    user.moves[3]=MoveId::None;user.movePp[3]=0;
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    const DedicatedMoveEffectState& state=enemy?f.battle.opponentMoveEffects[0]:f.battle.playerMoveEffects;
    assert(r.accepted&&state.typeOverrideActive&&state.type1==PokemonType::Fire&&
           saw(r,BattleMoveEffect::TypeChanged,enemy?BattleSide::Opponent:BattleSide::Player));
  }
  // Retail Gen III considers the first empty slot the end of the moveset.
  // A move placed after that gap cannot make Conversion succeed. This also
  // guards both execution sides against accidentally compacting all 4 slots.
  for(bool enemy:{false,true}){
    const MoveId conversion=static_cast<MoveId>(160);
    Fixture f(enemy?static_cast<MoveId>(150):conversion,
              enemy?conversion:static_cast<MoveId>(150));
    OwnedPokemon& user=enemy?f.opponent():f.player();
    user.moves[0]=conversion;user.movePp[0]=30;
    user.moves[1]=MoveId::None;user.movePp[1]=0;
    user.moves[2]=static_cast<MoveId>(7);user.movePp[2]=15;
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    const DedicatedMoveEffectState& state=enemy?f.battle.opponentMoveEffects[0]:
        f.battle.playerMoveEffects;
    assert(r.accepted&&!state.typeOverrideActive&&
           saw(r,BattleMoveEffect::Failed,enemy?BattleSide::Opponent:BattleSide::Player));
  }
  for(bool enemy:{false,true}){
    const MoveId move=static_cast<MoveId>(176);
    Fixture f(enemy?static_cast<MoveId>(150):move,enemy?move:static_cast<MoveId>(150));
    DedicatedMoveEffectState& state=enemy?f.battle.opponentMoveEffects[0]:
        f.battle.playerMoveEffects;
    state.lastLandedMove=static_cast<MoveId>(7);
    state.lastLandedType=PokemonType::Fire;
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    assert(r.accepted&&state.typeOverrideActive&&
           saw(r,BattleMoveEffect::TypeChanged,enemy?BattleSide::Opponent:BattleSide::Player));
  }

  symmetricFlag(static_cast<MoveId>(194),&DedicatedMoveEffectState::destinyBond,
                BattleMoveEffect::DestinyBondSet);
  symmetricFlag(static_cast<MoveId>(275),&DedicatedMoveEffectState::ingrained,
                BattleMoveEffect::Ingrained);
  symmetricFlag(static_cast<MoveId>(286),&DedicatedMoveEffectState::imprisoned,
                BattleMoveEffect::Imprisoned);
  symmetricFlag(static_cast<MoveId>(288),&DedicatedMoveEffectState::grudge,
                BattleMoveEffect::GrudgeSet);
  symmetricFlag(static_cast<MoveId>(300),&DedicatedMoveEffectState::mudSport,
                BattleMoveEffect::MudSportSet);
  symmetricFlag(static_cast<MoveId>(346),&DedicatedMoveEffectState::waterSport,
                BattleMoveEffect::WaterSportSet);

  // AtkCanceller removes Destiny Bond and Grudge before every attempted move,
  // even when flinch/status prevents that move.  Conversely, a faster foe
  // that KOs the user before its action must still be taken down by the bond.
  for(bool enemy:{false,true}){
    const MoveId destiny=static_cast<MoveId>(194),splash=static_cast<MoveId>(150);
    Fixture stopped(enemy?splash:destiny,enemy?destiny:splash);
    DedicatedMoveEffectState& vows=enemy?stopped.battle.opponentMoveEffects[0]:
        stopped.battle.playerMoveEffects;
    CombatVolatile& state=enemy?stopped.battle.opponentVolatiles[0]:
        stopped.battle.playerVolatile;
    vows.destinyBond=true;vows.grudge=true;state.flinched=true;
    const BattleActionResult interrupted=enemy?stopped.opponentUses():stopped.playerUses();
    assert(interrupted.accepted&&!vows.destinyBond&&!vows.grudge);
    assert(sawCannotMove(interrupted,
           enemy?BattleSide::Opponent:BattleSide::Player,2U));

    Fixture preempted(enemy?MoveId::Tackle:splash,
                      enemy?splash:MoveId::Tackle);
    OwnedPokemon& bonded=enemy?preempted.opponent():preempted.player();
    OwnedPokemon& attacker=enemy?preempted.player():preempted.opponent();
    DedicatedMoveEffectState& activeBond=enemy?preempted.battle.opponentMoveEffects[0]:
        preempted.battle.playerMoveEffects;
    bonded.level=1;bonded.maximumHp=bonded.currentHp=1;
    attacker.level=100;attacker.maximumHp=attacker.currentHp=5000;
    activeBond.destinyBond=true;
    const BattleActionResult ko=BattleEngine::fightPvp(
        preempted.battle,preempted.collection,0,0);
    assert(ko.accepted&&!bonded.currentHp&&!attacker.currentHp&&
           saw(ko,BattleMoveEffect::DestinyBondTriggered,
               enemy?BattleSide::Opponent:BattleSide::Player));
  }


  // Flinch feedback belongs to the denied action, not to the hit itself.
  // Exercise the complete turn in both directions with Fake Out's guaranteed
  // Gen-III flinch so Bite, Headbutt, Stomp, King's Rock and every other
  // source that reaches the same volatile flag cannot regress silently.
  for(bool enemy:{false,true}){
    const MoveId fakeOut=static_cast<MoveId>(252),splash=static_cast<MoveId>(150);
    Fixture flinch(enemy?splash:fakeOut,enemy?fakeOut:splash);
    // Resolve both selected moves; Fixture::playerUses() intentionally models
    // the remote side switching and would therefore have no denied action to
    // announce.
    const BattleActionResult turn=BattleEngine::fightPvp(
        flinch.battle,flinch.collection,0,0);
    const BattleSide attacker=enemy?BattleSide::Opponent:BattleSide::Player;
    const BattleSide target=enemy?BattleSide::Player:BattleSide::Opponent;
    assert(turn.accepted&&sawMove(turn,fakeOut,attacker)&&
           sawCannotMove(turn,target,2U)&&!sawMove(turn,splash,target));
  }

  // PERISH SONG starts at three after its setup turn and affects both sides.
  Fixture perish(static_cast<MoveId>(195),static_cast<MoveId>(150));
  const BattleActionResult perishResult=perish.playerUses();
  assert(perishResult.accepted&&perish.battle.playerMoveEffects.perishTurns==3&&
         perish.battle.opponentMoveEffects[0].perishTurns==3&&
         saw(perishResult,BattleMoveEffect::PerishSongSet,BattleSide::Player));
  perish.player().moves[0]=static_cast<MoveId>(150);perish.player().movePp[0]=40;
  perish.opponent().moves[0]=static_cast<MoveId>(150);perish.opponent().movePp[0]=40;
  BattleEngine::fightPvp(perish.battle,perish.collection,0,0xFEU);
  BattleEngine::fightPvp(perish.battle,perish.collection,0,0xFEU);
  const BattleActionResult perishEnd=BattleEngine::fightPvp(perish.battle,perish.collection,0,0xFEU);
  assert(perishEnd.accepted&&!perish.player().currentHp&&!perish.opponent().currentHp);

  // FireRed deliberately omits PERISH SONG and HEAL BELL from its generic
  // sound-move table. They inspect Soundproof per affected battler/member;
  // one Soundproof foe must not cancel the entire command.
  for(bool enemy:{false,true}){
    const MoveId perishSong=static_cast<MoveId>(195),splash=static_cast<MoveId>(150);
    Fixture mixed(enemy?splash:perishSong,enemy?perishSong:splash);
    OwnedPokemon& protectedTarget=enemy?mixed.player():mixed.opponent();
    protectedTarget.abilityId=43; // SOUNDPROOF
    const BattleActionResult r=enemy?mixed.opponentUses():mixed.playerUses();
    const DedicatedMoveEffectState& userState=enemy?mixed.battle.opponentMoveEffects[0]:
        mixed.battle.playerMoveEffects;
    const DedicatedMoveEffectState& targetState=enemy?mixed.battle.playerMoveEffects:
        mixed.battle.opponentMoveEffects[0];
    assert(r.accepted&&userState.perishTurns==3U&&targetState.perishTurns==0U);
  }
  {
    const MoveId healBell=static_cast<MoveId>(215);
    Fixture bell(healBell,static_cast<MoveId>(150));
    bell.player().status=StatusCondition::Poison;
    bell.opponent().abilityId=43; // an opposing SOUNDPROOF cannot block it
    const BattleActionResult r=bell.playerUses();
    assert(r.accepted&&bell.player().status==StatusCondition::None);
  }

  // DEFENSE CURL raises Defense and records Rollout's hidden double-power bit.
  Fixture curl(static_cast<MoveId>(111),static_cast<MoveId>(150));
  const BattleActionResult curlResult=curl.playerUses();
  assert(curlResult.accepted&&curl.battle.playerVolatile.defenseStage==1&&
         curl.battle.playerMoveEffects.defenseCurl);

  // In Gen III CHARGE only arms the following Electric attack; the Sp. Def
  // boost seen in later generations did not exist in FireRed/Emerald.
  for(bool enemy:{false,true}){
    const MoveId charge=static_cast<MoveId>(268);
    Fixture f(enemy?static_cast<MoveId>(150):charge,enemy?charge:static_cast<MoveId>(150));
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    assert(r.accepted);
    if(enemy)assert(f.battle.opponentVolatiles[0].spDefenseStage==0&&
                    f.battle.opponentMoveEffects[0].chargeTurns==1);
    else assert(f.battle.playerVolatile.spDefenseStage==0&&
                f.battle.playerMoveEffects.chargeTurns==1);
  }

  // FLAME WHEEL and SACRED FIRE are the two EFFECT_THAW_HIT moves in the
  // retail tables. A frozen user is allowed to execute them and thaws before
  // the attack, on either side of the battle.
  for(bool enemy:{false,true}){
    const MoveId flameWheel=static_cast<MoveId>(172),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:flameWheel,enemy?flameWheel:splash);
    OwnedPokemon& user=enemy?f.opponent():f.player();
    user.status=StatusCondition::Frozen;
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    assert(r.accepted&&user.status==StatusCondition::None&&
           (enemy?r.damageTaken:r.damageDealt)>0U);
  }

  // FLASH FIRE absorbs the triggering hit and arms a persistent 1.5x Fire
  // boost for that battler. Verify activation and boosted damage for both
  // execution paths.
  for(bool enemy:{false,true}){
    const MoveId firePunch=static_cast<MoveId>(7),splash=static_cast<MoveId>(150);
    Fixture activation(enemy?splash:firePunch,enemy?firePunch:splash);
    OwnedPokemon& absorber=enemy?activation.player():activation.opponent();
    absorber.abilityId=18; // FLASH FIRE
    const uint16_t hpBefore=absorber.currentHp;
    const BattleActionResult absorbed=enemy?activation.opponentUses():activation.playerUses();
    const DedicatedMoveEffectState& armed=enemy?activation.battle.playerMoveEffects:
        activation.battle.opponentMoveEffects[0];
    assert(absorbed.accepted&&absorber.currentHp==hpBefore&&armed.flashFireBoost);

    Fixture normal(enemy?splash:firePunch,enemy?firePunch:splash);
    Fixture boosted(enemy?splash:firePunch,enemy?firePunch:splash);
    (enemy?boosted.battle.opponentMoveEffects[0]:boosted.battle.playerMoveEffects).
        flashFireBoost=true;
    normal.battle.rngState=boosted.battle.rngState=0xF1A5F1A5U;
    const BattleActionResult normalHit=enemy?normal.opponentUses():normal.playerUses();
    const BattleActionResult boostedHit=enemy?boosted.opponentUses():boosted.playerUses();
    assert((enemy?boostedHit.damageTaken:boostedHit.damageDealt)>
           (enemy?normalHit.damageTaken:normalHit.damageDealt));
  }

  // SmellingSalt only cures paralysis after an effective hit. A Normal hit
  // into a Ghost immunity must leave the target's paralysis untouched.
  for(bool enemy:{false,true}){
    const MoveId smellingSalt=static_cast<MoveId>(265),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:smellingSalt,enemy?smellingSalt:splash);
    OwnedPokemon& target=enemy?f.player():f.opponent();
    const uint32_t uid=target.uid;
    target=CollectionLogic::createPokemon(uid,92,50,false,0x5A17U); // GASTLY
    target.status=StatusCondition::Paralysis;
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    assert(r.accepted&&target.status==StatusCondition::Paralysis&&
           (enemy?r.damageTaken:r.damageDealt)==0U);
  }

  // WISH is tied to the side's battlefield position, not to the original user.
  for(bool enemy:{false,true}){
    const MoveId wish=static_cast<MoveId>(273);
    Fixture f(enemy?static_cast<MoveId>(150):wish,enemy?wish:static_cast<MoveId>(150));
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    assert(r.accepted&&(enemy?f.battle.opponentWishDueTurn:f.battle.playerWishDueTurn)==2);
    OwnedPokemon& recipient=enemy?f.opponent():f.player();
    recipient.currentHp=static_cast<uint16_t>(recipient.maximumHp/2U);
    if(enemy){f.opponent().moves[0]=static_cast<MoveId>(150);f.opponent().movePp[0]=40;}
    else{f.player().moves[0]=static_cast<MoveId>(150);f.player().movePp[0]=40;}
    const uint16_t beforeWish=recipient.currentHp;
    const BattleActionResult granted=BattleEngine::fightPvp(f.battle,f.collection,0,0xFEU);
    assert(recipient.currentHp>beforeWish&&
           saw(granted,BattleMoveEffect::WishGranted,enemy?BattleSide::Opponent:BattleSide::Player));
  }

  // RECYCLE restores precisely the item consumed by that battler.
  for(bool enemy:{false,true}){
    const MoveId recycle=static_cast<MoveId>(278);
    Fixture f(enemy?static_cast<MoveId>(150):recycle,enemy?recycle:static_cast<MoveId>(150));
    DedicatedMoveEffectState& state=enemy?f.battle.opponentMoveEffects[0]:f.battle.playerMoveEffects;
    state.recyclableItem=HeldItem::OranBerry;
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    const CombatVolatile& volatileState=enemy?f.battle.opponentVolatiles[0]:f.battle.playerVolatile;
    assert(r.accepted&&volatileState.hasHeldItemOverride&&
           volatileState.heldItemOverride==HeldItem::OranBerry&&state.recyclableItem==HeldItem::None);
  }

  // Destiny Bond and Grudge trigger only when the armed user is directly
  // knocked out by the opposing move.
  Fixture destiny(MoveId::Tackle,static_cast<MoveId>(150));
  destiny.battle.opponentMoveEffects[0].destinyBond=true;
  destiny.opponent().currentHp=1;
  const BattleActionResult destinyHit=destiny.playerUses();
  assert(destinyHit.accepted&&!destiny.opponent().currentHp&&!destiny.player().currentHp&&
         saw(destinyHit,BattleMoveEffect::DestinyBondTriggered,BattleSide::Opponent));

  // Exercise the actual command sequence too.  The opponent moves first,
  // arms Destiny Bond, and is then knocked out by the player's move in that
  // same turn.  This is the path used by trainer AI on the device.
  Fixture enemyDestiny(MoveId::Tackle,static_cast<MoveId>(194));
  enemyDestiny.opponent().currentHp=1;
  enemyDestiny.battle.playerVolatile.speedStage=-6;
  enemyDestiny.battle.opponentVolatiles[0].speedStage=6;
  const BattleActionResult enemyDestinyHit=BattleEngine::fightPvp(
      enemyDestiny.battle,enemyDestiny.collection,0,0);
  assert(enemyDestinyHit.accepted&&!enemyDestiny.opponent().currentHp&&
         !enemyDestiny.player().currentHp&&
         saw(enemyDestinyHit,BattleMoveEffect::DestinyBondSet,BattleSide::Opponent)&&
         saw(enemyDestinyHit,BattleMoveEffect::DestinyBondTriggered,BattleSide::Opponent));

  // The inverse order must be identical: player arms the bond before the
  // opponent delivers a lethal hit.
  Fixture playerDestiny(static_cast<MoveId>(194),MoveId::Tackle);
  playerDestiny.player().currentHp=1;
  playerDestiny.battle.playerVolatile.speedStage=6;
  playerDestiny.battle.opponentVolatiles[0].speedStage=-6;
  const BattleActionResult playerDestinyHit=BattleEngine::fightPvp(
      playerDestiny.battle,playerDestiny.collection,0,0);
  assert(playerDestinyHit.accepted&&!playerDestiny.player().currentHp&&
         !playerDestiny.opponent().currentHp&&
         saw(playerDestinyHit,BattleMoveEffect::DestinyBondSet,BattleSide::Player)&&
         saw(playerDestinyHit,BattleMoveEffect::DestinyBondTriggered,BattleSide::Player));

  // Bide has a dedicated release path rather than the ordinary damage path;
  // it must still count as the final opposing hit for Destiny Bond.
  Fixture bideIntoDestiny(static_cast<MoveId>(117),static_cast<MoveId>(150));
  bideIntoDestiny.battle.playerBide.turns=1;
  bideIntoDestiny.battle.playerBide.damage=1000;
  bideIntoDestiny.battle.opponentMoveEffects[0].destinyBond=true;
  bideIntoDestiny.opponent().currentHp=1;
  const BattleActionResult bideDestinyHit=bideIntoDestiny.playerUses();
  assert(bideDestinyHit.accepted&&!bideIntoDestiny.opponent().currentHp&&
         !bideIntoDestiny.player().currentHp&&
         saw(bideDestinyHit,BattleMoveEffect::DestinyBondTriggered,BattleSide::Opponent));

  Fixture enemyBideIntoDestiny(static_cast<MoveId>(194),static_cast<MoveId>(117));
  enemyBideIntoDestiny.player().currentHp=1;
  enemyBideIntoDestiny.battle.playerVolatile.speedStage=6;
  enemyBideIntoDestiny.battle.opponentVolatiles[0].speedStage=-6;
  enemyBideIntoDestiny.battle.opponentBides[0].turns=1;
  enemyBideIntoDestiny.battle.opponentBides[0].damage=1000;
  const BattleActionResult enemyBideDestinyHit=BattleEngine::fightPvp(
      enemyBideIntoDestiny.battle,enemyBideIntoDestiny.collection,0,0);
  assert(enemyBideDestinyHit.accepted&&!enemyBideIntoDestiny.player().currentHp&&
         !enemyBideIntoDestiny.opponent().currentHp&&
         saw(enemyBideDestinyHit,BattleMoveEffect::DestinyBondTriggered,BattleSide::Player));

  Fixture grudge(MoveId::Tackle,static_cast<MoveId>(150));
  grudge.battle.opponentMoveEffects[0].grudge=true;
  grudge.opponent().currentHp=1;
  grudge.player().movePp[0]=20;
  const BattleActionResult grudgeHit=grudge.playerUses();
  assert(grudgeHit.accepted&&!grudge.opponent().currentHp&&grudge.player().movePp[0]==0);

  // Imprison exposes the same command-selection predicate used by the UI.
  Fixture imprison(static_cast<MoveId>(150),MoveId::Tackle);
  imprison.battle.opponentMoveEffects[0].imprisoned=true;
  imprison.opponent().moves[1]=static_cast<MoveId>(150);
  assert(BattleEngine::moveIsImprisoned(imprison.battle,imprison.collection,
                                        static_cast<MoveId>(150),BattleSide::Player));

  // CAMOUFLAGE consumes the battle's fixed terrain type.
  for(bool enemy:{false,true}){
    const MoveId camouflage=static_cast<MoveId>(293);
    Fixture f(enemy?static_cast<MoveId>(150):camouflage,enemy?camouflage:static_cast<MoveId>(150));
    f.battle.terrainType=PokemonType::Grass;
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    const DedicatedMoveEffectState& state=enemy?f.battle.opponentMoveEffects[0]:f.battle.playerMoveEffects;
    assert(r.accepted&&state.typeOverrideActive&&state.type1==PokemonType::Grass);
  }

  // The next dedicated batch used to fall through as ordinary power-0 moves.
  for(bool enemy:{false,true}){
    const MoveId foresight=static_cast<MoveId>(193);
    Fixture f(enemy?MoveId::Tackle:foresight,enemy?foresight:MoveId::Tackle);
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    const DedicatedMoveEffectState& target=enemy?f.battle.playerMoveEffects:
        f.battle.opponentMoveEffects[0];
    assert(r.accepted&&target.identified&&saw(r,BattleMoveEffect::Identified,
           enemy?BattleSide::Player:BattleSide::Opponent));
  }

  static Fixture spite(static_cast<MoveId>(180),MoveId::Tackle);
  spite.battle.opponentVolatiles[0].lastMoveUsed=MoveId::Tackle;
  spite.opponent().movePp[0]=20;
  const BattleActionResult spiteResult=spite.playerUses();
  assert(spiteResult.accepted&&spite.opponent().movePp[0]<=18&&
         saw(spiteResult,BattleMoveEffect::PpReduced,BattleSide::Opponent));

  static Fixture skillSwap(static_cast<MoveId>(285),MoveId::Tackle);
  skillSwap.player().abilityId=1;skillSwap.opponent().abilityId=2;
  const BattleActionResult swapResult=skillSwap.playerUses();
  assert(swapResult.accepted&&skillSwap.player().abilityId==2&&
         skillSwap.opponent().abilityId==1&&
         saw(swapResult,BattleMoveEffect::AbilitiesSwapped,BattleSide::Player));

  static Fixture taunt(static_cast<MoveId>(269),MoveId::Tackle);
  taunt.opponent().moves[1]=static_cast<MoveId>(14); // SWORDS DANCE
  taunt.opponent().movePp[1]=30;
  assert(taunt.playerUses().accepted&&taunt.battle.opponentMoveEffects[0].tauntTurns);
  assert(!BattleEngine::moveIsSelectable(taunt.battle,taunt.collection,
      static_cast<MoveId>(14),BattleSide::Opponent));

  static Fixture torment(static_cast<MoveId>(259),MoveId::Tackle);
  torment.battle.opponentVolatiles[0].lastMoveUsed=MoveId::Tackle;
  assert(torment.playerUses().accepted&&
         !BattleEngine::moveIsSelectable(torment.battle,torment.collection,
             MoveId::Tackle,BattleSide::Opponent));

  static Fixture yawn(static_cast<MoveId>(281),MoveId::Tackle);
  assert(yawn.playerUses().accepted&&yawn.battle.opponentMoveEffects[0].yawnTurns==1);
  yawn.player().moves[0]=MoveId::Tackle;yawn.player().movePp[0]=35;
  BattleEngine::fightPvp(yawn.battle,yawn.collection,0,0xFEU);
  assert(yawn.opponent().status==StatusCondition::Sleep);

  // ATTRACT uses the original species gender ratio plus personality byte;
  // genderless or same-gender targets fail instead of receiving a generic
  // status. Nidoran's fixed genders make both directions deterministic.
  for(bool enemy:{false,true}){
    const MoveId attract=static_cast<MoveId>(213);
    Fixture f(enemy?MoveId::Tackle:attract,enemy?attract:MoveId::Tackle);
    f.player().speciesId=enemy?32:29;      // male when targeted, female when using
    f.opponent().speciesId=enemy?29:32;
    CollectionLogic::refreshDerivedStats(f.player());
    CollectionLogic::refreshDerivedStats(f.opponent());
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    const DedicatedMoveEffectState& target=enemy?f.battle.playerMoveEffects:
        f.battle.opponentMoveEffects[0];
    const uint32_t expectedUid=enemy?f.opponent().uid:f.player().uid;
    assert(r.accepted&&target.attractedToUid==expectedUid&&
           saw(r,BattleMoveEffect::Infatuated,
               enemy?BattleSide::Player:BattleSide::Opponent));
  }

  // MEMENTO is independent from Damp: the user faints while both offensive
  // stages of the target fall by two.
  static Fixture memento(static_cast<MoveId>(262),MoveId::Tackle);
  const BattleActionResult mementoResult=memento.playerUses();
  assert(mementoResult.accepted&&!memento.player().currentHp&&
         memento.battle.opponentVolatiles[0].attackStage==-2&&
         memento.battle.opponentVolatiles[0].spAttackStage==-2);

  // Memento and Tickle use the ordinary Gen-III stat-loss command for each
  // affected stat. Mist therefore blocks every drop, while Memento still
  // makes its user faint. Keep this symmetric because the player and trainer
  // action pipelines are separate in the firmware.
  for(bool enemy:{false,true}){
    const MoveId mementoMove=static_cast<MoveId>(262),splash=static_cast<MoveId>(150);
    Fixture protectedMemento(enemy?splash:mementoMove,enemy?mementoMove:splash);
    if(enemy)protectedMemento.battle.playerMistTurns=5;
    else protectedMemento.battle.opponentMistTurns=5;
    const BattleActionResult r=enemy?protectedMemento.opponentUses():
        protectedMemento.playerUses();
    const OwnedPokemon& user=enemy?protectedMemento.opponent():protectedMemento.player();
    const CombatVolatile& target=enemy?protectedMemento.battle.playerVolatile:
        protectedMemento.battle.opponentVolatiles[0];
    assert(r.accepted&&!user.currentHp&&target.attackStage==0&&
           target.spAttackStage==0);

    const MoveId tickle=static_cast<MoveId>(321);
    Fixture protectedTickle(enemy?splash:tickle,enemy?tickle:splash);
    if(enemy)protectedTickle.battle.playerMistTurns=5;
    else protectedTickle.battle.opponentMistTurns=5;
    const BattleActionResult tickleResult=enemy?protectedTickle.opponentUses():
        protectedTickle.playerUses();
    const CombatVolatile& tickleTarget=enemy?protectedTickle.battle.playerVolatile:
        protectedTickle.battle.opponentVolatiles[0];
    assert(tickleResult.accepted&&tickleTarget.attackStage==0&&
           tickleTarget.defenseStage==0);
  }

  // BEAT UP is another real multi-hit loop in Gen III. Each healthy,
  // status-free team member contributes a separately applied strike; a
  // statused member is skipped. Verify both player and trainer paths and the
  // presentation journal, not merely the aggregate damage number.
  for(bool enemy:{false,true}){
    const MoveId beatUp=static_cast<MoveId>(251),splash=static_cast<MoveId>(150);
    Fixture solo(enemy?splash:beatUp,enemy?beatUp:splash);
    Fixture team(enemy?splash:beatUp,enemy?beatUp:splash);
    if(enemy){
      team.battle.opponentCount=2;
      team.battle.opponents[1]=CollectionLogic::createPokemon(
          0xBEEFB002U,68,50,false,0xBEEFU);
    }else{
      uint32_t partnerUid=0;
      assert(CollectionLogic::add(team.collection,
          CollectionLogic::createPokemon(0,68,50,false,0xBEEFU),&partnerUid));
      assert(CollectionLogic::setPartySlot(team.collection,1,partnerUid));
    }
    solo.battle.rngState=team.battle.rngState=0xBC01U;
    const BattleActionResult soloResult=enemy?solo.opponentUses():solo.playerUses();
    const BattleActionResult teamResult=enemy?team.opponentUses():team.playerUses();
    const BattleSide targetSide=enemy?BattleSide::Player:BattleSide::Opponent;
    const uint16_t soloDamage=enemy?soloResult.damageTaken:soloResult.damageDealt;
    const uint16_t teamDamage=enemy?teamResult.damageTaken:teamResult.damageDealt;
    assert(soloDamage>0&&teamDamage>soloDamage&&
           countEvents(soloResult,BattleEventType::HpChanged,targetSide)==1U&&
           countEvents(teamResult,BattleEventType::HpChanged,targetSide)==2U);

    if(enemy)team.battle.opponents[1].status=StatusCondition::Poison;
    else CollectionLogic::active(team.collection,1)->status=StatusCondition::Poison;
    team.battle.rngState=0xBC01U;
    team.player().currentHp=team.player().maximumHp;
    team.opponent().currentHp=team.opponent().maximumHp;
    const BattleActionResult skipped=enemy?team.opponentUses():team.playerUses();
    assert(countEvents(skipped,BattleEventType::HpChanged,targetSide)==1U);
  }

  // MAGIC COAT and SNATCH use FireRed's generated per-move flags. They are
  // turn-scoped and work identically regardless of which side owns them.
  for(bool enemyCoat:{false,true}){
    const MoveId coat=static_cast<MoveId>(277),wave=static_cast<MoveId>(86);
    Fixture f(enemyCoat?wave:coat,enemyCoat?coat:wave);
    (enemyCoat?f.opponent():f.player()).level=100;
    (enemyCoat?f.player():f.opponent()).level=5;
    const BattleActionResult r=f.opponentUses();
    assert(r.accepted);
    const OwnedPokemon& reflectedTarget=enemyCoat?f.player():f.opponent();
    const OwnedPokemon& protectedUser=enemyCoat?f.opponent():f.player();
    assert(reflectedTarget.status==StatusCondition::Paralysis&&
           protectedUser.status==StatusCondition::None&&
           saw(r,BattleMoveEffect::MagicCoatReflected,
                enemyCoat?BattleSide::Opponent:BattleSide::Player));

    Fixture last(coat,coat);
    (enemyCoat?last.opponent():last.player()).level=5;
    (enemyCoat?last.player():last.opponent()).level=100;
    const BattleActionResult failed=last.opponentUses();
    assert(failed.accepted&&saw(failed,BattleMoveEffect::Failed,
           enemyCoat?BattleSide::Opponent:BattleSide::Player));
  }
  for(bool enemySnatch:{false,true}){
    const MoveId snatch=static_cast<MoveId>(289),dance=static_cast<MoveId>(14);
    Fixture f(enemySnatch?dance:snatch,enemySnatch?snatch:dance);
    (enemySnatch?f.opponent():f.player()).level=100;
    (enemySnatch?f.player():f.opponent()).level=5;
    const BattleActionResult r=f.opponentUses();
    assert(r.accepted);
    const CombatVolatile& thief=enemySnatch?f.battle.opponentVolatiles[0]:
        f.battle.playerVolatile;
    const CombatVolatile& owner=enemySnatch?f.battle.playerVolatile:
        f.battle.opponentVolatiles[0];
    assert(thief.attackStage==2&&owner.attackStage==0&&
           saw(r,BattleMoveEffect::MoveSnatched,
               enemySnatch?BattleSide::Opponent:BattleSide::Player));

    Fixture last(snatch,snatch);
    (enemySnatch?last.opponent():last.player()).level=5;
    (enemySnatch?last.player():last.opponent()).level=100;
    const BattleActionResult failed=last.opponentUses();
    assert(failed.accepted&&saw(failed,BattleMoveEffect::Failed,
           enemySnatch?BattleSide::Opponent:BattleSide::Player));
  }

  // PURSUIT strikes the outgoing battler before the switch and doubles its
  // base power. The same selected enemy command is not repeated afterward.
  static Fixture pursuitNormal(static_cast<MoveId>(150),static_cast<MoveId>(228));
  static Fixture pursuitSwitch(static_cast<MoveId>(150),static_cast<MoveId>(228));
  pursuitSwitch.opponent().moves[1]=MoveId::None;pursuitSwitch.opponent().movePp[1]=0;
  pursuitNormal.battle.rngState=pursuitSwitch.battle.rngState=0x7711U;
  uint32_t replacementUid=0;
  assert(CollectionLogic::add(pursuitSwitch.collection,
      CollectionLogic::createPokemon(0,1,50,false,0x7712U),&replacementUid));
  assert(CollectionLogic::setPartySlot(pursuitSwitch.collection,1,replacementUid));
  const uint16_t ordinary=pursuitNormal.opponentUses().damageTaken;
  const uint32_t outgoingUid=pursuitSwitch.player().uid;
  const BattleActionResult pursued=BattleEngine::switchToPokemonPvp(
      pursuitSwitch.battle,pursuitSwitch.collection,replacementUid,0,false);
  assert(pursued.accepted&&pursuitSwitch.battle.playerUid==replacementUid&&
         pursued.damageTaken>ordinary);
  uint8_t pursuitEvent=0xFFU,switchEvent=0xFFU;
  for(uint8_t i=0;i<pursued.eventCount;++i){
    if(pursued.events[i].type==BattleEventType::MoveUsed&&
       pursued.events[i].pokemonUid==pursuitSwitch.opponent().uid)pursuitEvent=i;
    if(pursued.events[i].type==BattleEventType::SwitchedIn&&
       pursued.events[i].pokemonUid==replacementUid)switchEvent=i;
  }
  assert(outgoingUid!=replacementUid&&pursuitEvent<switchEvent);

  // Counter/Mirror Coat and Focus Punch depend on damage received earlier in
  // the same ordered turn, not on damage left over from a previous turn.
  static Fixture counter(static_cast<MoveId>(68),MoveId::Tackle);
  counter.player().moves[1]=MoveId::Tackle;counter.player().movePp[1]=35;
  counter.opponent().moves[0]=MoveId::Tackle;counter.opponent().movePp[0]=35;
  counter.player().level=5;counter.opponent().level=80;
  counter.player().maximumHp=counter.player().currentHp=1000;
  counter.opponent().maximumHp=counter.opponent().currentHp=1000;
  const uint16_t counterBefore=counter.opponent().currentHp;
  const BattleActionResult counterResult=BattleEngine::fightPvp(counter.battle,counter.collection,0,0);
  assert(counterResult.accepted&&counterResult.damageTaken>0&&
         counterResult.damageDealt==counterResult.damageTaken*2U&&
         counter.opponent().currentHp==counterBefore-counterResult.damageDealt&&
         !saw(counterResult,BattleMoveEffect::Failed,BattleSide::Opponent));

  // A special hit cannot feed Counter, and damage from the preceding turn
  // must never be reused when the foe does not hit physically this turn.
  counter.opponent().moves[0]=static_cast<MoveId>(52); // EMBER is special in Gen III.
  counter.opponent().movePp[0]=25;
  const uint16_t counterSpecialBefore=counter.opponent().currentHp;
  const BattleActionResult counterSpecial=BattleEngine::fightPvp(counter.battle,counter.collection,0,0);
  assert(counterSpecial.accepted&&counterSpecial.damageDealt==0&&
         counter.opponent().currentHp==counterSpecialBefore&&
         saw(counterSpecial,BattleMoveEffect::Failed,BattleSide::Player));
  const uint16_t counterIdleBefore=counter.opponent().currentHp;
  const BattleActionResult counterWithoutHit=
      BattleEngine::fightPvp(counter.battle,counter.collection,0,0xFEU);
  assert(counterWithoutHit.accepted&&counterWithoutHit.damageDealt==0&&
         counter.opponent().currentHp==counterIdleBefore&&
         saw(counterWithoutHit,BattleMoveEffect::Failed,BattleSide::Player));

  // The opponent path is governed by the same rule and must return twice the
  // physical HP actually removed by the player's attack.
  static Fixture enemyCounter(MoveId::Tackle,static_cast<MoveId>(68));
  enemyCounter.player().maximumHp=enemyCounter.player().currentHp=1000;
  enemyCounter.opponent().maximumHp=enemyCounter.opponent().currentHp=1000;
  const uint16_t enemyCounterBefore=enemyCounter.player().currentHp;
  const BattleActionResult enemyCounterResult=
      BattleEngine::fightPvp(enemyCounter.battle,enemyCounter.collection,0,0);
  const uint16_t physicalDealt=static_cast<uint16_t>(
      1000U-enemyCounter.opponent().currentHp);
  assert(enemyCounterResult.accepted&&physicalDealt>0&&
         enemyCounterResult.damageTaken==physicalDealt*2U&&
         enemyCounter.player().currentHp==enemyCounterBefore-enemyCounterResult.damageTaken&&
         !saw(enemyCounterResult,BattleMoveEffect::Failed,BattleSide::Player));

  // typecalc2 still runs for Counter/Mirror Coat even though their damage is
  // fixed. Foresight removes only Ghost's Fighting immunity (retaining the
  // other type's matchup), and Wonder Guard can still reject the retaliation.
  for(bool enemy:{false,true}){
    const MoveId counterMove=static_cast<MoveId>(68),tackle=MoveId::Tackle;
    Fixture ghost(enemy?tackle:counterMove,enemy?counterMove:tackle);
    OwnedPokemon& counterUser=enemy?ghost.opponent():ghost.player();
    OwnedPokemon& ghostTarget=enemy?ghost.player():ghost.opponent();
    const uint32_t ghostUid=ghostTarget.uid;
    ghostTarget=CollectionLogic::createPokemon(ghostUid,92,80,false,0xC017U); // GASTLY
    ghostTarget.moves[0]=tackle;ghostTarget.movePp[0]=35;
    counterUser.level=5;
    counterUser.maximumHp=counterUser.currentHp=1000;
    ghostTarget.maximumHp=ghostTarget.currentHp=1000;
    const BattleActionResult immune=BattleEngine::fightPvp(ghost.battle,ghost.collection,0,0);
    assert(immune.accepted&&(enemy?immune.damageTaken:immune.damageDealt)==0U);

    Fixture identified(enemy?tackle:counterMove,enemy?counterMove:tackle);
    OwnedPokemon& identifiedUser=enemy?identified.opponent():identified.player();
    OwnedPokemon& identifiedTarget=enemy?identified.player():identified.opponent();
    const uint32_t identifiedUid=identifiedTarget.uid;
    identifiedTarget=CollectionLogic::createPokemon(identifiedUid,92,80,false,0xC018U);
    identifiedTarget.moves[0]=tackle;identifiedTarget.movePp[0]=35;
    identifiedUser.level=5;
    identifiedUser.maximumHp=identifiedUser.currentHp=1000;
    identifiedTarget.maximumHp=identifiedTarget.currentHp=1000;
    (enemy?identified.battle.playerMoveEffects:
           identified.battle.opponentMoveEffects[0]).identified=true;
    const BattleActionResult connects=BattleEngine::fightPvp(
        identified.battle,identified.collection,0,0);
    assert(connects.accepted&&(enemy?connects.damageTaken:connects.damageDealt)>0U);

    Fixture guard(enemy?tackle:counterMove,enemy?counterMove:tackle);
    OwnedPokemon& guardUser=enemy?guard.opponent():guard.player();
    OwnedPokemon& guardTarget=enemy?guard.player():guard.opponent();
    const uint32_t guardUid=guardTarget.uid;
    guardTarget=CollectionLogic::createPokemon(guardUid,292,80,false,0xC019U); // SHEDINJA
    guardTarget.moves[0]=tackle;guardTarget.movePp[0]=35;
    guardUser.level=5;guardUser.maximumHp=guardUser.currentHp=1000;
    guardTarget.maximumHp=guardTarget.currentHp=1;
    (enemy?guard.battle.playerMoveEffects:
           guard.battle.opponentMoveEffects[0]).identified=true;
    const BattleActionResult guarded=BattleEngine::fightPvp(
        guard.battle,guard.collection,0,0);
    assert(guarded.accepted&&guardTarget.currentHp==1U&&
           (enemy?guarded.damageTaken:guarded.damageDealt)==0U);
  }

  // Mirror Coat is the category mirror of Counter: a Fire-type hit feeds it,
  // while the same Tackle used above must not.
  static Fixture mirrorCoat(static_cast<MoveId>(243),static_cast<MoveId>(52));
  mirrorCoat.player().maximumHp=mirrorCoat.player().currentHp=1000;
  mirrorCoat.opponent().maximumHp=mirrorCoat.opponent().currentHp=1000;
  const BattleActionResult mirrorCoatResult=
      BattleEngine::fightPvp(mirrorCoat.battle,mirrorCoat.collection,0,0);
  assert(mirrorCoatResult.accepted&&mirrorCoatResult.damageTaken>0&&
         mirrorCoatResult.damageDealt==mirrorCoatResult.damageTaken*2U&&
         !saw(mirrorCoatResult,BattleMoveEffect::Failed,BattleSide::Player));
  mirrorCoat.opponent().moves[0]=MoveId::Tackle;mirrorCoat.opponent().movePp[0]=35;
  const uint16_t mirrorPhysicalBefore=mirrorCoat.opponent().currentHp;
  const BattleActionResult mirrorPhysical=
      BattleEngine::fightPvp(mirrorCoat.battle,mirrorCoat.collection,0,0);
  assert(mirrorPhysical.accepted&&mirrorPhysical.damageDealt==0&&
         mirrorCoat.opponent().currentHp==mirrorPhysicalBefore&&
         saw(mirrorPhysical,BattleMoveEffect::Failed,BattleSide::Player));

  // The unusual fixed-damage family follows its dedicated script rather than
  // the common power formula. Psywave specifically rejects Random()%16 values
  // 11..15; seed 26 produces 11 then 9, making the expected 140% of level.
  for(bool enemy:{false,true}){
    const MoveId psywave=static_cast<MoveId>(149),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:psywave,enemy?psywave:splash);
    OwnedPokemon& user=enemy?f.opponent():f.player();
    OwnedPokemon& target=enemy?f.player():f.opponent();
    CombatVolatile& markedTarget=enemy?f.battle.playerVolatile:
        f.battle.opponentVolatiles[0];
    user.level=73;target.maximumHp=target.currentHp=5000;
    markedTarget.sureHitTurns=enemy?2U:1U;
    f.battle.rngState=26U;
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    assert(r.accepted&&(enemy?r.damageTaken:r.damageDealt)==102U);
  }

  // Endeavor fails before its accuracy check when the target does not have
  // more HP than the user. Damp prevents Explosion/Self-Destruct completely:
  // no target damage and, crucially, no self-KO.
  for(bool enemy:{false,true}){
    const MoveId endeavor=static_cast<MoveId>(283),splash=static_cast<MoveId>(150);
    Fixture equal(enemy?splash:endeavor,enemy?endeavor:splash);
    OwnedPokemon& equalUser=enemy?equal.opponent():equal.player();
    OwnedPokemon& equalTarget=enemy?equal.player():equal.opponent();
    equalUser.maximumHp=equalUser.currentHp=500;
    equalTarget.maximumHp=equalTarget.currentHp=500;
    const BattleActionResult failed=enemy?equal.opponentUses():equal.playerUses();
    assert(failed.accepted&&(enemy?failed.damageTaken:failed.damageDealt)==0U&&
           saw(failed,BattleMoveEffect::Failed,
               enemy?BattleSide::Player:BattleSide::Opponent));

    const MoveId explosion=static_cast<MoveId>(153);
    Fixture damped(enemy?splash:explosion,enemy?explosion:splash);
    OwnedPokemon& bomber=enemy?damped.opponent():damped.player();
    OwnedPokemon& dampTarget=enemy?damped.player():damped.opponent();
    dampTarget.abilityId=6; // DAMP
    const uint16_t bomberHp=bomber.currentHp,targetHp=dampTarget.currentHp;
    const BattleActionResult stopped=enemy?damped.opponentUses():damped.playerUses();
    assert(stopped.accepted&&bomber.currentHp==bomberHp&&
           dampTarget.currentHp==targetHp&&
           countEvents(stopped,BattleEventType::AbilityActivated,
               enemy?BattleSide::Player:BattleSide::Opponent)==1U);
  }

  // Hidden Power is no longer the generated placeholder power 1, and Weather
  // Ball receives both its weather type and doubled base power.
  static Fixture dynamic(static_cast<MoveId>(237),MoveId::Tackle);
  const uint16_t hidden=dynamic.playerUses().damageDealt;
  assert(hidden>5);
  static Fixture clearBallFixture(static_cast<MoveId>(311),MoveId::Tackle);
  static Fixture rainBallFixture(static_cast<MoveId>(311),MoveId::Tackle);
  clearBallFixture.battle.rngState=rainBallFixture.battle.rngState=0x9911U;
  rainBallFixture.battle.weather=BattleWeather::Rain;
  const uint16_t clearBall=clearBallFixture.playerUses().damageDealt;
  const uint16_t rainBall=rainBallFixture.playerUses().damageDealt;
  assert(rainBall>clearBall*2U);

  // Stat-changing move scripts are intentionally not collapsed into one
  // generic approximation.  Lock the Gen-III composite behavior from the
  // original battle scripts on both execution paths: partial boosts still
  // work, Belly Drum pays floor(maxHP/2), Psych Up copies all seven stages,
  // and Swagger/Flatter still raise a stat when confusion is prevented.
  for(bool enemy:{false,true}){
    const MoveId splash=static_cast<MoveId>(150);
    auto use=[&](Fixture& f){return enemy?f.opponentUses():f.playerUses();};
    auto Side=[&](Fixture& f)->CombatVolatile&{
      return enemy?f.battle.opponentVolatiles[0]:f.battle.playerVolatile;
    };
    auto Target=[&](Fixture& f)->CombatVolatile&{
      return enemy?f.battle.playerVolatile:f.battle.opponentVolatiles[0];
    };

    Fixture bulk(enemy?splash:static_cast<MoveId>(339),
                 enemy?static_cast<MoveId>(339):splash);
    Side(bulk).attackStage=6;Side(bulk).defenseStage=2;
    assert(use(bulk).accepted&&Side(bulk).attackStage==6&&
           Side(bulk).defenseStage==3);

    Fixture calm(enemy?splash:static_cast<MoveId>(347),
                 enemy?static_cast<MoveId>(347):splash);
    Side(calm).spAttackStage=6;Side(calm).spDefenseStage=-1;
    assert(use(calm).accepted&&Side(calm).spAttackStage==6&&
           Side(calm).spDefenseStage==0);

    Fixture cosmic(enemy?splash:static_cast<MoveId>(322),
                   enemy?static_cast<MoveId>(322):splash);
    Side(cosmic).defenseStage=6;Side(cosmic).spDefenseStage=0;
    assert(use(cosmic).accepted&&Side(cosmic).defenseStage==6&&
           Side(cosmic).spDefenseStage==1);

    Fixture dance(enemy?splash:static_cast<MoveId>(349),
                  enemy?static_cast<MoveId>(349):splash);
    Side(dance).attackStage=6;Side(dance).speedStage=0;
    assert(use(dance).accepted&&Side(dance).attackStage==6&&
           Side(dance).speedStage==1);

    Fixture drum(enemy?splash:static_cast<MoveId>(187),
                 enemy?static_cast<MoveId>(187):splash);
    OwnedPokemon& drummer=enemy?drum.opponent():drum.player();
    drummer.maximumHp=101;drummer.currentHp=101;
    assert(use(drum).accepted&&drummer.currentHp==51U&&
           Side(drum).attackStage==6);

    Fixture psych(enemy?splash:static_cast<MoveId>(244),
                  enemy?static_cast<MoveId>(244):splash);
    CombatVolatile& copied=Target(psych);
    copied.attackStage=-3;copied.defenseStage=2;copied.spAttackStage=4;
    copied.spDefenseStage=-2;copied.speedStage=5;copied.accuracyStage=-4;
    copied.evasionStage=3;
    assert(use(psych).accepted&&Side(psych).attackStage==-3&&
           Side(psych).defenseStage==2&&Side(psych).spAttackStage==4&&
           Side(psych).spDefenseStage==-2&&Side(psych).speedStage==5&&
           Side(psych).accuracyStage==-4&&Side(psych).evasionStage==3);

    Fixture swagger(enemy?splash:static_cast<MoveId>(207),
                    enemy?static_cast<MoveId>(207):splash);
    OwnedPokemon& ownTempo=enemy?swagger.player():swagger.opponent();
    ownTempo.abilityId=20; // OWN TEMPO
    Target(swagger).sureHitTurns=enemy?2U:1U;
    assert(use(swagger).accepted&&Target(swagger).attackStage==2&&
           !Target(swagger).confusionTurns);

    Fixture flatter(enemy?splash:static_cast<MoveId>(260),
                    enemy?static_cast<MoveId>(260):splash);
    if(enemy)flatter.battle.playerSafeguardTurns=3;
    else flatter.battle.opponentSafeguardTurns=3;
    Target(flatter).sureHitTurns=enemy?2U:1U;
    assert(use(flatter).accepted&&Target(flatter).spAttackStage==1&&
           !Target(flatter).confusionTurns);
  }

  // Foresight/Odor Sleuth simply set the identification bit and succeed on
  // repeat use. Skill Swap permits one side to have ABILITY_NONE; only two
  // empty slots (or Wonder Guard) make the retail command fail.
  for(bool enemy:{false,true}){
    const MoveId splash=static_cast<MoveId>(150),foresight=static_cast<MoveId>(193);
    Fixture sight(enemy?splash:foresight,enemy?foresight:splash);
    CombatVolatile& targetVolatile=enemy?sight.battle.playerVolatile:
        sight.battle.opponentVolatiles[0];
    DedicatedMoveEffectState& targetEffects=enemy?sight.battle.playerMoveEffects:
        sight.battle.opponentMoveEffects[0];
    targetVolatile.sureHitTurns=enemy?2U:1U;
    const BattleActionResult first=enemy?sight.opponentUses():sight.playerUses();
    targetVolatile.sureHitTurns=enemy?2U:1U;
    const BattleActionResult second=enemy?sight.opponentUses():sight.playerUses();
    const BattleSide targetSide=enemy?BattleSide::Player:BattleSide::Opponent;
    assert(first.accepted&&second.accepted&&targetEffects.identified&&
           saw(second,BattleMoveEffect::Identified,targetSide)&&
           !saw(second,BattleMoveEffect::Failed,
                enemy?BattleSide::Opponent:BattleSide::Player));

    const MoveId swap=static_cast<MoveId>(285);
    Fixture abilitySwap(enemy?splash:swap,enemy?swap:splash);
    OwnedPokemon& swapUser=enemy?abilitySwap.opponent():abilitySwap.player();
    OwnedPokemon& swapTarget=enemy?abilitySwap.player():abilitySwap.opponent();
    swapUser.abilityId=0;swapTarget.abilityId=2;
    CombatVolatile& swapAim=enemy?abilitySwap.battle.playerVolatile:
        abilitySwap.battle.opponentVolatiles[0];
    swapAim.sureHitTurns=enemy?2U:1U;
    const BattleActionResult swapped=enemy?abilitySwap.opponentUses():
        abilitySwap.playerUses();
    assert(swapped.accepted&&swapUser.abilityId==2&&swapTarget.abilityId==0&&
           saw(swapped,BattleMoveEffect::AbilitiesSwapped,
               enemy?BattleSide::Opponent:BattleSide::Player));
  }

  // Hustle's Gen-III accuracy penalty keys off the old physical-type table,
  // even for power-0 moves. Seed 7 produces an 83 roll: Sand-Attack hits at
  // its normal 100 accuracy but misses at Hustle's reduced 80.
  for(bool enemy:{false,true}){
    const MoveId sandAttack=static_cast<MoveId>(28),splash=static_cast<MoveId>(150);
    Fixture normal(enemy?splash:sandAttack,enemy?sandAttack:splash);
    Fixture hustle(enemy?splash:sandAttack,enemy?sandAttack:splash);
    normal.battle.rngState=hustle.battle.rngState=7U;
    OwnedPokemon& hustler=enemy?hustle.opponent():hustle.player();
    hustler.abilityId=55; // HUSTLE
    CombatVolatile& normalTarget=enemy?normal.battle.playerVolatile:
        normal.battle.opponentVolatiles[0];
    CombatVolatile& hustleTarget=enemy?hustle.battle.playerVolatile:
        hustle.battle.opponentVolatiles[0];
    const BattleActionResult normalResult=enemy?normal.opponentUses():normal.playerUses();
    const BattleActionResult hustleResult=enemy?hustle.opponentUses():hustle.playerUses();
    assert(normalResult.accepted&&hustleResult.accepted&&
           normalTarget.accuracyStage==-1&&hustleTarget.accuracyStage==0&&
           countEvents(hustleResult,BattleEventType::MoveMissed,
                       enemy?BattleSide::Opponent:BattleSide::Player)==1U);
  }

  static Fixture mirror(static_cast<MoveId>(119),MoveId::Tackle);
  mirror.battle.opponentVolatiles[0].lastMoveUsed=MoveId::Tackle;
  const BattleActionResult mirrorResult=mirror.playerUses();
  assert(mirrorResult.accepted&&mirrorResult.damageDealt&&
         sawMove(mirrorResult,MoveId::Tackle,BattleSide::Player));

  // NATURE POWER uses all ten FireRed terrain rows, not a reduced type-based
  // approximation. Execute every row from both sides and inspect the called
  // move written to the event journal.
  const BattleTerrain terrains[]={BattleTerrain::Grass,BattleTerrain::LongGrass,
      BattleTerrain::Sand,BattleTerrain::Underwater,BattleTerrain::Water,
      BattleTerrain::Pond,BattleTerrain::Mountain,BattleTerrain::Cave,
      BattleTerrain::Building,BattleTerrain::Plain};
  const MoveId terrainMoves[]={static_cast<MoveId>(78),static_cast<MoveId>(75),
      static_cast<MoveId>(89),static_cast<MoveId>(56),static_cast<MoveId>(57),
      static_cast<MoveId>(61),static_cast<MoveId>(157),static_cast<MoveId>(247),
      static_cast<MoveId>(129),static_cast<MoveId>(129)};
  for(bool enemy:{false,true})for(uint8_t index=0;index<10U;++index){
    Fixture nature(enemy?MoveId::Tackle:static_cast<MoveId>(267),
                   enemy?static_cast<MoveId>(267):MoveId::Tackle);
    nature.battle.terrain=terrains[index];
    nature.player().maximumHp=nature.player().currentHp=5000;
    nature.opponent().maximumHp=nature.opponent().currentHp=5000;
    const BattleActionResult natureResult=enemy?nature.opponentUses():nature.playerUses();
    assert(natureResult.accepted&&sawMove(natureResult,terrainMoves[index],
           enemy?BattleSide::Opponent:BattleSide::Player));
  }

  // Gen III only applies post-damage effects when the attack affected the
  // target, and a Substitute shields the held item behind it.  THIEF/COVET
  // and KNOCK OFF therefore cannot touch that item through a doll.
  for(bool enemy:{false,true})for(MoveId itemMove:{static_cast<MoveId>(168),
                                                   static_cast<MoveId>(282)}){
    const MoveId splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:itemMove,enemy?itemMove:splash);
    OwnedPokemon& target=enemy?f.player():f.opponent();
    CombatVolatile& targetState=enemy?f.battle.playerVolatile:
        f.battle.opponentVolatiles[0];
    target.maximumHp=target.currentHp=5000;
    target.heldItem=HeldItem::Leftovers;
    targetState.substituteHp=1000;
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    assert(r.accepted&&target.heldItem==HeldItem::Leftovers&&
           !r.heldItemStolen&&!r.heldItemKnockedOff&&
           !targetState.heldItemSuppressed);
  }

  // BIND/WRAP assign 3..6 turns in the retail engine.  The first residual
  // tick happens at the end of this public action, so the observable timer is
  // 2..5.  A Normal trap must have no effect at all on a Ghost target.
  for(bool enemy:{false,true}){
    const MoveId bind=static_cast<MoveId>(20),splash=static_cast<MoveId>(150);
    Fixture trapped(enemy?splash:bind,enemy?bind:splash);
    OwnedPokemon& target=enemy?trapped.player():trapped.opponent();
    CombatVolatile& targetState=enemy?trapped.battle.playerVolatile:
        trapped.battle.opponentVolatiles[0];
    CombatVolatile& guaranteedTarget=enemy?trapped.battle.playerVolatile:
        trapped.battle.opponentVolatiles[0];
    guaranteedTarget.sureHitTurns=enemy?2U:1U;
    target.maximumHp=target.currentHp=5000;
    const BattleActionResult hit=enemy?trapped.opponentUses():trapped.playerUses();
    assert(hit.accepted&&targetState.trappedTurns>=2U&&targetState.trappedTurns<=5U);

    Fixture immune(enemy?splash:bind,enemy?bind:splash);
    OwnedPokemon& ghost=enemy?immune.player():immune.opponent();
    const uint32_t ghostUid=ghost.uid;
    ghost=CollectionLogic::createPokemon(ghostUid,92,50,false,0xB17DU);
    ghost.maximumHp=ghost.currentHp=5000;
    CombatVolatile& guaranteedGhost=enemy?immune.battle.playerVolatile:
        immune.battle.opponentVolatiles[0];
    guaranteedGhost.sureHitTurns=enemy?2U:1U;
    const BattleActionResult noEffect=enemy?immune.opponentUses():immune.playerUses();
    const CombatVolatile& ghostState=enemy?immune.battle.playerVolatile:
        immune.battle.opponentVolatiles[0];
    assert(noEffect.accepted&&ghostState.trappedTurns==0U);
  }

  // Cmd_rapidspinfree is an ordered callback chain in FireRed/Emerald.  It
  // returns to the same command after each message, therefore one successful
  // Rapid Spin clears Wrap, Leech Seed and the user's Spikes in the same move.
  // It must not run when Rapid Spin is ineffective. Verify both sides.
  for(bool enemy:{false,true}){
    const MoveId spin=static_cast<MoveId>(229),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:spin,enemy?spin:splash);
    CombatVolatile& spinner=enemy?f.battle.opponentVolatiles[0]:
        f.battle.playerVolatile;
    spinner.trappedTurns=3;spinner.seeded=true;
    uint8_t& spikes=enemy?f.battle.opponentSpikesLayers:f.battle.playerSpikesLayers;
    spikes=2;
    OwnedPokemon& target=enemy?f.player():f.opponent();
    target.maximumHp=target.currentHp=5000;
    const BattleActionResult cleared=enemy?f.opponentUses():f.playerUses();
    assert(cleared.accepted&&!spinner.trappedTurns&&!spinner.seeded&&spikes==0U);

    Fixture immune(enemy?splash:spin,enemy?spin:splash);
    CombatVolatile& immuneSpinner=enemy?immune.battle.opponentVolatiles[0]:
        immune.battle.playerVolatile;
    immuneSpinner.seeded=true;
    uint8_t& immuneSpikes=enemy?immune.battle.opponentSpikesLayers:
        immune.battle.playerSpikesLayers;
    immuneSpikes=2;
    OwnedPokemon& ghost=enemy?immune.player():immune.opponent();
    const uint32_t ghostUid=ghost.uid;
    ghost=CollectionLogic::createPokemon(ghostUid,92,50,false,0x5A11U);
    ghost.maximumHp=ghost.currentHp=5000;
    const BattleActionResult noEffect=enemy?immune.opponentUses():immune.playerUses();
    assert(noEffect.accepted&&immuneSpinner.seeded&&immuneSpikes==2U);
  }

  // Creating a Substitute immediately frees the user from Wrap/Bind in the
  // original command, before end-of-turn residual processing.
  for(bool enemy:{false,true}){
    const MoveId substitute=static_cast<MoveId>(164),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:substitute,enemy?substitute:splash);
    OwnedPokemon& user=enemy?f.opponent():f.player();
    CombatVolatile& userState=enemy?f.battle.opponentVolatiles[0]:
        f.battle.playerVolatile;
    user.maximumHp=user.currentHp=5000;userState.trappedTurns=3;
    const BattleActionResult r=enemy?f.opponentUses():f.playerUses();
    assert(r.accepted&&userState.substituteHp==1250U&&!userState.trappedTurns);

    // The retail command explicitly clamps maxHP/4 to one.  Synthetic tiny
    // HP values are useful here because normal species never reach this edge.
    Fixture tiny(enemy?splash:substitute,enemy?substitute:splash);
    OwnedPokemon& tinyUser=enemy?tiny.opponent():tiny.player();
    CombatVolatile& tinyState=enemy?tiny.battle.opponentVolatiles[0]:
        tiny.battle.playerVolatile;
    tinyUser.maximumHp=3;tinyUser.currentHp=2;
    const BattleActionResult tinyResult=enemy?tiny.opponentUses():tiny.playerUses();
    assert(tinyResult.accepted&&tinyUser.currentHp==1U&&tinyState.substituteHp==1U);
  }

  // PAY DAY only scatters coins from the player's side and uses FireRed's
  // u16 saturation.  An enemy Meowth may still deal damage with Pay Day, but
  // it cannot manufacture prize money for the player.
  {
    const MoveId payDay=static_cast<MoveId>(6),splash=static_cast<MoveId>(150);
    Fixture playerCoins(payDay,splash);
    playerCoins.battle.payDayMoney=65400U;
    playerCoins.player().level=100;
    const BattleActionResult playerResult=playerCoins.playerUses();
    assert(playerResult.accepted&&playerCoins.battle.payDayMoney==0xFFFFU&&
           saw(playerResult,BattleMoveEffect::CoinsScattered,BattleSide::Player));

    Fixture enemyCoins(splash,payDay);
    enemyCoins.battle.payDayMoney=123U;
    const BattleActionResult enemyResult=enemyCoins.opponentUses();
    assert(enemyResult.accepted&&enemyCoins.battle.payDayMoney==123U&&
           !saw(enemyResult,BattleMoveEffect::CoinsScattered,BattleSide::Opponent));
  }

  // SPITE fails against a move with only one PP remaining. A successful
  // reduction to zero also executes CancelMultiTurnMoves for the target.
  for(bool enemy:{false,true}){
    const MoveId spite=static_cast<MoveId>(180),tackle=MoveId::Tackle;
    Fixture onePp(enemy?tackle:spite,enemy?spite:tackle);
    OwnedPokemon& oneTarget=enemy?onePp.player():onePp.opponent();
    CombatVolatile& oneTargetVolatile=enemy?onePp.battle.playerVolatile:
        onePp.battle.opponentVolatiles[0];
    oneTarget.moves[0]=tackle;oneTarget.movePp[0]=1;
    oneTargetVolatile.lastMoveUsed=tackle;oneTargetVolatile.flinched=true;
    const BattleActionResult failedSpite=enemy?onePp.opponentUses():onePp.playerUses();
    assert(oneTarget.movePp[0]==1U&&saw(failedSpite,BattleMoveEffect::Failed,
           enemy?BattleSide::Opponent:BattleSide::Player));

    Fixture zeroed(enemy?tackle:spite,enemy?spite:tackle);
    OwnedPokemon& target=enemy?zeroed.player():zeroed.opponent();
    CombatVolatile& targetVolatile=enemy?zeroed.battle.playerVolatile:
        zeroed.battle.opponentVolatiles[0];
    DedicatedMoveEffectState& targetEffects=enemy?zeroed.battle.playerMoveEffects:
        zeroed.battle.opponentMoveEffects[0];
    target.moves[0]=tackle;target.movePp[0]=2;
    targetVolatile.lastMoveUsed=tackle;targetVolatile.chargingMove=tackle;
    targetVolatile.flinched=true;
    targetEffects.lockedMove=tackle;targetEffects.lockedMoveTurns=3;
    const BattleActionResult reduced=enemy?zeroed.opponentUses():zeroed.playerUses();
    assert(reduced.accepted&&target.movePp[0]==0U&&
           targetVolatile.chargingMove==MoveId::None&&
           targetEffects.lockedMoveTurns==0U);
  }

  // Taunt's timer is exactly two in the source. The end-turn pass of the
  // setup turn consumes one tick, so only one remains afterwards.
  for(bool enemy:{false,true}){
    const MoveId taunt=static_cast<MoveId>(269),splash=static_cast<MoveId>(150);
    Fixture f(enemy?splash:taunt,enemy?taunt:splash);
    DedicatedMoveEffectState& targetEffects=enemy?f.battle.playerMoveEffects:
        f.battle.opponentMoveEffects[0];
    const BattleActionResult applied=enemy?f.opponentUses():f.playerUses();
    assert(applied.accepted&&targetEffects.tauntTurns==1U);
  }

  // End-turn move effects are one ordered pipeline even when the attempted
  // action is stopped. Toxic rounds maxHP/16 before multiplying its counter;
  // Leech Seed transfers only HP actually removed and Liquid Ooze reverses
  // that transfer; an expiring Wrap/Bind timer does not deal one last tick.
  for(bool enemy:{false,true}){
    const MoveId splash=static_cast<MoveId>(150);
    Fixture interrupted(splash,splash);
    OwnedPokemon& poisoned=enemy?interrupted.opponent():interrupted.player();
    CombatVolatile& poisonedState=enemy?interrupted.battle.opponentVolatiles[0]:
        interrupted.battle.playerVolatile;
    poisoned.maximumHp=poisoned.currentHp=160;
    poisoned.status=StatusCondition::Poison;poisonedState.flinched=true;
    const BattleActionResult poisonTurn=enemy?interrupted.opponentUses():
        interrupted.playerUses();
    assert(poisonTurn.accepted&&poisoned.currentHp==140U);

    Fixture toxic(splash,splash);
    OwnedPokemon& toxicMon=enemy?toxic.opponent():toxic.player();
    CombatVolatile& toxicState=enemy?toxic.battle.opponentVolatiles[0]:
        toxic.battle.playerVolatile;
    toxicMon.maximumHp=toxicMon.currentHp=31;
    toxicMon.status=StatusCondition::BadlyPoisoned;toxicState.toxicCounter=1;
    (enemy?toxic.opponentUses():toxic.playerUses());
    assert(toxicState.toxicCounter==2U&&toxicMon.currentHp==29U);

    Fixture seed(splash,splash);
    OwnedPokemon& seeded=enemy?seed.opponent():seed.player();
    OwnedPokemon& receiver=enemy?seed.player():seed.opponent();
    CombatVolatile& seedState=enemy?seed.battle.opponentVolatiles[0]:
        seed.battle.playerVolatile;
    seeded.maximumHp=80;seeded.currentHp=1;seedState.seeded=true;
    receiver.maximumHp=100;receiver.currentHp=50;
    (enemy?seed.opponentUses():seed.playerUses());
    assert(!seeded.currentHp&&receiver.currentHp==51U);

    Fixture ooze(splash,splash);
    OwnedPokemon& oozeTarget=enemy?ooze.opponent():ooze.player();
    OwnedPokemon& oozeDrainer=enemy?ooze.player():ooze.opponent();
    CombatVolatile& oozeState=enemy?ooze.battle.opponentVolatiles[0]:
        ooze.battle.playerVolatile;
    oozeTarget.maximumHp=80;oozeTarget.currentHp=1;oozeTarget.abilityId=64;
    oozeState.seeded=true;
    oozeDrainer.maximumHp=oozeDrainer.currentHp=100;
    (enemy?ooze.opponentUses():ooze.playerUses());
    assert(!oozeTarget.currentHp&&oozeDrainer.currentHp==99U);

    Fixture wrapEnd(splash,splash);
    OwnedPokemon& wrapped=enemy?wrapEnd.opponent():wrapEnd.player();
    CombatVolatile& wrapState=enemy?wrapEnd.battle.opponentVolatiles[0]:
        wrapEnd.battle.playerVolatile;
    wrapped.maximumHp=wrapped.currentHp=160;wrapState.trappedTurns=1;
    (enemy?wrapEnd.opponentUses():wrapEnd.playerUses());
    assert(!wrapState.trappedTurns&&wrapped.currentHp==160U);
  }

  static Fixture sleepTalk(static_cast<MoveId>(214),MoveId::Tackle);
  sleepTalk.player().moves[1]=MoveId::Tackle;sleepTalk.player().movePp[1]=35;
  sleepTalk.player().moves[2]=sleepTalk.player().moves[3]=MoveId::None;
  sleepTalk.player().movePp[2]=sleepTalk.player().movePp[3]=0;
  sleepTalk.player().status=StatusCondition::Sleep;
  sleepTalk.battle.playerVolatile.sleepTurns=3;
  const BattleActionResult sleepTalkResult=sleepTalk.playerUses();
  assert(sleepTalkResult.accepted&&sleepTalkResult.damageDealt&&
         sawMove(sleepTalkResult,MoveId::Tackle,BattleSide::Player));

  std::cout<<"Dedicated move-effect matrix passed.\n";
  return 0;
}
