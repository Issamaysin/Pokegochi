#pragma once

#include <cstdint>

// The former food/play actions belonged to the removed Fullness mechanic.
// The sole care interaction is now the Pokemon Center recovery action.
enum class CareAction : uint8_t { Center };

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
  uint8_t friendship = 70;
  StatusCondition status = StatusCondition::None;
  uint32_t defeatedUntil = 0;
  uint32_t friendshipRemainderSeconds = 0;
};

class PetLogic {
 public:
  // FireRed's friendship brackets (+5 below 100, +3 below 200 and +2
  // thereafter) are retained. Pokegochi applies one such step per active
  // party hour instead of requiring map walking.
  static void advanceFriendship(PetState& state, uint32_t elapsedSeconds);
  static void care(PetState& state, CareAction action);
  static bool canBattle(const PetState& state, uint32_t now);
  static void defeat(PetState& state, uint32_t now, uint32_t recoverySeconds);
  static void recoverIfReady(PetState& state, uint32_t now);
  // Noun used by status panels (POISON, PARALYSIS, ...).
  static const char* statusName(StatusCondition status);
  // Grammatical state used in battle prose (POISONED, PARALYZED, ...).
  static const char* statusAdjective(StatusCondition status);

 private:
  static uint8_t addClamped(uint8_t value, uint8_t amount);
};
