#pragma once

#include <Arduino.h>

class RgbLed {
 public:
  void begin(uint8_t redPin, uint8_t greenPin, uint8_t bluePin, bool activeLow);
  void set(bool red, bool green, bool blue);
  void off();

 private:
  void write(uint8_t pin, bool enabled);
  uint8_t redPin_ = 0;
  uint8_t greenPin_ = 0;
  uint8_t bluePin_ = 0;
  bool activeLow_ = true;
};
