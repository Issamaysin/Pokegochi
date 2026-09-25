#pragma once

#include <cstdint>

enum class TouchInputMode : uint8_t {
  Finger = 0,
  Stylus = 1,
};

struct FilteredTouchPoint {
  bool touched = false;
  int16_t x = 0;
  int16_t y = 0;
};

// Filters mapped screen coordinates independently from the XPT2046 driver.
// Finger mode smooths small contact-patch noise, while Stylus mode preserves
// every mapped coordinate. A large jump starts a new sequence instead of
// averaging two physically unrelated presses into a point between them.
class TouchInputFilter {
 public:
  static constexpr int16_t kFingerJumpThreshold = 42;
  static constexpr int16_t kFingerDeadband = 1;

  void reset() {
    sampleCount_ = 0;
    sampleCursor_ = 0;
    hasOutput_ = false;
    modeKnown_ = false;
  }

  FilteredTouchPoint update(bool touched, int16_t x, int16_t y,
                            TouchInputMode mode) {
    if (!modeKnown_ || mode != mode_) {
      resetSamples();
      mode_ = mode;
      modeKnown_ = true;
    }
    if (!touched) {
      resetSamples();
      return {};
    }
    if (mode == TouchInputMode::Stylus) {
      resetSamples();
      return {true, x, y};
    }

    if (sampleCount_) {
      const uint8_t newest = static_cast<uint8_t>((sampleCursor_ + 2U) % 3U);
      if (difference(x, samplesX_[newest]) > kFingerJumpThreshold ||
          difference(y, samplesY_[newest]) > kFingerJumpThreshold) {
        // This is a new press or a fast deliberate move. Never create the
        // phantom midpoint that used to appear after changing screens.
        resetSamples();
      }
    }

    samplesX_[sampleCursor_] = x;
    samplesY_[sampleCursor_] = y;
    sampleCursor_ = static_cast<uint8_t>((sampleCursor_ + 1U) % 3U);
    if (sampleCount_ < 3U) ++sampleCount_;

    int16_t filteredX = x;
    int16_t filteredY = y;
    if (sampleCount_ == 2U) {
      filteredX = static_cast<int16_t>((samplesX_[0] + samplesX_[1]) / 2);
      filteredY = static_cast<int16_t>((samplesY_[0] + samplesY_[1]) / 2);
    } else if (sampleCount_ >= 3U) {
      filteredX = median3(samplesX_[0], samplesX_[1], samplesX_[2]);
      filteredY = median3(samplesY_[0], samplesY_[1], samplesY_[2]);
    }
    if (hasOutput_) {
      if (difference(filteredX, outputX_) <= kFingerDeadband) filteredX = outputX_;
      if (difference(filteredY, outputY_) <= kFingerDeadband) filteredY = outputY_;
    }
    outputX_ = filteredX;
    outputY_ = filteredY;
    hasOutput_ = true;
    return {true, filteredX, filteredY};
  }

 private:
  static int16_t difference(int16_t left, int16_t right) {
    return left >= right ? static_cast<int16_t>(left - right)
                         : static_cast<int16_t>(right - left);
  }

  static int16_t median3(int16_t a, int16_t b, int16_t c) {
    if (a > b) { const int16_t temporary = a; a = b; b = temporary; }
    if (b > c) { const int16_t temporary = b; b = c; c = temporary; }
    if (a > b) { const int16_t temporary = a; a = b; b = temporary; }
    return b;
  }

  void resetSamples() {
    sampleCount_ = 0;
    sampleCursor_ = 0;
    hasOutput_ = false;
  }

  int16_t samplesX_[3]{};
  int16_t samplesY_[3]{};
  int16_t outputX_ = 0;
  int16_t outputY_ = 0;
  uint8_t sampleCount_ = 0;
  uint8_t sampleCursor_ = 0;
  TouchInputMode mode_ = TouchInputMode::Finger;
  bool hasOutput_ = false;
  bool modeKnown_ = false;
};
