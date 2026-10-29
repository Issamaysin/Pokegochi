#include "game/KalosAdventure.h"

#include <algorithm>
#include <iterator>

#include "game/Economy.h"
#include "game/Pokedex.h"

namespace {
constexpr uint8_t kRunActive = 1U << 0U;
constexpr uint8_t kHealUsed = 1U << 1U;
constexpr uint8_t kHiddenAbility = 1U << 0U;
constexpr uint8_t kChampionComplete = 1U << 0U;
constexpr uint8_t kXThemeSelected = 1U << 1U;

uint32_t advance(uint32_t& value) {
  if (!value) value = 0x4B414C4FUL;
  value ^= value << 13U;
  value ^= value >> 17U;
  value ^= value << 5U;
  return value;
}

uint32_t mixed(uint32_t value) {
  value ^= value >> 16U;
  value *= 0x7FEB352DUL;
  value ^= value >> 15U;
  value *= 0x846CA68BUL;
  return value ^ (value >> 16U);
}

constexpr uint16_t kAreaRosters[KalosAdventure::kAreaCount][12] = {
  {650,653,656,659,661,664,665,666,669,670,674,676},
  {650,652,659,660,667,668,672,674,676,696,698,701},
  {651,654,657,660,674,675,677,679,686,694,696,701},
  {652,655,658,662,673,675,681,688,690,692,693,700},
  {653,655,659,676,677,678,682,684,686,694,695,702},
  {654,656,670,671,682,683,684,685,686,687,700,702},
  {655,657,678,680,681,686,687,693,695,701,702,703},
  {652,658,673,676,689,691,697,699,701,708,712,713},
  {660,668,675,681,687,689,691,697,700,701,706,715},
};

constexpr uint16_t kBossSpecies[KalosAdventure::kAreaCount][3] = {
  {283,665,666}, {95,696,698}, {67,619,701},
  {70,189,673}, {82,587,695}, {122,303,700},
  {199,561,678}, {460,615,713}, {701,697,282},
};

constexpr uint16_t kLegendaryPools[KalosAdventure::kAreaCount][2] = {
  {719,0}, {719,721}, {716,717}, {720,0}, {720,721},
  {716,717}, {718,0}, {718,721}, {716,717},
};

constexpr const char* kAreaNames[] = {
  "SANTALUNE FOREST", "CYLLAGE CLIFFS", "GEOSENGE TRAIL",
  "COUMARINE COAST", "LUMIOSE CITY", "LAVERRE MARSH",
  "ANISTAR PATH", "SNOWBELLE WOODS", "POKEMON LEAGUE",
};

constexpr const char* kAreaSubtitles[] = {
  "FOREST PATH TO THE BUG BADGE", "FOSSILS BENEATH THE ROCK WALLS",
  "MEGA SECRETS AND TEAM FLARE", "A COAST COVERED IN GREEN",
  "THE LIGHTS OF KALOS", "FAIRY TALES HIDE IN THE FOG",
  "FOLLOW THE CRYSTAL SUNDIAL", "THE LAST PATH IS FROZEN",
  "CHALLENGE THE KALOS CHAMPION",
};

constexpr const char* kBossNames[] = {
  "VIOLA", "GRANT", "KORRINA", "RAMOS", "CLEMONT",
  "VALERIE", "OLYMPIA", "WULFRIC", "DIANTHA",
};

uint8_t normalizedArea(KalosArea area) {
  return std::min<uint8_t>(static_cast<uint8_t>(area),
                           KalosAdventure::kAreaCount - 1U);
}

bool ownsHiddenAbilitySpecies(const PokemonCollection& collection,
                              uint16_t speciesId) {
  for (const OwnedPokemon& pokemon : collection.box)
    if (pokemon.uid != kEmptyPokemonUid && pokemon.speciesId == speciesId &&
        CollectionLogic::hasHiddenAbility(pokemon)) return true;
  return false;
}

uint16_t friendSafariSpecies(const PokemonCollection& collection,
                             uint32_t roll) {
  uint16_t missing = 0U, total = 0U;
  for (uint16_t id = 650U; id <= 715U; ++id) {
    const SpeciesData* species = findSpecies(id);
    if (!species || !species->hiddenAbility ||
        PokedexLogic::isLegendaryOrMythical(id)) continue;
    ++total;
    if (!ownsHiddenAbilitySpecies(collection, id)) ++missing;
  }
  const bool missingOnly = missing != 0U;
  uint16_t selected = static_cast<uint16_t>(roll % (missingOnly ? missing : total));
  for (uint16_t id = 650U; id <= 715U; ++id) {
    const SpeciesData* species = findSpecies(id);
    if (!species || !species->hiddenAbility ||
        PokedexLogic::isLegendaryOrMythical(id) ||
        (missingOnly && ownsHiddenAbilitySpecies(collection, id))) continue;
    if (!selected--) return id;
  }
  return 0U;
}

uint8_t levelForArea(const PokemonCollection& collection, uint8_t area) {
  const uint8_t party = std::max<uint8_t>(5U,
      BattleEngine::highestPartyLevel(collection));
  return std::min<uint8_t>(100U,
      std::max<uint8_t>(party, static_cast<uint8_t>(70U + area * 3U)));
}

bool healthyPlayer(const KalosRunState& run,
                   const PokemonCollection& collection, uint32_t uid) {
  bool selected = false;
  for (uint32_t member : run.partyUids)
    if (member == uid && uid != kEmptyPokemonUid) selected = true;
  const OwnedPokemon* pokemon = CollectionLogic::find(collection, uid);
  return selected && pokemon && CollectionLogic::isInParty(collection, uid) &&
      pokemon->currentHp && !pokemon->recoverySecondsRemaining;
}

void initializeBattle(BattleState& battle, uint32_t playerUid,
                      uint32_t seed, BattleKind kind) {
  BattleEngine::clear(battle);
  battle.active = true;
  battle.kind = kind;
  battle.outcome = BattleOutcome::Ongoing;
  battle.playerUid = playerUid;
  battle.unlockedGeneration = 6U;
  battle.rngState = seed ? seed : 0x4B414C4FUL;
}
} // namespace

