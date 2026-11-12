#pragma once

#include <cstdint>

namespace BatteryProtection {

constexpr uint8_t kCutoffPercent = 10U;
constexpr uint32_t kDormantRecheckSeconds = 15U;
constexpr uint32_t kCriticalConfirmSeconds = 15U;
constexpr uint8_t kRequiredCriticalBursts = 2U;

// A radio/TFT load spike can briefly pull the ADC curve into the critical
// band even though the resting cell is healthy. Require two complete bursts
// separated by a confirmation interval; any invalid or recovered reading
// clears the pending shutdown.
constexpr uint8_t nextCriticalBurstCount(bool readingValid, uint8_t percent,
                                         uint8_t previousCount) {
  if (!readingValid || percent > kCutoffPercent) return 0U;
  return previousCount < kRequiredCriticalBursts
      ? static_cast<uint8_t>(previousCount + 1U)
      : kRequiredCriticalBursts;
}

// An invalid/floating ADC input must never make the device shut itself down.
constexpr bool shouldEnterDormancy(bool readingValid, uint8_t percent,
                                   uint8_t confirmedCriticalBursts,
                                   bool sleepInhibited = false) {
  return !sleepInhibited && readingValid && percent <= kCutoffPercent &&
         confirmedCriticalBursts >= kRequiredCriticalBursts;
}

// Resume strictly above the cutoff. This one-percent hysteresis prevents a
// cell sitting exactly at 10% from booting and immediately sleeping again.
constexpr bool canResume(bool readingValid, uint8_t percent) {
  return readingValid && percent > kCutoffPercent;
}

}  // namespace BatteryProtection
