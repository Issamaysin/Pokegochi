#include "game/BattleEngine.h"
#include <algorithm>
#include <cstring>
#include "game/TrainerData.h"

namespace {
uint8_t statValue(uint8_t base, uint8_t level) {
  return static_cast<uint8_t>(((2U * base * level) / 100U) + 5U);
}
uint32_t staged(uint32_t value, int8_t stage) {
  stage = std::max<int8_t>(-6, std::min<int8_t>(6, stage));
  return stage >= 0 ? value * (2U + stage) / 2U : value * 2U / (2U - stage);
}

uint16_t ballMultiplier100(PokeBallType ball) {
  switch (ball) {
    case PokeBallType::PokeBall: return 100;
    case PokeBallType::GreatBall: return 150;
    case PokeBallType::UltraBall: return 200;
    case PokeBallType::MasterBall: return 10000;
    case PokeBallType::Count: return 0;
  }
  return 0;
}

uint16_t typeMultiplier100(PokemonType attack, const SpeciesData& defender) {
  uint16_t value = 100;
  auto apply = [&](PokemonType type) {
    auto is = [&](PokemonType a, PokemonType d) { return attack == a && type == d; };
    if (is(PokemonType::Normal,PokemonType::Ghost) || is(PokemonType::Fighting,PokemonType::Ghost) ||
        is(PokemonType::Electric,PokemonType::Ground) || is(PokemonType::Ground,PokemonType::Flying) ||
        is(PokemonType::Psychic,PokemonType::Dark) || is(PokemonType::Ghost,PokemonType::Normal) ||
        is(PokemonType::Poison,PokemonType::Steel)) { value = 0; return; }
    const bool super =
      is(PokemonType::Fire,PokemonType::Grass)||is(PokemonType::Fire,PokemonType::Ice)||is(PokemonType::Fire,PokemonType::Bug)||is(PokemonType::Fire,PokemonType::Steel)||
      is(PokemonType::Water,PokemonType::Fire)||is(PokemonType::Water,PokemonType::Ground)||is(PokemonType::Water,PokemonType::Rock)||
      is(PokemonType::Electric,PokemonType::Water)||is(PokemonType::Electric,PokemonType::Flying)||
      is(PokemonType::Grass,PokemonType::Water)||is(PokemonType::Grass,PokemonType::Ground)||is(PokemonType::Grass,PokemonType::Rock)||
      is(PokemonType::Ice,PokemonType::Grass)||is(PokemonType::Ice,PokemonType::Ground)||is(PokemonType::Ice,PokemonType::Flying)||is(PokemonType::Ice,PokemonType::Dragon)||
      is(PokemonType::Fighting,PokemonType::Normal)||is(PokemonType::Fighting,PokemonType::Ice)||is(PokemonType::Fighting,PokemonType::Rock)||is(PokemonType::Fighting,PokemonType::Dark)||is(PokemonType::Fighting,PokemonType::Steel)||
      is(PokemonType::Poison,PokemonType::Grass)||is(PokemonType::Ground,PokemonType::Fire)||is(PokemonType::Ground,PokemonType::Electric)||is(PokemonType::Ground,PokemonType::Poison)||is(PokemonType::Ground,PokemonType::Rock)||is(PokemonType::Ground,PokemonType::Steel)||
      is(PokemonType::Flying,PokemonType::Grass)||is(PokemonType::Flying,PokemonType::Fighting)||is(PokemonType::Flying,PokemonType::Bug)||
      is(PokemonType::Psychic,PokemonType::Fighting)||is(PokemonType::Psychic,PokemonType::Poison)||
      is(PokemonType::Bug,PokemonType::Grass)||is(PokemonType::Bug,PokemonType::Psychic)||is(PokemonType::Bug,PokemonType::Dark)||
      is(PokemonType::Rock,PokemonType::Fire)||is(PokemonType::Rock,PokemonType::Ice)||is(PokemonType::Rock,PokemonType::Flying)||is(PokemonType::Rock,PokemonType::Bug)||
      is(PokemonType::Ghost,PokemonType::Psychic)||is(PokemonType::Ghost,PokemonType::Ghost)||is(PokemonType::Dragon,PokemonType::Dragon)||is(PokemonType::Dark,PokemonType::Psychic)||is(PokemonType::Dark,PokemonType::Ghost)||
      is(PokemonType::Steel,PokemonType::Ice)||is(PokemonType::Steel,PokemonType::Rock);
    if (super) { value *= 2; return; }
    const bool resist = is(PokemonType::Normal,PokemonType::Rock)||is(PokemonType::Normal,PokemonType::Steel)||
      is(PokemonType::Fire,PokemonType::Fire)||is(PokemonType::Fire,PokemonType::Water)||is(PokemonType::Fire,PokemonType::Rock)||is(PokemonType::Fire,PokemonType::Dragon)||
      is(PokemonType::Water,PokemonType::Water)||is(PokemonType::Water,PokemonType::Grass)||is(PokemonType::Water,PokemonType::Dragon)||
      is(PokemonType::Electric,PokemonType::Electric)||is(PokemonType::Electric,PokemonType::Grass)||is(PokemonType::Electric,PokemonType::Dragon)||
      is(PokemonType::Grass,PokemonType::Fire)||is(PokemonType::Grass,PokemonType::Grass)||is(PokemonType::Grass,PokemonType::Poison)||is(PokemonType::Grass,PokemonType::Flying)||is(PokemonType::Grass,PokemonType::Bug)||is(PokemonType::Grass,PokemonType::Dragon)||is(PokemonType::Grass,PokemonType::Steel)||
      is(PokemonType::Ice,PokemonType::Fire)||is(PokemonType::Ice,PokemonType::Water)||is(PokemonType::Ice,PokemonType::Ice)||is(PokemonType::Ice,PokemonType::Steel)||
      is(PokemonType::Fighting,PokemonType::Poison)||is(PokemonType::Fighting,PokemonType::Flying)||is(PokemonType::Fighting,PokemonType::Psychic)||is(PokemonType::Fighting,PokemonType::Bug)||
      is(PokemonType::Poison,PokemonType::Poison)||is(PokemonType::Poison,PokemonType::Ground)||is(PokemonType::Poison,PokemonType::Rock)||is(PokemonType::Poison,PokemonType::Ghost)||
      is(PokemonType::Ground,PokemonType::Grass)||is(PokemonType::Ground,PokemonType::Bug)||is(PokemonType::Flying,PokemonType::Electric)||is(PokemonType::Flying,PokemonType::Rock)||is(PokemonType::Flying,PokemonType::Steel)||
      is(PokemonType::Psychic,PokemonType::Psychic)||is(PokemonType::Psychic,PokemonType::Steel)||is(PokemonType::Bug,PokemonType::Fire)||is(PokemonType::Bug,PokemonType::Fighting)||is(PokemonType::Bug,PokemonType::Poison)||is(PokemonType::Bug,PokemonType::Flying)||is(PokemonType::Bug,PokemonType::Ghost)||is(PokemonType::Bug,PokemonType::Steel)||
      is(PokemonType::Rock,PokemonType::Fighting)||is(PokemonType::Rock,PokemonType::Ground)||is(PokemonType::Rock,PokemonType::Steel)||is(PokemonType::Ghost,PokemonType::Dark)||is(PokemonType::Dragon,PokemonType::Steel)||is(PokemonType::Dark,PokemonType::Fighting)||is(PokemonType::Dark,PokemonType::Dark)||is(PokemonType::Dark,PokemonType::Steel)||
      is(PokemonType::Steel,PokemonType::Fire)||is(PokemonType::Steel,PokemonType::Water)||is(PokemonType::Steel,PokemonType::Electric)||is(PokemonType::Steel,PokemonType::Steel);
    if (resist) value /= 2;
  };
  apply(defender.type1);
  if (defender.type2 != defender.type1) apply(defender.type2);
  return value;
}

OwnedPokemon* firstHealthyPartyMember(PokemonCollection& collection, uint32_t excludedUid = 0) {
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) {
    OwnedPokemon* candidate = CollectionLogic::active(collection, slot);
    if (candidate && candidate->uid != excludedUid && candidate->currentHp > 0 && candidate->recoverySecondsRemaining == 0) return candidate;
  }
  return nullptr;
}

