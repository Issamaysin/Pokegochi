#include "game/BattleEngine.h"
#include "game/BattleAnimationData.h"
#include "game/Pokedex.h"
#include "game/PokedexRewards.h"
#include "game/GymSystem.h"
#include "game/LeagueSystem.h"
#include "game/MegaChallengeSystem.h"
#include "game/BattleTowerSystem.h"
#include "game/Economy.h"
#include "game/EggSystem.h"
#include "game/TrainerData.h"
#include "game/MegaEvolution.h"
#include <algorithm>
#include <cassert>
#include <cstring>

template <typename Visitor>
void visitDefaultAnimationPath(uint16_t move, Visitor&& visitor) {
  const BattleAnimationProgram* program=findBattleAnimationProgram(move);
  assert(program);
  int16_t arguments[16]{};
  uint16_t returns[12]{};
  uint8_t depth=0;
  uint16_t pc=0;
  const uint32_t limit=static_cast<uint32_t>(program->commandCount)*32U+64U;
  for(uint32_t step=0;pc<program->commandCount&&step<limit;++step){
    const BattleAnimationCommand* command=battleAnimationProgramCommand(*program,pc);
    assert(command);
    const int16_t* values=battleAnimationCommandArguments(*command);
    const auto opcode=static_cast<BattleAnimationOpcode>(command->opcode);
    const auto value=[&](uint8_t index,int16_t fallback=0){
      return values&&index<command->argumentCount?values[index]:fallback;
    };
    if(opcode==BattleAnimationOpcode::SetArgument){
      if(value(0,-1)>=0&&value(0)<16)arguments[value(0)]=value(1);++pc;
    }else if(opcode==BattleAnimationOpcode::Query){
      // The deterministic audit path represents ordinary singles, turn zero,
      // clear weather/plain query result zero. Runtime-query behavior is
      // covered independently by the generated branch matrix.
      uint8_t destination=7;
      if(command->resourceId==static_cast<uint16_t>(BattleAnimationQuery::IsMovePowerOver99))destination=15;
      else if(command->resourceId==static_cast<uint16_t>(BattleAnimationQuery::BattleTerrain))destination=0;
      else if(command->resourceId==static_cast<uint16_t>(BattleAnimationQuery::RolloutCounter))destination=static_cast<uint8_t>(value(0));
      if(destination<16)arguments[destination]=0;++pc;
    }else if(opcode==BattleAnimationOpcode::Jump)pc=command->resourceId;
    else if(opcode==BattleAnimationOpcode::Call){assert(depth<12);returns[depth++]=pc+1U;pc=command->resourceId;}
    else if(opcode==BattleAnimationOpcode::Return){if(!depth)return;pc=returns[--depth];}
    else if(opcode==BattleAnimationOpcode::JumpIfArgumentEquals){
      const int16_t index=value(0,-1);pc=index>=0&&index<16&&arguments[index]==value(1)?command->resourceId:pc+1U;
    }else if(opcode==BattleAnimationOpcode::JumpIfMoveTurnEquals){
      pc=value(0)==0?command->resourceId:pc+1U;
    }else if(opcode==BattleAnimationOpcode::End)return;
    else{visitor(*command);++pc;}
  }
  assert(false&&"animation bytecode did not terminate");
}

uint16_t countDefaultAnimationOpcode(uint16_t move,BattleAnimationOpcode wanted){
  uint16_t count=0;
  visitDefaultAnimationPath(move,[&](const BattleAnimationCommand& command){
    if(static_cast<BattleAnimationOpcode>(command.opcode)==wanted)++count;
  });
  return count;
}

