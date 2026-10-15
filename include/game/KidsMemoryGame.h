#pragma once

#include <cstdint>

namespace KidsMemoryGame {

constexpr uint8_t kMaximumCards = 16;

struct State {
  uint16_t matched = 0;
  int8_t first = -1;
  int8_t second = -1;
  uint16_t attempts = 0;
};

enum class SelectionResult : uint8_t {
  Ignored,
  Revealed,
  Matched,
  ClearedMismatch,
};

// An attempt is recorded only when a face-down, unmatched card is actually
// revealed. Taps outside the board are rejected by the caller; repeated taps
// on a visible card and taps on completed pairs are ignored here.
SelectionResult select(const uint8_t* cards, uint8_t cardCount,
                       uint8_t index, State& state);

}  // namespace KidsMemoryGame