bool KalosAdventure::available(const UnovaAdventureState& unova) {
  return UnovaAdventure::championsCompleted(unova);
}

bool KalosAdventure::reconcile(KalosAdventureState& state,
                               const UnovaAdventureState& unova) {
  if (!available(unova) || state.unlockedAreas) return false;
  state.unlockedAreas = 1U;
  if (!state.rngState) state.rngState = 0x4B414C4FUL;
  return true;
}

bool KalosAdventure::isAreaUnlocked(const KalosAdventureState& state,
                                    KalosArea area) {
  const uint8_t index = static_cast<uint8_t>(area);
  return index < kAreaCount && (state.unlockedAreas & (1U << index));
}

bool KalosAdventure::championCompleted(const KalosAdventureState& state) {
  return (state.flags & kChampionComplete) != 0U;
}

bool KalosAdventure::fullyCompleted(const KalosAdventureState& state) {
  return championCompleted(state) &&
      (state.defeatedBosses & kAllAreasMask) == kAllAreasMask;
}

bool KalosAdventure::reconcileCompletionReward(
    KalosAdventureState& state, Inventory& inventory) {
  if (!fullyCompleted(state) ||
      (state.flags & kMasterBallRewardBit) != 0U)
    return false;
  uint16_t& masterBalls =
      inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)];
  if (masterBalls >= kInventoryStackLimit) return false;
  ++masterBalls;
  state.flags = static_cast<uint8_t>(state.flags | kMasterBallRewardBit);
  return true;
}

bool KalosAdventure::xThemeUnlocked(const KalosAdventureState& state) {
  return championCompleted(state);
}

bool KalosAdventure::xThemeSelected(const KalosAdventureState& state) {
  return xThemeUnlocked(state) && (state.flags & kXThemeSelected);
}

bool KalosAdventure::setXThemeSelected(KalosAdventureState& state,
                                       bool selected) {
  if (selected && !xThemeUnlocked(state)) return false;
  const uint8_t before = state.flags;
  if (selected) state.flags |= kXThemeSelected;
  else state.flags &= static_cast<uint8_t>(~kXThemeSelected);
  return before != state.flags;
}

uint8_t KalosAdventure::badgeCount(const KalosAdventureState& state) {
  uint8_t count = 0U;
  for (uint8_t area = 0U; area < 8U; ++area)
    if (state.defeatedBosses & (1U << area)) ++count;
  return count;
}

const char* KalosAdventure::areaName(KalosArea area) {
  return kAreaNames[normalizedArea(area)];
}

const char* KalosAdventure::areaSubtitle(KalosArea area) {
  return kAreaSubtitles[normalizedArea(area)];
}

const char* KalosAdventure::bossName(KalosArea area) {
  return kBossNames[normalizedArea(area)];
}

