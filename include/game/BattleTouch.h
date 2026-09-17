#pragma once

#include <cstdint>

namespace BattleTouch {

enum class RunConfirmationAction : uint8_t {
  None,
  Accept,
  Cancel,
};

// These hit regions cover the complete visible rows of the FireRed-style
// RUN modal.  Keeping the test independent from the generic touch tolerance
// prevents a narrow dead strip along CANCEL on the resistive panel.
inline RunConfirmationAction runConfirmationActionAt(int16_t x, int16_t y) {
  constexpr int16_t kLeft = 107;
  constexpr int16_t kRight = 213;
  if (x < kLeft || x >= kRight) return RunConfirmationAction::None;
  if (y >= 76 && y < 106) return RunConfirmationAction::Accept;
  if (y >= 106 && y < 143) return RunConfirmationAction::Cancel;
  return RunConfirmationAction::None;
}

}  // namespace BattleTouch
