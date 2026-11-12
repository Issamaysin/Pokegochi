#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "game/BattleEngine.h"
#include "game/Collection.h"
#include "game/PokemonData.h"

namespace {
constexpr MoveId kSplash = static_cast<MoveId>(150U);
constexpr uint16_t kMaximumRegularMove = 672U;
constexpr uint16_t kNihilLight = 920U;
constexpr uint16_t kSeeds = 128U;

enum StageBit : uint8_t {
  AttackBit = 1U << 0U,
  DefenseBit = 1U << 1U,
  SpAttackBit = 1U << 2U,
  SpDefenseBit = 1U << 3U,
  SpeedBit = 1U << 4U,
  AccuracyBit = 1U << 5U,
  EvasionBit = 1U << 6U,
  CriticalBit = 1U << 7U,
};

struct StageSnapshot {
  std::array<int16_t, 8U> values{};
};

StageSnapshot stages(const CombatVolatile& state) {
  return {{{state.attackStage, state.defenseStage, state.spAttackStage,
            state.spDefenseStage, state.speedStage, state.accuracyStage,
            state.evasionStage, state.criticalStage}}};
}

struct Observation {
  uint8_t actorGain = 0U;
  uint8_t actorLoss = 0U;
  uint8_t targetGain = 0U;
  uint8_t targetLoss = 0U;
  bool actorHpUp = false;
  bool actorHpDown = false;
  bool targetHpUp = false;
  bool targetHpDown = false;
  bool actorStatusChanged = false;
  bool targetStatusChanged = false;
  bool actorAbilityChanged = false;
  bool targetAbilityChanged = false;
  bool actorItemChanged = false;
  bool targetItemChanged = false;
  bool actorIdentityChanged = false;
  bool targetIdentityChanged = false;
  bool actorTypeChanged = false;
  bool targetTypeChanged = false;
  bool fieldChanged = false;
  bool moveUsed = false;
  bool moveMissed = false;
  bool failed = false;
  bool charged = false;
  bool rejected = false;
  bool eventCapacityReached = false;
};

void recordStageDelta(Observation& observation, const StageSnapshot& before,
                      const StageSnapshot& after, bool actor) {
  for (uint8_t index = 0U; index < before.values.size(); ++index) {
    const uint8_t bit = static_cast<uint8_t>(1U << index);
    if (after.values[index] > before.values[index]) {
      if (actor) observation.actorGain |= bit;
      else observation.targetGain |= bit;
    } else if (after.values[index] < before.values[index]) {
      if (actor) observation.actorLoss |= bit;
      else observation.targetLoss |= bit;
    }
  }
}

struct Fixture {
  PokemonCollection collection{};
  BattleState battle{};
  bool enemy = false;

  Fixture(MoveId move, bool enemySide, uint32_t seed, uint8_t scenario)
      : enemy(enemySide) {
    CollectionLogic::initialize(collection);
    const uint32_t playerUid = 1U;
    OwnedPokemon* playerPokemon = &collection.box[0];
    collection.party[0] = playerUid;
    *playerPokemon = CollectionLogic::createPokemon(
        playerUid, 1U, enemy ? 50U : 100U, false, 0xA1100002U);
    playerPokemon->moves[0] = enemy ? kSplash : move;
    playerPokemon->movePp[0] = findFullMove(playerPokemon->moves[0])->pp;
    playerPokemon->moves[1] = static_cast<MoveId>(7U);
    playerPokemon->movePp[1] = 15U;
    playerPokemon->heldItem = HeldItem::None;
    playerPokemon->abilityId = 0U;
    playerPokemon->maximumHp = 60000U;
    playerPokemon->currentHp = 30000U;

    BattleEngine::clear(battle);
    battle.active = true;
    battle.kind = BattleKind::Pvp;
    battle.outcome = BattleOutcome::Ongoing;
    battle.playerUid = playerUid;
    battle.opponentCount = 1U;
    battle.unlockedGeneration = 7U;
    battle.rngState = seed;
    battle.opponents[0] = CollectionLogic::createPokemon(
        0xF1A11001U, 1U, enemy ? 100U : 50U, false, 0xA1100003U);
    OwnedPokemon& foe = battle.opponents[0];
    // Keep role-swapped fixtures statistically identical. Personality and
    // UID remain distinct, but nature/IV/EV noise must not decide whether a
    // turn-order-dependent move is legal in only one direction.
    foe.nature = playerPokemon->nature;
    foe.ivs = playerPokemon->ivs;
    foe.evs = playerPokemon->evs;
    foe.moves[0] = enemy ? move : kSplash;
    foe.movePp[0] = findFullMove(foe.moves[0])->pp;
    foe.moves[1] = static_cast<MoveId>(7U);
    foe.movePp[1] = 15U;
    foe.heldItem = HeldItem::None;
    foe.abilityId = 0U;
    foe.maximumHp = 60000U;
    foe.currentHp = 30000U;

    CombatVolatile& actorState = actorVolatile();
    CombatVolatile& targetState = targetVolatile();
    actorState.accuracyStage = kMaximumBattleStatStage;
    targetState.evasionStage = kMinimumBattleStatStage;

    if (scenario == 1U) {
      target().status = StatusCondition::Sleep;
      targetState.sleepTurns = 3U;
    } else if (scenario == 2U) {
      target().status = StatusCondition::Poison;
      targetState.attackStage = 2;
      targetState.defenseStage = -2;
      targetState.spAttackStage = 1;
      targetState.spDefenseStage = -1;
      targetState.speedStage = 2;
      target().heldItem = HeldItem::OranBerry;
    }
  }

