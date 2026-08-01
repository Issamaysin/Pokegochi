#include "drivers/Backlight.h"

void Backlight::begin(uint8_t pin, bool onLevel) {
  pin_ = pin;
  onLevel_ = onLevel;
  pinMode(pin_, OUTPUT);
  setEnabled(true);
}

void Backlight::setEnabled(bool enabled) {
  enabled_ = enabled;
  digitalWrite(pin_, enabled_ ? onLevel_ : !onLevel_);
}

void Backlight::toggle() { setEnabled(!enabled_); }

bool Backlight::isEnabled() const { return enabled_; }

