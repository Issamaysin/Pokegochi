#pragma once

#include <cstdint>

namespace BattleTouch {

// The RUN confirmation lives in the arena rather than in the command pane.
// Expose its complete retained-compositor footprint so opening and closing
// the modal can invalidate the same pixels on every platform.
constexpr int16_t kRunConfirmationX = 110;
constexpr int16_t kRunConfirmationY = 50;
constexpr int16_t kRunConfirmationWidth = 100;
constexpr int16_t kRunConfirmationHeight = 88;
constexpr int16_t kRunConfirmationDamageX = 106;
constexpr int16_t kRunConfirmationDamageY = 46;
constexpr int16_t kRunConfirmationDamageWidth = 108;
constexpr int16_t kRunConfirmationDamageHeight = 99;

inline bool runConfirmationIntersectsClip(int16_t left, int16_t top,
                                          int16_t right, int16_t bottom) {
  return left < kRunConfirmationDamageX + kRunConfirmationDamageWidth &&
      right > kRunConfirmationDamageX &&
      top < kRunConfirmationDamageY + kRunConfirmationDamageHeight &&
      bottom > kRunConfirmationDamageY;
}

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
