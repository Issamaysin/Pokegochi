#pragma once
#include <cstdint>
#include "game/BattleEngine.h"

enum class GymId : uint8_t { Pewter, Cerulean, Vermilion, Celadon, Fuchsia, Saffron, Cinnabar, Viridian, Count };

struct GymPokemon {
  uint16_t speciesId;
  uint8_t level;
};

struct GymDefinition {
  GymId id;
  const char* leader;
  const char* badge;
  const char* frontAsset;
  const char* introLine1;
  const char* introLine2;
  GymPokemon team[3];
  uint8_t teamSize;
};

struct GymProgress {
  uint8_t badgeBits = 0;
};

class GymSystem {
 public:
  static const GymDefinition* definition(GymId id);
  static GymId next(const GymProgress& progress);
  static bool hasBadge(const GymProgress& progress, GymId id);
  static bool start(BattleState& battle, PokemonCollection& collection, uint32_t playerUid,
                    const GymProgress& progress, GymId id, uint32_t seed);
  static bool recordVictory(GymProgress& progress, GymId id);
};
