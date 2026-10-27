#include "game/WildChainSystem.h"
#include "game/GymSystem.h"

bool WildChainSystem::isUnlocked(const GymProgress& progress) {
  constexpr uint32_t kValidBadgeMask =
      (1UL << static_cast<uint8_t>(GymId::Count)) - 1UL;
  return (progress.badgeBits & kValidBadgeMask) != 0U;
}

bool WildChainSystem::begin(WildChainState& state, uint8_t encounters,
                            uint32_t battlerUid) {
  if (encounters < kMinimumEncounters || encounters > kMaximumEncounters ||
      battlerUid == 0U) {
    cancel(state);
    return false;
  }
  state.battlerUid = battlerUid;
  state.total = encounters;
  state.current = 1U;
  state.active = true;
  return true;
}

WildChainDecision WildChainSystem::completeEncounter(
    WildChainState& state, BattleOutcome outcome, uint32_t nextBattlerUid,
    bool battlerUsable) {
  if (!state.active) return WildChainDecision::Stop;
  if (outcome != BattleOutcome::Victory && outcome != BattleOutcome::Captured) {
    cancel(state);
    return WildChainDecision::Stop;
  }
  if (state.current >= state.total) {
    cancel(state);
    return WildChainDecision::Finished;
  }
  if (!battlerUsable || nextBattlerUid == 0U) {
    cancel(state);
    return WildChainDecision::Stop;
  }
  state.battlerUid = nextBattlerUid;
  ++state.current;
  return WildChainDecision::StartNext;
}

void WildChainSystem::cancel(WildChainState& state) {
  state = WildChainState{};
}

uint8_t WildChainSystem::remaining(const WildChainState& state) {
  return state.active && state.current <= state.total
      ? static_cast<uint8_t>(state.total - state.current + 1U) : 0U;
}
