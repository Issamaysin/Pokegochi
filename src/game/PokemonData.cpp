#include "game/PokemonData.h"
#include "game/MegaEvolution.h"
#include <algorithm>

namespace {
#include "PokemonDataGenerated.inc"
#include "MoveDataGenerated.inc"
#include "MoveDescriptionsGenerated.inc"

bool isLegendaryEncounterSpecies(uint16_t speciesId) {
  return speciesId == 144 || speciesId == 145 || speciesId == 146 ||
         speciesId == 150 || speciesId == 151 ||
         (speciesId >= 243 && speciesId <= 245) || speciesId == 249 ||
         speciesId == 250 || speciesId == 251 ||
         (speciesId >= 377 && speciesId <= 386);
}

uint8_t effectiveEncounterUnlockLevel(uint16_t speciesId, uint8_t configuredLevel) {
  if (speciesId == 150) return std::max<uint8_t>(configuredLevel, 60U);
  if (isLegendaryEncounterSpecies(speciesId))
    return std::max<uint8_t>(configuredLevel, 50U);
  return configuredLevel;
}

constexpr MoveData kMoves[] = {
    {MoveId::Tackle, "TACKLE", PokemonType::Normal, 40, 100},
    {MoveId::VineWhip, "VINE WHIP", PokemonType::Grass, 45, 100},
    {MoveId::Scratch, "SCRATCH", PokemonType::Normal, 40, 100},
    {MoveId::Ember, "EMBER", PokemonType::Fire, 40, 100},
    {MoveId::WaterGun, "WATER GUN", PokemonType::Water, 40, 100},
    {MoveId::Gust, "GUST", PokemonType::Flying, 40, 100},
    {MoveId::StringShot, "STRING SHOT", PokemonType::Bug, 0, 95},
    {MoveId::PoisonSting, "POISON STING", PokemonType::Poison, 15, 100},
};
}

const SpeciesData* findSpecies(uint16_t id) {
  for (const auto& species : kSpecies) if (species.id == id) return &species;
  return nullptr;
}

const EvolutionData* evolutionFor(uint16_t speciesId) {
  for (const auto& evolution : kEvolutions) if (evolution.fromSpeciesId == speciesId) return &evolution;
  return nullptr;
}

uint8_t evolutionChoices(uint16_t speciesId,uint8_t level,uint16_t attack,uint16_t defense,
                         uint8_t unlockedGeneration,uint16_t output[],uint8_t maximum){
  if(!output||!maximum)return 0;
  // Tyrogue retains its original stat comparison at level 20.
  if(speciesId==236&&level>=20){
    output[0]=attack>defense?106:attack<defense?107:237;return 1;
  }
  // Every non-level method is adapted to a deterministic level in Pokegochi.
  // For a branching family, using each adapted threshold independently made
  // the first branch evolve automatically before later choices could unlock
  // (Poliwrath before Politoed, Slowbro before Slowking, and Gen-II Eevee only
  // seeing Espeon/Umbreon). The family now has one trigger: the earliest level
  // among destinations visible in the currently unlocked National Dex. Once
  // that trigger is reached, every visible destination is offered together.
  uint8_t familyLevel=255;
  for(const auto& evolution:kEvolutions){
    if(evolution.fromSpeciesId!=speciesId)continue;
    const uint8_t targetGeneration=evolution.toSpeciesId<=151?1:evolution.toSpeciesId<=251?2:3;
    if(targetGeneration<=unlockedGeneration)familyLevel=std::min(familyLevel,evolution.level);
  }
  if(familyLevel==255||level<familyLevel)return 0;
  // Nincada is not a branch choice: it becomes Ninjask and may create a
  // Shedinja as a second Pokemon. The collection operation is handled by the
  // level-up resolver, where Box capacity and identity can be checked safely.
  if(speciesId==290){output[0]=291;return 1;}
  uint8_t count=0;
  for(const auto& evolution:kEvolutions){
    if(evolution.fromSpeciesId!=speciesId)continue;
    const uint8_t targetGeneration=evolution.toSpeciesId<=151?1:evolution.toSpeciesId<=251?2:3;
    if(targetGeneration>unlockedGeneration)continue;
    bool duplicate=false;for(uint8_t i=0;i<count;++i)if(output[i]==evolution.toSpeciesId)duplicate=true;
    if(!duplicate&&count<maximum)output[count++]=evolution.toSpeciesId;
  }
  return count;
}

uint16_t chooseEncounterSpecies(uint8_t teamLevel, uint8_t unlockedGeneration, uint32_t roll) {
  uint32_t total = 0;
  for (const auto& entry : kEncounterSpecies)
    if (entry.generation <= unlockedGeneration &&
        effectiveEncounterUnlockLevel(entry.speciesId, entry.unlockLevel) <= teamLevel)
      total += entry.weight;
  if (!total) return 19;
  uint32_t selected = roll % total;
  for (const auto& entry : kEncounterSpecies) {
    if (entry.generation > unlockedGeneration ||
        effectiveEncounterUnlockLevel(entry.speciesId, entry.unlockLevel) > teamLevel)
      continue;
    if (selected < entry.weight) return entry.speciesId;
    selected -= entry.weight;
  }
  return 19;
}

uint8_t minimumEncounterLevel(uint16_t speciesId) {
  for (const auto& entry : kEncounterSpecies)
    if (entry.speciesId == speciesId)
      return std::max<uint8_t>(5U,
          effectiveEncounterUnlockLevel(entry.speciesId, entry.unlockLevel));
  return 5U;
}

