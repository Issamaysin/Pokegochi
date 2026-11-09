#include "ServerBattleBot.h"

#include <algorithm>
#include <cstring>

#include "game/BattleEngine.h"

namespace PokegochiServerBot {
namespace {

constexpr RosterEntry kRoster[] = {
#include "ServerBattleBotRosterGenerated.inc"
};

struct MoveAiData {
  uint16_t id;
  PokemonType type;
  uint8_t power;
  uint8_t accuracy;
  MoveCategory category;
  uint8_t pp;
  int8_t priority;
  uint8_t effectChance;
  const char* effect;
};

struct SpeciesTypes {
  PokemonType first;
  PokemonType second;
};

constexpr MoveAiData kMoveAiData[] = {
#include "ServerBattleBotMoveAiGenerated.inc"
};

constexpr SpeciesTypes kSpeciesTypes[] = {
#include "ServerBattleBotSpeciesTypesGenerated.inc"
};

static_assert(sizeof(OwnedPokemon) == 64U,
              "Server bot must use the exact PGCH Pokemon ABI");
static_assert(sizeof(kRoster) / sizeof(kRoster[0]) == kRosterSize,
              "Server bot Box must contain 10 Pokemon from every generation");
static_assert(sizeof(kSpeciesTypes) / sizeof(kSpeciesTypes[0]) == 808U,
              "Server bot type table must cover National Dex #001-#807");

const MoveAiData* moveData(MoveId id) {
  const uint16_t value = static_cast<uint16_t>(id);
  for (const MoveAiData& move : kMoveAiData)
    if (move.id == value) return &move;
  return nullptr;
}

bool effectIs(const MoveAiData& move, const char* effect) {
  return std::strcmp(move.effect, effect) == 0;
}

bool effectContains(const MoveAiData& move, const char* fragment) {
  return std::strstr(move.effect, fragment) != nullptr;
}

SpeciesTypes speciesTypes(const OwnedPokemon* pokemon,
                          const DedicatedMoveEffectState* effects) {
  if (effects && effects->typeOverrideActive)
    return {effects->type1, effects->type2};
  if (!pokemon || pokemon->speciesId >=
          sizeof(kSpeciesTypes) / sizeof(kSpeciesTypes[0]))
    return kSpeciesTypes[0];
  return kSpeciesTypes[pokemon->speciesId];
}

uint16_t typeEffectiveness(PokemonType attack, SpeciesTypes defender) {
  uint16_t value = 100U;
  const auto apply = [&](PokemonType type) {
    const auto is = [&](PokemonType a, PokemonType d) {
      return attack == a && type == d;
    };
    if (is(PokemonType::Normal, PokemonType::Ghost) ||
        is(PokemonType::Fighting, PokemonType::Ghost) ||
        is(PokemonType::Electric, PokemonType::Ground) ||
        is(PokemonType::Ground, PokemonType::Flying) ||
        is(PokemonType::Psychic, PokemonType::Dark) ||
        is(PokemonType::Ghost, PokemonType::Normal) ||
        is(PokemonType::Poison, PokemonType::Steel) ||
        is(PokemonType::Dragon, PokemonType::Fairy)) {
      value = 0U;
      return;
    }
    const bool super =
        is(PokemonType::Fire, PokemonType::Grass) ||
        is(PokemonType::Fire, PokemonType::Ice) ||
        is(PokemonType::Fire, PokemonType::Bug) ||
        is(PokemonType::Fire, PokemonType::Steel) ||
        is(PokemonType::Water, PokemonType::Fire) ||
        is(PokemonType::Water, PokemonType::Ground) ||
        is(PokemonType::Water, PokemonType::Rock) ||
        is(PokemonType::Electric, PokemonType::Water) ||
        is(PokemonType::Electric, PokemonType::Flying) ||
        is(PokemonType::Grass, PokemonType::Water) ||
        is(PokemonType::Grass, PokemonType::Ground) ||
        is(PokemonType::Grass, PokemonType::Rock) ||
        is(PokemonType::Ice, PokemonType::Grass) ||
        is(PokemonType::Ice, PokemonType::Ground) ||
        is(PokemonType::Ice, PokemonType::Flying) ||
        is(PokemonType::Ice, PokemonType::Dragon) ||
        is(PokemonType::Fighting, PokemonType::Normal) ||
        is(PokemonType::Fighting, PokemonType::Ice) ||
        is(PokemonType::Fighting, PokemonType::Rock) ||
        is(PokemonType::Fighting, PokemonType::Dark) ||
        is(PokemonType::Fighting, PokemonType::Steel) ||
        is(PokemonType::Poison, PokemonType::Grass) ||
        is(PokemonType::Poison, PokemonType::Fairy) ||
        is(PokemonType::Ground, PokemonType::Fire) ||
        is(PokemonType::Ground, PokemonType::Electric) ||
        is(PokemonType::Ground, PokemonType::Poison) ||
        is(PokemonType::Ground, PokemonType::Rock) ||
        is(PokemonType::Ground, PokemonType::Steel) ||
        is(PokemonType::Flying, PokemonType::Grass) ||
        is(PokemonType::Flying, PokemonType::Fighting) ||
        is(PokemonType::Flying, PokemonType::Bug) ||
        is(PokemonType::Psychic, PokemonType::Fighting) ||
        is(PokemonType::Psychic, PokemonType::Poison) ||
        is(PokemonType::Bug, PokemonType::Grass) ||
        is(PokemonType::Bug, PokemonType::Psychic) ||
        is(PokemonType::Bug, PokemonType::Dark) ||
        is(PokemonType::Rock, PokemonType::Fire) ||
        is(PokemonType::Rock, PokemonType::Ice) ||
        is(PokemonType::Rock, PokemonType::Flying) ||
        is(PokemonType::Rock, PokemonType::Bug) ||
        is(PokemonType::Ghost, PokemonType::Psychic) ||
        is(PokemonType::Ghost, PokemonType::Ghost) ||
        is(PokemonType::Dragon, PokemonType::Dragon) ||
        is(PokemonType::Dark, PokemonType::Psychic) ||
        is(PokemonType::Dark, PokemonType::Ghost) ||
        is(PokemonType::Steel, PokemonType::Ice) ||
        is(PokemonType::Steel, PokemonType::Rock) ||
        is(PokemonType::Steel, PokemonType::Fairy) ||
        is(PokemonType::Fairy, PokemonType::Fighting) ||
        is(PokemonType::Fairy, PokemonType::Dragon) ||
        is(PokemonType::Fairy, PokemonType::Dark);
    if (super) {
      value = static_cast<uint16_t>(value * 2U);
      return;
    }
    const bool resist =
        is(PokemonType::Normal, PokemonType::Rock) ||
        is(PokemonType::Normal, PokemonType::Steel) ||
        is(PokemonType::Fire, PokemonType::Fire) ||
        is(PokemonType::Fire, PokemonType::Water) ||
        is(PokemonType::Fire, PokemonType::Rock) ||
        is(PokemonType::Fire, PokemonType::Dragon) ||
        is(PokemonType::Water, PokemonType::Water) ||
        is(PokemonType::Water, PokemonType::Grass) ||
        is(PokemonType::Water, PokemonType::Dragon) ||
        is(PokemonType::Electric, PokemonType::Electric) ||
        is(PokemonType::Electric, PokemonType::Grass) ||
        is(PokemonType::Electric, PokemonType::Dragon) ||
        is(PokemonType::Grass, PokemonType::Fire) ||
        is(PokemonType::Grass, PokemonType::Grass) ||
        is(PokemonType::Grass, PokemonType::Poison) ||
        is(PokemonType::Grass, PokemonType::Flying) ||
        is(PokemonType::Grass, PokemonType::Bug) ||
        is(PokemonType::Grass, PokemonType::Dragon) ||
        is(PokemonType::Grass, PokemonType::Steel) ||
        is(PokemonType::Ice, PokemonType::Fire) ||
        is(PokemonType::Ice, PokemonType::Water) ||
        is(PokemonType::Ice, PokemonType::Ice) ||
        is(PokemonType::Ice, PokemonType::Steel) ||
        is(PokemonType::Fighting, PokemonType::Poison) ||
        is(PokemonType::Fighting, PokemonType::Flying) ||
        is(PokemonType::Fighting, PokemonType::Psychic) ||
        is(PokemonType::Fighting, PokemonType::Bug) ||
        is(PokemonType::Fighting, PokemonType::Fairy) ||
        is(PokemonType::Poison, PokemonType::Poison) ||
        is(PokemonType::Poison, PokemonType::Ground) ||
        is(PokemonType::Poison, PokemonType::Rock) ||
        is(PokemonType::Poison, PokemonType::Ghost) ||
        is(PokemonType::Ground, PokemonType::Grass) ||
        is(PokemonType::Ground, PokemonType::Bug) ||
        is(PokemonType::Flying, PokemonType::Electric) ||
        is(PokemonType::Flying, PokemonType::Rock) ||
        is(PokemonType::Flying, PokemonType::Steel) ||
        is(PokemonType::Psychic, PokemonType::Psychic) ||
        is(PokemonType::Psychic, PokemonType::Steel) ||
        is(PokemonType::Bug, PokemonType::Fire) ||
        is(PokemonType::Bug, PokemonType::Fighting) ||
        is(PokemonType::Bug, PokemonType::Poison) ||
        is(PokemonType::Bug, PokemonType::Flying) ||
        is(PokemonType::Bug, PokemonType::Ghost) ||
        is(PokemonType::Bug, PokemonType::Steel) ||
        is(PokemonType::Bug, PokemonType::Fairy) ||
        is(PokemonType::Rock, PokemonType::Fighting) ||
        is(PokemonType::Rock, PokemonType::Ground) ||
        is(PokemonType::Rock, PokemonType::Steel) ||
        is(PokemonType::Ghost, PokemonType::Dark) ||
        is(PokemonType::Dragon, PokemonType::Steel) ||
        is(PokemonType::Dark, PokemonType::Fighting) ||
        is(PokemonType::Dark, PokemonType::Dark) ||
        is(PokemonType::Dark, PokemonType::Steel) ||
        is(PokemonType::Dark, PokemonType::Fairy) ||
        is(PokemonType::Steel, PokemonType::Fire) ||
        is(PokemonType::Steel, PokemonType::Water) ||
        is(PokemonType::Steel, PokemonType::Electric) ||
        is(PokemonType::Steel, PokemonType::Steel) ||
        is(PokemonType::Fairy, PokemonType::Fire) ||
        is(PokemonType::Fairy, PokemonType::Poison) ||
        is(PokemonType::Fairy, PokemonType::Steel);
    if (resist) value = static_cast<uint16_t>(value / 2U);
  };
  apply(defender.first);
  if (defender.second != defender.first) apply(defender.second);
  return value;
}

bool isRecoveryMove(const MoveAiData& move) {
  return effectIs(move, "RESTORE_HP") || effectIs(move, "SOFTBOILED") ||
      effectIs(move, "RESTORE_HALF_HP") || effectIs(move, "SYNTHESIS") ||
      effectIs(move, "MORNING_SUN") || effectIs(move, "MOONLIGHT") ||
      effectIs(move, "ROOST") || effectIs(move, "REST") ||
      effectIs(move, "STRENGTH_SAP") || effectIs(move, "WISH");
}

bool isProtectMove(const MoveAiData& move) {
  return effectIs(move, "PROTECT") || effectIs(move, "KINGS_SHIELD") ||
      effectIs(move, "SPIKY_SHIELD");
}

bool isPrimaryStatusMove(const MoveAiData& move) {
  if (move.category != MoveCategory::Status) return false;
  return effectIs(move, "SLEEP") || effectIs(move, "POISON") ||
      effectIs(move, "TOXIC") || effectIs(move, "PARALYZE") ||
      effectIs(move, "BURN") || effectIs(move, "FREEZE");
}

bool isCleanseMove(const MoveAiData& move) {
  return effectIs(move, "REFRESH") || effectIs(move, "HEAL_BELL") ||
      effectIs(move, "AROMATHERAPY");
}

bool isDrainMove(const MoveAiData& move) {
  return effectIs(move, "ABSORB") || effectContains(move, "DRAINING");
}

bool isRecoilMove(const MoveAiData& move) {
  return effectContains(move, "RECOIL") || effectIs(move, "DOUBLE_EDGE");
}

bool isPivotMove(const MoveAiData& move) {
  return effectIs(move, "SWITCH_HIT") || effectIs(move, "PARTING_SHOT");
}

bool isSetupEffect(const MoveAiData& move) {
  return effectContains(move, "_UP") || effectIs(move, "BULK_UP") ||
      effectIs(move, "CALM_MIND") || effectIs(move, "DRAGON_DANCE") ||
      effectIs(move, "QUIVER_DANCE") || effectIs(move, "SHELL_SMASH") ||
      effectIs(move, "GEOMANCY") || effectIs(move, "COIL") ||
      effectIs(move, "WORK_UP") || effectIs(move, "GROWTH") ||
      effectIs(move, "COSMIC_POWER") || effectIs(move, "SHIFT_GEAR");
}

uint8_t usefulSetupStats(const MoveAiData& move,
                         const CombatVolatile* state) {
  if (!state) return isSetupEffect(move) ? 1U : 0U;
  uint8_t useful = 0U;
  const auto room = [&](int8_t stage) {
    if (stage < kMaximumBattleStatStage) ++useful;
  };
  if (effectIs(move, "BULK_UP")) {
    room(state->attackStage); room(state->defenseStage);
  } else if (effectIs(move, "CALM_MIND")) {
    room(state->spAttackStage); room(state->spDefenseStage);
  } else if (effectIs(move, "DRAGON_DANCE") || effectIs(move, "SHIFT_GEAR")) {
    room(state->attackStage); room(state->speedStage);
  } else if (effectIs(move, "QUIVER_DANCE") || effectIs(move, "GEOMANCY")) {
    room(state->spAttackStage); room(state->spDefenseStage);
    room(state->speedStage);
  } else if (effectIs(move, "SHELL_SMASH")) {
    room(state->attackStage); room(state->spAttackStage); room(state->speedStage);
  } else if (effectIs(move, "COSMIC_POWER")) {
    room(state->defenseStage); room(state->spDefenseStage);
  } else if (effectContains(move, "SPECIAL_ATTACK_UP")) {
    room(state->spAttackStage);
  } else if (effectContains(move, "SPECIAL_DEFENSE_UP")) {
    room(state->spDefenseStage);
  } else if (effectContains(move, "ATTACK_UP")) {
    room(state->attackStage);
  } else if (effectContains(move, "DEFENSE_UP")) {
    room(state->defenseStage);
  } else if (effectContains(move, "SPEED_UP")) {
    room(state->speedStage);
  } else if (effectContains(move, "ACCURACY_UP")) {
    room(state->accuracyStage);
  } else if (effectContains(move, "EVASION_UP")) {
    room(state->evasionStage);
  } else if (effectIs(move, "COIL")) {
    room(state->attackStage); room(state->defenseStage); room(state->accuracyStage);
  } else if (effectIs(move, "WORK_UP") || effectIs(move, "GROWTH")) {
    room(state->attackStage); room(state->spAttackStage);
  }
  return useful;
}

bool primaryStatusIsImmune(const MoveAiData& move, SpeciesTypes target) {
  const auto has = [&](PokemonType type) {
    return target.first == type || target.second == type;
  };
  if ((effectIs(move, "POISON") || effectIs(move, "TOXIC")) &&
      (has(PokemonType::Poison) || has(PokemonType::Steel))) return true;
  if (effectIs(move, "BURN") && has(PokemonType::Fire)) return true;
  if (effectIs(move, "PARALYZE") && has(PokemonType::Electric)) return true;
  if (effectIs(move, "FREEZE") && has(PokemonType::Ice)) return true;
  return false;
}

bool passesHardRestrictions(const OwnedPokemon& pokemon, uint8_t slot,
                            const MoveAiData* move,
                            const MoveChoiceContext& context) {
  if (!move || slot >= kMoveSlots || !pokemon.movePp[slot]) return false;
  const CombatVolatile* volatileState = context.userVolatile;
  if (volatileState) {
    if (volatileState->disabledMove == pokemon.moves[slot]) return false;
    if (volatileState->encoreMove != MoveId::None &&
        volatileState->encoreMove != pokemon.moves[slot]) return false;
    if (volatileState->choiceMove != MoveId::None &&
        volatileState->choiceMove != pokemon.moves[slot]) return false;
  }
  if (context.userMoveEffects) {
    if (context.userMoveEffects->tauntTurns &&
        move->category == MoveCategory::Status) return false;
    if (context.userMoveEffects->tormented && volatileState &&
        volatileState->lastMoveUsed == pokemon.moves[slot]) return false;
  }
  return true;
}

int32_t scoreMove(const OwnedPokemon& pokemon, const MoveAiData& move,
                  const MoveChoiceContext& context) {
  const OwnedPokemon* opponent = context.opponent;
  const CombatVolatile* userState = context.userVolatile;
  const CombatVolatile* targetState = context.opponentVolatile;
  const MoveId lastMove = userState ? userState->lastMoveUsed : MoveId::None;
  const uint32_t maximumHp = std::max<uint16_t>(1U, pokemon.maximumHp);
  const uint32_t missingPercent =
      static_cast<uint32_t>(pokemon.maximumHp -
          std::min(pokemon.currentHp, pokemon.maximumHp)) * 100U / maximumHp;
  const SpeciesTypes userTypes = speciesTypes(&pokemon, context.userMoveEffects);
  const SpeciesTypes targetTypes =
      speciesTypes(opponent, context.opponentMoveEffects);

  if (isRecoveryMove(move)) {
    if (!pokemon.currentHp || pokemon.currentHp >= pokemon.maximumHp) return INT32_MIN;
    if (context.userGen4Effects && context.userGen4Effects->healBlockTurns)
      return INT32_MIN;
    if (effectIs(move, "STRENGTH_SAP") && targetState &&
        targetState->attackStage <= kMinimumBattleStatStage) return INT32_MIN;
    // Healing becomes progressively urgent. At 75% HP it remains behind a
    // strong attack; below half it competes, and in the red it is preferred.
    int32_t score = 20 + static_cast<int32_t>(missingPercent) * 4;
    if (pokemon.status != StatusCondition::None && effectIs(move, "REST"))
      score += 35;
    if (effectIs(move, "WISH")) score -= 20;
    if (lastMove == static_cast<MoveId>(move.id)) score -= 18;
    return score;
  }

  if (isCleanseMove(move))
    return pokemon.status == StatusCondition::None ? INT32_MIN : 150;

  if (isPrimaryStatusMove(move)) {
    if (!opponent || opponent->status != StatusCondition::None ||
        primaryStatusIsImmune(move, targetTypes)) return INT32_MIN;
    const uint8_t accuracy = (!move.accuracy || move.accuracy > 100U)
        ? 100U : move.accuracy;
    int32_t score = effectIs(move, "SLEEP") ? 145 : 118;
    score = score * accuracy / 100;
    if (lastMove == static_cast<MoveId>(move.id)) score -= 35;
    return score;
  }

  if (effectIs(move, "LEECH_SEED")) {
    if (targetState && targetState->seeded) return INT32_MIN;
    return lastMove == static_cast<MoveId>(move.id) ? 20 : 112;
  }
  if (effectIs(move, "ENCORE")) {
    if (!targetState || targetState->lastMoveUsed == MoveId::None ||
        targetState->encoreMove != MoveId::None) return INT32_MIN;
    return 88;
  }
  if (effectIs(move, "DREAM_EATER") &&
      (!opponent || opponent->status != StatusCondition::Sleep))
    return INT32_MIN;
  if (effectIs(move, "FIRST_IMPRESSION") &&
      userState && userState->lastMoveUsed != MoveId::None)
    return INT32_MIN;
  if (effectIs(move, "COUNTER")) {
    if (!context.userMoveEffects ||
        !context.userMoveEffects->lastDamageReceived ||
        !context.userMoveEffects->lastDamageWasPhysical) return INT32_MIN;
    return 210;
  }
  if (effectIs(move, "MIRROR_COAT")) {
    if (!context.userMoveEffects ||
        !context.userMoveEffects->lastDamageReceived ||
        context.userMoveEffects->lastDamageWasPhysical) return INT32_MIN;
    return 210;
  }

  if (isProtectMove(move)) {
    if (lastMove == static_cast<MoveId>(move.id)) return 12;
    const uint32_t hpPercent = pokemon.currentHp * 100U / maximumHp;
    return hpPercent < 35U ? 72 : 48;
  }

  const uint8_t usefulStats = usefulSetupStats(move, userState);
  if (isSetupEffect(move) && move.category == MoveCategory::Status) {
    if (!usefulStats) return INT32_MIN;
    const uint32_t hpPercent = pokemon.currentHp * 100U / maximumHp;
    int32_t score = hpPercent >= 70U ? 128 : hpPercent >= 45U ? 92 : 38;
    score += static_cast<int32_t>(usefulStats - 1U) * 12;
    if (lastMove == static_cast<MoveId>(move.id)) score -= 34;
    return score;
  }

  if (!move.power) {
    int32_t score = effectIs(move, "STEALTH_ROCK") ? 86 :
        effectIs(move, "DESTINY_BOND") ? 75 :
        effectIs(move, "PERISH_SONG") ? 70 : 55;
    if (lastMove == static_cast<MoveId>(move.id)) score -= 50;
    return score;
  }

  uint16_t power = move.power;
  if (power == 1U) {
    if (effectIs(move, "RETURN")) power = 102U;
    else if (effectIs(move, "GYRO_BALL") || effectIs(move, "HEAT_CRASH"))
      power = 80U;
    else power = 60U;
  }
  if (effectIs(move, "ERUPTION"))
    power = static_cast<uint16_t>(std::max<uint32_t>(1U,
        static_cast<uint32_t>(power) * pokemon.currentHp / maximumHp));
  const uint8_t accuracy = (!move.accuracy || move.accuracy > 100U)
      ? 100U : move.accuracy;
  int32_t score = static_cast<int32_t>(power) * accuracy / 100;
  if (move.type == userTypes.first || move.type == userTypes.second)
    score = score * 3 / 2;
  const uint16_t effectiveness = opponent
      ? typeEffectiveness(move.type, targetTypes) : 100U;
  score = score * effectiveness / 100;
  if (!effectiveness) return INT32_MIN;

  if (move.category == MoveCategory::Physical &&
      pokemon.evs.attack > pokemon.evs.spAttack) score += 10;
  else if (move.category == MoveCategory::Special &&
           pokemon.evs.spAttack > pokemon.evs.attack) score += 10;
  if (move.priority > 0) {
    score += static_cast<int32_t>(move.priority) * 14;
    if (opponent && opponent->maximumHp &&
        opponent->currentHp * 4U <= opponent->maximumHp) score += 30;
  } else if (move.priority < 0) {
    score += static_cast<int32_t>(move.priority) * 4;
  }
  if (isDrainMove(move) && missingPercent) score += 18;
  if (isRecoilMove(move) && pokemon.currentHp * 3U < maximumHp) score -= 45;
  if (isPivotMove(move)) {
    if (!context.healthyReserves) return INT32_MIN;
    score += pokemon.currentHp * 2U < maximumHp ? 32 : 8;
  }
  if (effectIs(move, "OVERHEAT") || effectIs(move, "SUPERPOWER") ||
      effectIs(move, "CLOSE_COMBAT")) {
    if (lastMove == static_cast<MoveId>(move.id)) score -= 18;
  }
  score += std::min<int32_t>(12, move.effectChance / 10U);
  return score;
}

uint32_t nextRandom(uint32_t& state) {
  if (!state) state = 0x6D2B79F5U;
  state ^= state << 13U;
  state ^= state >> 17U;
  state ^= state << 5U;
  return state;
}

uint16_t maximumHp(const RosterEntry& entry, uint8_t level) {
  return static_cast<uint16_t>(
      (((2U * entry.baseHp + 31U + entry.evs.hp / 4U) * level) / 100U) +
      level + 10U);
}

bool locksAuthoredItem(const RosterEntry& entry) {
  // Ash-Greninja is selected through its permanent Bond item. Replacing it
  // with the generic Mega Stone or a Z-Crystal silently removes the requested
  // form mechanic, so team decoration must leave this authored item intact.
  return entry.ordinaryItem == HeldItem::AshsBond;
}

OwnedPokemon materialize(const RosterEntry& entry, uint8_t rosterIndex,
                         uint8_t level, uint16_t battleSerial,
                         uint8_t teamSlot) {
  OwnedPokemon pokemon{};
  pokemon.uid = 0xB0000000UL |
      (static_cast<uint32_t>(battleSerial) << 4U) |
      static_cast<uint32_t>(teamSlot + 1U);
  pokemon.personality = 0xC0DE0001UL ^
      (static_cast<uint32_t>(rosterIndex + 1U) * 0x9E3779B9UL);
  pokemon.speciesId = entry.speciesId;
  pokemon.experience = 1250000U;
  pokemon.level = std::max<uint8_t>(1U, std::min<uint8_t>(100U, level));
  pokemon.maximumHp = maximumHp(entry, pokemon.level);
  pokemon.currentHp = pokemon.maximumHp;
  pokemon.friendship = 255U;
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    pokemon.moves[slot] = static_cast<MoveId>(entry.moves[slot]);
    pokemon.movePp[slot] = entry.movePp[slot];
  }
  pokemon.abilityId = entry.abilityId;
  pokemon.ivs = IndividualValues{31U, 31U, 31U, 31U, 31U, 31U};
  pokemon.nature = entry.nature;
  pokemon.heldItem = entry.ordinaryItem;
  pokemon.shiny = entry.shiny;
  pokemon.evs = entry.evs;
  return pokemon;
}

}  // namespace