  OwnedPokemon& player() {
    OwnedPokemon* value = CollectionLogic::find(collection, battle.playerUid);
    assert(value);
    return *value;
  }
  OwnedPokemon& actor() { return enemy ? battle.opponents[0] : player(); }
  OwnedPokemon& target() { return enemy ? player() : battle.opponents[0]; }
  CombatVolatile& actorVolatile() {
    return enemy ? battle.opponentVolatiles[0] : battle.playerVolatile;
  }
  CombatVolatile& targetVolatile() {
    return enemy ? battle.playerVolatile : battle.opponentVolatiles[0];
  }
  DedicatedMoveEffectState& actorEffects() {
    return enemy ? battle.opponentMoveEffects[0] : battle.playerMoveEffects;
  }
  DedicatedMoveEffectState& targetEffects() {
    return enemy ? battle.playerMoveEffects : battle.opponentMoveEffects[0];
  }
  BattleActionResult use() {
    // Both roles must receive the same opposing command.  Using 0xFE only
    // for a player-owned move made target-action mechanics (Copycat, Bide,
    // Me First, etc.) look asymmetric even though the engine was being fed
    // two different turns.
    return BattleEngine::fightPvp(battle, collection, 0U, 0U);
  }
};

void inspectResult(Observation& observation, const BattleActionResult& result,
                   MoveId move, BattleSide actorSide) {
  if (!result.accepted) observation.rejected = true;
  if (result.eventCount == kBattleEventCapacity)
    observation.eventCapacityReached = true;
  for (uint8_t index = 0U; index < result.eventCount; ++index) {
    const BattleEvent& event = result.events[index];
    if (event.move != move || event.side != actorSide) continue;
    if (event.type == BattleEventType::MoveUsed)
      observation.moveUsed = true;
    else if (event.type == BattleEventType::MoveMissed)
      observation.moveMissed = true;
    else if (event.type == BattleEventType::MoveEffect &&
             event.value == static_cast<uint16_t>(BattleMoveEffect::Failed))
      observation.failed = true;
    else if (event.type == BattleEventType::MoveEffect &&
             event.value == static_cast<uint16_t>(BattleMoveEffect::Charged))
      observation.charged = true;
  }
}

void observeOne(MoveId move, bool enemy, uint32_t seed, uint8_t scenario,
                Observation& observation) {
  Fixture fixture(move, enemy, seed, scenario);
  OwnedPokemon& actor = fixture.actor();
  OwnedPokemon& target = fixture.target();
  const StageSnapshot actorStagesBefore = stages(fixture.actorVolatile());
  const StageSnapshot targetStagesBefore = stages(fixture.targetVolatile());
  const uint16_t actorHpBefore = actor.currentHp;
  const uint16_t targetHpBefore = target.currentHp;
  const StatusCondition actorStatusBefore = actor.status;
  const StatusCondition targetStatusBefore = target.status;
  const uint8_t actorAbilityBefore = actor.abilityId;
  const uint8_t targetAbilityBefore = target.abilityId;
  const HeldItem actorItemBefore = actor.heldItem;
  const HeldItem targetItemBefore = target.heldItem;
  const uint16_t actorSpeciesBefore = actor.speciesId;
  const uint16_t targetSpeciesBefore = target.speciesId;
  const uint8_t actorFormBefore = actor.form;
  const uint8_t targetFormBefore = target.form;
  const bool actorTypeBefore = fixture.actorEffects().typeOverrideActive;
  const bool targetTypeBefore = fixture.targetEffects().typeOverrideActive;
  const BattleWeather weatherBefore = fixture.battle.weather;
  const Gen6Terrain terrainBefore = fixture.battle.gen6Terrain;
  const uint8_t gravityBefore = fixture.battle.gravityTurns;
  const uint8_t trickRoomBefore = fixture.battle.trickRoomTurns;
  const uint8_t wonderRoomBefore = fixture.battle.wonderRoomTurns;
  const uint8_t magicRoomBefore = fixture.battle.magicRoomTurns;

  const BattleSide side = enemy ? BattleSide::Opponent : BattleSide::Player;
  BattleActionResult result = fixture.use();
  inspectResult(observation, result, move, side);
  if (fixture.battle.active && fixture.actor().currentHp != 0U &&
      fixture.actorVolatile().chargingMove == move) {
    result = fixture.use();
    inspectResult(observation, result, move, side);
  }

  const OwnedPokemon& actorAfter = fixture.actor();
  const OwnedPokemon& targetAfter = fixture.target();
  recordStageDelta(observation, actorStagesBefore,
                   stages(fixture.actorVolatile()), true);
  recordStageDelta(observation, targetStagesBefore,
                   stages(fixture.targetVolatile()), false);
  observation.actorHpUp |= actorAfter.currentHp > actorHpBefore;
  observation.actorHpDown |= actorAfter.currentHp < actorHpBefore;
  observation.targetHpUp |= targetAfter.currentHp > targetHpBefore;
  observation.targetHpDown |= targetAfter.currentHp < targetHpBefore;
  observation.actorStatusChanged |= actorAfter.status != actorStatusBefore;
  observation.targetStatusChanged |= targetAfter.status != targetStatusBefore;
  observation.actorAbilityChanged |= actorAfter.abilityId != actorAbilityBefore;
  observation.targetAbilityChanged |= targetAfter.abilityId != targetAbilityBefore;
  observation.actorItemChanged |= actorAfter.heldItem != actorItemBefore;
  observation.targetItemChanged |= targetAfter.heldItem != targetItemBefore;
  observation.actorIdentityChanged |= actorAfter.speciesId != actorSpeciesBefore ||
      actorAfter.form != actorFormBefore;
  observation.targetIdentityChanged |= targetAfter.speciesId != targetSpeciesBefore ||
      targetAfter.form != targetFormBefore;
  observation.actorTypeChanged |=
      fixture.actorEffects().typeOverrideActive != actorTypeBefore;
  observation.targetTypeChanged |=
      fixture.targetEffects().typeOverrideActive != targetTypeBefore;
  observation.fieldChanged |= fixture.battle.weather != weatherBefore ||
      fixture.battle.gen6Terrain != terrainBefore ||
      fixture.battle.gravityTurns != gravityBefore ||
      fixture.battle.trickRoomTurns != trickRoomBefore ||
      fixture.battle.wonderRoomTurns != wonderRoomBefore ||
      fixture.battle.magicRoomTurns != magicRoomBefore;
}

Observation observe(MoveId move, bool enemy) {
  Observation total{};
  for (uint8_t scenario = 0U; scenario < 3U; ++scenario)
    for (uint16_t seed = 1U; seed <= kSeeds; ++seed)
      observeOne(move, enemy,
                 0xA1100000U ^ (static_cast<uint32_t>(scenario) << 24U) ^ seed,
                 scenario, total);
  return total;
}

bool equivalent(const Observation& lhs, const Observation& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(Observation)) == 0;
}

