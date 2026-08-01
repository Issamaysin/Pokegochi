#pragma once
#include <cstdint>

enum class PokemonType : uint8_t {
  Normal, Fighting, Flying, Poison, Ground, Rock, Bug, Ghost, Steel,
  Fire, Water, Grass, Electric, Psychic, Ice, Dragon, Dark
};
enum class GrowthRate : uint8_t { MediumFast, Erratic, Fluctuating, MediumSlow, Fast, Slow };

struct SpeciesData {
  uint16_t id;
  const char* name;
  PokemonType type1;
  PokemonType type2;
  uint8_t baseHp;
  uint8_t baseAttack;
  uint8_t baseDefense;
  uint8_t baseSpAttack;
  uint8_t baseSpDefense;
  uint8_t baseSpeed;
  uint8_t catchRate;
  uint8_t baseExperience;
  GrowthRate growthRate;
  uint8_t ability1;
  uint8_t ability2;
};
struct AbilityData { uint8_t id; const char* name; };

enum class MoveId : uint16_t { None=0, Scratch=10, Gust=16, VineWhip=22, Tackle=33, PoisonSting=40, Ember=52, WaterGun=55, StringShot=81 };

struct MoveData {
  MoveId id;
  const char* name;
  PokemonType type;
  uint8_t power;
  uint8_t accuracy;
};
struct FullMoveData { uint16_t id; const char* name; PokemonType type; uint8_t power; uint8_t accuracy; uint8_t pp; int8_t priority; uint8_t effectChance; const char* effect; bool makesContact; };
struct LearnsetEntry { uint16_t speciesId; uint8_t level; uint16_t moveId; };

struct EvolutionData { uint16_t fromSpeciesId; uint16_t toSpeciesId; uint8_t level; };
struct EncounterSpecies { uint16_t speciesId; uint8_t unlockLevel; uint8_t weight; };

const SpeciesData* findSpecies(uint16_t id);
const MoveData* findMove(MoveId id);
const FullMoveData* findFullMove(MoveId id);
uint8_t movesForLevel(uint16_t speciesId, uint8_t level, MoveId output[4]);
MoveId moveLearnedAtLevel(uint16_t speciesId, uint8_t level);
MoveId defaultMoveForSpecies(uint16_t speciesId);
uint32_t experienceForLevel(GrowthRate growth, uint8_t level);
const EvolutionData* evolutionFor(uint16_t speciesId);
uint16_t chooseEncounterSpecies(uint8_t teamLevel, uint32_t roll);
const AbilityData* findAbility(uint8_t id);
