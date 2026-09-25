#include <cassert>
#include <iostream>

#include "drivers/TouchFilter.h"

int main() {
  TouchInputFilter filter;

  // Finger mode remains immediate, then smooths a stable contact patch.
  FilteredTouchPoint point=filter.update(true,40,60,TouchInputMode::Finger);
  assert(point.touched&&point.x==40&&point.y==60);
  point=filter.update(true,44,56,TouchInputMode::Finger);
  assert(point.x==42&&point.y==58);
  point=filter.update(true,42,59,TouchInputMode::Finger);
  assert(point.x==42&&point.y==58);

  // A far coordinate is a new press/fast move, never an average midpoint.
  point=filter.update(true,280,120,TouchInputMode::Finger);
  assert(point.x==280&&point.y==120);
  point=filter.update(true,282,122,TouchInputMode::Finger);
  assert(point.x==280&&point.y==120);

  // Screen transitions explicitly reset history before the next Home tap.
  filter.reset();
  point=filter.update(true,31,205,TouchInputMode::Finger);
  assert(point.x==31&&point.y==205);

  // Stylus mode preserves exact coordinates and does not add spatial lag.
  point=filter.update(true,101,87,TouchInputMode::Stylus);
  assert(point.x==101&&point.y==87);
  point=filter.update(true,137,91,TouchInputMode::Stylus);
  assert(point.x==137&&point.y==91);

  // A release clears the finger history as a second new-press boundary.
  point=filter.update(false,0,0,TouchInputMode::Finger);
  assert(!point.touched);
  point=filter.update(true,300,20,TouchInputMode::Finger);
  assert(point.x==300&&point.y==20);

  std::cout << "Touch input filter tests passed.\n";
  return 0;
}
