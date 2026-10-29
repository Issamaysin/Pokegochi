#include "game/PokemonData.h"
#include "game/Collection.h"
#include "game/MegaEvolution.h"
#include "game/Pokedex.h"
#include <algorithm>

namespace {
#include "PokemonDataGenerated.inc"
#include "MoveDataGenerated.inc"
#include "MoveDescriptionsGenerated.inc"

// Pokemon Y ends at Light of Ruin (#617). Keep later signature moves beside,
// rather than forged into, the generated cartridge table. #618-621 are the
// four ORAS additions; Core Enforcer and Nihil Light are required by the
// authored Mega Zygarde form.
constexpr FullMoveData kPostXyMoves[] = {
    {618, "ORIGIN PULSE", PokemonType::Water, 110, 85,
     MoveCategory::Special, 10, 0, 0, "HIT",
     MoveTarget::Both, false, false, false, true, true, true},
    {619, "PRECIPICE BLADES", PokemonType::Ground, 120, 85,
     MoveCategory::Physical, 10, 0, 0, "HIT",
     MoveTarget::Both, false, false, false, true, true, true},
    {620, "DRAGON ASCENT", PokemonType::Flying, 120, 100,
     MoveCategory::Physical, 5, 0, 0, "CLOSE_COMBAT",
     MoveTarget::Selected, true, false, false, true, true, true},
    {621, "HYPERSPACE FURY", PokemonType::Dark, 100, 101,
     MoveCategory::Physical, 5, 0, 0, "HYPERSPACE_FURY",
     MoveTarget::Selected, false, false, false, false, true, true},
    {687, "CORE ENFORCER", PokemonType::Dragon, 100, 100,
     MoveCategory::Special, 10, 0, 0, "CORE_ENFORCER",
     MoveTarget::Both, false, false, false, true, true, true},
    {920, "NIHIL LIGHT", PokemonType::Dragon, 200, 100,
     MoveCategory::Special, 10, 0, 0, "NIHIL_LIGHT",
     MoveTarget::Both, false, false, false, false, true, false},
};

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
  const uint8_t evolutionGenerationLimit=std::min<uint8_t>(6U,unlockedGeneration);
  const auto generationFor=[](uint16_t id){
    return id<=151U?1U:id<=251U?2U:id<=386U?3U:id<=493U?4U:
        id<=649U?5U:6U;
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
  // The persisted picker has five compact slots, while Eevee has eight
  // destinations by Kalos. Put newly unlocked regional branches first so
  // Leafeon/Glaceon and later Sylveon can actually be selected; older forms
  // were already available throughout the preceding campaign.
  for(uint8_t generation=evolutionGenerationLimit;generation>=1U;--generation){
    for(const auto& evolution:kEvolutions){
      if(evolution.fromSpeciesId!=speciesId||
         generationFor(evolution.toSpeciesId)!=generation)continue;
      bool duplicate=false;
      for(uint8_t i=0;i<count;++i)if(output[i]==evolution.toSpeciesId)duplicate=true;
      if(!duplicate&&count<maximum)output[count++]=evolution.toSpeciesId;
    }
    if(generation==1U)break;
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
  for (const auto& move : kPostXyMoves) if (move.id == value) return &move;
  return nullptr;
}

const char* moveDescription(MoveId id) {
  switch (static_cast<uint16_t>(id)) {
    case 618U: return "BLASTS FOES WITH COUNTLESS BEAMS OF BLUE LIGHT.";
    case 619U: return "STRIKES FOES WITH BLADES OF EARTH.";
    case 620U: return "LOWERS THE USER'S DEFENSE AND SP. DEFENSE.";
    case 621U: return "BYPASSES PROTECTION, THEN LOWERS THE USER'S DEFENSE.";
    case 687U: return "SUPPRESSES A TARGET'S ABILITY IF IT ALREADY MOVED.";
    case 920U: return "IGNORES TARGET STAT CHANGES AND FAIRY IMMUNITY.";
    default: break;
  }
  return generatedMoveDescription(static_cast<uint16_t>(id));
}

bool isMoveAvailableInSingleBattle(MoveId id) {
  // These moves require a partner or, for Heal Pulse, can only benefit the
  // opposing target. None has a useful local role in Pokegochi's 1x1 battles.
  // Keep them in kFullMoves so imported/link data remains canonical, while
  // preventing them from occupying a local move slot.
  return id != MoveId::None && id != static_cast<MoveId>(266) &&
         id != static_cast<MoveId>(270) && id != static_cast<MoveId>(476) &&
         id != static_cast<MoveId>(502) && id != static_cast<MoveId>(505) &&
         id != static_cast<MoveId>(597) && id != static_cast<MoveId>(607);
}

uint8_t movesForLevel(uint16_t speciesId, uint8_t level, MoveId output[4],
                      uint8_t unlockedGeneration) {
  uint8_t count = 0;
  // A #387-493 species is intrinsically Platinum content. Older species keep
  // their FireRed learnset until the Sinnoh unlock is explicitly supplied.
  const uint8_t availableGeneration = std::min<uint8_t>(6U, unlockedGeneration);
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

struct FormLearnsetEntry { uint16_t move; uint8_t level; };
// Pokemon Y personal/learnset row 748 is female Meowstic. It is the only
// obtainable Kalos form whose level-up table differs from its National-Dex
// base row; keeping it compact here avoids importing the other 77 form rows.
constexpr FormLearnsetEntry kMeowsticFemaleLearnset[] = {
    {500,1},{382,1},{345,1},{10,1},{43,1},{343,5},{93,9},{113,13},
    {60,17},{252,19},{574,22},{473,25},{451,28},{247,31},{326,35},
    {94,40},{272,43},{324,45},{389,48},{248,50},{500,53},
};

// Platinum's Meteorites make every Deoxys form available in one save, and
// each form retains its own complete level-up row. Normal form already comes
// from the generated Platinum table; these are the three alternate rows.
constexpr FormLearnsetEntry kDeoxysAttackLearnset[] = {
    {43,1},{35,1},{101,9},{100,17},{269,25},{228,33},{94,41},
    {276,49},{375,57},{428,65},{322,73},{192,81},{354,89},{63,97},
};
constexpr FormLearnsetEntry kDeoxysDefenseLearnset[] = {
    {43,1},{35,1},{101,9},{100,17},{282,25},{191,33},{94,41},
    {289,49},{375,57},{428,65},{334,73},{133,73},{105,81},{354,89},
    {68,97},{243,97},
};
constexpr FormLearnsetEntry kDeoxysSpeedLearnset[] = {
    {43,1},{35,1},{101,9},{104,17},{282,25},{228,33},{94,41},
    {129,49},{375,57},{428,65},{97,73},{105,81},{354,89},{245,97},
};

// Wormadam permanently keeps the cloak Burmy wore at evolution. Sandy and
// Trash have independent Platinum level-up rows, not merely different art.
constexpr FormLearnsetEntry kWormadamSandyLearnset[] = {
    {33,1},{182,10},{450,15},{237,20},{93,23},{350,26},{106,29},
    {60,32},{445,35},{175,38},{213,41},{94,44},{90,47},
};
constexpr FormLearnsetEntry kWormadamTrashLearnset[] = {
    {33,1},{182,10},{450,15},{237,20},{93,23},{429,26},{319,29},
    {60,32},{445,35},{175,38},{213,41},{94,44},{442,47},
};

// Eternal Flower shares Floette's ordinary X/Y row and inserts its signature
// move at level 50, between Moonblast and Petal Dance.
constexpr FormLearnsetEntry kEternalFloetteLearnset[] = {
    {33,1},{22,1},{584,6},{381,10},{75,15},{273,20},{345,25},
    {580,27},{572,33},{312,38},{581,43},{585,46},{617,50},{80,51},
    {76,58},
};

// ORAS Hoopa Unbound level-up table. It is not present in Pokemon Y's GARC:
// unlike Confined, the Unbound form trades its Ghost-oriented rows for Knock
// Off/Dark Pulse and learns Hyperspace Fury both at level 1 and level 85.
constexpr FormLearnsetEntry kHoopaUnboundLearnset[] = {
    {621,1},{271,1},{194,1},{502,1},{93,1},{310,6},{277,10},{113,15},
    {60,19},{285,25},{471,29},{470,29},{282,46},{472,50},{433,50},
    {399,55},{94,75},{621,85},
};

uint8_t movesFromFormLearnset(const FormLearnsetEntry entries[],uint8_t size,
                              uint8_t level,MoveId output[4]){
  uint8_t count=0U;
  for(uint8_t i=0;i<size;++i){
    if(entries[i].level>level)continue;
    const MoveId move=static_cast<MoveId>(entries[i].move);
    if(!isMoveAvailableInSingleBattle(move))continue;
    bool duplicate=false;for(uint8_t slot=0;slot<count;++slot)
      if(output[slot]==move)duplicate=true;
    if(duplicate)continue;
    if(count<4U)output[count++]=move;
    else{output[0]=output[1];output[1]=output[2];output[2]=output[3];output[3]=move;}
  }
  return count;
}

uint8_t relearnableFromFormLearnset(const FormLearnsetEntry entries[],
                                    uint8_t size,uint8_t level,
                                    MoveId output[],uint8_t maximum){
  uint8_t count=0U;
  for(uint8_t i=0U;i<size&&count<maximum;++i){
    if(entries[i].level>level)continue;
    const MoveId move=static_cast<MoveId>(entries[i].move);
    if(!isMoveAvailableInSingleBattle(move))continue;
    bool duplicate=false;
    for(uint8_t slot=0U;slot<count;++slot)
      if(output[slot]==move){duplicate=true;break;}
    if(duplicate)continue;
    output[count++]=move;
  }
  return count;
}
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
  if(speciesId==413U&&form>0U&&unlockedGeneration>=4U){
    const FormLearnsetEntry* entries=form==1U?kWormadamSandyLearnset:
        kWormadamTrashLearnset;
    const uint8_t size=form==1U?
        static_cast<uint8_t>(sizeof(kWormadamSandyLearnset)/
                             sizeof(kWormadamSandyLearnset[0])):
        static_cast<uint8_t>(sizeof(kWormadamTrashLearnset)/
                             sizeof(kWormadamTrashLearnset[0]));
    return movesFromFormLearnset(entries,size,level,output);
  }
  if(speciesId==386U&&form>0U&&unlockedGeneration>=4U){
    const FormLearnsetEntry* entries=form==1U?kDeoxysAttackLearnset:
        form==2U?kDeoxysDefenseLearnset:kDeoxysSpeedLearnset;
    const uint8_t size=form==1U?
        static_cast<uint8_t>(sizeof(kDeoxysAttackLearnset)/sizeof(kDeoxysAttackLearnset[0])):
        form==2U?
        static_cast<uint8_t>(sizeof(kDeoxysDefenseLearnset)/sizeof(kDeoxysDefenseLearnset[0])):
        static_cast<uint8_t>(sizeof(kDeoxysSpeedLearnset)/sizeof(kDeoxysSpeedLearnset[0]));
    return movesFromFormLearnset(entries,size,level,output);
  }
  if(speciesId==670U&&form==5U&&unlockedGeneration>=6U)
    return movesFromFormLearnset(kEternalFloetteLearnset,
        static_cast<uint8_t>(sizeof(kEternalFloetteLearnset)/
                             sizeof(kEternalFloetteLearnset[0])),level,output);
  if(speciesId==678U&&form==1U&&unlockedGeneration>=6U)
    return movesFromFormLearnset(kMeowsticFemaleLearnset,
        static_cast<uint8_t>(sizeof(kMeowsticFemaleLearnset)/
                             sizeof(kMeowsticFemaleLearnset[0])),level,output);
  if(speciesId==720U&&form==1U&&unlockedGeneration>=6U)
    return movesFromFormLearnset(kHoopaUnboundLearnset,
        static_cast<uint8_t>(sizeof(kHoopaUnboundLearnset)/
                             sizeof(kHoopaUnboundLearnset[0])),level,output);
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
    const uint8_t availableGeneration = std::min<uint8_t>(6U, unlockedGeneration);
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
  if(speciesId==413U&&form>0U&&unlockedGeneration>=4U){
    const FormLearnsetEntry* entries=form==1U?kWormadamSandyLearnset:
        kWormadamTrashLearnset;
    const uint8_t size=form==1U?
        static_cast<uint8_t>(sizeof(kWormadamSandyLearnset)/
                             sizeof(kWormadamSandyLearnset[0])):
        static_cast<uint8_t>(sizeof(kWormadamTrashLearnset)/
                             sizeof(kWormadamTrashLearnset[0]));
    return relearnableFromFormLearnset(entries,size,level,output,maximum);
  }
  if(speciesId==386U&&form>0U&&unlockedGeneration>=4U){
    const FormLearnsetEntry* entries=form==1U?kDeoxysAttackLearnset:
        form==2U?kDeoxysDefenseLearnset:kDeoxysSpeedLearnset;
    const uint8_t size=form==1U?
        static_cast<uint8_t>(sizeof(kDeoxysAttackLearnset)/sizeof(kDeoxysAttackLearnset[0])):
        form==2U?
        static_cast<uint8_t>(sizeof(kDeoxysDefenseLearnset)/sizeof(kDeoxysDefenseLearnset[0])):
        static_cast<uint8_t>(sizeof(kDeoxysSpeedLearnset)/sizeof(kDeoxysSpeedLearnset[0]));
    return relearnableFromFormLearnset(entries,size,level,output,maximum);
  }
  uint8_t count = relearnableMovesForLine(
      speciesId==678U&&form==1U&&unlockedGeneration>=6U?677U:speciesId,
      level, output, maximum, unlockedGeneration);
  if(speciesId==670U&&form==5U&&unlockedGeneration>=6U&&level>=50U&&
     !hasMove(output,count,static_cast<MoveId>(617U))&&count<maximum)
    output[count++]=static_cast<MoveId>(617U);
  if(speciesId==678U&&form==1U&&unlockedGeneration>=6U){
    for(const FormLearnsetEntry& entry:kMeowsticFemaleLearnset){
      if(entry.level>level||count>=maximum)continue;
      const MoveId move=static_cast<MoveId>(entry.move);
      if(!isMoveAvailableInSingleBattle(move)||hasMove(output,count,move))continue;
      output[count++]=move;
    }
  }
  if(speciesId==720U&&form==1U&&unlockedGeneration>=6U){
    count=0U;
    for(const FormLearnsetEntry& entry:kHoopaUnboundLearnset){
      if(entry.level>level||count>=maximum)continue;
      const MoveId move=static_cast<MoveId>(entry.move);
      if(!isMoveAvailableInSingleBattle(move)||hasMove(output,count,move))continue;
      output[count++]=move;
    }
  }
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
  // ORAS and later signature tutors live outside Pokemon Y's generated GARC.
  // Summary is the project's event-move surface, just as it is for Keldeo,
  // Meloetta and Rotom above.
  MoveId laterSignature=MoveId::None;
  if(unlockedGeneration>=6U){
    if(speciesId==382U&&level>=45U)laterSignature=static_cast<MoveId>(618U);
    else if(speciesId==383U&&level>=45U)laterSignature=static_cast<MoveId>(619U);
    else if(speciesId==384U)laterSignature=static_cast<MoveId>(620U);
    else if(speciesId==718U)laterSignature=static_cast<MoveId>(687U);
  }
  if(laterSignature!=MoveId::None&&!hasMove(output,count,laterSignature)&&
     count<maximum)
    output[count++]=laterSignature;
  return count;
}

uint8_t relearnableMovesForPokemon(const OwnedPokemon& pokemon,
                                   MoveId output[],uint8_t maximum,
                                   uint8_t unlockedGeneration){
  uint8_t count=relearnableMovesForForm(
      pokemon.speciesId,CollectionLogic::resolvedForm(pokemon),pokemon.level,
      output,maximum,unlockedGeneration);
  if(pokemon.speciesId!=718U||
     MegaEvolution::variantFor(pokemon)!=MegaVariant::Mega)return count;
  const MoveId core=static_cast<MoveId>(687U);
  const MoveId nihil=static_cast<MoveId>(920U);
  for(uint8_t index=0U;index<count;++index){
    if(output[index]==core){output[index]=nihil;return count;}
  }
  if(count<maximum)output[count++]=nihil;
  return count;
}

MoveId moveLearnedAtLevel(uint16_t speciesId, uint8_t level,
                          uint8_t unlockedGeneration) {
  MoveId learned = MoveId::None;
  const uint8_t availableGeneration = std::min<uint8_t>(6U, unlockedGeneration);
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
  if(speciesId==413U&&form>0U&&unlockedGeneration>=4U){
    const FormLearnsetEntry* entries=form==1U?kWormadamSandyLearnset:
        kWormadamTrashLearnset;
    const uint8_t size=form==1U?
        static_cast<uint8_t>(sizeof(kWormadamSandyLearnset)/
                             sizeof(kWormadamSandyLearnset[0])):
        static_cast<uint8_t>(sizeof(kWormadamTrashLearnset)/
                             sizeof(kWormadamTrashLearnset[0]));
    MoveId learned=MoveId::None;
    for(uint8_t index=0U;index<size;++index)
      if(entries[index].level==level&&isMoveAvailableInSingleBattle(
          static_cast<MoveId>(entries[index].move)))
        learned=static_cast<MoveId>(entries[index].move);
    return learned;
  }
  if(speciesId==386U&&form>0U&&unlockedGeneration>=4U){
    const FormLearnsetEntry* entries=form==1U?kDeoxysAttackLearnset:
        form==2U?kDeoxysDefenseLearnset:kDeoxysSpeedLearnset;
    const uint8_t size=form==1U?
        static_cast<uint8_t>(sizeof(kDeoxysAttackLearnset)/sizeof(kDeoxysAttackLearnset[0])):
        form==2U?
        static_cast<uint8_t>(sizeof(kDeoxysDefenseLearnset)/sizeof(kDeoxysDefenseLearnset[0])):
        static_cast<uint8_t>(sizeof(kDeoxysSpeedLearnset)/sizeof(kDeoxysSpeedLearnset[0]));
    MoveId learned=MoveId::None;
    for(uint8_t index=0U;index<size;++index)
      if(entries[index].level==level&&isMoveAvailableInSingleBattle(
          static_cast<MoveId>(entries[index].move)))
        learned=static_cast<MoveId>(entries[index].move);
    return learned;
  }
  if(speciesId==670U&&form==5U&&unlockedGeneration>=6U){
    MoveId learned=MoveId::None;
    for(const FormLearnsetEntry& entry:kEternalFloetteLearnset)
      if(entry.level==level&&isMoveAvailableInSingleBattle(
          static_cast<MoveId>(entry.move)))learned=static_cast<MoveId>(entry.move);
    return learned;
  }
  if(speciesId==678U&&form==1U&&unlockedGeneration>=6U){
    MoveId learned=MoveId::None;
    for(const FormLearnsetEntry& entry:kMeowsticFemaleLearnset)
      if(entry.level==level&&isMoveAvailableInSingleBattle(
          static_cast<MoveId>(entry.move)))learned=static_cast<MoveId>(entry.move);
    return learned;
  }
  if(speciesId==720U&&form==1U&&unlockedGeneration>=6U){
    MoveId learned=MoveId::None;
    for(const FormLearnsetEntry& entry:kHoopaUnboundLearnset)
      if(entry.level==level&&isMoveAvailableInSingleBattle(
          static_cast<MoveId>(entry.move)))learned=static_cast<MoveId>(entry.move);
    return learned;
  }
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
