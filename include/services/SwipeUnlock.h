#pragma once

#include <cstdint>

// Small input-only state machine for the screen-off guard.  It deliberately
// knows nothing about TFT or Arduino so the exact pocket-safety rules can be
// exercised by native tests.
class SwipeUnlock {
 public:
  enum class Event : uint8_t { None, PositionChanged, Reset, Unlocked };

  static constexpr int16_t kTrackX = 28;
  static constexpr int16_t kTrackY = 164;
  static constexpr int16_t kTrackWidth = 264;
  static constexpr int16_t kTrackHeight = 54;
  static constexpr int16_t kBallRadius = 21;
  static constexpr int16_t kStartX = kTrackX + 27;
  static constexpr int16_t kFinishX = kTrackX + kTrackWidth - 27;
  static constexpr int16_t kCenterY = kTrackY + kTrackHeight / 2;

  void activate() {
    active_ = true;
    awaitingWakeTouchRelease_ = true;
    dragging_ = false;
    ballX_ = kStartX;
  }

  void deactivate() {
    active_ = false;
    awaitingWakeTouchRelease_ = false;
    dragging_ = false;
    ballX_ = kStartX;
  }

  Event update(bool touched, int16_t x, int16_t y) {
    if (!active_) return Event::None;
    if (!touched) {
      if (awaitingWakeTouchRelease_) {
        awaitingWakeTouchRelease_ = false;
        return Event::None;
      }
      if (dragging_) {
        dragging_ = false;
        const bool changed = ballX_ != kStartX;
        ballX_ = kStartX;
        return changed ? Event::Reset : Event::None;
      }
      return Event::None;
    }
    if (awaitingWakeTouchRelease_) return Event::None;

    // Resistive touch coordinates wander while a finger slides. Preserve a
    // valid drag through a moderate excursion outside the visible rail; a
    // released/abandoned gesture still springs back for pocket safety.
    constexpr int16_t kVerticalTolerance = 30;
    if (y < kTrackY - kVerticalTolerance ||
        y >= kTrackY + kTrackHeight + kVerticalTolerance) {
      const bool changed = dragging_ || ballX_ != kStartX;
      dragging_ = false;
      awaitingWakeTouchRelease_ = true;
      ballX_ = kStartX;
      return changed ? Event::Reset : Event::None;
    }

    if (!dragging_) {
      const int16_t dx = x >= ballX_ ? x - ballX_ : ballX_ - x;
      const int16_t dy = y >= kCenterY ? y - kCenterY : kCenterY - y;
      constexpr int16_t kGrabTolerance = 13;
      if (dx > kBallRadius + kGrabTolerance ||
          dy > kBallRadius + kGrabTolerance) return Event::None;
      dragging_ = true;
    }

    int16_t nextX = x;
    if (nextX < kStartX) nextX = kStartX;
    if (nextX > kFinishX) nextX = kFinishX;
    const bool changed = nextX != ballX_;
    ballX_ = nextX;
    if (ballX_ >= kFinishX) {
      active_ = false;
      dragging_ = false;
      return Event::Unlocked;
    }
    return changed ? Event::PositionChanged : Event::None;
  }

  bool active() const { return active_; }
  bool dragging() const { return dragging_; }
  bool awaitingWakeTouchRelease() const { return awaitingWakeTouchRelease_; }
  int16_t ballX() const { return ballX_; }

 private:
  bool active_ = false;
  bool awaitingWakeTouchRelease_ = false;
  bool dragging_ = false;
  int16_t ballX_ = kStartX;
};