void printAsymmetry(uint16_t id, const Observation& player,
                    const Observation& opponent) {
  const auto field = [&](const char* name, auto left, auto right) {
    if (left != right)
      std::cerr << ' ' << name << '=' << static_cast<unsigned>(left)
                << '/' << static_cast<unsigned>(right);
  };
  std::cerr << "ASYM " << id;
  field("actorGain", player.actorGain, opponent.actorGain);
  field("actorLoss", player.actorLoss, opponent.actorLoss);
  field("targetGain", player.targetGain, opponent.targetGain);
  field("targetLoss", player.targetLoss, opponent.targetLoss);
  field("actorHpUp", player.actorHpUp, opponent.actorHpUp);
  field("actorHpDown", player.actorHpDown, opponent.actorHpDown);
  field("targetHpUp", player.targetHpUp, opponent.targetHpUp);
  field("targetHpDown", player.targetHpDown, opponent.targetHpDown);
  field("actorStatus", player.actorStatusChanged, opponent.actorStatusChanged);
  field("targetStatus", player.targetStatusChanged, opponent.targetStatusChanged);
  field("actorAbility", player.actorAbilityChanged, opponent.actorAbilityChanged);
  field("targetAbility", player.targetAbilityChanged, opponent.targetAbilityChanged);
  field("actorItem", player.actorItemChanged, opponent.actorItemChanged);
  field("targetItem", player.targetItemChanged, opponent.targetItemChanged);
  field("actorIdentity", player.actorIdentityChanged, opponent.actorIdentityChanged);
  field("targetIdentity", player.targetIdentityChanged, opponent.targetIdentityChanged);
  field("actorType", player.actorTypeChanged, opponent.actorTypeChanged);
  field("targetType", player.targetTypeChanged, opponent.targetTypeChanged);
  field("field", player.fieldChanged, opponent.fieldChanged);
  field("used", player.moveUsed, opponent.moveUsed);
  field("missed", player.moveMissed, opponent.moveMissed);
  field("failed", player.failed, opponent.failed);
  field("charged", player.charged, opponent.charged);
  field("rejected", player.rejected, opponent.rejected);
  field("capacity", player.eventCapacityReached,
        opponent.eventCapacityReached);
  std::cerr << '\n';
}

