#include "drivers/RgbLed.h"

void RgbLed::begin(uint8_t redPin, uint8_t greenPin, uint8_t bluePin, bool activeLow) {
  redPin_ = redPin; greenPin_ = greenPin; bluePin_ = bluePin; activeLow_ = activeLow;
  pinMode(redPin_, OUTPUT); pinMode(greenPin_, OUTPUT); pinMode(bluePin_, OUTPUT);
  off();
}

void RgbLed::write(uint8_t pin, bool enabled) {
  digitalWrite(pin, enabled != activeLow_ ? HIGH : LOW);
}

void RgbLed::set(bool red, bool green, bool blue) {
  write(redPin_, red); write(greenPin_, green); write(bluePin_, blue);
}

void RgbLed::off() { set(false, false, false); }