const RosterEntry* roster() { return kRoster; }
size_t rosterSize() { return sizeof(kRoster) / sizeof(kRoster[0]); }

Team buildTeam(uint32_t seed, uint8_t level, uint16_t battleSerial) {
  Team team{};
  uint32_t random = seed ? seed : 0x50474F42U;
  for (uint8_t slot = 0; slot < kTeamSize; ++slot) {
    uint8_t candidate = 0U;
    bool duplicate = false;
    do {
      candidate = static_cast<uint8_t>(nextRandom(random) % kRosterSize);
      duplicate = false;
      for (uint8_t previous = 0; previous < slot; ++previous)
        duplicate = duplicate || team.rosterIndexes[previous] == candidate;
    } while (duplicate);
    team.rosterIndexes[slot] = candidate;
    team.members[slot] = materialize(kRoster[candidate], candidate, level,
                                     battleSerial, slot);
  }

  uint8_t megaCandidates[kTeamSize]{};
  uint8_t megaCount = 0U;
  for (uint8_t slot = 0; slot < kTeamSize; ++slot)
    if (kRoster[team.rosterIndexes[slot]].megaEligible &&
        !locksAuthoredItem(kRoster[team.rosterIndexes[slot]]))
      megaCandidates[megaCount++] = slot;
  if (megaCount) {
    team.megaSlot = megaCandidates[nextRandom(random) % megaCount];
    team.members[team.megaSlot].heldItem = HeldItem::MegaStone;
  }

  uint8_t zCandidates[kTeamSize]{};
  uint8_t zCount = 0U;
  for (uint8_t slot = 0; slot < kTeamSize; ++slot) {
    const RosterEntry& entry = kRoster[team.rosterIndexes[slot]];
    if (slot != team.megaSlot && !locksAuthoredItem(entry) &&
        entry.zCrystal != HeldItem::None &&
        entry.zMoveSlot < kMoveSlots)
      zCandidates[zCount++] = slot;
  }
  if (zCount) {
    team.zSlot = zCandidates[nextRandom(random) % zCount];
    const RosterEntry& entry = kRoster[team.rosterIndexes[team.zSlot]];
    team.zMoveSlot = entry.zMoveSlot;
    team.members[team.zSlot].heldItem = entry.zCrystal;
  }
  return team;
}

