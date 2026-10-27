#pragma once

#include "game/UnovaAdventure.h"

class UnovaTournament {
 public:
  static bool active(const UnovaAdventureState& state);
  // Completion unlocks Royal Unova but does not retire this replayable cup.
  // UI and entry flow must use this same prerequisite check.
  static bool championsUnlocked(const UnovaAdventureState& state);
  static bool beginPwt(UnovaAdventureState& state, uint8_t tier, uint32_t seed,
                       uint8_t season = 0U);
  static bool beginChampions(UnovaAdventureState& state, uint32_t seed,
                             uint8_t season = 0U);
  static bool beginRoyal(UnovaAdventureState& state, uint32_t localDay,
                         uint32_t seed, uint8_t season = 0U);
  static bool abandon(UnovaAdventureState& state);
  static const char* tierName(uint8_t tier);
  static const char* opponentName(const UnovaAdventureState& state);
  static bool startBattle(UnovaAdventureState& state, BattleState& battle,
                          PokemonCollection& collection, uint32_t playerUid);
  // Returns true when state changed. `completedChallenge` is set only after
  // the final required trainer was defeated.
  static bool recordBattle(UnovaAdventureState& state, BattleOutcome outcome,
                           bool& completedChallenge);
};