uint8_t encounterWeight(uint16_t speciesId) {
  for (const auto& entry : kEncounterSpecies)
    if (entry.speciesId == speciesId) return entry.weight;
  return 0U; // Regional starters and their descendants are intentionally absent.
}
const AbilityData* findAbility(uint8_t id) {
  for (const auto& ability : kAbilities) if (ability.id == id) return &ability;
  return MegaEvolution::customAbility(id);
}

const MoveData* findMove(MoveId id) {
  for (const auto& move : kMoves) if (move.id == id) return &move;
  return nullptr;
}

const FullMoveData* findFullMove(MoveId id) {
  const uint16_t value = static_cast<uint16_t>(id);
  for (const auto& move : kFullMoves) if (move.id == value) return &move;
  return nullptr;
}

const char* moveDescription(MoveId id) {
  return generatedMoveDescription(static_cast<uint16_t>(id));
}

bool isMoveAvailableInSingleBattle(MoveId id) {
  // Follow Me (266) only redirects attacks away from an ally, and Helping
  // Hand (270) targets that ally. Neither has a useful/legal effect in a
  // singles battle. Keep both in kFullMoves so imported/link data remains
  // canonical, while preventing them from occupying a Pokegochi move slot.
  return id != MoveId::None && id != static_cast<MoveId>(266) &&
         id != static_cast<MoveId>(270);
}

uint8_t movesForLevel(uint16_t speciesId, uint8_t level, MoveId output[4]) {
  uint8_t count = 0;
  for (const auto& entry : kLearnsets) {
    if (entry.speciesId != speciesId || entry.level > level) continue;
    const MoveId move = static_cast<MoveId>(entry.moveId);
    if (!isMoveAvailableInSingleBattle(move)) continue;
    bool duplicate = false; for (uint8_t i = 0; i < count; ++i) if (output[i] == move) duplicate = true;
    if (duplicate) continue;
    if (count < 4) output[count++] = move;
    else { output[0]=output[1]; output[1]=output[2]; output[2]=output[3]; output[3]=move; }
  }
  return count;
}

namespace {
void collectRelearnableMoves(uint16_t speciesId, uint8_t level, MoveId output[],
                             uint8_t maximum, uint8_t& count,
                             bool visitedSpecies[387]) {
  if (!speciesId || speciesId > 386 || visitedSpecies[speciesId]) return;
  visitedSpecies[speciesId] = true;
  // Visit parents first so the pool follows the evolutionary line before
  // listing moves exclusive to the current form.
  for (const auto& evolution : kEvolutions)
    if (evolution.toSpeciesId == speciesId)
      collectRelearnableMoves(evolution.fromSpeciesId, level, output, maximum,
                              count, visitedSpecies);
  for (const auto& entry : kLearnsets) {
    if (entry.speciesId != speciesId || entry.level > level) continue;
    const MoveId move = static_cast<MoveId>(entry.moveId);
    if (!isMoveAvailableInSingleBattle(move)) continue;
    bool duplicate = false;
    for (uint8_t index = 0; index < count; ++index)
      if (output[index] == move) { duplicate = true; break; }
    if (!duplicate && count < maximum) output[count++] = move;
  }
}
}

uint8_t relearnableMovesForLine(uint16_t speciesId, uint8_t level,
                               MoveId output[], uint8_t maximum) {
  if (!output || !maximum) return 0;
  bool visitedSpecies[387]{};
  uint8_t count = 0;
  collectRelearnableMoves(speciesId, level, output, maximum, count, visitedSpecies);
  return count;
}

MoveId moveLearnedAtLevel(uint16_t speciesId, uint8_t level) {
  MoveId learned = MoveId::None;
  for (const auto& entry : kLearnsets) {
    const MoveId move = static_cast<MoveId>(entry.moveId);
    if (entry.speciesId == speciesId && entry.level == level &&
        isMoveAvailableInSingleBattle(move)) learned = move;
  }
  return learned;
}

MoveId defaultMoveForSpecies(uint16_t speciesId) {
  switch (speciesId) {
    case 1: return MoveId::VineWhip;
    case 4: return MoveId::Ember;
    case 7: return MoveId::WaterGun;
    case 10: return MoveId::Tackle;
    case 13: return MoveId::PoisonSting;
    case 16: return MoveId::Gust;
    case 19: return MoveId::Tackle;
    default: return MoveId::Tackle;
  }
}

uint32_t experienceForLevel(GrowthRate growth, uint8_t level) {
  const int64_t n = level;
  int64_t xp = n * n * n;
  if (growth == GrowthRate::Fast) xp = 4 * xp / 5;
  else if (growth == GrowthRate::Slow) xp = 5 * xp / 4;
  else if (growth == GrowthRate::MediumSlow) xp = (6 * xp) / 5 - 15 * n * n + 100 * n - 140;
  else if (growth == GrowthRate::Erratic) {
    if (n <= 50) xp = xp * (100 - n) / 50;
    else if (n <= 68) xp = xp * (150 - n) / 100;
    else if (n <= 98) xp = xp * ((1911 - 10 * n) / 3) / 500;
    else xp = xp * (160 - n) / 100;
  } else if (growth == GrowthRate::Fluctuating) {
    if (n <= 15) xp = xp * (((n + 1) / 3) + 24) / 50;
    else if (n <= 36) xp = xp * (n + 14) / 50;
    else xp = xp * ((n / 2) + 32) / 50;
  }
  if (xp < 0) xp = 0;
  return static_cast<uint32_t>(xp);
}
