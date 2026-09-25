#pragma once

#include <Arduino.h>

struct TouchPoint {
  bool touched = false;
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  int16_t x = 0;
  int16_t y = 0;
};

class TouchDriver {
 public:
  void begin();
  void setFingerMode(bool enabled) { fingerMode_ = enabled; }
  TouchPoint read();

 private:
  uint8_t transfer(uint8_t value);
  uint16_t readChannel(uint8_t command);
  uint16_t median3(uint16_t a, uint16_t b, uint16_t c);
  uint16_t median5(uint16_t values[5]);
  bool fingerMode_ = true;
};
