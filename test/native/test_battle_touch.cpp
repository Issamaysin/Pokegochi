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
  return 0;
}
