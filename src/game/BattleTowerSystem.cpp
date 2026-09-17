#include "game/BattleTowerSystem.h"
#include "game/Economy.h"
#include "game/MegaChallengeSystem.h"

#include <algorithm>
#include <cstring>

namespace {
#include "BattleTowerDataGenerated.inc"

static_assert(sizeof(kBattleTowerMons)/sizeof(kBattleTowerMons[0]) ==
              BattleTowerSystem::kMonTemplateCount, "Emerald Tower set catalog changed");
static_assert(sizeof(kBattleTowerTrainers)/sizeof(kBattleTowerTrainers[0]) ==
              BattleTowerSystem::kTrainerCount, "Emerald Tower trainer catalog changed");

uint32_t next(BattleState& battle) {
  uint32_t value=battle.rngState?battle.rngState:0xB4771E5DU;
  value^=value<<13U;value^=value>>17U;value^=value<<5U;
  battle.rngState=value;return value;
}

void setTrainerId(BattleState& battle,uint16_t id) {
  // Tower reuses League's two one-byte selectors while BattleKind::Tower is
  // active. No save fields are added and mid-series power loss is safe.
  battle.leagueRegion=static_cast<uint8_t>(id&0xFFU);
  battle.leagueStage=static_cast<uint8_t>(id>>8U);
}

OwnedPokemon* firstHealthy(PokemonCollection& collection) {
  for(uint8_t slot=0;slot<kPartyCapacity;++slot){
    OwnedPokemon* pokemon=CollectionLogic::active(collection,slot);
    if(pokemon&&pokemon->currentHp&&!pokemon->recoverySecondsRemaining)return pokemon;
  }
  return nullptr;
}

uint8_t bitCount(uint8_t value) {
  uint8_t count=0;while(value){count=static_cast<uint8_t>(count+(value&1U));value>>=1U;}return count;
}

void applyAuthoredStats(OwnedPokemon& pokemon,const BattleTowerMonTemplate& authored,
                        uint8_t fixedIv) {
  pokemon.ivs={fixedIv,fixedIv,fixedIv,fixedIv,fixedIv,fixedIv};
  pokemon.nature=authored.nature;
  pokemon.heldItem=authored.heldItem;
  pokemon.evs={};
  const uint8_t stats=std::max<uint8_t>(1U,bitCount(authored.evSpread));
  const uint8_t each=static_cast<uint8_t>(std::min<uint16_t>(255U,510U/stats));
  if(authored.evSpread&(1U<<0U))pokemon.evs.hp=each;
  if(authored.evSpread&(1U<<1U))pokemon.evs.attack=each;
  if(authored.evSpread&(1U<<2U))pokemon.evs.defense=each;
  if(authored.evSpread&(1U<<3U))pokemon.evs.speed=each;
  if(authored.evSpread&(1U<<4U))pokemon.evs.spAttack=each;
  if(authored.evSpread&(1U<<5U))pokemon.evs.spDefense=each;
  for(uint8_t slot=0;slot<kMoveSlots;++slot){
    pokemon.moves[slot]=static_cast<MoveId>(authored.moves[slot]);
    const FullMoveData* move=findFullMove(pokemon.moves[slot]);
    pokemon.movePp[slot]=move?move->pp:0;
  }
  CollectionLogic::refreshDerivedStats(pokemon,false);
}

bool conflicts(const BattleState& battle,uint8_t selected,const BattleTowerMonTemplate& candidate) {
  for(uint8_t slot=0;slot<selected;++slot){
    if(battle.opponents[slot].speciesId==candidate.speciesId)return true;
    if(candidate.heldItem!=HeldItem::None&&battle.opponents[slot].heldItem==candidate.heldItem)return true;
  }
  return false;
}

uint16_t chooseTrainer(BattleState& battle) {
  // Each successive opponent comes from a stronger original Emerald tier.
  // Their authored IV tier remains intact; level itself follows the player's
  // strongest selected Pokemon as requested for Pokegochi.
  static constexpr uint16_t firstByStage[]={140,180,220};
  const uint16_t first=firstByStage[std::min<uint8_t>(battle.gymStage,2U)];
  const uint16_t count=static_cast<uint16_t>(BattleTowerSystem::kTrainerCount-first);
  uint16_t id=static_cast<uint16_t>(first+next(battle)%count);
  if(battle.gymStage&&id==BattleTowerSystem::trainerId(battle))
    id=static_cast<uint16_t>(first+(id-first+1U)%count);
  return id;
}

void resetFieldForStage(BattleState& battle) {
  battle.opponentIndex=0;battle.turn=0;battle.playerVolatile=CombatVolatile{};
  for(auto& state:battle.opponentVolatiles)state=CombatVolatile{};
  battle.playerMoveEffects=DedicatedMoveEffectState{};
  for(auto& state:battle.opponentMoveEffects)state=DedicatedMoveEffectState{};
  battle.playerSafeguardTurns=battle.opponentSafeguardTurns=0;
  battle.playerReflectTurns=battle.opponentReflectTurns=0;
  battle.playerLightScreenTurns=battle.opponentLightScreenTurns=0;
  battle.playerMistTurns=battle.opponentMistTurns=0;
  battle.playerProtectChain=battle.opponentProtectChain=0;
  battle.playerEndureThisTurn=battle.opponentEndureThisTurn=false;
  battle.playerNightmare=false;
  for(bool& nightmare:battle.opponentNightmares)nightmare=false;
  battle.playerBide=BideState{};
  for(auto& bide:battle.opponentBides)bide=BideState{};
  battle.playerEncoreTurns=0;
  for(uint8_t& turns:battle.opponentEncoreTurns)turns=0;
  battle.delayedToPlayer=DelayedAttackState{};battle.delayedToOpponent=DelayedAttackState{};
  battle.weather=BattleWeather::Clear;battle.weatherTurns=0;
  battle.playerSpikesLayers=battle.opponentSpikesLayers=0;
  battle.payDayMoney=0;
}

bool loadStage(BattleState& battle,PokemonCollection& collection) {
  const uint16_t id=chooseTrainer(battle);
  setTrainerId(battle,id);
  const BattleTowerTrainerDefinition* definition=BattleTowerSystem::trainer(id);
  if(!definition||definition->poolCount<3U)return false;
  resetFieldForStage(battle);
  battle.opponentCount=kOpponentTeamCapacity;
  battle.opponentItemUses=0; // Facility trainers never use Bag medicine.
  const uint8_t level=std::max<uint8_t>(5U,BattleEngine::highestPartyLevel(collection));
  battle.rewardMoney=static_cast<uint32_t>(level)*(80U+20U*battle.gymStage);
  for(uint8_t slot=0;slot<kOpponentTeamCapacity;++slot){
    const BattleTowerMonTemplate* selected=nullptr;
    for(uint16_t attempt=0;attempt<definition->poolCount*3U;++attempt){
      const uint16_t packed=static_cast<uint16_t>(definition->poolOffset+
          next(battle)%definition->poolCount);
      const uint16_t templateId=kBattleTowerPoolEntries[packed];
      const BattleTowerMonTemplate* candidate=BattleTowerSystem::monTemplate(templateId);
      if(candidate&&!conflicts(battle,slot,*candidate)){selected=candidate;break;}
    }
    if(!selected){
      for(uint16_t entry=0;entry<definition->poolCount;++entry){
        const BattleTowerMonTemplate* candidate=BattleTowerSystem::monTemplate(
            kBattleTowerPoolEntries[definition->poolOffset+entry]);
        if(candidate&&!conflicts(battle,slot,*candidate)){selected=candidate;break;}
      }
    }
    if(!selected)return false;
    battle.opponents[slot]=CollectionLogic::createPokemon(0,selected->speciesId,level,false,next(battle));
    applyAuthoredStats(battle.opponents[slot],*selected,definition->fixedIv);
  }
  BattleEngine::orderOpponentTeamWeakestFirst(battle);
  BattleEngine::ensureOpponentUids(battle);
  return true;
}
} // namespace