int main() {
  // TM01..TM50 and HM01..HM08, as well as every species compatibility bit,
  // are generated from pokeemerald. These anchors catch shifted machine IDs,
  // accidental FireRed tables and the internal-vs-National Hoenn ID trap.
  assert(Economy::machineMove(0) == static_cast<MoveId>(264));   // Focus Punch
  assert(Economy::machineMove(49) == static_cast<MoveId>(315)); // Overheat
  assert(Economy::machineMove(50) == static_cast<MoveId>(15));  // Cut
  assert(Economy::machineMove(57) == static_cast<MoveId>(291)); // Dive
  assert(Economy::machineMove(58) == MoveId::None);
  assert(!Economy::canLearnMachine(1, 0));   // Bulbasaur / Focus Punch
  assert(Economy::canLearnMachine(1, 5));    // Bulbasaur / Toxic
  assert(Economy::canLearnMachine(1, 50));   // Bulbasaur / Cut
  assert(!Economy::canLearnMachine(1, 52));  // Bulbasaur / Surf
  assert(Economy::canLearnMachine(4, 0));    // Charmander / Focus Punch
  assert(Economy::canLearnMachine(252, 8));  // Treecko / Bullet Seed
  for (uint8_t machine = 0; machine < kMachineCount; ++machine)
    assert(Economy::canLearnMachine(151, machine)); // Mew learns every machine.
  for (uint8_t machine = 0; machine < kMachineCount; ++machine) {
    assert(!Economy::canLearnMachine(201, machine)); // Unown learns no machines.
    assert(!Economy::canLearnMachine(235, machine)); // Neither does Smeargle.
  }
  uint16_t emeraldCompatibilityCount = 0;
  for (uint16_t species = 1; species <= 386; ++species)
    for (uint8_t machine = 0; machine < kMachineCount; ++machine)
      emeraldCompatibilityCount += Economy::canLearnMachine(species, machine) ? 1U : 0U;
  assert(emeraldCompatibilityCount == 8927U);
  assert(!Economy::canLearnMachine(0, 0) && !Economy::canLearnMachine(387, 0) &&
         !Economy::canLearnMachine(1, kMachineCount));

  // The Summary move editor exposes the complete level-up pool inherited
  // from pre-evolutions, but never leaks moves above the current level.
  MoveId venusaurPool[64]{};
  const uint8_t venusaurPoolCount = relearnableMovesForLine(3, 20, venusaurPool, 64);
  auto venusaurCanRelearn = [&](MoveId move) {
    for (uint8_t index = 0; index < venusaurPoolCount; ++index)
      if (venusaurPool[index] == move) return true;
    return false;
  };
  assert(venusaurCanRelearn(MoveId::Tackle));
  assert(venusaurCanRelearn(MoveId::VineWhip));
  assert(venusaurCanRelearn(static_cast<MoveId>(75)));  // Razor Leaf from Bulbasaur.
  assert(!venusaurCanRelearn(static_cast<MoveId>(76))); // SolarBeam is above Lv20.

  static_assert(sizeof(CombatVolatile)==34,
      "stockpileCount must use existing tail padding and keep saves compatible");
  assert(battleAnimationCount()==354);
  assert(battleAnimationProgramCount()==354);
  assert(battleAnimationSpriteResourceCount()>=250);
  // Every generated SpriteTemplate variant must remain drawable for its full
  // retained lifetime.  This is deliberately exhaustive rather than a list
  // of showcase moves: corrupt cel offsets, forgotten orientation variants
  // and invalid affine streams must fail on the host before they can become
  // a missing/white rectangle on the ESP32.
  for(uint16_t resourceId=0;resourceId<battleAnimationSpriteResourceCount();++resourceId){
    const BattleAnimationSpriteResource* source=battleAnimationSpriteResource(resourceId);
    assert(source&&source->frameCount>=1&&source->sequenceCount>=1&&
           source->lifetimeFrames>=1);
    for(uint8_t sequence=0;sequence<source->sequenceCount;++sequence){
      BattleAnimationSpriteResource selected=*source;
      selected.selectorArgument=-1;
      selected.selectorConstant=sequence;
      const uint8_t stepCount=battleAnimationSpriteSequenceStepCount(selected,nullptr,0);
      assert(stepCount>=1&&stepCount<=255);
      uint32_t frameMask=0;
      for(uint8_t step=0;step<stepCount;++step){
        const uint8_t frame=battleAnimationSpriteSequenceFrame(selected,step,nullptr,0);
        assert(frame<selected.frameCount);
        if(frame<32)frameMask|=1UL<<frame;
      }
      // One selected callback sequence must fit the eight-cel resident cache.
      // Pointer-table alternatives are tested independently and are never
      // flattened into a single carousel.
      uint8_t unique=0;
      for(uint8_t bit=0;bit<32;++bit)if(frameMask&(1UL<<bit))++unique;
      assert(unique>=1&&unique<=8);
      for(uint16_t age=0;age<selected.lifetimeFrames;++age)
        assert(battleAnimationSpriteFrameAtAge(
            selected,static_cast<uint8_t>(age),nullptr,0)<selected.frameCount);
    }
    for(uint8_t sequence=0;sequence<source->affineSequenceCount;++sequence){
      BattleAnimationSpriteResource selected=*source;
      selected.affineSelectorArgument=-1;
      selected.affineSelectorConstant=sequence;
      for(uint16_t age=0;age<selected.lifetimeFrames;++age){
        BattleAnimationAffineState affine{};
        const bool active=battleAnimationSpriteAffineAtAge(
            selected,static_cast<uint8_t>(age),nullptr,0,affine);
        if(age<selected.affineStartAge){assert(!active);continue;}
        assert(active);
        assert(affine.scaleXPercent>=-250&&affine.scaleXPercent<=250);
        assert(affine.scaleYPercent>=-250&&affine.scaleYPercent<=250);
      }
    }
  }
  // There is exactly one animation executor. Two-turn moves, type branches
  // and move-result branches are inputs to the generated FireRed VM rather
  // than reasons to bypass it with a second approximate player.
  for(uint16_t move=1;move<=354;++move){
    assert(battleAnimationPlaybackKind(move)==BattleAnimationPlaybackKind::Generated);
    assert(findBattleAnimationProgram(move));
  }
  assert(battleAnimationProgramCount()==354);
  uint16_t userTargetMoveCount = 0;
  for(uint16_t move=1;move<=354;++move){
    const FullMoveData* moveData=findFullMove(static_cast<MoveId>(move));
    assert(moveData);
    if(moveData->target==MoveTarget::User)++userTargetMoveCount;
    const BattleAnimationData* animation=findBattleAnimation(move);
    // FireRed has task-only animations (Psychic, Surf, Earthquake, etc.) with
    // no OBJ graphics. They must stay assetCount=0 instead of being disguised
    // as an unrelated HIT sprite; the firmware's procedural task renderer owns
    // their visible frames.
    assert(animation&&animation->assetCount<=4&&animation->particleCount>=1);
    for(uint8_t asset=0;asset<animation->assetCount;++asset)
      assert(animation->assets[asset][0]&&animation->frameCounts[asset]>=1);
    const BattleAnimationProgram* program=findBattleAnimationProgram(move);
    assert(program&&program->commandCount>=1);
    bool ended=false;
    for(uint16_t commandIndex=0;commandIndex<program->commandCount;++commandIndex){
      const BattleAnimationCommand* command=battleAnimationProgramCommand(*program,commandIndex);
      assert(command);
      if(command->argumentCount)assert(battleAnimationCommandArguments(*command));
      if(static_cast<BattleAnimationOpcode>(command->opcode)==BattleAnimationOpcode::SpawnSprite)
        assert(battleAnimationSpriteResource(command->resourceId));
      if(static_cast<BattleAnimationOpcode>(command->opcode)==BattleAnimationOpcode::VisualTask)
        assert(battleAnimationTaskResource(command->resourceId));
      if(static_cast<BattleAnimationOpcode>(command->opcode)==BattleAnimationOpcode::End)ended=true;
    }
    assert(ended);
    const char* description=moveDescription(static_cast<MoveId>(move));
    assert(description&&description[0]);
    assert(std::strstr(description,"CHANGES ONE OR MORE BATTLE STATS")==nullptr);
  }
  // This count comes from gMoves in the FireRed source. If generation ever
  // drops or guesses a target byte, the exhaustive USER-move tests below are
  // no longer meaningful and must fail immediately.
  assert(userTargetMoveCount==67);
  // FOCUS ENERGY calls EndureEffect three times (five exact particles each).
  // AnimEndureEnergy chooses gBattleAnimAttacker/gBattleAnimTarget through
  // argument 0, so losing that selector sends all fifteen particles to the
  // foe. Keep both the move target and generated callback origin under test.
  const FullMoveData* focusEnergyData=findFullMove(static_cast<MoveId>(116));
  assert(focusEnergyData&&focusEnergyData->target==MoveTarget::User);
  const BattleAnimationProgram* focusEnergyProgram=findBattleAnimationProgram(116);
  assert(focusEnergyProgram);
  uint8_t focusEnergyUserParticles=0;
  bool focusEnergyShakesUser=false;
  visitDefaultAnimationPath(116,[&](const BattleAnimationCommand& value){
    const BattleAnimationCommand* command=&value;
    const auto opcode=static_cast<BattleAnimationOpcode>(command->opcode);
    if(opcode==BattleAnimationOpcode::SpawnSprite){
      const BattleAnimationSpriteResource* resource=
          battleAnimationSpriteResource(command->resourceId);
      const int16_t* arguments=battleAnimationCommandArguments(*command);
      if(resource&&resource->origin==BattleAnimationOrigin::Argument0&&
         arguments&&command->argumentCount>=1&&arguments[0]==0)
        ++focusEnergyUserParticles;
    }else if(opcode==BattleAnimationOpcode::VisualTask&&command->anchor==0){
      const BattleAnimationTaskResource* resource=
          battleAnimationTaskResource(command->resourceId);
      if(resource&&resource->kind==BattleAnimationTaskKind::Shake)
        focusEnergyShakesUser=true;
    }
  });
  assert(focusEnergyUserParticles==15);
  assert(focusEnergyShakesUser);
  // AGILITY traces the user (argument 0 / ANIM_ATTACKER).  Its retained
  // palette task must follow that script anchor instead of flashing the foe.
  const FullMoveData* agilityData=findFullMove(static_cast<MoveId>(97));
  assert(agilityData&&agilityData->target==MoveTarget::User);
  bool agilityTraceFollowsUser=false;
  visitDefaultAnimationPath(97,[&](const BattleAnimationCommand& command){
    if(static_cast<BattleAnimationOpcode>(command.opcode)!=
       BattleAnimationOpcode::VisualTask||command.anchor!=0)return;
    const BattleAnimationTaskResource* resource=
        battleAnimationTaskResource(command.resourceId);
    if(resource&&resource->motion==
        BattleAnimationTaskMotion::TraceMonBlendedExact){
      assert(resource->target==BattleAnimationTaskTarget::ScriptAnchor);
      agilityTraceFollowsUser=true;
    }
  });
  assert(agilityTraceFollowsUser);
  // HYPER BEAM is built from repeated `call HyperBeamOrbs` subroutines in
  // FireRed. The generator must retain a travelling sequence rather than
  // flattening it into the old one/two-particle placeholder.
  const BattleAnimationData* hyperBeamAnimation=findBattleAnimation(63);
  assert(hyperBeamAnimation&&std::strcmp(hyperBeamAnimation->assets[0],"orbs")==0&&
         hyperBeamAnimation->frameCounts[0]==3&&hyperBeamAnimation->particleCount>=6);
  // PSYBEAM launches eleven overlapping GoldRing sprites in FireRed. This
  // catches both the old generic four-particle descriptor and stale firmware
  // objects that PlatformIO previously linked after regenerating the catalog.
  const BattleAnimationData* psybeamAnimation=findBattleAnimation(60);
  assert(psybeamAnimation&&std::strcmp(psybeamAnimation->assets[0],"gold_ring")==0&&
         psybeamAnimation->particleCount==11);
  // These are chronological source calls, not the old capped particle hint.
  assert(countDefaultAnimationOpcode(60,BattleAnimationOpcode::SpawnSprite)==11); // Psybeam rings
  assert(countDefaultAnimationOpcode(63,BattleAnimationOpcode::SpawnSprite)>=20); // Hyper Beam orbs + impact
  assert(countDefaultAnimationOpcode(126,BattleAnimationOpcode::SpawnSprite)>=20); // Fire Blast helpers repeat
  assert(countDefaultAnimationOpcode(311,BattleAnimationOpcode::SpawnSprite)>=3); // neutral Weather path; runtime selects weather branch
  assert(countDefaultAnimationOpcode(94,BattleAnimationOpcode::VisualTask)>=3); // task-only Psychic stays visible
  const BattleAnimationData* psychicAnimation=findBattleAnimation(94);
  assert(psychicAnimation&&psychicAnimation->assetCount==0&&
         (psychicAnimation->flags&AnimBlend)&&
         (psychicAnimation->flags&AnimShakeTarget)&&
         (psychicAnimation->flags&AnimBackgroundChange));
  const BattleAnimationData* iceBeamAnimation=findBattleAnimation(58);
  assert(iceBeamAnimation&&iceBeamAnimation->assetCount==1&&
         std::strcmp(iceBeamAnimation->assets[0],"ice_crystals")==0&&
         iceBeamAnimation->particleCount==12);
  const BattleAnimationData* blizzardAnimation=findBattleAnimation(59);
  assert(blizzardAnimation&&blizzardAnimation->assetCount==1&&
         std::strcmp(blizzardAnimation->assets[0],"ice_crystals")==0&&
         blizzardAnimation->frameCounts[0]==5);
  // SHADOW BALL's three arguments are phase durations. It must use the exact
  // retained attacker->centre->target callback rather than a local/generic
  // projectile that mistakes 16,16 for pixel offsets.
  bool shadowBallMotionFound=false;
  visitDefaultAnimationPath(247,[&](const BattleAnimationCommand& command){
    if(static_cast<BattleAnimationOpcode>(command.opcode)!=
       BattleAnimationOpcode::SpawnSprite)return;
    const BattleAnimationSpriteResource* resource=
        battleAnimationSpriteResource(command.resourceId);
    if(!resource||std::strcmp(resource->asset,"tpl_bd0d98e07a81")!=0)return;
    const int16_t* arguments=battleAnimationCommandArguments(command);
    assert(resource->motion==BattleAnimationMotion::ShadowBall);
    assert(resource->origin==BattleAnimationOrigin::Attacker);
    assert(resource->lifetimeFrames==41);
    assert(arguments&&command.argumentCount==3&&
           arguments[0]==16&&arguments[1]==16&&arguments[2]==8);
    shadowBallMotionFound=true;
  });
  assert(shadowBallMotionFound);
  // Cross-battler callbacks must retain their original argument ABI.  A
  // generic Projectile used to make all of these compile while silently
  // ignoring target offsets/duration or anchoring the effect on one battler.
  const auto assertMoveUsesMotion = [](uint16_t moveId,
                                       BattleAnimationMotion expected) {
    bool found = false;
    visitDefaultAnimationPath(moveId, [&](const BattleAnimationCommand& command) {
      if (static_cast<BattleAnimationOpcode>(command.opcode) !=
          BattleAnimationOpcode::SpawnSprite) return;
      const BattleAnimationSpriteResource* resource =
          battleAnimationSpriteResource(command.resourceId);
      if (resource && resource->motion == expected) found = true;
    });
    assert(found);
  };
  assertMoveUsesMotion(52, BattleAnimationMotion::TargetLocationProjectile);  // EMBER
  assertMoveUsesMotion(2, BattleAnimationMotion::DiagonalStrike);             // KARATE CHOP
  assertMoveUsesMotion(11, BattleAnimationMotion::ViceGripPincer);            // VICE GRIP
  assertMoveUsesMotion(12, BattleAnimationMotion::GuillotinePincer);          // GUILLOTINE
  assertMoveUsesMotion(28, BattleAnimationMotion::DirtProjectile);            // SAND ATTACK
  assertMoveUsesMotion(80, BattleAnimationMotion::PetalDanceBig);             // PETAL DANCE
  assertMoveUsesMotion(82, BattleAnimationMotion::DragonFire);                // DRAGON RAGE
  assertMoveUsesMotion(141, BattleAnimationMotion::LeechLifeNeedle);          // LEECH LIFE
  assertMoveUsesMotion(170, BattleAnimationMotion::TargetConverge);           // MIND READER
  assertMoveUsesMotion(200, BattleAnimationMotion::OutrageFlame);             // OUTRAGE
  assertMoveUsesMotion(207, BattleAnimationMotion::BreathPuff);               // SWAGGER
  assertMoveUsesMotion(215, BattleAnimationMotion::HealBellNote);             // HEAL BELL
  assertMoveUsesMotion(219, BattleAnimationMotion::GuardRingRise);            // SAFEGUARD
  assertMoveUsesMotion(220, BattleAnimationMotion::PainSplitBounce);           // PAIN SPLIT
  assertMoveUsesMotion(224, BattleAnimationMotion::MegahornStrike);           // MEGAHORN
  assertMoveUsesMotion(225, BattleAnimationMotion::DragonFire);                // DRAGON BREATH
  assertMoveUsesMotion(296, BattleAnimationMotion::TargetLocationProjectile); // MIST BALL
  assertMoveUsesMotion(311, BattleAnimationMotion::WeatherBallDown);          // WEATHER BALL
  assertMoveUsesMotion(344, BattleAnimationMotion::VoltTackleSlide);          // VOLT TACKLE
  assertMoveUsesMotion(350, BattleAnimationMotion::TargetLocationProjectile); // ROCK BLAST
  for (uint16_t id = 0; id < battleAnimationSpriteResourceCount(); ++id) {
    const BattleAnimationSpriteResource* resource = battleAnimationSpriteResource(id);
    assert(resource);
    assert(!(resource->motion == BattleAnimationMotion::Projectile &&
             resource->travel == BattleAnimationTravel::Local));
  }
  bool skyAttackBirdFound = false;
  for (uint16_t id = 0; id < battleAnimationSpriteResourceCount(); ++id) {
    const BattleAnimationSpriteResource* resource = battleAnimationSpriteResource(id);
    if (resource && resource->motion == BattleAnimationMotion::SkyAttackBird &&
        std::strcmp(resource->asset, "tpl_4487b8515995") == 0)
      skyAttackBirdFound = true;
  }
  assert(skyAttackBirdFound);
  bool painSplitHasBothOwners = false;
  uint8_t painSplitAttacker = 0, painSplitTarget = 0;
  visitDefaultAnimationPath(220, [&](const BattleAnimationCommand& command) {
    if (static_cast<BattleAnimationOpcode>(command.opcode) !=
        BattleAnimationOpcode::SpawnSprite) return;
    const BattleAnimationSpriteResource* resource =
        battleAnimationSpriteResource(command.resourceId);
    if (!resource || resource->motion != BattleAnimationMotion::PainSplitBounce) return;
    assert(resource->origin == BattleAnimationOrigin::Argument2);
    const int16_t* arguments = battleAnimationCommandArguments(command);
    assert(arguments && command.argumentCount >= 3U);
    if (arguments[2] == 0) ++painSplitAttacker;
    else ++painSplitTarget;
    painSplitHasBothOwners = painSplitAttacker > 0U && painSplitTarget > 0U;
  });
  assert(painSplitHasBothOwners);
  // Animation graphics are resolved through FireRed's exact ANIM_TAG tables,
  // never by fuzzy filename similarity. These were all wrong in the former
  // generator and are representative palette/composite-sheet regressions.
  assert(std::strcmp(findBattleAnimation(8)->assets[0],"ice_crystals")==0); // ICE PUNCH
  assert(std::strcmp(findBattleAnimation(22)->assets[0],"whip_hit")==0);    // VINE WHIP
  assert(std::strcmp(findBattleAnimation(78)->assets[0],"stun_spore")==0); // palette variant
  assert(findBattleAnimation(134)->frameCounts[0]==7);                     // ALERT atlas
  assert(std::strcmp(findBattleAnimation(229)->assets[1], "rapid_spin") == 0&&
         findBattleAnimation(229)->frameCounts[1]==3);                     // RAPID SPIN atlas
  const BattleAnimationData* triAttackAnimation=findBattleAnimation(161);
  assert(triAttackAnimation&&triAttackAnimation->assetCount==4&&
         std::strcmp(triAttackAnimation->assets[3],"ice_crystals")==0);
  uint8_t triAttackTriangleCount=0,triAttackFlameCount=0;
  uint8_t triAttackLightningCount=0,triAttackIceCount=0;
  visitDefaultAnimationPath(161,[&](const BattleAnimationCommand& command){
    if(static_cast<BattleAnimationOpcode>(command.opcode)!=
       BattleAnimationOpcode::SpawnSprite)return;
    const BattleAnimationSpriteResource* resource=
        battleAnimationSpriteResource(command.resourceId);
    assert(resource);
    if(std::strcmp(resource->asset,"tpl_8e57dde4739f")==0)++triAttackTriangleCount;
    else if(std::strcmp(resource->asset,"tpl_548b4df0c67a")==0)++triAttackFlameCount;
    else if(std::strcmp(resource->asset,"tpl_a1c712fdbe40")==0)++triAttackLightningCount;
    else if(std::strcmp(resource->asset,"ice_crystal_hit_large")==0||
            std::strcmp(resource->asset,"ice_crystal_hit_small")==0)++triAttackIceCount;
  });
  assert(triAttackTriangleCount==1&&triAttackFlameCount==8&&
         triAttackLightningCount==3&&triAttackIceCount==7);
  const BattleAnimationData* brickBreakAnimation=findBattleAnimation(280);
  assert(brickBreakAnimation&&brickBreakAnimation->assetCount==4&&
         std::strcmp(brickBreakAnimation->assets[3],"torn_metal")==0&&
         brickBreakAnimation->frameCounts[3]==4);
  assert(std::strstr(moveDescription(static_cast<MoveId>(97)),"SPEED"));       // AGILITY
  assert(std::strstr(moveDescription(static_cast<MoveId>(347)),"SP. ATK"));   // CALM MIND
  assert(std::strstr(moveDescription(static_cast<MoveId>(347)),"SP. DEF"));
  assert(trainerProfileCount() >= 100);
  for (uint8_t index = 0; index < trainerProfileCount(); ++index) {
    const TrainerProfile* profile = trainerProfile(index);
    assert(profile && profile->trainerClass[0] && profile->name[0] && profile->frontAsset[0]);
    assert(profile->speciesCount >= 1 && profile->speciesCount <= 3);
    for (uint8_t member = 0; member < profile->speciesCount; ++member) assert(findSpecies(profile->species[member]));
  }
  for (uint16_t speciesId = 1; speciesId <= 386; ++speciesId) assert(findSpecies(speciesId));
  // Wild rarity has a meaningful gap between legendary (1), rare ordinary
  // Pokemon (3+) and Eevee, whose branching family needs repeated captures.
  assert(encounterWeight(133)==10); // Eevee
  assert(encounterWeight(132)==6);  // Ditto
  assert(encounterWeight(147)==6&&encounterWeight(149)==3); // Dratini family
  assert(encounterWeight(201)==10); // Unown
  assert(encounterWeight(246)==6&&encounterWeight(248)==3); // Larvitar family
  assert(encounterWeight(349)==5); // Feebas
  assert(encounterWeight(371)==6&&encounterWeight(373)==3); // Bagon family
  assert(encounterWeight(374)==6&&encounterWeight(376)==3); // Beldum family
  for(uint16_t legendaryId: {144,145,146,150,151,243,244,245,249,250,251,
                             377,378,379,380,381,382,383,384,385,386})
    assert(encounterWeight(legendaryId)==1);
  for(uint16_t speciesId=1;speciesId<=386;++speciesId){
    const uint8_t weight=encounterWeight(speciesId);
    if(weight&&weight!=1)assert(weight>=3);
  }
  for (uint16_t speciesId = 1; speciesId <= 386; ++speciesId) {
    const PokedexEntryData* entry = pokedexEntry(speciesId);
    assert(entry && entry->category[0] && entry->description[0] && entry->heightDecimeters && entry->weightHectograms);
  }
  const EvolutionData* pikachuEvolution = evolutionFor(25);
  assert(pikachuEvolution && pikachuEvolution->toSpeciesId == 26 && pikachuEvolution->level == 36);
  auto hasEvolutionChoice=[](const uint16_t choices[],uint8_t count,uint16_t target){for(uint8_t i=0;i<count;++i)if(choices[i]==target)return true;return false;};
  uint16_t choices[kEvolutionChoiceCapacity]{};
  uint8_t choiceCount=evolutionChoices(133,36,1,1,1,choices,kEvolutionChoiceCapacity);
  assert(choiceCount==3&&hasEvolutionChoice(choices,choiceCount,134)&&hasEvolutionChoice(choices,choiceCount,135)&&hasEvolutionChoice(choices,choiceCount,136));
  choiceCount=evolutionChoices(133,35,1,1,1,choices,kEvolutionChoiceCapacity);
  assert(choiceCount==0);
  choiceCount=evolutionChoices(133,30,1,1,2,choices,kEvolutionChoiceCapacity);
  assert(choiceCount==5&&hasEvolutionChoice(choices,choiceCount,196)&&hasEvolutionChoice(choices,choiceCount,197));
  choiceCount=evolutionChoices(61,36,1,1,1,choices,kEvolutionChoiceCapacity);
  assert(choiceCount==1&&choices[0]==62);
  choiceCount=evolutionChoices(61,36,1,1,2,choices,kEvolutionChoiceCapacity);
  assert(choiceCount==2&&hasEvolutionChoice(choices,choiceCount,62)&&hasEvolutionChoice(choices,choiceCount,186));
  choiceCount=evolutionChoices(79,37,1,1,2,choices,kEvolutionChoiceCapacity);
  assert(choiceCount==2&&hasEvolutionChoice(choices,choiceCount,80)&&hasEvolutionChoice(choices,choiceCount,199));
  choiceCount=evolutionChoices(236,20,15,10,3,choices,kEvolutionChoiceCapacity);assert(choiceCount==1&&choices[0]==106);
  choiceCount=evolutionChoices(236,20,10,15,3,choices,kEvolutionChoiceCapacity);assert(choiceCount==1&&choices[0]==107);
  choiceCount=evolutionChoices(236,20,10,10,3,choices,kEvolutionChoiceCapacity);assert(choiceCount==1&&choices[0]==237);
  assert(findSpecies(150)->catchRate == 3);
  assert(evolutionFor(152) && evolutionFor(152)->toSpeciesId == 153);
  assert(CollectionLogic::isShinyRoll(0)&&CollectionLogic::isShinyRoll(8192)&&!CollectionLogic::isShinyRoll(1));
  assert(CollectionLogic::createPokemon(99,25,5,true).shiny);

  // Unown's letter is part of the individual identity, not a species-wide A
  // placeholder. A varied stream of encounter seeds must produce varied
  // forms, and save normalization must reconstruct the same FireRed letter
  // from personality for older records.
  bool unownFormsSeen[28]{};
  uint8_t distinctUnownForms = 0;
  for (uint32_t seed = 1; seed <= 128; ++seed) {
    OwnedPokemon unown = CollectionLogic::createPokemon(seed, 201, 10, false, seed);
    const uint8_t form = CollectionLogic::resolvedForm(unown);
    assert(form < 28U && std::strcmp(CollectionLogic::formName(201, form), "NORMAL") != 0);
    if (!unownFormsSeen[form]) { unownFormsSeen[form] = true; ++distinctUnownForms; }
    const uint8_t expected = form;
    unown.form = 0;
    CollectionLogic::normalizeForm(unown);
    assert(CollectionLogic::resolvedForm(unown) == expected);
  }
  assert(distinctUnownForms >= 20U);

  // Deoxys changes form outside battle. The same individual retains its UID,
  // level and training while the Generation-III form bases are recalculated.
  OwnedPokemon deoxys = CollectionLogic::createPokemon(900, 386, 50, false, 1234);
  assert(CollectionLogic::canChangeForm(deoxys));
  assert(std::strcmp(CollectionLogic::formName(386, 0), "NORMAL") == 0);
  assert(CollectionLogic::setForm(deoxys, 1));
  const uint16_t attackFormAttack = CollectionLogic::calculatedStat(deoxys, PokemonStat::Attack);
  const uint16_t attackFormDefense = CollectionLogic::calculatedStat(deoxys, PokemonStat::Defense);
  const uint16_t attackFormSpeed = CollectionLogic::calculatedStat(deoxys, PokemonStat::Speed);
  assert(attackFormAttack > attackFormDefense);
  assert(CollectionLogic::setForm(deoxys, 2));
  assert(CollectionLogic::calculatedStat(deoxys, PokemonStat::Defense) > attackFormDefense);
  assert(CollectionLogic::setForm(deoxys, 3));
  assert(CollectionLogic::calculatedStat(deoxys, PokemonStat::Speed) > attackFormSpeed);
  assert(!CollectionLogic::setForm(deoxys, 4));

  // Each Pokemon receives one permanent Gen-III identity. IVs are always
  // 0..31, Nature follows the original 25-entry matrix, and Ability uses the
  // personality bit instead of changing with level or Box position.
  const OwnedPokemon individual = CollectionLogic::createPokemon(42, 19, 50, false, 0x12345678U);
  const OwnedPokemon sameIndividual = CollectionLogic::createPokemon(42, 19, 50, false, 0x12345678U);
  const OwnedPokemon otherIndividual = CollectionLogic::createPokemon(42, 19, 50, false, 0x87654321U);
  assert(individual.personality == sameIndividual.personality);
  assert(individual.personality != otherIndividual.personality);
  assert(std::memcmp(&individual.ivs, &sameIndividual.ivs, sizeof(IndividualValues)) == 0);
  assert(individual.ivs.hp <= 31 && individual.ivs.attack <= 31 && individual.ivs.defense <= 31);
  assert(individual.ivs.spAttack <= 31 && individual.ivs.spDefense <= 31 && individual.ivs.speed <= 31);
  assert(static_cast<uint8_t>(individual.nature) < 25);
  assert(CollectionLogic::natureName(individual.nature)[0]);
  const SpeciesData* individualSpecies = findSpecies(individual.speciesId);
  assert(individual.abilityId == (individualSpecies->ability2 && (individual.personality & 1U)
      ? individualSpecies->ability2 : individualSpecies->ability1));
  assert(findSpecies(1)->evYieldSpAttack == 1);
  assert(findSpecies(1)->evYieldHp == 0);

  OwnedPokemon calculated = CollectionLogic::createPokemon(1, 1, 50, false, 77);
  calculated.ivs = IndividualValues{31, 31, 31, 31, 31, 31};
  calculated.nature = PokemonNature::Hardy;
  CollectionLogic::refreshDerivedStats(calculated, false);
  assert(calculated.maximumHp == 120);
  assert(CollectionLogic::calculatedStat(calculated, PokemonStat::Attack) == 69);
  calculated.nature = PokemonNature::Adamant;
  assert(CollectionLogic::natureEffect(calculated.nature, PokemonStat::Attack) == 1);
  assert(CollectionLogic::natureEffect(calculated.nature, PokemonStat::SpAttack) == -1);
  assert(CollectionLogic::calculatedStat(calculated, PokemonStat::Attack) == 75);
  assert(CollectionLogic::calculatedStat(calculated, PokemonStat::SpAttack) == 76);
  // Gen-III EVs contribute floor(EV / 4) to the stat formula, cap at 255 per
  // stat and 510 total, and immediately refresh the derived HP maximum.
  calculated.nature = PokemonNature::Hardy;
  calculated.evs = EffortValues{255, 255, 0, 0, 0, 0};
  CollectionLogic::refreshDerivedStats(calculated, false);
  assert(CollectionLogic::totalEffortValues(calculated) == 510);
  assert(calculated.maximumHp == 152);
  assert(CollectionLogic::calculatedStat(calculated, PokemonStat::Attack) == 101);
  assert(CollectionLogic::grantEffortValues(calculated, EffortValues{1, 1, 1, 1, 1, 1}) == 0);
  // The Lv.90 editor may only move EVs that have already been earned. It
  // preserves Gen-III's exact 510-point total and refreshes derived HP while
  // retaining the amount of damage already taken.
  OwnedPokemon redistributed = CollectionLogic::createPokemon(3, 1, 90, false, 99);
  redistributed.evs = EffortValues{100, 100, 50, 100, 100, 60};
  CollectionLogic::refreshDerivedStats(redistributed, false);
  redistributed.currentHp = static_cast<uint16_t>(redistributed.maximumHp - 20U);
  assert(CollectionLogic::redistributeEffortValues(
      redistributed, EffortValues{0, 200, 55, 100, 100, 55}));
  assert(CollectionLogic::totalEffortValues(redistributed) == 510);
  assert(redistributed.evs.hp == 0 && redistributed.evs.attack == 200);
  assert(redistributed.currentHp + 20U == redistributed.maximumHp);
  const EffortValues acceptedSpread = redistributed.evs;
  assert(!CollectionLogic::redistributeEffortValues(
      redistributed, EffortValues{0, 200, 55, 100, 100, 54}));
  assert(std::memcmp(&redistributed.evs, &acceptedSpread, sizeof(EffortValues)) == 0);
  OwnedPokemon tooYoung = CollectionLogic::createPokemon(4, 1, 89, false, 100);
  assert(!CollectionLogic::redistributeEffortValues(tooYoung, tooYoung.evs));
  OwnedPokemon naturalTraining = CollectionLogic::createPokemon(2, 1, 20, false, 88);
  assert(CollectionLogic::grantEffortValues(naturalTraining, EffortValues{0, 0, 0, 1, 0, 0}) == 1);
  assert(naturalTraining.evs.spAttack == 1 && CollectionLogic::totalEffortValues(naturalTraining) == 1);
  // Pokegochi's shiny bonus expands only the total pool by another 255 EVs.
  // The original 255 cap on each individual stat remains intact.
  OwnedPokemon shinyTraining = CollectionLogic::createPokemon(2, 1, 90, true, 188);
  shinyTraining.evs = EffortValues{255, 255, 0, 0, 0, 0};
  assert(CollectionLogic::maximumTotalEffortValues(shinyTraining) == 765);
  assert(CollectionLogic::grantEffortValues(
      shinyTraining, EffortValues{0, 0, 255, 0, 0, 0}) == 255);
  assert(CollectionLogic::totalEffortValues(shinyTraining) == 765);
  assert(shinyTraining.evs.defense == 255);
  assert(CollectionLogic::grantEffortValues(
      shinyTraining, EffortValues{0, 0, 0, 1, 0, 0}) == 0);
  assert(CollectionLogic::redistributeEffortValues(
      shinyTraining, EffortValues{255, 0, 255, 0, 0, 255}));
  assert(CollectionLogic::totalEffortValues(shinyTraining) == 765);
  OwnedPokemon ordinaryAtCap = CollectionLogic::createPokemon(2, 1, 90, false, 189);
  ordinaryAtCap.evs = EffortValues{255, 255, 0, 0, 0, 0};
  assert(CollectionLogic::maximumTotalEffortValues(ordinaryAtCap) == 510);
  assert(CollectionLogic::grantEffortValues(
      ordinaryAtCap, EffortValues{0, 0, 1, 0, 0, 0}) == 0);
  GymProgress eggGyms;eggGyms.badgeBits=0x7;EggState egg;
  assert(EggSystem::daycareUnlocked(eggGyms,1));assert(EggSystem::offerRegionalGift(egg,eggGyms,123));
  assert(egg.offerPending&&egg.speciesId&&egg.speciesId!=1&&egg.speciesId!=4&&egg.speciesId!=7);
  assert(EggSystem::accept(egg)&&egg.active&&egg.remainingSeconds>=EggSystem::kCommonSeconds);
  const uint32_t eggStart=egg.remainingSeconds;assert(EggSystem::addInteractionBonus(egg)==600&&egg.remainingSeconds==eggStart-600);
  EggSystem::advance(egg,egg.remainingSeconds);assert(EggSystem::ready(egg));
  assert(EggSystem::hatchLevel(GymProgress{})==5);
  assert(EggSystem::hatchLevel(eggGyms)==24);
  GymProgress lateEggGyms;lateEggGyms.badgeBits=(1UL<<23U);
  assert(EggSystem::hatchLevel(lateEggGyms)==89);
  PokemonCollection eggBox;CollectionLogic::initialize(eggBox);
  assert(EggSystem::hatch(egg,eggBox,eggGyms));
  assert(CollectionLogic::count(eggBox)==1&&!egg.active);
  assert(eggBox.box[0].level==24);
  for (uint32_t roll=0;roll<5000;++roll) {
    const uint16_t kanto=chooseEncounterSpecies(100,1,roll);
    const uint16_t johto=chooseEncounterSpecies(100,2,roll);
    const uint16_t hoenn=chooseEncounterSpecies(100,3,roll);
    assert(kanto<=151&&kanto!=1&&kanto!=4&&kanto!=7);
    assert(johto<=251&&johto!=152&&johto!=155&&johto!=158);
    assert(hoenn<=386&&hoenn!=252&&hoenn!=255&&hoenn!=258);
  }
  PokemonCollection collection;
  CollectionLogic::initialize(collection);
  assert(CollectionLogic::count(collection) == 0);
  assert(CollectionLogic::chooseStarter(collection, 4));
  assert(!CollectionLogic::chooseStarter(collection, 7));
  assert(CollectionLogic::validate(collection));
  assert(CollectionLogic::active(collection, 0)->speciesId == 4);
  OwnedPokemon* starter = CollectionLogic::active(collection, 0);
  starter->status = StatusCondition::Poison;
  starter->currentHp = static_cast<uint16_t>(starter->maximumHp / 2U);
  starter->recoverySecondsRemaining = 9000;
  starter->movePp[0] = 0;
  CollectionLogic::care(*starter, CareAction::Center);
  assert(starter->status == StatusCondition::None);
  assert(starter->currentHp == starter->maximumHp);
  assert(starter->recoverySecondsRemaining == 0);
  assert(starter->movePp[0] == findFullMove(starter->moves[0])->pp);

  // PC sorting must compact holes without changing Party identity. Dex order
  // is ascending; equal species use descending level. Level order is strongest
  // first and remains deterministic for ties.
  PokemonCollection sortedCollection;
  CollectionLogic::initialize(sortedCollection);
  assert(CollectionLogic::chooseStarter(sortedCollection, 4, 0x501U));
  uint32_t lowPikachuUid = 0, bulbasaurUid = 0, highPikachuUid = 0;
  assert(CollectionLogic::add(sortedCollection,
      CollectionLogic::createPokemon(0, 25, 12, false, 0x502U), &lowPikachuUid));
  assert(CollectionLogic::add(sortedCollection,
      CollectionLogic::createPokemon(0, 1, 50, false, 0x503U), &bulbasaurUid));
  assert(CollectionLogic::add(sortedCollection,
      CollectionLogic::createPokemon(0, 25, 30, false, 0x504U), &highPikachuUid));
  assert(CollectionLogic::setPartySlot(sortedCollection, 1, lowPikachuUid));
  std::swap(sortedCollection.box[1], sortedCollection.box[20]);
  const uint32_t partyOneBeforeSort = sortedCollection.party[1];

  CollectionLogic::sortBox(sortedCollection, BoxSortMode::DexNumber);
  assert(sortedCollection.box[0].speciesId == 1);
  assert(sortedCollection.box[1].speciesId == 4);
  assert(sortedCollection.box[2].uid == highPikachuUid);
  assert(sortedCollection.box[3].uid == lowPikachuUid);
  assert(!sortedCollection.box[4].uid);
  assert(sortedCollection.party[1] == partyOneBeforeSort);
  assert(CollectionLogic::active(sortedCollection, 1)->uid == lowPikachuUid);
  assert(CollectionLogic::validate(sortedCollection));

  CollectionLogic::sortBox(sortedCollection, BoxSortMode::Level);
  assert(sortedCollection.box[0].uid == bulbasaurUid);
  assert(sortedCollection.box[1].uid == highPikachuUid);
  assert(sortedCollection.box[2].uid == lowPikachuUid);
  assert(sortedCollection.box[3].speciesId == 4);
  assert(!sortedCollection.box[4].uid);
  assert(sortedCollection.party[1] == partyOneBeforeSort);
  assert(CollectionLogic::validate(sortedCollection));

  EncounterCharges charges;
  BattleState battle;
  const uint32_t starterUid = collection.party[0];
  assert(BattleEngine::startWild(battle, collection, starterUid, 12345));
  assert(charges.available == 3);  // Random wild encounters never use charges.
  assert(battle.active && battle.kind == BattleKind::Wild && BattleEngine::currentOpponent(battle));
  assert(battle.rewardMoney == 0);
  assert(BattleEngine::currentOpponent(battle)->level >= 2 &&
         BattleEngine::currentOpponent(battle)->level <= 4);
  bool sawBeginnerLow = false, sawBeginnerHigh = false;
  for (uint32_t seed = 1; seed <= 128; ++seed) {
    BattleState beginnerWild;
    assert(BattleEngine::startWild(beginnerWild, collection, starterUid, seed, 1, true));
    const OwnedPokemon* opponent = BattleEngine::currentOpponent(beginnerWild);
    assert(opponent && opponent->level >= 2 && opponent->level <= 4);
    // The protected opening deliberately draws only from the first Lv.5
    // encounter pool, which contains base-form species.
    assert(minimumEncounterLevel(opponent->speciesId) == 5);
    sawBeginnerLow |= opponent->level == 2;
    sawBeginnerHigh |= opponent->level == 4;
  }
  assert(sawBeginnerLow && sawBeginnerHigh);

  // Pokegochi's opening boost doubles the equally-shared award while the
  // strongest active partner is below Lv.10, then returns to FireRed's
  // ordinary formula immediately at Lv.10.
  const auto verifyEarlyExperienceMultiplier = [](uint8_t playerLevel,
                                                   uint8_t expectedMultiplier) {
    PokemonCollection xpCollection;
    CollectionLogic::initialize(xpCollection);
    assert(CollectionLogic::chooseStarter(xpCollection, 1, 0xE410U + playerLevel));
    OwnedPokemon* player = CollectionLogic::active(xpCollection, 0);
    assert(player);
    player->level = playerLevel;
    player->experience = experienceForLevel(findSpecies(player->speciesId)->growthRate,
                                             playerLevel);
    player->moves[0] = static_cast<MoveId>(129);  // Swift cannot miss Rattata.
    player->movePp[0] = 20;
    CollectionLogic::refreshDerivedStats(*player, false);

    BattleState xpBattle{};
    xpBattle.active = true;
    xpBattle.kind = BattleKind::Wild;
    xpBattle.outcome = BattleOutcome::Ongoing;
    xpBattle.playerUid = player->uid;
    xpBattle.opponentCount = 1;
    xpBattle.rngState = 0xE420U + playerLevel;
    xpBattle.opponents[0] = CollectionLogic::createPokemon(
        0xE430U + playerLevel, 19, 5, false, 0xE440U + playerLevel);
    xpBattle.opponents[0].currentHp = 1;
    for (uint8_t slot = 0; slot < kMoveSlots; ++slot)
      xpBattle.opponents[0].moves[slot] = MoveId::None;

    const uint32_t baseAward =
        (static_cast<uint32_t>(findSpecies(19)->baseExperience) * 5U) / 7U;
    const BattleActionResult xpResult = BattleEngine::fight(xpBattle, xpCollection, 0);
    assert(xpResult.accepted && xpResult.opponentDefeated);
    assert(xpResult.experienceGained == baseAward * expectedMultiplier);
  };
  verifyEarlyExperienceMultiplier(9, 2);
  verifyEarlyExperienceMultiplier(10, 1);

  // Lucky Egg is deliberately a party-wide Pokegochi bonus. If any selected
  // partner holds it, the total award doubles before being split, and
  // every selected Pokemon still receives exactly the same amount.
  PokemonCollection luckyXpCollection;
  CollectionLogic::initialize(luckyXpCollection);
  assert(CollectionLogic::chooseStarter(luckyXpCollection, 1, 0x1E660001U));
  OwnedPokemon* luckyLead = CollectionLogic::active(luckyXpCollection, 0);
  assert(luckyLead);
  luckyLead->level = 20;
  luckyLead->experience = experienceForLevel(
      findSpecies(luckyLead->speciesId)->growthRate, luckyLead->level);
  luckyLead->moves[0] = static_cast<MoveId>(129);  // SWIFT
  luckyLead->movePp[0] = 20;
  CollectionLogic::refreshDerivedStats(*luckyLead, false);
  uint32_t luckyPartnerUid = 0;
  assert(CollectionLogic::add(
      luckyXpCollection,
      CollectionLogic::createPokemon(0, 25, 20, false, 0x1E660002U),
      &luckyPartnerUid));
  assert(CollectionLogic::setPartySlot(luckyXpCollection, 1, luckyPartnerUid));
  OwnedPokemon* luckyPartner = CollectionLogic::find(luckyXpCollection, luckyPartnerUid);
  assert(luckyPartner);
  luckyPartner->heldItem = HeldItem::LuckyEgg;
  const uint32_t luckyLeadXpBefore = luckyLead->experience;
  const uint32_t luckyPartnerXpBefore = luckyPartner->experience;
  BattleState luckyXpBattle{};
  luckyXpBattle.active = true;
  luckyXpBattle.kind = BattleKind::Wild;
  luckyXpBattle.outcome = BattleOutcome::Ongoing;
  luckyXpBattle.playerUid = luckyLead->uid;
  luckyXpBattle.opponentCount = 1;
  luckyXpBattle.rngState = 0x1E660003U;
  luckyXpBattle.opponents[0] = CollectionLogic::createPokemon(
      0x1E660004U, 19, 5, false, 0x1E660005U);
  luckyXpBattle.opponents[0].currentHp = 1;
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot)
    luckyXpBattle.opponents[0].moves[slot] = MoveId::None;
  const uint32_t luckyBaseTotal =
      (static_cast<uint32_t>(findSpecies(19)->baseExperience) * 5U) / 7U;
  const uint32_t luckyExpectedShare = (luckyBaseTotal * 2U) / 2U;
  const BattleActionResult luckyXpResult = BattleEngine::fight(
      luckyXpBattle, luckyXpCollection, 0);
  assert(luckyXpResult.accepted && luckyXpResult.opponentDefeated);
  assert(luckyXpResult.experienceGained == luckyExpectedShare);
  assert(luckyLead->experience - luckyLeadXpBefore == luckyExpectedShare);
  assert(luckyPartner->experience - luckyPartnerXpBefore == luckyExpectedShare);

  // Even an impossible/corrupted save with two equipped Eggs must not stack
  // the global modifier: possession is a yes/no party condition.
  luckyLead->heldItem = HeldItem::LuckyEgg;
  const uint32_t stackedLeadXpBefore = luckyLead->experience;
  const uint32_t stackedPartnerXpBefore = luckyPartner->experience;
  BattleState stackedLuckyBattle{};
  stackedLuckyBattle.active = true;
  stackedLuckyBattle.kind = BattleKind::Wild;
  stackedLuckyBattle.outcome = BattleOutcome::Ongoing;
  stackedLuckyBattle.playerUid = luckyLead->uid;
  stackedLuckyBattle.opponentCount = 1;
  stackedLuckyBattle.rngState = 0x1E660006U;
  stackedLuckyBattle.opponents[0] = CollectionLogic::createPokemon(
      0x1E660007U, 19, 5, false, 0x1E660008U);
  stackedLuckyBattle.opponents[0].currentHp = 1;
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot)
    stackedLuckyBattle.opponents[0].moves[slot] = MoveId::None;
  const BattleActionResult stackedLuckyResult = BattleEngine::fight(
      stackedLuckyBattle, luckyXpCollection, 0);
  assert(stackedLuckyResult.accepted && stackedLuckyResult.opponentDefeated);
  assert(stackedLuckyResult.experienceGained == luckyExpectedShare);
  assert(luckyLead->experience - stackedLeadXpBefore == luckyExpectedShare);
  assert(luckyPartner->experience - stackedPartnerXpBefore == luckyExpectedShare);

  // Until the strongest active partner passes Lv.20, a Wild Pokemon can
  // never match that partner's level. At Lv.21 the inclusive ceiling resumes.
  PokemonCollection cappedWildRange;
  CollectionLogic::initialize(cappedWildRange);
  assert(CollectionLogic::chooseStarter(cappedWildRange, 1));
  OwnedPokemon* cappedPartner = CollectionLogic::active(cappedWildRange, 0);
  cappedPartner->level = 20;
  cappedPartner->experience = experienceForLevel(findSpecies(1)->growthRate, 20);
  CollectionLogic::refreshDerivedStats(*cappedPartner, false);
  bool sawLevel19 = false;
  for (uint32_t seed = 1; seed <= 512; ++seed) {
    BattleState cappedWild;
    assert(BattleEngine::startWild(cappedWild, cappedWildRange, cappedPartner->uid,
                                   seed, 1, false));
    const uint8_t level = BattleEngine::currentOpponent(cappedWild)->level;
    assert(level >= 5 && level <= 19);
    sawLevel19 |= level == 19;
  }
  assert(sawLevel19);
  cappedPartner->level = 21;
  cappedPartner->experience = experienceForLevel(findSpecies(1)->growthRate, 21);
  CollectionLogic::refreshDerivedStats(*cappedPartner, false);
  bool sawLevel21 = false;
  for (uint32_t seed = 1; seed <= 512; ++seed) {
    BattleState uncappedWild;
    assert(BattleEngine::startWild(uncappedWild, cappedWildRange, cappedPartner->uid,
                                   seed, 1, false));
    const uint8_t level = BattleEngine::currentOpponent(uncappedWild)->level;
    assert(level >= 5 && level <= 21);
    sawLevel21 |= level == 21;
  }
  assert(sawLevel21);

  PokemonCollection broadWildRange;
  CollectionLogic::initialize(broadWildRange);
  assert(CollectionLogic::chooseStarter(broadWildRange, 1));
  OwnedPokemon* highLevelPartner = CollectionLogic::active(broadWildRange, 0);
  highLevelPartner->level = 32;
  highLevelPartner->experience = experienceForLevel(findSpecies(1)->growthRate, 32);
  CollectionLogic::refreshDerivedStats(*highLevelPartner, false);
  bool sawLowWild = false, sawHighWild = false;
  for (uint32_t seed = 1; seed <= 256; ++seed) {
    BattleState variedWild;
    assert(BattleEngine::startWild(variedWild, broadWildRange, highLevelPartner->uid, seed));
    const uint8_t level = BattleEngine::currentOpponent(variedWild)->level;
    const uint16_t wildSpecies = BattleEngine::currentOpponent(variedWild)->speciesId;
    assert(level >= 5 && level <= 32);
    assert(level >= minimumEncounterLevel(wildSpecies));
    // Regional starters and every evolution in their families are exclusive
    // gifts and can never leak into the wild table.
    static constexpr uint16_t starterFamilies[] = {
      1,2,3,4,5,6,7,8,9,152,153,154,155,156,157,158,159,160,
      252,253,254,255,256,257,258,259,260
    };
    for (uint16_t exclusive : starterFamilies) assert(wildSpecies != exclusive);
    sawLowWild |= level == 5;
    sawHighWild |= level == 32;
  }
  assert(sawLowWild && sawHighWild);

  // Late-game wild levels retain the complete Lv.5..party-level range, but
  // the weighted curve should produce substantially more useful high-quarter
  // encounters than bottom-quarter encounters around Lv.40.
  highLevelPartner->level = 40;
  highLevelPartner->experience = experienceForLevel(findSpecies(1)->growthRate, 40);
  CollectionLogic::refreshDerivedStats(*highLevelPartner, false);
  uint32_t wildLevelTotal = 0;
  uint16_t lowQuarterCount = 0, highQuarterCount = 0;
  bool stillSawVeryLowWild = false;
  constexpr uint16_t kCurveSamples = 2048;
  for (uint32_t seed = 1; seed <= kCurveSamples; ++seed) {
    BattleState curvedWild;
    assert(BattleEngine::startWild(curvedWild, broadWildRange, highLevelPartner->uid,
                                   0x40000000U + seed));
    const uint8_t level = BattleEngine::currentOpponent(curvedWild)->level;
    assert(level >= 5U && level <= 40U);
    wildLevelTotal += level;
    lowQuarterCount += level <= 13U ? 1U : 0U;
    highQuarterCount += level >= 32U ? 1U : 0U;
    stillSawVeryLowWild |= level <= 10U;
  }
  assert(stillSawVeryLowWild);
  assert(highQuarterCount > lowQuarterCount * 2U);
  assert(wildLevelTotal / kCurveSamples >= 25U);

  // Transform copies the opponent's battle form, already-calculated stats,
  // IVs, seven stat stages and four moves with min(base PP, 5), then restores
  // Ditto exactly when it leaves the field. HP, level and training stay its
  // own, and Disable is cleared by Cmd_transformdataexecution.
  PokemonCollection transformCollection; CollectionLogic::initialize(transformCollection);
  assert(CollectionLogic::chooseStarter(transformCollection,1));
  OwnedPokemon* ditto=CollectionLogic::active(transformCollection,0);
  const uint32_t dittoUid=ditto->uid;*ditto=CollectionLogic::createPokemon(dittoUid,132,10,false,33);
  ditto->moves[0]=static_cast<MoveId>(144);ditto->movePp[0]=10;
  uint32_t transformPartnerUid=0;
  assert(CollectionLogic::add(transformCollection,CollectionLogic::createPokemon(0,16,10),&transformPartnerUid));
  assert(CollectionLogic::setPartySlot(transformCollection,1,transformPartnerUid));
  BattleState transformBattle;transformBattle.active=true;transformBattle.kind=BattleKind::Trainer;
  transformBattle.outcome=BattleOutcome::Ongoing;transformBattle.playerUid=ditto->uid;transformBattle.opponentCount=1;
  transformBattle.opponents[0]=CollectionLogic::createPokemon(0,1,10,false,44);
  transformBattle.opponents[0].moves[0]=static_cast<MoveId>(166); // SKETCH has 1 base PP.
  for(uint8_t i=0;i<kMoveSlots;++i)transformBattle.opponents[0].movePp[i]=0;
  transformBattle.playerVolatile.attackStage=-3;transformBattle.playerVolatile.defenseStage=2;
  transformBattle.playerVolatile.disabledMove=static_cast<MoveId>(33);
  transformBattle.playerVolatile.sureHitTurns=0x30U; // packed Disable timer
  transformBattle.opponentVolatiles[0].attackStage=4;
  transformBattle.opponentVolatiles[0].defenseStage=-2;
  transformBattle.opponentVolatiles[0].spAttackStage=3;
  transformBattle.opponentVolatiles[0].spDefenseStage=-1;
  transformBattle.opponentVolatiles[0].speedStage=5;
  transformBattle.opponentVolatiles[0].accuracyStage=-4;
  transformBattle.opponentVolatiles[0].evasionStage=2;
  const uint16_t targetAttack=CollectionLogic::calculatedStat(
      transformBattle.opponents[0],PokemonStat::Attack);
  const uint8_t originalLevel=ditto->level;
  const IndividualValues originalIvs=ditto->ivs;
  const BattleActionResult transformResult=BattleEngine::fight(transformBattle,transformCollection,0);
  assert(transformResult.accepted);
  bool sawTransformFeedback=false;
  for(uint8_t eventIndex=0;eventIndex<transformResult.eventCount;++eventIndex){
    const BattleEvent& event=transformResult.events[eventIndex];
    if(event.type!=BattleEventType::MoveEffect)continue;
    assert(event.value!=static_cast<uint16_t>(BattleMoveEffect::Failed));
    if(event.value==static_cast<uint16_t>(BattleMoveEffect::Transformed)){
      sawTransformFeedback=true;
      assert(event.before==132&&event.after==1);
    }
  }
  assert(sawTransformFeedback);
  ditto=CollectionLogic::find(transformCollection,transformBattle.playerUid);
  assert(ditto&&ditto->speciesId==1&&transformBattle.playerTransformed);
  assert(ditto->moves[0]==transformBattle.opponents[0].moves[0]&&ditto->movePp[0]==1);
  assert(transformBattle.playerTransformSnapshot.active&&
         transformBattle.playerTransformSnapshot.attack==targetAttack&&
         transformBattle.playerTransformSnapshot.ivs.attack==transformBattle.opponents[0].ivs.attack);
  assert(ditto->level==originalLevel&&ditto->ivs.attack==originalIvs.attack);
  assert(transformBattle.playerVolatile.attackStage==4&&
         transformBattle.playerVolatile.defenseStage==-2&&
         transformBattle.playerVolatile.spAttackStage==3&&
         transformBattle.playerVolatile.spDefenseStage==-1&&
         transformBattle.playerVolatile.speedStage==5&&
         transformBattle.playerVolatile.accuracyStage==-4&&
         transformBattle.playerVolatile.evasionStage==2&&
         transformBattle.playerVolatile.disabledMove==MoveId::None&&
         (transformBattle.playerVolatile.sureHitTurns&0x70U)==0U);
  assert(BattleEngine::switchToPokemon(transformBattle,transformCollection,transformPartnerUid).accepted);
  ditto=CollectionLogic::find(transformCollection,transformCollection.party[0]);
  assert(ditto&&ditto->speciesId==132&&ditto->moves[0]==static_cast<MoveId>(144)&&ditto->movePp[0]==9&&
         !transformBattle.playerTransformSnapshot.active);

  // BATON PASS is a two-stage command on touch hardware: announce the move,
  // ask which party member should enter, then let the already-selected enemy
  // command hit that replacement. Only Gen-III passable volatile effects are
  // carried; recharge, Disable and Choice locks stay with neither battler.
  PokemonCollection batonCollection;CollectionLogic::initialize(batonCollection);
  assert(CollectionLogic::chooseStarter(batonCollection,1));
  OwnedPokemon* batonUser=CollectionLogic::active(batonCollection,0);
  batonUser->moves[0]=static_cast<MoveId>(226);batonUser->movePp[0]=40;
  uint32_t batonPartnerUid=0;
  assert(CollectionLogic::add(batonCollection,
      CollectionLogic::createPokemon(0,25,12,false,0xBA7001U),&batonPartnerUid));
  assert(CollectionLogic::setPartySlot(batonCollection,1,batonPartnerUid));
  BattleState batonBattle;batonBattle.active=true;batonBattle.kind=BattleKind::Trainer;
  batonBattle.outcome=BattleOutcome::Ongoing;batonBattle.playerUid=batonUser->uid;
  batonBattle.opponentCount=1;batonBattle.rngState=0xBA7002U;
  batonBattle.opponents[0]=CollectionLogic::createPokemon(0,19,5,false,0xBA7003U);
  for(uint8_t slot=0;slot<kMoveSlots;++slot)batonBattle.opponents[0].movePp[slot]=0;
  batonBattle.playerVolatile.attackStage=3;
  batonBattle.playerVolatile.speedStage=-2;
  batonBattle.playerVolatile.accuracyStage=1;
  batonBattle.playerVolatile.confusionTurns=3;
  batonBattle.playerVolatile.trappedTurns=4;
  batonBattle.playerVolatile.criticalStage=2;
  batonBattle.playerVolatile.substituteHp=7;
  batonBattle.playerVolatile.seeded=true;
  // Packed Gen-III volatiles: Ghost Curse and Mean Look are Baton-Passable;
  // Wrap/Bind is explicitly not.
  batonBattle.playerVolatile.sureHitTurns=0x0CU;
  batonBattle.playerVolatile.recharging=true;
  batonBattle.playerVolatile.disabledMove=MoveId::Tackle;
  batonBattle.playerVolatile.choiceMove=static_cast<MoveId>(226);
  batonBattle.playerMoveEffects.typeOverrideActive=true;
  batonBattle.playerMoveEffects.type1=batonBattle.playerMoveEffects.type2=PokemonType::Fire;
  batonBattle.playerMoveEffects.defenseCurl=true;
  batonBattle.playerMoveEffects.identified=true;
  batonBattle.playerMoveEffects.perishTurns=4;
  batonBattle.playerMoveEffects.ingrained=true;
  batonBattle.playerMoveEffects.mudSport=true;
  batonBattle.playerMoveEffects.waterSport=true;
  // Recharge would prevent the selected command, so clear it immediately
  // before use and verify it does not reappear on the recipient.
  batonBattle.playerVolatile.recharging=false;
  const uint32_t batonUserUid=batonUser->uid;
  const BattleActionResult batonUse=BattleEngine::fight(batonBattle,batonCollection,0);
  assert(batonUse.accepted&&batonUse.batonPassSwitchRequired);
  assert(batonBattle.playerUid==batonUserUid);
  bool sawBatonPass=false,sawBatonFailure=false;
  for(uint8_t eventIndex=0;eventIndex<batonUse.eventCount;++eventIndex){
    const BattleEvent& event=batonUse.events[eventIndex];
    sawBatonPass|=event.type==BattleEventType::MoveEffect&&
        event.value==static_cast<uint16_t>(BattleMoveEffect::BatonPass);
    sawBatonFailure|=event.type==BattleEventType::MoveEffect&&
        event.value==static_cast<uint16_t>(BattleMoveEffect::Failed);
  }
  assert(sawBatonPass&&!sawBatonFailure);
  // The passer previously used Lock-On on the opposing position. Baton Pass
  // refreshes that relation for the incoming ally instead of clearing it.
  batonBattle.opponentVolatiles[0].sureHitTurns=1U;
  const BattleActionResult batonSwitch=BattleEngine::completeBatonPassSwitch(
      batonBattle,batonCollection,batonPartnerUid,0xFEU);
  assert(batonSwitch.accepted&&batonBattle.playerUid==batonPartnerUid);
  assert(batonBattle.playerVolatile.attackStage==3);
  assert(batonBattle.playerVolatile.speedStage==-2);
  assert(batonBattle.playerVolatile.accuracyStage==1);
  assert(batonBattle.playerVolatile.confusionTurns==2);
  assert(batonBattle.playerVolatile.trappedTurns==0);
  assert((batonBattle.playerVolatile.sureHitTurns&0x0CU)==0x0CU);
  assert(batonBattle.playerVolatile.criticalStage==2);
  assert(batonBattle.playerVolatile.substituteHp==7);
  assert(batonBattle.playerVolatile.seeded);
  assert(!batonBattle.playerVolatile.recharging);
  assert(batonBattle.playerVolatile.disabledMove==MoveId::None);
  assert(batonBattle.playerVolatile.choiceMove==MoveId::None);
  assert(!batonBattle.playerMoveEffects.typeOverrideActive&&
         !batonBattle.playerMoveEffects.defenseCurl&&
         !batonBattle.playerMoveEffects.identified);
  assert(batonBattle.playerMoveEffects.perishTurns==3U&&
         batonBattle.playerMoveEffects.ingrained&&
         batonBattle.playerMoveEffects.mudSport&&
         batonBattle.playerMoveEffects.waterSport);
  assert((batonBattle.opponentVolatiles[0].sureHitTurns&0x03U)==1U);

  // Trainer AI uses the same mechanic and selects another living roster
  // member instead of reporting the move as failed.
  PokemonCollection enemyBatonCollection;CollectionLogic::initialize(enemyBatonCollection);
  assert(CollectionLogic::chooseStarter(enemyBatonCollection,1));
  OwnedPokemon* enemyBatonTarget=CollectionLogic::active(enemyBatonCollection,0);
  enemyBatonTarget->moves[0]=static_cast<MoveId>(150);enemyBatonTarget->movePp[0]=40;
  BattleState enemyBatonBattle;enemyBatonBattle.active=true;
  enemyBatonBattle.kind=BattleKind::Trainer;enemyBatonBattle.outcome=BattleOutcome::Ongoing;
  enemyBatonBattle.playerUid=enemyBatonTarget->uid;enemyBatonBattle.opponentCount=2;
  enemyBatonBattle.rngState=0xBA7004U;
  enemyBatonBattle.opponents[0]=CollectionLogic::createPokemon(0,19,10,false,0xBA7005U);
  enemyBatonBattle.opponents[1]=CollectionLogic::createPokemon(0,16,10,false,0xBA7006U);
  assert(BattleEngine::ensureOpponentUids(enemyBatonBattle));
  for(uint8_t index=0;index<2;++index)for(uint8_t slot=0;slot<kMoveSlots;++slot)
    enemyBatonBattle.opponents[index].movePp[slot]=0;
  enemyBatonBattle.opponents[0].moves[0]=static_cast<MoveId>(226);
  enemyBatonBattle.opponents[0].movePp[0]=40;
  enemyBatonBattle.opponentVolatiles[0].defenseStage=4;
  enemyBatonBattle.opponentMoveEffects[0].typeOverrideActive=true;
  enemyBatonBattle.opponentMoveEffects[0].defenseCurl=true;
  enemyBatonBattle.opponentMoveEffects[0].identified=true;
  enemyBatonBattle.opponentMoveEffects[0].perishTurns=4;
  enemyBatonBattle.opponentMoveEffects[0].ingrained=true;
  enemyBatonBattle.playerVolatile.sureHitTurns=2U;
  const uint32_t enemyBatonOutgoingUid=enemyBatonBattle.opponents[0].uid;
  const uint32_t enemyBatonIncomingUid=enemyBatonBattle.opponents[1].uid;
  const BattleActionResult enemyBatonResult=BattleEngine::fight(
      enemyBatonBattle,enemyBatonCollection,0);
  assert(enemyBatonResult.accepted);
  assert(enemyBatonBattle.opponents[0].uid==enemyBatonIncomingUid);
  assert(enemyBatonBattle.opponents[0].uid!=enemyBatonOutgoingUid);
  assert(enemyBatonBattle.opponentVolatiles[0].defenseStage==4);
  assert(!enemyBatonBattle.opponentMoveEffects[0].typeOverrideActive&&
         !enemyBatonBattle.opponentMoveEffects[0].defenseCurl&&
         !enemyBatonBattle.opponentMoveEffects[0].identified);
  assert(enemyBatonBattle.opponentMoveEffects[0].perishTurns==3U&&
         enemyBatonBattle.opponentMoveEffects[0].ingrained);
  assert((enemyBatonBattle.playerVolatile.sureHitTurns&0x03U)==2U);

  // SOFT-BOILED is a self-targeted half-HP recovery move. It must consume PP,
  // emit a visible player HP event and never append the generic failed result
  // when healing actually occurred.
  PokemonCollection healCollection; CollectionLogic::initialize(healCollection);
  assert(CollectionLogic::chooseStarter(healCollection,1));
  OwnedPokemon* healer=CollectionLogic::active(healCollection,0);
  healer->moves[0]=static_cast<MoveId>(135);healer->movePp[0]=10;
  healer->currentHp=static_cast<uint16_t>(healer->maximumHp/4U);
  const uint16_t healHpBefore=healer->currentHp;
  BattleState healBattle;healBattle.active=true;healBattle.kind=BattleKind::Trainer;
  healBattle.outcome=BattleOutcome::Ongoing;healBattle.playerUid=healer->uid;healBattle.opponentCount=1;
  healBattle.opponents[0]=CollectionLogic::createPokemon(0,19,5,false,0x135U);
  for(uint8_t slot=0;slot<kMoveSlots;++slot)healBattle.opponents[0].movePp[slot]=0;
  const BattleActionResult healResult=BattleEngine::fight(healBattle,healCollection,0);
  healer=CollectionLogic::active(healCollection,0);
  assert(healResult.accepted&&healer->movePp[0]==9);
  assert(healer->currentHp==std::min<uint16_t>(healer->maximumHp,
      static_cast<uint16_t>(healHpBefore+healer->maximumHp/2U)));
  bool sawSelfHeal=false;
  for(uint8_t eventIndex=0;eventIndex<healResult.eventCount;++eventIndex){
    const BattleEvent& event=healResult.events[eventIndex];
    if(event.type==BattleEventType::HpChanged&&event.side==BattleSide::Player&&
       event.before==healHpBefore&&event.after==healer->currentHp)sawSelfHeal=true;
    assert(event.type!=BattleEventType::MoveEffect||
           event.value!=static_cast<uint16_t>(BattleMoveEffect::Failed));
  }
  assert(sawSelfHeal);

  // The same effect has a separate, explicit field command. It transfers
  // exactly one fifth of the donor's max HP, never revives, and does not use
  // battle PP.
  PokemonCollection fieldHealCollection;CollectionLogic::initialize(fieldHealCollection);
  assert(CollectionLogic::chooseStarter(fieldHealCollection,1));
  OwnedPokemon* fieldHealer=CollectionLogic::active(fieldHealCollection,0);
  fieldHealer->moves[0]=static_cast<MoveId>(135);fieldHealer->movePp[0]=7;
  uint32_t fieldTargetUid=0;
  assert(CollectionLogic::add(fieldHealCollection,
      CollectionLogic::createPokemon(0,19,5,false,0x1355U),&fieldTargetUid));
  assert(CollectionLogic::setPartySlot(fieldHealCollection,1,fieldTargetUid));
  OwnedPokemon* fieldTarget=CollectionLogic::find(fieldHealCollection,fieldTargetUid);
  fieldTarget->currentHp=1;
  const uint16_t fieldCost=std::max<uint16_t>(1,fieldHealer->maximumHp/5U);
  const uint16_t fieldHealerBefore=fieldHealer->currentHp;
  assert(BattleEngine::useFieldSoftBoiled(fieldHealCollection,fieldHealer->uid,fieldTargetUid));
  assert(fieldHealer->currentHp==fieldHealerBefore-fieldCost&&fieldHealer->movePp[0]==7);
  assert(fieldTarget->currentHp==std::min<uint16_t>(fieldTarget->maximumHp,
      static_cast<uint16_t>(1U+fieldCost)));
  fieldTarget->currentHp=0;
  const uint16_t sourceBeforeRejectedUse=fieldHealer->currentHp;
  assert(!BattleEngine::useFieldSoftBoiled(fieldHealCollection,fieldHealer->uid,fieldTargetUid));
  assert(fieldHealer->currentHp==sourceBeforeRejectedUse&&fieldTarget->currentHp==0);

  // DREAM EATER has its own effect rule: it only connects against a sleeping
  // target, then restores half the damage actually inflicted.  Failure of the
  // sleep prerequisite is not an accuracy miss and must produce "it failed".
  PokemonCollection dreamCollection; CollectionLogic::initialize(dreamCollection);
  assert(CollectionLogic::chooseStarter(dreamCollection,1));
  OwnedPokemon* dreamer=CollectionLogic::active(dreamCollection,0);
  dreamer->level=30;CollectionLogic::refreshDerivedStats(*dreamer,false);
  dreamer->moves[0]=static_cast<MoveId>(138);dreamer->movePp[0]=15;
  dreamer->currentHp=static_cast<uint16_t>(dreamer->maximumHp/2U);
  const uint16_t dreamHpBefore=dreamer->currentHp;
  BattleState dreamBattle;dreamBattle.active=true;dreamBattle.kind=BattleKind::Trainer;
  dreamBattle.outcome=BattleOutcome::Ongoing;dreamBattle.playerUid=dreamer->uid;dreamBattle.opponentCount=1;
  dreamBattle.opponents[0]=CollectionLogic::createPokemon(0,19,25,false,0x138U);
  dreamBattle.opponents[0].status=StatusCondition::Sleep;
  for(uint8_t slot=0;slot<kMoveSlots;++slot)dreamBattle.opponents[0].movePp[slot]=0;
  const BattleActionResult dreamResult=BattleEngine::fight(dreamBattle,dreamCollection,0);
  dreamer=CollectionLogic::active(dreamCollection,0);
  assert(dreamResult.accepted&&dreamResult.hit&&dreamResult.damageDealt>0);
  assert(dreamer->currentHp==std::min<uint16_t>(dreamer->maximumHp,
      static_cast<uint16_t>(dreamHpBefore+std::max<uint16_t>(1,dreamResult.damageDealt/2U))));
  bool sawDreamHeal=false;
  for(uint8_t eventIndex=0;eventIndex<dreamResult.eventCount;++eventIndex){
    const BattleEvent& event=dreamResult.events[eventIndex];
    if(event.type==BattleEventType::HpChanged&&event.side==BattleSide::Player&&
       event.before==dreamHpBefore&&event.after==dreamer->currentHp)sawDreamHeal=true;
    assert(event.type!=BattleEventType::MoveEffect||
           event.value!=static_cast<uint16_t>(BattleMoveEffect::Failed));
  }
  assert(sawDreamHeal);

  PokemonCollection failedDreamCollection;CollectionLogic::initialize(failedDreamCollection);
  assert(CollectionLogic::chooseStarter(failedDreamCollection,1));
  OwnedPokemon* failedDreamer=CollectionLogic::active(failedDreamCollection,0);
  failedDreamer->moves[0]=static_cast<MoveId>(138);failedDreamer->movePp[0]=15;
  failedDreamer->currentHp=static_cast<uint16_t>(failedDreamer->maximumHp/2U);
  const uint16_t failedDreamHpBefore=failedDreamer->currentHp;
  BattleState failedDreamBattle;failedDreamBattle.active=true;failedDreamBattle.kind=BattleKind::Trainer;
  failedDreamBattle.outcome=BattleOutcome::Ongoing;failedDreamBattle.playerUid=failedDreamer->uid;
  failedDreamBattle.opponentCount=1;
  failedDreamBattle.opponents[0]=CollectionLogic::createPokemon(0,19,5,false,0x139U);
  for(uint8_t slot=0;slot<kMoveSlots;++slot)failedDreamBattle.opponents[0].movePp[slot]=0;
  const uint16_t awakeTargetHp=failedDreamBattle.opponents[0].currentHp;
  const BattleActionResult failedDreamResult=BattleEngine::fight(
      failedDreamBattle,failedDreamCollection,0);
  failedDreamer=CollectionLogic::active(failedDreamCollection,0);
  assert(failedDreamResult.accepted&&!failedDreamResult.hit&&failedDreamResult.damageDealt==0);
  assert(failedDreamer->currentHp==failedDreamHpBefore);
  assert(failedDreamBattle.opponents[0].currentHp==awakeTargetHp);
  bool sawDreamFailure=false,sawDreamMiss=false;
  for(uint8_t eventIndex=0;eventIndex<failedDreamResult.eventCount;++eventIndex){
    const BattleEvent& event=failedDreamResult.events[eventIndex];
    sawDreamFailure|=event.type==BattleEventType::MoveEffect&&
        event.value==static_cast<uint16_t>(BattleMoveEffect::Failed);
    sawDreamMiss|=event.type==BattleEventType::MoveMissed;
  }
  assert(sawDreamFailure&&!sawDreamMiss);

  // NIGHTMARE is a persistent volatile condition, not an ordinary accuracy
  // attack. It only starts on a sleeping target, removes one quarter of max HP
  // at the end of every turn, rejects duplicate use, and ends on waking.
  PokemonCollection nightmareCollection;CollectionLogic::initialize(nightmareCollection);
  assert(CollectionLogic::chooseStarter(nightmareCollection,1));
  OwnedPokemon* nightmareUser=CollectionLogic::active(nightmareCollection,0);
  nightmareUser->level=30;CollectionLogic::refreshDerivedStats(*nightmareUser,false);
  nightmareUser->moves[0]=static_cast<MoveId>(171);nightmareUser->movePp[0]=15;
  nightmareUser->moves[1]=static_cast<MoveId>(150);nightmareUser->movePp[1]=40;
  BattleState nightmareBattle;nightmareBattle.active=true;nightmareBattle.kind=BattleKind::Trainer;
  nightmareBattle.outcome=BattleOutcome::Ongoing;nightmareBattle.playerUid=nightmareUser->uid;
  nightmareBattle.opponentCount=1;
  nightmareBattle.opponents[0]=CollectionLogic::createPokemon(0,19,30,false,0x171U);
  nightmareBattle.opponents[0].status=StatusCondition::Sleep;
  nightmareBattle.opponentVolatiles[0].sleepTurns=4;
  for(uint8_t slot=0;slot<kMoveSlots;++slot)nightmareBattle.opponents[0].movePp[slot]=0;
  const uint16_t nightmareTargetHp=nightmareBattle.opponents[0].currentHp;
  const uint16_t nightmareDamage=std::max<uint16_t>(1U,nightmareBattle.opponents[0].maximumHp/4U);
  const BattleActionResult nightmareResult=BattleEngine::fight(nightmareBattle,nightmareCollection,0);
  assert(nightmareResult.accepted&&nightmareResult.hit&&nightmareBattle.opponentNightmares[0]);
  assert(nightmareBattle.opponents[0].currentHp==nightmareTargetHp-nightmareDamage);
  bool sawNightmareApplied=false,sawNightmareHurt=false,sawNightmareMiss=false;
  for(uint8_t eventIndex=0;eventIndex<nightmareResult.eventCount;++eventIndex){
    const BattleEvent& event=nightmareResult.events[eventIndex];
    sawNightmareApplied|=event.type==BattleEventType::MoveEffect&&
        event.value==static_cast<uint16_t>(BattleMoveEffect::NightmareApplied);
    sawNightmareHurt|=event.type==BattleEventType::MoveEffect&&
        event.value==static_cast<uint16_t>(BattleMoveEffect::NightmareHurt);
    sawNightmareMiss|=event.type==BattleEventType::MoveMissed;
  }
  assert(sawNightmareApplied&&sawNightmareHurt&&!sawNightmareMiss);

  const uint16_t duplicateHpBefore=nightmareBattle.opponents[0].currentHp;
  const BattleActionResult duplicateNightmare=BattleEngine::fight(
      nightmareBattle,nightmareCollection,0);
  bool duplicateFailed=false;
  for(uint8_t eventIndex=0;eventIndex<duplicateNightmare.eventCount;++eventIndex)
    duplicateFailed|=duplicateNightmare.events[eventIndex].type==BattleEventType::MoveEffect&&
        duplicateNightmare.events[eventIndex].value==static_cast<uint16_t>(BattleMoveEffect::Failed);
  assert(duplicateFailed&&nightmareBattle.opponents[0].currentHp==duplicateHpBefore-nightmareDamage);

  nightmareBattle.opponents[0].status=StatusCondition::None;
  const uint16_t awakeNightmareHp=nightmareBattle.opponents[0].currentHp;
  const BattleActionResult afterWake=BattleEngine::fight(nightmareBattle,nightmareCollection,1);
  (void)afterWake;
  assert(!nightmareBattle.opponentNightmares[0]&&
         nightmareBattle.opponents[0].currentHp==awakeNightmareHp);

  BattleState invalidNightmare=nightmareBattle;
  invalidNightmare.active=true;invalidNightmare.outcome=BattleOutcome::Ongoing;
  invalidNightmare.opponents[0].status=StatusCondition::None;
  invalidNightmare.opponents[0].currentHp=invalidNightmare.opponents[0].maximumHp;
  const BattleActionResult invalidNightmareResult=BattleEngine::fight(
      invalidNightmare,nightmareCollection,0);
  bool invalidNightmareFailed=false,invalidNightmareMissed=false;
  for(uint8_t eventIndex=0;eventIndex<invalidNightmareResult.eventCount;++eventIndex){
    const BattleEvent& event=invalidNightmareResult.events[eventIndex];
    invalidNightmareFailed|=event.type==BattleEventType::MoveEffect&&
        event.value==static_cast<uint16_t>(BattleMoveEffect::Failed);
    invalidNightmareMissed|=event.type==BattleEventType::MoveMissed;
  }
  assert(invalidNightmareFailed&&!invalidNightmareMissed&&!invalidNightmare.opponentNightmares[0]);

  PokemonCollection faintStatusCollection; CollectionLogic::initialize(faintStatusCollection);
  assert(CollectionLogic::chooseStarter(faintStatusCollection, 1));
  OwnedPokemon* faintStatusPlayer = CollectionLogic::active(faintStatusCollection, 0);
  faintStatusPlayer->level = 100;
  CollectionLogic::refreshDerivedStats(*faintStatusPlayer, false);
  BattleState faintStatusBattle; faintStatusBattle.active = true;
  faintStatusBattle.kind = BattleKind::Trainer; faintStatusBattle.outcome = BattleOutcome::Ongoing;
  faintStatusBattle.playerUid = faintStatusPlayer->uid; faintStatusBattle.opponentCount = 1;
  faintStatusBattle.opponents[0] = CollectionLogic::createPokemon(0, 19, 20, false, 0xFA17U);
  faintStatusBattle.opponents[0].currentHp = 1;
  faintStatusBattle.opponents[0].status = StatusCondition::Poison;
  faintStatusPlayer->moves[0] = static_cast<MoveId>(129); // SWIFT cannot miss.
  faintStatusPlayer->movePp[0] = 20;
  const BattleActionResult faintStatusResult = BattleEngine::fight(
      faintStatusBattle, faintStatusCollection, 0);
  assert(faintStatusBattle.opponents[0].currentHp == 0);
  assert(faintStatusBattle.opponents[0].status == StatusCondition::None);
  bool sawStatusFaint = false;
  for (uint8_t eventIndex = 0; eventIndex < faintStatusResult.eventCount; ++eventIndex)
    sawStatusFaint |= faintStatusResult.events[eventIndex].type == BattleEventType::Fainted;
  assert(sawStatusFaint);

  Inventory inventory;
  inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] = 1;
  const uint8_t before = CollectionLogic::count(collection);
  const BattleActionResult capture = BattleEngine::throwBall(
      battle, collection, inventory, PokeBallType::MasterBall);
  assert(capture.caught && capture.outcome == BattleOutcome::Captured);
  assert(CollectionLogic::count(collection) == before + 1);

  const OwnedPokemon extra = CollectionLogic::createPokemon(0, 16, 4);
  uint32_t extraUid = 0;
  assert(CollectionLogic::add(collection, extra, &extraUid));
  assert(CollectionLogic::setPartySlot(collection, 1, extraUid));
  assert(CollectionLogic::validate(collection));
  assert(CollectionLogic::removeFromParty(collection,extraUid));
  assert(!CollectionLogic::isInParty(collection,extraUid));
  assert(!CollectionLogic::removeFromParty(collection,starterUid));
  assert(CollectionLogic::setPartySlot(collection,1,extraUid));
  // PC release is explicit and cannot orphan the one remaining virtual pet.
  PokemonCollection releaseCollection;
  CollectionLogic::initialize(releaseCollection);
  assert(CollectionLogic::chooseStarter(releaseCollection, 1));
  assert(!CollectionLogic::release(releaseCollection, releaseCollection.party[0]));
  uint32_t releaseUid = 0;
  assert(CollectionLogic::add(releaseCollection, CollectionLogic::createPokemon(0, 19, 3), &releaseUid));
  assert(CollectionLogic::release(releaseCollection, releaseUid));
  assert(!CollectionLogic::find(releaseCollection, releaseUid));
  assert(CollectionLogic::validate(releaseCollection));

  // A selected team member can enter the active battle and the opposing turn
  // still resolves after switching, matching the touch UI's SWITCH action.
  assert(BattleEngine::startTrainer(battle, charges, collection, starterUid, 67890));
  assert(battle.rewardMoney > 0);
  assert(battle.opponentItemUses == 1);
  assert(charges.available == 2);
  for (uint8_t index = 0; index < battle.opponentCount; ++index) {
    assert(battle.opponents[index].uid != 0);
    if (index) assert(battle.opponents[index - 1U].level <= battle.opponents[index].level);
    for (uint8_t previous = 0; previous < index; ++previous)
      assert(battle.opponents[index].uid != battle.opponents[previous].uid);
  }

  // AI healing scales by the active opponent's level in every non-wild
  // battle. Lv.69 is still the mid-game Super Potion; Lv.70 switches to the
  // Hyper Potion used by late trainers, Gyms, League and Battle Tower teams.
  auto verifyEnemyHealingItem = [](uint8_t opponentLevel, BattleItem expected) {
    PokemonCollection itemCollection;
    CollectionLogic::initialize(itemCollection);
    assert(CollectionLogic::chooseStarter(itemCollection, 1));
    OwnedPokemon* itemPlayer = CollectionLogic::active(itemCollection, 0);
    itemPlayer->moves[0] = static_cast<MoveId>(45);  // Growl: never damages.
    itemPlayer->movePp[0] = findFullMove(static_cast<MoveId>(45))->pp;
    BattleState itemBattle;
    itemBattle.active = true;
    itemBattle.kind = BattleKind::Trainer;
    itemBattle.outcome = BattleOutcome::Ongoing;
    itemBattle.playerUid = itemPlayer->uid;
    itemBattle.opponentCount = 1;
    itemBattle.opponentItemUses = 1;
    itemBattle.opponents[0] = CollectionLogic::createPokemon(
        0, 143, opponentLevel, false, 0x7000U + opponentLevel);
    itemBattle.opponents[0].currentHp = 1;
    const BattleActionResult itemResult = BattleEngine::fight(
        itemBattle, itemCollection, 0);
    assert(itemResult.accepted && itemResult.enemyItemUsed);
    assert(itemResult.enemyItem == expected);
    assert(itemBattle.opponentItemUses == 0);
  };
  verifyEnemyHealingItem(29, BattleItem::Potion);
  verifyEnemyHealingItem(30, BattleItem::SuperPotion);
  verifyEnemyHealingItem(69, BattleItem::SuperPotion);
  verifyEnemyHealingItem(70, BattleItem::HyperPotion);
  verifyEnemyHealingItem(100, BattleItem::HyperPotion);
  const uint32_t firstBattlerUid = battle.playerUid;
  const BattleActionResult switched = BattleEngine::switchPokemon(battle, collection);
  assert(switched.accepted);
  assert(battle.playerUid == extraUid);
  assert(battle.playerUid != firstBattlerUid);

  // A complete fight awards XP and leaves a terminal battle result that can
  // be persisted before the UI returns home.
  battle.opponentCount = 1;
  battle.opponentItemUses = 0;
  BattleEngine::currentOpponent(battle)->currentHp = 1;
  const uint32_t defeatedOpponentUid = BattleEngine::currentOpponent(battle)->uid;
  const uint32_t xpBefore = CollectionLogic::find(collection, extraUid)->experience;
  const uint32_t starterXpBefore = CollectionLogic::find(collection, starterUid)->experience;
  const uint16_t evBefore = CollectionLogic::totalEffortValues(*CollectionLogic::find(collection, extraUid));
  const BattleActionResult victory = BattleEngine::fight(battle, collection, 0);
  assert(victory.accepted && victory.outcome == BattleOutcome::Victory);
  int moveEvent=-1,hpEvent=-1,faintEvent=-1,xpEvent=-1,endEvent=-1;
  for(uint8_t index=0;index<victory.eventCount;++index){
    switch(victory.events[index].type){
      case BattleEventType::MoveUsed:if(moveEvent<0)moveEvent=index;break;
      case BattleEventType::HpChanged:if(hpEvent<0)hpEvent=index;break;
      case BattleEventType::Fainted:if(faintEvent<0)faintEvent=index;break;
      case BattleEventType::ExperienceGained:if(xpEvent<0)xpEvent=index;break;
      case BattleEventType::BattleEnded:if(endEvent<0)endEvent=index;break;
      default:break;
    }
  }
  // Presentation depends on this FireRed sequence: action/animation, HP,
  // faint, reward, then terminal result. Never expose the next state early.
  assert(moveEvent>=0&&hpEvent>moveEvent&&faintEvent>hpEvent&&xpEvent>faintEvent&&endEvent>xpEvent);
  assert(victory.events[faintEvent].pokemonUid == defeatedOpponentUid);
  assert(victory.events[faintEvent].pokemonUid != 0);
  assert(victory.experienceGained > 0);
  assert(CollectionLogic::find(collection, extraUid)->experience > xpBefore);
  assert(CollectionLogic::find(collection, extraUid)->experience - xpBefore ==
         CollectionLogic::find(collection, starterUid)->experience - starterXpBefore);
  assert(CollectionLogic::totalEffortValues(*CollectionLogic::find(collection, extraUid)) > evBefore);

  // A lethal HYPER BEAM must finish the victim before recharge can resolve.
  // Against a trainer's next Pokemon the user still recharges (FireRed), but
  // the defeated zero-HP battler can neither remain active nor take a turn.
  PokemonCollection beamCollection;CollectionLogic::initialize(beamCollection);
  assert(CollectionLogic::chooseStarter(beamCollection,1));
  OwnedPokemon* beamUser=CollectionLogic::active(beamCollection,0);
  beamUser->level=50;CollectionLogic::refreshDerivedStats(*beamUser,false);
  beamUser->moves[0]=static_cast<MoveId>(63);beamUser->movePp[0]=5;
  beamUser->moves[1]=static_cast<MoveId>(150);beamUser->movePp[1]=40;
  BattleState beamBattle;beamBattle.active=true;beamBattle.kind=BattleKind::Trainer;
  beamBattle.outcome=BattleOutcome::Ongoing;beamBattle.playerUid=beamUser->uid;
  beamBattle.opponentCount=2;beamBattle.rngState=0x63BEA001U;
  beamBattle.opponents[0]=CollectionLogic::createPokemon(0,19,5,false,0x6301U);
  beamBattle.opponents[1]=CollectionLogic::createPokemon(0,16,5,false,0x6302U);
  assert(BattleEngine::ensureOpponentUids(beamBattle));
  beamBattle.opponents[0].currentHp=1;
  beamBattle.opponents[1].moves[0]=MoveId::Tackle;
  beamBattle.opponents[1].movePp[0]=findFullMove(MoveId::Tackle)->pp;
  const uint32_t beamVictimUid=beamBattle.opponents[0].uid;
  const uint32_t beamNextUid=beamBattle.opponents[1].uid;
  const BattleActionResult beamLethal=BattleEngine::fight(beamBattle,beamCollection,0);
  assert(beamLethal.accepted&&beamLethal.hit&&beamBattle.opponents[0].currentHp==0);
  assert(beamBattle.active&&beamBattle.opponentIndex==1&&beamBattle.playerVolatile.recharging);
  bool beamVictimFainted=false,beamNextSwitched=false,beamNextActedEarly=false;
  for(uint8_t eventIndex=0;eventIndex<beamLethal.eventCount;++eventIndex){
    const BattleEvent& event=beamLethal.events[eventIndex];
    beamVictimFainted|=event.type==BattleEventType::Fainted&&event.pokemonUid==beamVictimUid;
    beamNextSwitched|=event.type==BattleEventType::SwitchedIn&&event.pokemonUid==beamNextUid;
    beamNextActedEarly|=event.type==BattleEventType::MoveUsed&&event.pokemonUid==beamNextUid;
  }
  assert(beamVictimFainted&&beamNextSwitched&&!beamNextActedEarly);

  const BattleActionResult beamRecharge=BattleEngine::fight(beamBattle,beamCollection,0);
  bool beamUserRecharged=false,beamNextActed=false,beamVictimActed=false;
  for(uint8_t eventIndex=0;eventIndex<beamRecharge.eventCount;++eventIndex){
    const BattleEvent& event=beamRecharge.events[eventIndex];
    beamUserRecharged|=event.type==BattleEventType::CannotMove&&
        event.side==BattleSide::Player&&event.value==1;
    beamNextActed|=event.type==BattleEventType::MoveUsed&&event.pokemonUid==beamNextUid;
    beamVictimActed|=event.type==BattleEventType::MoveUsed&&event.pokemonUid==beamVictimUid;
  }
  assert(beamRecharge.accepted&&beamUserRecharged&&beamNextActed&&!beamVictimActed&&
         !beamBattle.playerVolatile.recharging);

  const uint16_t turnBeforeInvalidFaintAction=beamBattle.turn;
  beamBattle.opponentIndex=0;
  const BattleActionResult invalidFaintAction=BattleEngine::fight(beamBattle,beamCollection,0);
  assert(!invalidFaintAction.accepted&&invalidFaintAction.eventCount==0&&
         beamBattle.turn==turnBeforeInvalidFaintAction);

  beamBattle.opponentIndex=1;
  beamBattle.opponentVolatiles[1].recharging=true;
  const BattleActionResult enemyRecharge=BattleEngine::fight(beamBattle,beamCollection,1);
  bool enemyRechargeMessage=false;
  for(uint8_t eventIndex=0;eventIndex<enemyRecharge.eventCount;++eventIndex)
    enemyRechargeMessage|=enemyRecharge.events[eventIndex].type==BattleEventType::CannotMove&&
        enemyRecharge.events[eventIndex].side==BattleSide::Opponent&&
        enemyRecharge.events[eventIndex].pokemonUid==beamNextUid&&
        enemyRecharge.events[eventIndex].value==1;
  assert(enemyRecharge.accepted&&enemyRechargeMessage&&
         !beamBattle.opponentVolatiles[1].recharging);

  // Saves produced before opponent presentation IDs existed are repaired in
  // place without changing team order or generating duplicate identities.
  BattleState legacyOpponentIds{};
  legacyOpponentIds.rngState = 0x12345678U;
  legacyOpponentIds.opponentCount = 3;
  assert(BattleEngine::ensureOpponentUids(legacyOpponentIds));
  assert(!BattleEngine::ensureOpponentUids(legacyOpponentIds));
  for (uint8_t index = 0; index < legacyOpponentIds.opponentCount; ++index) {
    assert(legacyOpponentIds.opponents[index].uid != 0);
    for (uint8_t previous = 0; previous < index; ++previous)
      assert(legacyOpponentIds.opponents[index].uid != legacyOpponentIds.opponents[previous].uid);
  }

  // Mart inventory has four guaranteed families plus sixteen rotating
  // products. Selection is random, but presentation is grouped and spans
  // exactly four five-row pages.
  MartState mart;
  uint64_t ownedMachines=0;
  Economy::rotate(mart,0,ownedMachines);
  assert(kMartOfferCount==20U);
  uint8_t pokeBallOffer=0xFF,potionOffer=0xFF,statusOffer=0xFF,machineOffer=0xFF;
  for(uint8_t i=0;i<kMartOfferCount;++i){
    const MartOffer& offer=mart.offers[i];
    if(!offer.isHeldItem()&&!offer.isMachine()&&offer.item==MartItem::PokeBall)pokeBallOffer=i;
    if(!offer.isHeldItem()&&!offer.isMachine()&&offer.item==MartItem::Potion)potionOffer=i;
    if(!offer.isHeldItem()&&!offer.isMachine()&&(offer.item==MartItem::FullHeal||
       (offer.item>=MartItem::Antidote&&offer.item<=MartItem::IceHeal)))statusOffer=i;
    if(offer.isMachine())machineOffer=i;
    if(i)assert(static_cast<uint8_t>(Economy::category(mart.offers[i-1U]))<=
                static_cast<uint8_t>(Economy::category(offer)));
    for(uint8_t previous=0;previous<i;++previous)
      assert(mart.offers[previous].item!=offer.item||
             mart.offers[previous].heldItem!=offer.heldItem);
  }
  assert(pokeBallOffer!=0xFF&&potionOffer!=0xFF&&statusOffer!=0xFF&&machineOffer!=0xFF);
  assert(!Economy::ownsMachine(ownedMachines,mart.offers[machineOffer].machineId()));
  const uint8_t offeredMachine=mart.offers[machineOffer].machineId();
  uint32_t machineMoney=10000;
  assert(Economy::buy(mart,machineOffer,machineMoney,inventory,ownedMachines));
  assert(Economy::ownsMachine(ownedMachines,offeredMachine));
  assert(!Economy::buy(mart,machineOffer,machineMoney,inventory,ownedMachines));
  uint32_t money = 1000;
  const uint16_t potionsBefore = inventory.medicine[static_cast<uint8_t>(BattleItem::Potion)];
  assert(Economy::buy(mart,potionOffer,money,inventory,ownedMachines));
  assert(money == 700 && inventory.medicine[static_cast<uint8_t>(BattleItem::Potion)] == potionsBefore + 1);

  // An Ultra Ball candidate has exactly a 1/100,000 promotion chance. This
  // deterministic RNG state reaches that branch in the first random slot,
  // proving the rare offer's stock, real 99,999 price and Master Ball pocket.
  assert(Economy::kMasterBallReplacementDenominator==100000U);
  MartState masterMart{};masterMart.rng=0x0005A97EU;
  Economy::rotate(masterMart,0,0);
  uint8_t masterOffer=0xFF;
  for(uint8_t i=0;i<kMartOfferCount;++i)
    if(masterMart.offers[i].item==MartItem::MasterBall)masterOffer=i;
  assert(masterOffer!=0xFF&&masterMart.offers[masterOffer].remaining==1U);
  assert(Economy::price(masterMart.offers[masterOffer])==99999U);
  Inventory masterInventory{};uint64_t masterMachines=0;uint32_t masterMoney=99998U;
  assert(Economy::maximumPurchasable(masterMart,masterOffer,masterMoney,
                                     masterInventory,masterMachines)==0U);
  ++masterMoney;
  assert(Economy::maximumPurchasable(masterMart,masterOffer,masterMoney,
                                     masterInventory,masterMachines)==1U);
  assert(Economy::buy(masterMart,masterOffer,masterMoney,masterInventory,masterMachines));
  assert(masterMoney==0U&&masterMart.offers[masterOffer].remaining==0U&&
         masterInventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)]==1U);

  // Multi-buy is atomic and bounded by all three constraints: offer stock,
  // available money and the FireRed-style 999-item stack ceiling.
  MartState bulkMart{}; bulkMart.offers[0]={MartItem::Potion,HeldItem::None,300,8};
  Inventory bulkInventory{}; uint64_t bulkMachines=0; uint32_t bulkMoney=1000;
  assert(Economy::maximumPurchasable(bulkMart,0,bulkMoney,bulkInventory,bulkMachines)==3);
  assert(Economy::buyQuantity(bulkMart,0,3,bulkMoney,bulkInventory,bulkMachines));
  assert(bulkMoney==100&&bulkMart.offers[0].remaining==5&&bulkInventory.medicine[0]==3);
  assert(!Economy::buyQuantity(bulkMart,0,1,bulkMoney,bulkInventory,bulkMachines));
  bulkInventory.medicine[0]=998;bulkMoney=10000;bulkMart.offers[0].remaining=8;
  assert(Economy::maximumPurchasable(bulkMart,0,bulkMoney,bulkInventory,bulkMachines)==1);
  assert(Economy::buyQuantity(bulkMart,0,1,bulkMoney,bulkInventory,bulkMachines));
  assert(bulkInventory.medicine[0]==kInventoryStackLimit);
  assert(Economy::maximumPurchasable(bulkMart,0,bulkMoney,bulkInventory,bulkMachines)==0);

  // PP restoratives use an appended inventory block, while PP Up follows the
  // Gen-III 20%-of-base formula and caps at three applications per move.
  PpItemInventory ppInventory{};
  MartState ppMart{};ppMart.offers[0]={MartItem::Ether,HeldItem::None,1200,4};
  uint32_t ppMoney=5000;
  assert(Economy::maximumPurchasable(ppMart,0,ppMoney,bulkInventory,bulkMachines,&ppInventory)==4);
  assert(Economy::buyQuantity(ppMart,0,2,ppMoney,bulkInventory,bulkMachines,&ppInventory));
  assert(ppInventory.quantities[ppInventoryIndex(BattleItem::Ether)]==2&&ppMoney==2600);
  OwnedPokemon ppPokemon=CollectionLogic::createPokemon(9000,1,20,false,9000);
  ppPokemon.moves[0]=MoveId::Tackle;CollectionLogic::clearMovePpUps(ppPokemon,0);
  ppPokemon.movePp[0]=findFullMove(MoveId::Tackle)->pp;
  const uint8_t basePp=ppPokemon.movePp[0];
  assert(CollectionLogic::applyPpUp(ppPokemon,0));
  assert(CollectionLogic::applyPpUp(ppPokemon,0));
  assert(CollectionLogic::applyPpUp(ppPokemon,0));
  assert(!CollectionLogic::applyPpUp(ppPokemon,0));
  assert(CollectionLogic::ppUpCount(ppPokemon,0)==3&&
         CollectionLogic::maximumMovePp(ppPokemon,0)==basePp+basePp*3U/5U&&
         ppPokemon.movePp[0]==CollectionLogic::maximumMovePp(ppPokemon,0));

  uint16_t ppRecoveryOffers=0,ppUpOffers=0;
  MartState frequencyMart{};frequencyMart.rng=0x50505245U;
  for(uint16_t rotation=0;rotation<300;++rotation){
    Economy::rotate(frequencyMart,0,(uint64_t{1}<<kMachineCount)-1U);
    for(uint8_t offer=0;offer<kMartOfferCount;++offer){
      const MartItem item=frequencyMart.offers[offer].item;
      if(item>=MartItem::Ether&&item<=MartItem::MaxElixir)++ppRecoveryOffers;
      else if(item==MartItem::PpUp)++ppUpOffers;
    }
  }
  assert(ppRecoveryOffers>ppUpOffers&&ppUpOffers>0);
  // In addition to the one guaranteed machine, the expanded random pool has
  // a modest chance for extra unique machines and a slightly larger held-item
  // share. Keep this deterministic sample as a regression guard for both.
  uint16_t heldRotationOffers=0,extraMachineOffers=0;
  MartState expandedFrequencyMart{};expandedFrequencyMart.rng=0x45585044U;
  for(uint16_t rotation=0;rotation<300;++rotation){
    Economy::rotate(expandedFrequencyMart,8,0);
    uint8_t machinesThisRotation=0;
    for(const MartOffer& offer:expandedFrequencyMart.offers){
      if(offer.isHeldItem())++heldRotationOffers;
      if(offer.isMachine())++machinesThisRotation;
    }
    if(machinesThisRotation>1U)extraMachineOffers+=machinesThisRotation-1U;
  }
  assert(heldRotationOffers>300U&&extraMachineOffers>0U);
  MartState testCadence{};testCadence.elapsedSeconds=0;testCadence.day=0;
  assert(Economy::kRotationSeconds == 6U * 60U * 60U);
  Economy::advance(testCadence,Economy::kRotationSeconds-1U,0,(uint64_t{1}<<kMachineCount)-1U);
  assert(testCadence.day==0);
  Economy::advance(testCadence,1U,0,(uint64_t{1}<<kMachineCount)-1U);
  assert(testCadence.day==1);
  const uint64_t everyMachine=(uint64_t{1}<<kMachineCount)-1U;
  Economy::rotate(mart,0,everyMachine);
  for(const MartOffer& offer:mart.offers)assert(!offer.isMachine());

  // Every held item is catalogued, Mart-locked by Badge count and can be
  // swapped or taken back without duplicating it.
  for(uint8_t raw=1;raw<static_cast<uint8_t>(HeldItem::Count);++raw){
    const HeldItemData* data=heldItemData(static_cast<HeldItem>(raw));assert(data&&data->name[0]);
    if(raw<kHeldItemInventorySlots)assert(data->price&&data->martWeight);
    else assert(data->unlockBadges==255U&&!data->martWeight);
  }
  assert(MegaEvolution::officialFormCount()==43);
  assert(heldItemIsTransferLocked(HeldItem::MegaStone));
  OwnedPokemon megaCharizard=CollectionLogic::createPokemon(7000,6,50,false,1);
  megaCharizard.personality=0;megaCharizard.heldItem=HeldItem::MegaStone;
  CollectionLogic::refreshAbility(megaCharizard);CollectionLogic::refreshDerivedStats(megaCharizard,false);
  assert(MegaEvolution::variantFor(megaCharizard)==MegaVariant::MegaX);
  assert(MegaEvolution::type2(megaCharizard,PokemonType::Flying)==PokemonType::Dragon);
  assert(findAbility(megaCharizard.abilityId)&&std::strcmp(findAbility(megaCharizard.abilityId)->name,"TOUGH CLAWS")==0);
  megaCharizard.personality=100;CollectionLogic::refreshAbility(megaCharizard);
  assert(MegaEvolution::variantFor(megaCharizard)==MegaVariant::MegaY);
  assert(findAbility(megaCharizard.abilityId)&&std::strcmp(findAbility(megaCharizard.abilityId)->name,"DROUGHT")==0);
  bool foundHeldOffer=false;
  for(uint16_t day=0;day<100;++day){Economy::rotate(mart,0,ownedMachines);for(uint8_t i=0;i<kMartOfferCount;++i)if(mart.offers[i].isHeldItem()){foundHeldOffer=true;assert(heldItemData(mart.offers[i].heldItem)->unlockBadges==0);}}
  assert(foundHeldOffer);
  MartState heldMart;heldMart.offers[0]={MartItem::PokeBall,HeldItem::Leftovers,4000,1};
  uint32_t heldMoney=5000;assert(Economy::buy(heldMart,0,heldMoney,inventory,ownedMachines));
  assert(heldMoney==1000&&inventory.heldItems[static_cast<uint8_t>(HeldItem::Leftovers)]==1);
  starter->heldItem=HeldItem::None;
  assert(equipHeldItem(starter->heldItem,HeldItem::Leftovers,inventory.heldItems,ownedMachines));
  assert(starter->heldItem==HeldItem::Leftovers&&inventory.heldItems[static_cast<uint8_t>(HeldItem::Leftovers)]==0);
  inventory.heldItems[static_cast<uint8_t>(HeldItem::OranBerry)]=1;
  assert(equipHeldItem(starter->heldItem,HeldItem::OranBerry,inventory.heldItems,ownedMachines));
  assert(starter->heldItem==HeldItem::OranBerry&&inventory.heldItems[static_cast<uint8_t>(HeldItem::Leftovers)]==1);
  assert(equipHeldItem(starter->heldItem,HeldItem::None,inventory.heldItems,ownedMachines));
  assert(starter->heldItem==HeldItem::None&&inventory.heldItems[static_cast<uint8_t>(HeldItem::OranBerry)]==1);
  starter->heldItem=HeldItem::Leftovers;inventory.heldItems[static_cast<uint8_t>(HeldItem::Leftovers)]=0;
  uint64_t ownedMachineItems=0;
  assert(unequipHeldItem(starter->heldItem,inventory.heldItems,ownedMachineItems));
  assert(starter->heldItem==HeldItem::None&&inventory.heldItems[static_cast<uint8_t>(HeldItem::Leftovers)]==1);
  starter->heldItem=HeldItem::MegaStone;
  assert(unequipHeldItem(starter->heldItem,inventory.heldItems,ownedMachineItems));
  assert(starter->heldItem==HeldItem::None&&(ownedMachineItems&kMegaStoneOwnershipBit));
  assert(heldItemIsTransferLocked(HeldItem::LuckyEgg));
  ownedMachineItems|=kLuckyEggOwnershipBit;
  assert(equipHeldItem(starter->heldItem,HeldItem::LuckyEgg,
                       inventory.heldItems,ownedMachineItems));
  assert(starter->heldItem==HeldItem::LuckyEgg&&
         !(ownedMachineItems&kLuckyEggOwnershipBit));
  assert(unequipHeldItem(starter->heldItem,inventory.heldItems,ownedMachineItems));
  assert(starter->heldItem==HeldItem::None&&
         (ownedMachineItems&kLuckyEggOwnershipBit));

  // Medicine and X-items are usable in any active battle, consume a turn and
  // mutate only temporary combat stages where appropriate.
  BattleEngine::clear(battle);
  battle.active = true; battle.outcome = BattleOutcome::Ongoing; battle.kind = BattleKind::Trainer;
  battle.playerUid = starterUid; battle.opponentCount = 1;
  battle.opponents[0] = CollectionLogic::createPokemon(0, 10, 2);
  battle.opponents[0].moves[0] = MoveId::None;
  OwnedPokemon* itemTarget = CollectionLogic::find(collection, starterUid);
  itemTarget->currentHp = static_cast<uint16_t>(itemTarget->maximumHp - 1);
  inventory.medicine[static_cast<uint8_t>(BattleItem::Potion)] = 1;
  BattleActionResult itemResult{};
  BattleEngine::useItemInto(battle, collection, inventory, BattleItem::Potion, itemResult);
  assert(itemResult.accepted && itemResult.eventCount >= 2);
  assert(itemTarget->currentHp == itemTarget->maximumHp);
  assert(inventory.medicine[static_cast<uint8_t>(BattleItem::Potion)] == 0);
  inventory.medicine[static_cast<uint8_t>(BattleItem::XAttack)] = 1;
  BattleEngine::useItemInto(battle, collection, inventory, BattleItem::XAttack, itemResult);
  assert(itemResult.accepted && itemResult.eventCount >= 1);
  battle.playerVolatile.attackStage=kMaximumBattleStatStage;
  inventory.medicine[static_cast<uint8_t>(BattleItem::XAttack)]=1;
  const uint16_t turnBeforeCappedXItem=battle.turn;
  BattleEngine::useItemInto(battle,collection,inventory,BattleItem::XAttack,itemResult);
  assert(!itemResult.accepted&&battle.turn==turnBeforeCappedXItem&&
         battle.playerVolatile.attackStage==kMaximumBattleStatStage&&
         inventory.medicine[static_cast<uint8_t>(BattleItem::XAttack)]==1U);
  // Restore the stage produced by the successful X Attack above; later
  // switch/reset assertions deliberately inspect that original setup.
  battle.playerVolatile.attackStage=1;
  itemTarget->movePp[0]=0;
  const uint8_t targetMaximumPp=CollectionLogic::maximumMovePp(*itemTarget,0);
  BattleEngine::useItemInto(battle,collection,inventory,BattleItem::Ether,itemResult,
                            itemTarget->uid,&ppInventory,0);
  assert(itemResult.accepted&&itemTarget->movePp[0]==std::min<uint8_t>(10U,targetMaximumPp)&&
         ppInventory.quantities[ppInventoryIndex(BattleItem::Ether)]==1);

  // Healing a reserve member keeps that member's UID on the HP journal
  // event. The renderer uses this identity to avoid animating the active
  // battler's meter for a Potion or Revive used from the party screen.
  OwnedPokemon reserve=CollectionLogic::createPokemon(0,4,5,false,0x1A2BU);
  uint32_t reserveUid=0;assert(CollectionLogic::add(collection,reserve,&reserveUid));
  collection.party[1]=reserveUid;
  OwnedPokemon* reserveTarget=CollectionLogic::find(collection,reserveUid);
  reserveTarget->currentHp=1;
  const uint16_t activeHpBeforeReserveItem=itemTarget->currentHp;
  inventory.medicine[static_cast<uint8_t>(BattleItem::Potion)]=1;
  BattleEngine::useItemInto(battle,collection,inventory,BattleItem::Potion,itemResult,reserveUid);
  bool reserveHpEvent=false;
  for(uint8_t event=0;event<itemResult.eventCount;++event)
    if(itemResult.events[event].type==BattleEventType::HpChanged){
      assert(itemResult.events[event].pokemonUid!=starterUid);
      if(itemResult.events[event].pokemonUid==reserveUid)reserveHpEvent=true;
    }
  assert(itemResult.accepted&&reserveHpEvent&&itemTarget->currentHp==activeHpBeforeReserveItem&&
         reserveTarget->currentHp>1);

  // Status moves resolve their own accuracy and are successful actions even
  // though their damage is zero. String Shot is a reliable Gen III example:
  // with this seed its 95% accuracy connects and lowers the foe's Speed.
  PokemonCollection statusCollection; CollectionLogic::initialize(statusCollection);
  assert(CollectionLogic::chooseStarter(statusCollection, 1, 77));
  OwnedPokemon* statusUser = CollectionLogic::active(statusCollection, 0);
  statusUser->moves[0] = MoveId::StringShot;
  statusUser->movePp[0] = findFullMove(MoveId::StringShot)->pp;
  BattleState statusBattle; BattleEngine::clear(statusBattle);
  statusBattle.active = true; statusBattle.kind = BattleKind::Trainer;
  statusBattle.outcome = BattleOutcome::Ongoing; statusBattle.playerUid = statusUser->uid;
  statusBattle.opponentCount = 1; statusBattle.rngState = 1;
  statusBattle.opponents[0] = CollectionLogic::createPokemon(0, 10, 5, false, 78);
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) statusBattle.opponents[0].moves[slot] = MoveId::None;
  const BattleActionResult statusMove = BattleEngine::fight(statusBattle, statusCollection, 0);
  assert(statusMove.accepted && statusMove.hit && statusMove.damageDealt == 0);
  assert(statusBattle.opponentVolatiles[0].speedStage < 0);
  assert(statusMove.eventCount >= 2);
  assert(statusMove.events[0].type == BattleEventType::MoveUsed);
  bool reportedSpeedDrop = false;
  for (uint8_t event = 0; event < statusMove.eventCount; ++event)
    if (statusMove.events[event].type == BattleEventType::StatChanged &&
        statusMove.events[event].side == BattleSide::Opponent && statusMove.events[event].value == 4U)
      reportedSpeedDrop = true;
  assert(reportedSpeedDrop);

  // FOCUS ENERGY must report a successful self effect, contribute the two
  // Gen-III critical stages, and fail only when that volatile is already set.
  // Exercise both sides because player and trainer actions share this handler.
  const MoveId focusEnergyMove = static_cast<MoveId>(116);
  PokemonCollection focusCollection; CollectionLogic::initialize(focusCollection);
  assert(CollectionLogic::chooseStarter(focusCollection, 1, 779));
  OwnedPokemon* focusUser = CollectionLogic::active(focusCollection, 0);
  focusUser->moves[0] = focusEnergyMove;
  focusUser->movePp[0] = findFullMove(focusEnergyMove)->pp;
  BattleState focusBattle; BattleEngine::clear(focusBattle);
  focusBattle.active=true;focusBattle.kind=BattleKind::Trainer;
  focusBattle.outcome=BattleOutcome::Ongoing;focusBattle.playerUid=focusUser->uid;
  focusBattle.opponentCount=1;focusBattle.rngState=0xF0C05EEDU;
  focusBattle.opponents[0]=CollectionLogic::createPokemon(0,10,5,false,780);
  for(uint8_t slot=0;slot<kMoveSlots;++slot){
    focusBattle.opponents[0].moves[slot]=MoveId::None;
    focusBattle.opponents[0].movePp[slot]=0;
  }
  const BattleActionResult focusSet=BattleEngine::fight(focusBattle,focusCollection,0);
  bool sawFocusSet=false,sawFocusFailure=false;
  for(uint8_t event=0;event<focusSet.eventCount;++event){
    sawFocusSet|=focusSet.events[event].type==BattleEventType::MoveEffect&&
        focusSet.events[event].side==BattleSide::Player&&
        focusSet.events[event].value==static_cast<uint16_t>(BattleMoveEffect::FocusEnergySet);
    sawFocusFailure|=focusSet.events[event].type==BattleEventType::MoveEffect&&
        focusSet.events[event].value==static_cast<uint16_t>(BattleMoveEffect::Failed);
  }
  assert(focusSet.accepted&&focusSet.hit&&focusBattle.playerVolatile.criticalStage==2U&&
         sawFocusSet&&!sawFocusFailure);
  const BattleActionResult focusAgain=BattleEngine::fight(focusBattle,focusCollection,0);
  sawFocusFailure=false;
  for(uint8_t event=0;event<focusAgain.eventCount;++event)
    sawFocusFailure|=focusAgain.events[event].type==BattleEventType::MoveEffect&&
        focusAgain.events[event].side==BattleSide::Player&&
        focusAgain.events[event].value==static_cast<uint16_t>(BattleMoveEffect::Failed);
  assert(focusAgain.accepted&&focusBattle.playerVolatile.criticalStage==2U&&sawFocusFailure);

  focusUser->moves[0]=static_cast<MoveId>(150); // SPLASH keeps the local turn harmless.
  focusUser->movePp[0]=findFullMove(focusUser->moves[0])->pp;
  focusBattle.opponents[0].moves[0]=focusEnergyMove;
  focusBattle.opponents[0].movePp[0]=findFullMove(focusEnergyMove)->pp;
  focusBattle.opponentVolatiles[0].criticalStage=0;
  const BattleActionResult enemyFocus=BattleEngine::fight(focusBattle,focusCollection,0);
  bool sawEnemyFocus=false;
  for(uint8_t event=0;event<enemyFocus.eventCount;++event)
    sawEnemyFocus|=enemyFocus.events[event].type==BattleEventType::MoveEffect&&
        enemyFocus.events[event].side==BattleSide::Opponent&&
        enemyFocus.events[event].value==static_cast<uint16_t>(BattleMoveEffect::FocusEnergySet);
  assert(enemyFocus.accepted&&focusBattle.opponentVolatiles[0].criticalStage==2U&&sawEnemyFocus);

  // SAFEGUARD is a five-turn side condition. It protects the whole Party,
  // not only its user: the protection survives a switch and blocks both major
  // status and confusion caused by the opposing side.
  const MoveId safeguardMove = static_cast<MoveId>(219);
  const MoveId toxicMove = static_cast<MoveId>(92);
  const MoveId confuseRayMove = static_cast<MoveId>(109);
  const MoveId growlMove = static_cast<MoveId>(45);
  PokemonCollection safeguardCollection; CollectionLogic::initialize(safeguardCollection);
  assert(CollectionLogic::chooseStarter(safeguardCollection, 1, 880));
  OwnedPokemon* safeguardUser = CollectionLogic::active(safeguardCollection, 0);
  safeguardUser->level = 50; CollectionLogic::refreshDerivedStats(*safeguardUser, false);
  safeguardUser->moves[0] = safeguardMove;
  safeguardUser->movePp[0] = findFullMove(safeguardMove)->pp;
  uint32_t safeguardPartnerUid = 0;
  assert(CollectionLogic::add(safeguardCollection,
      CollectionLogic::createPokemon(0, 7, 40, false, 881), &safeguardPartnerUid));
  assert(CollectionLogic::setPartySlot(safeguardCollection, 1, safeguardPartnerUid));
  OwnedPokemon* safeguardPartner = CollectionLogic::find(safeguardCollection, safeguardPartnerUid);
  safeguardPartner->moves[0] = growlMove;
  safeguardPartner->movePp[0] = findFullMove(growlMove)->pp;
  BattleState safeguardBattle; BattleEngine::clear(safeguardBattle);
  safeguardBattle.active = true; safeguardBattle.kind = BattleKind::Trainer;
  safeguardBattle.outcome = BattleOutcome::Ongoing; safeguardBattle.playerUid = safeguardUser->uid;
  safeguardBattle.opponentCount = 1; safeguardBattle.rngState = 0x53414645U;
  safeguardBattle.opponents[0] = CollectionLogic::createPokemon(0, 19, 5, false, 882);
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    safeguardBattle.opponents[0].moves[slot] = MoveId::None;
    safeguardBattle.opponents[0].movePp[slot] = 0;
  }
  safeguardBattle.opponents[0].moves[0] = toxicMove;
  safeguardBattle.opponents[0].movePp[0] = findFullMove(toxicMove)->pp;
  safeguardBattle.opponentVolatiles[0].accuracyStage = 6;
  const BattleActionResult safeguardSet = BattleEngine::fight(
      safeguardBattle, safeguardCollection, 0);
  bool sawSafeguardSet = false;
  for (uint8_t event = 0; event < safeguardSet.eventCount; ++event)
    sawSafeguardSet |= safeguardSet.events[event].type == BattleEventType::MoveEffect &&
        safeguardSet.events[event].side == BattleSide::Player &&
        safeguardSet.events[event].value == static_cast<uint16_t>(BattleMoveEffect::SafeguardSet);
  assert(safeguardSet.accepted && sawSafeguardSet && safeguardBattle.playerSafeguardTurns == 4);
  assert(safeguardUser->status == StatusCondition::None);

  // A voluntary switch consumes one turn, but the new active member remains
  // protected because the counter belongs to the player's side.
  const BattleActionResult safeguardSwitch = BattleEngine::switchToPokemon(
      safeguardBattle, safeguardCollection, safeguardPartnerUid);
  assert(safeguardSwitch.accepted && safeguardBattle.playerUid == safeguardPartnerUid &&
         safeguardBattle.playerSafeguardTurns == 3 &&
         safeguardPartner->status == StatusCondition::None);
  safeguardBattle.opponents[0].moves[0] = confuseRayMove;
  safeguardBattle.opponents[0].movePp[0] = findFullMove(confuseRayMove)->pp;
  const BattleActionResult safeguardConfusion = BattleEngine::fight(
      safeguardBattle, safeguardCollection, 0);
  assert(safeguardConfusion.accepted && safeguardBattle.playerSafeguardTurns == 2 &&
         safeguardBattle.playerVolatile.confusionTurns == 0);
  safeguardBattle.opponents[0].moves[0] = toxicMove;
  safeguardBattle.opponents[0].movePp[0] = findFullMove(toxicMove)->pp;
  const BattleActionResult safeguardFourthTurn = BattleEngine::fight(
      safeguardBattle, safeguardCollection, 0);
  assert(safeguardFourthTurn.accepted && safeguardBattle.playerSafeguardTurns == 1 &&
         safeguardPartner->status == StatusCondition::None);
  const BattleActionResult safeguardFifthTurn = BattleEngine::fight(
      safeguardBattle, safeguardCollection, 0);
  bool sawSafeguardEnd = false;
  for (uint8_t event = 0; event < safeguardFifthTurn.eventCount; ++event)
    sawSafeguardEnd |= safeguardFifthTurn.events[event].type == BattleEventType::MoveEffect &&
        safeguardFifthTurn.events[event].side == BattleSide::Player &&
        safeguardFifthTurn.events[event].value == static_cast<uint16_t>(BattleMoveEffect::SafeguardEnded);
  assert(safeguardFifthTurn.accepted && sawSafeguardEnd &&
         safeguardBattle.playerSafeguardTurns == 0 &&
         safeguardPartner->status == StatusCondition::None);
  const BattleActionResult afterSafeguard = BattleEngine::fight(
      safeguardBattle, safeguardCollection, 0);
  assert(afterSafeguard.accepted && safeguardPartner->status == StatusCondition::BadlyPoisoned);

  // Safeguard's scope is exact to FireRed, rather than a broad "negative
  // effect" filter. OBLIVIOUS prevents attraction, not confusion, and ATTRACT
  // itself is not confusion and is not stopped by Safeguard.
  PokemonCollection confusionScopeCollection; CollectionLogic::initialize(confusionScopeCollection);
  assert(CollectionLogic::chooseStarter(confusionScopeCollection, 4, 890));
  OwnedPokemon* confusionUser = CollectionLogic::active(confusionScopeCollection, 0);
  confusionUser->moves[0] = confuseRayMove;
  confusionUser->movePp[0] = findFullMove(confuseRayMove)->pp;
  BattleState confusionScopeBattle; BattleEngine::clear(confusionScopeBattle);
  confusionScopeBattle.active=true;confusionScopeBattle.kind=BattleKind::Trainer;
  confusionScopeBattle.outcome=BattleOutcome::Ongoing;
  confusionScopeBattle.playerUid=confusionUser->uid;confusionScopeBattle.opponentCount=1;
  confusionScopeBattle.rngState=0x0B110001U;
  confusionScopeBattle.opponents[0]=CollectionLogic::createPokemon(0,19,5,false,891);
  confusionScopeBattle.opponents[0].abilityId=12; // OBLIVIOUS
  for(uint8_t slot=0;slot<kMoveSlots;++slot){
    confusionScopeBattle.opponents[0].moves[slot]=MoveId::None;
    confusionScopeBattle.opponents[0].movePp[slot]=0;
  }
  assert(BattleEngine::fight(confusionScopeBattle,confusionScopeCollection,0).accepted);
  assert(confusionScopeBattle.opponentVolatiles[0].confusionTurns>0);
  confusionScopeBattle.opponentVolatiles[0].confusionTurns=0;
  confusionScopeBattle.opponents[0].abilityId=0;
  confusionUser->moves[0]=static_cast<MoveId>(213); // ATTRACT
  confusionUser->movePp[0]=findFullMove(confusionUser->moves[0])->pp;
  confusionScopeBattle.opponentSafeguardTurns=5;
  assert(BattleEngine::fight(confusionScopeBattle,confusionScopeCollection,0).accepted);
  assert(confusionScopeBattle.opponentVolatiles[0].confusionTurns==0);

  // Status reflected by SYNCHRONIZE is an Ability effect in FireRed and
  // explicitly bypasses Safeguard. The move's original target is still
  // protected only by its own side condition.
  PokemonCollection synchronizeCollection; CollectionLogic::initialize(synchronizeCollection);
  assert(CollectionLogic::chooseStarter(synchronizeCollection,4,892));
  OwnedPokemon* synchronizeUser=CollectionLogic::active(synchronizeCollection,0);
  synchronizeUser->moves[0]=toxicMove;
  synchronizeUser->movePp[0]=findFullMove(toxicMove)->pp;
  BattleState synchronizeBattle;BattleEngine::clear(synchronizeBattle);
  synchronizeBattle.active=true;synchronizeBattle.kind=BattleKind::Trainer;
  synchronizeBattle.outcome=BattleOutcome::Ongoing;synchronizeBattle.playerUid=synchronizeUser->uid;
  synchronizeBattle.playerSafeguardTurns=5;synchronizeBattle.opponentCount=1;
  synchronizeBattle.rngState=0x5A1E0001U;
  synchronizeBattle.playerVolatile.accuracyStage=6;
  synchronizeBattle.opponents[0]=CollectionLogic::createPokemon(0,19,5,false,893);
  synchronizeBattle.opponents[0].abilityId=28; // SYNCHRONIZE
  synchronizeBattle.opponentVolatiles[0].evasionStage=-6;
  for(uint8_t slot=0;slot<kMoveSlots;++slot){
    synchronizeBattle.opponents[0].moves[slot]=MoveId::None;
    synchronizeBattle.opponents[0].movePp[slot]=0;
  }
  assert(BattleEngine::fight(synchronizeBattle,synchronizeCollection,0).accepted);
  assert(synchronizeBattle.opponents[0].status==StatusCondition::BadlyPoisoned);
  // FireRed deliberately downgrades reflected Toxic to regular poison.
  assert(synchronizeUser->status==StatusCondition::Poison);

  // The opponent receives exactly the same side-wide implementation.
  PokemonCollection enemySafeguardCollection; CollectionLogic::initialize(enemySafeguardCollection);
  assert(CollectionLogic::chooseStarter(enemySafeguardCollection, 7, 883));
  OwnedPokemon* enemySafeguardTarget = CollectionLogic::active(enemySafeguardCollection, 0);
  enemySafeguardTarget->moves[0] = toxicMove;
  enemySafeguardTarget->movePp[0] = findFullMove(toxicMove)->pp;
  BattleState enemySafeguardBattle; BattleEngine::clear(enemySafeguardBattle);
  enemySafeguardBattle.active = true; enemySafeguardBattle.kind = BattleKind::Trainer;
  enemySafeguardBattle.outcome = BattleOutcome::Ongoing;
  enemySafeguardBattle.playerUid = enemySafeguardTarget->uid;
  enemySafeguardBattle.playerVolatile.accuracyStage = 6;
  enemySafeguardBattle.opponentCount = 1; enemySafeguardBattle.rngState = 0x53414646U;
  enemySafeguardBattle.opponents[0] = CollectionLogic::createPokemon(0, 1, 50, false, 884);
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    enemySafeguardBattle.opponents[0].moves[slot] = MoveId::None;
    enemySafeguardBattle.opponents[0].movePp[slot] = 0;
  }
  enemySafeguardBattle.opponents[0].moves[0] = safeguardMove;
  enemySafeguardBattle.opponents[0].movePp[0] = findFullMove(safeguardMove)->pp;
  const BattleActionResult enemySafeguardSet = BattleEngine::fight(
      enemySafeguardBattle, enemySafeguardCollection, 0);
  assert(enemySafeguardSet.accepted && enemySafeguardBattle.opponentSafeguardTurns == 4 &&
         enemySafeguardBattle.opponents[0].status == StatusCondition::None);

  // USER-target moves never interact with the opposing target. In particular,
  // Protect on one side must not block Swords Dance/Safeguard on the other.
  const MoveId protectMove = static_cast<MoveId>(182);
  const MoveId swordsDanceMove = static_cast<MoveId>(14);
  PokemonCollection selfTargetCollection; CollectionLogic::initialize(selfTargetCollection);
  assert(CollectionLogic::chooseStarter(selfTargetCollection, 1, 885));
  OwnedPokemon* selfTargetUser = CollectionLogic::active(selfTargetCollection, 0);
  selfTargetUser->moves[0] = swordsDanceMove;
  selfTargetUser->movePp[0] = findFullMove(swordsDanceMove)->pp;
  BattleState selfTargetBattle; BattleEngine::clear(selfTargetBattle);
  selfTargetBattle.active = true; selfTargetBattle.kind = BattleKind::Trainer;
  selfTargetBattle.outcome = BattleOutcome::Ongoing; selfTargetBattle.playerUid = selfTargetUser->uid;
  selfTargetBattle.opponentCount = 1; selfTargetBattle.rngState = 0x53454C46U;
  selfTargetBattle.opponents[0] = CollectionLogic::createPokemon(0, 19, 5, false, 886);
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    selfTargetBattle.opponents[0].moves[slot] = MoveId::None;
    selfTargetBattle.opponents[0].movePp[slot] = 0;
  }
  selfTargetBattle.opponents[0].moves[0] = protectMove;
  selfTargetBattle.opponents[0].movePp[0] = findFullMove(protectMove)->pp;
  const BattleActionResult selfThroughProtect = BattleEngine::fight(
      selfTargetBattle, selfTargetCollection, 0);
  assert(selfThroughProtect.accepted && selfTargetBattle.playerVolatile.attackStage == 2);

  selfTargetUser->moves[0] = protectMove;
  selfTargetUser->movePp[0] = findFullMove(protectMove)->pp;
  selfTargetBattle.opponents[0].moves[0] = swordsDanceMove;
  selfTargetBattle.opponents[0].movePp[0] = findFullMove(swordsDanceMove)->pp;
  selfTargetBattle.opponentVolatiles[0].attackStage = 0;
  const BattleActionResult enemySelfThroughProtect = BattleEngine::fight(
      selfTargetBattle, selfTargetCollection, 0);
  assert(enemySelfThroughProtect.accepted &&
         selfTargetBattle.opponentVolatiles[0].attackStage == 2);

  // Every one of FireRed's 67 MOVE_TARGET_USER moves bypasses the opposing
  // Pokemon's Protect and accuracy/evasion path. Exercise the complete set in
  // both directions so adding a bespoke handler cannot accidentally regress
  // only trainer/PvP use. A move may legitimately report Failed when its own
  // prerequisite is absent (REST at full HP, SWALLOW without STOCKPILE), but
  // it may never report MoveMissed against the other battler.
  for(uint16_t rawMove=1;rawMove<=354;++rawMove){
    const MoveId auditedMove=static_cast<MoveId>(rawMove);
    const FullMoveData* auditedData=findFullMove(auditedMove);
    if(!auditedData||auditedData->target!=MoveTarget::User)continue;

    PokemonCollection playerAuditCollection;CollectionLogic::initialize(playerAuditCollection);
    assert(CollectionLogic::chooseStarter(playerAuditCollection,1,0xA000U+rawMove));
    OwnedPokemon* playerAuditUser=CollectionLogic::active(playerAuditCollection,0);
    playerAuditUser->moves[0]=auditedMove;playerAuditUser->movePp[0]=std::max<uint8_t>(1U,auditedData->pp);
    BattleState playerAuditBattle;BattleEngine::clear(playerAuditBattle);
    playerAuditBattle.active=true;playerAuditBattle.kind=BattleKind::Trainer;
    playerAuditBattle.outcome=BattleOutcome::Ongoing;playerAuditBattle.playerUid=playerAuditUser->uid;
    playerAuditBattle.opponentCount=1;playerAuditBattle.rngState=0xB000U+rawMove;
    playerAuditBattle.opponents[0]=CollectionLogic::createPokemon(0,113,50,false,0xC000U+rawMove);
    for(uint8_t slot=0;slot<kMoveSlots;++slot)playerAuditBattle.opponents[0].moves[slot]=MoveId::None;
    playerAuditBattle.opponentVolatiles[0].protectedThisTurn=true;
    playerAuditBattle.opponentVolatiles[0].evasionStage=6;
    const BattleActionResult playerAudit=BattleEngine::fight(playerAuditBattle,playerAuditCollection,0);
    assert(playerAudit.accepted);
    for(uint8_t event=0;event<playerAudit.eventCount;++event)
      assert(!(playerAudit.events[event].type==BattleEventType::MoveMissed&&
               playerAudit.events[event].side==BattleSide::Player));

    PokemonCollection enemyAuditCollection;CollectionLogic::initialize(enemyAuditCollection);
    assert(CollectionLogic::chooseStarter(enemyAuditCollection,1,0xD000U+rawMove));
    OwnedPokemon* enemyAuditTarget=CollectionLogic::active(enemyAuditCollection,0);
    enemyAuditTarget->moves[0]=static_cast<MoveId>(150); // SPLASH: safe local action.
    enemyAuditTarget->movePp[0]=40;
    BattleState enemyAuditBattle;BattleEngine::clear(enemyAuditBattle);
    enemyAuditBattle.active=true;enemyAuditBattle.kind=BattleKind::Pvp;
    enemyAuditBattle.outcome=BattleOutcome::Ongoing;enemyAuditBattle.playerUid=enemyAuditTarget->uid;
    enemyAuditBattle.opponentCount=1;enemyAuditBattle.rngState=0xE000U+rawMove;
    enemyAuditBattle.opponents[0]=CollectionLogic::createPokemon(0,386,100,false,0xF000U+rawMove);
    enemyAuditBattle.opponents[0].moves[0]=auditedMove;
    enemyAuditBattle.opponents[0].movePp[0]=std::max<uint8_t>(1U,auditedData->pp);
    enemyAuditBattle.playerVolatile.protectedThisTurn=true;
    enemyAuditBattle.playerVolatile.evasionStage=6;
    const BattleActionResult enemyAudit=BattleEngine::fightPvp(
        enemyAuditBattle,enemyAuditCollection,0,0);
    assert(enemyAudit.accepted);
    for(uint8_t event=0;event<enemyAudit.eventCount;++event)
      assert(!(enemyAudit.events[event].type==BattleEventType::MoveMissed&&
               enemyAudit.events[event].side==BattleSide::Opponent));
  }

  // Screens and Mist are side conditions: they alter incoming moves, survive
  // the active Pokemon's volatile state, and work identically for either
  // combatant. Compare deterministic turns with and without each condition.
  auto incomingDamageWithScreen=[](uint8_t screenKind)->uint16_t{
    PokemonCollection c;CollectionLogic::initialize(c);assert(CollectionLogic::chooseStarter(c,7,0x5151));
    OwnedPokemon* p=CollectionLogic::active(c,0);p->level=50;CollectionLogic::refreshDerivedStats(*p,false);
    p->moves[0]=static_cast<MoveId>(150);p->movePp[0]=40;
    BattleState b;BattleEngine::clear(b);b.active=true;b.kind=BattleKind::Pvp;
    b.outcome=BattleOutcome::Ongoing;b.playerUid=p->uid;b.opponentCount=1;b.rngState=0x1234ABCDU;
    b.opponents[0]=CollectionLogic::createPokemon(0,4,30,false,0x5252);
    b.opponents[0].moves[0]=screenKind>=2?MoveId::Ember:MoveId::Tackle;
    b.opponents[0].movePp[0]=35;
    if(screenKind==1)b.playerReflectTurns=5;
    if(screenKind==2)b.playerLightScreenTurns=5;
    const uint16_t before=p->currentHp;
    const BattleActionResult r=BattleEngine::fightPvp(b,c,0,0);assert(r.accepted);
    return static_cast<uint16_t>(before-p->currentHp);
  };
  const uint16_t physicalWithoutScreen=incomingDamageWithScreen(0);
  const uint16_t physicalWithReflect=incomingDamageWithScreen(1);
  assert(physicalWithoutScreen>0&&physicalWithReflect>0&&
         physicalWithReflect<=static_cast<uint16_t>(physicalWithoutScreen/2U+1U));
  const uint16_t specialWithoutScreen=incomingDamageWithScreen(3);
  const uint16_t specialWithLightScreen=incomingDamageWithScreen(2);
  assert(specialWithoutScreen>0&&specialWithLightScreen>0&&
         specialWithLightScreen<=static_cast<uint16_t>(specialWithoutScreen/2U+1U));

  PokemonCollection mistCollection;CollectionLogic::initialize(mistCollection);
  assert(CollectionLogic::chooseStarter(mistCollection,1,0x5353));
  OwnedPokemon* mistUser=CollectionLogic::active(mistCollection,0);
  mistUser->moves[0]=static_cast<MoveId>(150);mistUser->movePp[0]=40;
  BattleState mistBattle;BattleEngine::clear(mistBattle);mistBattle.active=true;
  mistBattle.kind=BattleKind::Pvp;mistBattle.outcome=BattleOutcome::Ongoing;
  mistBattle.playerUid=mistUser->uid;mistBattle.opponentCount=1;mistBattle.playerMistTurns=5;
  mistBattle.opponents[0]=CollectionLogic::createPokemon(0,19,20,false,0x5454);
  mistBattle.opponents[0].moves[0]=static_cast<MoveId>(45);mistBattle.opponents[0].movePp[0]=40;
  assert(BattleEngine::fightPvp(mistBattle,mistCollection,0,0).accepted);
  assert(mistBattle.playerVolatile.attackStage==0);

  // REST restores all HP and stores FireRed's raw three-turn sleep counter
  // (the first decrement happens before the next attempted action);
  // the former generic half-heal path silently restored only 50% for players.
  PokemonCollection restCollection;CollectionLogic::initialize(restCollection);
  assert(CollectionLogic::chooseStarter(restCollection,1,0x5555));
  OwnedPokemon* restUser=CollectionLogic::active(restCollection,0);
  restUser->moves[0]=static_cast<MoveId>(156);restUser->movePp[0]=10;
  restUser->currentHp=std::max<uint16_t>(1U,restUser->maximumHp/4U);
  BattleState restBattle;BattleEngine::clear(restBattle);restBattle.active=true;
  restBattle.kind=BattleKind::Trainer;restBattle.outcome=BattleOutcome::Ongoing;
  restBattle.playerUid=restUser->uid;restBattle.opponentCount=1;
  restBattle.opponents[0]=CollectionLogic::createPokemon(0,10,2,false,0x5656);
  for(uint8_t slot=0;slot<kMoveSlots;++slot)restBattle.opponents[0].moves[slot]=MoveId::None;
  assert(BattleEngine::fight(restBattle,restCollection,0).accepted);
  assert(restUser->currentHp==restUser->maximumHp&&restUser->status==StatusCondition::Sleep&&
         restBattle.playerVolatile.sleepTurns==3);

  // jumpifcantmakeasleep also rejects REST while either active battler is in
  // Uproar. It must not heal first and then silently fail to apply Sleep.
  restUser->status=StatusCondition::None;
  restUser->currentHp=std::max<uint16_t>(1U,restUser->maximumHp/4U);
  restBattle.playerMoveEffects.uproar=true;
  const uint16_t beforeBlockedRest=restUser->currentHp;
  assert(BattleEngine::fight(restBattle,restCollection,0).accepted);
  assert(restUser->currentHp==beforeBlockedRest&&restUser->status==StatusCondition::None);

  // Recoil is symmetric: TAKE DOWN damages whichever side used it, emits a
  // visible recoil notice and a separate HP event after the target's damage.
  const MoveId takeDown = static_cast<MoveId>(36);
  PokemonCollection recoilCollection; CollectionLogic::initialize(recoilCollection);
  assert(CollectionLogic::chooseStarter(recoilCollection, 1, 910));
  OwnedPokemon* recoilUser = CollectionLogic::active(recoilCollection, 0);
  recoilUser->level = 30; CollectionLogic::refreshDerivedStats(*recoilUser, false);
  recoilUser->moves[0] = takeDown; recoilUser->movePp[0] = findFullMove(takeDown)->pp;
  BattleState recoilBattle; BattleEngine::clear(recoilBattle);
  recoilBattle.active = true; recoilBattle.kind = BattleKind::Trainer;
  recoilBattle.outcome = BattleOutcome::Ongoing; recoilBattle.playerUid = recoilUser->uid;
  recoilBattle.opponentCount = 1; recoilBattle.playerVolatile.accuracyStage = 6;
  recoilBattle.opponents[0] = CollectionLogic::createPokemon(0, 113, 30, false, 911);
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) recoilBattle.opponents[0].moves[slot] = MoveId::None;
  const uint16_t playerBeforeRecoil = recoilUser->currentHp;
  const BattleActionResult playerRecoil = BattleEngine::fight(recoilBattle, recoilCollection, 0);
  assert(playerRecoil.accepted && playerRecoil.hit && playerRecoil.damageDealt > 0);
  assert(playerBeforeRecoil - recoilUser->currentHp == std::max<uint16_t>(1U, playerRecoil.damageDealt / 4U));
  bool playerRecoilNotice = false, playerRecoilHp = false;
  for (uint8_t event = 0; event < playerRecoil.eventCount; ++event) {
    playerRecoilNotice |= playerRecoil.events[event].type == BattleEventType::MoveEffect &&
        playerRecoil.events[event].side == BattleSide::Player &&
        playerRecoil.events[event].value == static_cast<uint16_t>(BattleMoveEffect::Recoil);
    playerRecoilHp |= playerRecoil.events[event].type == BattleEventType::HpChanged &&
        playerRecoil.events[event].side == BattleSide::Player;
  }
  assert(playerRecoilNotice && playerRecoilHp);

  PokemonCollection enemyRecoilCollection; CollectionLogic::initialize(enemyRecoilCollection);
  assert(CollectionLogic::chooseStarter(enemyRecoilCollection, 7, 912));
  OwnedPokemon* enemyRecoilTarget = CollectionLogic::active(enemyRecoilCollection, 0);
  enemyRecoilTarget->level = 30; CollectionLogic::refreshDerivedStats(*enemyRecoilTarget, false);
  enemyRecoilTarget->moves[0] = static_cast<MoveId>(45); // GROWL keeps the target alive.
  enemyRecoilTarget->movePp[0] = findFullMove(enemyRecoilTarget->moves[0])->pp;
  BattleState enemyRecoilBattle; BattleEngine::clear(enemyRecoilBattle);
  enemyRecoilBattle.active = true; enemyRecoilBattle.kind = BattleKind::Trainer;
  enemyRecoilBattle.outcome = BattleOutcome::Ongoing; enemyRecoilBattle.playerUid = enemyRecoilTarget->uid;
  enemyRecoilBattle.opponentCount = 1; enemyRecoilBattle.opponentVolatiles[0].accuracyStage = 6;
  enemyRecoilBattle.opponents[0] = CollectionLogic::createPokemon(0, 1, 30, false, 913);
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) enemyRecoilBattle.opponents[0].moves[slot] = MoveId::None;
  enemyRecoilBattle.opponents[0].moves[0] = takeDown;
  enemyRecoilBattle.opponents[0].movePp[0] = findFullMove(takeDown)->pp;
  const uint16_t enemyBeforeRecoil = enemyRecoilBattle.opponents[0].currentHp;
  const BattleActionResult enemyRecoil = BattleEngine::fight(enemyRecoilBattle, enemyRecoilCollection, 0);
  assert(enemyRecoil.enemyMoveUsed == takeDown && enemyRecoil.damageTaken > 0);
  assert(enemyBeforeRecoil - enemyRecoilBattle.opponents[0].currentHp ==
         std::max<uint16_t>(1U, enemyRecoil.damageTaken / 4U));
  bool enemyRecoilNotice = false, enemyRecoilHp = false;
  for (uint8_t event = 0; event < enemyRecoil.eventCount; ++event) {
    enemyRecoilNotice |= enemyRecoil.events[event].type == BattleEventType::MoveEffect &&
        enemyRecoil.events[event].side == BattleSide::Opponent &&
        enemyRecoil.events[event].value == static_cast<uint16_t>(BattleMoveEffect::Recoil);
    enemyRecoilHp |= enemyRecoil.events[event].type == BattleEventType::HpChanged &&
        enemyRecoil.events[event].side == BattleSide::Opponent;
  }
  assert(enemyRecoilNotice && enemyRecoilHp);

  // STOCKPILE/SPIT UP/SWALLOW share FireRed's single 0..3 volatile counter.
  // SPIT UP fails safely at zero, scales from 100 to 300 power and consumes
  // the counter; SWALLOW heals 1/4, 1/2 or all HP and also consumes it.
  PokemonCollection stockCollection;CollectionLogic::initialize(stockCollection);
  assert(CollectionLogic::chooseStarter(stockCollection,1,0x5700));
  OwnedPokemon* stockUser=CollectionLogic::active(stockCollection,0);
  stockUser->level=50;CollectionLogic::refreshDerivedStats(*stockUser,false);
  stockUser->moves[0]=static_cast<MoveId>(254);stockUser->movePp[0]=10; // STOCKPILE
  stockUser->moves[1]=static_cast<MoveId>(255);stockUser->movePp[1]=10; // SPIT UP
  stockUser->moves[2]=static_cast<MoveId>(256);stockUser->movePp[2]=10; // SWALLOW
  BattleState stockBattle;BattleEngine::clear(stockBattle);stockBattle.active=true;
  stockBattle.kind=BattleKind::Trainer;stockBattle.outcome=BattleOutcome::Ongoing;
  stockBattle.playerUid=stockUser->uid;stockBattle.opponentCount=1;
  stockBattle.opponents[0]=CollectionLogic::createPokemon(0,242,100,false,0x5701);
  for(uint8_t slot=0;slot<kMoveSlots;++slot)stockBattle.opponents[0].moves[slot]=MoveId::None;
  for(uint8_t count=1;count<=3;++count){
    const BattleActionResult stored=BattleEngine::fight(stockBattle,stockCollection,0);
    assert(stored.accepted&&stockBattle.playerVolatile.stockpileCount==count);
  }
  const BattleActionResult overstock=BattleEngine::fight(stockBattle,stockCollection,0);
  assert(overstock.accepted&&!overstock.hit&&stockBattle.playerVolatile.stockpileCount==3);
  const uint16_t spitTargetBefore=stockBattle.opponents[0].currentHp;
  const BattleActionResult tripleSpit=BattleEngine::fight(stockBattle,stockCollection,1);
  assert(tripleSpit.accepted&&tripleSpit.hit&&tripleSpit.damageDealt>0&&
         stockBattle.opponents[0].currentHp<spitTargetBefore&&
         stockBattle.playerVolatile.stockpileCount==0);
  const uint16_t afterTripleSpit=stockBattle.opponents[0].currentHp;
  const BattleActionResult emptySpit=BattleEngine::fight(stockBattle,stockCollection,1);
  assert(emptySpit.accepted&&!emptySpit.hit&&emptySpit.damageDealt==0&&
         stockBattle.opponents[0].currentHp==afterTripleSpit);
  stockBattle.playerVolatile.stockpileCount=1;stockUser->currentHp=stockUser->maximumHp/4U;
  const uint16_t swallowBefore=stockUser->currentHp;
  const BattleActionResult swallowed=BattleEngine::fight(stockBattle,stockCollection,2);
  assert(swallowed.accepted&&stockBattle.playerVolatile.stockpileCount==0&&
         stockUser->currentHp==std::min<uint16_t>(stockUser->maximumHp,
             static_cast<uint16_t>(swallowBefore+stockUser->maximumHp/4U)));
  stockBattle.playerVolatile.stockpileCount=2;stockUser->currentHp=stockUser->maximumHp;
  const BattleActionResult fullSwallow=BattleEngine::fight(stockBattle,stockCollection,2);
  assert(fullSwallow.accepted&&stockBattle.playerVolatile.stockpileCount==0&&
         stockUser->currentHp==stockUser->maximumHp);

  // The same counter and release behavior must work for the opponent path.
  stockUser->moves[0]=static_cast<MoveId>(150);stockUser->movePp[0]=40; // SPLASH
  stockBattle.opponents[0].moves[0]=static_cast<MoveId>(254);
  stockBattle.opponents[0].movePp[0]=10;
  assert(BattleEngine::fight(stockBattle,stockCollection,0).accepted);
  assert(stockBattle.opponentVolatiles[0].stockpileCount==1);
  stockBattle.opponents[0].moves[0]=static_cast<MoveId>(255);
  stockBattle.opponents[0].movePp[0]=10;
  const uint16_t stockUserBeforeEnemySpit=stockUser->currentHp;
  assert(BattleEngine::fight(stockBattle,stockCollection,0).accepted);
  assert(stockBattle.opponentVolatiles[0].stockpileCount==0&&
         stockUser->currentHp<stockUserBeforeEnemySpit);

  // Trainer AI must understand the complete STOCKPILE -> SPIT UP cycle.
  // With SWALLOW unusable at full HP it stores exactly three stages, never
  // selects an empty SPIT UP (including the random-choice branch), and only
  // then releases the accumulated attack.
  PokemonCollection stockAiCollection;CollectionLogic::initialize(stockAiCollection);
  assert(CollectionLogic::chooseStarter(stockAiCollection,1,0x5710));
  OwnedPokemon* stockAiTarget=CollectionLogic::active(stockAiCollection,0);
  stockAiTarget->level=50;CollectionLogic::refreshDerivedStats(*stockAiTarget,false);
  stockAiTarget->maximumHp=stockAiTarget->currentHp=5000;
  for(uint8_t slot=0;slot<kMoveSlots;++slot){
    stockAiTarget->moves[slot]=MoveId::None;stockAiTarget->movePp[slot]=0;
  }
  stockAiTarget->moves[0]=static_cast<MoveId>(150);stockAiTarget->movePp[0]=40;
  BattleState stockAiBattle;BattleEngine::clear(stockAiBattle);stockAiBattle.active=true;
  stockAiBattle.kind=BattleKind::Trainer;stockAiBattle.outcome=BattleOutcome::Ongoing;
  stockAiBattle.playerUid=stockAiTarget->uid;stockAiBattle.opponentCount=1;
  stockAiBattle.opponents[0]=CollectionLogic::createPokemon(0,24,50,false,0x5711);
  OwnedPokemon& stockAiArbok=stockAiBattle.opponents[0];
  stockAiArbok.maximumHp=stockAiArbok.currentHp=5000;
  for(uint8_t slot=0;slot<kMoveSlots;++slot){
    stockAiArbok.moves[slot]=MoveId::None;stockAiArbok.movePp[slot]=0;
  }
  stockAiArbok.moves[0]=static_cast<MoveId>(254);stockAiArbok.movePp[0]=10;
  stockAiArbok.moves[1]=static_cast<MoveId>(255);stockAiArbok.movePp[1]=10;
  stockAiArbok.moves[2]=static_cast<MoveId>(256);stockAiArbok.movePp[2]=10;
  for(uint8_t count=1;count<=3;++count){
    const BattleActionResult setup=BattleEngine::fight(stockAiBattle,stockAiCollection,0);
    assert(setup.accepted&&setup.enemyMoveUsed==static_cast<MoveId>(254)&&
           stockAiBattle.opponentVolatiles[0].stockpileCount==count);
  }
  const BattleActionResult released=BattleEngine::fight(stockAiBattle,stockAiCollection,0);
  assert(released.accepted&&released.enemyMoveUsed==static_cast<MoveId>(255)&&
         released.damageTaken>0&&
         stockAiBattle.opponentVolatiles[0].stockpileCount==0);

  // Automatic and permanent held-item effects are resolved by the battle
  // engine and a consumed Berry does not return to the Bag.
  PokemonCollection heldCollection;CollectionLogic::initialize(heldCollection);assert(CollectionLogic::chooseStarter(heldCollection,4,123));
  OwnedPokemon* heldUser=CollectionLogic::active(heldCollection,0);heldUser->moves[0]=MoveId::Scratch;heldUser->movePp[0]=35;
  BattleState heldBattle;heldBattle.active=true;heldBattle.kind=BattleKind::Trainer;heldBattle.outcome=BattleOutcome::Ongoing;heldBattle.playerUid=heldUser->uid;heldBattle.opponentCount=1;
  heldBattle.opponents[0]=CollectionLogic::createPokemon(0,150,100,false,321);for(uint8_t i=0;i<kMoveSlots;++i)heldBattle.opponents[0].moves[i]=MoveId::None;
  heldUser->currentHp=heldUser->maximumHp/2U;heldUser->heldItem=HeldItem::OranBerry;
  const uint16_t berryHp=heldUser->currentHp;assert(BattleEngine::fight(heldBattle,heldCollection,0).accepted);
  assert(heldUser->currentHp>berryHp&&heldUser->heldItem==HeldItem::None);
  heldUser->currentHp=static_cast<uint16_t>(heldUser->maximumHp-10U);heldUser->heldItem=HeldItem::Leftovers;
  const uint16_t leftoversHp=heldUser->currentHp;assert(BattleEngine::fight(heldBattle,heldCollection,0).accepted);assert(heldUser->currentHp>leftoversHp&&heldUser->heldItem==HeldItem::Leftovers);
  heldUser->moves[1]=MoveId::Ember;heldUser->movePp[1]=25;heldUser->heldItem=HeldItem::ChoiceBand;heldBattle.playerVolatile.choiceMove=MoveId::None;
  assert(BattleEngine::fight(heldBattle,heldCollection,0).accepted);assert(!BattleEngine::fight(heldBattle,heldCollection,1).accepted);

  BattleState smokeBattle;assert(BattleEngine::startWild(smokeBattle,heldCollection,heldUser->uid,999));heldUser->heldItem=HeldItem::SmokeBall;
  const BattleActionResult smokeRun=BattleEngine::run(smokeBattle,heldCollection);assert(smokeRun.accepted&&smokeRun.outcome==BattleOutcome::Escaped);

  // RUN is a surrender in every local trainer-style encounter. It closes the
  // battle immediately without prize money or victory progression; Gym and
  // League retry state is owned by their persistent progress structures.
  for (const BattleKind kind : {BattleKind::Trainer, BattleKind::Gym, BattleKind::League}) {
    BattleState surrenderBattle; BattleEngine::clear(surrenderBattle);
    surrenderBattle.active=true; surrenderBattle.kind=kind;
    surrenderBattle.outcome=BattleOutcome::Ongoing; surrenderBattle.playerUid=heldUser->uid;
    surrenderBattle.opponentCount=1; surrenderBattle.rewardMoney=999;
    surrenderBattle.opponents[0]=CollectionLogic::createPokemon(0,150,100,false,456);
    heldUser->currentHp=heldUser->maximumHp;
    const uint16_t hpBeforeSurrender=heldUser->currentHp;
    const BattleActionResult surrendered=BattleEngine::run(surrenderBattle,heldCollection);
    assert(surrendered.accepted&&surrendered.outcome==BattleOutcome::Escaped);
    assert(!surrenderBattle.active&&surrenderBattle.outcome==BattleOutcome::Escaped);
    assert(surrendered.moneyGained==0&&surrendered.experienceGained==0);
    assert(heldUser->currentHp==hpBeforeSurrender);
  }

  BattleState coinBattle;coinBattle.active=true;coinBattle.kind=BattleKind::Trainer;coinBattle.outcome=BattleOutcome::Ongoing;coinBattle.playerUid=heldUser->uid;coinBattle.opponentCount=1;coinBattle.rewardMoney=100;
  coinBattle.opponents[0]=CollectionLogic::createPokemon(0,10,2,false,777);coinBattle.opponents[0].currentHp=1;for(uint8_t i=0;i<kMoveSlots;++i)coinBattle.opponents[0].moves[i]=MoveId::None;
  heldUser->heldItem=HeldItem::AmuletCoin;coinBattle.playerVolatile=CombatVolatile{};
  coinBattle.payDayMoney=25;
  const BattleActionResult coinWin=BattleEngine::fight(coinBattle,heldCollection,0);assert(coinWin.outcome==BattleOutcome::Victory&&coinWin.moneyGained==250);

  // Thief, Covet, Trick and Knock Off manipulate equipment for this battle
  // only. The saved held item is never silently lost to a temporary NPC/wild.
  auto itemMoveBattle=[&](MoveId move,HeldItem playerItem,HeldItem opponentItem){
    BattleState state;BattleEngine::clear(state);state.active=true;state.kind=BattleKind::Trainer;state.outcome=BattleOutcome::Ongoing;state.playerUid=heldUser->uid;state.opponentCount=1;
    state.opponents[0]=CollectionLogic::createPokemon(0,150,100,false,987);state.opponents[0].heldItem=opponentItem;
    for(uint8_t i=0;i<kMoveSlots;++i)state.opponents[0].moves[i]=MoveId::None;
    heldUser->moves[0]=move;heldUser->movePp[0]=20;heldUser->heldItem=playerItem;heldUser->currentHp=heldUser->maximumHp;
    return state;
  };
  BattleState thiefBattle=itemMoveBattle(static_cast<MoveId>(168),HeldItem::None,HeldItem::Leftovers);
  const BattleActionResult thiefResult=BattleEngine::fight(thiefBattle,heldCollection,0);
  assert(thiefResult.accepted&&thiefResult.heldItemStolen&&thiefResult.affectedHeldItem==HeldItem::Leftovers);
  assert(thiefBattle.playerVolatile.hasHeldItemOverride&&thiefBattle.playerVolatile.heldItemOverride==HeldItem::Leftovers);
  assert(thiefBattle.opponentVolatiles[0].hasHeldItemOverride&&thiefBattle.opponentVolatiles[0].heldItemOverride==HeldItem::None);
  assert(heldUser->heldItem==HeldItem::None&&thiefBattle.opponents[0].heldItem==HeldItem::Leftovers);
  BattleState covetBattle=itemMoveBattle(static_cast<MoveId>(343),HeldItem::None,HeldItem::SmokeBall);
  assert(BattleEngine::fight(covetBattle,heldCollection,0).heldItemStolen&&covetBattle.playerVolatile.heldItemOverride==HeldItem::SmokeBall);
  BattleState trickBattle=itemMoveBattle(static_cast<MoveId>(271),HeldItem::SmokeBall,HeldItem::Leftovers);
  const BattleActionResult trickResult=BattleEngine::fight(trickBattle,heldCollection,0);
  assert(trickResult.accepted&&trickResult.heldItemSwapped);
  assert(heldUser->heldItem==HeldItem::Leftovers&&trickBattle.opponents[0].heldItem==HeldItem::SmokeBall);
  bool sawTrickMessage=false;
  for(uint8_t event=0;event<trickResult.eventCount;++event)
    sawTrickMessage|=trickResult.events[event].type==BattleEventType::MoveEffect&&
        trickResult.events[event].value==static_cast<uint16_t>(BattleMoveEffect::HeldItemsSwapped);
  assert(sawTrickMessage);
  BattleState trickTakeBattle=itemMoveBattle(static_cast<MoveId>(271),HeldItem::None,HeldItem::Leftovers);
  assert(BattleEngine::fight(trickTakeBattle,heldCollection,0).heldItemSwapped);
  assert(heldUser->heldItem==HeldItem::Leftovers&&trickTakeBattle.opponents[0].heldItem==HeldItem::None);
  BattleState trickGiveBattle=itemMoveBattle(static_cast<MoveId>(271),HeldItem::SmokeBall,HeldItem::None);
  assert(BattleEngine::fight(trickGiveBattle,heldCollection,0).heldItemSwapped);
  assert(heldUser->heldItem==HeldItem::None&&trickGiveBattle.opponents[0].heldItem==HeldItem::SmokeBall);
  BattleState emptyTrickBattle=itemMoveBattle(static_cast<MoveId>(271),HeldItem::None,HeldItem::None);
  assert(!BattleEngine::fight(emptyTrickBattle,heldCollection,0).heldItemSwapped);
  BattleState substituteTrickBattle=itemMoveBattle(static_cast<MoveId>(271),HeldItem::SmokeBall,HeldItem::Leftovers);
  substituteTrickBattle.opponentVolatiles[0].substituteHp=10;
  assert(!BattleEngine::fight(substituteTrickBattle,heldCollection,0).heldItemSwapped);
  assert(heldUser->heldItem==HeldItem::SmokeBall&&substituteTrickBattle.opponents[0].heldItem==HeldItem::Leftovers);
  BattleState enemyTrickBattle=itemMoveBattle(static_cast<MoveId>(150),HeldItem::Leftovers,HeldItem::SmokeBall);
  enemyTrickBattle.opponents[0].moves[0]=static_cast<MoveId>(271);
  enemyTrickBattle.opponents[0].movePp[0]=10;
  const BattleActionResult enemyTrick=BattleEngine::fight(enemyTrickBattle,heldCollection,0);
  assert(enemyTrick.enemyMoveUsed==static_cast<MoveId>(271)&&!enemyTrick.heldItemSwapped);
  assert(heldUser->heldItem==HeldItem::Leftovers&&enemyTrickBattle.opponents[0].heldItem==HeldItem::SmokeBall);
  BattleState pvpTrickBattle=itemMoveBattle(static_cast<MoveId>(150),HeldItem::Leftovers,HeldItem::SmokeBall);
  pvpTrickBattle.kind=BattleKind::Pvp;
  pvpTrickBattle.opponents[0].moves[0]=static_cast<MoveId>(271);
  pvpTrickBattle.opponents[0].movePp[0]=10;
  const BattleActionResult pvpTrick=BattleEngine::fightPvp(pvpTrickBattle,heldCollection,0,0);
  assert(pvpTrick.heldItemSwapped&&pvpTrickBattle.playerVolatile.hasHeldItemOverride&&
         pvpTrickBattle.playerVolatile.heldItemOverride==HeldItem::SmokeBall&&
         pvpTrickBattle.opponentVolatiles[0].heldItemOverride==HeldItem::Leftovers);
  assert(heldUser->heldItem==HeldItem::Leftovers&&pvpTrickBattle.opponents[0].heldItem==HeldItem::SmokeBall);
  BattleState knockBattle=itemMoveBattle(static_cast<MoveId>(282),HeldItem::None,HeldItem::Leftovers);
  const BattleActionResult knockResult=BattleEngine::fight(knockBattle,heldCollection,0);
  assert(knockResult.accepted&&knockResult.heldItemKnockedOff&&knockResult.affectedHeldItem==HeldItem::Leftovers&&knockBattle.opponentVolatiles[0].heldItemSuppressed);
  assert(knockBattle.opponents[0].heldItem==HeldItem::Leftovers);
  assert(battle.playerVolatile.attackStage == 1);
  BattleEngine::clear(battle);

  PokedexState pokedex;
  PokedexLogic::markSeen(pokedex, 16);
  assert(PokedexLogic::hasSeen(pokedex, 16));
  assert(!PokedexLogic::hasCaught(pokedex, 16));
  PokedexLogic::markCaught(pokedex, 16);
  assert(PokedexLogic::hasCaught(pokedex, 16));
  assert(PokedexLogic::seenCount(pokedex) == 1);
  assert(PokedexLogic::caughtCount(pokedex) == 1);

  // Each regional block is completed independently. Reconciliation grants
  // one persistent Master Ball, queues a visible acknowledgement and never
  // duplicates the reward on a later Home visit or reboot.
  PokedexState completedDex{};
  for (uint16_t id = 1; id <= 151; ++id) PokedexLogic::markCaught(completedDex, id);
  assert(PokedexLogic::generationSpeciesCount(1) == 151);
  assert(PokedexLogic::generationSpeciesCount(2) == 251);
  assert(PokedexLogic::generationSpeciesCount(3) == 386);
  assert(PokedexLogic::generationCaughtCount(completedDex, 1) == 151);
  assert(PokedexLogic::isGenerationComplete(completedDex, 1));
  assert(!PokedexLogic::isGenerationComplete(completedDex, 2));
  Inventory rewardInventory{};
  rewardInventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] = 0;
  uint8_t rewardFlags = 0;
  PokedexRewardUpdate reward = PokedexRewards::reconcile(completedDex, rewardInventory, 1, rewardFlags);
  assert(reward.grantedGenerations == 0x01 && reward.pendingGeneration == 1);
  assert(rewardInventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] == 1);
  reward = PokedexRewards::reconcile(completedDex, rewardInventory, 1, rewardFlags);
  assert(reward.grantedGenerations == 0 && reward.pendingGeneration == 1);
  assert(rewardInventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] == 1);
  PokedexRewards::acknowledge(rewardFlags, 1);
  assert(PokedexRewards::pendingGeneration(rewardFlags, 1) == 0);
  for (uint16_t id = 152; id <= 251; ++id) PokedexLogic::markCaught(completedDex, id);
  // A locked generation cannot pay out early even if trade/debug data has
  // already filled its entries.
  reward = PokedexRewards::reconcile(completedDex, rewardInventory, 1, rewardFlags);
  assert(reward.grantedGenerations == 0);
  reward = PokedexRewards::reconcile(completedDex, rewardInventory, 2, rewardFlags);
  assert(reward.grantedGenerations == 0x02 && reward.pendingGeneration == 2);
  assert(rewardInventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] == 2);

  charges.available = 0;
  charges.rechargeTargetSeconds = EncounterCharges::kMinimumRechargeSeconds;
  charges.rechargeProgressSeconds = 0;
  EncounterLogic::advance(charges, EncounterCharges::kMinimumRechargeSeconds - 1);
  assert(charges.available == 0);
  EncounterLogic::advance(charges, 1);
  assert(charges.available == 1);
  EncounterLogic::advance(charges, EncounterCharges::kMinimumRechargeSeconds * 20U);
  assert(charges.available == EncounterCharges::kMaximum);

  // No charge is consumed if a battle is already active or none is available.
  assert(BattleEngine::startTrainer(battle, charges, collection, starterUid, 111));
  const uint8_t afterStart = charges.available;
  assert(!BattleEngine::startTrainer(battle, charges, collection, starterUid, 222));
  assert(charges.available == afterStart);
  battle = BattleState{};
  charges.available = 0;
  assert(!BattleEngine::startTrainer(battle, charges, collection, starterUid, 333));
  assert(charges.available == 0);

  // The persistent Box has one slot per Generation-I species. A full Box
  // rejects both direct additions and capture attempts without consuming a ball.
  while (CollectionLogic::count(collection) < kBoxCapacity) {
    assert(CollectionLogic::add(collection, CollectionLogic::createPokemon(0, 19, 2)));
  }
  assert(!CollectionLogic::add(collection, CollectionLogic::createPokemon(0, 16, 3)));
  battle = BattleState{};
  battle.active = true;
  battle.outcome = BattleOutcome::Ongoing;
  battle.playerUid = collection.party[0];
  battle.kind = BattleKind::Wild;
  battle.opponentCount = 1;
  battle.opponents[0] = CollectionLogic::createPokemon(0, 16, 3);
  inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] = 1;
  const BattleActionResult fullBoxCapture = BattleEngine::throwBall(
      battle, collection, inventory, PokeBallType::MasterBall);
  assert(!fullBoxCapture.accepted && !fullBoxCapture.caught);
  assert(inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] == 1);

  // Trainer teams can contain up to three Pokemon and can never be captured.
  PokemonCollection trainerCollection;
  CollectionLogic::initialize(trainerCollection);
  assert(CollectionLogic::chooseStarter(trainerCollection, 1));
  CollectionLogic::active(trainerCollection, 0)->level = 45;
  EncounterCharges trainerCharges;
  BattleState trainerBattle;
  assert(BattleEngine::startTrainer(trainerBattle, trainerCharges, trainerCollection,
                                    trainerCollection.party[0], 999));
  assert(trainerBattle.kind == BattleKind::Trainer);
  assert(trainerBattle.opponentCount >= 1 && trainerBattle.opponentCount <= 3);
  for (uint8_t i = 0; i < trainerBattle.opponentCount; ++i) {
    assert(trainerBattle.opponents[i].level >= 44 && trainerBattle.opponents[i].level <= 46);
  }
  const uint16_t ballCount = inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)];
  assert(!BattleEngine::throwBall(trainerBattle, trainerCollection, inventory,
                                  PokeBallType::MasterBall).accepted);
  assert(inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] == ballCount);

  WildEncounterClock wildClock;
  // The legacy Wild clock now stores the five Pokemon Center charges. Wild
  // battles themselves are unlimited.
  assert(WildEncounterClock::kMinimumSeconds == 60U * 60U);
  assert(WildEncounterClock::kWindowSeconds == 0U);
  wildClock.available = 0;
  EncounterLogic::advanceWild(wildClock, WildEncounterClock::kMinimumSeconds - 1);
  assert(wildClock.available == 0);
  EncounterLogic::advanceWild(wildClock, 1);
  assert(wildClock.available == 1);
  assert(EncounterLogic::consumeWildCharge(wildClock));
  assert(wildClock.available == 0 && wildClock.elapsedSeconds == 0);
  assert(wildClock.targetSeconds >= WildEncounterClock::kMinimumSeconds);
  assert(wildClock.targetSeconds <= WildEncounterClock::kMinimumSeconds + WildEncounterClock::kWindowSeconds);
  // Center recovery runs whenever the stock is below five, not only after it
  // reaches zero. Two complete periods must refill a 3/5 stock to 5/5.
  wildClock.available = 3;
  wildClock.elapsedSeconds = 0;
  EncounterLogic::advanceCenter(wildClock, WildEncounterClock::kMinimumSeconds * 2U);
  assert(wildClock.available == WildEncounterClock::kMaximum);
  // Spending another charge while already below capacity preserves the
  // partial timer instead of restarting it from zero.
  wildClock.available = 3;
  wildClock.elapsedSeconds = WildEncounterClock::kMinimumSeconds / 2U;
  const uint32_t partialCenterProgress = wildClock.elapsedSeconds;
  assert(EncounterLogic::consumeCenterCharge(wildClock));
  assert(wildClock.available == 2);
  assert(wildClock.elapsedSeconds == partialCenterProgress);
  EncounterLogic::advanceCenter(wildClock, WildEncounterClock::kMinimumSeconds / 2U);
  assert(wildClock.available == 3 && wildClock.elapsedSeconds == 0);

  PokemonCollection recoveryCollection;
  CollectionLogic::initialize(recoveryCollection);
  assert(CollectionLogic::chooseStarter(recoveryCollection, 1));
  OwnedPokemon* recovering = CollectionLogic::active(recoveryCollection, 0);
  // Lv.21+ Party recovery completes in thirty minutes.
  recovering->level = 21;
  recovering->currentHp = 0;
  recovering->recoverySecondsRemaining = 1;
  recovering->status = StatusCondition::Poison;
  recovering->movePp[0] = 0;
  CollectionLogic::advanceRecovery(*recovering, 15U * 60U, true, 15U * 60U);
  assert(recovering->status == StatusCondition::None);
  assert(recovering->currentHp > 0 && recovering->currentHp < recovering->maximumHp);
  assert(recovering->recoverySecondsRemaining != 0); // Still locked after fainting.
  CollectionLogic::advanceRecovery(*recovering, 15U * 60U, true, 30U * 60U);
  assert(recovering->currentHp == recovering->maximumHp);
  assert(recovering->recoverySecondsRemaining == 0);

  recovering->currentHp = 0;
  recovering->recoverySecondsRemaining = 1;
  CollectionLogic::advanceRecovery(*recovering, 2U * 60U * 60U, false, 2U * 60U * 60U);
  assert(recovering->currentHp < recovering->maximumHp);
  CollectionLogic::advanceRecovery(*recovering, 60U * 60U, false, 3U * 60U * 60U);
  assert(recovering->currentHp == recovering->maximumHp);

  recovering->currentHp = 0;
  recovering->recoverySecondsRemaining = 1;
  // The accelerated rule includes Lv.20 exactly and restores HP/PP over 10m.
  recovering->level = 20;
  recovering->movePp[0] = 0;
  const uint8_t acceleratedMaximumPp = CollectionLogic::maximumMovePp(*recovering, 0);
  CollectionLogic::advanceRecovery(*recovering, 5U * 60U, true,
                                   5U * 60U);
  assert(recovering->currentHp > 0 && recovering->currentHp < recovering->maximumHp);
  assert(recovering->movePp[0] > 0 && recovering->movePp[0] < acceleratedMaximumPp);
  CollectionLogic::advanceRecovery(*recovering, 5U * 60U, true,
                                   10U * 60U);
  assert(recovering->currentHp == recovering->maximumHp);
  assert(recovering->movePp[0] == acceleratedMaximumPp);
  assert(recovering->recoverySecondsRemaining == 0);

  GymProgress gyms;
  assert(GymSystem::next(gyms) == GymId::Pewter);
  // Gym progression is level-gated and deterministic. Brock's ace is Lv14,
  // so the challenge is absent at Lv13 and immediately available at Lv14.
  assert(GymSystem::requiredLevel(GymId::Pewter)==14U);
  assert(!GymSystem::challengeAvailable(gyms,13U));
  assert(GymSystem::challengeAvailable(gyms,14U));
  const GymDefinition* brock = GymSystem::definition(GymId::Pewter);
  const GymDefinition* giovanni = GymSystem::definition(GymId::Viridian);
  assert(brock && brock->teamSize == 3 && brock->team[1].speciesId == 27);
  assert(giovanni && giovanni->teamSize == 3 && giovanni->team[2].level == 50);
  assert(!GymSystem::recordVictory(gyms, GymId::Cerulean));
  assert(GymSystem::recordVictory(gyms, GymId::Pewter));
  assert(GymSystem::hasBadge(gyms, GymId::Pewter) && GymSystem::next(gyms) == GymId::Cerulean);
  assert(GymSystem::requiredLevel(GymId::Cerulean)==21U);
  assert(!GymSystem::challengeAvailable(gyms,20U));
  assert(GymSystem::challengeAvailable(gyms,21U));
  // Kanto's Pokegochi-specific curve removes FireRed's route-dependent
  // fourteen-level jump between Erika and Koga. Each gate remains equal to
  // that Leader's ace level.
  constexpr uint8_t kKantoGymLevels[8]={14,21,24,29,34,39,44,50};
  for(uint8_t gym=0;gym<8;++gym)
    assert(GymSystem::requiredLevel(static_cast<GymId>(gym))==kKantoGymLevels[gym]);
  const GymDefinition* koga=GymSystem::definition(GymId::Fuchsia);
  const GymDefinition* sabrina=GymSystem::definition(GymId::Saffron);
  const GymDefinition* blaine=GymSystem::definition(GymId::Cinnabar);
  assert(koga&&koga->team[2].level==34U&&sabrina&&sabrina->team[2].level==39U&&
         blaine&&blaine->team[2].level==44U);
  for(uint8_t i=1;i<8;++i)assert(GymSystem::recordVictory(gyms,static_cast<GymId>(i)));
  assert(GymSystem::regionComplete(gyms,1)&&GymSystem::leagueAvailable(gyms));
  assert(!GymSystem::challengeAvailable(gyms,100U));
  assert(!GymSystem::needsRegionalStarter(gyms));
  assert(GymSystem::next(gyms)==GymId::Count);
  PokemonCollection leagueCollection;CollectionLogic::initialize(leagueCollection);
  assert(CollectionLogic::chooseStarter(leagueCollection,7));BattleState leagueBattle;
  assert(LeagueSystem::start(leagueBattle,leagueCollection,leagueCollection.party[0],gyms,77));
  assert(leagueBattle.kind==BattleKind::League&&leagueBattle.leagueRegion==1&&leagueBattle.leagueStage==0);
  assert(leagueBattle.opponentCount==3&&leagueBattle.opponents[0].speciesId==87&&
         leagueBattle.opponents[2].speciesId==131);
  for(uint8_t stage=1;stage<LeagueSystem::kMemberCount;++stage){leagueBattle.active=false;leagueBattle.outcome=BattleOutcome::Victory;assert(LeagueSystem::advance(leagueBattle,leagueCollection));assert(leagueBattle.leagueStage==stage);}
  assert(LeagueSystem::isChampionStage(leagueBattle));
  assert(leagueBattle.opponents[2].speciesId==3); // Squirtle's rival saves Venusaur for last.
  leagueBattle.active=false;leagueBattle.outcome=BattleOutcome::Victory;
  assert(!LeagueSystem::advance(leagueBattle,leagueCollection));
  assert(GymSystem::recordLeagueVictory(gyms,1));
  assert(GymSystem::leagueComplete(gyms,1)&&!GymSystem::leagueAvailable(gyms));
  uint64_t luckyEggOwnership=0;
  assert(LeagueSystem::reconcileKantoLuckyEggReward(
      gyms,leagueCollection,luckyEggOwnership));
  assert(luckyEggOwnership&kLuckyEggOwnershipBit);
  assert(!LeagueSystem::reconcileKantoLuckyEggReward(
      gyms,leagueCollection,luckyEggOwnership));
  luckyEggOwnership&=~kLuckyEggOwnershipBit;
  CollectionLogic::active(leagueCollection,0)->heldItem=HeldItem::LuckyEgg;
  assert(!LeagueSystem::reconcileKantoLuckyEggReward(
      gyms,leagueCollection,luckyEggOwnership));
  CollectionLogic::active(leagueCollection,0)->heldItem=HeldItem::None;
  assert(GymSystem::needsRegionalStarter(gyms));
  assert(GymSystem::unlockWithStarter(gyms,152));
  assert(gyms.unlockedGeneration==2&&GymSystem::next(gyms)==GymId::Violet);
  assert(GymSystem::requiredLevel(GymId::Violet)==66U);
  assert(GymSystem::unlockLevel(GymId::Violet)==64U);
  assert(!GymSystem::challengeAvailable(gyms,63U));
  assert(GymSystem::challengeAvailable(gyms,64U));
  assert(!GymSystem::unlockWithStarter(gyms,155));
  assert(CollectionLogic::addRegionalStarter(leagueCollection,152,99));
  const OwnedPokemon* johtoStarter=nullptr;
  for(const auto& pokemon:leagueCollection.box)if(pokemon.uid&&pokemon.speciesId==152){johtoStarter=&pokemon;break;}
  assert(johtoStarter&&johtoStarter->level==5); // Regional starters still require training.
  PokemonCollection gymCalibrationCollection;CollectionLogic::initialize(gymCalibrationCollection);
  assert(CollectionLogic::chooseStarter(gymCalibrationCollection,1));
  OwnedPokemon* gymCalibrationLead=CollectionLogic::active(gymCalibrationCollection,0);
  *gymCalibrationLead=CollectionLogic::createPokemon(gymCalibrationLead->uid,1,66,false,123);
  BattleState johtoGymBattle;
  assert(GymSystem::start(johtoGymBattle,gymCalibrationCollection,
      gymCalibrationCollection.party[0],gyms,GymId::Violet,123));
  assert(johtoGymBattle.opponents[0].level==63&&johtoGymBattle.opponents[1].level==64);
  johtoGymBattle.active=false;johtoGymBattle.outcome=BattleOutcome::Victory;
  assert(GymSystem::advance(johtoGymBattle,gymCalibrationCollection));
  assert(johtoGymBattle.opponents[0].level==64&&johtoGymBattle.opponents[1].level==65);
  johtoGymBattle.active=false;johtoGymBattle.outcome=BattleOutcome::Victory;
  assert(GymSystem::advance(johtoGymBattle,gymCalibrationCollection));
  assert(johtoGymBattle.opponents[0].level==64&&johtoGymBattle.opponents[2].level==66);
  const GymDefinition* falkner=GymSystem::definition(GymId::Violet);
  const GymDefinition* clair=GymSystem::definition(GymId::Blackthorn);
  const GymDefinition* juan=GymSystem::definition(GymId::Sootopolis);
  assert(falkner&&falkner->team[0].level==64&&falkner->team[2].level==66);
  assert(clair&&clair->teamSize==3&&clair->team[2].speciesId==230);
  assert(clair->team[2].level==80);
  assert(juan&&juan->teamSize==3&&juan->team[2].level==96);
  assert(LeagueSystem::definition(2,0)&&LeagueSystem::definition(2,4)->team[2].speciesId==149);
  assert(LeagueSystem::definition(3,0)&&LeagueSystem::definition(3,4)->team[2].speciesId==350);
  assert(LeagueSystem::minimumLevel(1)==52&&LeagueSystem::maximumLevel(1)==63);
  assert(LeagueSystem::minimumLevel(2)==80&&LeagueSystem::maximumLevel(2)==86);
  assert(LeagueSystem::minimumLevel(3)==96&&LeagueSystem::maximumLevel(3)==100);

  // Level-up queues evolution before mutating the Pokemon, so the cinematic
  // may still be canceled. Committing the chosen destination preserves the
  // same Nature, IVs, personality and shiny identity while recalculating HP
  // and the evolved species' Ability.
  PokemonCollection evolutionCollection; CollectionLogic::initialize(evolutionCollection);
  assert(CollectionLogic::chooseStarter(evolutionCollection, 1, 4567));
  OwnedPokemon* evolving = CollectionLogic::active(evolutionCollection, 0);
  const uint32_t evolvingUid = evolving->uid;
  *evolving = CollectionLogic::createPokemon(evolvingUid, 1, 15, true, 0xCAFE1234U);
  evolving->moves[0]=MoveId::Tackle;evolving->movePp[0]=findFullMove(MoveId::Tackle)->pp;
  evolving->experience = experienceForLevel(findSpecies(1)->growthRate, 16) - 1U;
  const uint32_t preservedPersonality = evolving->personality;
  const PokemonNature preservedNature = evolving->nature;
  const IndividualValues preservedIvs = evolving->ivs;
  BattleState evolutionBattle; evolutionBattle.active=true; evolutionBattle.kind=BattleKind::Trainer;
  evolutionBattle.outcome=BattleOutcome::Ongoing; evolutionBattle.playerUid=evolvingUid;
  evolutionBattle.opponentCount=1; evolutionBattle.opponents[0]=CollectionLogic::createPokemon(0,10,2,false,88);
  evolutionBattle.opponents[0].currentHp=1;
  for(uint8_t i=0;i<kMoveSlots;++i)evolutionBattle.opponents[0].moves[i]=MoveId::None;
  const BattleActionResult evolutionResult=BattleEngine::fight(evolutionBattle,evolutionCollection,0);
  evolving=CollectionLogic::active(evolutionCollection,0);
  assert(!evolutionResult.evolved&&evolving->speciesId==1&&evolving->shiny);
  assert(evolutionResult.pendingEvolutionCount==1&&
         evolutionResult.pendingEvolutionUids[0]==evolvingUid&&
         evolutionResult.pendingEvolutionChoiceCounts[0]==1&&
         evolutionResult.pendingEvolutionChoices[0][0]==2);
  assert(evolutionResult.levelUpCount==1);
  const LevelUpSummary& levelSummary=evolutionResult.levelUps[0];
  assert(levelSummary.pokemonUid==evolvingUid&&levelSummary.speciesId==1&&levelSummary.newLevel==16);
  assert(levelSummary.gains[0]>0&&levelSummary.stats[0]>levelSummary.gains[0]);
  for(uint8_t stat=0;stat<6;++stat)assert(levelSummary.stats[stat]>0);
  int levelXpEvent=-1,levelEvent=-1;
  for(uint8_t eventIndex=0;eventIndex<evolutionResult.eventCount;++eventIndex){
    if(evolutionResult.events[eventIndex].type==BattleEventType::ExperienceGained&&levelXpEvent<0)levelXpEvent=eventIndex;
    if(evolutionResult.events[eventIndex].type==BattleEventType::LevelUp&&levelEvent<0)levelEvent=eventIndex;
  }
  assert(levelXpEvent>=0&&levelEvent>levelXpEvent);
  // Leaving the queued evolution unresolved models a canceled cinematic.
  // The unchanged species must become eligible again on the next level-up.
  evolving->experience=experienceForLevel(findSpecies(1)->growthRate,17)-1U;
  BattleState retryEvolutionBattle;retryEvolutionBattle.active=true;
  retryEvolutionBattle.kind=BattleKind::Trainer;
  retryEvolutionBattle.outcome=BattleOutcome::Ongoing;
  retryEvolutionBattle.playerUid=evolvingUid;retryEvolutionBattle.opponentCount=1;
  retryEvolutionBattle.opponents[0]=CollectionLogic::createPokemon(0,10,2,false,89);
  retryEvolutionBattle.opponents[0].currentHp=1;
  for(uint8_t i=0;i<kMoveSlots;++i)retryEvolutionBattle.opponents[0].moves[i]=MoveId::None;
  const BattleActionResult retryEvolutionResult=
      BattleEngine::fight(retryEvolutionBattle,evolutionCollection,0);
  assert(evolving->level==17&&evolving->speciesId==1&&
         retryEvolutionResult.pendingEvolutionCount==1&&
         retryEvolutionResult.pendingEvolutionChoices[0][0]==2);
  assert(CollectionLogic::evolve(evolutionCollection,evolvingUid,2));
  evolving=CollectionLogic::active(evolutionCollection,0);
  assert(evolving->personality==preservedPersonality&&evolving->nature==preservedNature);
  assert(std::memcmp(&evolving->ivs,&preservedIvs,sizeof(IndividualValues))==0);
  assert(evolving->maximumHp==CollectionLogic::calculatedMaximumHp(*evolving));
  const SpeciesData* evolvedSpecies=findSpecies(evolving->speciesId);
  assert(evolving->abilityId==(evolvedSpecies->ability2&&(evolving->personality&1U)
      ?evolvedSpecies->ability2:evolvedSpecies->ability1));

  // A branching evolution must stop combat progression with a persistent
  // selection, rather than silently choosing the first generated destination.
  PokemonCollection branchCollection; CollectionLogic::initialize(branchCollection);
  assert(CollectionLogic::chooseStarter(branchCollection,1,913));
  OwnedPokemon* eevee=CollectionLogic::active(branchCollection,0); const uint32_t eeveeUid=eevee->uid;
  *eevee=CollectionLogic::createPokemon(eeveeUid,133,35,false,0xE3E3U);
  eevee->moves[0]=MoveId::Tackle;eevee->movePp[0]=findFullMove(MoveId::Tackle)->pp;
  eevee->experience=experienceForLevel(findSpecies(133)->growthRate,36)-1U;
  BattleState branchBattle;branchBattle.active=true;branchBattle.kind=BattleKind::Trainer;branchBattle.outcome=BattleOutcome::Ongoing;
  branchBattle.playerUid=eeveeUid;branchBattle.unlockedGeneration=1;branchBattle.opponentCount=1;
  branchBattle.opponents[0]=CollectionLogic::createPokemon(0,10,2,false,99);branchBattle.opponents[0].currentHp=1;
  for(uint8_t i=0;i<kMoveSlots;++i)branchBattle.opponents[0].moves[i]=MoveId::None;
  const BattleActionResult branchResult=BattleEngine::fight(branchBattle,branchCollection,0);
  eevee=CollectionLogic::active(branchCollection,0);
  assert(branchResult.pendingEvolutionCount==1&&branchResult.pendingEvolutionUids[0]==eeveeUid);
  assert(branchResult.pendingEvolutionChoiceCounts[0]==3&&eevee->speciesId==133);

  // Nincada follows the original dual result only after the queued evolution
  // is committed: it becomes Ninjask and a same-identity Shedinja is added to
  // the Box with its fixed one HP.
  PokemonCollection nincadaCollection;CollectionLogic::initialize(nincadaCollection);
  assert(CollectionLogic::chooseStarter(nincadaCollection,1,0x290U));
  OwnedPokemon* nincada=CollectionLogic::active(nincadaCollection,0);
  const uint32_t nincadaUid=nincada->uid;
  *nincada=CollectionLogic::createPokemon(nincadaUid,290,19,true,0x292U);
  nincada->moves[0]=MoveId::Tackle;nincada->movePp[0]=findFullMove(MoveId::Tackle)->pp;
  nincada->experience=experienceForLevel(findSpecies(290)->growthRate,20)-1U;
  const uint32_t nincadaPersonality=nincada->personality;
  BattleState nincadaBattle;nincadaBattle.active=true;nincadaBattle.kind=BattleKind::Trainer;
  nincadaBattle.outcome=BattleOutcome::Ongoing;nincadaBattle.playerUid=nincadaUid;
  nincadaBattle.unlockedGeneration=3;nincadaBattle.opponentCount=1;
  nincadaBattle.opponents[0]=CollectionLogic::createPokemon(0,10,2,false,0x293U);
  nincadaBattle.opponents[0].currentHp=1;
  for(uint8_t i=0;i<kMoveSlots;++i)nincadaBattle.opponents[0].moves[i]=MoveId::None;
  const BattleActionResult nincadaResult=BattleEngine::fight(nincadaBattle,nincadaCollection,0);
  nincada=CollectionLogic::active(nincadaCollection,0);
  assert(nincadaResult.pendingEvolutionCount==1&&nincada->speciesId==290);
  uint32_t shedinjaUid=0;
  assert(CollectionLogic::evolve(nincadaCollection,nincadaUid,291,&shedinjaUid));
  nincada=CollectionLogic::active(nincadaCollection,0);
  const OwnedPokemon* shedinja=nullptr;
  for(const OwnedPokemon& candidate:nincadaCollection.box)
    if(candidate.uid!=kEmptyPokemonUid&&candidate.speciesId==292){shedinja=&candidate;break;}
  assert(nincada->speciesId==291&&shedinja&&shedinjaUid==shedinja->uid);
  assert(shedinja->currentHp==1&&shedinja->maximumHp==1&&shedinja->shiny&&
         shedinja->personality==nincadaPersonality&&shedinja->abilityId==25);

  PokemonCollection gymCollection; CollectionLogic::initialize(gymCollection);
  assert(CollectionLogic::chooseStarter(gymCollection,7)); BattleState gymBattle;
  GymProgress freshGyms;
  // The battle entry point enforces the same gate, so a stale beta invitation
  // cannot start Brock below his deterministic Lv14 requirement.
  assert(!GymSystem::start(gymBattle,gymCollection,gymCollection.party[0],
                           freshGyms,GymId::Pewter,44));
  OwnedPokemon* gymLead=CollectionLogic::active(gymCollection,0);
  *gymLead=CollectionLogic::createPokemon(gymLead->uid,7,14,false,44);
  assert(GymSystem::start(gymBattle,gymCollection,gymCollection.party[0],freshGyms,GymId::Pewter,44));
  assert(gymBattle.gymStage==0&&gymBattle.opponentItemUses==0);
  gymBattle.active=false;gymBattle.outcome=BattleOutcome::Victory;assert(GymSystem::advance(gymBattle,gymCollection));
  assert(gymBattle.gymStage==1&&gymBattle.active);
  gymBattle.active=false;gymBattle.outcome=BattleOutcome::Victory;assert(GymSystem::advance(gymBattle,gymCollection));
  assert(gymBattle.gymStage==2&&gymBattle.opponentItemUses==2&&gymBattle.opponentCount==3);
  assert(!GymSystem::advance(gymBattle,gymCollection));

  PokemonCollection learnCollection;CollectionLogic::initialize(learnCollection);assert(CollectionLogic::chooseStarter(learnCollection,4));
  OwnedPokemon* learner=CollectionLogic::active(learnCollection,0);const uint32_t learnerUid=learner->uid;
  *learner=CollectionLogic::createPokemon(learnerUid,4,18,true);learner->experience=experienceForLevel(findSpecies(4)->growthRate,19)-1;
  BattleState learnBattle;learnBattle.active=true;learnBattle.kind=BattleKind::Trainer;learnBattle.outcome=BattleOutcome::Ongoing;
  learnBattle.playerUid=learnerUid;learnBattle.opponentCount=1;learnBattle.opponents[0]=CollectionLogic::createPokemon(0,150,100);
  learnBattle.opponents[0].currentHp=1;for(uint8_t i=0;i<kMoveSlots;++i)learnBattle.opponents[0].moves[i]=MoveId::None;
  const BattleActionResult learnResult=BattleEngine::fight(learnBattle,learnCollection,0);
  assert(learnResult.movesToLearnCount>=1&&learnResult.moveLearnerUids[0]==learnerUid);
  assert(CollectionLogic::active(learnCollection,0)->shiny);
  assert(learnResult.movesToLearn[0]==moveLearnedAtLevel(4,19));

  // A multiplayer trade is an in-place replacement: all individual data is
  // retained, the remote UID is never trusted, and an active party position
  // remains occupied by the received Pokemon.
  PokemonCollection tradeCollection; CollectionLogic::initialize(tradeCollection);
  assert(CollectionLogic::chooseStarter(tradeCollection, 1, 123));
  const uint32_t outgoingUid = tradeCollection.party[0];
  OwnedPokemon incoming = CollectionLogic::createPokemon(999999, 25, 31, true, 0xBEEFU);
  incoming.friendship = 177; incoming.heldItem = HeldItem::Leftovers;
  incoming.evs.speed = 42; incoming.currentHp = incoming.maximumHp / 2U;
  uint32_t receivedUid = 0;
  assert(CollectionLogic::tradeReplace(tradeCollection, outgoingUid, incoming, &receivedUid));
  assert(receivedUid != 0 && receivedUid != 999999 && tradeCollection.party[0] == receivedUid);
  const OwnedPokemon* received = CollectionLogic::find(tradeCollection, receivedUid);
  assert(received && received->speciesId == 25 && received->level == 31 && received->shiny);
  assert(received->personality == incoming.personality && received->friendship == 177);
  assert(received->heldItem == HeldItem::Leftovers && received->evs.speed == 42);
  assert(CollectionLogic::validate(tradeCollection));
  incoming.heldItem=HeldItem::MegaStone;
  assert(!CollectionLogic::tradeReplace(tradeCollection,receivedUid,incoming,nullptr));
  OwnedPokemon* tradeHolder=CollectionLogic::find(tradeCollection,receivedUid);
  assert(tradeHolder);tradeHolder->heldItem=HeldItem::MegaStone;
  incoming.heldItem=HeldItem::None;
  assert(!CollectionLogic::tradeReplace(tradeCollection,receivedUid,incoming,nullptr));

  // FireRed stores Future Sight's base damage at setup, waits two complete
  // turns, and only then rolls accuracy/random damage. Gen III deliberately
  // skips type calculation, so a Dark target is still damaged. A second
  // future attack aimed at the occupied side must fail without resetting the
  // original timer.
  bool futureSightLanded = false;
  for (uint32_t seed = 1; seed <= 64 && !futureSightLanded; ++seed) {
    PokemonCollection futureCollection; CollectionLogic::initialize(futureCollection);
    assert(CollectionLogic::chooseStarter(futureCollection, 1, seed));
    OwnedPokemon* alakazam = CollectionLogic::active(futureCollection, 0);
    const uint32_t alakazamUid = alakazam->uid;
    *alakazam = CollectionLogic::createPokemon(alakazamUid, 65, 50, false, seed + 100U);
    const MoveId futureSight = static_cast<MoveId>(248);
    const MoveId growlMove = static_cast<MoveId>(45);
    alakazam->moves[0] = futureSight; alakazam->movePp[0] = 15;
    alakazam->moves[1] = growlMove; alakazam->movePp[1] = 40;
    BattleState futureBattle; futureBattle.active = true; futureBattle.kind = BattleKind::Trainer;
    futureBattle.outcome = BattleOutcome::Ongoing; futureBattle.playerUid = alakazamUid;
    futureBattle.opponentCount = 1; futureBattle.rngState = seed;
    futureBattle.opponents[0] = CollectionLogic::createPokemon(0xF2000001U, 197, 50, false, seed + 200U);
    for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
      futureBattle.opponents[0].moves[slot] = MoveId::None;
      futureBattle.opponents[0].movePp[slot] = 0;
    }
    const uint16_t hpBefore = futureBattle.opponents[0].currentHp;
    const BattleActionResult setup = BattleEngine::fight(futureBattle, futureCollection, 0);
    assert(setup.accepted && futureBattle.opponents[0].currentHp == hpBefore);
    assert(futureBattle.delayedToOpponent.move == futureSight && futureBattle.delayedToOpponent.dueTurn == 3);
    const BattleActionResult duplicate = BattleEngine::fight(futureBattle, futureCollection, 0);
    assert(duplicate.accepted && futureBattle.opponents[0].currentHp == hpBefore);
    assert(futureBattle.delayedToOpponent.move == futureSight && futureBattle.delayedToOpponent.dueTurn == 3);
    bool duplicateFailed = false;
    for (uint8_t i = 0; i < duplicate.eventCount; ++i)
      duplicateFailed |= duplicate.events[i].type == BattleEventType::MoveEffect &&
          duplicate.events[i].value == static_cast<uint16_t>(BattleMoveEffect::Failed);
    assert(duplicateFailed);
    const BattleActionResult impact = BattleEngine::fight(futureBattle, futureCollection, 1);
    bool sawDelayedHit = false;
    for (uint8_t i = 0; i < impact.eventCount; ++i)
      sawDelayedHit |= impact.events[i].type == BattleEventType::DelayedAttackHit &&
          impact.events[i].move == futureSight;
    assert(sawDelayedHit && futureBattle.delayedToOpponent.move == MoveId::None);
    futureSightLanded = futureBattle.opponents[0].currentHp < hpBefore;
  }
  assert(futureSightLanded);

  // PvP consumes the exact move chosen by the remote peer and never awards
  // XP/EV/money. A peer switch is represented as the opponent's whole turn,
  // so the resolver must not silently let the trainer AI act afterwards.
  PokemonCollection pvpCollection; CollectionLogic::initialize(pvpCollection);
  assert(CollectionLogic::chooseStarter(pvpCollection, 1, 321));
  OwnedPokemon* pvpPlayer = CollectionLogic::active(pvpCollection, 0);
  BattleState pvpBattle; pvpBattle.active = true; pvpBattle.kind = BattleKind::Pvp;
  pvpBattle.outcome = BattleOutcome::Ongoing; pvpBattle.playerUid = pvpPlayer->uid;
  pvpBattle.opponentCount = 2; pvpBattle.rngState = 0x12345678U;
  pvpBattle.opponents[0] = CollectionLogic::createPokemon(0xF1000001U, 4, 5, false, 17);
  pvpBattle.opponents[1] = CollectionLogic::createPokemon(0xF1000002U, 7, 5, false, 18);
  pvpBattle.opponents[0].moves[0] = MoveId::Tackle;
  pvpBattle.opponents[0].movePp[0] = findFullMove(MoveId::Tackle)->pp;
  const MoveId growl = static_cast<MoveId>(45);
  pvpBattle.opponents[0].moves[1] = growl;
  pvpBattle.opponents[0].movePp[1] = findFullMove(growl)->pp;
  const uint32_t pvpExperienceBefore = pvpPlayer->experience;
  const BattleActionResult pvpTurn = BattleEngine::fightPvp(pvpBattle, pvpCollection, 0, 1);
  assert(pvpTurn.accepted && pvpTurn.enemyActed);
  assert(pvpBattle.opponents[0].movePp[1] == findFullMove(growl)->pp - 1U);
  assert(pvpPlayer->experience == pvpExperienceBefore && pvpTurn.experienceGained == 0 && pvpTurn.moneyGained == 0);
  const uint8_t oldOpponentIndex = pvpBattle.opponentIndex;
  const uint32_t incomingOpponentUid = pvpBattle.opponents[1].uid;
  const BattleActionResult peerSwitch = BattleEngine::switchOpponentPvp(pvpBattle, pvpCollection, 1);
  assert(peerSwitch.accepted && pvpBattle.opponentIndex == oldOpponentIndex);
  assert(BattleEngine::currentOpponent(pvpBattle)->uid == incomingOpponentUid);
  const BattleActionResult pvpForfeit = BattleEngine::forfeitPvp(pvpBattle, pvpCollection, false);
  assert(pvpForfeit.accepted && !pvpBattle.active && pvpForfeit.outcome == BattleOutcome::Victory);

  // SHELL BELL recovery is a visible journal transition, not merely a final
  // HP mutation.  Both local and remote users must announce the item first
  // and then supply the exact rising-bar event for the same battler.
  const auto shellBellHasAnimatedHeal = [](const BattleActionResult& result,
                                            BattleSide side,uint32_t uid) {
    int activation=-1,heal=-1;
    for(uint8_t index=0;index<result.eventCount;++index){
      const BattleEvent& event=result.events[index];
      if(activation<0&&event.type==BattleEventType::HeldItemActivated&&
         event.side==side&&event.pokemonUid==uid&&
         event.value==static_cast<uint16_t>(HeldItem::ShellBell))activation=index;
      if(event.type==BattleEventType::HpChanged&&event.side==side&&
         event.pokemonUid==uid&&event.after>event.before)heal=index;
    }
    return activation>=0&&heal>activation;
  };
  const MoveId swiftMove=static_cast<MoveId>(129);
  const MoveId splashMove=static_cast<MoveId>(150);
  PokemonCollection playerBellCollection;CollectionLogic::initialize(playerBellCollection);
  assert(CollectionLogic::chooseStarter(playerBellCollection,1,0x5100U));
  OwnedPokemon* playerBell=CollectionLogic::active(playerBellCollection,0);
  const uint32_t playerBellUid=playerBell->uid;
  *playerBell=CollectionLogic::createPokemon(playerBellUid,143,50,false,0x5101U);
  playerBell->moves[0]=swiftMove;playerBell->movePp[0]=findFullMove(swiftMove)->pp;
  playerBell->moves[1]=splashMove;playerBell->movePp[1]=findFullMove(splashMove)->pp;
  playerBell->heldItem=HeldItem::ShellBell;
  playerBell->currentHp=static_cast<uint16_t>(playerBell->maximumHp/2U);
  BattleState playerBellBattle;BattleEngine::clear(playerBellBattle);
  playerBellBattle.active=true;playerBellBattle.kind=BattleKind::Pvp;
  playerBellBattle.outcome=BattleOutcome::Ongoing;
  playerBellBattle.playerUid=playerBellUid;playerBellBattle.opponentCount=1;
  playerBellBattle.rngState=0x5102U;
  playerBellBattle.opponents[0]=CollectionLogic::createPokemon(
      0xF5100001U,143,50,false,0x5103U);
  playerBellBattle.opponents[0].moves[0]=splashMove;
  playerBellBattle.opponents[0].movePp[0]=findFullMove(splashMove)->pp;
  const BattleActionResult playerBellResult=BattleEngine::fightPvp(
      playerBellBattle,playerBellCollection,0,0);
  assert(playerBellResult.accepted&&
         shellBellHasAnimatedHeal(playerBellResult,BattleSide::Player,playerBellUid));

  PokemonCollection enemyBellCollection;CollectionLogic::initialize(enemyBellCollection);
  assert(CollectionLogic::chooseStarter(enemyBellCollection,1,0x5200U));
  OwnedPokemon* enemyBellTarget=CollectionLogic::active(enemyBellCollection,0);
  enemyBellTarget->moves[0]=splashMove;
  enemyBellTarget->movePp[0]=findFullMove(splashMove)->pp;
  BattleState enemyBellBattle;BattleEngine::clear(enemyBellBattle);
  enemyBellBattle.active=true;enemyBellBattle.kind=BattleKind::Pvp;
  enemyBellBattle.outcome=BattleOutcome::Ongoing;
  enemyBellBattle.playerUid=enemyBellTarget->uid;enemyBellBattle.opponentCount=1;
  enemyBellBattle.rngState=0x5201U;
  enemyBellBattle.opponents[0]=CollectionLogic::createPokemon(
      0xF5200001U,143,50,false,0x5202U);
  OwnedPokemon& enemyBell=enemyBellBattle.opponents[0];
  enemyBell.moves[0]=swiftMove;enemyBell.movePp[0]=findFullMove(swiftMove)->pp;
  enemyBell.heldItem=HeldItem::ShellBell;
  enemyBell.currentHp=static_cast<uint16_t>(enemyBell.maximumHp/2U);
  const BattleActionResult enemyBellResult=BattleEngine::fightPvp(
      enemyBellBattle,enemyBellCollection,0,0);
  assert(enemyBellResult.accepted&&shellBellHasAnimatedHeal(
      enemyBellResult,BattleSide::Opponent,enemyBell.uid));

  // A PvP KO with another party member available must remain active and the
  // forced replacement must be a zero-turn transition. This is the engine
  // contract used by the BLE state publisher after the faint animation.
  PokemonCollection pvpFaintCollection; CollectionLogic::initialize(pvpFaintCollection);
  assert(CollectionLogic::chooseStarter(pvpFaintCollection, 1, 654));
  OwnedPokemon* pvpFaintPlayer = CollectionLogic::active(pvpFaintCollection, 0);
  uint32_t pvpReplacementUid = 0;
  assert(CollectionLogic::add(pvpFaintCollection,
      CollectionLogic::createPokemon(0, 16, 5, false, 655), &pvpReplacementUid));
  assert(CollectionLogic::setPartySlot(pvpFaintCollection, 1, pvpReplacementUid));
  const MoveId pvpGrowl = static_cast<MoveId>(45);
  pvpFaintPlayer->moves[0] = pvpGrowl;
  pvpFaintPlayer->movePp[0] = findFullMove(pvpGrowl)->pp;
  pvpFaintPlayer->currentHp = 1;
  BattleState pvpFaintBattle; pvpFaintBattle.active = true; pvpFaintBattle.kind = BattleKind::Pvp;
  pvpFaintBattle.outcome = BattleOutcome::Ongoing; pvpFaintBattle.playerUid = pvpFaintPlayer->uid;
  pvpFaintBattle.opponentCount = 1; pvpFaintBattle.rngState = 0x87654321U;
  pvpFaintBattle.opponents[0] = CollectionLogic::createPokemon(0xF1000001U, 4, 10, false, 656);
  pvpFaintBattle.opponents[0].moves[0] = MoveId::Tackle;
  pvpFaintBattle.opponents[0].movePp[0] = findFullMove(MoveId::Tackle)->pp;
  const BattleActionResult pvpFaintTurn = BattleEngine::fightPvp(
      pvpFaintBattle, pvpFaintCollection, 0, 0);
  uint8_t localFaintCount = 0;
  for (uint8_t eventIndex = 0; eventIndex < pvpFaintTurn.eventCount; ++eventIndex)
    if (pvpFaintTurn.events[eventIndex].type == BattleEventType::Fainted &&
        pvpFaintTurn.events[eventIndex].side == BattleSide::Player) ++localFaintCount;
  assert(pvpFaintTurn.accepted && localFaintCount == 1 && pvpFaintBattle.active &&
         CollectionLogic::find(pvpFaintCollection, pvpFaintBattle.playerUid)->currentHp == 0);
  const uint16_t faintTurnNumber = pvpFaintBattle.turn;
  const BattleActionResult pvpForcedSwitch = BattleEngine::switchToPokemonPvp(
      pvpFaintBattle, pvpFaintCollection, pvpReplacementUid, 0xFEU, true);
  assert(pvpForcedSwitch.accepted && pvpFaintBattle.active &&
         pvpFaintBattle.playerUid == pvpReplacementUid && pvpFaintBattle.turn == faintTurnNumber);

  // Defeating one member of the remote PvP roster must leave that fainted
  // member active until its owner explicitly chooses a replacement.
  OwnedPokemon* pvpReplacement = CollectionLogic::find(pvpFaintCollection, pvpReplacementUid);
  assert(pvpReplacement);
  pvpReplacement->moves[0] = MoveId::Tackle;
  pvpReplacement->movePp[0] = findFullMove(MoveId::Tackle)->pp;
  pvpFaintBattle.opponentCount = 2;
  pvpFaintBattle.opponents[0].currentHp = 1;
  pvpFaintBattle.opponents[1] = CollectionLogic::createPokemon(0xF1000002U, 7, 5, false, 657);
  const uint32_t faintedRemoteUid = pvpFaintBattle.opponents[0].uid;
  const uint16_t remoteFaintTurn = pvpFaintBattle.turn;
  const BattleActionResult pvpRemoteFaint = BattleEngine::fightPvp(
      pvpFaintBattle, pvpFaintCollection, 0, 0xFEU);
  bool prematurelySwitchedRemote = false;
  for (uint8_t eventIndex = 0; eventIndex < pvpRemoteFaint.eventCount; ++eventIndex)
    prematurelySwitchedRemote |= pvpRemoteFaint.events[eventIndex].type == BattleEventType::SwitchedIn &&
        pvpRemoteFaint.events[eventIndex].side == BattleSide::Opponent;
  assert(pvpRemoteFaint.accepted && pvpFaintBattle.active &&
         BattleEngine::currentOpponent(pvpFaintBattle)->uid == faintedRemoteUid &&
         BattleEngine::currentOpponent(pvpFaintBattle)->currentHp == 0 &&
         !prematurelySwitchedRemote);
  const BattleActionResult selectedRemote = BattleEngine::switchOpponentPvp(
      pvpFaintBattle, pvpFaintCollection, 1);
  assert(selectedRemote.accepted && pvpFaintBattle.active &&
         BattleEngine::currentOpponent(pvpFaintBattle)->uid == 0xF1000002U &&
         pvpFaintBattle.turn == remoteFaintTurn + 1U);

  // SNORE bypasses the ordinary sleep action lock, but only while the user
  // is still asleep. Waking up before execution makes the move fail normally.
  PokemonCollection snoreCollection; CollectionLogic::initialize(snoreCollection);
  assert(CollectionLogic::chooseStarter(snoreCollection, 1, 700));
  OwnedPokemon* snorer = CollectionLogic::active(snoreCollection, 0);
  const MoveId snoreMove = static_cast<MoveId>(173);
  snorer->moves[0] = snoreMove; snorer->movePp[0] = findFullMove(snoreMove)->pp;
  snorer->status = StatusCondition::Sleep;
  BattleState snoreBattle; snoreBattle.active = true; snoreBattle.kind = BattleKind::Trainer;
  snoreBattle.outcome = BattleOutcome::Ongoing; snoreBattle.playerUid = snorer->uid;
  snoreBattle.opponentCount = 1; snoreBattle.rngState = 0x700U;
  snoreBattle.opponents[0] = CollectionLogic::createPokemon(0xF1000001U, 19, 20, false, 701);
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) snoreBattle.opponents[0].movePp[slot] = 0;
  snoreBattle.playerVolatile.sleepTurns = 2;
  const uint16_t snoreTargetHp = snoreBattle.opponents[0].currentHp;
  const BattleActionResult sleepingSnore = BattleEngine::fight(snoreBattle, snoreCollection, 0);
  bool sleepBlockedSnore = false;
  for (uint8_t eventIndex = 0; eventIndex < sleepingSnore.eventCount; ++eventIndex)
    sleepBlockedSnore |= sleepingSnore.events[eventIndex].type == BattleEventType::CannotMove &&
        sleepingSnore.events[eventIndex].value == 6;
  assert(sleepingSnore.accepted && !sleepBlockedSnore &&
         snoreBattle.opponents[0].currentHp < snoreTargetHp &&
         snorer->status == StatusCondition::Sleep && snoreBattle.playerVolatile.sleepTurns == 1);

  snorer->status = StatusCondition::None;
  snorer->currentHp = snorer->maximumHp;
  snoreBattle.opponents[0].currentHp = snoreTargetHp;
  const BattleActionResult awakeSnore = BattleEngine::fight(snoreBattle, snoreCollection, 0);
  bool awakeSnoreFailed = false;
  for (uint8_t eventIndex = 0; eventIndex < awakeSnore.eventCount; ++eventIndex)
    awakeSnoreFailed |= awakeSnore.events[eventIndex].type == BattleEventType::MoveEffect &&
        awakeSnore.events[eventIndex].value == static_cast<uint16_t>(BattleMoveEffect::Failed);
  assert(awakeSnore.accepted && awakeSnoreFailed &&
         snoreBattle.opponents[0].currentHp == snoreTargetHp);

  // ROLE PLAY follows FireRed's trycopyability command: any present Ability
  // except Wonder Guard can be copied (including an identical Ability). The
  // copied Ability is temporary and must be restored when the user switches.
  PokemonCollection roleCollection; CollectionLogic::initialize(roleCollection);
  assert(CollectionLogic::chooseStarter(roleCollection, 1, 800));
  OwnedPokemon* rolePlayer = CollectionLogic::active(roleCollection, 0);
  const uint32_t rolePlayerUid = rolePlayer->uid;
  uint32_t roleReplacementUid = 0;
  assert(CollectionLogic::add(roleCollection,
      CollectionLogic::createPokemon(0, 16, 20, false, 801), &roleReplacementUid));
  assert(CollectionLogic::setPartySlot(roleCollection, 1, roleReplacementUid));
  const MoveId rolePlay = static_cast<MoveId>(272);
  rolePlayer->moves[0] = rolePlay;
  rolePlayer->movePp[0] = findFullMove(rolePlay)->pp;
  const uint8_t originalRoleAbility = rolePlayer->abilityId;
  BattleState roleBattle; roleBattle.active = true; roleBattle.kind = BattleKind::Trainer;
  roleBattle.outcome = BattleOutcome::Ongoing; roleBattle.playerUid = rolePlayerUid;
  roleBattle.opponentCount = 1; roleBattle.rngState = 0x800U;
  roleBattle.opponents[0] = CollectionLogic::createPokemon(0xF1000001U, 19, 20, false, 802);
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) roleBattle.opponents[0].movePp[slot] = 0;
  const uint8_t copiedAbility = roleBattle.opponents[0].abilityId;
  const BattleActionResult roleSuccess = BattleEngine::fight(roleBattle, roleCollection, 0);
  bool sawAbilityCopied = false, roleUnexpectedlyFailed = false;
  for (uint8_t eventIndex = 0; eventIndex < roleSuccess.eventCount; ++eventIndex) {
    const BattleEvent& event = roleSuccess.events[eventIndex];
    sawAbilityCopied |= event.type == BattleEventType::MoveEffect &&
        event.value == static_cast<uint16_t>(BattleMoveEffect::AbilityCopied) &&
        event.side == BattleSide::Player && event.after == copiedAbility;
    roleUnexpectedlyFailed |= event.type == BattleEventType::MoveEffect &&
        event.value == static_cast<uint16_t>(BattleMoveEffect::Failed);
  }
  assert(roleSuccess.accepted && sawAbilityCopied && !roleUnexpectedlyFailed &&
         rolePlayer->abilityId == copiedAbility && roleBattle.playerAbilityTraced &&
         roleBattle.playerPreTraceAbilityId == originalRoleAbility);
  const BattleActionResult roleSwitch = BattleEngine::switchToPokemon(
      roleBattle, roleCollection, roleReplacementUid, false);
  assert(roleSwitch.accepted &&
         CollectionLogic::find(roleCollection, rolePlayerUid)->abilityId == originalRoleAbility);

  // Wonder Guard is the one real Ability rejected by FireRed Role Play.
  PokemonCollection wonderCollection; CollectionLogic::initialize(wonderCollection);
  assert(CollectionLogic::chooseStarter(wonderCollection, 1, 810));
  OwnedPokemon* wonderPlayer = CollectionLogic::active(wonderCollection, 0);
  wonderPlayer->moves[0] = rolePlay; wonderPlayer->movePp[0] = findFullMove(rolePlay)->pp;
  const uint8_t wonderOriginalAbility = wonderPlayer->abilityId;
  BattleState wonderBattle; wonderBattle.active = true; wonderBattle.kind = BattleKind::Trainer;
  wonderBattle.outcome = BattleOutcome::Ongoing; wonderBattle.playerUid = wonderPlayer->uid;
  wonderBattle.opponentCount = 1; wonderBattle.rngState = 0x810U;
  wonderBattle.opponents[0] = CollectionLogic::createPokemon(0xF1000001U, 292, 20, false, 811);
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) wonderBattle.opponents[0].movePp[slot] = 0;
  const BattleActionResult wonderFailure = BattleEngine::fight(wonderBattle, wonderCollection, 0);
  bool sawWonderFailure = false;
  for (uint8_t eventIndex = 0; eventIndex < wonderFailure.eventCount; ++eventIndex)
    sawWonderFailure |= wonderFailure.events[eventIndex].type == BattleEventType::MoveEffect &&
        wonderFailure.events[eventIndex].value == static_cast<uint16_t>(BattleMoveEffect::Failed);
  assert(wonderFailure.accepted && sawWonderFailure &&
         wonderPlayer->abilityId == wonderOriginalAbility && !wonderBattle.playerAbilityTraced);

  // The same handler is required for trainer/PvP-controlled opponents.
  PokemonCollection enemyRoleCollection; CollectionLogic::initialize(enemyRoleCollection);
  assert(CollectionLogic::chooseStarter(enemyRoleCollection, 1, 820));
  OwnedPokemon* enemyRoleTarget = CollectionLogic::active(enemyRoleCollection, 0);
  const uint8_t enemyCopiedAbility = enemyRoleTarget->abilityId;
  enemyRoleTarget->moves[0] = static_cast<MoveId>(45);
  enemyRoleTarget->movePp[0] = findFullMove(enemyRoleTarget->moves[0])->pp;
  BattleState enemyRoleBattle; enemyRoleBattle.active = true; enemyRoleBattle.kind = BattleKind::Pvp;
  enemyRoleBattle.outcome = BattleOutcome::Ongoing; enemyRoleBattle.playerUid = enemyRoleTarget->uid;
  enemyRoleBattle.opponentCount = 1; enemyRoleBattle.rngState = 0x820U;
  enemyRoleBattle.opponents[0] = CollectionLogic::createPokemon(0xF1000001U, 19, 20, false, 821);
  enemyRoleBattle.opponents[0].moves[0] = rolePlay;
  enemyRoleBattle.opponents[0].movePp[0] = findFullMove(rolePlay)->pp;
  const BattleActionResult enemyRoleSuccess = BattleEngine::fightPvp(
      enemyRoleBattle, enemyRoleCollection, 0, 0);
  bool sawEnemyAbilityCopied = false;
  for (uint8_t eventIndex = 0; eventIndex < enemyRoleSuccess.eventCount; ++eventIndex) {
    const BattleEvent& event = enemyRoleSuccess.events[eventIndex];
    sawEnemyAbilityCopied |= event.type == BattleEventType::MoveEffect &&
        event.value == static_cast<uint16_t>(BattleMoveEffect::AbilityCopied) &&
        event.side == BattleSide::Opponent && event.after == enemyCopiedAbility;
  }
  assert(enemyRoleSuccess.accepted && sawEnemyAbilityCopied &&
         enemyRoleBattle.opponents[0].abilityId == enemyCopiedAbility);

  // ENCORE must replace the foe's selected command, not reject that command
  // after selection and silently spend its turn. FireRed stores 3..6 turns,
  // decrements at end of turn, and stops when the move is gone or out of PP.
  PokemonCollection encoreCollection;CollectionLogic::initialize(encoreCollection);
  assert(CollectionLogic::chooseStarter(encoreCollection,1,900));
  OwnedPokemon* encoreUser=CollectionLogic::active(encoreCollection,0);
  const MoveId encore=static_cast<MoveId>(227),encoreGrowl=static_cast<MoveId>(45);
  encoreUser->moves[0]=encore;encoreUser->movePp[0]=findFullMove(encore)->pp;
  encoreUser->moves[1]=encoreGrowl;encoreUser->movePp[1]=findFullMove(encoreGrowl)->pp;
  BattleState encoreBattle;BattleEngine::clear(encoreBattle);encoreBattle.active=true;
  encoreBattle.kind=BattleKind::Pvp;encoreBattle.outcome=BattleOutcome::Ongoing;
  encoreBattle.playerUid=encoreUser->uid;encoreBattle.opponentCount=1;encoreBattle.rngState=0x900U;
  encoreBattle.opponents[0]=CollectionLogic::createPokemon(0xF1000001U,19,20,false,901);
  encoreBattle.opponents[0].moves[0]=encoreGrowl;encoreBattle.opponents[0].movePp[0]=20;
  encoreBattle.opponents[0].moves[1]=MoveId::Tackle;encoreBattle.opponents[0].movePp[1]=20;
  encoreBattle.opponentVolatiles[0].lastMoveUsed=encoreGrowl;
  const BattleActionResult encoreApplied=BattleEngine::fightPvp(encoreBattle,encoreCollection,0,0xFEU);
  assert(encoreApplied.accepted&&encoreBattle.opponentVolatiles[0].encoreMove==encoreGrowl&&
         encoreBattle.opponentEncoreTurns[0]>=2&&encoreBattle.opponentEncoreTurns[0]<=5);
  const uint8_t growlPpBefore=encoreBattle.opponents[0].movePp[0];
  const uint8_t tacklePpBefore=encoreBattle.opponents[0].movePp[1];
  const BattleActionResult forcedEncore=BattleEngine::fightPvp(encoreBattle,encoreCollection,1,1);
  bool encoreLostTurn=false;
  for(uint8_t i=0;i<forcedEncore.eventCount;++i)
    encoreLostTurn|=forcedEncore.events[i].type==BattleEventType::CannotMove&&forcedEncore.events[i].value==5;
  assert(forcedEncore.accepted&&!encoreLostTurn&&forcedEncore.enemyMoveUsed==encoreGrowl&&
         encoreBattle.opponents[0].movePp[0]==growlPpBefore-1U&&
         encoreBattle.opponents[0].movePp[1]==tacklePpBefore);

  // BIDE spends one setup turn, stores real HP damage over the following two
  // locked turns, then releases exactly twice the total. Selecting another
  // move while locked must not cancel it or spend that move's PP.
  PokemonCollection bideCollection;CollectionLogic::initialize(bideCollection);
  assert(CollectionLogic::chooseStarter(bideCollection,1,910));
  OwnedPokemon* bideUser=CollectionLogic::active(bideCollection,0);
  const uint32_t bideUserUid=bideUser->uid;
  *bideUser=CollectionLogic::createPokemon(bideUserUid,150,80,false,910);
  const MoveId bide=static_cast<MoveId>(117);
  bideUser->moves[0]=bide;bideUser->movePp[0]=findFullMove(bide)->pp;
  bideUser->moves[1]=encoreGrowl;bideUser->movePp[1]=20;
  BattleState bideBattle;BattleEngine::clear(bideBattle);bideBattle.active=true;
  bideBattle.kind=BattleKind::Pvp;bideBattle.outcome=BattleOutcome::Ongoing;
  bideBattle.playerUid=bideUser->uid;bideBattle.opponentCount=1;bideBattle.rngState=0x910U;
  bideBattle.opponents[0]=CollectionLogic::createPokemon(0xF1000001U,143,50,false,911);
  bideBattle.opponents[0].moves[0]=MoveId::Tackle;bideBattle.opponents[0].movePp[0]=35;
  const uint8_t untouchedGrowlPp=bideUser->movePp[1];
  const BattleActionResult bideSetup=BattleEngine::fightPvp(bideBattle,bideCollection,0,0);
  assert(bideSetup.accepted&&bideBattle.playerBide.turns==2&&bideBattle.playerBide.damage>0);
  const BattleActionResult bideStore=BattleEngine::fightPvp(bideBattle,bideCollection,1,0);
  assert(bideStore.accepted&&bideBattle.playerBide.turns==1&&
         bideBattle.playerBide.damage>0&&bideUser->movePp[1]==untouchedGrowlPp);
  const uint16_t storedDamage=bideBattle.playerBide.damage;
  const uint16_t bideTargetBefore=bideBattle.opponents[0].currentHp;
  const BattleActionResult bideRelease=BattleEngine::fightPvp(bideBattle,bideCollection,1,0);
  assert(bideRelease.accepted&&bideBattle.playerBide.turns==0&&bideRelease.hit&&
         bideTargetBefore-bideBattle.opponents[0].currentHp==
             std::min<uint16_t>(bideTargetBefore,static_cast<uint16_t>(storedDamage*2U))&&
         bideUser->movePp[1]==untouchedGrowlPp);

  // The opponent path uses the same lock and release semantics even when a
  // PvP/AI caller proposes another move.
  BattleState enemyBideBattle;BattleEngine::clear(enemyBideBattle);enemyBideBattle.active=true;
  enemyBideBattle.kind=BattleKind::Pvp;enemyBideBattle.outcome=BattleOutcome::Ongoing;
  enemyBideBattle.playerUid=bideUser->uid;enemyBideBattle.opponentCount=1;enemyBideBattle.rngState=0x920U;
  enemyBideBattle.opponents[0]=CollectionLogic::createPokemon(0xF1000002U,150,80,false,921);
  enemyBideBattle.opponents[0].moves[0]=bide;enemyBideBattle.opponents[0].movePp[0]=10;
  enemyBideBattle.opponents[0].moves[1]=encoreGrowl;enemyBideBattle.opponents[0].movePp[1]=20;
  enemyBideBattle.opponentBides[0].turns=1;enemyBideBattle.opponentBides[0].damage=7;
  bideUser->currentHp=bideUser->maximumHp;
  const uint16_t enemyBideTargetBefore=bideUser->currentHp;
  const BattleActionResult enemyBideRelease=BattleEngine::fightPvp(enemyBideBattle,bideCollection,1,1);
  assert(enemyBideRelease.accepted&&enemyBideRelease.enemyMoveUsed==bide&&
         enemyBideTargetBefore-bideUser->currentHp==14U&&enemyBideBattle.opponentBides[0].turns==0);

  // The one-time post-Hoenn boss gates the Mega Stone and the repeatable
  // Battle Tower without enlarging GymProgress. Its team is fully authored:
  // shiny Ditto, Shedinja and female/therefore-X Mega Charizard.
  GymProgress megaProgress{};
  megaProgress.badgeBits=(uint32_t{1}<<static_cast<uint8_t>(GymId::Count))-1U;
  megaProgress.unlockedGeneration=3U;
  megaProgress.starterClaimedBits=0x07U;
  megaProgress.leagueChampionBits=0x07U;
  assert(MegaChallengeSystem::available(megaProgress));
  assert(!MegaChallengeSystem::completed(megaProgress));
  assert(!BattleTowerSystem::available(megaProgress));
  PokemonCollection megaCollection;CollectionLogic::initialize(megaCollection);
  assert(CollectionLogic::chooseStarter(megaCollection,1,924));
  OwnedPokemon* megaLead=CollectionLogic::active(megaCollection,0);
  *megaLead=CollectionLogic::createPokemon(megaLead->uid,1,100,false,924);
  BattleState megaBattle;
  assert(MegaChallengeSystem::start(megaBattle,megaCollection,megaLead->uid,
                                    megaProgress,0x4D454741U));
  assert(megaBattle.kind==BattleKind::MegaChallenge&&megaBattle.opponentCount==3U);
  assert(megaBattle.opponents[0].speciesId==132U&&megaBattle.opponents[0].level==100U&&
         megaBattle.opponents[0].shiny&&megaBattle.opponents[0].heldItem==HeldItem::MetalPowder&&
         megaBattle.opponents[0].moves[0]==static_cast<MoveId>(144)&&
         megaBattle.opponents[0].nature==PokemonNature::Relaxed&&
         megaBattle.opponents[0].evs.hp==252U&&megaBattle.opponents[0].evs.defense==252U&&
         megaBattle.opponents[0].evs.spDefense==4U);
  assert(megaBattle.opponents[1].speciesId==292U&&megaBattle.opponents[1].level==100U&&
         megaBattle.opponents[1].heldItem==HeldItem::LumBerry&&
         megaBattle.opponents[1].moves[0]==static_cast<MoveId>(14)&&
         megaBattle.opponents[1].moves[1]==static_cast<MoveId>(247)&&
         megaBattle.opponents[1].moves[2]==static_cast<MoveId>(318)&&
         megaBattle.opponents[1].moves[3]==static_cast<MoveId>(182)&&
         megaBattle.opponents[1].nature==PokemonNature::Lonely&&
         megaBattle.opponents[1].evs.attack==252U&&megaBattle.opponents[1].evs.speed==252U);
  assert(megaBattle.opponents[2].speciesId==6U&&megaBattle.opponents[2].level==100U&&
         megaBattle.opponents[2].heldItem==HeldItem::MegaStone&&
         megaBattle.opponents[2].moves[0]==static_cast<MoveId>(349)&&
         megaBattle.opponents[2].moves[1]==static_cast<MoveId>(337)&&
         megaBattle.opponents[2].moves[2]==static_cast<MoveId>(89)&&
         megaBattle.opponents[2].moves[3]==static_cast<MoveId>(126)&&
         megaBattle.opponents[2].nature==PokemonNature::Jolly&&
         megaBattle.opponents[2].evs.attack==252U&&megaBattle.opponents[2].evs.speed==252U&&
         MegaEvolution::variantFor(megaBattle.opponents[2])==MegaVariant::MegaX);
  const AbilityData* megaBossAbility=findAbility(megaBattle.opponents[2].abilityId);
  assert(megaBossAbility&&std::strcmp(megaBossAbility->name,"TOUGH CLAWS")==0);
  uint64_t megaOwnership=0;
  assert(MegaChallengeSystem::awardVictory(megaProgress,megaOwnership));
  assert(MegaChallengeSystem::completed(megaProgress)&&
         (megaOwnership&kMegaStoneOwnershipBit));
  assert(!MegaChallengeSystem::awardVictory(megaProgress,megaOwnership));
  assert(BattleTowerSystem::available(megaProgress));

  // Pre-challenge beta stones are removed from the Bag/Pokemon, while a
  // completed reward is repaired if neither representation owns it.
  GymProgress lockedMegaProgress=megaProgress;
  lockedMegaProgress.starterClaimedBits&=static_cast<uint8_t>(~MegaChallengeSystem::kCompletionBit);
  megaLead->heldItem=HeldItem::MegaStone;
  uint64_t staleMegaOwnership=kMegaStoneOwnershipBit;
  assert(MegaChallengeSystem::reconcileStoneGate(lockedMegaProgress,megaCollection,
                                                  staleMegaOwnership));
  assert(megaLead->heldItem==HeldItem::None&&!(staleMegaOwnership&kMegaStoneOwnershipBit));
  uint64_t repairedMegaOwnership=0;
  assert(MegaChallengeSystem::reconcileStoneGate(megaProgress,megaCollection,
                                                  repairedMegaOwnership));
  assert(repairedMegaOwnership&kMegaStoneOwnershipBit);

  // All 300 Emerald Battle Tower trainers and 882 authored sets become
  // permanently available after that boss. The facility starts a
  // three-opponent series without a timer, invitation roll, Center charge,
  // or VS Seeker charge;
  // every stage has three distinct species, original moves/items/nature/EVs,
  // and the level of the strongest selected partner.
  GymProgress towerProgress=megaProgress;
  assert(BattleTowerSystem::available(towerProgress));
  GymProgress oneBadgeShort=towerProgress;
  oneBadgeShort.badgeBits&=~(uint32_t{1}<<(static_cast<uint8_t>(GymId::Count)-1U));
  assert(!BattleTowerSystem::available(oneBadgeShort));
  Inventory towerRewardInventory{};
  assert(BattleTowerSystem::awardCompletionReward(towerRewardInventory));
  assert(towerRewardInventory.heldItems[kRareCandyInventorySlot]==1U);
  towerRewardInventory.heldItems[kRareCandyInventorySlot]=kInventoryStackLimit;
  assert(!BattleTowerSystem::awardCompletionReward(towerRewardInventory));
  // Completion has one reward slot: Rare Candy is the fallback and exactly
  // one hashed roll in 200 may replace it with a Special Egg.  Merely seeing
  // a starter does not remove it; only the caught/obtained Pokédex bit does.
  uint32_t specialRewardSeed=0;
  uint16_t specialRewardHits=0;
  for(uint32_t seed=1;seed<=20000U;++seed){
    Inventory rewardInventory{};EggState rewardEgg{};PokedexState rewardDex{};
    const BattleTowerRewardResult reward=BattleTowerSystem::awardCompletionReward(
        rewardInventory,rewardEgg,rewardDex,seed);
    if(reward.kind==BattleTowerRewardKind::SpecialEgg){
      ++specialRewardHits;
      if(!specialRewardSeed)specialRewardSeed=seed;
      assert(rewardEgg.active&&rewardEgg.rarity==EggRarity::Special&&
             rewardEgg.remainingSeconds==EggSystem::kRareSeconds&&
             rewardInventory.heldItems[kRareCandyInventorySlot]==0U);
    }else{
      assert(reward.kind==BattleTowerRewardKind::RareCandy&&
             rewardInventory.heldItems[kRareCandyInventorySlot]==1U&&!rewardEgg.active);
    }
  }
  assert(specialRewardSeed&&specialRewardHits>=70U&&specialRewardHits<=130U);
  constexpr uint16_t towerStarters[]={1,4,7,152,155,158,252,255,258};
  PokedexState oneStarterMissing{};
  for(uint16_t starterSpecies:towerStarters)
    if(starterSpecies!=258U)PokedexLogic::markCaught(oneStarterMissing,starterSpecies);
  PokedexLogic::markSeen(oneStarterMissing,258U);
  Inventory specialInventory{};EggState specialEgg{};
  const BattleTowerRewardResult starterReward=BattleTowerSystem::awardCompletionReward(
      specialInventory,specialEgg,oneStarterMissing,specialRewardSeed);
  assert(starterReward.kind==BattleTowerRewardKind::SpecialEgg&&
         starterReward.eggSpeciesId==258U&&specialEgg.speciesId==258U);
  PokedexLogic::markCaught(oneStarterMissing,258U);
  specialEgg=EggState{};
  const BattleTowerRewardResult legendaryReward=BattleTowerSystem::awardCompletionReward(
      specialInventory,specialEgg,oneStarterMissing,specialRewardSeed);
  const uint16_t legendarySpecies=legendaryReward.eggSpeciesId;
  assert(legendaryReward.kind==BattleTowerRewardKind::SpecialEgg&&
      (legendarySpecies==144U||legendarySpecies==145U||legendarySpecies==146U||
       legendarySpecies==150U||legendarySpecies==151U||legendarySpecies==243U||
       legendarySpecies==244U||legendarySpecies==245U||legendarySpecies==249U||
       legendarySpecies==250U||legendarySpecies==251U||legendarySpecies==377U||
       legendarySpecies==378U||legendarySpecies==379U||legendarySpecies==380U||
       legendarySpecies==381U||legendarySpecies==382U||legendarySpecies==383U||
       legendarySpecies==384U||legendarySpecies==385U||legendarySpecies==386U));
  EggState occupiedEgg{};occupiedEgg.active=true;occupiedEgg.speciesId=16U;
  Inventory occupiedRewardInventory{};
  const BattleTowerRewardResult occupiedReward=BattleTowerSystem::awardCompletionReward(
      occupiedRewardInventory,occupiedEgg,oneStarterMissing,specialRewardSeed);
  assert(occupiedReward.kind==BattleTowerRewardKind::RareCandy&&
         occupiedRewardInventory.heldItems[kRareCandyInventorySlot]==1U&&
         occupiedEgg.speciesId==16U);
  assert(BattleTowerSystem::trainer(0)&&BattleTowerSystem::trainer(299));
  assert(BattleTowerSystem::monTemplate(0)&&BattleTowerSystem::monTemplate(881));
  PokemonCollection towerCollection;CollectionLogic::initialize(towerCollection);
  assert(CollectionLogic::chooseStarter(towerCollection,1,925));
  OwnedPokemon* towerLead=CollectionLogic::active(towerCollection,0);
  *towerLead=CollectionLogic::createPokemon(towerLead->uid,1,73,false,925);
  BattleState towerBattle;
  assert(BattleTowerSystem::start(towerBattle,towerCollection,towerLead->uid,
                                  towerProgress,0xB4770001U));
  assert(towerBattle.kind==BattleKind::Tower&&towerBattle.gymStage==0U&&
         towerBattle.opponentCount==3U);
  for(uint8_t stage=0;stage<BattleTowerSystem::kStageCount;++stage){
    const BattleTowerTrainerDefinition* towerTrainer=BattleTowerSystem::trainer(towerBattle);
    assert(towerTrainer&&towerTrainer->poolCount>=3U);
    for(uint8_t slot=0;slot<3;++slot){
      const OwnedPokemon& towerMon=towerBattle.opponents[slot];
      assert(towerMon.speciesId&&towerMon.level==73U&&towerMon.moves[0]!=MoveId::None);
      assert(towerMon.nature<=PokemonNature::Quirky);
      for(uint8_t previous=0;previous<slot;++previous){
        assert(towerMon.speciesId!=towerBattle.opponents[previous].speciesId);
        if(towerMon.heldItem!=HeldItem::None)
          assert(towerMon.heldItem!=towerBattle.opponents[previous].heldItem);
      }
    }
    if(stage+1U<BattleTowerSystem::kStageCount){
      towerBattle.active=false;towerBattle.outcome=BattleOutcome::Victory;
      assert(BattleTowerSystem::advance(towerBattle,towerCollection));
      assert(towerBattle.gymStage==stage+1U&&towerBattle.active);
    }
  }
  towerBattle.active=false;towerBattle.outcome=BattleOutcome::Victory;
  assert(!BattleTowerSystem::advance(towerBattle,towerCollection));

  // Mind Reader/Lock-On is a user-target sure-hit relation, not merely +6
  // Accuracy. It must make the following OHKO move connect for either side
  // while retaining the FireRed rule that a lower-level user still fails.
  PokemonCollection lockCollection;CollectionLogic::initialize(lockCollection);
  assert(CollectionLogic::chooseStarter(lockCollection,1,930));
  OwnedPokemon* lockUser=CollectionLogic::active(lockCollection,0);
  *lockUser=CollectionLogic::createPokemon(lockUser->uid,144,80,false,930);
  lockUser->moves[0]=static_cast<MoveId>(170);lockUser->movePp[0]=5;
  lockUser->moves[1]=static_cast<MoveId>(329);lockUser->movePp[1]=5;
  BattleState lockBattle;BattleEngine::clear(lockBattle);lockBattle.active=true;
  lockBattle.kind=BattleKind::Pvp;lockBattle.outcome=BattleOutcome::Ongoing;
  lockBattle.playerUid=lockUser->uid;lockBattle.opponentCount=1;lockBattle.rngState=1;
  lockBattle.opponents[0]=CollectionLogic::createPokemon(0xF2000001U,143,80,false,931);
  lockBattle.opponents[0].moves[0]=MoveId::Tackle;lockBattle.opponents[0].movePp[0]=35;
  const BattleActionResult reader=BattleEngine::fightPvp(lockBattle,lockCollection,0,0);
  bool readerSet=false,readerFailed=false;
  for(uint8_t eventIndex=0;eventIndex<reader.eventCount;++eventIndex){
    const BattleEvent& event=reader.events[eventIndex];
    if(event.type!=BattleEventType::MoveEffect||event.side!=BattleSide::Player||
       event.move!=static_cast<MoveId>(170)) continue;
    readerSet|=event.value==static_cast<uint16_t>(BattleMoveEffect::SureHitSet);
    readerFailed|=event.value==static_cast<uint16_t>(BattleMoveEffect::Failed);
  }
  assert(reader.accepted&&readerSet&&!readerFailed&&
         lockBattle.opponentVolatiles[0].sureHitTurns==1U);
  const BattleActionResult cold=BattleEngine::fightPvp(lockBattle,lockCollection,1,0);
  assert(cold.accepted&&cold.hit&&lockBattle.opponents[0].currentHp==0U);

  // Exercise the same relation from the opponent side. The previous test's
  // comment promised both sides but only ever let the player use the combo,
  // so an asymmetric regression could still ship to trainer/PvP AI.
  PokemonCollection enemyLockCollection;CollectionLogic::initialize(enemyLockCollection);
  assert(CollectionLogic::chooseStarter(enemyLockCollection,1,936));
  OwnedPokemon* enemyLockTarget=CollectionLogic::active(enemyLockCollection,0);
  *enemyLockTarget=CollectionLogic::createPokemon(enemyLockTarget->uid,143,80,false,936);
  enemyLockTarget->moves[0]=static_cast<MoveId>(150);enemyLockTarget->movePp[0]=40;
  BattleState enemyLockBattle;BattleEngine::clear(enemyLockBattle);
  enemyLockBattle.active=true;enemyLockBattle.kind=BattleKind::Pvp;
  enemyLockBattle.outcome=BattleOutcome::Ongoing;
  enemyLockBattle.playerUid=enemyLockTarget->uid;enemyLockBattle.opponentCount=1;
  enemyLockBattle.rngState=937;
  enemyLockBattle.opponents[0]=CollectionLogic::createPokemon(
      0xF2000005U,144,80,false,937);
  enemyLockBattle.opponents[0].moves[0]=static_cast<MoveId>(170);
  enemyLockBattle.opponents[0].movePp[0]=5;
  enemyLockBattle.opponents[0].moves[1]=static_cast<MoveId>(329);
  enemyLockBattle.opponents[0].movePp[1]=5;
  const BattleActionResult enemyReader=BattleEngine::fightPvp(
      enemyLockBattle,enemyLockCollection,0,0);
  bool enemyReaderSet=false,enemyReaderFailed=false;
  for(uint8_t eventIndex=0;eventIndex<enemyReader.eventCount;++eventIndex){
    const BattleEvent& event=enemyReader.events[eventIndex];
    if(event.type!=BattleEventType::MoveEffect||event.side!=BattleSide::Opponent||
       event.move!=static_cast<MoveId>(170)) continue;
    enemyReaderSet|=event.value==static_cast<uint16_t>(BattleMoveEffect::SureHitSet);
    enemyReaderFailed|=event.value==static_cast<uint16_t>(BattleMoveEffect::Failed);
  }
  assert(enemyReader.accepted&&enemyReaderSet&&!enemyReaderFailed&&
         (enemyLockBattle.playerVolatile.sureHitTurns&0x03U)==2U);
  const BattleActionResult enemyCold=BattleEngine::fightPvp(
      enemyLockBattle,enemyLockCollection,0,1);
  assert(enemyCold.accepted&&enemyLockTarget->currentHp==0U);

  // Mind Reader guarantees accuracy, not the independent Gen-III OHKO level
  // rule. A lower-level Articuno must report a failed effect rather than an
  // ordinary accuracy miss, which makes the restriction clear on hardware.
  *lockUser=CollectionLogic::createPokemon(lockUser->uid,144,79,false,938);
  lockUser->moves[0]=static_cast<MoveId>(170);lockUser->movePp[0]=5;
  lockUser->moves[1]=static_cast<MoveId>(329);lockUser->movePp[1]=5;
  BattleEngine::clear(lockBattle);lockBattle.active=true;lockBattle.kind=BattleKind::Pvp;
  lockBattle.outcome=BattleOutcome::Ongoing;lockBattle.playerUid=lockUser->uid;
  lockBattle.opponentCount=1;lockBattle.rngState=939;
  lockBattle.opponents[0]=CollectionLogic::createPokemon(0xF2000006U,143,80,false,939);
  lockBattle.opponents[0].moves[0]=static_cast<MoveId>(150);
  lockBattle.opponents[0].movePp[0]=40;
  assert(BattleEngine::fightPvp(lockBattle,lockCollection,0,0).accepted);
  const BattleActionResult tooLow=BattleEngine::fightPvp(lockBattle,lockCollection,1,0);
  bool sawOhkoLevelBlock=false,sawOhkoMiss=false;
  for(uint8_t eventIndex=0;eventIndex<tooLow.eventCount;++eventIndex){
    const BattleEvent& event=tooLow.events[eventIndex];
    sawOhkoLevelBlock|=event.type==BattleEventType::MoveEffect&&
        event.value==static_cast<uint16_t>(BattleMoveEffect::OhkoTargetHigherLevel);
    sawOhkoMiss|=event.type==BattleEventType::MoveMissed;
  }
  assert(tooLow.accepted&&!tooLow.hit&&sawOhkoLevelBlock&&!sawOhkoMiss&&
         lockBattle.opponents[0].currentHp>0U);

  // This is the hardware-reported Battle Tower situation: the saved Articuno
  // is Lv90 while a Lv100 party member scales the facility opponent to Lv100.
  // Mind Reader must land, but Sheer Cold must report the level gate rather
  // than pretending that the setup move failed.
  *lockUser=CollectionLogic::createPokemon(lockUser->uid,144,90,false,940);
  lockUser->moves[0]=static_cast<MoveId>(170);lockUser->movePp[0]=5;
  lockUser->moves[1]=static_cast<MoveId>(329);lockUser->movePp[1]=5;
  BattleEngine::clear(lockBattle);lockBattle.active=true;lockBattle.kind=BattleKind::Tower;
  lockBattle.outcome=BattleOutcome::Ongoing;lockBattle.playerUid=lockUser->uid;
  lockBattle.opponentCount=1;lockBattle.rngState=941;
  lockBattle.opponents[0]=CollectionLogic::createPokemon(0xF2000007U,190,100,false,941);
  lockBattle.opponents[0].moves[0]=static_cast<MoveId>(150);
  lockBattle.opponents[0].movePp[0]=40;
  assert(BattleEngine::fightPvp(lockBattle,lockCollection,0,0).accepted);
  const BattleActionResult towerCold=BattleEngine::fightPvp(lockBattle,lockCollection,1,0);
  bool towerLevelBlock=false;
  for(uint8_t eventIndex=0;eventIndex<towerCold.eventCount;++eventIndex)
    towerLevelBlock|=towerCold.events[eventIndex].type==BattleEventType::MoveEffect&&
        towerCold.events[eventIndex].value==
            static_cast<uint16_t>(BattleMoveEffect::OhkoTargetHigherLevel);
  assert(towerCold.accepted&&!towerCold.hit&&towerLevelBlock&&
         lockBattle.opponents[0].currentHp>0U);

  // The guarantee is generic, not a Sheer Cold exception. With the target at
  // maximum evasion, Mind Reader still makes an ordinary inaccurate attack
  // connect during that next action; the end-turn timer then expires the
  // user->target relation exactly as in FireRed.
  *lockUser=CollectionLogic::createPokemon(lockUser->uid,150,80,false,932);
  lockUser->moves[0]=static_cast<MoveId>(170);lockUser->movePp[0]=5;
  lockUser->moves[1]=static_cast<MoveId>(87);lockUser->movePp[1]=10;
  BattleEngine::clear(lockBattle);lockBattle.active=true;lockBattle.kind=BattleKind::Pvp;
  lockBattle.outcome=BattleOutcome::Ongoing;lockBattle.playerUid=lockUser->uid;
  lockBattle.opponentCount=1;lockBattle.rngState=2;
  lockBattle.opponents[0]=CollectionLogic::createPokemon(0xF2000003U,143,80,false,933);
  lockBattle.opponents[0].moves[0]=static_cast<MoveId>(45);lockBattle.opponents[0].movePp[0]=40;
  const BattleActionResult genericReader=BattleEngine::fightPvp(lockBattle,lockCollection,0,0);
  assert(genericReader.accepted&&lockBattle.opponentVolatiles[0].sureHitTurns==1U);
  lockBattle.opponentVolatiles[0].evasionStage=6;
  const uint16_t genericBefore=lockBattle.opponents[0].currentHp;
  const BattleActionResult genericHit=BattleEngine::fightPvp(lockBattle,lockCollection,1,0);
  assert(genericHit.accepted&&genericHit.hit&&lockBattle.opponents[0].currentHp<genericBefore&&
         lockBattle.opponentVolatiles[0].sureHitTurns==0U);

  // STATUS3_ALWAYS_HITS_TURN(2) expires after the following turn when it is
  // not consumed.  It must not remain banked until some later inaccurate
  // attack happens to use it.
  *lockUser=CollectionLogic::createPokemon(lockUser->uid,150,80,false,934);
  lockUser->moves[0]=static_cast<MoveId>(170);lockUser->movePp[0]=5;
  lockUser->moves[1]=static_cast<MoveId>(150);lockUser->movePp[1]=40;
  BattleEngine::clear(lockBattle);lockBattle.active=true;lockBattle.kind=BattleKind::Pvp;
  lockBattle.outcome=BattleOutcome::Ongoing;lockBattle.playerUid=lockUser->uid;
  lockBattle.opponentCount=1;lockBattle.rngState=4;
  lockBattle.opponents[0]=CollectionLogic::createPokemon(0xF2000004U,143,80,false,935);
  lockBattle.opponents[0].moves[0]=static_cast<MoveId>(150);
  lockBattle.opponents[0].movePp[0]=40;
  const BattleActionResult expiringReader=BattleEngine::fightPvp(
      lockBattle,lockCollection,0,0xFEU);
  assert(expiringReader.accepted&&lockBattle.opponentVolatiles[0].sureHitTurns==1U);
  const BattleActionResult unusedGuarantee=BattleEngine::fightPvp(
      lockBattle,lockCollection,1,0xFEU);
  assert(unusedGuarantee.accepted&&lockBattle.opponentVolatiles[0].sureHitTurns==0U);

  // Protect uses its original +3 priority, blocks only moves carrying
  // FLAG_PROTECT_AFFECTED and leaves a slow user unharmed on the first use.
  PokemonCollection protectCollection;CollectionLogic::initialize(protectCollection);
  assert(CollectionLogic::chooseStarter(protectCollection,1,940));
  OwnedPokemon* protector=CollectionLogic::active(protectCollection,0);
  *protector=CollectionLogic::createPokemon(protector->uid,143,50,false,940);
  protector->moves[0]=static_cast<MoveId>(182);protector->movePp[0]=10;
  BattleState protectBattle;BattleEngine::clear(protectBattle);protectBattle.active=true;
  protectBattle.kind=BattleKind::Pvp;protectBattle.outcome=BattleOutcome::Ongoing;
  protectBattle.playerUid=protector->uid;protectBattle.opponentCount=1;protectBattle.rngState=941;
  protectBattle.opponents[0]=CollectionLogic::createPokemon(0xF2000002U,101,50,false,941);
  protectBattle.opponents[0].moves[0]=MoveId::Tackle;protectBattle.opponents[0].movePp[0]=35;
  const uint16_t protectedHp=protector->currentHp;
  const BattleActionResult protectedTurn=BattleEngine::fightPvp(
      protectBattle,protectCollection,0,0);
  assert(protectedTurn.accepted&&protector->currentHp==protectedHp&&
         protectBattle.playerProtectChain==1U);

  // Trainer items are selected before attack order is calculated. Even a
  // much faster player must see the opposing Potion resolve before its move.
  PokemonCollection itemOrderCollection;CollectionLogic::initialize(itemOrderCollection);
  assert(CollectionLogic::chooseStarter(itemOrderCollection,1,950));
  OwnedPokemon* fastPlayer=CollectionLogic::active(itemOrderCollection,0);
  *fastPlayer=CollectionLogic::createPokemon(fastPlayer->uid,135,100,false,950);
  fastPlayer->moves[0]=static_cast<MoveId>(45);fastPlayer->movePp[0]=40;
  BattleState itemOrderBattle;BattleEngine::clear(itemOrderBattle);
  itemOrderBattle.active=true;itemOrderBattle.kind=BattleKind::Trainer;
  itemOrderBattle.outcome=BattleOutcome::Ongoing;itemOrderBattle.playerUid=fastPlayer->uid;
  itemOrderBattle.opponentCount=1;itemOrderBattle.opponentItemUses=1;
  itemOrderBattle.opponents[0]=CollectionLogic::createPokemon(0xF3000001U,143,30,false,951);
  itemOrderBattle.opponents[0].moves[0]=MoveId::Tackle;
  itemOrderBattle.opponents[0].movePp[0]=35;
  itemOrderBattle.opponents[0].currentHp=1;
  const BattleActionResult orderedItem=BattleEngine::fight(itemOrderBattle,itemOrderCollection,0);
  int itemEventIndex=-1,playerMoveIndex=-1;
  for(uint8_t eventIndex=0;eventIndex<orderedItem.eventCount;++eventIndex){
    const BattleEvent& event=orderedItem.events[eventIndex];
    if(itemEventIndex<0&&event.type==BattleEventType::ItemUsed&&
       event.side==BattleSide::Opponent)itemEventIndex=eventIndex;
    if(playerMoveIndex<0&&event.type==BattleEventType::MoveUsed&&
       event.side==BattleSide::Player)playerMoveIndex=eventIndex;
  }
  assert(orderedItem.enemyItemUsed&&itemEventIndex>=0&&playerMoveIndex>=0&&
         itemEventIndex<playerMoveIndex);

  // Full Heal includes confusion in Gen III, even when no persistent major
  // status is present. Awakening also clears Nightmare's battle-local state.
  Inventory cureInventory{};
  cureInventory.medicine[static_cast<uint8_t>(BattleItem::FullHeal)]=1;
  itemOrderBattle.opponentItemUses=0;
  itemOrderBattle.playerVolatile.confusionTurns=4;
  BattleActionResult cureResult{};
  BattleEngine::useItemInto(itemOrderBattle,itemOrderCollection,cureInventory,
                            BattleItem::FullHeal,cureResult);
  assert(cureResult.accepted&&!itemOrderBattle.playerVolatile.confusionTurns&&
         cureInventory.medicine[static_cast<uint8_t>(BattleItem::FullHeal)]==0);
  fastPlayer->status=StatusCondition::Sleep;
  itemOrderBattle.playerVolatile.sleepTurns=3;
  itemOrderBattle.playerNightmare=true;
  cureInventory.medicine[static_cast<uint8_t>(BattleItem::Awakening)]=1;
  BattleEngine::useItemInto(itemOrderBattle,itemOrderCollection,cureInventory,
                            BattleItem::Awakening,cureResult);
  assert(cureResult.accepted&&fastPlayer->status==StatusCondition::None&&
         !itemOrderBattle.playerVolatile.sleepTurns&&!itemOrderBattle.playerNightmare);

  // A primary status move checks an existing major status before accuracy.
  // Same status gets FireRed's dedicated message; a different status gets
  // the generic failure. Neither case may be presented as an accuracy miss.
  fastPlayer->moves[0]=static_cast<MoveId>(79); // SLEEP POWDER (75 accuracy)
  fastPlayer->movePp[0]=15;
  itemOrderBattle.opponents[0].status=StatusCondition::Sleep;
  itemOrderBattle.opponents[0].currentHp=itemOrderBattle.opponents[0].maximumHp;
  const BattleActionResult alreadySleeping=BattleEngine::fightPvp(
      itemOrderBattle,itemOrderCollection,0,0xFEU);
  bool sawAlreadySleeping=false,sawStatusMiss=false;
  for(uint8_t eventIndex=0;eventIndex<alreadySleeping.eventCount;++eventIndex){
    const BattleEvent& event=alreadySleeping.events[eventIndex];
    sawAlreadySleeping|=event.type==BattleEventType::MoveEffect&&
        event.value==static_cast<uint16_t>(BattleMoveEffect::AlreadyAsleep);
    sawStatusMiss|=event.type==BattleEventType::MoveMissed;
  }
  assert(sawAlreadySleeping&&!sawStatusMiss&&
         itemOrderBattle.opponents[0].status==StatusCondition::Sleep);

  itemOrderBattle.opponents[0].status=StatusCondition::Poison;
  const BattleActionResult secondStatus=BattleEngine::fightPvp(
      itemOrderBattle,itemOrderCollection,0,0xFEU);
  bool sawGenericFailure=false;
  sawStatusMiss=false;
  for(uint8_t eventIndex=0;eventIndex<secondStatus.eventCount;++eventIndex){
    const BattleEvent& event=secondStatus.events[eventIndex];
    sawGenericFailure|=event.type==BattleEventType::MoveEffect&&
        event.value==static_cast<uint16_t>(BattleMoveEffect::Failed);
    sawStatusMiss|=event.type==BattleEventType::MoveMissed;
  }
  assert(sawGenericFailure&&!sawStatusMiss&&
         itemOrderBattle.opponents[0].status==StatusCondition::Poison);

  // The opponent-controlled path follows the same rule.
  fastPlayer->status=StatusCondition::Paralysis;
  fastPlayer->moves[0]=static_cast<MoveId>(150);fastPlayer->movePp[0]=40;
  itemOrderBattle.opponents[0].status=StatusCondition::None;
  itemOrderBattle.opponents[0].moves[0]=static_cast<MoveId>(86); // THUNDER WAVE
  itemOrderBattle.opponents[0].movePp[0]=20;
  const BattleActionResult enemyAlreadyParalyzed=BattleEngine::fightPvp(
      itemOrderBattle,itemOrderCollection,0,0);
  bool sawAlreadyParalyzed=false;
  sawStatusMiss=false;
  for(uint8_t eventIndex=0;eventIndex<enemyAlreadyParalyzed.eventCount;++eventIndex){
    const BattleEvent& event=enemyAlreadyParalyzed.events[eventIndex];
    sawAlreadyParalyzed|=event.type==BattleEventType::MoveEffect&&
        event.value==static_cast<uint16_t>(BattleMoveEffect::AlreadyParalyzed);
    sawStatusMiss|=event.type==BattleEventType::MoveMissed;
  }
  assert(sawAlreadyParalyzed&&!sawStatusMiss&&
         fastPlayer->status==StatusCondition::Paralysis);

  // Confusion expiry is an attack-canceller event in FireRed: announce that
  // the battler snapped out before showing the move it can now execute.
  PokemonCollection expiryCollection;CollectionLogic::initialize(expiryCollection);
  assert(CollectionLogic::chooseStarter(expiryCollection,1,960));
  OwnedPokemon* expiryPlayer=CollectionLogic::active(expiryCollection,0);
  expiryPlayer->moves[0]=MoveId::Tackle;expiryPlayer->movePp[0]=35;
  BattleState expiryBattle;BattleEngine::clear(expiryBattle);
  expiryBattle.active=true;expiryBattle.kind=BattleKind::Pvp;
  expiryBattle.outcome=BattleOutcome::Ongoing;expiryBattle.playerUid=expiryPlayer->uid;
  expiryBattle.opponentCount=1;expiryBattle.rngState=961;
  expiryBattle.opponents[0]=CollectionLogic::createPokemon(0xF4000001U,143,5,false,961);
  expiryBattle.opponents[0].moves[0]=MoveId::Tackle;
  expiryBattle.opponents[0].movePp[0]=35;
  expiryBattle.playerVolatile.confusionTurns=1;
  const BattleActionResult playerConfusionEnded=BattleEngine::fightPvp(
      expiryBattle,expiryCollection,0,0);
  int confusionEndIndex=-1,expiryMoveIndex=-1;
  for(uint8_t eventIndex=0;eventIndex<playerConfusionEnded.eventCount;++eventIndex){
    const BattleEvent& event=playerConfusionEnded.events[eventIndex];
    if(confusionEndIndex<0&&event.type==BattleEventType::MoveEffect&&
       event.side==BattleSide::Player&&
       event.value==static_cast<uint16_t>(BattleMoveEffect::ConfusionEnded))
      confusionEndIndex=eventIndex;
    if(expiryMoveIndex<0&&event.type==BattleEventType::MoveUsed&&
       event.side==BattleSide::Player)expiryMoveIndex=eventIndex;
  }
  assert(playerConfusionEnded.accepted&&confusionEndIndex>=0&&expiryMoveIndex>=0&&
         confusionEndIndex<expiryMoveIndex&&!expiryBattle.playerVolatile.confusionTurns);

  expiryBattle.opponentVolatiles[0].confusionTurns=1;
  const BattleActionResult opponentConfusionEnded=BattleEngine::fightPvp(
      expiryBattle,expiryCollection,0,0);
  confusionEndIndex=-1;expiryMoveIndex=-1;
  for(uint8_t eventIndex=0;eventIndex<opponentConfusionEnded.eventCount;++eventIndex){
    const BattleEvent& event=opponentConfusionEnded.events[eventIndex];
    if(confusionEndIndex<0&&event.type==BattleEventType::MoveEffect&&
       event.side==BattleSide::Opponent&&
       event.value==static_cast<uint16_t>(BattleMoveEffect::ConfusionEnded))
      confusionEndIndex=eventIndex;
    if(expiryMoveIndex<0&&event.type==BattleEventType::MoveUsed&&
       event.side==BattleSide::Opponent)expiryMoveIndex=eventIndex;
  }
  assert(opponentConfusionEnded.accepted&&confusionEndIndex>=0&&expiryMoveIndex>=0&&
         confusionEndIndex<expiryMoveIndex&&
         !expiryBattle.opponentVolatiles[0].confusionTurns);

  // Temporary weather owns a journal event too. `before` retains the old
  // field state after the resolver returns to clear weather, allowing the UI
  // to select each exact FireRed ending message.
  static constexpr BattleWeather expiringWeather[]={BattleWeather::Rain,
      BattleWeather::Sun,BattleWeather::Sandstorm,BattleWeather::Hail};
  for(const BattleWeather weather:expiringWeather){
    expiryPlayer->currentHp=expiryPlayer->maximumHp;
    expiryPlayer->movePp[0]=35;
    BattleState weatherBattle;BattleEngine::clear(weatherBattle);
    weatherBattle.active=true;weatherBattle.kind=BattleKind::Pvp;
    weatherBattle.outcome=BattleOutcome::Ongoing;
    weatherBattle.playerUid=expiryPlayer->uid;weatherBattle.opponentCount=1;
    weatherBattle.rngState=static_cast<uint32_t>(970U+static_cast<uint8_t>(weather));
    weatherBattle.opponents[0]=CollectionLogic::createPokemon(
        static_cast<uint32_t>(0xF4000010U+static_cast<uint8_t>(weather)),143,5,false,
        static_cast<uint32_t>(970U+static_cast<uint8_t>(weather)));
    weatherBattle.opponents[0].moves[0]=MoveId::Tackle;
    weatherBattle.opponents[0].movePp[0]=35;
    weatherBattle.weather=weather;weatherBattle.weatherTurns=1;
    const BattleActionResult weatherEnded=BattleEngine::fightPvp(
        weatherBattle,expiryCollection,0,0);
    bool sawWeatherEnd=false;
    int weatherContinuesIndex=-1,firstWeatherHurtIndex=-1,
        firstWeatherHpIndex=-1;
    uint8_t weatherHurtCount=0;
    for(uint8_t eventIndex=0;eventIndex<weatherEnded.eventCount;++eventIndex){
      const BattleEvent& event=weatherEnded.events[eventIndex];
      sawWeatherEnd|=event.type==BattleEventType::MoveEffect&&
          event.value==static_cast<uint16_t>(BattleMoveEffect::WeatherEnded)&&
          event.before==static_cast<uint16_t>(weather);
      if(event.type==BattleEventType::MoveEffect&&
         event.value==static_cast<uint16_t>(BattleMoveEffect::WeatherContinues)&&
         event.before==static_cast<uint16_t>(weather))weatherContinuesIndex=eventIndex;
      if(event.type==BattleEventType::MoveEffect&&
         event.value==static_cast<uint16_t>(BattleMoveEffect::WeatherHurt)&&
         event.before==static_cast<uint16_t>(weather)){
        if(firstWeatherHurtIndex<0)firstWeatherHurtIndex=eventIndex;
        ++weatherHurtCount;
      }else if(firstWeatherHurtIndex>=0&&firstWeatherHpIndex<0&&
               event.type==BattleEventType::HpChanged)
        firstWeatherHpIndex=eventIndex;
    }
    assert(weatherEnded.accepted&&sawWeatherEnd&&
           weatherBattle.weather==BattleWeather::Clear&&weatherBattle.weatherTurns==0);
    const bool damagingWeather=weather==BattleWeather::Sandstorm||
        weather==BattleWeather::Hail;
    if(damagingWeather)
      assert(weatherContinuesIndex>=0&&weatherHurtCount==2U&&
             weatherContinuesIndex<firstWeatherHurtIndex&&
             firstWeatherHurtIndex<firstWeatherHpIndex);
    else assert(weatherContinuesIndex<0&&weatherHurtCount==0U);
  }
  return 0;
}