bool selectablePartyMember(PokemonCollection& collection, uint32_t uid) {
  OwnedPokemon* pokemon = CollectionLogic::find(collection, uid);
  return pokemon && CollectionLogic::isInParty(collection, uid) && pokemon->currentHp > 0 &&
         pokemon->recoverySecondsRemaining == 0;
}

uint8_t boundedLevel(int16_t level) {
  return static_cast<uint8_t>(std::max<int16_t>(2, std::min<int16_t>(100, level)));
}
bool abilityIs(const OwnedPokemon& pokemon, const char* name) {
  const AbilityData* ability = findAbility(pokemon.abilityId);
  return ability && std::strcmp(ability->name, name) == 0;
}

void refreshLevelMoves(OwnedPokemon& pokemon) {
  MoveId desired[4]{}; movesForLevel(pokemon.speciesId, pokemon.level, desired);
  uint8_t pp[4]{};
  for (uint8_t i = 0; i < 4; ++i) {
    const FullMoveData* data = findFullMove(desired[i]);
    pp[i] = data ? data->pp : 0;
    for (uint8_t old = 0; old < 4; ++old) if (pokemon.moves[old] == desired[i]) pp[i] = pokemon.movePp[old];
  }
  for (uint8_t i = 0; i < 4; ++i) { pokemon.moves[i] = desired[i]; pokemon.movePp[i] = pp[i]; }
}
void changeStage(int8_t& stage, int8_t amount) { stage = std::max<int8_t>(-6, std::min<int8_t>(6, stage + amount)); }
void applyStageEffect(const char* effect, CombatVolatile& self, CombatVolatile& target) {
  const bool down = std::strstr(effect,"DOWN") != nullptr; CombatVolatile& v = down ? target : self;
  const int8_t amount = (std::strstr(effect,"_2") ? 2 : 1) * (down ? -1 : 1);
  if (std::strstr(effect,"SP_ATTACK")) changeStage(v.spAttackStage,amount);
  else if (std::strstr(effect,"SP_DEFENSE")) changeStage(v.spDefenseStage,amount);
  else if (std::strstr(effect,"ATTACK")) changeStage(v.attackStage,amount);
  else if (std::strstr(effect,"DEFENSE")) changeStage(v.defenseStage,amount);
  else if (std::strstr(effect,"SPEED")) changeStage(v.speedStage,amount);
  else if (std::strstr(effect,"ACCURACY")) changeStage(v.accuracyStage,amount);
  else if (std::strstr(effect,"EVASION")) changeStage(v.evasionStage,amount);
}
}

void EncounterLogic::advance(EncounterCharges& charges, uint32_t elapsedSeconds) {
  if (charges.available >= EncounterCharges::kMaximum) {
    charges.available = EncounterCharges::kMaximum;
    charges.rechargeProgressSeconds = 0;
    return;
  }
  const uint64_t total = static_cast<uint64_t>(charges.rechargeProgressSeconds) + elapsedSeconds;
  const uint32_t restored = static_cast<uint32_t>(total / EncounterCharges::kRechargeSeconds);
  charges.available = static_cast<uint8_t>(std::min<uint32_t>(EncounterCharges::kMaximum, charges.available + restored));
  charges.rechargeProgressSeconds = charges.available == EncounterCharges::kMaximum
      ? 0 : static_cast<uint32_t>(total % EncounterCharges::kRechargeSeconds);
}

