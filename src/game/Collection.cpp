#include "game/Collection.h"
#include "game/MegaEvolution.h"
#include <algorithm>

namespace {
uint32_t mixed(uint32_t value) {
  value ^= value >> 16U;
  value *= 0x7FEB352DU;
  value ^= value >> 15U;
  value *= 0x846CA68BU;
  value ^= value >> 16U;
  return value ? value : 0xA341316CU;
}

uint32_t nextValue(uint32_t& state) {
  state ^= state << 13U;
  state ^= state >> 17U;
  state ^= state << 5U;
  return state;
}

uint8_t baseForStat(const SpeciesData& species, PokemonStat stat) {
  switch (stat) {
    case PokemonStat::Attack: return species.baseAttack;
    case PokemonStat::Defense: return species.baseDefense;
    case PokemonStat::Speed: return species.baseSpeed;
    case PokemonStat::SpAttack: return species.baseSpAttack;
    case PokemonStat::SpDefense: return species.baseSpDefense;
  }
  return 0;
}

uint8_t deoxysBaseForStat(uint8_t form, PokemonStat stat) {
  // Generation-III form stats. HP is 50 for all four forms and is handled by
  // calculatedMaximumHp(); the remaining order is Normal, Attack, Defense,
  // Speed. These are the actual R/S, FR, LG and Emerald distributions.
  // Columns follow PokemonStat exactly: Attack, Defense, Speed, SpAttack,
  // SpDefense. Keeping the source-data order here used to swap the last three
  // stats, making Attack form as fast as Speed form and corrupting both
  // special stats.
  static constexpr uint8_t kStats[4][5] = {
      {150,  50, 150, 150,  50},
      {180,  20, 150, 180,  20},
      { 70, 160,  90,  70, 160},
      { 95,  90, 180,  95,  90},
  };
  const uint8_t index = static_cast<uint8_t>(stat);
  return kStats[std::min<uint8_t>(form, 3U)][std::min<uint8_t>(index, 4U)];
}

uint8_t ivForStat(const IndividualValues& ivs, PokemonStat stat) {
  switch (stat) {
    case PokemonStat::Attack: return ivs.attack;
    case PokemonStat::Defense: return ivs.defense;
    case PokemonStat::Speed: return ivs.speed;
    case PokemonStat::SpAttack: return ivs.spAttack;
    case PokemonStat::SpDefense: return ivs.spDefense;
  }
  return 0;
}

uint8_t evForStat(const EffortValues& evs, PokemonStat stat) {
  switch (stat) {
    case PokemonStat::Attack: return evs.attack;
    case PokemonStat::Defense: return evs.defense;
    case PokemonStat::Speed: return evs.speed;
    case PokemonStat::SpAttack: return evs.spAttack;
    case PokemonStat::SpDefense: return evs.spDefense;
  }
  return 0;
}

// Platinum carries one authored Arceus sprite family for each of the sixteen
// original Plates. Keep this table in the same order as the exported form
// folders so every UI that asks for resolvedForm() gets the exact same asset.
// The project also has a Fairy/Pixie Plate, but Platinum predates Fairy and
// therefore has no Fairy Arceus art; that extension intentionally uses the
// canonical Normal sprite while MegaEvolution still resolves its Fairy type.
uint8_t arceusFormForType(PokemonType type) {
  switch (type) {
    case PokemonType::Fighting: return 1U;
    case PokemonType::Flying: return 2U;
    case PokemonType::Poison: return 3U;
    case PokemonType::Rock: return 4U;
    case PokemonType::Bug: return 5U;
    case PokemonType::Ghost: return 6U;
    case PokemonType::Dragon: return 7U;
    case PokemonType::Steel: return 8U;
    case PokemonType::Fire: return 9U;
    case PokemonType::Water: return 10U;
    case PokemonType::Grass: return 11U;
    case PokemonType::Electric: return 12U;
    case PokemonType::Psychic: return 13U;
    case PokemonType::Ice: return 14U;
    case PokemonType::Ground: return 15U;
    case PokemonType::Dark: return 16U;
    case PokemonType::Normal:
    case PokemonType::Fairy:
      return 0U;
  }
  return 0U;
}

bool isRotomFormMove(MoveId move) {
  switch (static_cast<uint16_t>(move)) {
    case 315: // Overheat
    case 56:  // Hydro Pump
    case 59:  // Blizzard
    case 403: // Air Slash
    case 437: // Leaf Storm
      return true;
    default:
      return false;
  }
}

void setMovePpUps(OwnedPokemon& pokemon, uint8_t slot, uint8_t uses) {
  if (slot >= kMoveSlots) return;
  const uint32_t mask = 0x03U << (slot * 2U);
  pokemon.legacyCareCounterReserved =
      (pokemon.legacyCareCounterReserved & ~mask) |
      ((static_cast<uint32_t>(uses) & 0x03U) << (slot * 2U));
}

// The reference Platinum implementation removes the old appliance move,
// inserts the new one into an empty slot, and otherwise asks the player which
// move to replace. The Pokegochi form picker has no second dialog, so a full
// set keeps its four trained moves and exposes the appliance move through the
// Summary relearn pool instead of silently deleting a player's move.
void synchronizeRotomFormMove(OwnedPokemon& pokemon, uint8_t form) {
  if (pokemon.speciesId != 479U) return;
  const MoveId desired = formSignatureMove(pokemon.speciesId, form);
  bool installed = false;
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    if (!isRotomFormMove(pokemon.moves[slot])) continue;
    if (desired != MoveId::None && !installed) {
      pokemon.moves[slot] = desired;
      const FullMoveData* move = findFullMove(desired);
      pokemon.movePp[slot] = move ? move->pp : 0;
      setMovePpUps(pokemon, slot, 0);
      installed = true;
      continue;
    }
    for (uint8_t next = slot; next + 1U < kMoveSlots; ++next) {
      pokemon.moves[next] = pokemon.moves[next + 1U];
      pokemon.movePp[next] = pokemon.movePp[next + 1U];
      setMovePpUps(pokemon, next, CollectionLogic::ppUpCount(pokemon, next + 1U));
    }
    pokemon.moves[kMoveSlots - 1U] = MoveId::None;
    pokemon.movePp[kMoveSlots - 1U] = 0;
    setMovePpUps(pokemon, kMoveSlots - 1U, 0);
    --slot;
  }
  if (desired == MoveId::None || installed) return;
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    if (pokemon.moves[slot] != MoveId::None) continue;
    pokemon.moves[slot] = desired;
    const FullMoveData* move = findFullMove(desired);
    pokemon.movePp[slot] = move ? move->pp : 0;
    setMovePpUps(pokemon, slot, 0);
    return;
  }
}
}

