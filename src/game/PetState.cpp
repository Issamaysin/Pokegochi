#include "game/PetState.h"

#include <algorithm>

uint8_t PetLogic::addClamped(uint8_t value, uint8_t amount) {
  return static_cast<uint8_t>(std::min<uint16_t>(255, value + amount));
}

void PetLogic::advanceFriendship(PetState& state, uint32_t elapsedSeconds) {
  constexpr uint32_t kFriendshipTickSeconds = 60U * 60U;
  const uint64_t total = static_cast<uint64_t>(state.friendshipRemainderSeconds) + elapsedSeconds;
  const uint32_t ticks = static_cast<uint32_t>(total / kFriendshipTickSeconds);
  state.friendshipRemainderSeconds =
      static_cast<uint32_t>(total % kFriendshipTickSeconds);
  if (ticks == 0) return;

  for (uint32_t tick = 0; tick < ticks; ++tick) {
    const uint8_t gain = state.friendship < 100U ? 5U
                         : state.friendship < 200U ? 3U : 2U;
    state.friendship = addClamped(state.friendship, gain);
    if (state.friendship == 255U) break;
  }
}

void PetLogic::care(PetState& state, CareAction action) {
  switch (action) {
    case CareAction::Center:
      state.status = StatusCondition::None;
      state.currentHp = state.maximumHp;
      state.defeatedUntil = 0;
      break;
  }
}

bool PetLogic::canBattle(const PetState& state, uint32_t now) {
  return state.currentHp > 0 && (state.defeatedUntil == 0 || now >= state.defeatedUntil);
}

void PetLogic::defeat(PetState& state, uint32_t now, uint32_t recoverySeconds) {
  state.currentHp = 0;
  state.status = StatusCondition::None;
  state.defeatedUntil = now + recoverySeconds;
}

void PetLogic::recoverIfReady(PetState& state, uint32_t now) {
  if (state.defeatedUntil != 0 && now >= state.defeatedUntil) {
    state.currentHp = state.maximumHp;
    state.defeatedUntil = 0;
  }
}

const char* PetLogic::statusName(StatusCondition status) {
  switch (status) {
    case StatusCondition::Poison: return "POISON";
    case StatusCondition::BadlyPoisoned: return "BAD POISON";
    case StatusCondition::Paralysis: return "PARALYSIS";
    case StatusCondition::Burn: return "BURN";
    case StatusCondition::Sleep: return "SLEEP";
    case StatusCondition::Frozen: return "FROZEN";
    case StatusCondition::None: return "HEALTHY";
  }
  return "UNKNOWN";
}

const char* PetLogic::statusAdjective(StatusCondition status) {
  switch (status) {
    case StatusCondition::Poison: return "POISONED";
    case StatusCondition::BadlyPoisoned: return "BADLY POISONED";
    case StatusCondition::Paralysis: return "PARALYZED";
    case StatusCondition::Burn: return "BURNED";
    case StatusCondition::Sleep: return "ASLEEP";
    case StatusCondition::Frozen: return "FROZEN";
    case StatusCondition::None: return "HEALTHY";
  }
  return "UNKNOWN";
}
