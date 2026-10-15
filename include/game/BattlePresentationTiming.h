#pragma once

#include <algorithm>
#include <cstdint>

namespace BattlePresentationTiming {

constexpr uint8_t kMinimumSpeed = 1U;
constexpr uint8_t kMaximumSpeed = 3U;
constexpr uint16_t kSourceFramesPerSecond = 60U;

inline uint8_t normalizeSpeed(uint8_t speed) {
  return std::clamp<uint8_t>(speed, kMinimumSpeed, kMaximumSpeed);
}

// Durations authored at normal Pokemon speed are divided by the selected
// multiplier. Rounding upward keeps every nonzero beat visible.
inline uint32_t scaledDurationMs(uint32_t durationMs, uint8_t speed) {
  if (!durationMs) return 0U;
  const uint32_t normalized = normalizeSpeed(speed);
  return (durationMs + normalized - 1U) / normalized;
}

// The move VM always advances through every original 60 Hz source tick. On
// mobile, faster modes sample every Nth source tick so the presenter remains
// at a sustainable 60 Hz instead of attempting to enqueue 120/180 frames/s.
inline uint8_t mobileSampleStride(uint8_t speed) {
  return normalizeSpeed(speed);
}

// The caller supplies the measured sustainable physical cadence. Faster game
// modes advance proportionally more source ticks between panel samples.
inline uint8_t panelSampleStride(uint8_t speed,
                                 uint8_t sourceTicksAt1x = 4U) {
  return static_cast<uint8_t>(std::max<uint8_t>(1U, sourceTicksAt1x) *
                              normalizeSpeed(speed));
}

// Command boundaries normally collapse into the next sustainable panel
// sample at 2x/3x. Short authored impacts are different: if their spawn sits
// between two samples, coalescing it can erase a complete hit from the LCD.
// Keep those sparse discontinuities while continuous motion remains sampled.
inline bool commandBoundaryRequiresFrame(uint8_t speed, bool sampleBoundary,
                                         bool timelineStart,
                                         bool atomicReveal,
                                         bool authoredImpact) {
  return authoredImpact || normalizeSpeed(speed) == 1U || sampleBoundary ||
      timelineStart || atomicReveal;
}

inline uint32_t sourceTickDeadlineMs(uint32_t sourceTick, uint8_t speed) {
  const uint32_t denominator = static_cast<uint32_t>(kSourceFramesPerSecond) *
      normalizeSpeed(speed);
  return static_cast<uint32_t>(
      (static_cast<uint64_t>(sourceTick) * 1000ULL + denominator - 1U) /
      denominator);
}

}  // namespace BattlePresentationTiming