bool EncounterLogic::consumeManualCharge(EncounterCharges& charges) {
  if (charges.available == 0) return false;
  --charges.available;
  return true;
}

uint32_t EncounterLogic::secondsUntilNext(const EncounterCharges& charges) {
  if (charges.available >= EncounterCharges::kMaximum) return 0;
  return EncounterCharges::kRechargeSeconds - charges.rechargeProgressSeconds;
}

void EncounterLogic::advanceWild(WildEncounterClock& clock, uint32_t elapsedSeconds) {
  if (clock.pending) return;
  clock.elapsedSeconds += elapsedSeconds;
  if (clock.elapsedSeconds >= clock.targetSeconds) clock.pending = true;
}

void EncounterLogic::acknowledgeWild(WildEncounterClock& clock) {
  uint32_t x = clock.rngState ? clock.rngState : 0x91E10DA5U;
  x ^= x << 13U; x ^= x >> 17U; x ^= x << 5U;
  clock.rngState = x; clock.elapsedSeconds = 0; clock.pending = false;
  clock.targetSeconds = WildEncounterClock::kMinimumSeconds + x % (WildEncounterClock::kWindowSeconds + 1U);
}

uint32_t BattleEngine::random(BattleState& battle) {
  uint32_t x = battle.rngState ? battle.rngState : 0xA341316CU;
  x ^= x << 13U; x ^= x >> 17U; x ^= x << 5U;
  battle.rngState = x;
  return x;
}
void BattleEngine::clear(BattleState& battle) {
  std::memset(&battle, 0, sizeof(battle));
  battle.rngState = 0xA341316CU; battle.trainerProfileId = 0xFF; battle.gymId = 0xFF;
}

uint8_t BattleEngine::highestPartyLevel(const PokemonCollection& collection) {
  uint8_t highest = 1;
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) {
    const OwnedPokemon* pokemon = CollectionLogic::find(collection, collection.party[slot]);
    if (pokemon) highest = std::max(highest, pokemon->level);
  }
  return highest;
}

OwnedPokemon* BattleEngine::currentOpponent(BattleState& battle) {
  return battle.opponentIndex < battle.opponentCount ? &battle.opponents[battle.opponentIndex] : nullptr;
}

const OwnedPokemon* BattleEngine::currentOpponent(const BattleState& battle) {
  return battle.opponentIndex < battle.opponentCount ? &battle.opponents[battle.opponentIndex] : nullptr;
}

bool BattleEngine::startWild(BattleState& battle, PokemonCollection& collection,
                             uint32_t playerUid, uint32_t seed) {
  if (battle.active || !CollectionLogic::validate(collection) || !selectablePartyMember(collection, playerUid)) return false;
  clear(battle);
  battle.active = true; battle.kind = BattleKind::Wild; battle.outcome = BattleOutcome::Ongoing;
  battle.playerUid = playerUid; battle.rngState = seed ? seed : 0xB5297A4DU;
  const uint8_t teamLevel = highestPartyLevel(collection);
  battle.rewardMoney = 0;  // Wild encounters grant XP/capture only.
  const uint16_t speciesId = chooseEncounterSpecies(teamLevel, random(battle));
  const uint8_t level = boundedLevel(static_cast<int16_t>(teamLevel) - 2 + random(battle) % 4U);
  battle.opponentCount = 1;
  battle.opponents[0] = CollectionLogic::createPokemon(0, speciesId, level);
  return battle.opponents[0].speciesId != 0;
}

bool BattleEngine::startTrainer(BattleState& battle, EncounterCharges& charges,
                                PokemonCollection& collection, uint32_t playerUid, uint32_t seed) {
  if (battle.active || !CollectionLogic::validate(collection) || !selectablePartyMember(collection, playerUid) ||
      charges.available == 0) return false;
  clear(battle);
  battle.active = true; battle.kind = BattleKind::Trainer; battle.outcome = BattleOutcome::Ongoing;
  battle.playerUid = playerUid; battle.rngState = seed ? seed : 0x68E31DA4U;
  battle.trainerProfileId = static_cast<uint8_t>(random(battle) % trainerProfileCount());
  const TrainerProfile* profile = trainerProfile(battle.trainerProfileId);
  if (!profile) return false;
  const uint8_t teamLevel = highestPartyLevel(collection);
  battle.rewardMoney = static_cast<uint32_t>(teamLevel) * 40U;
  battle.opponentCount = static_cast<uint8_t>(1U + random(battle) % std::min<uint8_t>(3, 1U + teamLevel / 15U));
  for (uint8_t index = 0; index < battle.opponentCount; ++index) {
    const uint16_t speciesId = profile->species[random(battle) % profile->speciesCount];
    const int16_t offset = static_cast<int16_t>(random(battle) % 3U) - 1;
    battle.opponents[index] = CollectionLogic::createPokemon(0, speciesId, boundedLevel(teamLevel + offset));
    if (!battle.opponents[index].speciesId) return false;
  }
  return EncounterLogic::consumeManualCharge(charges);
}

