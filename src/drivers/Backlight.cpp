#include "drivers/Backlight.h"

void Backlight::begin(uint8_t pin, bool onLevel) {
  pin_ = pin;
  onLevel_ = onLevel;
  pinMode(pin_, OUTPUT);
  apply();
}

void Backlight::setEnabled(bool enabled) {
  enabled_ = enabled;
  apply();
}

void Backlight::setBrightness(uint8_t percent) {
  brightnessPercent_ = constrain(percent, 20, 100);
  apply();
}

void Backlight::apply() {
  const uint8_t brightnessDuty = static_cast<uint8_t>(255U * brightnessPercent_ / 100U);
  const uint8_t duty = enabled_ ? (onLevel_ ? brightnessDuty : 255U - brightnessDuty)
                                : (onLevel_ ? 0U : 255U);
  analogWrite(pin_, duty);
}

bool Backlight::isEnabled() const { return enabled_; }
uint8_t Backlight::brightness() const { return brightnessPercent_; }