bool KalosAdventure::begin(KalosAdventureState& state, KalosArea area,
                           uint32_t seed, const PokemonCollection& collection) {
  if (!isAreaUnlocked(state, area)) return false;
  bool ready = false;
  for (uint8_t slot = 0U; slot < kPartyCapacity; ++slot) {
    const OwnedPokemon* pokemon = CollectionLogic::active(collection, slot);
    ready = ready || (pokemon && pokemon->currentHp &&
                      !pokemon->recoverySecondsRemaining);
  }
  if (!ready) return false;
  state.run = KalosRunState{};
  state.run.seed = seed ? seed : mixed(advance(state.rngState));
  state.run.area = normalizedArea(area);
  state.run.flags = kRunActive;
  std::copy(std::begin(collection.party), std::end(collection.party),
            std::begin(state.run.partyUids));
  return true;
}

bool KalosAdventure::abandon(KalosAdventureState& state) {
  if (!active(state)) return false;
  state.run = KalosRunState{};
  return true;
}

bool KalosAdventure::active(const KalosAdventureState& state) {
  return (state.run.flags & kRunActive) != 0U;
}

uint8_t KalosAdventure::choices(const KalosAdventureState& state,
                                KalosRoomChoice output[2]) {
  if (!output || !active(state) ||
      state.run.pendingBattle != KalosBattleRole::None) return 0U;
  if (state.run.step >= kStepsPerArea) {
    output[0] = {KalosRoomKind::Leader, 0U, true};
    return 1U;
  }
  const uint32_t roll = mixed(state.run.seed ^
      (static_cast<uint32_t>(state.run.step + 1U) * 0x9E3779B9UL));
  const bool flareArea = state.run.area == 2U || state.run.area == 4U ||
                         state.run.area == 6U;
  output[0] = {flareArea && state.run.step == 3U
                   ? KalosRoomKind::TeamFlare : KalosRoomKind::Trainer,
               0U, flareArea && state.run.step == 3U};
  static constexpr KalosRoomKind kWildRooms[] = {
      KalosRoomKind::TallGrass, KalosRoomKind::FlowerPatch,
      KalosRoomKind::Horde, KalosRoomKind::FriendSafari,
      KalosRoomKind::Item, KalosRoomKind::Healing,
  };
  KalosRoomKind alternate = kWildRooms[(roll >> 5U) % 6U];
  if (alternate == KalosRoomKind::Healing && (state.run.flags & kHealUsed))
    alternate = KalosRoomKind::FlowerPatch;
  output[1] = {alternate, 1U,
               alternate == KalosRoomKind::FriendSafari};
  return 2U;
}

KalosRoomResult KalosAdventure::choose(KalosAdventureState& state,
                                       uint8_t choice,
                                       PokemonCollection& collection) {
  KalosRoomResult result;
  KalosRoomChoice options[2]{};
  const uint8_t count = choices(state, options);
  if (choice >= count) return result;
  result.kind = options[choice].kind;
  const uint32_t roll = mixed(state.run.seed ^
      (static_cast<uint32_t>(state.run.step + 1U) << 16U) ^ choice);
  const uint8_t area = std::min<uint8_t>(state.run.area, kAreaCount - 1U);
  switch (result.kind) {
    case KalosRoomKind::Healing:
      result.healed = useAreaHeal(state, collection);
      break;
    case KalosRoomKind::Item:
      result.foundItem = true;
      result.rewardKind = static_cast<uint8_t>(1U + ((roll >> 6U) % 3U));
      break;
    case KalosRoomKind::Trainer:
      state.run.pendingBattle = KalosBattleRole::Trainer;
      result.startsBattle = true;
      break;
    case KalosRoomKind::TeamFlare:
      state.run.pendingBattle = KalosBattleRole::TeamFlare;
      result.startsBattle = true;
      break;
    case KalosRoomKind::Leader:
      state.run.pendingBattle = KalosBattleRole::Leader;
      result.startsBattle = true;
      break;
    default: {
      uint16_t species = 0U;
      bool hidden = result.kind == KalosRoomKind::FriendSafari;
      bool legendary = !hidden && (roll % kLegendaryChanceDenominator == 0U);
      if (hidden) species = friendSafariSpecies(collection, roll >> 7U);
      if (legendary) {
        species = kLegendaryPools[area][(roll >> 10U) & 1U];
        if (!species) legendary = false;
      }
      if (!species) species = kAreaRosters[area][(roll >> 8U) % 12U];
      state.run.pendingSpecies = species;
      state.run.pendingFlags = hidden ? kHiddenAbility : 0U;
      state.run.pendingBattle = KalosBattleRole::Wild;
      result.startsBattle = true;
      result.speciesId = species;
      result.hiddenAbilityEncounter = hidden;
      result.legendaryEncounter = legendary;
      break;
    }
  }
  if (!result.startsBattle && state.run.step < kStepsPerArea) ++state.run.step;
  return result;
}