uint16_t BattleEngine::calculateDamage(BattleState& battle, const OwnedPokemon& attacker,
                                       const OwnedPokemon& defender, MoveId moveId,
                                       const CombatVolatile& attackerVolatile,
                                       const CombatVolatile& defenderVolatile) {
  const FullMoveData* move = findFullMove(moveId);
  const SpeciesData* attackerSpecies = findSpecies(attacker.speciesId);
  const SpeciesData* defenderSpecies = findSpecies(defender.speciesId);
  if (!move || !attackerSpecies || !defenderSpecies || move->power == 0) return 0;
  uint32_t accuracy = move->accuracy;
  if (accuracy && abilityIs(attacker, "COMPOUND EYES")) accuracy = accuracy * 130U / 100U;
  if (accuracy && abilityIs(attacker, "HUSTLE") && move->power) accuracy = accuracy * 80U / 100U;
  if (accuracy) accuracy = std::min<uint32_t>(100, staged(accuracy, attackerVolatile.accuracyStage - defenderVolatile.evasionStage));
  if (accuracy && random(battle) % 100U >= accuracy) return 0;
  if (std::strstr(move->effect, "OHKO")) return abilityIs(defender, "STURDY") ? 0 : defender.currentHp;
  if (std::strstr(move->effect, "DRAGON_RAGE")) return 40;
  if (std::strstr(move->effect, "SONIC_BOOM")) return 20;
  if (std::strstr(move->effect, "LEVEL_DAMAGE")) return attacker.level;
  if (std::strstr(move->effect, "HALF_HP")) return std::max<uint16_t>(1, defender.currentHp / 2U);
  const bool physical = move->type == PokemonType::Normal || move->type == PokemonType::Fighting || move->type == PokemonType::Flying || move->type == PokemonType::Poison || move->type == PokemonType::Ground || move->type == PokemonType::Rock || move->type == PokemonType::Bug || move->type == PokemonType::Ghost || move->type == PokemonType::Steel;
  uint32_t attack = staged(statValue(physical ? attackerSpecies->baseAttack : attackerSpecies->baseSpAttack, attacker.level), physical ? attackerVolatile.attackStage : attackerVolatile.spAttackStage);
  uint32_t defense = std::max<uint32_t>(1, staged(statValue(physical ? defenderSpecies->baseDefense : defenderSpecies->baseSpDefense, defender.level), physical ? defenderVolatile.defenseStage : defenderVolatile.spDefenseStage));
  if (physical && (abilityIs(attacker,"HUGE POWER") || abilityIs(attacker,"PURE POWER"))) attack *= 2U;
  if (physical && abilityIs(attacker,"HUSTLE")) attack = attack * 3U / 2U;
  if (physical && abilityIs(attacker,"GUTS") && attacker.status != StatusCondition::None) attack = attack * 3U / 2U;
  if (physical && abilityIs(defender,"MARVEL SCALE") && defender.status != StatusCondition::None) defense = defense * 3U / 2U;
  uint32_t damage = (((2U * attacker.level / 5U + 2U) * move->power * attack / defense) / 50U) + 2U;
  const uint8_t criticalChance = attackerVolatile.criticalStage ? 8U : 16U;
  if (!abilityIs(defender,"BATTLE ARMOR") && !abilityIs(defender,"SHELL ARMOR") &&
      random(battle) % criticalChance == 0) damage *= 2U;
  if (move->type == attackerSpecies->type1 || move->type == attackerSpecies->type2) damage = damage * 150U / 100U;
  const uint16_t effectiveness = typeMultiplier100(move->type, *defenderSpecies);
  if (!effectiveness) return 0;
  if ((move->type == PokemonType::Ground && abilityIs(defender,"LEVITATE")) ||
      (move->type == PokemonType::Fire && abilityIs(defender,"FLASH FIRE")) ||
      (move->type == PokemonType::Water && abilityIs(defender,"WATER ABSORB")) ||
      (move->type == PokemonType::Electric && abilityIs(defender,"VOLT ABSORB"))) return 0;
  if (abilityIs(defender,"WONDER GUARD") && effectiveness <= 100U) return 0;
  damage = damage * effectiveness / 100U;
  if (abilityIs(defender,"THICK FAT") && (move->type==PokemonType::Fire || move->type==PokemonType::Ice)) damage /= 2U;
  if (attacker.currentHp * 3U <= attacker.maximumHp &&
      ((abilityIs(attacker,"OVERGROW")&&move->type==PokemonType::Grass)||(abilityIs(attacker,"BLAZE")&&move->type==PokemonType::Fire)||(abilityIs(attacker,"TORRENT")&&move->type==PokemonType::Water)||(abilityIs(attacker,"SWARM")&&move->type==PokemonType::Bug))) damage=damage*3U/2U;
  damage = damage * (85U + random(battle) % 16U) / 100U;
  if (std::strstr(move->effect, "DOUBLE_HIT")) damage *= 2U;
  else if (std::strstr(move->effect, "MULTI_HIT")) damage *= 2U + random(battle) % 4U;
  return static_cast<uint16_t>(std::max<uint32_t>(1, damage));
}

void BattleEngine::enemyTurn(BattleState& battle, PokemonCollection& collection,
                             OwnedPokemon& player, BattleActionResult& result, uint8_t moveSlot) {
  OwnedPokemon* opponent = currentOpponent(battle);
  if (!battle.active || !opponent || opponent->currentHp == 0) return;
  CombatVolatile& enemyVolatile = battle.opponentVolatiles[battle.opponentIndex];
  if (enemyVolatile.recharging) { enemyVolatile.recharging=false; result.enemyActed=true; return; }
  if (battle.playerVolatile.protectedThisTurn) { result.enemyActed=true; return; }
  if (moveSlot >= kMoveSlots || opponent->moves[moveSlot] == MoveId::None || opponent->movePp[moveSlot] == 0) {
    uint8_t usable[kMoveSlots]{}, count = 0;
    for (uint8_t slot = 0; slot < kMoveSlots; ++slot)
      if (opponent->moves[slot] != MoveId::None && opponent->movePp[slot]) usable[count++] = slot;
    if (!count) { result.enemyActed = true; return; }
    moveSlot = usable[random(battle) % count];
  }
  const MoveId enemyMove = opponent->moves[moveSlot];
  const uint16_t damage = calculateDamage(battle, *opponent, player, enemyMove,
                                           battle.opponentVolatiles[battle.opponentIndex], battle.playerVolatile);
  --opponent->movePp[moveSlot];
  uint16_t appliedDamage=damage;
  if (battle.playerVolatile.substituteHp) { const uint16_t absorbed=std::min<uint16_t>(battle.playerVolatile.substituteHp,damage);battle.playerVolatile.substituteHp-=absorbed;appliedDamage=damage-absorbed; }
  player.currentHp = appliedDamage >= player.currentHp ? 0 : static_cast<uint16_t>(player.currentHp - appliedDamage);
  result.enemyActed = true; result.damageTaken = damage;
  applyMoveStatus(battle, *opponent, enemyMove, player, result);
  if (player.currentHp == 0) {
    player.recoverySecondsRemaining = 9000;
    OwnedPokemon* replacement = firstHealthyPartyMember(collection, player.uid);
    if (replacement) battle.playerUid = replacement->uid;
    else { battle.active = false; battle.outcome = BattleOutcome::Defeat; }
  }
}

