#include "game/PokemonData.h"

namespace {
#include "PokemonDataGenerated.inc"
#include "MoveDataGenerated.inc"

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

uint16_t chooseEncounterSpecies(uint8_t teamLevel, uint32_t roll) {
  uint32_t total = 0;
  for (const auto& entry : kEncounterSpecies) if (entry.unlockLevel <= teamLevel + 3U) total += entry.weight;
  if (!total) return 19;
  uint32_t selected = roll % total;
  for (const auto& entry : kEncounterSpecies) {
    if (entry.unlockLevel > teamLevel + 3U) continue;
    if (selected < entry.weight) return entry.speciesId;
    selected -= entry.weight;
  }
  return 19;
}
const AbilityData* findAbility(uint8_t id) {
  for (const auto& ability : kAbilities) if (ability.id == id) return &ability;
  return nullptr;
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

uint8_t movesForLevel(uint16_t speciesId, uint8_t level, MoveId output[4]) {
  uint8_t count = 0;
  for (const auto& entry : kLearnsets) {
    if (entry.speciesId != speciesId || entry.level > level) continue;
    const MoveId move = static_cast<MoveId>(entry.moveId);
    bool duplicate = false; for (uint8_t i = 0; i < count; ++i) if (output[i] == move) duplicate = true;
    if (duplicate) continue;
    if (count < 4) output[count++] = move;
    else { output[0]=output[1]; output[1]=output[2]; output[2]=output[3]; output[3]=move; }
  }
  return count;
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