OwnedPokemon CollectionLogic::createPokemon(uint32_t uid, uint16_t speciesId, uint8_t level,
                                             bool shiny, uint32_t personalitySeed,
                                             uint8_t unlockedGeneration) {
  OwnedPokemon pokemon;
  const SpeciesData* species = findSpecies(speciesId);
  if (!species) return pokemon;
  pokemon.uid = uid;
  const uint32_t fallbackSeed = 0x9E3779B9U ^ (uid * 0x85EBCA6BU) ^
                                (static_cast<uint32_t>(speciesId) << 8U) ^ level;
  pokemon.personality = mixed(personalitySeed ? personalitySeed : fallbackSeed);
  uint32_t ivState = mixed(pokemon.personality ^ 0xC2B2AE35U);
  pokemon.ivs.hp = static_cast<uint8_t>(nextValue(ivState) & 31U);
  pokemon.ivs.attack = static_cast<uint8_t>(nextValue(ivState) & 31U);
  pokemon.ivs.defense = static_cast<uint8_t>(nextValue(ivState) & 31U);
  pokemon.ivs.spAttack = static_cast<uint8_t>(nextValue(ivState) & 31U);
  pokemon.ivs.spDefense = static_cast<uint8_t>(nextValue(ivState) & 31U);
  pokemon.ivs.speed = static_cast<uint8_t>(nextValue(ivState) & 31U);
  pokemon.nature = static_cast<PokemonNature>(pokemon.personality % 25U);
  pokemon.speciesId = speciesId;
  if (speciesId == 201) {
    // Generation III derives the Unown letter from these personality bits.
    // Store the resolved result as well so every renderer (and Transform)
    // carries the exact encountered form instead of falling back to A.
    const uint32_t personality = pokemon.personality;
    pokemon.form = static_cast<uint8_t>((((personality & 0x03000000U) >> 18U) |
                                         ((personality & 0x00030000U) >> 12U) |
                                         ((personality & 0x00000300U) >> 6U) |
                                          (personality & 0x00000003U)) % 28U);
  }
  // Basculin's stripe is stable per individual, but it must not reuse the
  // low personality bit which already selects Ability slot 1/2. Keeping the
  // two rolls independent lets both stripes legitimately have Adaptability,
  // while Red can roll Reckless and Blue can roll Rock Head. Deerling and
  // Sawsbuck start in Spring and follow the world-calendar season at runtime.
  if (speciesId == 550U)
    pokemon.form=static_cast<uint8_t>((pokemon.personality>>1U)&1U);
  pokemon.shiny = shiny;
  pokemon.level = level;
  pokemon.experience = experienceForLevel(species->growthRate, level);
  pokemon.maximumHp = calculatedMaximumHp(pokemon);
  pokemon.currentHp = pokemon.maximumHp;
  const uint8_t contentGeneration = std::min<uint8_t>(5U, unlockedGeneration);
  refreshAbility(pokemon, contentGeneration);
  const uint8_t moveCount = movesForForm(speciesId, pokemon.form, level, pokemon.moves,
                                          contentGeneration);
  if (!moveCount) pokemon.moves[0] = defaultMoveForSpecies(speciesId);
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    const FullMoveData* move = findFullMove(pokemon.moves[slot]);
    pokemon.movePp[slot] = move ? move->pp : 0;
  }
  return pokemon;
}

bool CollectionLogic::ensureUsableMoves(OwnedPokemon& pokemon) {
  bool changed = false;
  uint8_t validCount = 0;
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    if (pokemon.moves[slot] == MoveId::None) {
      if (pokemon.movePp[slot] != 0) { pokemon.movePp[slot] = 0; changed = true; }
      if (ppUpCount(pokemon, slot)) { clearMovePpUps(pokemon, slot); changed = true; }
      continue;
    }
    const FullMoveData* move = findFullMove(pokemon.moves[slot]);
    if (!move || !isMoveAvailableInSingleBattle(pokemon.moves[slot])) {
      pokemon.moves[slot] = MoveId::None;
      pokemon.movePp[slot] = 0;
      clearMovePpUps(pokemon, slot);
      changed = true;
      continue;
    }
    ++validCount;
    const uint8_t maximumPp = maximumMovePp(pokemon, slot);
    if (pokemon.movePp[slot] > maximumPp) {
      pokemon.movePp[slot] = maximumPp;
      changed = true;
    }
  }
  if (validCount) return changed;

  MoveId recovered[kMoveSlots]{};
  const uint8_t moveCount = movesForForm(pokemon.speciesId, resolvedForm(pokemon),
                                         pokemon.level, recovered);
  if (!moveCount) recovered[0] = defaultMoveForSpecies(pokemon.speciesId);
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    clearMovePpUps(pokemon, slot);
    pokemon.moves[slot] = recovered[slot];
    const FullMoveData* move = findFullMove(recovered[slot]);
    pokemon.movePp[slot] = move ? move->pp : 0;
  }
  return true;
}

uint8_t CollectionLogic::ppUpCount(const OwnedPokemon& pokemon, uint8_t slot) {
  if (slot >= kMoveSlots) return 0;
  return static_cast<uint8_t>((pokemon.legacyCareCounterReserved >> (slot * 2U)) & 0x03U);
}

uint8_t CollectionLogic::maximumMovePp(const OwnedPokemon& pokemon, uint8_t slot) {
  if (slot >= kMoveSlots) return 0;
  const FullMoveData* move = findFullMove(pokemon.moves[slot]);
  if (!move) return 0;
  const uint16_t maximum = static_cast<uint16_t>(move->pp) +
      static_cast<uint16_t>(move->pp) * ppUpCount(pokemon, slot) / 5U;
  return static_cast<uint8_t>(std::min<uint16_t>(maximum, 255U));
}

