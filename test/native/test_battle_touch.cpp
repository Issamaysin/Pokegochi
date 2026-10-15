#include <cassert>

#include "game/BattleTouch.h"

int main() {
  using BattleTouch::RunConfirmationAction;
  using BattleTouch::runConfirmationActionAt;

  assert(runConfirmationActionAt(160, 92) == RunConfirmationAction::Accept);
  assert(runConfirmationActionAt(160, 120) == RunConfirmationAction::Cancel);

  // Every visible edge of CANCEL remains active on a slightly imprecise
  // resistive touch, with no dead row between OK and CANCEL.
  assert(runConfirmationActionAt(107, 106) == RunConfirmationAction::Cancel);
  assert(runConfirmationActionAt(212, 142) == RunConfirmationAction::Cancel);
  assert(runConfirmationActionAt(160, 105) == RunConfirmationAction::Accept);

  assert(runConfirmationActionAt(106, 120) == RunConfirmationAction::None);
  assert(runConfirmationActionAt(214, 120) == RunConfirmationAction::None);
  assert(runConfirmationActionAt(160, 145) == RunConfirmationAction::None);

  // The modal is above the lower command pane. Its retained composition must
  // therefore be selected by arena stripes as well as by a full-screen draw.
  assert(BattleTouch::runConfirmationIntersectsClip(96, 40, 224, 54));
  assert(BattleTouch::runConfirmationIntersectsClip(96, 136, 224, 146));
  assert(!BattleTouch::runConfirmationIntersectsClip(96, 0, 224, 40));
  assert(!BattleTouch::runConfirmationIntersectsClip(96, 146, 224, 160));
  assert(!BattleTouch::runConfirmationIntersectsClip(0, 40, 96, 150));
  assert(!BattleTouch::runConfirmationIntersectsClip(224, 40, 320, 150));
  return 0;
}