void printObservation(uint16_t id, const FullMoveData& move,
                      const Observation& player,
                      const Observation& opponent) {
  const bool symmetric = equivalent(player, opponent);
  std::cout << id << ',' << move.name << ',' << move.effect << ','
            << static_cast<unsigned>(player.actorGain) << ','
            << static_cast<unsigned>(player.actorLoss) << ','
            << static_cast<unsigned>(player.targetGain) << ','
            << static_cast<unsigned>(player.targetLoss) << ','
            << player.actorHpUp << ',' << player.actorHpDown << ','
            << player.targetHpUp << ',' << player.targetHpDown << ','
            << player.actorStatusChanged << ',' << player.targetStatusChanged << ','
            << player.actorAbilityChanged << ',' << player.targetAbilityChanged << ','
            << player.actorItemChanged << ',' << player.targetItemChanged << ','
            << player.actorIdentityChanged << ',' << player.targetIdentityChanged << ','
            << player.actorTypeChanged << ',' << player.targetTypeChanged << ','
            << player.fieldChanged << ',' << player.moveUsed << ','
            << player.moveMissed << ',' << player.failed << ',' << player.charged << ','
            << player.rejected << ',' << player.eventCapacityReached << ','
            << symmetric << '\n';
}
}  // namespace

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::cout << "id,name,effect,actor_gain,actor_loss,target_gain,target_loss,"
               "actor_hp_up,actor_hp_down,target_hp_up,target_hp_down,"
               "actor_status,target_status,actor_ability,target_ability,"
               "actor_item,target_item,actor_identity,target_identity,"
               "actor_type,target_type,field,move_used,move_missed,failed,charged,"
               "rejected,event_capacity,symmetric\n";
  uint16_t audited = 0U;
  uint16_t asymmetric = 0U;
  for (uint16_t id = 1U; id <= kMaximumRegularMove; ++id) {
    const FullMoveData* move = findFullMove(static_cast<MoveId>(id));
    if (!move) continue;
    const Observation player = observe(static_cast<MoveId>(id), false);
    const Observation opponent = observe(static_cast<MoveId>(id), true);
    printObservation(id, *move, player, opponent);
    ++audited;
    if (!equivalent(player, opponent)) {
      ++asymmetric;
      printAsymmetry(id, player, opponent);
    }
  }
  const FullMoveData* nihilLight = findFullMove(static_cast<MoveId>(kNihilLight));
  assert(nihilLight);
  const Observation nihilPlayer = observe(static_cast<MoveId>(kNihilLight), false);
  const Observation nihilOpponent = observe(static_cast<MoveId>(kNihilLight), true);
  printObservation(kNihilLight, *nihilLight, nihilPlayer, nihilOpponent);
  ++audited;
  if (!equivalent(nihilPlayer, nihilOpponent)) {
    ++asymmetric;
    printAsymmetry(kNihilLight, nihilPlayer, nihilOpponent);
  }
  std::cerr << "Audited " << audited << " moves across " << kSeeds
            << " seeds, three scenarios and both sides; asymmetric="
            << asymmetric << '\n';
  return 0;
}
