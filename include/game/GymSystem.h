#pragma once
#include <cstdint>
#include "game/BattleEngine.h"

enum class GymId : uint8_t {
  Pewter, Cerulean, Vermilion, Celadon, Fuchsia, Saffron, Cinnabar, Viridian,
  Violet, Azalea, Goldenrod, Ecruteak, Cianwood, Olivine, Mahogany, Blackthorn,
  Rustboro, Dewford, Mauville, Lavaridge, Petalburg, Fortree, Mossdeep, Sootopolis,
  Count
};

struct GymPokemon {
  uint16_t speciesId;
  uint8_t level;
};

struct GymDefinition {
  GymId id;
  const char* leader;
  const char* badge;
  const char* type;
  const char* frontAsset;
  const char* introLine1;
  const char* introLine2;
  GymPokemon team[3];
  uint8_t teamSize;
};

struct GymProgress {
  uint32_t badgeBits = 0;
  uint8_t unlockedGeneration = 1;
  uint8_t starterClaimedBits = 0;
  uint8_t leagueChampionBits = 0;
};

class GymSystem {
 public:
  static const GymDefinition* definition(GymId id);
  static GymId next(const GymProgress& progress);
  static bool hasBadge(const GymProgress& progress, GymId id);
  static uint8_t badgeCount(const GymProgress& progress);
  static bool regionComplete(const GymProgress& progress, uint8_t generation);
  // A Gym is unlocked deterministically when the strongest selected partner
  // reaches its configured appearance level. There is no play-time or random
  // roll. requiredLevel() remains the leader's ace level for battle/reward
  // systems; unlockLevel() may intentionally introduce a challenge earlier.
  static uint8_t requiredLevel(GymId id);
  static uint8_t unlockLevel(GymId id);
  static bool challengeAvailable(const GymProgress& progress,
                                 uint8_t highestPartyLevel);
  static bool leagueAvailable(const GymProgress& progress);
  static bool leagueComplete(const GymProgress& progress, uint8_t generation);
  static bool recordLeagueVictory(GymProgress& progress, uint8_t generation);
  static bool needsRegionalStarter(const GymProgress& progress);
  static bool unlockWithStarter(GymProgress& progress, uint16_t speciesId);
  static bool start(BattleState& battle, PokemonCollection& collection, uint32_t playerUid,
                    const GymProgress& progress, GymId id, uint32_t seed);
  static bool advance(BattleState& battle, PokemonCollection& collection);
  static bool isLeaderStage(const BattleState& battle);
  static const char* opponentClass(const BattleState& battle);
  static const char* opponentName(const BattleState& battle);
  static const char* opponentAsset(const BattleState& battle);
  static const char* opponentLine1(const BattleState& battle);
  static const char* opponentLine2(const BattleState& battle);
  static bool recordVictory(GymProgress& progress, GymId id);
};
