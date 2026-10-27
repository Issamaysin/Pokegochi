#include "game/PokemonData.h"
#include "game/MegaEvolution.h"
#include "game/Pokedex.h"
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
  // Sinnoh is unlocked only after the Mega challenge, which itself follows
  // the Hoenn campaign.  The battle state deliberately remains capped at
  // Gen III so it cannot alter Gym, Tower or Boss progression; once that
  // cap has been reached, evolution families are nevertheless allowed to
  // continue into their authentic Gen-IV members.
  // Gen-IV branches are Sinnoh content. Reaching Hoenn must not silently
  // expose them; the expedition raises the content generation to 4 only
  // after the player has opened Sinnoh.
  const uint8_t evolutionGenerationLimit=std::min<uint8_t>(5U,unlockedGeneration);
  const auto generationFor=[](uint16_t id){
    return id<=151U?1U:id<=251U?2U:id<=386U?3U:id<=493U?4U:5U;
  };
  uint8_t familyLevel=255;
  for(const auto& evolution:kEvolutions){
    if(evolution.fromSpeciesId!=speciesId)continue;
    const uint8_t targetGeneration=generationFor(evolution.toSpeciesId);
    if(targetGeneration<=evolutionGenerationLimit)familyLevel=std::min(familyLevel,evolution.level);
  }
  if(familyLevel==255||level<familyLevel)return 0;
  // Nincada is not a branch choice: it becomes Ninjask and may create a
  // Shedinja as a second Pokemon. The collection operation is handled by the
  // level-up resolver, where Box capacity and identity can be checked safely.
  if(speciesId==290){output[0]=291;return 1;}
  uint8_t count=0;
  for(const auto& evolution:kEvolutions){
    if(evolution.fromSpeciesId!=speciesId)continue;
    const uint8_t targetGeneration=generationFor(evolution.toSpeciesId);
    if(targetGeneration>evolutionGenerationLimit)continue;
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
  // These moves require a partner or, for Heal Pulse, can only benefit the
  // opposing target. None has a useful local role in Pokegochi's 1x1 battles.
  // Keep them in kFullMoves so imported/link data remains canonical, while
  // preventing them from occupying a local move slot.
  return id != MoveId::None && id != static_cast<MoveId>(266) &&
         id != static_cast<MoveId>(270) && id != static_cast<MoveId>(476) &&
         id != static_cast<MoveId>(502) && id != static_cast<MoveId>(505);
}

