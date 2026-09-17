#pragma once

#include <cstdint>
#include "game/BattleEngine.h"
#include "game/Pokedex.h"

struct PokedexRewardUpdate {
  uint8_t grantedGenerations = 0;
  uint8_t pendingGeneration = 0;
};

// The save already reserves bits 1-6 in GameSave::flags.  Bits 1-3 record
// delivery of each regional Master Ball and bits 4-6 record that its visible
// presentation was acknowledged.  Keeping both ledgers persistent prevents a
// power loss from either duplicating the item or silently swallowing the
// notification.
class PokedexRewards {
 public:
  static PokedexRewardUpdate reconcile(const PokedexState& pokedex, Inventory& inventory,
                                        uint8_t unlockedGeneration, uint8_t& saveFlags);
  static uint8_t pendingGeneration(uint8_t saveFlags, uint8_t unlockedGeneration);
  static void acknowledge(uint8_t& saveFlags, uint8_t generation);
  static bool wasGranted(uint8_t saveFlags, uint8_t generation);

 private:
  static uint8_t grantedBit(uint8_t generation);
  static uint8_t acknowledgedBit(uint8_t generation);
};
