#pragma once

namespace WildAutoBattlePolicy {

// The runtime toggle is intentionally narrower than BattleKind itself.  The
// UI supplies these already-resolved facts so this policy stays portable to
// native tests and cannot accidentally make trainer/PvP battles automatic.
inline bool mayRun(bool wildBattle, bool opponentShiny,
                   bool opponentLegendary, bool playerAlive,
                   bool hasRemainingPp) {
  return wildBattle && !opponentShiny && !opponentLegendary &&
         playerAlive && hasRemainingPp;
}

}  // namespace WildAutoBattlePolicy
