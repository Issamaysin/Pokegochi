#include "game/MegaChallengeSystem.h"

#include "game/HeldItems.h"
#include "game/MegaEvolution.h"

namespace {
constexpr MegaChallengeTrainerDefinition kTrainer = {
    "MEGA MASTER", "RED", "red_front_pic",
    "SO YOU CLEARED EVERY LEAGUE.",
    "SHOW ME YOUR TRUE POTENTIAL."
};

void setMoves(OwnedPokemon& pokemon, MoveId first, MoveId second = MoveId::None,
              MoveId third = MoveId::None, MoveId fourth = MoveId::None) {
  const MoveId moves[kMoveSlots] = {first, second, third, fourth};
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    pokemon.moves[slot] = moves[slot];
    const FullMoveData* move = findFullMove(moves[slot]);
    pokemon.movePp[slot] = move ? move->pp : 0U;
  }
}

void finishAuthoredPokemon(OwnedPokemon& pokemon, PokemonNature nature,
                           HeldItem item, const EffortValues& evs) {
  pokemon.ivs = {31, 31, 31, 31, 31, 31};
  pokemon.evs = evs;
  pokemon.nature = nature;
  pokemon.heldItem = item;
  CollectionLogic::refreshAbility(pokemon);
  CollectionLogic::refreshDerivedStats(pokemon, false);
}

OwnedPokemon authoredDitto() {
  OwnedPokemon pokemon = CollectionLogic::createPokemon(0, 132, 100, true, 0xD1770D17U);
  setMoves(pokemon, static_cast<MoveId>(144));  // TRANSFORM
  finishAuthoredPokemon(pokemon, PokemonNature::Relaxed, HeldItem::MetalPowder,
                        {252, 0, 252, 0, 4, 0});
  return pokemon;
}

OwnedPokemon authoredShedinja() {
  OwnedPokemon pokemon = CollectionLogic::createPokemon(0, 292, 100, false, 0x5EED1A99U);
  setMoves(pokemon, static_cast<MoveId>(14),   // SWORDS DANCE
                    static_cast<MoveId>(247),  // SHADOW BALL
                    static_cast<MoveId>(318),  // SILVER WIND
                    static_cast<MoveId>(182)); // PROTECT
  finishAuthoredPokemon(pokemon, PokemonNature::Lonely, HeldItem::LumBerry,
                        {0, 252, 0, 0, 4, 252});
  return pokemon;
}

OwnedPokemon authoredMegaCharizardX() {
  OwnedPokemon pokemon = CollectionLogic::createPokemon(0, 6, 100, false, 0x4D454741U);
  setMoves(pokemon, static_cast<MoveId>(349),  // DRAGON DANCE
                    static_cast<MoveId>(337),  // DRAGON CLAW
                    static_cast<MoveId>(89),   // EARTHQUAKE
                    static_cast<MoveId>(126)); // FIRE BLAST
  finishAuthoredPokemon(pokemon, PokemonNature::Jolly, HeldItem::MegaStone,
                        {0, 252, 0, 0, 4, 252});
  MegaEvolution::setVariantChoice(pokemon, MegaVariant::MegaX);
  CollectionLogic::refreshAbility(pokemon);
  CollectionLogic::refreshDerivedStats(pokemon, false);
  return pokemon;
}
}  // namespace

bool MegaChallengeSystem::prerequisitesMet(const GymProgress& progress) {
  return progress.unlockedGeneration >= 3U &&
         GymSystem::badgeCount(progress) == static_cast<uint8_t>(GymId::Count) &&
         GymSystem::leagueComplete(progress, 1U) &&
         GymSystem::leagueComplete(progress, 2U) &&
         GymSystem::leagueComplete(progress, 3U);
}

bool MegaChallengeSystem::completed(const GymProgress& progress) {
  return (progress.starterClaimedBits & kCompletionBit) != 0U;
}

bool MegaChallengeSystem::available(const GymProgress& progress) {
  return prerequisitesMet(progress) && !completed(progress);
}

bool MegaChallengeSystem::start(BattleState& battle, PokemonCollection& collection,
                                uint32_t playerUid, const GymProgress& progress,
                                uint32_t seed) {
  OwnedPokemon* player = CollectionLogic::find(collection, playerUid);
  if (battle.active || !available(progress) || !player ||
      !CollectionLogic::isInParty(collection, playerUid) || !player->currentHp ||
      player->recoverySecondsRemaining) return false;

  BattleEngine::clear(battle);
  battle.active = true;
  battle.kind = BattleKind::MegaChallenge;
  battle.outcome = BattleOutcome::Ongoing;
  battle.playerUid = playerUid;
  battle.unlockedGeneration = progress.unlockedGeneration;
  battle.rngState = seed ? seed : 0x4D454741U;
  battle.opponentCount = kOpponentTeamCapacity;
  battle.opponentIndex = 0;
  battle.opponentItemUses = 3;
  battle.rewardMoney = 25000U;
  battle.opponents[0] = authoredDitto();
  battle.opponents[1] = authoredShedinja();
  battle.opponents[2] = authoredMegaCharizardX();
  BattleEngine::orderOpponentTeamWeakestFirst(battle);
  BattleEngine::ensureOpponentUids(battle);
  BattleEngine::applyEntryAbilities(battle, collection);
  return true;
}

bool MegaChallengeSystem::awardVictory(GymProgress& progress,
                                       uint64_t& ownedMachines) {
  if (!available(progress)) return false;
  progress.starterClaimedBits |= kCompletionBit;
  ownedMachines |= kMegaStoneOwnershipBit;
  return true;
}

bool MegaChallengeSystem::reconcileStoneGate(const GymProgress& progress,
                                             PokemonCollection& collection,
                                             uint64_t& ownedMachines) {
  bool changed = false;
  bool held = false;
  for (OwnedPokemon& pokemon : collection.box) {
    if (!pokemon.uid || pokemon.heldItem != HeldItem::MegaStone) continue;
    if (completed(progress)) {
      held = true;
      continue;
    }
    pokemon.heldItem = HeldItem::None;
    CollectionLogic::refreshAbility(pokemon);
    CollectionLogic::refreshDerivedStats(pokemon, true);
    changed = true;
  }

  if (!completed(progress)) {
    if ((ownedMachines & kMegaStoneOwnershipBit) != 0U) changed = true;
    ownedMachines &= ~kMegaStoneOwnershipBit;
  } else if (!held && (ownedMachines & kMegaStoneOwnershipBit) == 0U) {
    ownedMachines |= kMegaStoneOwnershipBit;
    changed = true;
  }
  return changed;
}

const MegaChallengeTrainerDefinition& MegaChallengeSystem::trainer() {
  return kTrainer;
}
