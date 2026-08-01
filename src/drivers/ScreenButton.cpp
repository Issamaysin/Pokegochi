#include "drivers/ScreenButton.h"

void ScreenButton::begin(uint8_t pin, bool pressedLevel, uint32_t debounceMs) {
  pin_ = pin;
  pressedLevel_ = pressedLevel;
  debounceMs_ = debounceMs;
  pinMode(pin_, INPUT);  // GPIO35 requires an external pull-up.
  sampledState_ = digitalRead(pin_) == pressedLevel_;
  stableState_ = sampledState_;
  changedAtMs_ = millis();
}

bool ScreenButton::pressed() {
  const bool sample = digitalRead(pin_) == pressedLevel_;
  if (sample != sampledState_) {
    sampledState_ = sample;
    changedAtMs_ = millis();
  }

  if (stableState_ != sampledState_ && millis() - changedAtMs_ >= debounceMs_) {
    stableState_ = sampledState_;
    return stableState_;
  }
  return false;
}

