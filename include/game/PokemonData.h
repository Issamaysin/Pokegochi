#pragma once
#include <cstdint>

enum class PokemonType : uint8_t {
  Normal, Fighting, Flying, Poison, Ground, Rock, Bug, Ghost, Steel,
  Fire, Water, Grass, Electric, Psychic, Ice, Dragon, Dark, Fairy
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
  // Generation-III effort-value yield granted when this species is defeated.
  // These values come directly from FireRed's species_info table.
  uint8_t evYieldHp;
  uint8_t evYieldAttack;
  uint8_t evYieldDefense;
  uint8_t evYieldSpAttack;
  uint8_t evYieldSpDefense;
  uint8_t evYieldSpeed;
  // Exact Generation-III threshold: 0=male only, 254=female only,
  // 255=genderless; otherwise personality's low byte selects the gender.
  uint8_t genderRatio;
};
struct AbilityData { uint8_t id; const char* name; const char* description; };

enum class MoveId : uint16_t { None=0, Scratch=10, Gust=16, VineWhip=22, Tackle=33, PoisonSting=40, Ember=52, WaterGun=55, StringShot=81, Struggle=165 };

// Values intentionally match FireRed's MOVE_TARGET_* flags. Keeping the
// original target is essential: a move used on the user/field must never be
// accuracy-checked against, redirected by or blocked by the opposing Pokemon.
enum class MoveTarget : uint8_t {
  Selected = 0,
  Depends = 1U << 0,
  UserOrSelected = 1U << 1,
  Random = 1U << 2,
  Both = 1U << 3,
  User = 1U << 4,
  FoesAndAlly = 1U << 5,
  OpponentsField = 1U << 6,
};

struct MoveData {
  MoveId id;
  const char* name;
  PokemonType type;
  uint8_t power;
  uint8_t accuracy;
};
struct FullMoveData {
  uint16_t id; const char* name; PokemonType type; uint8_t power; uint8_t accuracy;
  uint8_t pp; int8_t priority; uint8_t effectChance; const char* effect;
  MoveTarget target; bool makesContact;
  bool magicCoatAffected;
  bool snatchAffected;
  bool protectAffected;
  bool mirrorMoveAffected;
  bool kingsRockAffected;
};
struct LearnsetEntry { uint16_t speciesId; uint8_t level; uint16_t moveId; };

struct EvolutionData { uint16_t fromSpeciesId; uint16_t toSpeciesId; uint8_t level; };
struct EncounterSpecies { uint16_t speciesId; uint8_t unlockLevel; uint8_t weight; uint8_t generation; };

const SpeciesData* findSpecies(uint16_t id);
const MoveData* findMove(MoveId id);
const FullMoveData* findFullMove(MoveId id);
const char* moveDescription(MoveId id);
// Returns whether a move has a legal target in Pokegochi's strictly
// one-on-one battle format. Doubles-only moves remain in the canonical data
// and battle engine, but are not offered to players or selected by the AI.
bool isMoveAvailableInSingleBattle(MoveId id);
uint8_t movesForLevel(uint16_t speciesId, uint8_t level, MoveId output[4]);
// Returns every level-up move available to this species and each of its
// pre-evolutions up to the Pokemon's current level, without duplicates.
uint8_t relearnableMovesForLine(uint16_t speciesId, uint8_t level,
                               MoveId output[], uint8_t maximum);
MoveId moveLearnedAtLevel(uint16_t speciesId, uint8_t level);
MoveId defaultMoveForSpecies(uint16_t speciesId);
uint32_t experienceForLevel(GrowthRate growth, uint8_t level);
const EvolutionData* evolutionFor(uint16_t speciesId);
uint8_t evolutionChoices(uint16_t speciesId, uint8_t level, uint16_t attack, uint16_t defense,
                         uint8_t unlockedGeneration, uint16_t output[], uint8_t maximum);
uint16_t chooseEncounterSpecies(uint8_t teamLevel, uint8_t unlockedGeneration, uint32_t roll);
uint8_t minimumEncounterLevel(uint16_t speciesId);
uint8_t encounterWeight(uint16_t speciesId);
const AbilityData* findAbility(uint8_t id);
