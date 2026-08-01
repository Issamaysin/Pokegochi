#pragma once

#include <cstdint>

enum class CareAction : uint8_t { Feed, Bathe, Play };

enum class StatusCondition : uint8_t {
  None,
  Poison,
  BadlyPoisoned,
  Paralysis,
  Burn,
  Sleep,
  Frozen,
};

struct PetState {
  uint16_t speciesId = 1;
  uint8_t level = 5;
  uint32_t experience = 0;
  uint16_t currentHp = 20;
  uint16_t maximumHp = 20;
  uint8_t fullness = 80;
  uint8_t happiness = 80;
  StatusCondition status = StatusCondition::None;
  uint32_t defeatedUntil = 0;
  uint32_t careRemainderSeconds = 0;
  uint32_t careTickCounter = 0;
};

class PetLogic {
 public:
  static void advance(PetState& state, uint32_t elapsedSeconds);
  static void care(PetState& state, CareAction action);
  static bool canBattle(const PetState& state, uint32_t now);
  static void defeat(PetState& state, uint32_t now, uint32_t recoverySeconds);
  static void recoverIfReady(PetState& state, uint32_t now);
  static uint8_t condition(const PetState& state);
  static const char* statusName(StatusCondition status);

 private:
  static uint8_t addClamped(uint8_t value, uint8_t amount);
  static uint8_t subtractClamped(uint8_t value, uint32_t amount);
};
