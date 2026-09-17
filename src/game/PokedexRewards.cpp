#include "game/PokedexRewards.h"

#include "game/Economy.h"

namespace {
constexpr uint8_t kFirstGeneration = 1;
constexpr uint8_t kLastGeneration = 3;
constexpr uint8_t kReservedFlagMask = 0x7EU;
}

uint8_t PokedexRewards::grantedBit(uint8_t generation) {
  return generation >= kFirstGeneration && generation <= kLastGeneration
      ? static_cast<uint8_t>(1U << generation) : 0U;
}

uint8_t PokedexRewards::acknowledgedBit(uint8_t generation) {
  return generation >= kFirstGeneration && generation <= kLastGeneration
      ? static_cast<uint8_t>(1U << (generation + 3U)) : 0U;
}

bool PokedexRewards::wasGranted(uint8_t saveFlags, uint8_t generation) {
  const uint8_t bit = grantedBit(generation);
  return bit != 0 && (saveFlags & bit) != 0;
}

uint8_t PokedexRewards::pendingGeneration(uint8_t saveFlags, uint8_t unlockedGeneration) {
  const uint8_t limit = unlockedGeneration > kLastGeneration ? kLastGeneration : unlockedGeneration;
  for (uint8_t generation = kFirstGeneration; generation <= limit; ++generation) {
    if ((saveFlags & grantedBit(generation)) != 0 &&
        (saveFlags & acknowledgedBit(generation)) == 0)
      return generation;
  }
  return 0;
}

PokedexRewardUpdate PokedexRewards::reconcile(const PokedexState& pokedex, Inventory& inventory,
                                              uint8_t unlockedGeneration, uint8_t& saveFlags) {
  PokedexRewardUpdate update{};
  const uint8_t limit = unlockedGeneration > kLastGeneration ? kLastGeneration : unlockedGeneration;
  uint16_t& masterBalls = inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)];
  for (uint8_t generation = kFirstGeneration; generation <= limit; ++generation) {
    const uint8_t bit = grantedBit(generation);
    if ((saveFlags & bit) != 0 || !PokedexLogic::isGenerationComplete(pokedex, generation)) continue;
    // A normal playthrough can earn only three Master Balls here.  Respect
    // the same stack ceiling as the Mart so even a modified beta save cannot
    // wrap the uint16 inventory counter.
    if (masterBalls >= kInventoryStackLimit) continue;
    ++masterBalls;
    saveFlags |= bit;
    update.grantedGenerations |= static_cast<uint8_t>(1U << (generation - 1U));
  }
  update.pendingGeneration = pendingGeneration(saveFlags, limit);
  return update;
}

void PokedexRewards::acknowledge(uint8_t& saveFlags, uint8_t generation) {
  const uint8_t granted = grantedBit(generation);
  if (granted != 0 && (saveFlags & granted) != 0)
    saveFlags |= acknowledgedBit(generation);
  // Compile-time documentation that the reward system occupies only the six
  // previously unused bits and never touches the shared special/Tower invite
  // flag (bit 0) or the
  // beta helper marker (bit 7).
  static_assert((kReservedFlagMask & 0x81U) == 0U, "Pokedex reward flags overlap existing save flags");
}