void BattleEngine::applyMoveStatus(BattleState& battle, const OwnedPokemon& source, MoveId move, OwnedPokemon& target,
                                   BattleActionResult& result) {
  if (target.status != StatusCondition::None) return;
  const FullMoveData* data = findFullMove(move);
  if (!data) return;
  StatusCondition status = StatusCondition::None;
  if (std::strstr(data->effect, "TOXIC")) status = StatusCondition::BadlyPoisoned;
  else if (std::strstr(data->effect, "POISON")) status = StatusCondition::Poison;
  else if (std::strstr(data->effect, "PARALYZE")) status = StatusCondition::Paralysis;
  else if (std::strstr(data->effect, "BURN")) status = StatusCondition::Burn;
  else if (std::strstr(data->effect, "FREEZE")) status = StatusCondition::Frozen;
  else if (std::strstr(data->effect, "SLEEP")) status = StatusCondition::Sleep;
  if ((status==StatusCondition::Poison||status==StatusCondition::BadlyPoisoned)&&abilityIs(target,"IMMUNITY")) return;
  if (status==StatusCondition::Paralysis&&abilityIs(target,"LIMBER")) return;
  if (status==StatusCondition::Burn&&abilityIs(target,"WATER VEIL")) return;
  if (status==StatusCondition::Frozen&&abilityIs(target,"MAGMA ARMOR")) return;
  if (status==StatusCondition::Sleep&&(abilityIs(target,"INSOMNIA")||abilityIs(target,"VITAL SPIRIT"))) return;
  uint8_t chance = data->effectChance ? data->effectChance : (data->power == 0 ? 100 : 0);
  if (abilityIs(source, "SERENE GRACE")) chance = static_cast<uint8_t>(std::min<uint16_t>(100, chance * 2U));
  if (status != StatusCondition::None && random(battle) % 100U < chance) {
    target.status = status; result.statusApplied = true; result.appliedStatus = status;
  }
}

void BattleEngine::awardExperience(BattleState& battle, PokemonCollection& collection, BattleActionResult& result) {
  const OwnedPokemon* opponent = currentOpponent(battle);
  const SpeciesData* wildSpecies = opponent ? findSpecies(opponent->speciesId) : nullptr;
  if (!wildSpecies) return;
  uint8_t recipients = 0;
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) if (CollectionLogic::active(collection, slot)) ++recipients;
  if (!recipients) return;
  result.experienceGained = static_cast<uint16_t>(((wildSpecies->baseExperience * opponent->level) / 7U) / recipients);
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) {
  OwnedPokemon* recipient = CollectionLogic::active(collection, slot); if (!recipient) continue;
  OwnedPokemon& player = *recipient;
  const SpeciesData* playerSpecies = findSpecies(player.speciesId); if (!playerSpecies) continue;
  player.experience += result.experienceGained;
  while (player.level < 100 && player.experience >= experienceForLevel(playerSpecies->growthRate, player.level + 1)) {
    ++player.level;
    const uint16_t oldMax = player.maximumHp;
    OwnedPokemon recalculated = CollectionLogic::createPokemon(player.uid, player.speciesId, player.level);
    player.maximumHp = recalculated.maximumHp;
    player.currentHp = static_cast<uint16_t>(player.currentHp + player.maximumHp - oldMax);
    const EvolutionData* evolution = evolutionFor(player.speciesId);
    if (evolution && player.level >= evolution->level) {
      const uint16_t oldSpeciesId = player.speciesId;
      const uint16_t preservedHp = player.currentHp;
      const uint16_t preservedMaxHp = player.maximumHp;
      const uint32_t uid = player.uid;
      const uint32_t experience = player.experience;
      const uint8_t fullness = player.fullness, happiness = player.happiness;
      const StatusCondition status = player.status;
      const uint32_t recovery = player.recoverySecondsRemaining;
      player = CollectionLogic::createPokemon(uid, evolution->toSpeciesId, player.level);
      player.experience = experience; player.fullness = fullness; player.happiness = happiness;
      player.status = status; player.recoverySecondsRemaining = recovery;
      player.currentHp = static_cast<uint16_t>(std::min<uint32_t>(player.maximumHp,
          preservedHp + player.maximumHp - preservedMaxHp));
      result.evolved = true; result.evolvedSpeciesId = player.speciesId;
      if (result.evolvedCount < kPartyCapacity) { result.evolvedFromSpeciesIds[result.evolvedCount] = oldSpeciesId; result.evolvedSpeciesIds[result.evolvedCount++] = player.speciesId; }
      playerSpecies = findSpecies(player.speciesId);
      if (!playerSpecies) break;
    } else refreshLevelMoves(player);
  }
  }
}

