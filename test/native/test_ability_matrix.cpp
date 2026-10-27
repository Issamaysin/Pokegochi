#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "game/BattleEngine.h"
#include "game/Collection.h"
#include "game/EggSystem.h"
#include "game/PokemonData.h"

namespace {
struct Fixture {
  PokemonCollection collection{};
  BattleState battle{};
  explicit Fixture(MoveId move=MoveId::Tackle) {
    CollectionLogic::initialize(collection);
    assert(CollectionLogic::chooseStarter(collection,1,0xAB110001U));
    OwnedPokemon* owned=CollectionLogic::active(collection,0);
    const uint32_t uid=owned->uid;
    *owned=CollectionLogic::createPokemon(uid,137,50,false,0xAB110002U);
    owned->moves[0]=move;
    owned->movePp[0]=findFullMove(move)?findFullMove(move)->pp:20U;
    BattleEngine::clear(battle);
    battle.active=true;battle.kind=BattleKind::Pvp;
    battle.outcome=BattleOutcome::Ongoing;battle.playerUid=uid;
    battle.opponentCount=1;battle.rngState=0xAB110003U;
    battle.opponents[0]=CollectionLogic::createPokemon(
        0xF1AB1101U,137,50,false,0xAB110004U);
    for(uint8_t slot=0;slot<kMoveSlots;++slot){
      battle.opponents[0].moves[slot]=MoveId::None;
      battle.opponents[0].movePp[slot]=0;
    }
  }
  OwnedPokemon& player(){return *CollectionLogic::find(collection,battle.playerUid);}
  OwnedPokemon& opponent(){return battle.opponents[battle.opponentIndex];}
  BattleActionResult act(){return BattleEngine::fightPvp(battle,collection,0,0xFEU);}
};

bool saw(const BattleActionResult& result,BattleEventType type,BattleSide side,
         uint16_t value=UINT16_MAX){
  for(uint8_t index=0;index<result.eventCount;++index){
    const BattleEvent& event=result.events[index];
    if(event.type==type&&event.side==side&&
       (value==UINT16_MAX||event.value==value))return true;
  }
  return false;
}

const BattleEvent* moveEvent(const BattleActionResult& result,
                             BattleSide side,MoveId move){
  for(uint8_t index=0;index<result.eventCount;++index){
    const BattleEvent& event=result.events[index];
    if(event.type==BattleEventType::MoveUsed&&event.side==side&&
       event.move==move)return &event;
  }
  return nullptr;
}

BattleSide firstMoveSide(const BattleActionResult& result){
  for(uint8_t index=0;index<result.eventCount;++index)
    if(result.events[index].type==BattleEventType::MoveUsed)
      return result.events[index].side;
  assert(false&&"expected a MoveUsed event");
  return BattleSide::Player;
}

MoveId moveNamed(const char* name){
  for(uint16_t id=1;id<=559U;++id){
    const FullMoveData* move=findFullMove(static_cast<MoveId>(id));
    if(move&&std::strcmp(move->name,name)==0)return static_cast<MoveId>(id);
  }
  assert(false&&"move name missing from generated table");
  return MoveId::None;
}

uint8_t abilityNamed(const char* name){
  for(uint16_t id=1;id<=255U;++id){
    const AbilityData* ability=findAbility(static_cast<uint8_t>(id));
    if(ability&&std::strcmp(ability->name,name)==0)
      return static_cast<uint8_t>(id);
  }
  assert(false&&"ability name missing from generated/custom table");
  return 0;
}

uint16_t damageFor(Fixture& fixture,MoveId move){
  fixture.player().moves[0]=move;
  fixture.player().movePp[0]=findFullMove(move)?findFullMove(move)->pp:20U;
  fixture.opponent().maximumHp=fixture.opponent().currentHp=60000U;
  const uint16_t before=fixture.opponent().currentHp;
  fixture.act();
  if(fixture.opponent().currentHp==before&&
     fixture.battle.playerVolatile.chargingMove==move)
    fixture.act();
  return static_cast<uint16_t>(before-fixture.opponent().currentHp);
}
}

