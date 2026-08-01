#pragma once

#include <Arduino.h>

class Backlight {
 public:
  void begin(uint8_t pin, bool onLevel);
  void setEnabled(bool enabled);
  void toggle();
  bool isEnabled() const;

 private:
  uint8_t pin_ = 0;
  bool onLevel_ = HIGH;
  bool enabled_ = true;
};