BattleActionResult BattleEngine::fight(BattleState& battle, PokemonCollection& collection, uint8_t moveSlot) {
  BattleActionResult result;
  OwnedPokemon* player = CollectionLogic::find(collection, battle.playerUid);
  OwnedPokemon* opponent = currentOpponent(battle);
  if (!battle.active || !player || !opponent || moveSlot >= kMoveSlots || player->moves[moveSlot] == MoveId::None || player->movePp[moveSlot] == 0) return result;
  result.accepted = true; ++battle.turn;
  CombatVolatile& selfVolatile=battle.playerVolatile; CombatVolatile& targetVolatile=battle.opponentVolatiles[battle.opponentIndex];
  selfVolatile.protectedThisTurn=false;
  uint8_t enemyMoveSlot = 0xFF;
  for (uint8_t offset = 0; offset < kMoveSlots; ++offset) {
    const uint8_t candidate = static_cast<uint8_t>((random(battle) + offset) % kMoveSlots);
    if (opponent->moves[candidate] != MoveId::None && opponent->movePp[candidate]) { enemyMoveSlot = candidate; break; }
  }
  const FullMoveData* plannedPlayerMove = findFullMove(player->moves[moveSlot]);
  const FullMoveData* plannedEnemyMove = enemyMoveSlot < kMoveSlots ? findFullMove(opponent->moves[enemyMoveSlot]) : nullptr;
  const SpeciesData* playerSpecies = findSpecies(player->speciesId);
  const SpeciesData* opponentSpecies = findSpecies(opponent->speciesId);
  int32_t playerSpeed = playerSpecies ? staged(statValue(playerSpecies->baseSpeed, player->level), selfVolatile.speedStage) : 0;
  int32_t enemySpeed = opponentSpecies ? staged(statValue(opponentSpecies->baseSpeed, opponent->level), targetVolatile.speedStage) : 0;
  if (player->status == StatusCondition::Paralysis) playerSpeed /= 4;
  if (opponent->status == StatusCondition::Paralysis) enemySpeed /= 4;
  const int8_t playerPriority = plannedPlayerMove ? plannedPlayerMove->priority : 0;
  const int8_t enemyPriority = plannedEnemyMove ? plannedEnemyMove->priority : 0;
  const bool enemyFirst = plannedEnemyMove && (enemyPriority > playerPriority ||
      (enemyPriority == playerPriority && (enemySpeed > playerSpeed ||
       (enemySpeed == playerSpeed && (random(battle) & 1U)))));
  if (enemyFirst) {
    const uint32_t actingUid = player->uid;
    enemyTurn(battle, collection, *player, result, enemyMoveSlot);
    if (!battle.active || battle.playerUid != actingUid || player->currentHp == 0) { result.outcome = battle.outcome; return result; }
  }
  auto enemyIfNeeded = [&]() { if (!result.enemyActed) enemyTurn(battle, collection, *player, result, enemyMoveSlot); };
  if(selfVolatile.recharging){selfVolatile.recharging=false;enemyIfNeeded();result.outcome=battle.outcome;return result;}
  if(selfVolatile.flinched){selfVolatile.flinched=false;enemyIfNeeded();result.outcome=battle.outcome;return result;}
  if(selfVolatile.confusionTurns){--selfVolatile.confusionTurns;if(random(battle)%2U==0){const uint16_t hurt=std::max<uint16_t>(1,player->maximumHp/8U);player->currentHp=hurt>=player->currentHp?0:player->currentHp-hurt;enemyIfNeeded();result.outcome=battle.outcome;return result;}}
  if(selfVolatile.disabledMove==player->moves[moveSlot]) { result.outcome=battle.outcome; return result; }
  if(selfVolatile.encoreMove!=MoveId::None && selfVolatile.encoreMove!=player->moves[moveSlot]) { result.outcome=battle.outcome; return result; }
  if (player->status == StatusCondition::Sleep) {
    if (random(battle) % 3U) { enemyIfNeeded(); result.outcome = battle.outcome; return result; }
    player->status = StatusCondition::None;
  } else if (player->status == StatusCondition::Frozen) {
    if (random(battle) % 5U) { enemyIfNeeded(); result.outcome = battle.outcome; return result; }
    player->status = StatusCondition::None;
  } else if (player->status == StatusCondition::Paralysis && random(battle) % 4U == 0) {
    enemyIfNeeded(); result.outcome = battle.outcome; return result;
  }
  const bool releasingCharge = selfVolatile.chargingMove == player->moves[moveSlot];
  if (!releasingCharge) --player->movePp[moveSlot];
  const uint16_t damage = calculateDamage(battle, *player, *opponent, player->moves[moveSlot],
                                           battle.playerVolatile, battle.opponentVolatiles[battle.opponentIndex]);
  const FullMoveData* usedMove = findFullMove(player->moves[moveSlot]);
  if(usedMove && (std::strstr(usedMove->effect,"SEMI_INVULNERABLE")||std::strstr(usedMove->effect,"RAZOR_WIND")||std::strstr(usedMove->effect,"SOLAR_BEAM")) && !releasingCharge){selfVolatile.chargingMove=player->moves[moveSlot];enemyIfNeeded();result.outcome=battle.outcome;return result;}
  selfVolatile.chargingMove=MoveId::None;
  result.hit = damage != 0; result.damageDealt = damage;
  opponent->currentHp = damage >= opponent->currentHp ? 0 : static_cast<uint16_t>(opponent->currentHp - damage);
  if (usedMove && std::strstr(usedMove->effect, "ABSORB") && damage)
    player->currentHp = std::min<uint16_t>(player->maximumHp, static_cast<uint16_t>(player->currentHp + std::max<uint16_t>(1, damage / 2U)));
  if (usedMove && std::strstr(usedMove->effect, "RECOIL") && !std::strstr(usedMove->effect, "RECOIL_IF_MISS") && damage) {
    const uint16_t recoil = std::max<uint16_t>(1, damage / 4U); player->currentHp = recoil >= player->currentHp ? 0 : player->currentHp - recoil;
  }
  if (usedMove && usedMove->power == 0 && (std::strstr(usedMove->effect, "HEAL_HALF") || std::strstr(usedMove->effect, "REST"))) {
    player->currentHp = std::min<uint16_t>(player->maximumHp, static_cast<uint16_t>(player->currentHp + player->maximumHp / 2U));
    if (std::strstr(usedMove->effect, "REST")) player->status = StatusCondition::Sleep;
  }
  if(usedMove){
    applyStageEffect(usedMove->effect,selfVolatile,targetVolatile);
    if(std::strstr(usedMove->effect,"CONFUSION"))targetVolatile.confusionTurns=static_cast<uint8_t>(2U+random(battle)%4U);
    if(std::strstr(usedMove->effect,"FLINCH")&&usedMove->effectChance&&random(battle)%100U<usedMove->effectChance)targetVolatile.flinched=true;
    if(std::strstr(usedMove->effect,"TRAP"))targetVolatile.trappedTurns=static_cast<uint8_t>(2U+random(battle)%4U);
    if(std::strstr(usedMove->effect,"PROTECT")){selfVolatile.protectedThisTurn=true;}
    if(std::strstr(usedMove->effect,"SUBSTITUTE")&&!selfVolatile.substituteHp&&player->currentHp>player->maximumHp/4U){const uint16_t cost=player->maximumHp/4U;player->currentHp-=cost;selfVolatile.substituteHp=cost;}
    if(std::strstr(usedMove->effect,"RECHARGE"))selfVolatile.recharging=true;
    if(std::strstr(usedMove->effect,"DISABLE"))targetVolatile.disabledMove=opponent->moves[0];
    if(std::strstr(usedMove->effect,"ENCORE"))targetVolatile.encoreMove=opponent->moves[0];
  }
  applyMoveStatus(battle, *player, player->moves[moveSlot], *opponent, result);
  if (opponent->currentHp == 0) {
    result.opponentDefeated = true; awardExperience(battle, collection, result);
    if (battle.opponentIndex + 1U < battle.opponentCount) ++battle.opponentIndex;
    else { battle.active = false; battle.outcome = BattleOutcome::Victory; result.moneyGained = battle.rewardMoney; }
  } else if (!result.enemyActed) enemyTurn(battle, collection, *player, result, enemyMoveSlot);
  if (battle.active && (player->status == StatusCondition::Poison || player->status == StatusCondition::BadlyPoisoned || player->status == StatusCondition::Burn)) {
    const uint16_t residual = std::max<uint16_t>(1, player->maximumHp / 8U);
    player->currentHp = residual >= player->currentHp ? 0 : player->currentHp - residual;
    if (!player->currentHp) { player->recoverySecondsRemaining = 9000; if (OwnedPokemon* replacement = firstHealthyPartyMember(collection, player->uid)) battle.playerUid = replacement->uid; else { battle.active = false; battle.outcome = BattleOutcome::Defeat; } }
  }
  result.outcome = battle.outcome; return result;
}

