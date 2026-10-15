#include <cassert>
#include <cstring>

#include "game/MegaChallengeSystem.h"
#include "game/Economy.h"
#include "game/Pokedex.h"
#include "game/SinnohExpedition.h"

int main() {
  PokemonCollection collection{};
  CollectionLogic::initialize(collection);
  assert(CollectionLogic::chooseStarter(collection, 1U, 1U));
  const uint32_t playerUid=collection.party[0];

  GymProgress progress{};
  SinnohExpeditionState state{};
  assert(!SinnohExpedition::available(progress, state));
  progress.starterClaimedBits=MegaChallengeSystem::kCompletionBit;
  SinnohExpedition::reconcile(state, progress);
  assert(!SinnohExpedition::available(progress, state));
  assert(!SinnohExpedition::isUnlocked(state, SinnohSite::LakeVerity));
  assert(!SinnohExpedition::discoveryNotificationPending(progress, state));

  // Mega completion opens Tower and Boss, but Sinnoh requires one complete
  // three-opponent Tower clear and one Boss victory, in either order.
  assert(SinnohExpedition::recordTowerCompletion(state));
  assert(!SinnohExpedition::recordTowerCompletion(state));
  assert(SinnohExpedition::towerCompleted(state));
  assert(!SinnohExpedition::bossDefeated(state));
  assert(!SinnohExpedition::available(progress, state));
  assert(SinnohExpedition::recordBossVictory(state));
  assert(!SinnohExpedition::recordBossVictory(state));
  assert(SinnohExpedition::available(progress, state));
  assert(SinnohExpedition::discoveryNotificationPending(progress, state));

  SinnohExpeditionState reverseOrderState{};
  SinnohExpedition::reconcile(reverseOrderState, progress);
  assert(SinnohExpedition::recordBossVictory(reverseOrderState));
  assert(!SinnohExpedition::available(progress, reverseOrderState));
  assert(SinnohExpedition::recordTowerCompletion(reverseOrderState));
  assert(SinnohExpedition::available(progress, reverseOrderState));

  // The Home DEX badge survives ordinary reconciliation and disappears only
  // after the player explicitly opens the Sinnoh node once.
  SinnohExpedition::reconcile(state,progress);
  assert(SinnohExpedition::isUnlocked(state,SinnohSite::LakeVerity));
  assert(SinnohExpedition::discoveryNotificationPending(progress, state));
  assert(SinnohExpedition::acknowledgeDiscovery(state));
  assert(!SinnohExpedition::acknowledgeDiscovery(state));
  assert(!SinnohExpedition::discoveryNotificationPending(progress, state));
  assert(std::strcmp(SinnohExpedition::siteName(SinnohSite::LakeVerity),"LAKE VERITY")==0);
  assert(SinnohExpedition::chooseEncounterSpecies(state,SinnohSite::LakeVerity,10U,4U)>=387U);
  assert(SinnohExpedition::chooseEncounterSpecies(state,SinnohSite::LakeVerity,10U,4U)<=419U);
  uint16_t visibleSpecies[32]{};
  const uint8_t visibleCount = SinnohExpedition::availableSpecies(
      SinnohSite::LakeVerity, visibleSpecies, 32U);
  assert(visibleCount == 21U && visibleSpecies[0] == 387U &&
         visibleSpecies[visibleCount - 1U] == 480U);
  assert(SinnohExpedition::availableSpecies(
      SinnohSite::DistortionRift, visibleSpecies, 32U) == 0U);

  // Research unlocks one authored location at a time and never opens the
  // postgame rift merely from selecting its card.
  assert(!SinnohExpedition::selectOrUnlock(state,SinnohSite::EternaForest));
  state.research=999U;
  for(uint8_t index=1;index<SinnohExpedition::kSiteCount;++index)
    assert(SinnohExpedition::selectOrUnlock(state,static_cast<SinnohSite>(index)));
  assert(SinnohExpedition::allSitesUnlocked(state));
  Inventory completionInventory{};
  const SinnohExpedition::CompletionReward completionReward =
      SinnohExpedition::reconcileCompletionReward(state, completionInventory);
  assert(completionReward.platinumThemeUnlocked &&
         completionReward.masterBallGranted &&
         SinnohExpedition::platinumThemeUnlocked(state));
  assert(completionInventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] == 1U);
  const SinnohExpedition::CompletionReward duplicateReward =
      SinnohExpedition::reconcileCompletionReward(state, completionInventory);
  assert(!duplicateReward.platinumThemeUnlocked && !duplicateReward.masterBallGranted &&
         completionInventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] == 1U);

  // Old builds opened Lake Verity automatically after Mega. Merely carrying
  // that bit must not bypass the new Tower+Boss gate. Conversely, saves with
  // durable evidence of having actually played Sinnoh stay unlocked and do
  // not receive a stale discovery badge after migration.
  SinnohExpeditionState oldAutoUnlockedState{};
  oldAutoUnlockedState.unlockedSites = 1U;
  SinnohExpedition::reconcile(oldAutoUnlockedState, progress);
  assert(!SinnohExpedition::available(progress, oldAutoUnlockedState));
  assert(!SinnohExpedition::discoveryNotificationPending(
      progress, oldAutoUnlockedState));

  SinnohExpeditionState oldPlayedState{};
  oldPlayedState.unlockedSites = 1U;
  oldPlayedState.battles = 1U;
  SinnohExpedition::reconcile(oldPlayedState, progress);
  assert(SinnohExpedition::available(progress, oldPlayedState));
  assert(SinnohExpedition::towerCompleted(oldPlayedState));
  assert(SinnohExpedition::bossDefeated(oldPlayedState));
  assert(!SinnohExpedition::discoveryNotificationPending(progress, oldPlayedState));

  // A full Master Ball stack never swallows the reward: the theme unlocks
  // immediately and the ball is delivered when the player creates space.
  SinnohExpeditionState pendingState{};
  pendingState.unlockedSites = SinnohExpedition::kAllSitesMask;
  Inventory fullInventory{};
  fullInventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] =
      kInventoryStackLimit;
  const SinnohExpedition::CompletionReward pendingReward =
      SinnohExpedition::reconcileCompletionReward(pendingState, fullInventory);
  assert(pendingReward.platinumThemeUnlocked && !pendingReward.masterBallGranted &&
         SinnohExpedition::platinumThemeUnlocked(pendingState));
  fullInventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] = 0U;
  const SinnohExpedition::CompletionReward deferredReward =
      SinnohExpedition::reconcileCompletionReward(pendingState, fullInventory);
  assert(!deferredReward.platinumThemeUnlocked && deferredReward.masterBallGranted &&
         fullInventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] == 1U);
  const uint16_t rift=SinnohExpedition::chooseEncounterSpecies(
      state,SinnohSite::DistortionRift,5U,0x1234U);
  assert(rift>=387U&&rift<=493U);
  // Unlocking the Rift consumes research, but must not leave its table stuck
  // on the first entry. The complete table must include both common and
  // legendary results even at a low expedition-team level.
  bool sawCommonRiftSpecies = false;
  bool sawLegendaryRiftSpecies = false;
  for (uint32_t seed = 1U; seed < 512U; ++seed) {
    const uint16_t candidate = SinnohExpedition::chooseEncounterSpecies(
        state, SinnohSite::DistortionRift, 5U, seed);
    if (candidate < 479U) sawCommonRiftSpecies = true;
    if (candidate >= 480U) sawLegendaryRiftSpecies = true;
  }
  assert(sawCommonRiftSpecies&&sawLegendaryRiftSpecies);

  // Rare visitors in the ordinary zones are not hidden behind their authored
  // level. They remain weight-1 encounters, but are possible from the first
  // visit just like a rare wild roll.
  for (uint8_t index = 0; index < SinnohExpedition::kSiteCount - 1U; ++index) {
    bool sawRareVisitor = false;
    for (uint32_t seed = 1U; seed < 4096U; ++seed) {
      const uint16_t candidate = SinnohExpedition::chooseEncounterSpecies(
          state, static_cast<SinnohSite>(index), 5U, seed);
      if (candidate >= 479U) {
        sawRareVisitor = true;
        break;
      }
    }
    assert(sawRareVisitor);
  }

  // Every Sinnoh site, including the full Rift, shares the normal-wild
  // legendary baseline instead of scaling rarity with its roster size.
  for (uint8_t index = 0; index < SinnohExpedition::kSiteCount; ++index) {
    uint16_t legendaryCount = 0;
    for (uint32_t seed = 1U; seed <= 4095U; ++seed) {
      const uint16_t candidate = SinnohExpedition::chooseEncounterSpecies(
          state, static_cast<SinnohSite>(index), 90U, seed);
      if (PokedexLogic::isLegendaryOrMythical(candidate)) ++legendaryCount;
    }
    // 4095 / 265 is 15.45; keep a generous deterministic band around the
    // target while still catching a site-specific 14/278-style regression.
    assert(legendaryCount >= 8U && legendaryCount <= 24U);
  }

  const uint16_t beforeResearch = state.research;
  assert(SinnohExpedition::recordBattle(state, false) == 0U);
  assert(state.research == beforeResearch);
  assert(SinnohExpedition::recordBattle(state, true) == 1U);
  assert(state.research == beforeResearch + 1U);

  // A Sinnoh encounter starts as a normal, capturable Wild battle. It does
  // not repurpose the Tower or Boss battle kinds.
  BattleState battle{};
  assert(SinnohExpedition::startEncounter(state,battle,collection,playerUid,
      SinnohSite::LakeVerity,0xBEEF1234U));
  assert(battle.active&&battle.kind==BattleKind::Wild&&battle.opponentCount==1U);
  assert(battle.opponents[0].speciesId>=387U&&battle.opponents[0].speciesId<=419U);

  // Even with a level-100 expedition team, Sinnoh wild Pokemon stay at or
  // below the authored level cap. This value is intentional: the post-game
  // region may reach 95, but never produces a level-96+ wild encounter.
  static_assert(SinnohExpedition::kMaxEncounterLevel == 95U,
                "Sinnoh encounter cap must remain at level 95");
  CollectionLogic::active(collection, 0)->level = 100U;
  for (uint32_t seed = 1U; seed <= 32U; ++seed) {
    BattleState cappedBattle{};
    assert(SinnohExpedition::startEncounter(state, cappedBattle, collection,
        playerUid, SinnohSite::LakeVerity, seed));
    assert(cappedBattle.opponents[0].level <=
           SinnohExpedition::kMaxEncounterLevel);
  }
  return 0;
}
