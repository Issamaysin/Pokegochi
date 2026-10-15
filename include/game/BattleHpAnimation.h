#pragma once

#include <algorithm>
#include <cstdint>

namespace BattleHpAnimation {

// FireRed advances a virtual 48-pixel HP bar once per 60 Hz source frame.
// At 48 maximum HP or above that is one HP point per source frame. Below it,
// the original engine stores the current value as Q24.8 and advances by
// floor(maximum * 256 / 48), including its asymmetric displayed-HP rounding.
constexpr uint32_t kFireRedBarPixels = 48U;
constexpr uint32_t kEmeraldBarPixels = kFireRedBarPixels;
constexpr uint32_t kFixedPointOne = 256U;
constexpr uint32_t kMobileSpeedMultiplier = 3U;
constexpr uint32_t kEsp32SourceTicksPerFrame = 2U;

enum class BarLevel : uint8_t { Red, Yellow, Green };

inline uint32_t clampHp(uint32_t value, uint32_t maximum) {
  return std::min<uint32_t>(value, maximum);
}

inline uint32_t fixedPointStep(uint32_t maximum) {
  if (!maximum || maximum >= kFireRedBarPixels) return 0U;
  return std::max<uint32_t>(
      1U, static_cast<uint32_t>((static_cast<uint64_t>(maximum) *
                                 kFixedPointOne) /
                                kFireRedBarPixels));
}

inline uint32_t sourceFrameCount(uint32_t start, uint32_t finish,
                                 uint32_t maximum) {
  if (start == finish || maximum == 0U) return 0U;
  start = clampHp(start, maximum);
  finish = clampHp(finish, maximum);
  const uint32_t change = start > finish ? start - finish : finish - start;
  if (!change) return 0U;
  if (maximum >= kFireRedBarPixels) return change;
  const uint32_t step = fixedPointStep(maximum);
  const uint64_t distance = static_cast<uint64_t>(change) * kFixedPointOne;
  return static_cast<uint32_t>((distance + step - 1U) / step);
}

inline uint32_t displayFrameCount(uint32_t start, uint32_t finish,
                                  uint32_t maximum,
                                  uint32_t speedMultiplier =
                                      kMobileSpeedMultiplier) {
  const uint32_t sourceFrames = sourceFrameCount(start, finish, maximum);
  if (!sourceFrames) return 0U;
  speedMultiplier = std::max<uint32_t>(1U, speedMultiplier);
  return (sourceFrames + speedMultiplier - 1U) / speedMultiplier;
}

inline uint32_t sourceTickForDisplayFrame(uint32_t frame,
                                          uint32_t speedMultiplier,
                                          uint32_t sourceFrames) {
  if (!sourceFrames || !frame) return 0U;
  speedMultiplier = std::max<uint32_t>(1U, speedMultiplier);
  return std::min<uint32_t>(sourceFrames, static_cast<uint32_t>(
      std::min<uint64_t>(sourceFrames,
                         static_cast<uint64_t>(frame) * speedMultiplier)));
}

inline uint32_t fixedPointValueAtSourceTick(uint32_t start, uint32_t finish,
                                            uint32_t maximum,
                                            uint32_t sourceTick) {
  if (!maximum) return 0U;
  start = clampHp(start, maximum);
  finish = clampHp(finish, maximum);
  if (start == finish) return start * kFixedPointOne;
  const uint32_t sourceFrames = sourceFrameCount(start, finish, maximum);
  if (sourceTick >= sourceFrames) return finish * kFixedPointOne;
  if (maximum >= kFireRedBarPixels) {
    const uint32_t moved = std::min<uint32_t>(
        sourceTick, start > finish ? start - finish : finish - start);
    const uint32_t value = start > finish ? start - moved : start + moved;
    return value * kFixedPointOne;
  }
  const uint32_t step = fixedPointStep(maximum);
  const uint64_t moved = static_cast<uint64_t>(sourceTick) * step;
  const uint64_t startFixed = static_cast<uint64_t>(start) * kFixedPointOne;
  const uint64_t finishFixed = static_cast<uint64_t>(finish) * kFixedPointOne;
  if (start > finish)
    return static_cast<uint32_t>(std::max<uint64_t>(
        finishFixed, moved >= startFixed ? 0U : startFixed - moved));
  return static_cast<uint32_t>(std::min<uint64_t>(finishFixed,
                                                   startFixed + moved));
}

inline uint32_t valueAtSourceTick(uint32_t start, uint32_t finish,
                                  uint32_t maximum, uint32_t sourceTick) {
  if (!maximum) return 0U;
  start = clampHp(start, maximum);
  finish = clampHp(finish, maximum);
  if (start == finish || sourceTick >= sourceFrameCount(start, finish, maximum))
    return finish;
  if (maximum >= kFireRedBarPixels) {
    const uint32_t moved = std::min<uint32_t>(
        sourceTick, start > finish ? start - finish : finish - start);
    return start > finish ? start - moved : start + moved;
  }
  const uint32_t fixed = fixedPointValueAtSourceTick(
      start, finish, maximum, sourceTick);
  // FireRed rounds a falling bar upward and a filling bar downward until the
  // exact destination is reached. This is why a low-level HP number can hold
  // for one frame while its bar has already moved by one pixel.
  return start > finish ? (fixed + kFixedPointOne - 1U) / kFixedPointOne
                        : fixed / kFixedPointOne;
}

inline uint32_t valueAtDisplayFrame(uint32_t start, uint32_t finish,
                                    uint32_t maximum, uint32_t frame,
                                    uint32_t speedMultiplier) {
  const uint32_t sourceFrames = sourceFrameCount(start, finish, maximum);
  return valueAtSourceTick(start, finish, maximum,
      sourceTickForDisplayFrame(frame, speedMultiplier, sourceFrames));
}

inline uint8_t sourcePixelsAtSourceTick(uint32_t start, uint32_t finish,
                                        uint32_t maximum,
                                        uint32_t sourceTick) {
  if (!maximum) return 0U;
  start = clampHp(start, maximum);
  finish = clampHp(finish, maximum);
  uint32_t pixels = 0U;
  if (maximum < kFireRedBarPixels) {
    const uint32_t fixed = fixedPointValueAtSourceTick(
        start, finish, maximum, sourceTick);
    pixels = static_cast<uint32_t>(
        (static_cast<uint64_t>(fixed) * kFireRedBarPixels) /
        (static_cast<uint64_t>(maximum) * kFixedPointOne));
  } else {
    const uint32_t value = valueAtSourceTick(
        start, finish, maximum, sourceTick);
    pixels = static_cast<uint32_t>(
        (static_cast<uint64_t>(value) * kFireRedBarPixels) / maximum);
  }
  // CalcBarFilledPixels keys the one-pixel sliver to the destination value,
  // not the rounded intermediate value. Preserve that original quirk.
  if (!pixels && finish > 0U) pixels = 1U;
  return static_cast<uint8_t>(std::min<uint32_t>(kFireRedBarPixels, pixels));
}

inline uint8_t sourcePixelsForValue(uint32_t current, uint32_t maximum) {
  if (!current || !maximum) return 0U;
  current = clampHp(current, maximum);
  uint32_t pixels = static_cast<uint32_t>(
      (static_cast<uint64_t>(current) * kFireRedBarPixels) / maximum);
  if (!pixels) pixels = 1U;
  return static_cast<uint8_t>(std::min<uint32_t>(kFireRedBarPixels, pixels));
}

inline uint16_t scaleSourcePixels(uint8_t sourcePixels, uint16_t width) {
  if (!sourcePixels || !width) return 0U;
  const uint32_t scaled =
      (static_cast<uint32_t>(sourcePixels) * width +
       kFireRedBarPixels / 2U) /
      kFireRedBarPixels;
  return static_cast<uint16_t>(std::max<uint32_t>(
      1U, std::min<uint32_t>(width, scaled)));
}

inline uint16_t pixelsAtDisplayFrame(uint32_t start, uint32_t finish,
                                     uint32_t maximum, uint16_t width,
                                     uint32_t frame,
                                     uint32_t speedMultiplier) {
  const uint32_t sourceFrames = sourceFrameCount(start, finish, maximum);
  const uint32_t tick = sourceTickForDisplayFrame(
      frame, speedMultiplier, sourceFrames);
  return scaleSourcePixels(sourcePixelsAtSourceTick(
      start, finish, maximum, tick), width);
}

inline uint16_t pixelsForValue(uint32_t current, uint32_t maximum,
                               uint16_t width) {
  return scaleSourcePixels(sourcePixelsForValue(current, maximum), width);
}

inline BarLevel levelForSourcePixels(uint8_t pixels) {
  if (pixels > kFireRedBarPixels * 50U / 100U) return BarLevel::Green;
  if (pixels > kFireRedBarPixels * 20U / 100U) return BarLevel::Yellow;
  return BarLevel::Red;
}

inline BarLevel levelForValue(uint32_t current, uint32_t maximum) {
  return levelForSourcePixels(sourcePixelsForValue(current, maximum));
}

inline BarLevel levelForScaledPixels(uint16_t pixels, uint16_t width) {
  if (pixels > scaleSourcePixels(24U, width)) return BarLevel::Green;
  if (pixels > scaleSourcePixels(9U, width)) return BarLevel::Yellow;
  return BarLevel::Red;
}

inline uint32_t valueAtFrame(uint32_t start, uint32_t finish,
                             uint32_t frame, uint32_t frameCount) {
  if (!frameCount || frame >= frameCount) return finish;
  const uint32_t change = start > finish ? start - finish : finish - start;
  const uint32_t moved = static_cast<uint32_t>(
      (static_cast<uint64_t>(change) * frame) / frameCount);
  return start > finish ? start - moved : start + moved;
}

inline uint16_t pixelsAtFrame(uint16_t start, uint16_t finish,
                              uint32_t frame, uint32_t frameCount) {
  return static_cast<uint16_t>(valueAtFrame(start, finish, frame, frameCount));
}

}  // namespace BattleHpAnimation
