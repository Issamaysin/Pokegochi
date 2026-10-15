#include "game/BossSystem.h"

#include <algorithm>
#include <cstring>

#include "game/Economy.h"
#include "game/MegaChallengeSystem.h"
#include "game/MegaEvolution.h"

namespace {

constexpr uint16_t kLegendaryAndMythicalSpecies[] = {
    144, 145, 146, 150, 151,
    243, 244, 245, 249, 250, 251,
    377, 378, 379, 380, 381, 382, 383, 384, 385, 386,
};

uint32_t nextRandom(uint32_t& state) {
  if (!state) state = 0x424F5353U;
  state ^= state << 13U;
  state ^= state >> 17U;
  state ^= state << 5U;
  return state;
}

bool isPhysicalType(PokemonType type) {
  return type == PokemonType::Normal || type == PokemonType::Fighting ||
      type == PokemonType::Flying || type == PokemonType::Poison ||
      type == PokemonType::Ground || type == PokemonType::Rock ||
      type == PokemonType::Bug || type == PokemonType::Ghost ||
      type == PokemonType::Steel;
}

bool effectContains(const FullMoveData* move, const char* text) {
  return move && move->effect && text && std::strstr(move->effect, text);
}

uint16_t moveScore(const FullMoveData& move, PokemonType type1, PokemonType type2,
                   bool physicalBoss) {
  uint16_t score = static_cast<uint16_t>(move.power) * 4U + move.accuracy;
  if (move.type == type1 || move.type == type2) score += 240U;
  if (isPhysicalType(move.type) == physicalBoss) score += 80U;
  return score;
}

void chooseMoves(OwnedPokemon& boss, PokemonType type1, PokemonType type2,
                 bool physicalBoss) {
  MoveId candidates[64]{};
  const uint8_t count = relearnableMovesForLine(
      boss.speciesId, boss.level, candidates,
      static_cast<uint8_t>(sizeof(candidates) / sizeof(candidates[0])));
  MoveId selected[kMoveSlots]{};
  uint16_t selectedScores[kMoveSlots]{};

  auto consider = [&](MoveId move) {
    if (!BossSystem::moveAllowed(move)) return;
    for (MoveId existing : selected) if (existing == move) return;
    const FullMoveData* data = findFullMove(move);
    if (!data) return;
    const uint16_t score = moveScore(*data, type1, type2, physicalBoss);
    for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
      if (score <= selectedScores[slot]) continue;
      for (uint8_t shift = kMoveSlots - 1U; shift > slot; --shift) {
        selected[shift] = selected[shift - 1U];
        selectedScores[shift] = selectedScores[shift - 1U];
      }
      selected[slot] = move;
      selectedScores[slot] = score;
      return;
    }
  };

  for (uint8_t index = 0; index < count; ++index) consider(candidates[index]);
  // Every selected species normally has at least four damaging level-up
  // moves. These neutral, non-healing fallbacks keep a future data-table
  // change from creating an unusable Boss.
  const MoveId fallbacks[] = {
      static_cast<MoveId>(129),  // SWIFT
      static_cast<MoveId>(163),  // SLASH
      static_cast<MoveId>(34),   // BODY SLAM
      static_cast<MoveId>(63),   // HYPER BEAM
  };
  for (MoveId fallback : fallbacks) {
    uint8_t empty = 0;
    while (empty < kMoveSlots && selected[empty] != MoveId::None) ++empty;
    if (empty >= kMoveSlots) break;
    bool duplicate = false;
    for (MoveId existing : selected) if (existing == fallback) duplicate = true;
    if (!duplicate && BossSystem::moveAllowed(fallback)) selected[empty] = fallback;
  }

  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    boss.moves[slot] = selected[slot];
    const FullMoveData* move = findFullMove(selected[slot]);
    boss.movePp[slot] = move ? move->pp : 0U;
  }
}

const MegaFormData* megaCandidate(uint16_t wanted) {
  uint16_t found = 0;
  for (uint8_t index = 0; index < MegaEvolution::officialFormCount(); ++index) {
    const MegaFormData* form = MegaEvolution::officialForm(index);
    // Sinnoh's Megas belong to the expedition reward loop; the established
    // Boss pool stays exactly on the original Gen-I--III roster.
    if (!form || form->speciesId > 386U || form->variant == MegaVariant::Primal) continue;
    if (found++ == wanted) return form;
  }
  return nullptr;
}

uint16_t megaCandidateCount() {
  uint16_t count = 0;
  for (uint8_t index = 0; index < MegaEvolution::officialFormCount(); ++index) {
    const MegaFormData* form = MegaEvolution::officialForm(index);
    if (form && form->speciesId <= 386U && form->variant != MegaVariant::Primal) ++count;
  }
  return count;
}