MoveChoice chooseMove(const OwnedPokemon& pokemon,
                      const MoveChoiceContext& context) {
  bool knowsMove = false;
  uint8_t hardUsable[kMoveSlots]{};
  uint8_t hardUsableCount = 0U;
  uint8_t bestSlots[kMoveSlots]{};
  uint8_t bestCount = 0U;
  int32_t bestScore = INT32_MIN;
  for (uint8_t slot = 0U; slot < kMoveSlots; ++slot) {
    const MoveAiData* move = moveData(pokemon.moves[slot]);
    if (move) knowsMove = true;
    if (!passesHardRestrictions(pokemon, slot, move, context)) continue;
    hardUsable[hardUsableCount++] = slot;
    const int32_t score = scoreMove(pokemon, *move, context);
    if (score == INT32_MIN) continue;
    if (score > bestScore) {
      bestScore = score;
      bestSlots[0] = slot;
      bestCount = 1U;
    } else if (score == bestScore) {
      bestSlots[bestCount++] = slot;
    }
  }

  MoveChoice choice{};
  if (bestCount) {
    choice.slot = bestSlots[context.randomValue % bestCount];
    choice.score = static_cast<int16_t>(std::max<int32_t>(INT16_MIN,
        std::min<int32_t>(INT16_MAX, bestScore)));
  } else if (hardUsableCount) {
    // A legal command is still required if a pathological imported set has
    // only contextually failing moves. Ordinary authored bot sets always have
    // damaging alternatives, so this cannot reintroduce full-HP recovery.
    choice.slot = hardUsable[context.randomValue % hardUsableCount];
    choice.score = INT16_MIN;
  } else if (knowsMove) {
    // Slot kMoveSlots is Struggle in the shared PvP wire format. Keep it
    // below useful Bag actions so the finite Max Elixir is consumed first.
    choice.slot = kMoveSlots;
    choice.score = 50;
    return choice;
  } else {
    return choice;
  }

  const MoveAiData* selected = moveData(pokemon.moves[choice.slot]);
  choice.useZMove = context.zMoveAvailable &&
      choice.slot == context.zMoveSlot && selected && selected->power != 0U;
  return choice;
}