bool CollectionLogic::applyPpUp(OwnedPokemon& pokemon, uint8_t slot) {
  if (slot >= kMoveSlots || pokemon.moves[slot] == MoveId::None) return false;
  const FullMoveData* move = findFullMove(pokemon.moves[slot]);
  const uint8_t uses = ppUpCount(pokemon, slot);
  if (!move || !move->pp || uses >= 3U) return false;
  const uint8_t previousMaximum = maximumMovePp(pokemon, slot);
  const uint32_t shift = static_cast<uint32_t>(slot) * 2U;
  pokemon.legacyCareCounterReserved =
      (pokemon.legacyCareCounterReserved & ~(0x03UL << shift)) |
      (static_cast<uint32_t>(uses + 1U) << shift);
  const uint8_t increasedMaximum = maximumMovePp(pokemon, slot);
  pokemon.movePp[slot] = static_cast<uint8_t>(std::min<uint16_t>(
      increasedMaximum, static_cast<uint16_t>(pokemon.movePp[slot]) +
                            increasedMaximum - previousMaximum));
  return true;
}

void CollectionLogic::clearMovePpUps(OwnedPokemon& pokemon, uint8_t slot) {
  if (slot >= kMoveSlots) return;
  pokemon.legacyCareCounterReserved &= ~(0x03UL << (slot * 2U));
}

void CollectionLogic::swapMovePpUps(OwnedPokemon& pokemon, uint8_t first, uint8_t second) {
  if (first >= kMoveSlots || second >= kMoveSlots || first == second) return;
  const uint8_t firstCount = ppUpCount(pokemon, first);
  const uint8_t secondCount = ppUpCount(pokemon, second);
  clearMovePpUps(pokemon, first);
  clearMovePpUps(pokemon, second);
  pokemon.legacyCareCounterReserved |= static_cast<uint32_t>(secondCount) << (first * 2U);
  pokemon.legacyCareCounterReserved |= static_cast<uint32_t>(firstCount) << (second * 2U);
}

namespace {
constexpr uint8_t kHeldBerryQuantityShift = 8U;
constexpr uint32_t kHeldBerryQuantityMask = 0x07UL << kHeldBerryQuantityShift;
}

uint8_t CollectionLogic::heldItemQuantity(const OwnedPokemon& pokemon) {
  if (pokemon.heldItem == HeldItem::None) return 0;
  if (!heldItemIsBerry(pokemon.heldItem)) return 1;
  const uint8_t stored = static_cast<uint8_t>(
      (pokemon.legacyCareCounterReserved & kHeldBerryQuantityMask) >>
      kHeldBerryQuantityShift);
  return static_cast<uint8_t>(std::min<uint8_t>(
      kMaximumHeldBerryQuantity, static_cast<uint8_t>(stored + 1U)));
}

bool CollectionLogic::setHeldItemQuantity(OwnedPokemon& pokemon, HeldItem item,
                                          uint8_t quantity) {
  pokemon.legacyCareCounterReserved &= ~kHeldBerryQuantityMask;
  if (item == HeldItem::None || quantity == 0U) {
    pokemon.heldItem = HeldItem::None;
    return true;
  }
  if (!heldItemIsBerry(item) && quantity != 1U) return false;
  pokemon.heldItem = item;
  if (heldItemIsBerry(item)) {
    const uint8_t bounded = std::min<uint8_t>(quantity, kMaximumHeldBerryQuantity);
    pokemon.legacyCareCounterReserved |=
        static_cast<uint32_t>(bounded - 1U) << kHeldBerryQuantityShift;
  }
  return true;
}

bool CollectionLogic::consumeHeldItem(OwnedPokemon& pokemon) {
  const uint8_t quantity = heldItemQuantity(pokemon);
  if (!quantity) return false;
  if (heldItemIsBerry(pokemon.heldItem) && quantity > 1U)
    return setHeldItemQuantity(pokemon, pokemon.heldItem,
                               static_cast<uint8_t>(quantity - 1U));
  return setHeldItemQuantity(pokemon, HeldItem::None, 0U);
}

const char* CollectionLogic::natureName(PokemonNature nature) {
  static constexpr const char* kNames[] = {
    "HARDY", "LONELY", "BRAVE", "ADAMANT", "NAUGHTY",
    "BOLD", "DOCILE", "RELAXED", "IMPISH", "LAX",
    "TIMID", "HASTY", "SERIOUS", "JOLLY", "NAIVE",
    "MODEST", "MILD", "QUIET", "BASHFUL", "RASH",
    "CALM", "GENTLE", "SASSY", "CAREFUL", "QUIRKY",
  };
  const uint8_t index = static_cast<uint8_t>(nature);
  return index < 25U ? kNames[index] : "HARDY";
}

int8_t CollectionLogic::natureEffect(PokemonNature nature, PokemonStat stat) {
  const uint8_t natureIndex = static_cast<uint8_t>(nature);
  if (natureIndex >= 25U) return 0;
  const uint8_t statIndex = static_cast<uint8_t>(stat);
  const uint8_t boosted = natureIndex / 5U;
  const uint8_t reduced = natureIndex % 5U;
  if (boosted == reduced) return 0;
  if (statIndex == boosted) return 1;
  if (statIndex == reduced) return -1;
  return 0;
}

uint16_t CollectionLogic::totalEffortValues(const OwnedPokemon& pokemon) {
  return static_cast<uint16_t>(pokemon.evs.hp) + pokemon.evs.attack + pokemon.evs.defense +
         pokemon.evs.spAttack + pokemon.evs.spDefense + pokemon.evs.speed;
}

uint16_t CollectionLogic::maximumTotalEffortValues(const OwnedPokemon& pokemon) {
  return pokemon.shiny ? kShinyMaximumTotalEffortValues : kMaximumTotalEffortValues;
}

bool CollectionLogic::redistributeEffortValues(OwnedPokemon& pokemon,
                                                const EffortValues& replacement) {
  if (pokemon.level < 90U) return false;
  const uint16_t earned = totalEffortValues(pokemon);
  const uint16_t replacementTotal = static_cast<uint16_t>(replacement.hp) + replacement.attack +
      replacement.defense + replacement.spAttack + replacement.spDefense + replacement.speed;
  if (earned > maximumTotalEffortValues(pokemon) || replacementTotal != earned) return false;
  pokemon.evs = replacement;
  refreshDerivedStats(pokemon, true);
  return true;
}

