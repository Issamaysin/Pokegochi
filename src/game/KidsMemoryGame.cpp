#include "game/KidsMemoryGame.h"

#include <cstdint>

namespace KidsMemoryGame {

SelectionResult select(const uint8_t* cards, uint8_t cardCount,
                       uint8_t index, State& state) {
  if (!cards || !cardCount || cardCount > kMaximumCards || index >= cardCount)
    return SelectionResult::Ignored;

  const uint16_t selectedBit = static_cast<uint16_t>(1U << index);
  if (state.matched & selectedBit) return SelectionResult::Ignored;

  if (state.second >= 0) {
    const bool alreadyVisible = state.first == static_cast<int8_t>(index) ||
        state.second == static_cast<int8_t>(index);
    state.first = -1;
    state.second = -1;
    if (alreadyVisible) return SelectionResult::ClearedMismatch;
  }

  if (state.first == static_cast<int8_t>(index))
    return SelectionResult::Ignored;

  if (state.attempts < UINT16_MAX) ++state.attempts;
  if (state.first < 0) {
    state.first = static_cast<int8_t>(index);
    return SelectionResult::Revealed;
  }

  state.second = static_cast<int8_t>(index);
  if (cards[static_cast<uint8_t>(state.first)] != cards[index])
    return SelectionResult::Revealed;

  state.matched = static_cast<uint16_t>(state.matched |
      (1U << static_cast<uint8_t>(state.first)) | selectedBit);
  state.first = -1;
  state.second = -1;
  return SelectionResult::Matched;
}

}  // namespace KidsMemoryGame
