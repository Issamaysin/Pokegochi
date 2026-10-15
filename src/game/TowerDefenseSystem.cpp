#include "game/TowerDefenseSystem.h"

#include <algorithm>

namespace TowerDefense {
namespace {

constexpr StageInfo kStages[kStageCount] = {
    {"VERDANT TRAIL", "VIRIDIAN FOREST", 0, 3, 22, 8, {10, 13, 16, 25}},
    {"MOON PASS", "MT. MOON", 2, 3, 34, 9, {41, 46, 74, 95}},
    {"CERULEAN RUN", "RIVER CITY", 3, 4, 48, 10, {54, 60, 72, 86}},
    {"TOWER PLAZA", "BATTLE FRONTIER", 21, 4, 66, 11, {81, 100, 137, 233}},
    {"PIKE CROSSING", "THREE PATHS", 44, 5, 84, 12, {109, 169, 206, 330}},
    {"FACTORY FINAL", "BATTLE FACTORY", 43, 5, 106, 13, {137, 212, 233, 376}},
};

// These routes stay deliberately compact and use the clear walkways already
// visible in the selected original-game map crops. Coordinates are local to
// the 304x134 board image.
constexpr Point kRoutes[kStageCount][5] = {
    {{18, 105}, {65, 79}, {126, 103}, {206, 55}, {287, 89}},
    {{18, 94}, {79, 50}, {148, 90}, {218, 63}, {287, 108}},
    {{17, 46}, {80, 79}, {151, 54}, {218, 96}, {288, 70}},
    {{17, 101}, {74, 60}, {145, 95}, {224, 50}, {288, 91}},
    {{17, 71}, {74, 104}, {149, 52}, {221, 99}, {287, 61}},
    {{17, 100}, {84, 61}, {153, 96}, {223, 57}, {288, 88}},
};

constexpr Point kTowerSlots[kStageCount][kTowerSlotCount] = {
    {{57, 37}, {103, 113}, {153, 58}, {204, 111}, {253, 37}},
    {{50, 40}, {99, 111}, {151, 39}, {211, 108}, {259, 43}},
    {{48, 99}, {99, 40}, {151, 112}, {207, 37}, {259, 111}},
    {{48, 41}, {98, 113}, {151, 40}, {208, 113}, {259, 42}},
    {{49, 38}, {98, 113}, {151, 39}, {207, 113}, {259, 39}},
    {{49, 40}, {98, 112}, {152, 40}, {209, 112}, {258, 41}},
};

uint16_t squaredDistance(Point first, Point second) {
  const int32_t dx = static_cast<int32_t>(first.x) - second.x;
  const int32_t dy = static_cast<int32_t>(first.y) - second.y;
  return static_cast<uint16_t>(std::min<int32_t>(65535, dx * dx + dy * dy));
}

uint16_t defenderDamage(const DefenderSnapshot& defender) {
  const uint16_t power = std::max(defender.attack, defender.specialAttack);
  return static_cast<uint16_t>(std::min<uint16_t>(255U,
      static_cast<uint16_t>(3U + power / 18U + defender.level / 3U)));
}

uint16_t defenderCooldown(const DefenderSnapshot& defender) {
  const uint16_t reduction = static_cast<uint16_t>(std::min<uint16_t>(500U, defender.speed * 3U));
  return static_cast<uint16_t>(std::max<uint16_t>(360U, 1100U - reduction));
}

uint16_t defenderRangeSquared(const DefenderSnapshot& defender) {
  const uint16_t range = static_cast<uint16_t>(58U + std::min<uint16_t>(34U, defender.speed / 4U));
  return static_cast<uint16_t>(range * range);
}

bool hasActiveEnemy(const Runtime& runtime) {
  for (const Enemy& enemy : runtime.enemies)
    if (enemy.active) return true;
  return false;
}

int8_t targetFor(const Runtime& runtime, uint8_t stage, Point tower,
                 uint16_t rangeSquared) {
  int8_t result = -1;
  uint16_t furthest = 0;
  for (uint8_t index = 0; index < kEnemyCapacity; ++index) {
    const Enemy& enemy = runtime.enemies[index];
    if (!enemy.active || squaredDistance(tower, enemyPosition(stage, enemy)) > rangeSquared)
      continue;
    if (result < 0 || enemy.progress > furthest) {
      result = static_cast<int8_t>(index);
      furthest = enemy.progress;
    }
  }
  return result;
}

bool spawnEnemy(Runtime& runtime) {
  const StageInfo& info = stageInfo(runtime.stage);
  const uint8_t count = enemiesForWave(runtime.stage, runtime.wave);
  if (runtime.spawnedInWave >= count) return false;
  for (Enemy& enemy : runtime.enemies) {
    if (enemy.active) continue;
    const uint16_t hp = static_cast<uint16_t>(info.enemyBaseHp + runtime.wave * 15U +
        (runtime.spawnedInWave % 2U) * 6U);
    enemy.active = true;
    enemy.speciesId = info.enemySpecies[(runtime.wave + runtime.spawnedInWave) % 4U];
    enemy.hp = hp;
    enemy.maximumHp = hp;
    enemy.progress = 0;
    enemy.speed = static_cast<uint8_t>(info.enemySpeed + (runtime.wave / 2U));
    ++runtime.spawnedInWave;
    return true;
  }
  return false;
}

}  // namespace

const StageInfo& stageInfo(uint8_t stage) { return kStages[normalizeStage(stage)]; }

uint8_t normalizeStage(uint8_t stage) { return stage < kStageCount ? stage : 0U; }

bool isUnlocked(const CampaignState& campaign, uint8_t stage) {
  const uint8_t normalized = normalizeStage(stage);
  return (campaign.unlockedStages & static_cast<uint8_t>(1U << normalized)) != 0U;
}

void reconcile(CampaignState& campaign) {
  if (!campaign.rngState) campaign.rngState = 0x544F5745UL;
  campaign.unlockedStages = static_cast<uint8_t>(campaign.unlockedStages | 1U);
  campaign.selectedStage = normalizeStage(campaign.selectedStage);
  if (!isUnlocked(campaign, campaign.selectedStage)) campaign.selectedStage = 0U;
  campaign.enabledDefendersMask &= 0x07U;
  for (uint8_t& stars : campaign.stageStars) stars = std::min<uint8_t>(3U, stars);
}

uint8_t starsForLives(uint8_t lives) {
  return lives >= 3U ? 3U : lives == 2U ? 2U : lives ? 1U : 0U;
}

bool recordVictory(CampaignState& campaign, const Runtime& runtime) {
  if (runtime.phase != Phase::Victory || runtime.stage >= kStageCount) return false;
  const uint8_t stars = starsForLives(runtime.lives);
  bool changed = false;
  if (stars > campaign.stageStars[runtime.stage]) {
    campaign.stageStars[runtime.stage] = stars;
    changed = true;
  }
  if (runtime.score > campaign.bestScore[runtime.stage]) {
    campaign.bestScore[runtime.stage] = runtime.score;
    changed = true;
  }
  if (runtime.score || runtime.defeated) {
    campaign.totalScore = static_cast<uint16_t>(std::min<uint32_t>(9999U,
        campaign.totalScore + runtime.score));
    campaign.totalDefeated = static_cast<uint16_t>(std::min<uint32_t>(9999U,
        campaign.totalDefeated + runtime.defeated));
    changed = true;
  }
  if (runtime.stage + 1U < kStageCount) {
    const uint8_t bit = static_cast<uint8_t>(1U << (runtime.stage + 1U));
    if ((campaign.unlockedStages & bit) == 0U) {
      campaign.unlockedStages = static_cast<uint8_t>(campaign.unlockedStages | bit);
      changed = true;
    }
  }
  return changed;
}

void reset(Runtime& runtime, uint8_t stage) {
  runtime = Runtime{};
  runtime.stage = normalizeStage(stage);
  runtime.lives = kStartingLives;
  for (TowerSlot& slot : runtime.slots) slot.defenderIndex = kNoDefender;
}

bool placeDefender(Runtime& runtime, uint8_t slot, uint8_t defenderIndex,
                   uint8_t defenderCount) {
  if (runtime.phase != Phase::Setup || slot >= kTowerSlotCount ||
      defenderIndex >= defenderCount) return false;
  for (uint8_t index = 0; index < kTowerSlotCount; ++index)
    if (index != slot && runtime.slots[index].defenderIndex == defenderIndex)
      runtime.slots[index].defenderIndex = kNoDefender;
  runtime.slots[slot].defenderIndex = defenderIndex;
  runtime.slots[slot].cooldownMs = 0;
  return true;
}

void clearDefender(Runtime& runtime, uint8_t slot) {
  if (runtime.phase != Phase::Setup || slot >= kTowerSlotCount) return;
  runtime.slots[slot] = TowerSlot{};
}

uint8_t placedDefenderCount(const Runtime& runtime) {
  uint8_t count = 0;
  for (const TowerSlot& slot : runtime.slots)
    if (slot.defenderIndex != kNoDefender) ++count;
  return count;
}

bool beginWave(Runtime& runtime, const DefenderSnapshot* defenders, uint8_t defenderCount) {
  if (runtime.phase != Phase::Setup || !defenders || !defenderCount) return false;
  bool validPlacement = false;
  for (const TowerSlot& slot : runtime.slots)
    if (slot.defenderIndex < defenderCount && defenders[slot.defenderIndex].active)
      validPlacement = true;
  if (!validPlacement) return false;
  runtime.phase = Phase::Active;
  runtime.wave = 0;
  runtime.spawnedInWave = 0;
  runtime.defeatedInWave = 0;
  runtime.spawnAccumulatorMs = 800U;
  runtime.elapsedAccumulatorMs = 0;
  runtime.shotCount = 0;
  return true;
}

void update(Runtime& runtime, const DefenderSnapshot* defenders, uint8_t defenderCount,
            uint32_t elapsedMs) {
  if (runtime.phase != Phase::Active || !defenders || !defenderCount) return;
  const uint32_t elapsed = std::min<uint32_t>(250U, elapsedMs);
  runtime.shotCount = 0;
  runtime.elapsedAccumulatorMs += elapsed;
  while (runtime.elapsedAccumulatorMs >= 1000U) {
    ++runtime.elapsedSeconds;
    runtime.elapsedAccumulatorMs -= 1000U;
  }

  runtime.spawnAccumulatorMs += elapsed;
  while (runtime.spawnAccumulatorMs >= 900U) {
    if (!spawnEnemy(runtime)) break;
    runtime.spawnAccumulatorMs -= 900U;
  }

  for (Enemy& enemy : runtime.enemies) {
    if (!enemy.active) continue;
    const uint32_t advance = elapsed * enemy.speed / 100U;
    enemy.progress = static_cast<uint16_t>(std::min<uint32_t>(1000U,
        static_cast<uint32_t>(enemy.progress) + std::max<uint32_t>(1U, advance)));
    if (enemy.progress < 1000U) continue;
    enemy.active = false;
    if (runtime.lives) --runtime.lives;
    if (!runtime.lives) {
      runtime.phase = Phase::Defeat;
      return;
    }
  }

  for (uint8_t slotIndex = 0; slotIndex < kTowerSlotCount; ++slotIndex) {
    TowerSlot& slot = runtime.slots[slotIndex];
    if (slot.defenderIndex >= defenderCount) continue;
    const DefenderSnapshot& defender = defenders[slot.defenderIndex];
    if (!defender.active) continue;
    if (slot.cooldownMs > elapsed) {
      slot.cooldownMs = static_cast<uint16_t>(slot.cooldownMs - elapsed);
      continue;
    }
    slot.cooldownMs = 0;
    const Point tower = towerPosition(runtime.stage, slotIndex);
    const int8_t target = targetFor(runtime, runtime.stage, tower, defenderRangeSquared(defender));
    if (target < 0) continue;
    Enemy& enemy = runtime.enemies[static_cast<uint8_t>(target)];
    const uint16_t damage = defenderDamage(defender);
    enemy.hp = enemy.hp > damage ? static_cast<uint16_t>(enemy.hp - damage) : 0U;
    slot.cooldownMs = defenderCooldown(defender);
    if (runtime.shotCount < kTowerSlotCount)
      runtime.shots[runtime.shotCount++] = {true, tower, enemyPosition(runtime.stage, enemy), defender.typeIndex};
    if (enemy.hp) continue;
    enemy.active = false;
    ++runtime.defeatedInWave;
    ++runtime.defeated;
    runtime.score = static_cast<uint16_t>(std::min<uint32_t>(9999U,
        runtime.score + 10U + runtime.wave * 4U));
  }

  const uint8_t waveEnemyCount = enemiesForWave(runtime.stage, runtime.wave);
  if (runtime.spawnedInWave < waveEnemyCount || hasActiveEnemy(runtime)) return;
  ++runtime.wave;
  if (runtime.wave >= stageInfo(runtime.stage).waveCount) {
    runtime.phase = Phase::Victory;
    return;
  }
  runtime.spawnedInWave = 0;
  runtime.defeatedInWave = 0;
  runtime.spawnAccumulatorMs = 900U;
}

Point routePoint(uint8_t stage, uint8_t index) {
  return kRoutes[normalizeStage(stage)][std::min<uint8_t>(4U, index)];
}

Point enemyPosition(uint8_t stage, const Enemy& enemy) {
  const uint16_t scaled = static_cast<uint16_t>(std::min<uint16_t>(999U, enemy.progress) * 4U);
  const uint8_t segment = static_cast<uint8_t>(scaled / 1000U);
  const uint16_t fraction = static_cast<uint16_t>(scaled % 1000U);
  const Point first = routePoint(stage, segment);
  const Point second = routePoint(stage, static_cast<uint8_t>(segment + 1U));
  return {static_cast<int16_t>(first.x +
              (static_cast<int32_t>(second.x - first.x) * fraction) / 1000L),
          static_cast<int16_t>(first.y +
              (static_cast<int32_t>(second.y - first.y) * fraction) / 1000L)};
}

Point towerPosition(uint8_t stage, uint8_t slot) {
  return kTowerSlots[normalizeStage(stage)][std::min<uint8_t>(kTowerSlotCount - 1U, slot)];
}

uint8_t enemiesForWave(uint8_t stage, uint8_t wave) {
  const uint8_t normalizedWave = std::min<uint8_t>(wave, stageInfo(stage).waveCount - 1U);
  return static_cast<uint8_t>(4U + normalizedWave + (normalizeStage(stage) / 2U));
}

}  // namespace TowerDefense