BattleActionResult BattleEngine::run(BattleState& battle, PokemonCollection& collection) {
  BattleActionResult result;
  OwnedPokemon* player = CollectionLogic::find(collection, battle.playerUid);
  const SpeciesData* playerSpecies = player ? findSpecies(player->speciesId) : nullptr;
  const OwnedPokemon* opponent = currentOpponent(battle);
  const SpeciesData* wildSpecies = opponent ? findSpecies(opponent->speciesId) : nullptr;
  if (!battle.active || battle.kind != BattleKind::Wild || !player || !playerSpecies || !wildSpecies ||
      battle.playerVolatile.trappedTurns) return result;
  result.accepted = true; ++battle.turn;
  const uint16_t playerSpeed = statValue(playerSpecies->baseSpeed, player->level);
  const uint16_t wildSpeed = statValue(wildSpecies->baseSpeed, opponent->level);
  const uint16_t chance = std::min<uint16_t>(95, static_cast<uint16_t>(50 + (playerSpeed * 30U) / std::max<uint16_t>(1, wildSpeed)));
  if (random(battle) % 100U < chance) { battle.active = false; battle.outcome = BattleOutcome::Escaped; }
  else enemyTurn(battle, collection, *player, result);
  result.outcome = battle.outcome; return result;
}

BattleActionResult BattleEngine::switchPokemon(BattleState& battle, PokemonCollection& collection) {
  BattleActionResult result;
  OwnedPokemon* current = CollectionLogic::find(collection, battle.playerUid);
  if (!battle.active || !current || battle.playerVolatile.trappedTurns) return result;
  OwnedPokemon* replacement = nullptr;
  uint8_t currentSlot = 0;
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) if (collection.party[slot] == current->uid) currentSlot = slot;
  for (uint8_t offset = 1; offset < kPartyCapacity; ++offset) {
    OwnedPokemon* candidate = CollectionLogic::active(collection, static_cast<uint8_t>((currentSlot + offset) % kPartyCapacity));
    if (candidate && candidate->currentHp > 0 && candidate->recoverySecondsRemaining == 0) { replacement = candidate; break; }
  }
  if (!replacement) return result;
  result.accepted = true; ++battle.turn; battle.playerUid = replacement->uid;
  enemyTurn(battle, collection, *replacement, result);
  result.outcome = battle.outcome;
  return result;
}

