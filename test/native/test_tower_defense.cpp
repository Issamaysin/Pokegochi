#include "game/TowerDefenseSystem.h"

#include <cassert>

int main() {
  using namespace TowerDefense;

  CampaignState campaign{};
  reconcile(campaign);
  assert(isUnlocked(campaign, 0));
  assert(!isUnlocked(campaign, 1));
  assert(campaign.selectedStage == 0U);

  DefenderSnapshot defenders[kDefenderCount]{};
  for (uint8_t index = 0; index < kDefenderCount; ++index) {
    defenders[index].active = true;
    defenders[index].uid = index + 1U;
    defenders[index].speciesId = static_cast<uint16_t>(1U + index);
    defenders[index].attack = 220U;
    defenders[index].specialAttack = 220U;
    defenders[index].speed = 180U;
    defenders[index].level = 100U;
    defenders[index].typeIndex = index;
  }

  Runtime runtime{};
  reset(runtime, 0);
  assert(!beginWave(runtime, defenders));
  assert(placeDefender(runtime, 0, 0));
  assert(placeDefender(runtime, 2, 1));
  assert(placeDefender(runtime, 4, 2));
  // Moving a defender removes the prior placement instead of duplicating it.
  assert(placeDefender(runtime, 1, 0));
  assert(runtime.slots[0].defenderIndex == kNoDefender);
  assert(runtime.slots[1].defenderIndex == 0U);
  assert(placedDefenderCount(runtime) == 3U);
  assert(beginWave(runtime, defenders));
  for (uint16_t tick = 0; tick < 2200U && runtime.phase == Phase::Active; ++tick)
    update(runtime, defenders, kDefenderCount, 100U);
  assert(runtime.phase == Phase::Victory);
  assert(runtime.score > 0U);
  assert(recordVictory(campaign, runtime));
  assert(campaign.stageStars[0] >= 1U);
  assert(isUnlocked(campaign, 1));
  assert(campaign.bestScore[0] == runtime.score);
  assert(runtime.defeated > 0U);
  assert(campaign.totalDefeated == runtime.defeated);

  Runtime routeProbe{};
  reset(routeProbe, 4);
  Enemy enemy{};
  enemy.progress = 500U;
  const Point midpoint = enemyPosition(4, enemy);
  assert(midpoint.x > 90 && midpoint.x < 210);
  assert(midpoint.y > 35 && midpoint.y < 120);

  Runtime loss{};
  reset(loss, 5);
  assert(placeDefender(loss, 0, 0));
  defenders[0].active = false;
  assert(!beginWave(loss, defenders));
  return 0;
}