uint8_t CollectionLogic::grantEffortValues(OwnedPokemon& pokemon, const EffortValues& yield) {
  uint16_t remaining = totalEffortValues(pokemon);
  const uint16_t maximum = maximumTotalEffortValues(pokemon);
  if (remaining >= maximum) return 0;
  remaining = static_cast<uint16_t>(maximum - remaining);
  uint8_t awarded = 0;
  auto award = [&](uint8_t& current, uint8_t gained) {
    const uint16_t roomInStat = static_cast<uint16_t>(kMaximumEffortValue - current);
    const uint8_t amount = static_cast<uint8_t>(std::min<uint16_t>(gained, std::min(roomInStat, remaining)));
    current = static_cast<uint8_t>(current + amount);
    remaining = static_cast<uint16_t>(remaining - amount);
    awarded = static_cast<uint8_t>(awarded + amount);
  };
  // FireRed's source data and its AddEVs routine process these in this order.
  award(pokemon.evs.hp, yield.hp);
  award(pokemon.evs.attack, yield.attack);
  award(pokemon.evs.defense, yield.defense);
  award(pokemon.evs.speed, yield.speed);
  award(pokemon.evs.spAttack, yield.spAttack);
  award(pokemon.evs.spDefense, yield.spDefense);
  if (awarded) refreshDerivedStats(pokemon);
  return awarded;
}

uint8_t CollectionLogic::resolvedBaseHp(const OwnedPokemon& pokemon) {
  const SpeciesData* species = findSpecies(pokemon.speciesId);
  return species ? MegaEvolution::baseHp(pokemon, species->baseHp) : 0U;
}

uint8_t CollectionLogic::resolvedBaseStat(const OwnedPokemon& pokemon, PokemonStat stat) {
  const SpeciesData* species = findSpecies(pokemon.speciesId);
  if (!species) return 0U;
  const uint8_t normalBase = pokemon.speciesId == 386
      ? deoxysBaseForStat(resolvedForm(pokemon), stat) : baseForStat(*species, stat);
  return MegaEvolution::baseStat(pokemon, static_cast<uint8_t>(stat), normalBase);
}

uint16_t CollectionLogic::calculatedMaximumHp(const OwnedPokemon& pokemon) {
  const SpeciesData* species = findSpecies(pokemon.speciesId);
  if (!species) return 0;
  // Shedinja's HP is always exactly one regardless of level, IVs or EVs.
  if (pokemon.speciesId == 292) return 1;
  const uint8_t baseHp=resolvedBaseHp(pokemon);
  return static_cast<uint16_t>((((2U * baseHp + pokemon.ivs.hp + pokemon.evs.hp / 4U) * pokemon.level) / 100U) +
                               pokemon.level + 10U);
}

uint16_t CollectionLogic::calculatedStat(const OwnedPokemon& pokemon, PokemonStat stat) {
  const SpeciesData* species = findSpecies(pokemon.speciesId);
  if (!species) return 0;
  const uint8_t base=resolvedBaseStat(pokemon,stat);
  uint32_t value = (((2U * base + ivForStat(pokemon.ivs, stat) +
                     evForStat(pokemon.evs, stat) / 4U) *
                     pokemon.level) / 100U) + 5U;
  const int8_t effect = natureEffect(pokemon.nature, stat);
  if (effect > 0) value = value * 110U / 100U;
  else if (effect < 0) value = value * 90U / 100U;
  return static_cast<uint16_t>(value);
}

void CollectionLogic::refreshDerivedStats(OwnedPokemon& pokemon, bool preserveDamage) {
  const uint16_t previousMaximum = pokemon.maximumHp;
  const uint16_t previousHp = pokemon.currentHp;
  pokemon.maximumHp = calculatedMaximumHp(pokemon);
  if (!preserveDamage) {
    pokemon.currentHp = pokemon.maximumHp;
  } else if (previousHp == 0) {
    pokemon.currentHp = 0;
  } else {
    const int32_t adjusted = static_cast<int32_t>(previousHp) + pokemon.maximumHp - previousMaximum;
    pokemon.currentHp = static_cast<uint16_t>(std::max<int32_t>(1, std::min<int32_t>(pokemon.maximumHp, adjusted)));
  }
}

void CollectionLogic::refreshAbility(OwnedPokemon& pokemon, uint8_t unlockedGeneration) {
  const SpeciesData* species = findSpecies(pokemon.speciesId);
  if (!species) {
    pokemon.abilityId = 0;
    return;
  }
  const bool unova = unlockedGeneration >= 5U;
  const bool platinum = unlockedGeneration >= 4U;
  const uint8_t ability1 = unova ? species->unovaAbility1 :
      platinum ? species->sinnohAbility1 : species->ability1;
  const uint8_t ability2 = unova ? species->unovaAbility2 :
      platinum ? species->sinnohAbility2 : species->ability2;
  const uint8_t normal=species ?
      ((ability2 && (pokemon.personality & 1U)) ? ability2 : ability1) : 0;
  const uint8_t resolved = unova && hasHiddenAbility(pokemon) &&
      species->hiddenAbility ? species->hiddenAbility : normal;
  pokemon.abilityId=MegaEvolution::abilityId(pokemon,resolved);
}

namespace {
constexpr uint32_t kHiddenAbilityMask = 1UL << 14U;
}

bool CollectionLogic::hasHiddenAbility(const OwnedPokemon& pokemon) {
  return (pokemon.legacyCareCounterReserved & kHiddenAbilityMask) != 0U;
}

bool CollectionLogic::setHiddenAbility(OwnedPokemon& pokemon, bool enabled,
                                       uint8_t unlockedGeneration) {
  const SpeciesData* species = findSpecies(pokemon.speciesId);
  if (enabled && (!species || !species->hiddenAbility || unlockedGeneration < 5U))
    return false;
  const bool before = hasHiddenAbility(pokemon);
  if (enabled) pokemon.legacyCareCounterReserved |= kHiddenAbilityMask;
  else pokemon.legacyCareCounterReserved &= ~kHiddenAbilityMask;
  refreshAbility(pokemon, unlockedGeneration);
  return before != enabled;
}

