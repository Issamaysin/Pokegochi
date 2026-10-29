#include "game/UnovaAdventure.h"

#include <algorithm>
#include <iterator>

#include "game/Economy.h"
#include "game/Pokedex.h"

namespace {
constexpr uint8_t kRunActive = 1U << 0U;
constexpr uint8_t kHealUsed = 1U << 1U;
constexpr uint8_t kGateDefeated = 1U << 2U;
constexpr uint8_t kBossAvailable = 1U << 3U;
constexpr uint8_t kChampionsComplete = 1U << 0U;
constexpr uint8_t kWhite2Theme = 1U << 1U;
constexpr uint8_t kWhite2ThemeSelected = 1U << 2U;

uint32_t advance(uint32_t& value) {
  if (!value) value = 0x554E4F56UL;
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

constexpr uint16_t kAreaRosters[UnovaAdventure::kAreaCount][12] = {
  {495,498,501,504,511,513,515,519,540,543,546,547},
  {524,527,529,532,551,552,553,554,556,557,559,561},
  {525,526,529,530,599,600,601,602,603,604,605,606},
  {507,508,522,523,529,536,550,568,569,588,589,618},
  {517,518,561,562,563,574,575,576,577,578,607,608},
  {525,530,532,533,534,582,583,584,613,614,615,621},
  {535,536,537,538,539,550,564,565,580,581,616,617},
  {551,552,553,554,555,556,557,558,559,560,631,632},
  {583,584,599,600,601,610,611,612,615,621,623,625},
  {510,512,521,528,530,573,579,596,609,620,624,635},
};

constexpr uint16_t kLegendaryPools[UnovaAdventure::kAreaCount][3] = {
  {494, 640, 0}, {494, 0, 0}, {638, 0, 0}, {641, 642, 645},
  {648, 0, 0}, {638, 647, 0}, {639, 640, 0}, {645, 0, 0},
  {643, 644, 646}, {647, 648, 649},
};

constexpr uint16_t kBossSpecies[UnovaAdventure::kAreaCount][3] = {
  {505, 508, 512}, {531, 538, 542}, {526, 601, 604}, {530, 537, 553},
  {561, 576, 609}, {584, 615, 621}, {589, 617, 623}, {612, 625, 635},
  {637, 643, 644}, {380, 445, 637},
};

struct SpeciesRange {
  uint16_t first;
  uint16_t last;
};

SpeciesRange generationSpeciesRange(uint8_t generation) {
  switch (generation) {
    case 1U: return {1U, 151U};
    case 2U: return {152U, 251U};
    case 3U: return {252U, 386U};
    case 4U: return {387U, 493U};
    default: return {494U, 649U};
  }
}

bool ownsHiddenAbilitySpecies(const PokemonCollection& collection,
                              uint16_t speciesId) {
  for (const OwnedPokemon& pokemon : collection.box) {
    if (pokemon.uid != kEmptyPokemonUid && pokemon.speciesId == speciesId &&
        CollectionLogic::hasHiddenAbility(pokemon)) return true;
  }
  return false;
}

bool eligibleHiddenGrottoSpecies(const PokemonCollection& collection,
                                 uint16_t speciesId, bool legendaryOnly,
                                 bool missingOnly) {
  const SpeciesData* species = findSpecies(speciesId);
  return species && species->hiddenAbility &&
      PokedexLogic::isLegendaryOrMythical(speciesId) == legendaryOnly &&
      (!missingOnly || !ownsHiddenAbilitySpecies(collection, speciesId));
}

uint16_t hiddenGrottoSpecies(const PokemonCollection& collection,
                             uint8_t generation, bool legendaryOnly,
                             uint32_t roll) {
  const SpeciesRange range = generationSpeciesRange(generation);
  uint16_t missingCount = 0U;
  uint16_t totalCount = 0U;
  for (uint16_t speciesId = range.first; speciesId <= range.last; ++speciesId) {
    if (!eligibleHiddenGrottoSpecies(
            collection, speciesId, legendaryOnly, false)) continue;
    ++totalCount;
    if (eligibleHiddenGrottoSpecies(
            collection, speciesId, legendaryOnly, true)) ++missingCount;
  }

  // Prefer a species whose Hidden-Ability variant is not already owned. Once
  // that generation is complete, the whole legal pool becomes replayable.
  const bool missingOnly = missingCount != 0U;
  const uint16_t candidateCount = missingOnly ? missingCount : totalCount;
  if (!candidateCount) return 0U;
  uint16_t selected = static_cast<uint16_t>(roll % candidateCount);
  for (uint16_t speciesId = range.first; speciesId <= range.last; ++speciesId) {
    if (!eligibleHiddenGrottoSpecies(
            collection, speciesId, legendaryOnly, missingOnly))
      continue;
    if (!selected--) return speciesId;
  }
  return 0U;
}

constexpr const char* kAreaNames[] = {
  "PINWHEEL FOREST", "DESERT RESORT", "CHARGESTONE CAVE",
  "DRIFTVEIL PASS", "CELESTIAL TOWER", "TWIST MOUNTAIN",
  "MOOR OF ICIRRUS", "REVERSAL MOUNTAIN", "GIANT CHASM",
  "WHITE TREEHOLLOW",
};
constexpr const char* kAreaSubtitles[] = {
  "SHAKING GRASS AND HIDDEN PATHS", "DUST CLOUDS AMONG THE RUINS",
  "CHARGED CRYSTALS SPLIT THE ROAD", "SHADOWS CROSS THE OLD BRIDGE",
  "CLIMB TOWARD THE SPIRIT BELL", "ICE TUNNELS CHANGE EVERY VISIT",
  "BUBBLING WATER HIDES THE ROUTE", "HEAT AND ASH CONCEAL THE GATE",
  "A FROZEN PATH INTO THE UNKNOWN", "TEN FLOORS. ONE FINAL GATE.",
};
constexpr const char* kBossNames[] = {
  "CILAN", "LENORA", "BURGH", "ELESA", "CLAY",
  "SKYLA", "BRYCEN", "DRAYDEN", "GHETSIS", "BENGA",
};

uint8_t normalizedArea(UnovaArea area) {
  return std::min<uint8_t>(static_cast<uint8_t>(area),
                           UnovaAdventure::kAreaCount - 1U);
}

constexpr uint16_t gateRouteVisitedBit(uint8_t route) {
  // Area traversal consumes bits 0..7. Keep the three Gate-floor branches in
  // the spare high bits so a defeated decoy Trainer cannot be farmed forever.
  return static_cast<uint16_t>(1U << (8U + std::min<uint8_t>(route, 2U)));
}

uint8_t levelForArea(const PokemonCollection& collection, uint8_t area) {
  const uint8_t party = std::max<uint8_t>(5U,
      BattleEngine::highestPartyLevel(collection));
  return std::min<uint8_t>(100U,
      std::max<uint8_t>(party, static_cast<uint8_t>(55U + area * 4U)));
}

bool healthyPlayer(const UnovaRunState& run,
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
  battle.unlockedGeneration = 5U;
  battle.rngState = seed ? seed : 0x554E4F56UL;
}
} // namespace

bool UnovaAdventure::available(const SinnohExpeditionState& sinnoh) {
  return SinnohExpedition::completed(sinnoh);
}

bool UnovaAdventure::reconcile(UnovaAdventureState& state,
                               const SinnohExpeditionState& sinnoh) {
  if (!available(sinnoh) || state.unlockedAreas) return false;
  state.unlockedAreas = 1U;
  if (!state.rngState) state.rngState = 0x554E4F56UL;
  return true;
}

bool UnovaAdventure::isAreaUnlocked(const UnovaAdventureState& state,
                                    UnovaArea area) {
  const uint8_t index = static_cast<uint8_t>(area);
  return index < kAreaCount && (state.unlockedAreas & (1U << index));
}

bool UnovaAdventure::allAreasCompleted(const UnovaAdventureState& state) {
  return (state.defeatedBosses & kAllAreasMask) == kAllAreasMask;
}
bool UnovaAdventure::championsCompleted(const UnovaAdventureState& state) {
  return (state.flags & kChampionsComplete) != 0U;
}
bool UnovaAdventure::royalUnovaCompletedAtLeastOnce(
    const UnovaAdventureState& state) {
  return state.royalUnovaLastDay != UINT32_MAX;
}
bool UnovaAdventure::fullyCompleted(const UnovaAdventureState& state) {
  return allAreasCompleted(state) &&
      (state.pwtCompletedTiers & 0x1FU) == 0x1FU &&
      championsCompleted(state) && royalUnovaCompletedAtLeastOnce(state);
}
bool UnovaAdventure::reconcileCompletionReward(
    UnovaAdventureState& state, Inventory& inventory) {
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
bool UnovaAdventure::white2ThemeUnlocked(const UnovaAdventureState& state) {
  return (state.flags & kWhite2Theme) != 0U;
}
bool UnovaAdventure::royalUnovaUnlocked(const UnovaAdventureState& state) {
  return championsCompleted(state);
}
bool UnovaAdventure::white2ThemeSelected(const UnovaAdventureState& state) {
  return white2ThemeUnlocked(state) && (state.flags & kWhite2ThemeSelected);
}
bool UnovaAdventure::setWhite2ThemeSelected(UnovaAdventureState& state,
                                             bool selected) {
  if (selected && !white2ThemeUnlocked(state)) return false;
  const uint8_t before = state.flags;
  if (selected) state.flags |= kWhite2ThemeSelected;
  else state.flags &= ~kWhite2ThemeSelected;
  return before != state.flags;
}

const char* UnovaAdventure::areaName(UnovaArea area) {
  return kAreaNames[normalizedArea(area)];
}
const char* UnovaAdventure::areaSubtitle(UnovaArea area) {
  return kAreaSubtitles[normalizedArea(area)];
}

uint8_t UnovaAdventure::hiddenGrottoGeneration(UnovaArea area) {
  const uint8_t index = std::min<uint8_t>(
      static_cast<uint8_t>(area), kAreaCount - 1U);
  return static_cast<uint8_t>(index / 2U + 1U);
}
const char* UnovaAdventure::bossName(UnovaArea area) {
  return kBossNames[normalizedArea(area)];
}

bool UnovaAdventure::begin(UnovaAdventureState& state, UnovaArea area,
                           uint32_t seed, const PokemonCollection& collection,
                           uint8_t season) {
  if (!isAreaUnlocked(state, area)) return false;
  bool hasReadyMember = false;
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) {
    const OwnedPokemon* pokemon = CollectionLogic::active(collection, slot);
    if (pokemon && pokemon->currentHp && !pokemon->recoverySecondsRemaining)
      hasReadyMember = true;
  }
  if (!hasReadyMember) return false;
  const uint8_t index = normalizedArea(area);
  state.run = UnovaRunState{};
  state.run.seed = seed ? seed : mixed(advance(state.rngState));
  state.run.area = index;
  state.run.gateDepth = static_cast<uint8_t>(4U + mixed(state.run.seed) % 2U);
  state.run.gateRoute = static_cast<uint8_t>(mixed(state.run.seed ^ 0x47415445UL) % 3U);
  state.run.flags = kRunActive;
  state.run.season = std::min<uint8_t>(season, 3U);
  std::copy(std::begin(collection.party), std::end(collection.party),
            std::begin(state.run.partyUids));
  return true;
}

bool UnovaAdventure::abandon(UnovaAdventureState& state) {
  if (!active(state)) return false;
  state.run = UnovaRunState{};
  return true;
}
bool UnovaAdventure::active(const UnovaAdventureState& state) {
  return (state.run.flags & kRunActive) != 0U;
}

uint8_t UnovaAdventure::choices(const UnovaAdventureState& state,
                                UnovaRoomChoice output[3]) {
  if (!output || !active(state) || state.run.pendingBattle != UnovaBattleRole::None)
    return 0U;
  if (state.run.flags & kBossAvailable) {
    output[0] = {UnovaRoomKind::Boss, 0U, true};
    return 1U;
  }
  for (uint8_t route = 0; route < 3U; ++route) {
    if (state.run.depth == state.run.gateDepth) {
      const bool consumed = (state.run.visitedRooms & gateRouteVisitedBit(route)) != 0U;
      output[route] = {
          route == state.run.gateRoute ? UnovaRoomKind::GateTrainer :
              consumed ? UnovaRoomKind::ClueNpc :
              ((route + state.run.depth) & 1U ? UnovaRoomKind::ClueNpc :
                                                UnovaRoomKind::Trainer),
          route,
          (state.run.clueMask & (1U << route)) != 0U,
      };
      continue;
    }
    const uint32_t roll = mixed(state.run.seed ^
        (static_cast<uint32_t>(state.run.depth + 1U) * 0x9E3779B9UL) ^
        (static_cast<uint32_t>(route + 1U) * 0x85EBCA6BUL));
    static constexpr UnovaRoomKind kPhenomena[] = {
      UnovaRoomKind::ShakingGrass, UnovaRoomKind::DustCloud,
      UnovaRoomKind::GroundShadow, UnovaRoomKind::BubblingWater,
      UnovaRoomKind::HiddenGrotto,
    };
    UnovaRoomKind kind;
    if ((roll % 11U) == 0U && !(state.run.flags & kHealUsed))
      kind = UnovaRoomKind::Healing;
    else if ((roll % 5U) == 0U)
      kind = UnovaRoomKind::ClueNpc;
    else if ((roll & 3U) == 0U)
      kind = UnovaRoomKind::Trainer;
    else
      kind = kPhenomena[(roll >> 4U) % 5U];
    output[route] = {kind, route, false};
  }
  return 3U;
}

UnovaRoomResult UnovaAdventure::choose(UnovaAdventureState& state,
                                       uint8_t choice,
                                       PokemonCollection& collection) {
  UnovaRoomResult result;
  UnovaRoomChoice options[3]{};
  const uint8_t count = choices(state, options);
  if (choice >= count) return result;
  const UnovaRoomChoice selected = options[choice];
  result.kind = selected.kind;
  state.run.visitedRooms |= static_cast<uint16_t>(
      1U << std::min<uint8_t>(15U, state.run.depth));
  if (state.run.depth == state.run.gateDepth &&
      selected.kind != UnovaRoomKind::GateTrainer)
    state.run.visitedRooms |= gateRouteVisitedBit(selected.route);
  const uint32_t roll = mixed(state.run.seed ^
      (static_cast<uint32_t>(state.run.depth + 1U) << 16U) ^ choice);
  switch (selected.kind) {
    case UnovaRoomKind::Healing:
      result.healed = useAreaHeal(state, collection);
      break;
    case UnovaRoomKind::ClueNpc: {
      const uint8_t distance = state.run.gateDepth > state.run.depth
          ? static_cast<uint8_t>(state.run.gateDepth - state.run.depth) : 0U;
      const uint8_t hinted = distance <= 2U ? state.run.gateRoute :
          static_cast<uint8_t>(roll % 3U);
      state.run.clueMask |= static_cast<uint8_t>(1U << hinted);
      result.grantedClue = true;
      break;
    }
    case UnovaRoomKind::GateTrainer:
      state.run.pendingBattle = UnovaBattleRole::GateTrainer;
      result.startsBattle = true;
      break;
    case UnovaRoomKind::Boss:
      state.run.pendingBattle = UnovaBattleRole::Boss;
      result.startsBattle = true;
      break;
    case UnovaRoomKind::Trainer:
      state.run.pendingBattle = UnovaBattleRole::Trainer;
      result.startsBattle = true;
      break;
    default: {
      const uint8_t area = std::min<uint8_t>(state.run.area, kAreaCount - 1U);
      if(selected.kind==UnovaRoomKind::HiddenGrotto&&((roll>>3U)&1U)){
        result.foundItem=true;
        result.rewardKind=static_cast<uint8_t>(1U+((roll>>5U)%3U));
        break;
      }
      uint16_t species = 0U;
      bool legendary = false;
      if (selected.kind == UnovaRoomKind::HiddenGrotto) {
        legendary = roll % kSpecialLegendaryChanceDenominator == 0U;
        species = hiddenGrottoSpecies(
            collection,
            hiddenGrottoGeneration(static_cast<UnovaArea>(area)),
            legendary,
            mixed(roll ^ 0x48414752UL));
        // Generated data currently supplies legal HA legendaries to all five
        // generations. If a future data revision removes one pool, preserve
        // the encounter as an ordinary guaranteed-HA Grotto result.
        if (!species && legendary) {
          legendary = false;
          species = hiddenGrottoSpecies(
              collection,
              hiddenGrottoGeneration(static_cast<UnovaArea>(area)), false,
              mixed(roll ^ 0x48414752UL));
        }
      } else {
        const bool specialSite =
            area == static_cast<uint8_t>(UnovaArea::GiantChasm) ||
            area == static_cast<uint8_t>(UnovaArea::WhiteTreehollow);
        const uint16_t denominator = specialSite ?
            kSpecialLegendaryChanceDenominator : kLegendaryChanceDenominator;
        legendary = roll % denominator == 0U;
        if (legendary) {
          const uint8_t offset = static_cast<uint8_t>((roll >> 9U) % 3U);
          for (uint8_t attempt = 0; attempt < 3U && !species; ++attempt)
            species = kLegendaryPools[area][(offset + attempt) % 3U];
        }
        if (!species) species = kAreaRosters[area][(roll >> 8U) % 12U];
      }
      // Every generation has many legal candidates. Keep a defensive fallback
      // that still carries a real Hidden Ability if generated data changes.
      if (!species && selected.kind == UnovaRoomKind::HiddenGrotto)
        species = hiddenGrottoSpecies(collection, 1U, false, roll);
      if (!species) break;
      state.run.pendingSpecies = species;
      state.run.pendingPhenomenon = static_cast<uint8_t>(selected.kind);
      state.run.pendingBattle = UnovaBattleRole::Wild;
      result.startsBattle = true;
      result.speciesId = species;
      result.legendaryEncounter = legendary;
      // A Hidden Grotto has exactly the approved two outcomes: an item or a
      // Pokemon with its Hidden Ability. It must never degrade into an
      // ordinary wild encounter after choosing the Pokemon branch.
      result.hiddenAbilityEncounter = selected.kind == UnovaRoomKind::HiddenGrotto;
      if (result.hiddenAbilityEncounter) state.run.pendingPhenomenon |= 0x80U;
      break;
    }
  }
  // Choosing a wrong branch on the Gate floor must not skip the only Gate
  // Trainer and strand the run forever. It consumes the room but remains on
  // the branching floor until the hinted/correct route is chosen.
  if (!result.startsBattle && state.run.depth < kRoomsPerArea &&
      state.run.depth != state.run.gateDepth)
    ++state.run.depth;
  return result;
}

bool UnovaAdventure::startPendingBattle(UnovaAdventureState& state,
                                        BattleState& battle,
                                        PokemonCollection& collection,
                                        uint32_t playerUid) {
  if (!active(state) || state.run.pendingBattle == UnovaBattleRole::None ||
      battle.active || !healthyPlayer(state.run, collection, playerUid)) return false;
  const uint8_t area = std::min<uint8_t>(state.run.area, kAreaCount - 1U);
  const uint8_t level = levelForArea(collection, area);
  const uint32_t seed = mixed(state.run.seed ^ state.run.depth ^
      (static_cast<uint32_t>(state.run.pendingBattle) << 24U));
  if (state.run.pendingBattle == UnovaBattleRole::Wild) {
    initializeBattle(battle, playerUid, seed, BattleKind::Wild);
    OwnedPokemon opponent = CollectionLogic::createPokemon(
        0U, state.run.pendingSpecies, level,
        CollectionLogic::isShinyRoll(seed >> 3U), seed, 5U);
    if (!opponent.speciesId) {
      BattleEngine::clear(battle);
      return false;
    }
    if (state.run.pendingPhenomenon & 0x80U)
      CollectionLogic::setHiddenAbility(opponent, true, 5U);
    CollectionLogic::applyUnovaSeason(opponent, state.run.season);
    battle.opponents[0] = opponent;
    battle.opponentCount = 1U;
    battle.opponentIndex = 0U;
    BattleEngine::ensureOpponentUids(battle);
  } else {
    initializeBattle(battle, playerUid, seed, BattleKind::Trainer);
    const bool boss = state.run.pendingBattle == UnovaBattleRole::Boss;
    const bool gate = state.run.pendingBattle == UnovaBattleRole::GateTrainer;
    battle.opponentCount = boss ? 3U : gate ? 2U : 1U;
    battle.rewardMoney = static_cast<uint32_t>(level) * (boss ? 120U : gate ? 80U : 50U);
    for (uint8_t slot = 0; slot < battle.opponentCount; ++slot) {
      const uint16_t species = boss ? kBossSpecies[area][slot] :
          kAreaRosters[area][(seed + slot * 5U) % 12U];
      battle.opponents[slot] = CollectionLogic::createPokemon(
          0U, species, static_cast<uint8_t>(std::min<uint16_t>(100U,
              level + (boss ? 2U : gate ? 1U : 0U))), false,
          seed ^ (slot * 0x9E3779B9UL), 5U);
      CollectionLogic::applyUnovaSeason(battle.opponents[slot],
                                        state.run.season);
    }
    BattleEngine::orderOpponentTeamWeakestFirst(battle);
    BattleEngine::ensureOpponentUids(battle);
  }
  BattleEngine::applyEntryAbilities(battle, collection);
  return true;
}

bool UnovaAdventure::recordBattle(UnovaAdventureState& state,
                                  BattleOutcome outcome) {
  if (!active(state) || state.run.pendingBattle == UnovaBattleRole::None ||
      outcome == BattleOutcome::Ongoing || outcome == BattleOutcome::None)
    return false;
  const UnovaBattleRole role = state.run.pendingBattle;
  state.run.pendingBattle = UnovaBattleRole::None;
  state.run.pendingSpecies = 0U;
  state.run.pendingPhenomenon = 0U;
  if (outcome != BattleOutcome::Victory && outcome != BattleOutcome::Captured) {
    state.run = UnovaRunState{};
    return true;
  }
  if (role == UnovaBattleRole::GateTrainer) {
    state.run.flags |= static_cast<uint8_t>(kGateDefeated | kBossAvailable);
  } else if (role == UnovaBattleRole::Boss) {
    const uint8_t area = std::min<uint8_t>(state.run.area, kAreaCount - 1U);
    state.defeatedBosses |= static_cast<uint16_t>(1U << area);
    if (area + 1U < kAreaCount)
      state.unlockedAreas |= static_cast<uint16_t>(1U << (area + 1U));
    state.run = UnovaRunState{};
    return true;
  } else if (state.run.depth < kRoomsPerArea &&
             state.run.depth != state.run.gateDepth) {
    ++state.run.depth;
  }
  return true;
}

bool UnovaAdventure::useAreaHeal(UnovaAdventureState& state,
                                 PokemonCollection& collection) {
  if (!active(state) || (state.run.flags & kHealUsed)) return false;
  for (uint32_t uid : state.run.partyUids) {
    OwnedPokemon* pokemon = CollectionLogic::find(collection, uid);
    if (!pokemon) continue;
    // This room is intentionally not a revival service.  A fainted Pokemon
    // can already have partial HP from passive recovery while its incapacity
    // latch is still set, so checking currentHp alone would let the room fill
    // it and the next recovery tick would silently clear the latch.
    const bool incapacitated = pokemon->currentHp == 0U ||
        pokemon->recoverySecondsRemaining != 0U;
    if (!incapacitated) pokemon->currentHp = pokemon->maximumHp;
    pokemon->status = StatusCondition::None;
    for (uint8_t move = 0; move < kMoveSlots; ++move)
      pokemon->movePp[move] = CollectionLogic::maximumMovePp(*pokemon, move);
  }
  state.run.flags |= kHealUsed;
  return true;
}

bool UnovaAdventure::pwtTierUnlocked(const UnovaAdventureState& state,
                                     uint8_t tier) {
  if (tier >= 5U) return false;
  const uint8_t requiredBosses = static_cast<uint8_t>((tier + 1U) * 2U);
  uint8_t defeated = 0U;
  for (uint8_t area = 0; area < kAreaCount; ++area)
    if (state.defeatedBosses & (1U << area)) ++defeated;
  return defeated >= requiredBosses;
}
bool UnovaAdventure::recordPwtVictory(UnovaAdventureState& state,
                                      uint8_t tier) {
  if (!pwtTierUnlocked(state, tier)) return false;
  const uint8_t before = state.pwtCompletedTiers;
  state.pwtCompletedTiers |= static_cast<uint8_t>(1U << tier);
  return before != state.pwtCompletedTiers;
}
bool UnovaAdventure::recordChampionsVictory(UnovaAdventureState& state) {
  if (!allAreasCompleted(state) || state.pwtCompletedTiers != 0x1FU)
    return false;
  const uint8_t before = state.flags;
  state.flags |= static_cast<uint8_t>(kChampionsComplete | kWhite2Theme);
  return before != state.flags;
}
uint8_t UnovaAdventure::royalTrainerCount(uint32_t localDay) {
  // Unix day 0 is Thursday. The exact weekday label is presentation-only;
  // this seven-entry schedule guarantees that the same weekday always has
  // the same 3..7-Trainer voyage, including across week boundaries.
  static constexpr uint8_t kWeeklySchedule[7] = {3U, 4U, 5U, 6U, 7U, 5U, 4U};
  return kWeeklySchedule[localDay % 7U];
}
bool UnovaAdventure::royalAvailableToday(const UnovaAdventureState& state,
                                         uint32_t localDay) {
  return royalUnovaUnlocked(state) && state.royalUnovaLastDay != localDay;
}
bool UnovaAdventure::recordRoyalCompletion(UnovaAdventureState& state,
                                           uint32_t localDay) {
  if (!royalAvailableToday(state, localDay)) return false;
  state.royalUnovaLastDay = localDay;
  return true;
}
