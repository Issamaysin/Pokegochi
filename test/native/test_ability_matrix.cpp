#include <algorithm>
#include <cassert>
#include <cstdint>
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
    bypass.player().abilityId=134; // MOLD BREAKER
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
    f.player().abilityId=134; // MOLD BREAKER
    f.opponent().abilityId=26; // LEVITATE
    const uint16_t before=f.opponent().currentHp;
    f.act();
    assert(f.opponent().currentHp<before);
  }

  // Mold Breaker only suppresses Abilities marked breakable by the retail
  // engine. Post-move contact effects and Liquid Ooze still activate.
  {
    Fixture f(MoveId::Tackle);
    f.player().abilityId=134; // MOLD BREAKER
    f.opponent().abilityId=24; // ROUGH SKIN (not breakable)
    const uint16_t before=f.player().currentHp;
    f.act();
    assert(f.player().currentHp==static_cast<uint16_t>(before-
           std::max<uint16_t>(1U,f.player().maximumHp/16U)));
  }
  {
    Fixture f(static_cast<MoveId>(71)); // ABSORB
    f.player().abilityId=134; // MOLD BREAKER
    f.opponent().abilityId=64; // LIQUID OOZE (not breakable)
    const uint16_t before=f.player().currentHp;
    f.act();
    assert(f.player().currentHp<before);
  }
  {
    Fixture f(static_cast<MoveId>(86)); // THUNDER WAVE
    f.player().abilityId=134; // MOLD BREAKER
    f.opponent().abilityId=141; // MAGIC BOUNCE (breakable)
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
    f.player().abilityId=131; // NO GUARD
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
    f.player().abilityId=132; // PARENTAL BOND
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
    f.player().abilityId=147; // PRIMORDIAL SEA
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
    f.player().abilityId=147; // PRIMORDIAL SEA
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
  // Shield Dust rejects secondary effects (including King's Rock) only
  // after the effect roll, while leaving the damaging hit intact.
  {
    Fixture f(static_cast<MoveId>(189)); // MUD-SLAP
    f.opponent().abilityId=19; // SHIELD DUST
    const uint16_t before=f.opponent().currentHp;
    const BattleActionResult result=f.act();
    assert(f.opponent().currentHp<before&&
           f.battle.opponentVolatiles[0].accuracyStage==0&&
           saw(result,BattleEventType::AbilityActivated,BattleSide::Opponent,19U));
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

  std::cout<<"Ability behavior matrix passed.\n";
  return 0;
}
