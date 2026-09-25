#pragma once

#include <cstdint>
#include "game/GymSystem.h"

struct LeaguePokemon {
  uint16_t speciesId;
  uint8_t level;
};

struct LeagueMemberDefinition {
  uint8_t region;
  uint8_t stage;
  const char* trainerClass;
  const char* name;
  const char* frontAsset;
  const char* introLine1;
  const char* introLine2;
  LeaguePokemon team[kOpponentTeamCapacity];
};

class LeagueSystem {
 public:
  static constexpr uint8_t kMemberCount = 5;
  static const LeagueMemberDefinition* definition(uint8_t region, uint8_t stage);
  static bool start(BattleState& battle, PokemonCollection& collection, uint32_t playerUid,
                    const GymProgress& progress, uint32_t seed);
  static bool advance(BattleState& battle, PokemonCollection& collection);
  static bool isChampionStage(const BattleState& battle);
  // Unlocks the permanent post-Kanto Lucky Egg account bonus. Legacy saves
  // that still have it equipped are migrated without losing the reward.
  static bool reconcileKantoLuckyEggReward(const GymProgress& progress,
                                            PokemonCollection& collection,
                                            uint64_t& ownedMachines);
  static uint8_t minimumLevel(uint8_t region);
  static uint8_t maximumLevel(uint8_t region);
};