bool KalosAdventure::startPendingBattle(KalosAdventureState& state,
                                        BattleState& battle,
                                        PokemonCollection& collection,
                                        uint32_t playerUid) {
  if (!active(state) || state.run.pendingBattle == KalosBattleRole::None ||
      battle.active || !healthyPlayer(state.run, collection, playerUid)) return false;
  const uint8_t area = std::min<uint8_t>(state.run.area, kAreaCount - 1U);
  const uint8_t level = levelForArea(collection, area);
  const uint32_t seed = mixed(state.run.seed ^ state.run.step ^
      (static_cast<uint32_t>(state.run.pendingBattle) << 24U));
  if (state.run.pendingBattle == KalosBattleRole::Wild) {
    initializeBattle(battle, playerUid, seed, BattleKind::Wild);
    OwnedPokemon opponent = CollectionLogic::createPokemon(
        0U, state.run.pendingSpecies, level,
        CollectionLogic::isShinyRoll(seed >> 3U), seed, 6U);
    if (!opponent.speciesId) {
      BattleEngine::clear(battle);
      return false;
    }
    if (state.run.pendingFlags & kHiddenAbility)
      CollectionLogic::setHiddenAbility(opponent, true, 6U);
    battle.opponents[0] = opponent;
    battle.opponentCount = 1U;
    battle.opponentIndex = 0U;
  } else {
    initializeBattle(battle, playerUid, seed, BattleKind::KalosAdventure);
    const bool boss = state.run.pendingBattle == KalosBattleRole::Leader;
    const bool flare = state.run.pendingBattle == KalosBattleRole::TeamFlare;
    battle.opponentCount = boss ? 3U : flare ? 2U : 1U;
    battle.rewardMoney = static_cast<uint32_t>(level) *
        (boss ? 140U : flare ? 90U : 55U);
    for (uint8_t slot = 0U; slot < battle.opponentCount; ++slot) {
      const uint16_t species = boss ? kBossSpecies[area][slot] :
          kAreaRosters[area][(seed + slot * 5U) % 12U];
      battle.opponents[slot] = CollectionLogic::createPokemon(
          0U, species, static_cast<uint8_t>(std::min<uint16_t>(
              100U, level + (boss ? 2U : flare ? 1U : 0U))),
          false, seed ^ (slot * 0x9E3779B9UL), 6U);
    }
    BattleEngine::orderOpponentTeamWeakestFirst(battle);
  }
  BattleEngine::ensureOpponentUids(battle);
  BattleEngine::applyEntryAbilities(battle, collection);
  return true;
}

bool KalosAdventure::recordBattle(KalosAdventureState& state,
                                  BattleOutcome outcome) {
  if (!active(state) || state.run.pendingBattle == KalosBattleRole::None ||
      outcome == BattleOutcome::Ongoing || outcome == BattleOutcome::None)
    return false;
  const KalosBattleRole role = state.run.pendingBattle;
  state.run.pendingBattle = KalosBattleRole::None;
  state.run.pendingSpecies = 0U;
  state.run.pendingFlags = 0U;
  if (outcome != BattleOutcome::Victory && outcome != BattleOutcome::Captured) {
    state.run = KalosRunState{};
    return true;
  }
  if (role == KalosBattleRole::Leader) {
    const uint8_t area = std::min<uint8_t>(state.run.area, kAreaCount - 1U);
    state.defeatedBosses |= static_cast<uint16_t>(1U << area);
    if (area + 1U < kAreaCount)
      state.unlockedAreas |= static_cast<uint16_t>(1U << (area + 1U));
    else
      state.flags |= kChampionComplete;
    state.run = KalosRunState{};
    return true;
  }
  if (state.run.step < kStepsPerArea) ++state.run.step;
  return true;
}

bool KalosAdventure::useAreaHeal(KalosAdventureState& state,
                                 PokemonCollection& collection) {
  if (!active(state) || (state.run.flags & kHealUsed)) return false;
  for (uint32_t uid : state.run.partyUids) {
    OwnedPokemon* pokemon = CollectionLogic::find(collection, uid);
    if (!pokemon) continue;
    const bool incapacitated = pokemon->currentHp == 0U ||
        pokemon->recoverySecondsRemaining != 0U;
    if (!incapacitated) pokemon->currentHp = pokemon->maximumHp;
    pokemon->status = StatusCondition::None;
    for (uint8_t move = 0U; move < kMoveSlots; ++move)
      pokemon->movePp[move] = CollectionLogic::maximumMovePp(*pokemon, move);
  }
  state.run.flags |= kHealUsed;
  return true;
}