uint8_t movesForLevel(uint16_t speciesId, uint8_t level, MoveId output[4],
                      uint8_t unlockedGeneration) {
  uint8_t count = 0;
  // A #387-493 species is intrinsically Platinum content. Older species keep
  // their FireRed learnset until the Sinnoh unlock is explicitly supplied.
  const uint8_t availableGeneration = std::min<uint8_t>(5U, unlockedGeneration);
  for (const auto& entry : kLearnsets) {
    if (entry.speciesId != speciesId || entry.level > level ||
        entry.generation > availableGeneration) continue;
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
struct FormLevelMoveOverride {
  uint16_t speciesId;
  uint8_t form;
  uint8_t level;
  MoveId normalMove;
  MoveId formMove;
};

// Directly mirrored from the authored form learnsets. The form files for
// Giratina and the Rotom appliances keep the same level-up table as their base
// species; Rotom's appliance move is handled separately by
// formSignatureMove(), exactly like Pokemon_SetRotomForm in the reference ROM.
// Black/White Kyurem are the only Generation-V forms whose ordinary level-up
// table changes: B2W2 replaces Scary Face/Glaciate at levels 43/50.
constexpr FormLevelMoveOverride kFormLevelMoveOverrides[] = {
    {492, 1, 28, static_cast<MoveId>(235), static_cast<MoveId>(98)},  // Synthesis -> Quick Attack
    {492, 1, 64, static_cast<MoveId>(312), static_cast<MoveId>(403)},  // Aromatherapy -> Air Slash
    {492, 1, 91, static_cast<MoveId>(361), static_cast<MoveId>(437)},  // Healing Wish -> Leaf Storm
    {646, 1, 43, static_cast<MoveId>(184), static_cast<MoveId>(559)},  // Scary Face -> Fusion Bolt
    {646, 1, 50, static_cast<MoveId>(549), static_cast<MoveId>(553)},  // Glaciate -> Freeze Shock
    {646, 2, 43, static_cast<MoveId>(184), static_cast<MoveId>(558)},  // Scary Face -> Fusion Flare
    {646, 2, 50, static_cast<MoveId>(549), static_cast<MoveId>(554)},  // Glaciate -> Ice Burn
};
}

MoveId formSignatureMove(uint16_t speciesId, uint8_t form) {
  if (speciesId != 479U || form == 0U || form > 5U) return MoveId::None;
  // Platinum appliance order: Heat, Wash, Frost, Fan, Mow.
  static constexpr uint16_t kRotomMoves[] = {0, 315, 56, 59, 403, 437};
  return static_cast<MoveId>(kRotomMoves[form]);
}

namespace {
template <typename Replacer>
void applyFormLevelMoves(MoveId output[], uint8_t count, uint8_t form,
                         uint16_t speciesId, Replacer&& replacer) {
  for (const auto& override : kFormLevelMoveOverrides) {
    if (override.speciesId != speciesId || override.form != form) continue;
    for (uint8_t index = 0; index < count; ++index) {
      if (output[index] == override.normalMove) {
        output[index] = override.formMove;
        break;
      }
    }
    replacer(override);
  }
}

bool hasMove(const MoveId output[], uint8_t count, MoveId move) {
  for (uint8_t index = 0; index < count; ++index)
    if (output[index] == move) return true;
  return false;
}
}

uint8_t movesForForm(uint16_t speciesId, uint8_t form, uint8_t level,
                     MoveId output[4], uint8_t unlockedGeneration) {
  if (!output) return 0;
  const uint8_t count = movesForLevel(speciesId, level, output, unlockedGeneration);
  applyFormLevelMoves(output, count, form, speciesId, [](const auto&) {});
  return count;
}

namespace {
void collectRelearnableMoves(uint16_t speciesId, uint8_t level, MoveId output[],
                             uint8_t maximum, uint8_t& count,
                             bool visitedSpecies[kPokedexSpeciesCount + 1],
                             uint8_t unlockedGeneration) {
  if (!speciesId || speciesId > kPokedexSpeciesCount || visitedSpecies[speciesId]) return;
  visitedSpecies[speciesId] = true;
  // Visit parents first so the pool follows the evolutionary line before
  // listing moves exclusive to the current form.
  for (const auto& evolution : kEvolutions)
    if (evolution.toSpeciesId == speciesId)
      collectRelearnableMoves(evolution.fromSpeciesId, level, output, maximum,
                              count, visitedSpecies, unlockedGeneration);
  for (const auto& entry : kLearnsets) {
    const uint8_t availableGeneration = std::min<uint8_t>(5U, unlockedGeneration);
    if (entry.speciesId != speciesId || entry.level > level ||
        entry.generation > availableGeneration) continue;
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
                               MoveId output[], uint8_t maximum,
                               uint8_t unlockedGeneration) {
  if (!output || !maximum) return 0;
  bool visitedSpecies[kPokedexSpeciesCount + 1]{};
  uint8_t count = 0;
  collectRelearnableMoves(speciesId, level, output, maximum, count,
                          visitedSpecies, unlockedGeneration);
  return count;
}

uint8_t relearnableMovesForForm(uint16_t speciesId, uint8_t form,
                                uint8_t level, MoveId output[], uint8_t maximum,
                                uint8_t unlockedGeneration) {
  if (!output || !maximum) return 0;
  uint8_t count = relearnableMovesForLine(speciesId, level, output, maximum,
                                          unlockedGeneration);
  applyFormLevelMoves(output, count, form, speciesId, [&](const auto& override) {
    if (override.level > level) return;
    if (hasMove(output, count, override.formMove)) return;
    if (!hasMove(output, count, override.normalMove) && count < maximum)
      output[count++] = override.formMove;
  });
  const MoveId signature = formSignatureMove(speciesId, form);
  if (signature != MoveId::None && !hasMove(output, count, signature) && count < maximum)
    output[count++] = signature;
  // In Black/White 2 these are taught by dedicated NPC events rather than
  // appearing in either species' ordinary level-up table.  Summary is
  // Pokegochi's move-relearner/event-tutor surface, so expose them there only
  // after Unova itself has unlocked.  Keldeo's resolved form then follows
  // Secret Sword, while Relic Song changes Meloetta only during battle.
  const MoveId eventMove = unlockedGeneration >= 5U
      ? speciesId == 647U ? static_cast<MoveId>(548U)   // Secret Sword
      : speciesId == 648U ? static_cast<MoveId>(547U)  // Relic Song
                          : MoveId::None
      : MoveId::None;
  if (eventMove != MoveId::None && !hasMove(output, count, eventMove) &&
      count < maximum)
    output[count++] = eventMove;
  return count;
}

MoveId moveLearnedAtLevel(uint16_t speciesId, uint8_t level,
                          uint8_t unlockedGeneration) {
  MoveId learned = MoveId::None;
  const uint8_t availableGeneration = std::min<uint8_t>(5U, unlockedGeneration);
  for (const auto& entry : kLearnsets) {
    const MoveId move = static_cast<MoveId>(entry.moveId);
    if (entry.speciesId == speciesId && entry.level == level &&
        entry.generation <= availableGeneration &&
        isMoveAvailableInSingleBattle(move)) learned = move;
  }
  return learned;
}

MoveId moveLearnedAtLevelForForm(uint16_t speciesId, uint8_t form,
                                 uint8_t level,
                                 uint8_t unlockedGeneration) {
  MoveId learned = moveLearnedAtLevel(speciesId, level, unlockedGeneration);
  for (const auto& override : kFormLevelMoveOverrides) {
    if (override.speciesId == speciesId && override.form == form &&
        override.level == level && learned == override.normalMove)
      return override.formMove;
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