BossEncounterState chooseEncounter(uint32_t& randomState) {
  constexpr uint16_t legendaryCount = static_cast<uint16_t>(
      sizeof(kLegendaryAndMythicalSpecies) / sizeof(kLegendaryAndMythicalSpecies[0]));
  const uint16_t megaCount = megaCandidateCount();
  const uint16_t roll = static_cast<uint16_t>(
      nextRandom(randomState) % static_cast<uint32_t>(legendaryCount + megaCount));
  const MegaFormData* mega = roll >= legendaryCount
      ? megaCandidate(static_cast<uint16_t>(roll - legendaryCount)) : nullptr;
  BossEncounterState encounter;
  encounter.speciesId = mega ? mega->speciesId : kLegendaryAndMythicalSpecies[roll];
  encounter.personality = nextRandom(randomState);
  encounter.currentHp = BossSystem::kMaximumHp;
  encounter.megaVariant = mega ? mega->variant : MegaVariant::None;
  const OwnedPokemon identity = CollectionLogic::createPokemon(
      0, encounter.speciesId, 100, false, encounter.personality);
  encounter.form = identity.form;
  return encounter;
}

OwnedPokemon createBoss(const BossEncounterState& encounter) {
  OwnedPokemon boss = CollectionLogic::createPokemon(
      0, encounter.speciesId, 100, false, encounter.personality);
  boss.form = encounter.form;
  const MegaFormData* mega = MegaEvolution::formData(
      encounter.speciesId, encounter.megaVariant);

  if (mega) {
    boss.heldItem = HeldItem::MegaStone;
    MegaEvolution::setVariantChoice(boss, mega->variant);
  } else {
    boss.heldItem = HeldItem::None;
  }
  boss.ivs = {31, 31, 31, 31, 31, 31};

  const SpeciesData* species = findSpecies(encounter.speciesId);
  const uint8_t attack = mega ? mega->baseAttack : species ? species->baseAttack : 100U;
  const uint8_t spAttack = mega ? mega->baseSpAttack : species ? species->baseSpAttack : 100U;
  const bool physicalBoss = attack >= spAttack;
  // HP is replaced by the explicit 200,000-point Boss pool, so the remaining
  // four EVs belong in Sp. Defense instead of being wasted in the overridden
  // HP calculation.
  boss.evs = physicalBoss ? EffortValues{0, 252, 0, 0, 4, 252}
                          : EffortValues{0, 0, 0, 252, 4, 252};
  boss.nature = physicalBoss ? PokemonNature::Adamant : PokemonNature::Modest;
  CollectionLogic::refreshAbility(boss);
  CollectionLogic::refreshDerivedStats(boss, false);

  const PokemonType type1 = mega ? mega->type1 : species ? species->type1 : PokemonType::Normal;
  const PokemonType type2 = mega ? mega->type2 : species ? species->type2 : type1;
  chooseMoves(boss, type1, type2, physicalBoss);
  boss.maximumHp = BossSystem::kBattleHpLayer;
  boss.currentHp = static_cast<uint16_t>(std::min<uint32_t>(
      BossSystem::kBattleHpLayer,std::max<uint32_t>(1U,encounter.currentHp)));
  boss.status = StatusCondition::None;
  return boss;
}

}  // namespace

bool BossSystem::available(const GymProgress& progress) {
  return MegaChallengeSystem::prerequisitesMet(progress) &&
         MegaChallengeSystem::completed(progress);
}

bool BossSystem::ready(const GymProgress& progress,
                       uint32_t cooldownSecondsRemaining) {
  return available(progress) && cooldownSecondsRemaining == 0U;
}

void BossSystem::beginCooldown(uint32_t& cooldownSecondsRemaining) {
  cooldownSecondsRemaining = kCooldownSeconds;
}

void BossSystem::advanceCooldown(uint32_t& cooldownSecondsRemaining,
                                 uint32_t elapsedSeconds) {
  cooldownSecondsRemaining = elapsedSeconds >= cooldownSecondsRemaining
      ? 0U : cooldownSecondsRemaining - elapsedSeconds;
}

bool BossSystem::hasEncounter(const BossEncounterState& encounter) {
  return encounter.speciesId != 0U && encounter.currentHp != 0U &&
      encounter.currentHp <= kMaximumHp &&
      findSpecies(encounter.speciesId) != nullptr &&
      (encounter.megaVariant == MegaVariant::None ||
       MegaEvolution::formData(encounter.speciesId,
                               encounter.megaVariant) != nullptr);
}

void BossSystem::ensureEncounter(BossEncounterState& encounter, uint32_t seed) {
  if (hasEncounter(encounter)) return;
  uint32_t randomState = seed ? seed : 0x424F5353U;
  encounter = chooseEncounter(randomState);
}

OwnedPokemon BossSystem::encounterPokemon(const BossEncounterState& encounter) {
  return hasEncounter(encounter) ? createBoss(encounter) : OwnedPokemon{};
}

uint32_t BossSystem::battleCurrentHp(const BattleState& battle) {
  if(battle.kind!=BattleKind::Boss)return 0U;
  return std::min<uint32_t>(kMaximumHp,battle.payDayMoney);
}

