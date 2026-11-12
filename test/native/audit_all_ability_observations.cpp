#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "game/BattleEngine.h"
#include "game/Collection.h"
#include "game/PokemonData.h"

namespace {
constexpr MoveId move(uint16_t id) { return static_cast<MoveId>(id); }

enum class Role : uint8_t { Player, Opponent };

struct Fixture {
  PokemonCollection collection{};
  BattleState battle{};

  Fixture(MoveId playerMove, MoveId opponentMove, uint32_t seed) {
    CollectionLogic::initialize(collection);
    assert(CollectionLogic::chooseStarter(collection, 1U, 0xAB1E0001U));
    OwnedPokemon* owned = CollectionLogic::active(collection, 0U);
    assert(owned);
    const uint32_t uid = owned->uid;
    *owned = CollectionLogic::createPokemon(uid, 137U, 75U, false, 0xAB1E0002U);
    owned->moves[0] = playerMove;
    owned->movePp[0] = findFullMove(playerMove)->pp;
    owned->abilityId = 0U;
    owned->heldItem = HeldItem::None;
    owned->maximumHp = owned->currentHp = 2000U;

    BattleEngine::clear(battle);
    battle.active = true;
    battle.kind = BattleKind::Pvp;
    battle.outcome = BattleOutcome::Ongoing;
    battle.playerUid = uid;
    battle.opponentCount = 1U;
    battle.unlockedGeneration = 7U;
    battle.rngState = seed;
    battle.opponents[0] = CollectionLogic::createPokemon(
        0xF1AB1E01U, 137U, 50U, false, 0xAB1E0003U);
    OwnedPokemon& foe = battle.opponents[0];
    foe.moves[0] = opponentMove;
    foe.movePp[0] = findFullMove(opponentMove)->pp;
    foe.abilityId = 0U;
    foe.heldItem = HeldItem::None;
    foe.maximumHp = foe.currentHp = 2000U;
  }

