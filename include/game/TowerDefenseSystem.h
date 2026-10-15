#pragma once

#include <cstdint>

// Poke Defense is intentionally its own compact game loop.  It reads a
// snapshot of the Party for its defenders but never changes Adventure battle
// HP, PP, status, money or encounter progression.
namespace TowerDefense {

constexpr uint8_t kStageCount = 6;
constexpr uint8_t kDefenderCount = 3;
constexpr uint8_t kTowerSlotCount = 5;
constexpr uint8_t kEnemyCapacity = 4;
constexpr uint8_t kStartingLives = 3;
constexpr uint8_t kNoDefender = 0xFF;

struct CampaignState {
  uint32_t rngState = 0x544F5745UL;  // "TOWE"
  uint16_t totalScore = 0;
  uint16_t totalDefeated = 0;
  uint16_t bestScore[kStageCount]{};
  uint8_t stageStars[kStageCount]{};
  // A bit per arena. The Verdant Trail opens on a fresh save.
  uint8_t unlockedStages = 1U;
  uint8_t selectedStage = 0;
  // Three active-Party positions are the available defenders. The player can
  // bench a member for a lighter, self-imposed challenge.
  uint8_t enabledDefendersMask = 0x07U;
  uint8_t reserved[3]{};
};
static_assert(sizeof(CampaignState) == 32,
              "Tower-defense campaign save layout changed");

enum class Phase : uint8_t { Setup, Active, Victory, Defeat };

struct Point {
  int16_t x = 0;
  int16_t y = 0;
};

struct StageInfo {
  const char* name;
  const char* tagline;
  uint8_t backgroundId;
  uint8_t waveCount;
  uint16_t enemyBaseHp;
  uint8_t enemySpeed;
  uint16_t enemySpecies[4];
};

struct DefenderSnapshot {
  bool active = false;
  uint32_t uid = 0;
  uint16_t speciesId = 0;
  uint16_t attack = 1;
  uint16_t specialAttack = 1;
  uint16_t speed = 1;
  uint8_t level = 1;
  uint8_t typeIndex = 0;
};

struct Enemy {
  bool active = false;
  uint16_t speciesId = 0;
  uint16_t hp = 0;
  uint16_t maximumHp = 0;
  // 0..1000 along the authored, visible route.
  uint16_t progress = 0;
  uint8_t speed = 0;
};

struct TowerSlot {
  uint8_t defenderIndex = kNoDefender;
  uint16_t cooldownMs = 0;
};

struct Shot {
  bool active = false;
  Point from{};
  Point to{};
  uint8_t typeIndex = 0;
};

struct Runtime {
  Phase phase = Phase::Setup;
  uint8_t stage = 0;
  uint8_t wave = 0;
  uint8_t spawnedInWave = 0;
  uint8_t defeatedInWave = 0;
  uint8_t lives = kStartingLives;
  uint8_t selectedDefender = 0;
  uint16_t score = 0;
  uint16_t defeated = 0;
  uint16_t elapsedSeconds = 0;
  uint32_t spawnAccumulatorMs = 0;
  uint32_t elapsedAccumulatorMs = 0;
  TowerSlot slots[kTowerSlotCount]{};
  Enemy enemies[kEnemyCapacity]{};
  Shot shots[kTowerSlotCount]{};
  uint8_t shotCount = 0;
};

const StageInfo& stageInfo(uint8_t stage);
uint8_t normalizeStage(uint8_t stage);
bool isUnlocked(const CampaignState& campaign, uint8_t stage);
void reconcile(CampaignState& campaign);
uint8_t starsForLives(uint8_t lives);
bool recordVictory(CampaignState& campaign, const Runtime& runtime);

void reset(Runtime& runtime, uint8_t stage);
bool placeDefender(Runtime& runtime, uint8_t slot, uint8_t defenderIndex,
                   uint8_t defenderCount = kDefenderCount);
void clearDefender(Runtime& runtime, uint8_t slot);
uint8_t placedDefenderCount(const Runtime& runtime);
bool beginWave(Runtime& runtime, const DefenderSnapshot* defenders,
               uint8_t defenderCount = kDefenderCount);
void update(Runtime& runtime, const DefenderSnapshot* defenders, uint8_t defenderCount,
            uint32_t elapsedMs);

Point routePoint(uint8_t stage, uint8_t index);
Point enemyPosition(uint8_t stage, const Enemy& enemy);
Point towerPosition(uint8_t stage, uint8_t slot);
uint8_t enemiesForWave(uint8_t stage, uint8_t wave);

}  // namespace TowerDefense
