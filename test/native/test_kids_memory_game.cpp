#include <cassert>
#include <cstdint>

#include "game/KidsMemoryGame.h"

int main() {
  using namespace KidsMemoryGame;
  const uint8_t cards[12] = {0, 0, 1, 2, 3, 3, 4, 4, 5, 5, 6, 6};
  State state{};

  assert(select(cards, 12, 0, state) == SelectionResult::Revealed);
  assert(state.attempts == 1 && state.first == 0);

  // The already-visible first card is not another attempt.
  assert(select(cards, 12, 0, state) == SelectionResult::Ignored);
  assert(state.attempts == 1 && state.first == 0);

  assert(select(cards, 12, 1, state) == SelectionResult::Matched);
  assert(state.attempts == 2 && state.first < 0 && state.second < 0);
  assert((state.matched & 0x0003U) == 0x0003U);

  // Completed pairs and invalid positions never affect the score.
  assert(select(cards, 12, 1, state) == SelectionResult::Ignored);
  assert(select(cards, 12, 12, state) == SelectionResult::Ignored);
  assert(state.attempts == 2);

  assert(select(cards, 12, 2, state) == SelectionResult::Revealed);
  assert(select(cards, 12, 3, state) == SelectionResult::Revealed);
  assert(state.attempts == 4 && state.first == 2 && state.second == 3);

  // Touching either card from a visible mismatch only closes that mismatch.
  assert(select(cards, 12, 3, state) == SelectionResult::ClearedMismatch);
  assert(state.attempts == 4 && state.first < 0 && state.second < 0);

  assert(select(cards, 12, 4, state) == SelectionResult::Revealed);
  assert(state.attempts == 5 && state.first == 4);

  // A third, face-down card both clears the old mismatch and counts exactly
  // one new attempt because that third card really becomes visible.
  State thirdCard{};
  assert(select(cards, 12, 2, thirdCard) == SelectionResult::Revealed);
  assert(select(cards, 12, 3, thirdCard) == SelectionResult::Revealed);
  assert(select(cards, 12, 4, thirdCard) == SelectionResult::Revealed);
  assert(thirdCard.attempts == 3 && thirdCard.first == 4 && thirdCard.second < 0);

  thirdCard.attempts = UINT16_MAX;
  assert(select(cards, 12, 5, thirdCard) == SelectionResult::Matched);
  assert(thirdCard.attempts == UINT16_MAX);
  return 0;
}
