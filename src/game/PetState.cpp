#include "game/PetState.h"

#include <algorithm>

uint8_t PetLogic::addClamped(uint8_t value, uint8_t amount) {
  return static_cast<uint8_t>(std::min<uint16_t>(100, value + amount));
}

uint8_t PetLogic::subtractClamped(uint8_t value, uint32_t amount) {
  return amount >= value ? 0 : static_cast<uint8_t>(value - amount);
}

void PetLogic::advance(PetState& state, uint32_t elapsedSeconds) {
  // One simulation tick is 15 minutes. Remainders survive save/load.
  const uint64_t total = static_cast<uint64_t>(state.careRemainderSeconds) + elapsedSeconds;
  const uint32_t ticks = static_cast<uint32_t>(total / 900U);
  state.careRemainderSeconds = static_cast<uint32_t>(total % 900U);
  if (ticks == 0) return;

  for (uint32_t tick = 0; tick < ticks; ++tick) {
    ++state.careTickCounter;
    state.fullness = subtractClamped(state.fullness, 1);
    if (state.fullness < 25 || state.careTickCounter % 4U == 0) {
      state.happiness = subtractClamped(state.happiness, 1);
    }
    if (state.fullness == 0 && state.happiness == 0) {
      state.careTickCounter += ticks - tick - 1U;
      break;
    }
  }
}

void PetLogic::care(PetState& state, CareAction action) {
  switch (action) {
    case CareAction::Feed:
      state.fullness = addClamped(state.fullness, 25);
      state.happiness = addClamped(state.happiness, 3);
      break;
    case CareAction::Bathe:
      state.status = StatusCondition::None;
      state.happiness = addClamped(state.happiness, 2);
      break;
    case CareAction::Play:
      state.happiness = addClamped(state.happiness, 15);
      state.fullness = subtractClamped(state.fullness, 3);
      break;
  }
}

bool PetLogic::canBattle(const PetState& state, uint32_t now) {
  return state.currentHp > 0 && (state.defeatedUntil == 0 || now >= state.defeatedUntil);
}

void PetLogic::defeat(PetState& state, uint32_t now, uint32_t recoverySeconds) {
  state.currentHp = 0;
  state.defeatedUntil = now + recoverySeconds;
}

void PetLogic::recoverIfReady(PetState& state, uint32_t now) {
  if (state.defeatedUntil != 0 && now >= state.defeatedUntil) {
    state.currentHp = state.maximumHp;
    state.defeatedUntil = 0;
  }
}

uint8_t PetLogic::condition(const PetState& state) {
  return static_cast<uint8_t>((static_cast<uint16_t>(state.fullness) + state.happiness) / 2U);
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