ItemChoice chooseItem(const ItemChoiceContext& context) {
  ItemChoice best{};
  if (!context.team || !context.bag || !context.teamCount ||
      context.activeSlot >= context.teamCount)
    return best;

  const OwnedPokemon& active = context.team[context.activeSlot];
  const CombatVolatile* activeVolatile = context.volatiles
      ? &context.volatiles[context.activeSlot] : nullptr;
  // BattleEngine uses bits 1..3 of heldItemSuppressed for Embargo. It blocks
  // every Bag item aimed at the active battler, but not a benched target.
  const bool activeEmbargoed = activeVolatile &&
      ((activeVolatile->heldItemSuppressed >> 1U) & 0x07U);
  const auto consider = [&](BattleItem item, uint8_t targetSlot,
                            int32_t score) {
    if (score <= best.score) return;
    best.item = item;
    best.targetSlot = targetSlot;
    best.score = static_cast<int16_t>(std::max<int32_t>(INT16_MIN,
        std::min<int32_t>(INT16_MAX, score)));
    best.valid = true;
  };

  if (!activeEmbargoed && active.currentHp && active.maximumHp) {
    const uint32_t hpPercent = static_cast<uint32_t>(active.currentHp) *
        100U / active.maximumHp;
    const uint32_t missingHp = active.maximumHp -
        std::min(active.currentHp, active.maximumHp);
    if (context.bag->hyperPotions && missingHp) {
      // Do not spend a finite potion for cosmetic chip damage. Below half HP
      // urgency grows sharply, while a natural recovery move can still beat
      // this score and conserve the Bag.
      int32_t score = INT32_MIN;
      if (hpPercent <= 10U) score = 1000;
      else if (hpPercent <= 20U) score = 430;
      else if (hpPercent <= 35U) score = 325;
      else if (hpPercent <= 50U) score = 270;
      else if (hpPercent <= 65U && missingHp >= 100U) score = 205;
      if (score != INT32_MIN)
        consider(BattleItem::HyperPotion, context.activeSlot, score);
    }

    if (context.bag->fullHeals &&
        (active.status != StatusCondition::None ||
         (activeVolatile && activeVolatile->confusionTurns))) {
      int32_t score = 175;
      if (active.status == StatusCondition::Sleep ||
          active.status == StatusCondition::Frozen)
        score = 360;
      else if (activeVolatile && activeVolatile->confusionTurns)
        score = 285;
      else if (active.status == StatusCondition::BadlyPoisoned)
        score = 240;
      else if (active.status == StatusCondition::Paralysis)
        score = 205;
      consider(BattleItem::FullHeal, context.activeSlot, score);
    }

    if (context.bag->maxElixirs) {
      uint16_t currentPp = 0U;
      uint16_t maximumPp = 0U;
      uint8_t exhaustedMoves = 0U;
      for (uint8_t slot = 0U; slot < kMoveSlots; ++slot) {
        const MoveAiData* move = moveData(active.moves[slot]);
        if (!move || !move->pp) continue;
        currentPp = static_cast<uint16_t>(currentPp + active.movePp[slot]);
        maximumPp = static_cast<uint16_t>(maximumPp + move->pp);
        if (!active.movePp[slot]) ++exhaustedMoves;
      }
      if (maximumPp && currentPp < maximumPp) {
        if (!currentPp)
          consider(BattleItem::MaxElixir, context.activeSlot, 600);
        else if (exhaustedMoves >= 3U || currentPp * 5U <= maximumPp)
          consider(BattleItem::MaxElixir, context.activeSlot, 235);
      }
    }
  }

  if (context.bag->maxRevives) {
    uint8_t healthyCount = 0U;
    for (uint8_t slot = 0U; slot < context.teamCount; ++slot)
      if (context.team[slot].currentHp &&
          !context.team[slot].recoverySecondsRemaining)
        ++healthyCount;
    for (uint8_t slot = 0U; slot < context.teamCount; ++slot) {
      const OwnedPokemon& target = context.team[slot];
      if (slot == context.activeSlot ||
          (target.currentHp && !target.recoverySecondsRemaining))
        continue;
      // Reviving the last reserve is more urgent than restoring depth while
      // two healthy members remain, but it must not eclipse a likely KO.
      const int32_t score = healthyCount <= 1U ? 245 : 175;
      consider(BattleItem::MaxRevive, slot, score);
    }
  }
  return best;
}

bool consumeItem(BattleBag& bag, BattleItem item) {
  uint8_t* quantity = nullptr;
  switch (item) {
    case BattleItem::HyperPotion: quantity = &bag.hyperPotions; break;
    case BattleItem::FullHeal: quantity = &bag.fullHeals; break;
    case BattleItem::MaxRevive: quantity = &bag.maxRevives; break;
    case BattleItem::MaxElixir: quantity = &bag.maxElixirs; break;
    default: return false;
  }
  if (!quantity || !*quantity) return false;
  --*quantity;
  return true;
}

}  // namespace PokegochiServerBot