bool BattleTowerSystem::available(const GymProgress& progress) {
  return MegaChallengeSystem::prerequisitesMet(progress) &&
         MegaChallengeSystem::completed(progress);
}

bool BattleTowerSystem::awardCompletionReward(Inventory& inventory) {
  uint16_t& rareCandies=inventory.heldItems[kRareCandyInventorySlot];
  if(rareCandies>=kInventoryStackLimit)return false;
  ++rareCandies;return true;
}

BattleTowerRewardResult BattleTowerSystem::awardCompletionReward(
    Inventory& inventory,EggState& egg,const PokedexState& pokedex,uint32_t seed) {
  BattleTowerRewardResult reward;
  // Hash the battle RNG once before applying the exact 1/200 threshold. The
  // player cannot reroll the result by reopening a screen because completion
  // and its prize are committed in the same save transaction.
  uint32_t roll=seed?seed:0xB477E660U;
  roll^=roll>>16U;roll*=0x7FEB352DUL;
  roll^=roll>>15U;roll*=0x846CA68BUL;
  roll^=roll>>16U;
  if(!egg.active&&!egg.offerPending&&
     (roll%kSpecialEggOddsDenominator)==0U&&
     EggSystem::grantBattleTowerSpecial(egg,pokedex,roll^0x45474721UL)){
    reward.kind=BattleTowerRewardKind::SpecialEgg;
    reward.eggSpeciesId=egg.speciesId;
    return reward;
  }
  if(awardCompletionReward(inventory))reward.kind=BattleTowerRewardKind::RareCandy;
  return reward;
}