  OwnedPokemon& player() {
    OwnedPokemon* value = CollectionLogic::find(collection, battle.playerUid);
    assert(value);
    return *value;
  }
  OwnedPokemon& opponent() { return battle.opponents[0]; }
  BattleActionResult act(bool both = false) {
    return BattleEngine::fightPvp(battle, collection, 0U, both ? 0U : 0xFEU);
  }
};

struct Digest {
  uint16_t playerHp = 0U;
  uint16_t opponentHp = 0U;
  StatusCondition playerStatus = StatusCondition::None;
  StatusCondition opponentStatus = StatusCondition::None;
  std::array<int8_t, 8U> playerStages{};
  std::array<int8_t, 8U> opponentStages{};
  BattleWeather weather = BattleWeather::Clear;
  Gen6Terrain terrain = Gen6Terrain::None;
  uint8_t playerForm = 0U;
  uint8_t opponentForm = 0U;
  bool playerAbilityChanged = false;
  bool opponentAbilityChanged = false;
  bool playerItemChanged = false;
  bool opponentItemChanged = false;
  bool playerTypeChanged = false;
  bool opponentTypeChanged = false;
  uint8_t firstMoveSide = 2U;
  uint8_t criticalEvents = 0U;
  uint8_t statEvents = 0U;
  uint8_t statusEvents = 0U;
  uint8_t faintEvents = 0U;
  uint8_t abilityEvents = 0U;
};

std::array<int8_t, 8U> stages(const CombatVolatile& state) {
  return {{state.attackStage, state.defenseStage, state.spAttackStage,
           state.spDefenseStage, state.speedStage, state.accuracyStage,
           state.evasionStage, static_cast<int8_t>(state.criticalStage)}};
}

Digest digest(const Fixture& source, const BattleActionResult& result,
              uint8_t playerAbilityBefore, uint8_t opponentAbilityBefore,
              HeldItem playerItemBefore, HeldItem opponentItemBefore) {
  Fixture& fixture = const_cast<Fixture&>(source);
  Digest value{};
  value.playerHp = fixture.player().currentHp;
  value.opponentHp = fixture.opponent().currentHp;
  value.playerStatus = fixture.player().status;
  value.opponentStatus = fixture.opponent().status;
  value.playerStages = stages(fixture.battle.playerVolatile);
  value.opponentStages = stages(fixture.battle.opponentVolatiles[0]);
  value.weather = fixture.battle.weather;
  value.terrain = fixture.battle.gen6Terrain;
  value.playerForm = fixture.player().form;
  value.opponentForm = fixture.opponent().form;
  value.playerAbilityChanged = fixture.player().abilityId != playerAbilityBefore;
  value.opponentAbilityChanged = fixture.opponent().abilityId != opponentAbilityBefore;
  value.playerItemChanged = fixture.player().heldItem != playerItemBefore;
  value.opponentItemChanged = fixture.opponent().heldItem != opponentItemBefore;
  value.playerTypeChanged = fixture.battle.playerMoveEffects.typeOverrideActive;
  value.opponentTypeChanged =
      fixture.battle.opponentMoveEffects[0].typeOverrideActive;
  for (uint8_t index = 0U; index < result.eventCount; ++index) {
    const BattleEvent& event = result.events[index];
    if (event.type == BattleEventType::MoveUsed && value.firstMoveSide == 2U)
      value.firstMoveSide = static_cast<uint8_t>(event.side);
    if (event.type == BattleEventType::CriticalHit) ++value.criticalEvents;
    if (event.type == BattleEventType::StatChanged) ++value.statEvents;
    if (event.type == BattleEventType::StatusApplied ||
        event.type == BattleEventType::StatusCured)
      ++value.statusEvents;
    if (event.type == BattleEventType::Fainted) ++value.faintEvents;
    if (event.type == BattleEventType::AbilityActivated) ++value.abilityEvents;
  }
  return value;
}

bool same(const Digest& left, const Digest& right) {
  return std::memcmp(&left, &right, sizeof(Digest)) == 0;
}

struct Scenario {
  const char* name;
  Role role;
  MoveId playerMove;
  MoveId opponentMove;
  BattleWeather weather = BattleWeather::Clear;
  StatusCondition holderStatus = StatusCondition::None;
  uint16_t holderHp = 2000U;
  uint16_t otherHp = 2000U;
  HeldItem holderItem = HeldItem::None;
  bool bothAct = false;
  bool entryOnly = false;
  int8_t holderAccuracy = 0;
  int8_t holderEvasion = 0;
};

const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> values = {
      {"entry", Role::Player, move(150), move(150), BattleWeather::Clear,
       StatusCondition::None, 2000, 2000, HeldItem::None, false, true},
      {"entry-rain", Role::Player, move(150), move(150), BattleWeather::Rain,
       StatusCondition::None, 2000, 2000, HeldItem::None, false, true},
      {"attack-contact", Role::Player, move(33), move(150)},
      {"attack-low-hp", Role::Player, move(33), move(150), BattleWeather::Clear,
       StatusCondition::None, 500},
      {"attack-poisoned", Role::Player, move(33), move(150), BattleWeather::Clear,
       StatusCondition::Poison},
      {"attack-fire-sun", Role::Player, move(53), move(150), BattleWeather::Sun,
       StatusCondition::None, 500},
      {"attack-water-rain", Role::Player, move(55), move(150), BattleWeather::Rain},
      {"attack-grass", Role::Player, move(75), move(150)},
      {"attack-bite", Role::Player, move(44), move(150)},
      {"attack-pulse", Role::Player, move(396), move(150)},
      {"attack-multihit", Role::Player, move(331), move(150)},
      {"attack-priority", Role::Player, move(98), move(33), BattleWeather::Clear,
       StatusCondition::None, 2000, 2000, HeldItem::None, true},
      {"attack-ko", Role::Player, move(33), move(150), BattleWeather::Clear,
       StatusCondition::None, 2000, 1},
      {"defend-contact", Role::Opponent, move(33), move(150)},
      {"defend-fire", Role::Opponent, move(53), move(150)},
      {"defend-water", Role::Opponent, move(55), move(150)},
      {"defend-electric", Role::Opponent, move(85), move(150)},
      {"defend-grass", Role::Opponent, move(75), move(150)},
      {"defend-growl", Role::Opponent, move(45), move(150)},
      {"defend-tail-whip", Role::Opponent, move(39), move(150)},
      {"defend-toxic", Role::Opponent, move(92), move(150)},
      {"defend-sleep", Role::Opponent, move(47), move(150)},
      {"defend-confuse", Role::Opponent, move(109), move(150)},
      {"defend-sound", Role::Opponent, move(304), move(150)},
      {"defend-priority", Role::Opponent, move(98), move(33),
       BattleWeather::Clear, StatusCondition::None, 2000, 2000,
       HeldItem::None, true},
      {"end-clear", Role::Player, move(150), move(150), BattleWeather::Clear,
       StatusCondition::None, 1000},
      {"end-rain", Role::Player, move(150), move(150), BattleWeather::Rain,
       StatusCondition::None, 1000},
      {"end-sun", Role::Player, move(150), move(150), BattleWeather::Sun,
       StatusCondition::None, 1000},
      {"end-sand", Role::Player, move(150), move(150), BattleWeather::Sandstorm,
       StatusCondition::None, 1000},
      {"end-hail", Role::Player, move(150), move(150), BattleWeather::Hail,
       StatusCondition::None, 1000},
      {"end-poison", Role::Player, move(150), move(150), BattleWeather::Clear,
       StatusCondition::Poison, 1000},
      {"berry-low-hp", Role::Player, move(150), move(150), BattleWeather::Clear,
       StatusCondition::None, 400, 2000, HeldItem::OranBerry},
      {"accuracy-low", Role::Player, move(56), move(150), BattleWeather::Clear,
       StatusCondition::None, 2000, 2000, HeldItem::None, false, false, -6, 0},
      {"evasion-high", Role::Opponent, move(56), move(150), BattleWeather::Clear,
       StatusCondition::None, 2000, 2000, HeldItem::None, false, false, 0, 6},
  };
  return values;
}

