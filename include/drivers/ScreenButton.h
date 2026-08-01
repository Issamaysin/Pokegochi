#pragma once

#include <Arduino.h>

class ScreenButton {
 public:
  void begin(uint8_t pin, bool pressedLevel, uint32_t debounceMs = 35);
  bool pressed();

 private:
  uint8_t pin_ = 0;
  bool pressedLevel_ = LOW;
  bool stableState_ = false;
  bool sampledState_ = false;
  uint32_t changedAtMs_ = 0;
  uint32_t debounceMs_ = 35;
};

