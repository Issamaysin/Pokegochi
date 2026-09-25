#pragma once

#include <cstdint>

// Home weather is derived from the world clock instead of being saved. This
// keeps every event stable across redraws and reboots without changing the
// frozen GameSave layout.
enum class HomeWeatherKind : uint8_t { Clear, Rain, Wind, Hail, Sun };

struct HomeWeatherSample {
  HomeWeatherKind kind = HomeWeatherKind::Clear;
  uint32_t eventKey = 0;
  uint16_t elapsedSeconds = 0;
  uint16_t remainingSeconds = 0;

  constexpr bool active() const { return kind != HomeWeatherKind::Clear; }
};

namespace HomeWeather {
constexpr uint32_t kWindowSeconds = 20U * 60U;
constexpr uint16_t kMinimumDurationSeconds = 3U * 60U;
constexpr uint16_t kDurationStepSeconds = 30U;
constexpr uint32_t kSecondsPerDay = 24U * 60U * 60U;
constexpr uint32_t kSunnyStartSeconds = 6U * 60U * 60U;
constexpr uint32_t kSunnyEndSeconds = 18U * 60U * 60U;

constexpr uint32_t mix(uint32_t value) {
  value ^= value >> 16U;
  value *= 0x7FEB352DU;
  value ^= value >> 15U;
  value *= 0x846CA68BU;
  value ^= value >> 16U;
  return value;
}

// One quarter of twenty-minute windows contains a three-to-four-minute
// event. The start is distributed away from the window edges so events do
// not join into an apparently permanent storm when adjacent windows match.
constexpr HomeWeatherSample sample(uint32_t worldSeconds) {
  const uint32_t window = worldSeconds / kWindowSeconds;
  const uint32_t hash = mix(window ^ 0x57454154UL);
  if ((hash & 3U) != 0U) return {};
  const uint16_t duration = static_cast<uint16_t>(
      kMinimumDurationSeconds + ((hash >> 4U) % 3U) * kDurationStepSeconds);
  const uint16_t available = static_cast<uint16_t>(kWindowSeconds - duration - 120U);
  const uint16_t start = static_cast<uint16_t>(60U + ((hash >> 9U) % available));
  const uint16_t offset = static_cast<uint16_t>(worldSeconds % kWindowSeconds);
  if (offset < start || offset >= static_cast<uint16_t>(start + duration)) return {};
  HomeWeatherSample result;
  result.kind = static_cast<HomeWeatherKind>(1U + ((hash >> 20U) % 4U));
  // Sunny Day should never appear in the middle of the night. Keep the event
  // window itself deterministic, but turn a nighttime sun roll into one of
  // the three weather types that still makes sense after dark.
  const uint32_t secondsOfDay =
      (window * kWindowSeconds + start) % kSecondsPerDay;
  if (result.kind == HomeWeatherKind::Sun &&
      (secondsOfDay < kSunnyStartSeconds ||
       secondsOfDay + duration > kSunnyEndSeconds)) {
    result.kind = static_cast<HomeWeatherKind>(1U + ((hash >> 24U) % 3U));
  }
  result.eventKey = hash ? hash : 1U;
  result.elapsedSeconds = static_cast<uint16_t>(offset - start);
  result.remainingSeconds = static_cast<uint16_t>(duration - result.elapsedSeconds);
  return result;
}
}