void CollectionLogic::applyGenerationContent(PokemonCollection& collection,
                                              uint8_t unlockedGeneration) {
  for (auto& pokemon : collection.box) {
    if (pokemon.uid == kEmptyPokemonUid) continue;
    refreshAbility(pokemon, unlockedGeneration);
  }
}

uint8_t CollectionLogic::resolvedForm(const OwnedPokemon& pokemon) {
  if (pokemon.speciesId == 201) return std::min<uint8_t>(pokemon.form, 27U);
  // Castform writes this byte only while it is on the battlefield. The
  // battle teardown path always restores NORMAL before returning Home.
  if (pokemon.speciesId == 351) return std::min<uint8_t>(pokemon.form, 3U);
  // Cherrim's Sunshine form is likewise battle/weather-driven and is not a
  // player-selectable persistent form.
  if (pokemon.speciesId == 421) return std::min<uint8_t>(pokemon.form, 1U);
  if (pokemon.speciesId == 386) return std::min<uint8_t>(pokemon.form, 3U);
  if (pokemon.speciesId == 479) return std::min<uint8_t>(pokemon.form, 5U);
  if (pokemon.speciesId == 487)
    return pokemon.heldItem == HeldItem::GriseousOrb ? 1U : std::min<uint8_t>(pokemon.form, 1U);
  if (pokemon.speciesId == 492) return std::min<uint8_t>(pokemon.form, 1U);
  if (pokemon.speciesId == 493 && heldItemIsArceusPlate(pokemon.heldItem))
    return arceusFormForType(arceusPlateType(pokemon.heldItem));
  if (pokemon.speciesId == 550U) return std::min<uint8_t>(pokemon.form,1U);
  if (pokemon.speciesId == 555U) return std::min<uint8_t>(pokemon.form,1U);
  if (pokemon.speciesId == 585U || pokemon.speciesId == 586U)
    return std::min<uint8_t>(pokemon.form,3U);
  if (pokemon.speciesId == 641U || pokemon.speciesId == 642U ||
      pokemon.speciesId == 645U) return std::min<uint8_t>(pokemon.form,1U);
  if (pokemon.speciesId == 646U) return std::min<uint8_t>(pokemon.form,2U);
  if (pokemon.speciesId == 647U) {
    for (MoveId move:pokemon.moves)
      if (move==static_cast<MoveId>(548U)) return 1U; // SECRET SWORD
    return 0U;
  }
  if (pokemon.speciesId == 648U) return std::min<uint8_t>(pokemon.form,1U);
  if (pokemon.speciesId == 649U) {
    switch(pokemon.heldItem){
      case HeldItem::DouseDrive:return 1U;
      case HeldItem::ShockDrive:return 2U;
      case HeldItem::BurnDrive:return 3U;
      case HeldItem::ChillDrive:return 4U;
      default:return 0U;
    }
  }
  return 0;
}

uint8_t CollectionLogic::formCount(uint16_t speciesId) {
  if (speciesId == 201) return 28U;
  if (speciesId == 351 || speciesId == 386) return 4U;
  if (speciesId == 479) return 6U;
  if (speciesId == 487 || speciesId == 492) return 2U;
  if (speciesId == 493) return 17U;
  if (speciesId == 550U || speciesId == 555U || speciesId == 641U ||
      speciesId == 642U || speciesId == 645U || speciesId == 647U ||
      speciesId == 648U) return 2U;
  if (speciesId == 585U || speciesId == 586U) return 4U;
  if (speciesId == 646U) return 3U;
  if (speciesId == 649U) return 5U;
  return 1U;
}

const char* CollectionLogic::formName(uint16_t speciesId, uint8_t form) {
  static constexpr const char* kUnownForms[] = {
      "A","B","C","D","E","F","G","H","I","J","K","L","M","N",
      "O","P","Q","R","S","T","U","V","W","X","Y","Z","!","?"};
  static constexpr const char* kDeoxysForms[] = {"NORMAL", "ATTACK", "DEFENSE", "SPEED"};
  static constexpr const char* kCastformForms[] = {"NORMAL", "SUNNY", "RAINY", "SNOWY"};
  static constexpr const char* kRotomForms[] = {"NORMAL", "HEAT", "WASH", "FROST", "FAN", "MOW"};
  static constexpr const char* kArceusForms[] = {
      "NORMAL", "FIGHTING", "FLYING", "POISON", "ROCK", "BUG", "GHOST",
      "DRAGON", "STEEL", "FIRE", "WATER", "GRASS", "ELECTRIC", "PSYCHIC",
      "ICE", "GROUND", "DARK"};
  static constexpr const char* kSeasons[] = {"SPRING","SUMMER","AUTUMN","WINTER"};
  static constexpr const char* kGenesectForms[] = {"NORMAL","DOUSE","SHOCK","BURN","CHILL"};
  if (speciesId == 201) return kUnownForms[std::min<uint8_t>(form, 27U)];
  if (speciesId == 386) return kDeoxysForms[std::min<uint8_t>(form, 3U)];
  if (speciesId == 351) return kCastformForms[std::min<uint8_t>(form, 3U)];
  if (speciesId == 479) return kRotomForms[std::min<uint8_t>(form, 5U)];
  if (speciesId == 487) return form ? "ORIGIN" : "ALTERED";
  if (speciesId == 492) return form ? "SKY" : "LAND";
  if (speciesId == 493) return kArceusForms[std::min<uint8_t>(form, 16U)];
  if (speciesId == 550U) return form?"BLUE STRIPE":"RED STRIPE";
  if (speciesId == 555U) return form?"ZEN":"STANDARD";
  if (speciesId == 585U || speciesId == 586U) return kSeasons[std::min<uint8_t>(form,3U)];
  if (speciesId == 641U || speciesId == 642U || speciesId == 645U)
    return form?"THERIAN":"INCARNATE";
  if (speciesId == 646U) return form==1U?"BLACK":form==2U?"WHITE":"NORMAL";
  if (speciesId == 647U) return form?"RESOLUTE":"ORDINARY";
  if (speciesId == 648U) return form?"PIROUETTE":"ARIA";
  if (speciesId == 649U) return kGenesectForms[std::min<uint8_t>(form,4U)];
  return "NORMAL";
}