BattleActionResult BattleEngine::switchToPokemon(BattleState& battle, PokemonCollection& collection, uint32_t uid) {
  BattleActionResult result;
  OwnedPokemon* current = CollectionLogic::find(collection, battle.playerUid);
  OwnedPokemon* replacement = CollectionLogic::find(collection, uid);
  if (!battle.active || !current || !replacement || replacement->uid == current->uid || battle.playerVolatile.trappedTurns ||
      !CollectionLogic::isInParty(collection, uid) || replacement->currentHp == 0 ||
      replacement->recoverySecondsRemaining) return result;
  if (abilityIs(*current, "NATURAL CURE")) current->status = StatusCondition::None;
  result.accepted = true; ++battle.turn; battle.playerUid = uid;
  battle.playerVolatile = CombatVolatile{};
  enemyTurn(battle, collection, *replacement, result);
  result.outcome = battle.outcome; return result;
}

BattleActionResult BattleEngine::throwBall(BattleState& battle, PokemonCollection& collection,
                                            Inventory& inventory, PokeBallType ball) {
  BattleActionResult result;
  const uint8_t index = static_cast<uint8_t>(ball);
  OwnedPokemon* player = CollectionLogic::find(collection, battle.playerUid);
  OwnedPokemon* opponent = currentOpponent(battle);
  const SpeciesData* species = opponent ? findSpecies(opponent->speciesId) : nullptr;
  if (!battle.active || battle.kind != BattleKind::Wild || !player || !opponent || !species || index >= static_cast<uint8_t>(PokeBallType::Count) ||
      inventory.balls[index] == 0 || CollectionLogic::count(collection) >= kBoxCapacity) return result;
  result.accepted = true; --inventory.balls[index]; ++battle.turn;
  uint32_t chance = ((3U * opponent->maximumHp - 2U * opponent->currentHp) * species->catchRate * ballMultiplier100(ball));
  chance /= std::max<uint32_t>(1, 3U * opponent->maximumHp * 100U);
  if (opponent->status != StatusCondition::None) chance = chance * 3U / 2U;
  chance = std::min<uint32_t>(255, chance);
  if (ball == PokeBallType::MasterBall || random(battle) % 256U < chance) {
    OwnedPokemon captured = *opponent; captured.currentHp = std::max<uint16_t>(1, captured.currentHp);
    result.caught = CollectionLogic::add(collection, captured);
    if (result.caught) { battle.active = false; battle.outcome = BattleOutcome::Captured; }
  }
  if (battle.active) enemyTurn(battle, collection, *player, result);
  result.outcome = battle.outcome; return result;
}

BattleActionResult BattleEngine::useItem(BattleState& battle, PokemonCollection& collection,
                                          Inventory& inventory, BattleItem item) {
  BattleActionResult result;
  OwnedPokemon* player = CollectionLogic::find(collection, battle.playerUid);
  const uint8_t index = static_cast<uint8_t>(item);
  if (!battle.active || !player || index >= static_cast<uint8_t>(BattleItem::Count) ||
      inventory.medicine[index] == 0) return result;

  bool useful = false;
  auto heal = [&](uint16_t amount) {
    if (player->currentHp < player->maximumHp) {
      player->currentHp = std::min<uint16_t>(player->maximumHp,
          static_cast<uint16_t>(player->currentHp + amount));
      useful = true;
    }
  };
  auto cure = [&](StatusCondition status) {
    if (player->status == status) { player->status = StatusCondition::None; useful = true; }
  };
  switch (item) {
    case BattleItem::Potion: heal(20); break;
    case BattleItem::SuperPotion: heal(50); break;
    case BattleItem::HyperPotion: heal(200); break;
    case BattleItem::FullHeal:
      if (player->status != StatusCondition::None) { player->status = StatusCondition::None; useful = true; }
      break;
    case BattleItem::Antidote:
      if (player->status == StatusCondition::Poison || player->status == StatusCondition::BadlyPoisoned) {
        player->status = StatusCondition::None; useful = true;
      }
      break;
    case BattleItem::ParalyzeHeal: cure(StatusCondition::Paralysis); break;
    case BattleItem::Awakening: cure(StatusCondition::Sleep); break;
    case BattleItem::BurnHeal: cure(StatusCondition::Burn); break;
    case BattleItem::IceHeal: cure(StatusCondition::Frozen); break;
    case BattleItem::XAttack: changeStage(battle.playerVolatile.attackStage, 1); useful = true; break;
    case BattleItem::XDefense: changeStage(battle.playerVolatile.defenseStage, 1); useful = true; break;
    case BattleItem::XSpeed: changeStage(battle.playerVolatile.speedStage, 1); useful = true; break;
    case BattleItem::XAccuracy: changeStage(battle.playerVolatile.accuracyStage, 1); useful = true; break;
    case BattleItem::DireHit:
      if (!battle.playerVolatile.criticalStage) { battle.playerVolatile.criticalStage = 1; useful = true; }
      break;
    case BattleItem::Count: break;
  }
  if (!useful) return result;
  --inventory.medicine[index]; ++battle.turn; result.accepted = true;
  enemyTurn(battle, collection, *player, result);
  result.outcome = battle.outcome;
  return result;
}

const char* BattleEngine::ballName(PokeBallType ball) {
  switch (ball) {
    case PokeBallType::PokeBall: return "POKE BALL";
    case PokeBallType::GreatBall: return "GREAT BALL";
    case PokeBallType::UltraBall: return "ULTRA BALL";
    case PokeBallType::MasterBall: return "MASTER BALL";
    case PokeBallType::Count: return "UNKNOWN";
  }
  return "UNKNOWN";
}

const char* BattleEngine::itemName(BattleItem item) {
  static const char* names[] = {"POTION","SUPER POTION","HYPER POTION","FULL HEAL","ANTIDOTE",
    "PARALYZE HEAL","AWAKENING","BURN HEAL","ICE HEAL","X ATTACK","X DEFENSE","X SPEED",
    "X ACCURACY","DIRE HIT"};
  const uint8_t index = static_cast<uint8_t>(item);
  return index < static_cast<uint8_t>(BattleItem::Count) ? names[index] : "UNKNOWN";
}
