#include <cassert>
#include "services/SwipeUnlock.h"

int main() {
  SwipeUnlock slider;
  slider.activate();
  assert(slider.active() && slider.awaitingWakeTouchRelease());

  // The same finger that woke the panel cannot begin the unlock gesture.
  assert(slider.update(true, SwipeUnlock::kStartX, SwipeUnlock::kCenterY) ==
         SwipeUnlock::Event::None);
  assert(slider.update(false, 0, 0) == SwipeUnlock::Event::None);

  // Tapping the destination does not bypass the required start point.
  assert(slider.update(true, SwipeUnlock::kFinishX, SwipeUnlock::kCenterY) ==
         SwipeUnlock::Event::None);
  slider.update(false, 0, 0);
  assert(slider.active());

  // An incomplete drag springs back when released.
  assert(slider.update(true, SwipeUnlock::kStartX, SwipeUnlock::kCenterY) ==
         SwipeUnlock::Event::None);
  assert(slider.update(true, 150, SwipeUnlock::kCenterY) ==
         SwipeUnlock::Event::PositionChanged);
  assert(slider.update(false, 0, 0) == SwipeUnlock::Event::Reset);
  assert(slider.ballX() == SwipeUnlock::kStartX);

  // Normal resistive-panel drift just outside the rail must keep progress.
  slider.update(true, SwipeUnlock::kStartX, SwipeUnlock::kCenterY);
  assert(slider.update(true, 160, SwipeUnlock::kTrackY - 22) ==
         SwipeUnlock::Event::PositionChanged);
  assert(slider.ballX() == 160);
  slider.update(false, 0, 0);

  // Leaving the vertical track cancels the gesture and requires a release.
  slider.update(true, SwipeUnlock::kStartX, SwipeUnlock::kCenterY);
  assert(slider.update(true, 170, 40) == SwipeUnlock::Event::Reset);
  assert(slider.awaitingWakeTouchRelease());
  slider.update(false, 0, 0);

  // Only one continuous, in-track drag unlocks the screen.
  slider.update(true, SwipeUnlock::kStartX, SwipeUnlock::kCenterY);
  slider.update(true, 180, SwipeUnlock::kCenterY);
  assert(slider.update(true, SwipeUnlock::kFinishX, SwipeUnlock::kCenterY) ==
         SwipeUnlock::Event::Unlocked);
  assert(!slider.active());
  return 0;
}
