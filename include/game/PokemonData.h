#pragma once
#include <cstdint>

enum class PokemonType : uint8_t {
  Normal, Fighting, Flying, Poison, Ground, Rock, Bug, Ghost, Steel,
  Fire, Water, Grass, Electric, Psychic, Ice, Dragon, Dark, Fairy
};
enum class GrowthRate : uint8_t { MediumFast, Erratic, Fluctuating, MediumSlow, Fast, Slow };

// Maximum number of destinations offered by any authored branching family.
// Shared by progression, battle results and the persistent evolution picker.
constexpr uint8_t kEvolutionChoiceCapacity = 5;

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
  uint16_t baseExperience;
  GrowthRate growthRate;
  // The first pair is the original FireRed/Gen-III identity. The second pair
  // is Platinum's ability pair and is enabled only once Sinnoh is unlocked.
  uint8_t ability1;
  uint8_t ability2;
  uint8_t sinnohAbility1;
  uint8_t sinnohAbility2;
  // White's normal pair and Hidden Ability. They become active only after
  // Unova is unlocked; hiddenAbility is selected solely for Pokemon obtained
  // from an authored Hidden-Ability encounter.
  uint8_t unovaAbility1;
  uint8_t unovaAbility2;
  uint8_t hiddenAbility;
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

struct OwnedPokemon;

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

// Gen IV introduced the per-move physical/special split. Every move through
// Sinnoh uses Platinum's category matched by canonical move ID, while Unova
// uses White's category byte. Never infer a move's category from its type.
enum class MoveCategory : uint8_t { Physical, Special, Status };

struct MoveData {
  MoveId id;
  const char* name;
  PokemonType type;
  uint8_t power;
  uint8_t accuracy;
};
struct FullMoveData {
  // Black/White stores 101 for moves whose accuracy is "--": they still
  // obey target reachability, but never run the ordinary Accuracy/Evasion
  // roll. Zero remains the equivalent sentinel in the older imported data.
  uint16_t id; const char* name; PokemonType type; uint8_t power; uint8_t accuracy;
  MoveCategory category; uint8_t pp; int8_t priority; uint8_t effectChance; const char* effect;
  MoveTarget target; bool makesContact;
  bool magicCoatAffected;
  bool snatchAffected;
  bool protectAffected;
  bool mirrorMoveAffected;
  bool kingsRockAffected;
};
struct LearnsetEntry {
  // National species through #721 and custom move rows through #920 need ten
  // bits each.
  // Levels fit in seven bits and the authored generation in three, so one
  // row fits exactly in a uint32_t without losing any X/Y data.
  uint32_t speciesId : 10;
  uint32_t moveId : 10;
  uint32_t level : 7;
  // 1 = the original FireRed/Hoenn-era learnset, 4 = Platinum additions.
  uint32_t generation : 3;
};
// This table has almost fifteen thousand rows after Kalos. Expanding it by a
// single byte costs more flash than many gameplay systems, so guard the
// packed representation at compile time.
static_assert(sizeof(LearnsetEntry) == 4, "LearnsetEntry must remain flash-packed");

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
uint8_t movesForLevel(uint16_t speciesId, uint8_t level, MoveId output[4],
                      uint8_t unlockedGeneration = 1);
// Form-aware level-up table. Most alternate forms share their base table;
// Deoxys, Shaymin-Sky, Black/White Kyurem, Eternal Floette, Meowstic-F and
// Hoopa Unbound have explicit replacements/additions.
uint8_t movesForForm(uint16_t speciesId, uint8_t form, uint8_t level,
                     MoveId output[4], uint8_t unlockedGeneration = 1);
// Returns every level-up move available to this species and each of its
// pre-evolutions up to the Pokemon's current level, without duplicates.
uint8_t relearnableMovesForLine(uint16_t speciesId, uint8_t level,
                               MoveId output[], uint8_t maximum,
                               uint8_t unlockedGeneration = 1);
// Form-aware Summary/move-reminder pool. This includes Platinum's Rotom and
// Deoxys rules plus authored Shaymin/Kyurem/Floette/Meowstic/Hoopa tables.
uint8_t relearnableMovesForForm(uint16_t speciesId, uint8_t form,
                                uint8_t level, MoveId output[], uint8_t maximum,
                                uint8_t unlockedGeneration = 1);
// Pokemon-aware counterpart for transformations whose move identity changes
// without changing the persistent species/form byte. All ordinary Megas keep
// their base pool; Mega Zygarde alone converts Core Enforcer to Nihil Light.
uint8_t relearnableMovesForPokemon(const OwnedPokemon& pokemon,
                                   MoveId output[], uint8_t maximum,
                                   uint8_t unlockedGeneration = 1);
// Rotom's appliance move is taught by changing form, not by a TM. Returning
// None for every other species keeps this table explicit and auditable.
MoveId formSignatureMove(uint16_t speciesId, uint8_t form);
MoveId moveLearnedAtLevel(uint16_t speciesId, uint8_t level,
                          uint8_t unlockedGeneration = 1);
// Form-aware counterpart used by battle XP and Rare Candy so a fused Kyurem
// learns the same move exposed by its Summary pool at levels 43 and 50.
MoveId moveLearnedAtLevelForForm(uint16_t speciesId, uint8_t form,
                                 uint8_t level,
                                 uint8_t unlockedGeneration = 1);
MoveId defaultMoveForSpecies(uint16_t speciesId);
uint32_t experienceForLevel(GrowthRate growth, uint8_t level);
const EvolutionData* evolutionFor(uint16_t speciesId);
uint8_t evolutionChoices(uint16_t speciesId, uint8_t level, uint16_t attack, uint16_t defense,
                         uint8_t unlockedGeneration, uint16_t output[], uint8_t maximum);
uint16_t chooseEncounterSpecies(uint8_t teamLevel, uint8_t unlockedGeneration, uint32_t roll);
uint8_t minimumEncounterLevel(uint16_t speciesId);
uint8_t encounterWeight(uint16_t speciesId);
const AbilityData* findAbility(uint8_t id);
