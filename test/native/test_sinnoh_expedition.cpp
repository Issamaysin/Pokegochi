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
  assert(!SinnohExpedition::goldenBallUnlocked(progress, state));
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
  assert(SinnohExpedition::goldenBallUnlocked(progress, state));
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

  // Each badge grants exactly 20 Research once. Replaying a defeated leader
  // cannot farm points, and the shared Research counter remains capped.
  static_assert(SinnohExpedition::kGymResearchReward == 20U,
                "Each Sinnoh Gym must award 20 Research");
  SinnohExpeditionState gymRewardState{};
  gymRewardState.unlockedSites = SinnohExpedition::kAllSitesMask;
  for (uint8_t index = 0; index < SinnohExpedition::kSiteCount; ++index) {
    const SinnohSite site = static_cast<SinnohSite>(index);
    const uint16_t before = gymRewardState.research;
    assert(SinnohExpedition::recordGymVictory(gymRewardState, site));
    assert(gymRewardState.research ==
           static_cast<uint16_t>(before + SinnohExpedition::kGymResearchReward));
    assert(!SinnohExpedition::recordGymVictory(gymRewardState, site));
    assert(gymRewardState.research ==
           static_cast<uint16_t>(before + SinnohExpedition::kGymResearchReward));
  }
  assert(gymRewardState.research == 160U);

  SinnohExpeditionState cappedGymRewardState{};
  cappedGymRewardState.unlockedSites = 1U;
  cappedGymRewardState.research = 990U;
  assert(SinnohExpedition::recordGymVictory(
      cappedGymRewardState, SinnohSite::LakeVerity));
  assert(cappedGymRewardState.research == SinnohExpedition::kMaximumResearch);
  assert(!SinnohExpedition::recordGymVictory(
      cappedGymRewardState, SinnohSite::LakeVerity));
  assert(cappedGymRewardState.research == SinnohExpedition::kMaximumResearch);

  // Research alone is no longer enough to skip through Sinnoh. Every site
  // owns one Platinum leader and defeating that Gym is also required before
  // the following site can open.
  assert(!SinnohExpedition::selectOrUnlock(state,SinnohSite::EternaForest));
  state.research=999U;
  assert(!SinnohExpedition::selectOrUnlock(state,SinnohSite::EternaForest));
  constexpr const char* kExpectedLeaders[] = {
      "ROARK", "GARDENIA", "FANTINA", "MAYLENE",
      "CRASHER WAKE", "BYRON", "CANDICE", "VOLKNER"};
  constexpr const char* kExpectedBadges[] = {
      "COAL", "FOREST", "RELIC", "COBBLE",
      "FEN", "MINE", "ICICLE", "BEACON"};
  for(uint8_t index=0;index<SinnohExpedition::kSiteCount;++index) {
    const SinnohSite site=static_cast<SinnohSite>(index);
    assert(SinnohExpedition::isUnlocked(state,site));
    assert(SinnohExpedition::gymChallengeAvailable(state,site));
    const SinnohGymDefinition* gym=SinnohExpedition::gymDefinition(site);
    assert(gym&&gym->site==site&&
           std::strcmp(gym->leader,kExpectedLeaders[index])==0&&
           std::strcmp(gym->badge,kExpectedBadges[index])==0);
    for(uint8_t slot=0;slot<kOpponentTeamCapacity;++slot) {
      assert(gym->team[slot].speciesId>0U&&gym->team[slot].speciesId<=493U);
      assert(gym->team[slot].level>=96U&&gym->team[slot].level<=100U);
      for(uint8_t moveSlot=0;moveSlot<kMoveSlots;++moveSlot)
        if(gym->team[slot].moves[moveSlot]!=MoveId::None)
          assert(findFullMove(gym->team[slot].moves[moveSlot]));
    }

    BattleState gymBattle{};
    assert(SinnohExpedition::startGym(
        state,gymBattle,collection,playerUid,site,0x53470000U+index));
    assert(gymBattle.active&&gymBattle.kind==BattleKind::SinnohGym&&
           gymBattle.gymId==index&&gymBattle.gymStage==2U&&
           gymBattle.opponentCount==kOpponentTeamCapacity&&
           gymBattle.unlockedGeneration==4U);
    assert(SinnohExpedition::recordGymVictory(state,site));
    assert(!SinnohExpedition::recordGymVictory(state,site));
    assert(SinnohExpedition::hasGymVictory(state,site));
    assert(SinnohExpedition::gymVictoryCount(state)==index+1U);

    if(index+1U<SinnohExpedition::kSiteCount)
      assert(SinnohExpedition::selectOrUnlock(
          state,static_cast<SinnohSite>(index+1U)));
  }
  assert(SinnohExpedition::allSitesUnlocked(state));
  assert(SinnohExpedition::allGymsDefeated(state));
  assert(SinnohExpedition::completed(state));
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
  assert(SinnohExpedition::goldenBallUnlocked(progress, oldPlayedState));
  assert(SinnohExpedition::towerCompleted(oldPlayedState));
  assert(SinnohExpedition::bossDefeated(oldPlayedState));
  assert(!SinnohExpedition::discoveryNotificationPending(progress, oldPlayedState));

  // A player who had opened every site before site Gyms existed keeps the
  // old completion, rewards and Unova gate. The badges themselves remain
  // unclaimed, allowing all eight new battles to be played normally.
  SinnohExpeditionState oldCompletedState{};
  oldCompletedState.unlockedSites = SinnohExpedition::kAllSitesMask;
  oldCompletedState.gymVictoryMask = 0xFFU;  // legacy fossil bits
  SinnohExpedition::reconcile(oldCompletedState, progress);
  assert(SinnohExpedition::completed(oldCompletedState));
  assert(SinnohExpedition::gymVictoryCount(oldCompletedState) == 0U);
  assert(SinnohExpedition::gymChallengeAvailable(
      oldCompletedState, SinnohSite::LakeVerity));

  // Once the new schema exists, opening every site without winning the
  // badges is not completion and must not grant either final reward.
  SinnohExpeditionState gatedCompletion{};
  gatedCompletion.unlockedSites = SinnohExpedition::kAllSitesMask;
  gatedCompletion.rewardFlags = SinnohExpedition::kGymSchemaBit;
  assert(!SinnohExpedition::completed(gatedCompletion));
  Inventory gatedInventory{};
  const SinnohExpedition::CompletionReward gatedReward =
      SinnohExpedition::reconcileCompletionReward(
          gatedCompletion, gatedInventory);
  assert(!gatedReward.platinumThemeUnlocked &&
         !gatedReward.masterBallGranted);

  // A full Master Ball stack never swallows the reward: the theme unlocks
  // immediately and the ball is delivered when the player creates space.
  SinnohExpeditionState pendingState{};
  pendingState.unlockedSites = SinnohExpedition::kAllSitesMask;
  pendingState.gymVictoryMask = SinnohExpedition::kAllSitesMask;
  pendingState.rewardFlags = SinnohExpedition::kGymSchemaBit;
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
