#pragma once

#include <cstdint>
#include "game/GymSystem.h"

struct MegaChallengeTrainerDefinition {
  const char* trainerClass;
  const char* name;
  const char* frontAsset;
  const char* introLine1;
  const char* introLine2;
};

// One persistent post-League boss separates the story campaign from the
// repeatable Battle Tower.  The completion ledger deliberately occupies an
// unused high bit of GymProgress::starterClaimedBits, preserving the current
// save layout and every existing Box entry.
class MegaChallengeSystem {
 public:
  static constexpr uint8_t kCompletionBit = 1U << 7U;

  static bool prerequisitesMet(const GymProgress& progress);
  static bool available(const GymProgress& progress);
  static bool completed(const GymProgress& progress);
  static bool start(BattleState& battle, PokemonCollection& collection,
                    uint32_t playerUid, const GymProgress& progress, uint32_t seed);
  static bool awardVictory(GymProgress& progress, uint64_t& ownedMachines);

  // Removes the development stone from unfinished saves and repairs the
  // unique reward after completion if neither the Bag nor a Pokemon has it.
  static bool reconcileStoneGate(const GymProgress& progress,
                                 PokemonCollection& collection,
                                 uint64_t& ownedMachines);

  static const MegaChallengeTrainerDefinition& trainer();
};