bool BattleTowerSystem::start(BattleState& battle,
                              PokemonCollection& collection,uint32_t playerUid,
                              const GymProgress& progress,uint32_t seed) {
  OwnedPokemon* player=CollectionLogic::find(collection,playerUid);
  if(battle.active||!available(progress)||!player||
     !CollectionLogic::isInParty(collection,playerUid)||!player->currentHp||
     player->recoverySecondsRemaining)return false;
  BattleEngine::clear(battle);battle.active=true;battle.kind=BattleKind::Tower;
  battle.outcome=BattleOutcome::Ongoing;battle.playerUid=playerUid;
  battle.unlockedGeneration=progress.unlockedGeneration;
  battle.rngState=seed?seed:0xB4771E5DU;battle.gymStage=0;
  if(!loadStage(battle,collection)){BattleEngine::clear(battle);return false;}
  BattleEngine::applyEntryAbilities(battle,collection);return true;
}

bool BattleTowerSystem::advance(BattleState& battle,PokemonCollection& collection) {
  if(battle.kind!=BattleKind::Tower||battle.outcome!=BattleOutcome::Victory||
     battle.gymStage+1U>=kStageCount)return false;
  OwnedPokemon* current=CollectionLogic::find(collection,battle.playerUid);
  if(!current||!current->currentHp||current->recoverySecondsRemaining){
    current=firstHealthy(collection);
    if(!current){battle.active=false;battle.outcome=BattleOutcome::Defeat;return false;}
    battle.playerUid=current->uid;
  }
  ++battle.gymStage;battle.active=true;battle.outcome=BattleOutcome::Ongoing;
  if(!loadStage(battle,collection)){battle.active=false;battle.outcome=BattleOutcome::Defeat;return false;}
  BattleEngine::applyEntryAbilities(battle,collection);return true;
}

uint16_t BattleTowerSystem::trainerId(const BattleState& battle) {
  return static_cast<uint16_t>(battle.leagueRegion|
      (static_cast<uint16_t>(battle.leagueStage)<<8U));
}

const BattleTowerTrainerDefinition* BattleTowerSystem::trainer(const BattleState& battle) {
  return battle.kind==BattleKind::Tower?trainer(trainerId(battle)):nullptr;
}

const BattleTowerTrainerDefinition* BattleTowerSystem::trainer(uint16_t id) {
  return id<kTrainerCount?&kBattleTowerTrainers[id]:nullptr;
}

const BattleTowerMonTemplate* BattleTowerSystem::monTemplate(uint16_t id) {
  return id<kMonTemplateCount?&kBattleTowerMons[id]:nullptr;
}
