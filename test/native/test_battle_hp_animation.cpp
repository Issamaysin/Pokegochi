#include <cassert>
#include <cstdint>

#include "game/BattleHpAnimation.h"

int main() {
  using namespace BattleHpAnimation;

  // FireRed's normal path advances exactly one HP point per 60 Hz source
  // frame, while 2x/3x consume two or three of those source ticks at once.
  assert(sourceFrameCount(100U, 75U, 100U) == 25U);
  assert(displayFrameCount(100U, 75U, 100U, 1U) == 25U);
  assert(displayFrameCount(100U, 75U, 100U, 2U) == 13U);
  assert(displayFrameCount(100U, 75U, 100U, 3U) == 9U);
  assert(displayFrameCount(100U, 52U, 100U,
                           kEsp32SourceTicksPerFrame) == 24U);
  assert(sourceTickForDisplayFrame(1U, 3U, 25U) == 3U);
  assert(sourceTickForDisplayFrame(9U, 3U, 25U) == 25U);
  assert(valueAtDisplayFrame(100U, 75U, 100U, 1U, 3U) == 97U);
  assert(valueAtDisplayFrame(100U, 75U, 100U, 9U, 3U) == 75U);
  assert(pixelsAtDisplayFrame(100U, 75U, 100U, 88U, 1U, 3U) == 84U);
  assert(pixelsAtDisplayFrame(100U, 75U, 100U, 88U, 9U, 3U) == 66U);

  // Below 48 max HP the original uses floor(max * 256 / 48), not a linear
  // ratio. The 20 -> 15 path therefore owns thirteen ticks, including the
  // final HP-number tick after the virtual bar has reached pixel 36.
  assert(fixedPointStep(20U) == 106U);
  assert(sourceFrameCount(20U, 15U, 20U) == 13U);
  assert(displayFrameCount(20U, 15U, 20U, 3U) == 5U);
  assert(displayFrameCount(15U, 20U, 20U, 3U) == 5U);
  assert(valueAtSourceTick(20U, 15U, 20U, 1U) == 20U);
  assert(sourcePixelsAtSourceTick(20U, 15U, 20U, 1U) == 47U);
  assert(valueAtSourceTick(20U, 15U, 20U, 12U) == 16U);
  assert(sourcePixelsAtSourceTick(20U, 15U, 20U, 12U) == 36U);
  assert(valueAtSourceTick(20U, 15U, 20U, 13U) == 15U);
  assert(sourcePixelsAtSourceTick(20U, 15U, 20U, 13U) == 36U);
  assert(valueAtDisplayFrame(20U, 15U, 20U, 1U, 3U) == 19U);
  assert(valueAtDisplayFrame(20U, 15U, 20U, 5U, 3U) == 15U);
  assert(valueAtSourceTick(15U, 20U, 20U, 1U) == 15U);
  assert(valueAtDisplayFrame(15U, 20U, 20U, 5U, 3U) == 20U);

  // Only zero HP may produce an empty static meter. The virtual 48-pixel
  // sliver scales to the wider Pokegochi healthbox without disappearing.
  assert(sourcePixelsForValue(0U, 100U) == 0U);
  assert(sourcePixelsForValue(1U, 100U) == 1U);
  assert(pixelsForValue(0U, 100U, 88U) == 0U);
  assert(pixelsForValue(1U, 100U, 88U) == 2U);
  assert(pixelsForValue(100U, 100U, 88U) == 88U);

  // FireRed chooses colour from filled virtual pixels: green above 50%,
  // yellow above 20%, red at 20% or less.
  assert(levelForSourcePixels(25U) == BarLevel::Green);
  assert(levelForSourcePixels(24U) == BarLevel::Yellow);
  assert(levelForSourcePixels(10U) == BarLevel::Yellow);
  assert(levelForSourcePixels(9U) == BarLevel::Red);
  assert(levelForValue(53U, 100U) == BarLevel::Green);
  assert(levelForValue(50U, 100U) == BarLevel::Yellow);
  assert(levelForValue(21U, 100U) == BarLevel::Yellow);
  assert(levelForValue(20U, 100U) == BarLevel::Red);
  assert(levelForScaledPixels(scaleSourcePixels(25U, 84U), 84U) ==
         BarLevel::Green);
  assert(levelForScaledPixels(scaleSourcePixels(24U, 84U), 84U) ==
         BarLevel::Yellow);
  assert(levelForScaledPixels(scaleSourcePixels(9U, 84U), 84U) ==
         BarLevel::Red);

  // Damage, healing and KO paths must be monotonic and land exactly on the
  // requested HP and width at every speed.
  for (uint32_t speed = 1U; speed <= 3U; ++speed) {
    const uint32_t frames = displayFrameCount(100U, 0U, 100U, speed);
    uint32_t previousHp = 100U;
    uint16_t previousPixels = 88U;
    for (uint32_t frame = 1U; frame <= frames; ++frame) {
      const uint32_t hp = valueAtDisplayFrame(
          100U, 0U, 100U, frame, speed);
      const uint16_t pixels = pixelsAtDisplayFrame(
          100U, 0U, 100U, 88U, frame, speed);
      assert(hp <= previousHp);
      assert(pixels <= previousPixels);
      previousHp = hp;
      previousPixels = pixels;
    }
    assert(previousHp == 0U);
    assert(previousPixels == 0U);

    const uint32_t healFrames = displayFrameCount(1U, 100U, 100U, speed);
    previousHp = 1U;
    previousPixels = pixelsForValue(1U, 100U, 88U);
    for (uint32_t frame = 1U; frame <= healFrames; ++frame) {
      const uint32_t hp = valueAtDisplayFrame(
          1U, 100U, 100U, frame, speed);
      const uint16_t pixels = pixelsAtDisplayFrame(
          1U, 100U, 100U, 88U, frame, speed);
      assert(hp >= previousHp);
      assert(pixels >= previousPixels);
      previousHp = hp;
      previousPixels = pixels;
    }
    assert(previousHp == 100U);
    assert(previousPixels == 88U);
  }

  assert(displayFrameCount(20U, 20U, 20U) == 0U);
  assert(valueAtFrame(6000U, 3000U, 1U, 3U) == 5000U);
  assert(valueAtFrame(6000U, 3000U, 3U, 3U) == 3000U);

  // Hardware regression from the four-move Dialga sequence. After Flash
  // Cannon leaves the third foe at 880/999, Dragon Claw must still publish a
  // real descending meter before the foe's action. At the CYD's 2x cadence
  // this is a 37-frame transition with many distinct visible widths, not one
  // final redraw at the end of the turn.
  constexpr uint32_t sequenceStartHp = 880U;
  constexpr uint32_t sequenceFinishHp = 734U;
  constexpr uint32_t sequenceMaximumHp = 999U;
  constexpr uint16_t sequenceMeterWidth = 84U;
  constexpr uint8_t sequenceSourceTicksPerFrame =
      2U * kEsp32SourceTicksPerFrame;
  const uint32_t sequenceFrames = displayFrameCount(
      sequenceStartHp, sequenceFinishHp, sequenceMaximumHp,
      sequenceSourceTicksPerFrame);
  assert(sequenceFrames == 37U);
  uint16_t sequencePreviousPixels = pixelsForValue(
      sequenceStartHp, sequenceMaximumHp, sequenceMeterWidth);
  uint8_t sequenceVisibleChanges = 0U;
  for (uint32_t frame = 1U; frame <= sequenceFrames; ++frame) {
    const uint16_t pixels = pixelsAtDisplayFrame(
        sequenceStartHp, sequenceFinishHp, sequenceMaximumHp,
        sequenceMeterWidth, frame, sequenceSourceTicksPerFrame);
    assert(pixels <= sequencePreviousPixels);
    if (pixels != sequencePreviousPixels) ++sequenceVisibleChanges;
    sequencePreviousPixels = pixels;
  }
  // The native 48-pixel source bar crosses seven authored pixel boundaries;
  // each is widened to the 84-pixel Pokegochi meter on presentation.
  assert(sequenceVisibleChanges >= 7U);
  assert(sequencePreviousPixels == pixelsForValue(
      sequenceFinishHp, sequenceMaximumHp, sequenceMeterWidth));
  return 0;
}