bool CollectionLogic::canChangeForm(const OwnedPokemon& pokemon) {
  // Unown and Castform are derived/battle-local. The explicit picker is for
  // stable forms that the player can select directly from the Box.
  return pokemon.speciesId == 386U || pokemon.speciesId == 479U ||
         (pokemon.speciesId == 487U && pokemon.heldItem != HeldItem::GriseousOrb) ||
         pokemon.speciesId == 492U || pokemon.speciesId == 641U ||
         pokemon.speciesId == 642U || pokemon.speciesId == 645U;
}

bool CollectionLogic::setForm(OwnedPokemon& pokemon, uint8_t form) {
  const uint8_t count = formCount(pokemon.speciesId);
  if (!canChangeForm(pokemon) || form >= count || pokemon.form == form) return false;
  pokemon.form = form;
  synchronizeRotomFormMove(pokemon, resolvedForm(pokemon));
  refreshAbility(pokemon, pokemon.speciesId >= 494U ? 5U : 4U);
  refreshDerivedStats(pokemon, true);
  return true;
}

bool CollectionLogic::normalizeForm(OwnedPokemon& pokemon) {
  uint8_t normalized = 0;
  if (pokemon.speciesId == 386) normalized = std::min<uint8_t>(pokemon.form, 3U);
  else if (pokemon.speciesId == 479) normalized = std::min<uint8_t>(pokemon.form, 5U);
  else if (pokemon.speciesId == 487 || pokemon.speciesId == 492)
    normalized = pokemon.speciesId == 487 && pokemon.heldItem == HeldItem::GriseousOrb
        ? 1U : std::min<uint8_t>(pokemon.form, 1U);
  else if (pokemon.speciesId == 493)
    normalized = resolvedForm(pokemon);
  else if (pokemon.speciesId == 550U || pokemon.speciesId == 555U ||
           pokemon.speciesId == 641U || pokemon.speciesId == 642U ||
           pokemon.speciesId == 645U || pokemon.speciesId == 647U ||
           pokemon.speciesId == 648U)
    normalized = resolvedForm(pokemon);
  else if (pokemon.speciesId == 585U || pokemon.speciesId == 586U ||
           pokemon.speciesId == 646U || pokemon.speciesId == 649U)
    normalized = resolvedForm(pokemon);
  else if (pokemon.speciesId == 201) {
    // Repair older saves whose Unown form byte was always cleared to zero.
    const uint32_t personality = pokemon.personality;
    normalized = static_cast<uint8_t>((((personality & 0x03000000U) >> 18U) |
                                       ((personality & 0x00030000U) >> 12U) |
                                       ((personality & 0x00000300U) >> 6U) |
                                        (personality & 0x00000003U)) % 28U);
  }
  const bool changed = pokemon.form != normalized;
  if (changed) pokemon.form = normalized;
  synchronizeRotomFormMove(pokemon, resolvedForm(pokemon));
  refreshAbility(pokemon, pokemon.speciesId >= 494U ? 5U : 4U);
  if (changed) refreshDerivedStats(pokemon, true);
  return changed;
}

bool CollectionLogic::applyUnovaSeason(OwnedPokemon& pokemon, uint8_t season) {
  if (pokemon.speciesId != 585U && pokemon.speciesId != 586U) return false;
  season = std::min<uint8_t>(season, 3U);
  if (pokemon.form == season) return false;
  pokemon.form = season;
  return true;
}

bool CollectionLogic::applyUnovaSeason(PokemonCollection& collection,
                                       uint8_t season) {
  bool changed = false;
  for (OwnedPokemon& pokemon : collection.box)
    if (pokemon.uid != kEmptyPokemonUid && applyUnovaSeason(pokemon, season))
      changed = true;
  return changed;
}

void CollectionLogic::initialize(PokemonCollection& collection) {
  collection = PokemonCollection{};
}

bool CollectionLogic::chooseStarter(PokemonCollection& collection, uint16_t speciesId, uint32_t shinyRoll) {
  if (count(collection) != 0 || (speciesId != 1 && speciesId != 4 && speciesId != 7)) return false;
  collection.box[0] = createPokemon(collection.nextUid++, speciesId, 5, isShinyRoll(shinyRoll), shinyRoll);
  collection.party[0] = collection.box[0].uid;
  return true;
}

bool CollectionLogic::addRegionalStarter(PokemonCollection& collection, uint16_t speciesId, uint32_t shinyRoll) {
  const bool valid = speciesId == 152 || speciesId == 155 || speciesId == 158 ||
                     speciesId == 252 || speciesId == 255 || speciesId == 258;
  if (!valid) return false;
  return add(collection, createPokemon(0, speciesId, 5, isShinyRoll(shinyRoll), shinyRoll));
}

OwnedPokemon* CollectionLogic::find(PokemonCollection& collection, uint32_t uid) {
  if (uid == kEmptyPokemonUid) return nullptr;
  for (auto& pokemon : collection.box) if (pokemon.uid == uid) return &pokemon;
  return nullptr;
}

const OwnedPokemon* CollectionLogic::find(const PokemonCollection& collection, uint32_t uid) {
  if (uid == kEmptyPokemonUid) return nullptr;
  for (const auto& pokemon : collection.box) if (pokemon.uid == uid) return &pokemon;
  return nullptr;
}

OwnedPokemon* CollectionLogic::active(PokemonCollection& collection, uint8_t slot) {
  return slot < kPartyCapacity ? find(collection, collection.party[slot]) : nullptr;
}

const OwnedPokemon* CollectionLogic::active(const PokemonCollection& collection, uint8_t slot) {
  return slot < kPartyCapacity ? find(collection, collection.party[slot]) : nullptr;
}

uint16_t CollectionLogic::count(const PokemonCollection& collection) {
  uint16_t total = 0;
  for (const auto& pokemon : collection.box) if (pokemon.uid != kEmptyPokemonUid) ++total;
  return total;
}

