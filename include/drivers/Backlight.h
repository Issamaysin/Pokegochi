#pragma once

#include <Arduino.h>

class Backlight {
 public:
  void begin(uint8_t pin, bool onLevel);
  void setEnabled(bool enabled);
  void setBrightness(uint8_t percent);
  bool isEnabled() const;
  uint8_t brightness() const;

 private:
  void apply();
  uint8_t pin_ = 0;
  bool onLevel_ = HIGH;
  bool enabled_ = true;
  uint8_t brightnessPercent_ = 100;
};
