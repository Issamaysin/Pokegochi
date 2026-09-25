#pragma once

#include <cstdint>

namespace BatteryProtection {

constexpr uint8_t kCutoffPercent = 10U;
constexpr uint32_t kDormantRecheckSeconds = 15U;

// An invalid/floating ADC input must never make the device shut itself down.
constexpr bool shouldEnterDormancy(bool readingValid, uint8_t percent) {
  return readingValid && percent <= kCutoffPercent;
}

// Resume strictly above the cutoff. This one-percent hysteresis prevents a
// cell sitting exactly at 10% from booting and immediately sleeping again.
constexpr bool canResume(bool readingValid, uint8_t percent) {
  return readingValid && percent > kCutoffPercent;
}

}  // namespace BatteryProtection
