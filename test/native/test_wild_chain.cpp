#include <cassert>

#include "game/WildChainSystem.h"

int main() {
  WildChainState state{};
  assert(!WildChainSystem::begin(state, 0U, 42U));
  assert(!WildChainSystem::begin(state, 51U, 42U));
  assert(!WildChainSystem::begin(state, 5U, 0U));

  assert(WildChainSystem::begin(state, 3U, 42U));
  assert(state.active && state.current == 1U && state.total == 3U);
  assert(WildChainSystem::remaining(state) == 3U);
  assert(WildChainSystem::completeEncounter(
      state, BattleOutcome::Victory, 43U, true) ==
      WildChainDecision::StartNext);
  assert(state.current == 2U && state.battlerUid == 43U);
  assert(WildChainSystem::completeEncounter(
      state, BattleOutcome::Captured, 43U, true) ==
      WildChainDecision::StartNext);
  assert(state.current == 3U);
  assert(WildChainSystem::completeEncounter(
      state, BattleOutcome::Victory, 43U, true) ==
      WildChainDecision::Finished);
  assert(!state.active && WildChainSystem::remaining(state) == 0U);

  assert(WildChainSystem::begin(state, 10U, 7U));
  assert(WildChainSystem::completeEncounter(
      state, BattleOutcome::Escaped, 7U, true) == WildChainDecision::Stop);
  assert(!state.active);
  assert(WildChainSystem::begin(state, 10U, 7U));
  assert(WildChainSystem::completeEncounter(
      state, BattleOutcome::Defeat, 7U, false) == WildChainDecision::Stop);
  assert(!state.active);
  assert(WildChainSystem::begin(state, 10U, 7U));
  assert(WildChainSystem::completeEncounter(
      state, BattleOutcome::Victory, 7U, false) == WildChainDecision::Stop);
  assert(!state.active);

  assert(WildChainSystem::begin(state, 1U, 9U));
  assert(WildChainSystem::completeEncounter(
      state, BattleOutcome::Captured, 9U, true) ==
      WildChainDecision::Finished);
  return 0;
}