void BossSystem::recordAttempt(BossEncounterState& encounter,
                               const BattleState& battle, bool defeated) {
  if (battle.kind != BattleKind::Boss || !battle.opponentCount) return;
  const OwnedPokemon& boss = battle.opponents[0];
  const uint32_t remaining=battleCurrentHp(battle);
  if (defeated || remaining == 0U) {
    encounter = BossEncounterState{};
    return;
  }
  if (hasEncounter(encounter)) {
    encounter.currentHp = remaining;
    return;
  }
  // Identity normally already exists. Copying it from the battle also repairs
  // a migrated save that was updated while an older Boss battle was active.
  encounter.personality = boss.personality;
  encounter.speciesId = boss.speciesId;
  encounter.currentHp = remaining;
  encounter.form = boss.form;
  encounter.megaVariant = MegaEvolution::variantFor(boss);
  encounter.reserved = 0U;
}

BossRewardResult BossSystem::awardVictoryReward(
    Inventory& inventory, EggInventory& eggs, const PokedexState& pokedex,
    uint32_t seed) {
  BossRewardResult reward;
  uint32_t roll = seed ? seed : 0xB055A11U;
  roll ^= roll >> 16U;
  roll *= 0x7FEB352DUL;
  roll ^= roll >> 15U;
  roll *= 0x846CA68BUL;
  roll ^= roll >> 16U;
  static_assert(kMasterBallRewardBuckets + kSpecialEggRewardBuckets ==
                    kRewardBucketCount,
                "Boss reward buckets must cover the complete probability table");
  const bool masterBallFirst =
      roll % kRewardBucketCount < kMasterBallRewardBuckets;

  auto grantMasterBall = [&]() {
    uint16_t& count =
        inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)];
    if (count >= kInventoryStackLimit) return false;
    ++count;
    reward.kind = BossRewardKind::MasterBall;
    return true;
  };
  auto grantSpecialEgg = [&]() {
    if (!EggSystem::storeBattleTowerSpecial(
            eggs,pokedex,roll^0x424F5353UL,&reward.eggSpeciesId))return false;
    reward.kind = BossRewardKind::SpecialEgg;
    return true;
  };

  if (masterBallFirst) {
    if (grantMasterBall() || grantSpecialEgg()) return reward;
  } else {
    if (grantSpecialEgg() || grantMasterBall()) return reward;
  }
  reward.kind = BossRewardKind::StorageFull;
  return reward;
}

bool BossSystem::isLegendaryOrMythical(uint16_t speciesId) {
  for (uint16_t candidate : kLegendaryAndMythicalSpecies)
    if (candidate == speciesId) return true;
  return false;
}

bool BossSystem::moveAllowed(MoveId move) {
  const FullMoveData* data = findFullMove(move);
  if (!data || !data->power || !isMoveAvailableInSingleBattle(move)) return false;
  // Bosses never receive recovery, drain, target-healing, instant-KO or
  // percentage-HP attacks. This also keeps their AI fair if the move tables
  // grow later without a hand-authored Boss set being updated.
  return !effectContains(data, "RESTORE_HP") &&
      !effectContains(data, "HEAL_HALF") &&
      !effectContains(data, "SOFTBOILED") &&
      !effectContains(data, "SYNTHESIS") &&
      !effectContains(data, "MORNING_SUN") &&
      !effectContains(data, "MOONLIGHT") &&
      !effectContains(data, "ABSORB") &&
      !effectContains(data, "DREAM_EATER") &&
      !effectContains(data, "PRESENT") &&
      !effectContains(data, "OHKO") &&
      !effectContains(data, "SUPER_FANG") &&
      !effectContains(data, "ENDEAVOR") &&
      !effectContains(data, "EXPLOSION");
}

bool BossSystem::start(BattleState& battle, PokemonCollection& collection,
                       uint32_t playerUid, const GymProgress& progress,
                       uint32_t cooldownSecondsRemaining,
                       BossEncounterState& encounter, uint32_t seed) {
  OwnedPokemon* player = CollectionLogic::find(collection, playerUid);
  if (battle.active || !ready(progress, cooldownSecondsRemaining) || !player ||
      !CollectionLogic::isInParty(collection, playerUid) || !player->currentHp ||
      player->recoverySecondsRemaining) return false;
  ensureEncounter(encounter, seed);
  if (!hasEncounter(encounter)) return false;

  BattleEngine::clear(battle);
  battle.active = true;
  battle.kind = BattleKind::Boss;
  battle.outcome = BattleOutcome::Ongoing;
  battle.playerUid = playerUid;
  battle.unlockedGeneration = progress.unlockedGeneration;
  battle.rngState = (seed ? seed : 0x424F5353U) ^ encounter.personality ^
      (static_cast<uint32_t>(encounter.speciesId) << 16U);
  if (!battle.rngState) battle.rngState = 0x424F5353U;
  battle.opponentCount = 1U;
  battle.opponentIndex = 0U;
  battle.opponentItemUses = 0U;
  battle.rewardMoney = 0U;
  // Boss raid HP uses this otherwise monetary battle-local counter. The
  // engine explicitly excludes it from Pay Day and prize-money calculations
  // in Boss battles, preserving the frozen BattleState/save ABI.
  battle.payDayMoney=encounter.currentHp;
  battle.opponents[0] = createBoss(encounter);
  BattleEngine::ensureOpponentUids(battle);
  BattleEngine::applyEntryAbilities(battle, collection);
  return true;
}
