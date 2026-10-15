#pragma once

#include <cstdint>
#include "game/BattleEngine.h"

// Transient convenience flow for endgame farming. Every member is deliberately
// outside GameSave: each Wild battle is committed normally, while a reboot or
// leaving the flow simply ends the convenience chain.
struct WildChainState {
  uint32_t battlerUid = 0;
  uint8_t total = 0;
  uint8_t current = 0;  // One-based encounter currently being played.
  bool active = false;
};

enum class WildChainDecision : uint8_t {
  Stop,
  StartNext,
  Finished,
};

class WildChainSystem {
 public:
  static constexpr uint8_t kMinimumEncounters = 1;
  static constexpr uint8_t kMaximumEncounters = 50;

  static bool begin(WildChainState& state, uint8_t encounters,
                    uint32_t battlerUid);
  static WildChainDecision completeEncounter(WildChainState& state,
                                             BattleOutcome outcome,
                                             uint32_t nextBattlerUid,
                                             bool battlerUsable);
  static void cancel(WildChainState& state);
  static uint8_t remaining(const WildChainState& state);
};