Digest run(const Scenario& scenario, uint8_t ability, uint32_t seed) {
  Fixture fixture(scenario.playerMove, scenario.opponentMove, seed);
  OwnedPokemon& holder = scenario.role == Role::Player
      ? fixture.player() : fixture.opponent();
  holder.abilityId = ability;
  holder.status = scenario.holderStatus;
  holder.currentHp = scenario.holderHp;
  holder.heldItem = scenario.holderItem;
  OwnedPokemon& other = scenario.role == Role::Player
      ? fixture.opponent() : fixture.player();
  other.currentHp = scenario.otherHp;
  fixture.battle.weather = scenario.weather;
  fixture.battle.weatherTurns = scenario.weather == BattleWeather::Clear ? 0U : 0xFFU;
  CombatVolatile& holderState = scenario.role == Role::Player
      ? fixture.battle.playerVolatile : fixture.battle.opponentVolatiles[0];
  holderState.accuracyStage = scenario.holderAccuracy;
  holderState.evasionStage = scenario.holderEvasion;
  const uint8_t playerAbilityBefore = fixture.player().abilityId;
  const uint8_t opponentAbilityBefore = fixture.opponent().abilityId;
  const HeldItem playerItemBefore = fixture.player().heldItem;
  const HeldItem opponentItemBefore = fixture.opponent().heldItem;
  BattleActionResult result{};
  if (scenario.entryOnly) {
    BattleEngine::applyEntryAbilities(fixture.battle, fixture.collection,
        scenario.role == Role::Player ? BattleEntryScope::Player
                                      : BattleEntryScope::Opponent,
        &result);
  } else {
    result = fixture.act(scenario.bothAct);
  }
  return digest(fixture, result, playerAbilityBefore, opponentAbilityBefore,
                playerItemBefore, opponentItemBefore);
}
}  // namespace

int main() {
  uint16_t audited = 0U;
  uint16_t observed = 0U;
  for (uint16_t raw = 1U; raw <= 255U; ++raw) {
    const AbilityData* ability = findAbility(static_cast<uint8_t>(raw));
    if (!ability) continue;
    ++audited;
    std::string hits;
    for (const Scenario& scenario : scenarios()) {
      bool changed = false;
      for (uint32_t seed = 1U; seed <= 128U && !changed; ++seed) {
        const uint32_t actualSeed = 0xAB1E0000U ^ seed;
        const Digest baseline = run(scenario, 0U, actualSeed);
        const Digest candidate = run(scenario, static_cast<uint8_t>(raw), actualSeed);
        changed = !same(baseline, candidate);
      }
      if (changed) {
        if (!hits.empty()) hits += ';';
        hits += scenario.name;
      }
    }
    if (!hits.empty()) ++observed;
    std::cout << raw << ',' << ability->name << ','
              << (hits.empty() ? "UNOBSERVED" : hits) << '\n';
  }
  std::cerr << "Ability observer audited=" << audited
            << " observed=" << observed
            << " unobserved=" << (audited - observed) << '\n';
  return 0;
}