bool CollectionLogic::add(PokemonCollection& collection, OwnedPokemon pokemon, uint32_t* assignedUid) {
  for (auto& slot : collection.box) {
    if (slot.uid != kEmptyPokemonUid) continue;
    pokemon.uid = collection.nextUid++;
    if (pokemon.uid == kEmptyPokemonUid) pokemon.uid = collection.nextUid++;
    slot = pokemon;
    if (assignedUid) *assignedUid = pokemon.uid;
    return true;
  }
  return false;
}

bool CollectionLogic::evolve(PokemonCollection& collection, uint32_t uid,
                             uint16_t newSpeciesId, uint32_t* bonusUid,
                             uint8_t unlockedGeneration) {
  if (bonusUid) *bonusUid = kEmptyPokemonUid;
  OwnedPokemon* pokemon = find(collection, uid);
  if (!pokemon || !findSpecies(newSpeciesId) || pokemon->speciesId == newSpeciesId)
    return false;

  const bool createsShedinja = pokemon->speciesId == 290U && newSpeciesId == 291U;
  OwnedPokemon shedinjaSource{};
  if (createsShedinja) shedinjaSource = *pokemon;

  const uint16_t oldSpeciesId = pokemon->speciesId;
  const uint8_t oldForm = pokemon->form;
  pokemon->speciesId = newSpeciesId;
  pokemon->form = oldSpeciesId == 585U && newSpeciesId == 586U
      ? std::min<uint8_t>(oldForm, 3U) : 0U;
  refreshAbility(*pokemon, std::min<uint8_t>(5U, unlockedGeneration));
  refreshDerivedStats(*pokemon);

  if (createsShedinja) {
    shedinjaSource.uid = kEmptyPokemonUid;
    shedinjaSource.speciesId = 292U;
    shedinjaSource.form = 0;
    refreshAbility(shedinjaSource, std::min<uint8_t>(5U, unlockedGeneration));
    refreshDerivedStats(shedinjaSource, false);
    add(collection, shedinjaSource, bonusUid);
  }
  return true;
}

RareCandyProgression CollectionLogic::applyRareCandy(
    OwnedPokemon& pokemon, uint8_t unlockedGeneration) {
  RareCandyProgression result{};
  const SpeciesData* species = findSpecies(pokemon.speciesId);
  if (!species) return result;

  const uint8_t generation = std::min<uint8_t>(5U, unlockedGeneration);
  if (pokemon.level < 100U) {
    ++pokemon.level;
    pokemon.experience = std::max<uint32_t>(
        pokemon.experience,
        experienceForLevel(species->growthRate, pokemon.level));
    result.learnedMove = moveLearnedAtLevelForForm(
        pokemon.speciesId, resolvedForm(pokemon), pokemon.level, generation);
    refreshDerivedStats(pokemon, true);
    result.levelIncreased = true;
  }

  result.evolutionChoiceCount = evolutionChoices(
      pokemon.speciesId, pokemon.level,
      calculatedStat(pokemon, PokemonStat::Attack),
      calculatedStat(pokemon, PokemonStat::Defense), generation,
      result.evolutionChoices, kEvolutionChoiceCapacity);

  // Below Lv.100 the level increase is itself a valid use. At Lv.100 the
  // Candy is useful only when it can recover a pending evolution.
  result.applied = result.levelIncreased || result.evolutionChoiceCount != 0U;
  return result;
}

bool CollectionLogic::isInParty(const PokemonCollection& collection, uint32_t uid) {
  for (uint32_t member : collection.party) if (member == uid && uid != kEmptyPokemonUid) return true;
  return false;
}

void CollectionLogic::sortBox(PokemonCollection& collection, BoxSortMode mode) {
  const auto comesBefore = [mode](const OwnedPokemon& left,
                                  const OwnedPokemon& right) {
    if (!left.uid) return false;
    if (!right.uid) return true;
    if (mode == BoxSortMode::EffortValues) {
      const uint16_t leftTotal = totalEffortValues(left);
      const uint16_t rightTotal = totalEffortValues(right);
      if (leftTotal != rightTotal) return leftTotal > rightTotal;
      if (left.level != right.level) return left.level > right.level;
    }
    if (mode == BoxSortMode::Level && left.level != right.level)
      return left.level > right.level;
    if (left.speciesId != right.speciesId)
      return left.speciesId < right.speciesId;
    if (left.level != right.level) return left.level > right.level;
    return left.uid < right.uid;
  };

  // A small in-place insertion sort avoids allocating a second ~26 KiB Box
  // on the ESP32's constrained stack/heap. This is a deliberate user action,
  // so O(n^2) over at most 386 entries is both bounded and fast enough.
  for (uint16_t index = 1; index < kBoxCapacity; ++index) {
    OwnedPokemon moving = collection.box[index];
    uint16_t destination = index;
    while (destination > 0U &&
           comesBefore(moving, collection.box[destination - 1U])) {
      collection.box[destination] = collection.box[destination - 1U];
      --destination;
    }
    if (destination != index) collection.box[destination] = moving;
  }
}

bool CollectionLogic::setPartySlot(PokemonCollection& collection, uint8_t slot, uint32_t uid) {
  if (slot >= kPartyCapacity || !find(collection, uid)) return false;
  for (uint8_t i = 0; i < kPartyCapacity; ++i) {
    if (i != slot && collection.party[i] == uid) {
      const uint32_t replaced = collection.party[slot];
      collection.party[slot] = uid;
      collection.party[i] = replaced;
      return true;
    }
  }
  collection.party[slot] = uid;
  return true;
}

bool CollectionLogic::removeFromParty(PokemonCollection& collection, uint32_t uid) {
  uint8_t count=0,found=kPartyCapacity;for(uint8_t i=0;i<kPartyCapacity;++i){if(collection.party[i])++count;if(collection.party[i]==uid)found=i;}
  if(count<=1||found>=kPartyCapacity)return false;
  for(uint8_t i=found;i+1<kPartyCapacity;++i)collection.party[i]=collection.party[i+1];
  collection.party[kPartyCapacity-1]=kEmptyPokemonUid;return validate(collection);
}