int main(){
  // Switching one side must never reactivate the other side's entry Ability.
  {
    Fixture f;
    f.player().abilityId=22; // INTIMIDATE
    BattleEngine::applyEntryAbilities(f.battle,f.collection);
    assert(f.battle.opponentVolatiles[0].attackStage==-1);
    BattleActionResult result{};
    BattleEngine::applyEntryAbilities(f.battle,f.collection,
                                      BattleEntryScope::Opponent,&result);
    assert(f.battle.opponentVolatiles[0].attackStage==-1);
  }

  // Truant's cadence is local to the battler and starts over after entry.
  {
    Fixture f;
    f.player().abilityId=54; // TRUANT
    assert(f.act().accepted);
    const BattleActionResult loaf=f.act();
    assert(saw(loaf,BattleEventType::CannotMove,BattleSide::Player,9U));
    Fixture switched;
    switched.player().abilityId=54;
    switched.battle.turn=10;
    switched.battle.playerMoveEffects.enteredTurn=10;
    const BattleActionResult first=switched.act();
    assert(!saw(first,BattleEventType::CannotMove,BattleSide::Player,9U));
    const BattleActionResult second=switched.act();
    assert(saw(second,BattleEventType::CannotMove,BattleSide::Player,9U));
  }

  // Early Bird consumes two sleep ticks per attempted action, not half the
  // remaining counter on every turn.
  {
    Fixture f;
    f.player().abilityId=48; // EARLY BIRD
    f.player().status=StatusCondition::Sleep;
    f.battle.playerVolatile.sleepTurns=4;
    assert(f.act().accepted&&f.battle.playerVolatile.sleepTurns==2U&&
           f.player().status==StatusCondition::Sleep);
    const BattleActionResult wake=f.act();
    assert(f.player().status==StatusCondition::None&&
           saw(wake,BattleEventType::StatusCured,BattleSide::Player));
  }

  // Trace/Skill Swap-style acquisition immediately cures forbidden state.
  {
    Fixture f;
    f.player().abilityId=36; // TRACE
    f.player().status=StatusCondition::Poison;
    f.opponent().abilityId=17; // IMMUNITY
    BattleActionResult result{};
    BattleEngine::applyEntryAbilities(f.battle,f.collection,
                                      BattleEntryScope::Player,&result);
    assert(f.player().abilityId==17&&f.player().status==StatusCondition::None&&
           saw(result,BattleEventType::StatusCured,BattleSide::Player));
  }
  {
    Fixture f;
    f.player().abilityId=36; // TRACE
    f.battle.playerVolatile.confusionTurns=3;
    f.opponent().abilityId=20; // OWN TEMPO
    BattleActionResult result{};
    BattleEngine::applyEntryAbilities(f.battle,f.collection,
                                      BattleEntryScope::Player,&result);
    assert(f.battle.playerVolatile.confusionTurns==0&&
           saw(result,BattleEventType::MoveEffect,BattleSide::Player,
               static_cast<uint16_t>(BattleMoveEffect::AbilityCured)));
  }

  // FireRed resolves native switch-in effects before Trace. Copying an
  // entry-only Ability does not retroactively enqueue it during that pass.
  {
    Fixture f;
    f.player().abilityId=36; // TRACE
    f.opponent().abilityId=22; // INTIMIDATE
    BattleEngine::applyEntryAbilities(f.battle,f.collection,
                                      BattleEntryScope::Player);
    assert(f.player().abilityId==22&&
           f.battle.opponentVolatiles[0].attackStage==0);
  }
  {
    Fixture f;
    f.player().abilityId=36; // TRACE
    f.opponent().abilityId=2; // DRIZZLE
    BattleEngine::applyEntryAbilities(f.battle,f.collection,
                                      BattleEntryScope::Player);
    assert(f.player().abilityId==2&&f.battle.weather==BattleWeather::Clear);
  }
  {
    Fixture f;
    const uint32_t playerUid=f.player().uid;
    f.player()=CollectionLogic::createPokemon(playerUid,79,50,false,0xAB110008U); // SLOWPOKE
    f.opponent()=CollectionLogic::createPokemon(
        0xF1AB1101U,291,50,false,0xAB110009U); // NINJASK
    f.player().abilityId=2; // slower DRIZZLE
    f.opponent().abilityId=70; // faster DROUGHT
    BattleEngine::applyEntryAbilities(f.battle,f.collection);
    assert(f.battle.weather==BattleWeather::Rain);
  }

  // Mold Breaker must apply in accuracy/effect code as well as raw damage.
  {
    Fixture blocked(static_cast<MoveId>(45)); // GROWL
    blocked.opponent().abilityId=43; // SOUNDPROOF
    const BattleActionResult protectedResult=blocked.act();
    assert(blocked.battle.opponentVolatiles[0].attackStage==0&&
           saw(protectedResult,BattleEventType::AbilityActivated,
               BattleSide::Opponent,43U));
    Fixture bypass(static_cast<MoveId>(45));
    bypass.player().abilityId=abilityNamed("MOLD BREAKER");
    bypass.opponent().abilityId=43;
    bypass.act();
    assert(bypass.battle.opponentVolatiles[0].attackStage==-1);
  }

  // A copied Flash Fire must cancel the complete Fire move, not merely its
  // damage. In particular a non-Fire holder cannot be burned by Will-O-Wisp.
  {
    bool connected=false;
    for(uint32_t seed=1;seed<128U&&!connected;++seed){
      Fixture f(static_cast<MoveId>(261)); // WILL-O-WISP
      f.battle.rngState=seed;
      f.opponent().abilityId=18; // FLASH FIRE on neutral Porygon
      const BattleActionResult result=f.act();
      if(saw(result,BattleEventType::AbilityActivated,BattleSide::Opponent,18U)){
        connected=true;
        assert(f.opponent().status==StatusCondition::None&&
               f.battle.opponentMoveEffects[0].flashFireBoost);
      }
    }
    assert(connected);
  }

  // Major-status immunities announce the actual blocking Ability, and
  // Synchronize converts reflected Toxic into regular poison in Gen III.
  {
    Fixture f(static_cast<MoveId>(86)); // THUNDER WAVE
    f.opponent().abilityId=7; // LIMBER
    const BattleActionResult result=f.act();
    assert(f.opponent().status==StatusCondition::None&&
           saw(result,BattleEventType::AbilityActivated,BattleSide::Opponent,7U));
  }
  {
    bool reflected=false;
    for(uint32_t seed=1;seed<128U&&!reflected;++seed){
      Fixture f(static_cast<MoveId>(92)); // TOXIC
      f.battle.rngState=seed;
      f.opponent().abilityId=28; // SYNCHRONIZE
      const BattleActionResult result=f.act();
      if(f.opponent().status==StatusCondition::BadlyPoisoned){
        reflected=true;
        assert(f.player().status==StatusCondition::Poison&&
               saw(result,BattleEventType::AbilityActivated,
                   BattleSide::Opponent,28U)&&
               saw(result,BattleEventType::StatusApplied,BattleSide::Player));
      }
    }
    assert(reflected);
  }

  // Clear Body/White Smoke/Hyper Cutter/Keen Eye use the shared stat-drop
  // gate. A blocked drop must leave the stage intact and identify the Ability.
  {
    Fixture f(static_cast<MoveId>(39)); // TAIL WHIP
    f.opponent().abilityId=29; // CLEAR BODY
    const BattleActionResult result=f.act();
    assert(f.battle.opponentVolatiles[0].defenseStage==0&&
           saw(result,BattleEventType::AbilityActivated,
               BattleSide::Opponent,29U));
  }

  // Damp blocks both damage and the user's self-KO and reports whichever
  // battler supplied the Ability.
  {
    Fixture f(static_cast<MoveId>(153)); // EXPLOSION
    f.opponent().abilityId=6; // DAMP
    const uint16_t playerBefore=f.player().currentHp;
    const uint16_t opponentBefore=f.opponent().currentHp;
    const BattleActionResult result=f.act();
    assert(f.player().currentHp==playerBefore&&f.opponent().currentHp==opponentBefore&&
           saw(result,BattleEventType::AbilityActivated,
               BattleSide::Opponent,6U));
  }

  // Contact with an unrelated Ability must not secretly consume an extra RNG
  // value. That would desynchronise seeded PvP/replays after an ordinary hit.
  {
    Fixture none(MoveId::Tackle);
    Fixture unrelated(MoveId::Tackle);
    unrelated.opponent().abilityId=53; // PICKUP has no in-turn contact effect
    none.act();unrelated.act();
    assert(none.battle.rngState==unrelated.battle.rngState);
  }
  {
    Fixture f(static_cast<MoveId>(89)); // EARTHQUAKE
    f.player().abilityId=abilityNamed("MOLD BREAKER");
    f.opponent().abilityId=26; // LEVITATE
    const uint16_t before=f.opponent().currentHp;
    f.act();
    assert(f.opponent().currentHp<before);
  }

  // Mold Breaker only suppresses Abilities marked breakable by the retail
  // engine. Post-move contact effects and Liquid Ooze still activate.
  {
    Fixture f(MoveId::Tackle);
    f.player().abilityId=abilityNamed("MOLD BREAKER");
    f.opponent().abilityId=24; // ROUGH SKIN (not breakable)
    const uint16_t before=f.player().currentHp;
    f.act();
    assert(f.player().currentHp==static_cast<uint16_t>(before-
           std::max<uint16_t>(1U,f.player().maximumHp/16U)));
  }
  {
    Fixture f(static_cast<MoveId>(71)); // ABSORB
    f.player().abilityId=abilityNamed("MOLD BREAKER");
    f.opponent().abilityId=64; // LIQUID OOZE (not breakable)
    const uint16_t before=f.player().currentHp;
    f.act();
    assert(f.player().currentHp<before);
  }
  {
    Fixture f(static_cast<MoveId>(86)); // THUNDER WAVE
    f.player().abilityId=abilityNamed("MOLD BREAKER");
    f.opponent().abilityId=abilityNamed("MAGIC BOUNCE");
    f.act();
    assert(f.opponent().status==StatusCondition::Paralysis&&
           f.player().status==StatusCondition::None);
  }

  // FireRed executes MOVEEND_NEXT_TARGET between multi-hit strikes. Contact
  // and on-damage Abilities consequently resolve once for each successful
  // hit, including type changes which affect the remaining damage rolls.
  {
    Fixture f(static_cast<MoveId>(24)); // DOUBLE KICK
    f.opponent().abilityId=24; // ROUGH SKIN
    f.opponent().maximumHp=f.opponent().currentHp=1000;
    const uint16_t before=f.player().currentHp;
    f.act();
    const uint16_t recoil=std::max<uint16_t>(1U,f.player().maximumHp/16U);
    assert(f.player().currentHp==static_cast<uint16_t>(before-2U*recoil));
  }
  {
    Fixture ordinary(static_cast<MoveId>(24)); // DOUBLE KICK
    ordinary.opponent().maximumHp=ordinary.opponent().currentHp=1000;
    const uint16_t ordinaryBefore=ordinary.opponent().currentHp;
    ordinary.act();
    const uint16_t ordinaryDamage=static_cast<uint16_t>(ordinaryBefore-
        ordinary.opponent().currentHp);
    Fixture colorChange(static_cast<MoveId>(24));
    colorChange.opponent().abilityId=16;
    colorChange.opponent().maximumHp=colorChange.opponent().currentHp=1000;
    const uint16_t colorBefore=colorChange.opponent().currentHp;
    colorChange.act();
    assert(colorBefore-colorChange.opponent().currentHp<ordinaryDamage&&
           colorChange.battle.opponentMoveEffects[0].typeOverrideActive&&
           colorChange.battle.opponentMoveEffects[0].type1==PokemonType::Fighting);
  }

  // Entry stat drops respect both Substitute and Mist.
  {
    Fixture substitute;
    substitute.player().abilityId=22;
    substitute.battle.opponentVolatiles[0].substituteHp=10;
    BattleEngine::applyEntryAbilities(substitute.battle,substitute.collection,
                                      BattleEntryScope::Player);
    assert(substitute.battle.opponentVolatiles[0].attackStage==0);
    Fixture mist;
    mist.player().abilityId=22;
    mist.battle.opponentMistTurns=3;
    BattleEngine::applyEntryAbilities(mist.battle,mist.collection,
                                      BattleEntryScope::Player);
    assert(mist.battle.opponentVolatiles[0].attackStage==0);
  }

  // No Guard owned by either side reaches semi-invulnerable positions.
  {
    Fixture f(MoveId::Tackle);
    f.player().abilityId=abilityNamed("NO GUARD");
    f.battle.opponentVolatiles[0].chargingMove=static_cast<MoveId>(19); // FLY
    const uint16_t before=f.opponent().currentHp;
    f.act();
    assert(f.opponent().currentHp<before);
  }

  // Pressure only taxes moves which affect the opposing side.
  {
    Fixture self(static_cast<MoveId>(14)); // SWORDS DANCE
    self.opponent().abilityId=46;
    const uint8_t before=self.player().movePp[0];
    self.act();
    assert(self.player().movePp[0]+1U==before);
    Fixture targeted(MoveId::Tackle);
    targeted.opponent().abilityId=46;
    const uint8_t targetedBefore=targeted.player().movePp[0];
    targeted.act();
    assert(targeted.player().movePp[0]+2U==targetedBefore);
  }

  // Volt/Water Absorb only trigger on damaging moves in Generation III.
  {
    Fixture f(static_cast<MoveId>(86)); // THUNDER WAVE
    f.opponent().abilityId=10; // VOLT ABSORB
    f.opponent().currentHp=10;
    f.opponent().maximumHp=100;
    f.act();
    assert(f.opponent().currentHp==10);
  }
  {
    Fixture f(MoveId::WaterGun);
    f.opponent().abilityId=11; // WATER ABSORB
    f.opponent().maximumHp=100;f.opponent().currentHp=25;
    const BattleActionResult result=f.act();
    assert(f.opponent().currentHp==50&&
           saw(result,BattleEventType::AbilityActivated,BattleSide::Opponent,11U));
  }

  // Flash Fire is inactive while its holder is frozen.
  {
    Fixture f(MoveId::Ember);
    f.opponent().abilityId=18;
    f.opponent().status=StatusCondition::Frozen;
    const uint16_t before=f.opponent().currentHp;
    f.act();
    assert(f.opponent().currentHp<before);
  }

  // Parental Bond is represented as two ordered strikes, with the second at
  // one quarter power, rather than one opaque 1.25x damage lump.
  {
    Fixture f(MoveId::Tackle);
    f.player().abilityId=abilityNamed("PARENTAL BOND");
    const BattleActionResult result=f.act();
    assert(saw(result,BattleEventType::MultiHitStrike,BattleSide::Player)&&
           saw(result,BattleEventType::MultiHitCount,BattleSide::Player,2U));
  }

  // Speed Boost skips the holder's entry turn and then announces each rise.
  {
    Fixture f(static_cast<MoveId>(150)); // SPLASH
    f.player().abilityId=3;
    f.act();
    assert(f.battle.playerVolatile.speedStage==0);
    const BattleActionResult boosted=f.act();
    assert(f.battle.playerVolatile.speedStage==1&&
           saw(boosted,BattleEventType::StatChanged,BattleSide::Player));
  }

  // Shed Skin removes the complete condition payload. In particular a cured
  // sleep cannot retain a stale counter or Nightmare marker.
  {
    bool cured=false;
    for(uint32_t seed=1;seed<256U&&!cured;++seed){
      Fixture f(static_cast<MoveId>(150)); // SPLASH
      f.battle.rngState=seed;
      f.player().abilityId=61; // SHED SKIN
      f.player().status=StatusCondition::Sleep;
      f.battle.playerVolatile.sleepTurns=5;
      f.battle.playerNightmare=true;
      const BattleActionResult result=f.act();
      if(f.player().status==StatusCondition::None){
        cured=true;
        assert(f.battle.playerVolatile.sleepTurns==0&&!f.battle.playerNightmare&&
               saw(result,BattleEventType::StatusCured,BattleSide::Player));
      }
    }
    assert(cured);
  }

  // Effect Spore sleep must carry a real 2-5 turn counter rather than wake
  // immediately on the next command.
  {
    bool slept=false;
    for(uint32_t seed=1;seed<2048U&&!slept;++seed){
      Fixture f(MoveId::Tackle);
      f.battle.rngState=seed;
      f.opponent().abilityId=27;
      f.act();
      if(f.player().status==StatusCondition::Sleep){
        slept=true;
        assert(f.battle.playerVolatile.sleepTurns>=2U&&
               f.battle.playerVolatile.sleepTurns<=5U);
      }
    }
    assert(slept);
  }

  // Natural Cure uses the current battle Ability at the exact moment the
  // Pokemon leaves, including an Ability copied with Trace.
  {
    Fixture f(MoveId::Tackle);
    f.player().abilityId=36; // TRACE
    f.opponent().abilityId=30; // NATURAL CURE
    BattleEngine::applyEntryAbilities(f.battle,f.collection,
                                      BattleEntryScope::Player);
    f.player().status=StatusCondition::Poison;
    f.opponent().currentHp=1;
    f.act();
    assert(f.player().status==StatusCondition::None&&f.player().abilityId==36);
  }

  // Temporary opponent Abilities copied by Trace/Role Play/Skill Swap end
  // when that battler switches out.
  {
    Fixture f;
    f.player().abilityId=17; // IMMUNITY
    f.opponent().abilityId=36; // TRACE
    f.battle.opponentCount=2;
    f.battle.opponents[1]=CollectionLogic::createPokemon(
        0xF1AB1102U,25,50,false,0xAB110005U);
    BattleEngine::applyEntryAbilities(f.battle,f.collection,
                                      BattleEntryScope::Opponent);
    assert(f.opponent().abilityId==17);
    const BattleActionResult switched=BattleEngine::switchOpponentPvp(
        f.battle,f.collection,1);
    assert(switched.accepted&&f.battle.opponents[1].abilityId==36);
  }

  // Strong weather ends with its final source; ordinary entry weather is not
  // tied to the source in Generation III.
  {
    Fixture f;
    f.player().abilityId=abilityNamed("PRIMORDIAL SEA");
    f.opponent().abilityId=0;
    BattleEngine::applyEntryAbilities(f.battle,f.collection);
    assert(f.battle.weather==BattleWeather::HeavyRain);
    f.player().abilityId=0;
    BattleEngine::applyEntryAbilities(f.battle,f.collection,
                                      BattleEntryScope::Player);
    assert(f.battle.weather==BattleWeather::Clear);
  }
  {
    Fixture f(static_cast<MoveId>(240)); // RAIN DANCE
    f.battle.weather=BattleWeather::HeavyRain;
    f.act();
    assert(f.battle.weather==BattleWeather::HeavyRain);
  }
  {
    Fixture f;
    f.player().abilityId=abilityNamed("PRIMORDIAL SEA");
    f.opponent().abilityId=2; // DRIZZLE cannot replace strong weather
    f.battle.weather=BattleWeather::HeavyRain;
    BattleEngine::applyEntryAbilities(f.battle,f.collection,
                                      BattleEntryScope::Opponent);
    assert(f.battle.weather==BattleWeather::HeavyRain);
  }
  {
    Fixture clear(static_cast<MoveId>(58)); // ICE BEAM
    clear.opponent()=CollectionLogic::createPokemon(
        0xF1AB1101U,227,50,false,0xAB110004U); // SKARMORY
    for(uint8_t slot=0;slot<kMoveSlots;++slot){
      clear.opponent().moves[slot]=MoveId::None;clear.opponent().movePp[slot]=0;
    }
    const uint16_t clearBefore=clear.opponent().currentHp;
    clear.act();
    const uint16_t clearDamage=static_cast<uint16_t>(clearBefore-clear.opponent().currentHp);
    Fixture winds(static_cast<MoveId>(58));
    winds.opponent()=CollectionLogic::createPokemon(
        0xF1AB1101U,227,50,false,0xAB110004U);
    for(uint8_t slot=0;slot<kMoveSlots;++slot){
      winds.opponent().moves[slot]=MoveId::None;winds.opponent().movePp[slot]=0;
    }
    winds.battle.weather=BattleWeather::StrongWinds;
    const uint16_t windsBefore=winds.opponent().currentHp;
    const BattleActionResult result=winds.act();
    const uint16_t windsDamage=static_cast<uint16_t>(windsBefore-winds.opponent().currentHp);
    assert(windsDamage<clearDamage&&result.effectiveness100==50U);
  }

  // Sand Veil's Gen-III weather immunity is independent of its evasion
  // modifier.
  {
    Fixture f(static_cast<MoveId>(150)); // SPLASH
    f.player().abilityId=8;
    f.battle.weather=BattleWeather::Sandstorm;
    const uint16_t before=f.player().currentHp;
    f.act();
    assert(f.player().currentHp==before);
  }

  // Flame Body/Magma Armor retain their battle immunity/contact roles and
  // also halve egg time while present anywhere in the active party.
  {
    PokemonCollection warm{};CollectionLogic::initialize(warm);
    assert(CollectionLogic::chooseStarter(warm,4,0xE6600001U));
    CollectionLogic::active(warm,0)->abilityId=49; // FLAME BODY
    EggState egg{};egg.active=true;egg.remainingSeconds=100;
    EggSystem::advance(egg,10,&warm);
    assert(egg.remainingSeconds==80);
    EggState ordinary{};ordinary.active=true;ordinary.remainingSeconds=100;
    EggSystem::advance(ordinary,10,nullptr);
    assert(ordinary.remainingSeconds==90);
  }

  // Pickup is evaluated independently for each eligible party member after
  // a non-link victory and places the found object in an empty held slot.
  {
    bool picked=false;
    for(uint32_t seed=1;seed<512&&!picked;++seed){
      Fixture f(MoveId::Tackle);
      f.battle.kind=BattleKind::Trainer;
      f.battle.rngState=seed;
      const uint32_t uid=f.player().uid;
      f.player()=CollectionLogic::createPokemon(uid,52,50,false,0xAB110006U); // MEOWTH
      f.player().moves[0]=MoveId::Tackle;
      f.player().movePp[0]=findFullMove(MoveId::Tackle)->pp;
      assert(f.player().abilityId==53); // PICKUP
      f.player().heldItem=HeldItem::None;
      f.opponent().currentHp=1;
      f.opponent().maximumHp=1;
      BattleEngine::fight(f.battle,f.collection,0);
      picked=f.player().heldItem!=HeldItem::None;
    }
    assert(picked);
  }

  // Link battles never run Pickup, and a battle-local copied Pickup neither
  // grants an item nor persists in the collection after teardown.
  {
    Fixture f(MoveId::Tackle);
    const uint32_t uid=f.player().uid;
    f.player()=CollectionLogic::createPokemon(uid,52,50,false,0xAB110007U); // MEOWTH
    f.player().moves[0]=MoveId::Tackle;
    f.player().movePp[0]=findFullMove(MoveId::Tackle)->pp;
    f.opponent().currentHp=f.opponent().maximumHp=1;
    f.act();
    assert(f.player().heldItem==HeldItem::None);
  }
  {
    for(uint32_t seed=1;seed<512U;++seed){
      const MoveId aerialAce=static_cast<MoveId>(332);
      Fixture f(aerialAce);
      f.battle.kind=BattleKind::Trainer;
      f.battle.rngState=seed;
      f.player().abilityId=53; // copied PICKUP; native Porygon has TRACE
      f.player().moves[0]=aerialAce;
      f.player().movePp[0]=findFullMove(aerialAce)->pp;
      f.opponent().currentHp=f.opponent().maximumHp=1;
      BattleEngine::fight(f.battle,f.collection,0);
      assert(f.player().heldItem==HeldItem::None);
      assert(f.player().abilityId==36);
    }
  }

  // Mega Meganium's MEGA SOL is personal sunlight. It must never replace the
  // field weather or grant an opposing Pokemon sunny recovery/charging rules.
  {
    constexpr uint8_t megaSol=220U;
    const MoveId synthesis=static_cast<MoveId>(235);
    Fixture holder(synthesis);
    holder.player().abilityId=megaSol;
    holder.opponent().abilityId=53U; // PICKUP; no Trace copy
    holder.player().maximumHp=120U;holder.player().currentHp=1U;
    BattleEngine::applyEntryAbilities(holder.battle,holder.collection);
    assert(holder.battle.weather==BattleWeather::Clear&&
           holder.battle.weatherTurns==0U);
    holder.act();
    assert(holder.player().currentHp==81U); // 1 + 2/3 of 120

    Fixture noLeak(static_cast<MoveId>(150)); // SPLASH
    noLeak.player().abilityId=megaSol;
    noLeak.opponent().abilityId=53U;
    noLeak.opponent().moves[0]=synthesis;
    noLeak.opponent().movePp[0]=findFullMove(synthesis)->pp;
    noLeak.opponent().maximumHp=120U;noLeak.opponent().currentHp=1U;
    BattleEngine::applyEntryAbilities(noLeak.battle,noLeak.collection);
    BattleEngine::fightPvp(noLeak.battle,noLeak.collection,0,0);
    assert(noLeak.opponent().currentHp==61U); // ordinary clear-weather half

    Fixture enemyHolder(static_cast<MoveId>(150)); // SPLASH
    enemyHolder.player().abilityId=53U;
    enemyHolder.opponent().abilityId=megaSol;
    enemyHolder.opponent().moves[0]=synthesis;
    enemyHolder.opponent().movePp[0]=findFullMove(synthesis)->pp;
    enemyHolder.opponent().maximumHp=120U;
    enemyHolder.opponent().currentHp=1U;
    BattleEngine::applyEntryAbilities(enemyHolder.battle,
                                      enemyHolder.collection);
    assert(enemyHolder.battle.weather==BattleWeather::Clear);
    BattleEngine::fightPvp(enemyHolder.battle,enemyHolder.collection,0,0);
    assert(enemyHolder.opponent().currentHp==81U);
  }
  {
    constexpr uint8_t megaSol=220U;
    const MoveId solarBeam=static_cast<MoveId>(76);

    // Solar Beam normally presents setup on turn one and release on turn two.
    Fixture ordinary(solarBeam);
    ordinary.opponent().maximumHp=ordinary.opponent().currentHp=1000U;
    const uint16_t ordinaryBefore=ordinary.opponent().currentHp;
    const BattleActionResult setup=ordinary.act();
    const BattleEvent* setupEvent=moveEvent(
        setup,BattleSide::Player,solarBeam);
    assert(setupEvent&&setupEvent->stageBefore==0&&
           ordinary.battle.playerVolatile.chargingMove==solarBeam&&
           ordinary.opponent().currentHp==ordinaryBefore);
    const BattleActionResult release=ordinary.act();
    const BattleEvent* releaseEvent=moveEvent(
        release,BattleSide::Player,solarBeam);
    assert(releaseEvent&&releaseEvent->stageBefore==1&&
           ordinary.battle.playerVolatile.chargingMove==MoveId::None&&
           ordinary.opponent().currentHp<ordinaryBefore);

    // Sunny Day makes Solar Beam a one-turn move. Its sole journal event must
    // select the beam/release animation, for both battle sides.
    Fixture sunny(solarBeam);
    sunny.battle.weather=BattleWeather::Sun;
    sunny.battle.weatherTurns=5U;
    sunny.opponent().maximumHp=sunny.opponent().currentHp=1000U;
    const uint16_t sunnyBefore=sunny.opponent().currentHp;
    const BattleActionResult sunnyResult=sunny.act();
    const BattleEvent* sunnyEvent=moveEvent(
        sunnyResult,BattleSide::Player,solarBeam);
    assert(sunnyEvent&&sunnyEvent->stageBefore==1&&
           sunny.battle.playerVolatile.chargingMove==MoveId::None&&
           sunny.opponent().currentHp<sunnyBefore);

    Fixture sunnyEnemy(static_cast<MoveId>(150)); // SPLASH
    sunnyEnemy.battle.weather=BattleWeather::Sun;
    sunnyEnemy.battle.weatherTurns=5U;
    sunnyEnemy.opponent().moves[0]=solarBeam;
    sunnyEnemy.opponent().movePp[0]=findFullMove(solarBeam)->pp;
    sunnyEnemy.player().maximumHp=sunnyEnemy.player().currentHp=1000U;
    const uint16_t sunnyPlayerBefore=sunnyEnemy.player().currentHp;
    const BattleActionResult sunnyEnemyResult=BattleEngine::fightPvp(
        sunnyEnemy.battle,sunnyEnemy.collection,0,0);
    const BattleEvent* sunnyEnemyEvent=moveEvent(
        sunnyEnemyResult,BattleSide::Opponent,solarBeam);
    assert(sunnyEnemyEvent&&sunnyEnemyEvent->stageBefore==1&&
           sunnyEnemy.battle.opponentVolatiles[0].chargingMove==MoveId::None&&
           sunnyEnemy.player().currentHp<sunnyPlayerBefore);

    Fixture holder(solarBeam);
    holder.player().abilityId=megaSol;
    holder.opponent().abilityId=53U;
    holder.opponent().maximumHp=holder.opponent().currentHp=1000U;
    const uint16_t before=holder.opponent().currentHp;
    const BattleActionResult holderResult=holder.act();
    const BattleEvent* holderEvent=moveEvent(
        holderResult,BattleSide::Player,solarBeam);
    assert(holderEvent&&holderEvent->stageBefore==1&&
           holder.battle.playerVolatile.chargingMove==MoveId::None&&
           holder.opponent().currentHp<before);

    Fixture noLeak(static_cast<MoveId>(150)); // SPLASH
    noLeak.player().abilityId=megaSol;
    noLeak.opponent().abilityId=53U;
    noLeak.opponent().moves[0]=solarBeam;
    noLeak.opponent().movePp[0]=findFullMove(solarBeam)->pp;
    const uint16_t playerBefore=noLeak.player().currentHp;
    BattleEngine::applyEntryAbilities(noLeak.battle,noLeak.collection);
    const BattleActionResult noLeakResult=BattleEngine::fightPvp(
        noLeak.battle,noLeak.collection,0,0);
    const BattleEvent* noLeakEvent=moveEvent(
        noLeakResult,BattleSide::Opponent,solarBeam);
    assert(noLeakEvent&&noLeakEvent->stageBefore==0&&
           noLeak.battle.opponentVolatiles[0].chargingMove==solarBeam&&
           noLeak.player().currentHp==playerBefore);
  }

  // Struggle is typeless in FireRed: Wonder Guard and Ghost immunity cannot
  // stop it.
  {
    Fixture f(MoveId::Tackle);
    f.opponent().abilityId=25; // WONDER GUARD
    f.opponent().maximumHp=f.opponent().currentHp=1000;
    for(uint8_t slot=0;slot<kMoveSlots;++slot)f.player().movePp[slot]=0;
    const uint16_t before=f.opponent().currentHp;
    BattleEngine::fightPvp(f.battle,f.collection,kMoveSlots,0xFEU);
    assert(f.opponent().currentHp<before);
  }
  {
    Fixture normal(MoveId::Tackle), changedType(MoveId::Tackle);
    normal.opponent().maximumHp=normal.opponent().currentHp=1000;
    changedType.opponent().maximumHp=changedType.opponent().currentHp=1000;
    for(uint8_t slot=0;slot<kMoveSlots;++slot){
      normal.player().movePp[slot]=0;
      changedType.player().movePp[slot]=0;
    }
    changedType.battle.playerMoveEffects.typeOverrideActive=true;
    changedType.battle.playerMoveEffects.type1=PokemonType::Grass;
    changedType.battle.playerMoveEffects.type2=PokemonType::Grass;
    const uint16_t normalBefore=normal.opponent().currentHp;
    const uint16_t changedBefore=changedType.opponent().currentHp;
    BattleEngine::fightPvp(normal.battle,normal.collection,kMoveSlots,0xFEU);
    BattleEngine::fightPvp(changedType.battle,changedType.collection,kMoveSlots,0xFEU);
    assert(normalBefore-normal.opponent().currentHp==
           changedBefore-changedType.opponent().currentHp);
  }
  // FireRed's Shield Dust gate covers effect codes 1..9 (major status,
  // confusion and flinch), but not secondary stat drops. Mud-Slap therefore
  // lowers Accuracy normally, while King's Rock remains blocked after its
  // effect roll.
  {
    Fixture f(static_cast<MoveId>(189)); // MUD-SLAP
    f.opponent().abilityId=19; // SHIELD DUST
    const uint16_t before=f.opponent().currentHp;
    const BattleActionResult result=f.act();
    assert(f.opponent().currentHp<before&&
           f.battle.opponentVolatiles[0].accuracyStage==-1&&
           !saw(result,BattleEventType::AbilityActivated,BattleSide::Opponent,19U));
  }
  {
    bool blocked=false;
    for(uint32_t seed=1;seed<4096U&&!blocked;++seed){
      Fixture f(MoveId::Tackle);
      f.battle.rngState=seed;
      f.player().heldItem=HeldItem::KingsRock;
      f.opponent().abilityId=19; // SHIELD DUST
      f.opponent().maximumHp=f.opponent().currentHp=1000;
      const BattleActionResult result=f.act();
      if(saw(result,BattleEventType::AbilityActivated,BattleSide::Opponent,19U)){
        blocked=true;
        assert(!f.battle.opponentVolatiles[0].flinched);
      }
    }
    assert(blocked);
  }

  // UNBURDEN uses Platinum's battle-local item latch. A persistent Berry
  // consumption and Knock Off both activate it; entering without an item
  // does not grant a free speed boost.
  {
    Fixture boosted(MoveId::Tackle);
    boosted.player().abilityId=85; // UNBURDEN
    boosted.opponent().abilityId=53; // avoid Trace
    boosted.player().heldItem=HeldItem::SitrusBerry;
    boosted.player().maximumHp=100;boosted.player().currentHp=40;
    boosted.opponent().moves[0]=MoveId::Tackle;
    boosted.opponent().movePp[0]=findFullMove(MoveId::Tackle)->pp;
    boosted.battle.opponentVolatiles[0].speedStage=1;
    BattleEngine::applyEntryAbilities(boosted.battle,boosted.collection);
    assert(boosted.player().heldItem==HeldItem::None&&
           (boosted.battle.playerGen4Effects.healBlockTurns&0x80U));
    const BattleActionResult result=BattleEngine::fightPvp(
        boosted.battle,boosted.collection,0,0);
    assert(firstMoveSide(result)==BattleSide::Player);

    Fixture empty(MoveId::Tackle);
    empty.player().abilityId=85;
    empty.opponent().abilityId=53;
    empty.opponent().moves[0]=MoveId::Tackle;
    empty.opponent().movePp[0]=findFullMove(MoveId::Tackle)->pp;
    empty.battle.opponentVolatiles[0].speedStage=1;
    BattleEngine::applyEntryAbilities(empty.battle,empty.collection);
    assert(!(empty.battle.playerGen4Effects.healBlockTurns&0x80U));
    const BattleActionResult emptyResult=BattleEngine::fightPvp(
        empty.battle,empty.collection,0,0);
    assert(firstMoveSide(emptyResult)==BattleSide::Opponent);
  }
  {
    Fixture knocked(moveNamed("KNOCK OFF"));
    knocked.player().abilityId=53;
    knocked.opponent().abilityId=85; // UNBURDEN
    knocked.opponent().heldItem=HeldItem::OranBerry;
    knocked.opponent().maximumHp=knocked.opponent().currentHp=1000;
    knocked.opponent().moves[0]=MoveId::Tackle;
    knocked.opponent().movePp[0]=findFullMove(MoveId::Tackle)->pp;
    BattleEngine::applyEntryAbilities(knocked.battle,knocked.collection);
    BattleEngine::fightPvp(knocked.battle,knocked.collection,0,0xFEU);
    assert((knocked.battle.opponentGen4Effects[0].healBlockTurns&0x80U)&&
           (knocked.battle.opponentVolatiles[0].heldItemSuppressed&1U));
    knocked.player().moves[0]=MoveId::Tackle;
    knocked.player().movePp[0]=findFullMove(MoveId::Tackle)->pp;
    knocked.battle.playerVolatile.speedStage=1;
    const BattleActionResult result=BattleEngine::fightPvp(
        knocked.battle,knocked.collection,0,0);
    assert(firstMoveSide(result)==BattleSide::Opponent);
  }

  // SIMPLE doubles effective stages in Gen IV without writing doubled stage
  // values. Mold Breaker ignores a defending Simple during move resolution.
  {
    Fixture f(moveNamed("SWORDS DANCE"));
    f.player().abilityId=87; // SIMPLE
    f.act();
    assert(f.battle.playerVolatile.attackStage==2);
  }
  {
    Fixture simple(MoveId::Tackle),ordinary(MoveId::Tackle);
    simple.player().abilityId=87;
    simple.battle.playerVolatile.attackStage=1;
    ordinary.battle.playerVolatile.attackStage=2;
    simple.battle.rngState=ordinary.battle.rngState=0x55112233U;
    assert(damageFor(simple,MoveId::Tackle)==damageFor(ordinary,MoveId::Tackle));
  }
  {
    Fixture blocked(MoveId::Tackle),bypassed(MoveId::Tackle);
    blocked.opponent().abilityId=87;
    bypassed.opponent().abilityId=87;
    bypassed.player().abilityId=abilityNamed("MOLD BREAKER");
    blocked.battle.opponentVolatiles[0].defenseStage=1;
    bypassed.battle.opponentVolatiles[0].defenseStage=1;
    blocked.battle.rngState=bypassed.battle.rngState=0x55112234U;
    assert(damageFor(bypassed,MoveId::Tackle)>damageFor(blocked,MoveId::Tackle));
  }

  // Generation-IV absorption/weather details.
  {
    Fixture f(static_cast<MoveId>(86)); // THUNDER WAVE
    f.opponent().abilityId=79; // MOTOR DRIVE
    const BattleActionResult result=f.act();
    assert(f.opponent().status==StatusCondition::None&&
           f.battle.opponentVolatiles[0].speedStage==1&&
           saw(result,BattleEventType::AbilityActivated,
               BattleSide::Opponent,79U));
  }
  {
    Fixture cloak(static_cast<MoveId>(150)); // SPLASH
    cloak.player().abilityId=82; // SNOW CLOAK
    cloak.player().maximumHp=160;cloak.player().currentHp=100;
    cloak.battle.weather=BattleWeather::Hail;cloak.battle.weatherTurns=0xFF;
    cloak.act();
    assert(cloak.player().currentHp==100);

    Fixture body(static_cast<MoveId>(150));
    body.player().abilityId=116; // ICE BODY
    body.player().maximumHp=160;body.player().currentHp=100;
    body.battle.weather=BattleWeather::Hail;body.battle.weatherTurns=0xFF;
    body.act();
    assert(body.player().currentHp==110);
  }
  {
    Fixture f(MoveId::WaterGun);
    f.opponent().abilityId=115; // STORM DRAIN is redirect-only in Gen IV
    f.opponent().maximumHp=f.opponent().currentHp=1000;
    const uint16_t before=f.opponent().currentHp;
    f.act();
    assert(f.opponent().currentHp<before&&
           f.battle.opponentVolatiles[0].spAttackStage==0);
  }

  // DOWNLOAD compares effective staged defenses, raises Sp. Atk on equality,
  // and cannot inspect through Substitute.
  {
    Fixture f;
    f.player().abilityId=89;f.opponent().abilityId=53;
    const uint32_t uid=f.opponent().uid;
    f.opponent()=CollectionLogic::createPokemon(uid,213,50,false,0x10101010U);
    f.opponent().abilityId=53;
    f.opponent().ivs=IndividualValues{};f.opponent().evs=EffortValues{};
    f.opponent().nature=PokemonNature::Hardy;
    BattleEngine::applyEntryAbilities(f.battle,f.collection,
                                      BattleEntryScope::Player);
    assert(f.battle.playerVolatile.spAttackStage==1&&
           f.battle.playerVolatile.attackStage==0);
  }
  {
    Fixture f;
    f.player().abilityId=89;f.opponent().abilityId=53;
    f.battle.opponentVolatiles[0].substituteHp=1;
    BattleEngine::applyEntryAbilities(f.battle,f.collection,
                                      BattleEntryScope::Player);
    assert(f.battle.playerVolatile.attackStage==0&&
           f.battle.playerVolatile.spAttackStage==0);
  }

  // Exact move families for the offensive passives. These names are the
  // manually reviewed Gen-I--IV intersections of Platinum/expansion data.
  {
    for(const char* name:{"DYNAMIC PUNCH","HAMMER ARM"}){
      Fixture plain(moveNamed(name)),boosted(moveNamed(name));
      boosted.player().abilityId=90; // IRON FIST
      plain.opponent().abilityId=boosted.opponent().abilityId=100; // NO GUARD
      plain.battle.rngState=boosted.battle.rngState=0x88776655U;
      assert(damageFor(boosted,moveNamed(name))>damageFor(plain,moveNamed(name)));
    }
  }
  {
    static constexpr const char* slicing[]={
      "CUT","RAZOR LEAF","SLASH","FURY CUTTER","AIR CUTTER","AERIAL ACE",
      "LEAF BLADE","NIGHT SLASH","AIR SLASH","X SCISSOR","PSYCHO CUT",
      "CROSS POISON"};
    for(const char* name:slicing){
      Fixture plain(moveNamed(name)),boosted(moveNamed(name));
      boosted.player().abilityId=abilityNamed("SHARPNESS");
      plain.opponent().abilityId=boosted.opponent().abilityId=100;
      plain.battle.rngState=boosted.battle.rngState=0x77665544U;
      assert(damageFor(boosted,moveNamed(name))>damageFor(plain,moveNamed(name)));
    }
    Fixture plain(moveNamed("RAZOR WIND")),notSlicing(moveNamed("RAZOR WIND"));
    notSlicing.player().abilityId=abilityNamed("SHARPNESS");
    plain.opponent().abilityId=notSlicing.opponent().abilityId=100;
    plain.battle.rngState=notSlicing.battle.rngState=0x77665545U;
    assert(damageFor(plain,moveNamed("RAZOR WIND"))==
           damageFor(notSlicing,moveNamed("RAZOR WIND")));
  }
  {
    Fixture plain(MoveId::Struggle),technician(MoveId::Struggle),
            reckless(MoveId::Struggle);
    technician.player().abilityId=102;
    reckless.player().abilityId=121;
    plain.battle.rngState=technician.battle.rngState=
        reckless.battle.rngState=0x44556677U;
    const uint16_t ordinary=damageFor(plain,MoveId::Struggle);
    assert(damageFor(technician,MoveId::Struggle)==ordinary&&
           damageFor(reckless,MoveId::Struggle)==ordinary);
  }

  // MAGIC GUARD blocks ordinary recoil/crash-style indirect damage but not
  // Struggle's special recoil path.
  {
    Fixture f(static_cast<MoveId>(38)); // DOUBLE EDGE
    f.player().abilityId=99;f.player().maximumHp=f.player().currentHp=1000;
    f.opponent().maximumHp=f.opponent().currentHp=1000;
    f.act();
    assert(f.player().currentHp==1000);
  }
  {
    Fixture f(MoveId::Tackle);
    f.player().abilityId=99;f.player().maximumHp=f.player().currentHp=1000;
    f.opponent().maximumHp=f.opponent().currentHp=1000;
    for(uint8_t slot=0;slot<kMoveSlots;++slot)f.player().movePp[slot]=0;
    BattleEngine::fightPvp(f.battle,f.collection,kMoveSlots,0xFEU);
    assert(f.player().currentHp<1000);
  }

  // AFTERMATH respects Damp, Magic Guard and U-turn. Innards Out returns only
  // the HP lost on the final strike of a multi-hit move.
  {
    for(uint8_t attackerAbility:{99U,6U}){ // MAGIC GUARD, DAMP
      Fixture f(MoveId::Tackle);
      f.player().abilityId=attackerAbility;
      f.player().maximumHp=f.player().currentHp=400;
      f.opponent().abilityId=107;f.opponent().maximumHp=f.opponent().currentHp=1;
      f.act();
      assert(f.player().currentHp==400);
    }
    Fixture pivot(static_cast<MoveId>(369)); // U TURN
    pivot.player().maximumHp=pivot.player().currentHp=400;
    pivot.opponent().abilityId=107;
    pivot.opponent().maximumHp=pivot.opponent().currentHp=1;
    pivot.act();
    assert(pivot.player().currentHp==400);
  }
  {
    bool exercised=false;
    for(uint32_t seed=1;seed<1024U&&!exercised;++seed){
      Fixture f(static_cast<MoveId>(3)); // DOUBLE SLAP
      f.battle.rngState=seed;
      f.player().maximumHp=f.player().currentHp=1000;
      f.opponent().abilityId=abilityNamed("INNARDS OUT");
      f.opponent().maximumHp=f.opponent().currentHp=25;
      const BattleActionResult result=f.act();
      uint8_t hpEvents=0;uint16_t finalStrike=0;
      for(uint8_t i=0;i<result.eventCount;++i)
        if(result.events[i].type==BattleEventType::HpChanged&&
           result.events[i].side==BattleSide::Opponent&&
           result.events[i].pokemonUid==f.opponent().uid){
          ++hpEvents;finalStrike=result.events[i].value;
        }
      if(!f.opponent().currentHp&&hpEvents>=2U){
        exercised=true;
        assert(static_cast<uint16_t>(1000U-f.player().currentHp)==finalStrike);
      }
    }
    assert(exercised);
  }

  // LEAF GUARD covers direct sleep, Yawn setup and Yawn's delayed sleep tick.
  {
    Fixture sleep(static_cast<MoveId>(79)); // SLEEP POWDER
    sleep.battle.weather=BattleWeather::Sun;sleep.battle.weatherTurns=0xFF;
    sleep.opponent().abilityId=103;
    sleep.act();
    assert(sleep.opponent().status==StatusCondition::None);

    Fixture yawn(static_cast<MoveId>(281));
    yawn.battle.weather=BattleWeather::Sun;yawn.battle.weatherTurns=0xFF;
    yawn.opponent().abilityId=103;
    yawn.act();
    assert(yawn.battle.opponentMoveEffects[0].yawnTurns==0);

    Fixture delayed(static_cast<MoveId>(150));
    delayed.battle.weather=BattleWeather::Sun;delayed.battle.weatherTurns=0xFF;
    delayed.player().abilityId=103;
    delayed.battle.playerMoveEffects.yawnTurns=1;
    delayed.act();
    assert(delayed.player().status==StatusCondition::None&&
           delayed.battle.playerMoveEffects.yawnTurns==0);
  }

  // Custom PRANKSTER follows category metadata and its current-generation
  // Dark-type immunity for opposing status moves.
  {
    Fixture f(static_cast<MoveId>(86)); // THUNDER WAVE
    f.player().abilityId=abilityNamed("PRANKSTER");
    const uint32_t uid=f.opponent().uid;
    f.opponent()=CollectionLogic::createPokemon(uid,197,50,false,0x22222222U);
    f.opponent().abilityId=53;
    const BattleActionResult result=f.act();
    assert(f.opponent().status==StatusCondition::None&&
           saw(result,BattleEventType::MoveEffect,BattleSide::Opponent,
               static_cast<uint16_t>(BattleMoveEffect::Failed)));
  }

  // Anger Point does not activate on a critical hit that also faints its
  // holder.
  {
    bool criticalKo=false;
    for(uint32_t seed=1;seed<512U&&!criticalKo;++seed){
      Fixture f(moveNamed("SLASH"));
      f.battle.rngState=seed;
      f.opponent().abilityId=84;
      f.opponent().maximumHp=f.opponent().currentHp=1;
      const BattleActionResult result=f.act();
      if(saw(result,BattleEventType::CriticalHit,BattleSide::Player)){
        criticalKo=true;
        assert(!f.opponent().currentHp&&
               f.battle.opponentVolatiles[0].attackStage==0);
      }
    }
    assert(criticalKo);
  }

  // FLOWER GIFT drives Cherrim's official Sunshine battle form and its Sp.
  // Def boost remains one of Mold Breaker's breakable defender abilities.
  {
    Fixture f;
    const uint32_t uid=f.player().uid;
    f.player()=CollectionLogic::createPokemon(uid,421,50,false,0x33333333U);
    f.player().abilityId=123;
    f.battle.weather=BattleWeather::Sun;f.battle.weatherTurns=0xFF;
    BattleEngine::applyEntryAbilities(f.battle,f.collection,
                                      BattleEntryScope::Player);
    assert(f.player().form==1&&CollectionLogic::resolvedForm(f.player())==1);
    f.battle.weather=BattleWeather::Clear;f.battle.weatherTurns=0;
    BattleEngine::synchronizeWeatherForms(f.battle,f.collection);
    assert(f.player().form==0);
  }
  {
    const MoveId psychic=moveNamed("PSYCHIC");
    Fixture blocked(psychic),bypassed(psychic);
    blocked.battle.weather=bypassed.battle.weather=BattleWeather::Sun;
    blocked.opponent().abilityId=bypassed.opponent().abilityId=123;
    bypassed.player().abilityId=abilityNamed("MOLD BREAKER");
    blocked.battle.rngState=bypassed.battle.rngState=0x33445566U;
    assert(damageFor(bypassed,psychic)>damageFor(blocked,psychic));
  }

  // The three abilities introduced with the Unova Megas are behavioral,
  // not display-only. Piercing Drill and Unseen Fist cross Protect only for
  // contact moves, in both action directions.
  for(const char* abilityName:{"PIERCING DRILL","UNSEEN FIST"}){
    Fixture playerContact(MoveId::Tackle);
    playerContact.player().abilityId=abilityNamed(abilityName);
    playerContact.opponent().maximumHp=playerContact.opponent().currentHp=1000;
    playerContact.opponent().moves[0]=moveNamed("PROTECT");
    playerContact.opponent().movePp[0]=findFullMove(moveNamed("PROTECT"))->pp;
    const uint16_t contactBefore=playerContact.opponent().currentHp;
    const BattleActionResult contactResult=BattleEngine::fightPvp(
        playerContact.battle,playerContact.collection,0,0);
    assert(contactResult.accepted&&
           playerContact.opponent().currentHp<contactBefore);

    Fixture playerRanged(MoveId::WaterGun);
    playerRanged.player().abilityId=abilityNamed(abilityName);
    playerRanged.opponent().maximumHp=playerRanged.opponent().currentHp=1000;
    playerRanged.opponent().moves[0]=moveNamed("PROTECT");
    playerRanged.opponent().movePp[0]=findFullMove(moveNamed("PROTECT"))->pp;
    const uint16_t rangedBefore=playerRanged.opponent().currentHp;
    const BattleActionResult rangedResult=BattleEngine::fightPvp(
        playerRanged.battle,playerRanged.collection,0,0);
    assert(playerRanged.opponent().currentHp==rangedBefore&&
           saw(rangedResult,BattleEventType::MoveEffect,
               BattleSide::Opponent,
               static_cast<uint16_t>(BattleMoveEffect::Protected)));

    Fixture enemyContact(moveNamed("PROTECT"));
    enemyContact.opponent().moves[0]=MoveId::Tackle;
    enemyContact.opponent().movePp[0]=findFullMove(MoveId::Tackle)->pp;
    enemyContact.opponent().abilityId=abilityNamed(abilityName);
    enemyContact.player().maximumHp=enemyContact.player().currentHp=1000;
    const uint16_t enemyBefore=enemyContact.player().currentHp;
    BattleEngine::fightPvp(enemyContact.battle,enemyContact.collection,0,0);
    assert(enemyContact.player().currentHp<enemyBefore);
  }

  // Eelevate combines Levitate's complete grounding behavior with a
  // post-KO boost to the holder's highest non-HP calculated stat.
  {
    Fixture immune(moveNamed("EARTHQUAKE"));
    immune.opponent().abilityId=abilityNamed("EELEVATE");
    immune.opponent().maximumHp=immune.opponent().currentHp=1000;
    const BattleActionResult result=immune.act();
    assert(immune.opponent().currentHp==1000U&&
           saw(result,BattleEventType::AbilityActivated,
               BattleSide::Opponent,abilityNamed("EELEVATE")));

    Fixture bypassed(moveNamed("EARTHQUAKE"));
    bypassed.player().abilityId=abilityNamed("MOLD BREAKER");
    bypassed.opponent().abilityId=abilityNamed("EELEVATE");
    bypassed.opponent().maximumHp=bypassed.opponent().currentHp=1000;
    bypassed.act();
    assert(bypassed.opponent().currentHp<1000U);
  }
  {
    Fixture hazards(moveNamed("SPLASH"));
    uint32_t incomingUid=0;
    assert(CollectionLogic::add(hazards.collection,
        CollectionLogic::createPokemon(0,137,50,false,0xEE1E0001U),
        &incomingUid));
    assert(CollectionLogic::setPartySlot(hazards.collection,1,incomingUid));
    OwnedPokemon* incoming=CollectionLogic::find(hazards.collection,incomingUid);
    assert(incoming);incoming->abilityId=abilityNamed("EELEVATE");
    incoming->maximumHp=incoming->currentHp=1000;
    hazards.battle.playerSpikesLayers=3;
    hazards.battle.playerToxicSpikesLayers=2;
    const BattleActionResult switched=BattleEngine::switchToPokemonPvp(
        hazards.battle,hazards.collection,incomingUid,0xFEU,false);
    assert(switched.accepted&&incoming->currentHp==1000U&&
           incoming->status==StatusCondition::None);
  }
  {
    Fixture trap(moveNamed("SPLASH"));
    trap.player().abilityId=abilityNamed("EELEVATE");
    trap.opponent().abilityId=71; // ARENA TRAP
    uint32_t replacementUid=0;
    assert(CollectionLogic::add(trap.collection,
        CollectionLogic::createPokemon(0,137,50,false,0xEE1E0002U),
        &replacementUid));
    assert(CollectionLogic::setPartySlot(trap.collection,1,replacementUid));
    assert(BattleEngine::switchToPokemonPvp(
        trap.battle,trap.collection,replacementUid,0xFEU,false).accepted);
  }
  {
    Fixture boost(MoveId::Tackle);
    boost.player().abilityId=abilityNamed("EELEVATE");
    boost.opponent().maximumHp=boost.opponent().currentHp=1;
    const BattleActionResult result=boost.act();
    assert(boost.battle.playerVolatile.spAttackStage==1&&
           boost.battle.playerVolatile.attackStage==0&&
           saw(result,BattleEventType::AbilityActivated,
               BattleSide::Player,abilityNamed("EELEVATE")));
  }

  std::cout<<"Ability behavior matrix passed.\n";
  return 0;
}