bool CollectionLogic::release(PokemonCollection& collection, uint32_t uid) {
  const OwnedPokemon* selected=find(collection,uid);
  if (uid == kEmptyPokemonUid || !selected || heldItemIsTransferLocked(selected->heldItem)) return false;
  // Do not permit a release which would leave the virtual pet with no active
  // partner.  A party member is first shifted out using the same invariant
  // checked by normal party management, then its box record is cleared.
  if (isInParty(collection, uid) && !removeFromParty(collection, uid)) return false;
  for (auto& pokemon : collection.box) {
    if (pokemon.uid != uid) continue;
    pokemon = OwnedPokemon{};
    return validate(collection);
  }
  return false;
}

bool CollectionLogic::tradeReplace(PokemonCollection& collection, uint32_t outgoingUid,
                                   OwnedPokemon incoming, uint32_t* assignedUid) {
  if (outgoingUid == kEmptyPokemonUid || incoming.speciesId == 0 ||
      !findSpecies(incoming.speciesId) || heldItemIsTransferLocked(incoming.heldItem)) return false;
  const OwnedPokemon* outgoing=find(collection,outgoingUid);
  if(!outgoing||heldItemIsTransferLocked(outgoing->heldItem))return false;
  uint16_t boxIndex = kBoxCapacity;
  for (uint16_t index = 0; index < kBoxCapacity; ++index) {
    if (collection.box[index].uid == outgoingUid) { boxIndex = index; break; }
  }
  if (boxIndex >= kBoxCapacity) return false;

  uint32_t newUid = collection.nextUid++;
  if (newUid == kEmptyPokemonUid) newUid = collection.nextUid++;
  incoming.uid = newUid;
  incoming.status = StatusCondition::None;
  incoming.recoverySecondsRemaining = 0;
  ensureUsableMoves(incoming);
  refreshDerivedStats(incoming, true);

  collection.box[boxIndex] = incoming;
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot)
    if (collection.party[slot] == outgoingUid) collection.party[slot] = newUid;
  if (!validate(collection)) return false;
  if (assignedUid) *assignedUid = newUid;
  return true;
}

bool CollectionLogic::validate(const PokemonCollection& collection) {
  if (collection.party[0] == kEmptyPokemonUid) return false;
  for (uint8_t i = 0; i < kPartyCapacity; ++i) {
    if (collection.party[i] != kEmptyPokemonUid && !find(collection, collection.party[i])) return false;
    for (uint8_t j = i + 1; j < kPartyCapacity; ++j) {
      if (collection.party[i] != kEmptyPokemonUid && collection.party[i] == collection.party[j]) return false;
    }
  }
  return true;
}

void CollectionLogic::advanceRecovery(OwnedPokemon& pokemon, uint32_t elapsedSeconds,
                                      bool inParty, uint32_t worldTimeSeconds) {
  if (pokemon.uid == kEmptyPokemonUid) return;
  // Repair fainted records created by older beta firmware as well as enforcing
  // the current rule before the first passive HP point is restored.
  if (pokemon.currentHp == 0) pokemon.status = StatusCondition::None;
  if (!elapsedSeconds) return;
  constexpr uint8_t kAcceleratedPartyMaximumLevel = 20U;
  constexpr uint32_t kPartyRecoverySeconds = 30U * 60U;
  constexpr uint32_t kAcceleratedPartyRecoverySeconds = 10U * 60U;
  constexpr uint32_t kBoxRecoverySeconds = 3U * 60U * 60U;
  const uint32_t duration = inParty
      ? (pokemon.level <= kAcceleratedPartyMaximumLevel
             ? kAcceleratedPartyRecoverySeconds : kPartyRecoverySeconds)
      : kBoxRecoverySeconds;
  const uint32_t previousTime = worldTimeSeconds - elapsedSeconds;

  // Count evenly spaced unit boundaries crossed in this exact time interval.
  // A stat with maximum M therefore gains exactly M points per duration,
  // irrespective of how often advanceGameClocks() happens to call us.
  auto recoveredUnits = [&](uint16_t maximum) -> uint16_t {
    if (!maximum) return 0;
    const uint64_t before = static_cast<uint64_t>(previousTime) * maximum / duration;
    const uint64_t after = static_cast<uint64_t>(worldTimeSeconds) * maximum / duration;
    return static_cast<uint16_t>(std::min<uint64_t>(maximum, after - before));
  };

  if (pokemon.currentHp < pokemon.maximumHp) {
    pokemon.currentHp = std::min<uint16_t>(pokemon.maximumHp,
        static_cast<uint16_t>(pokemon.currentHp + recoveredUnits(pokemon.maximumHp)));
  }
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    const uint8_t maximumPp = maximumMovePp(pokemon, slot);
    if (pokemon.movePp[slot] >= maximumPp) continue;
    pokemon.movePp[slot] = std::min<uint8_t>(maximumPp,
        static_cast<uint8_t>(pokemon.movePp[slot] + recoveredUnits(maximumPp)));
  }
  // recoverySecondsRemaining is now an incapacity latch, not a countdown.
  // Passive healing unlocks a fainted Pokemon only at full HP.
  if (pokemon.maximumHp && pokemon.currentHp >= pokemon.maximumHp)
    pokemon.recoverySecondsRemaining = 0;
}

void CollectionLogic::advanceFriendship(OwnedPokemon& pokemon, uint32_t elapsedSeconds) {
  PetState state;
  state.friendship = pokemon.friendship;
  state.friendshipRemainderSeconds = pokemon.friendshipRemainderSeconds;
  PetLogic::advanceFriendship(state, elapsedSeconds);
  pokemon.friendship = state.friendship;
  pokemon.friendshipRemainderSeconds = state.friendshipRemainderSeconds;
}

void CollectionLogic::care(OwnedPokemon& pokemon, CareAction action) {
  PetState state;
  state.status = pokemon.status;
  PetLogic::care(state, action);
  pokemon.status = state.status;
  if (action == CareAction::Center) {
    // Bathing is the Pokégochi's full recovery station: clear battle damage
    // as well as status effects, then refill every move's PP.  A defeated
    // Pokémon keeps its recovery timer, so the original 2–3 hour incapacity
    // rule is not bypassed by bathing.
    pokemon.currentHp = pokemon.maximumHp;
    pokemon.recoverySecondsRemaining = 0;
    for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
      pokemon.movePp[slot] = maximumMovePp(pokemon, slot);
    }
  }
}
