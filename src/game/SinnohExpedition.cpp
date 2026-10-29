#include "game/SinnohExpedition.h"

#include <algorithm>

#include "game/Economy.h"
#include "game/MegaChallengeSystem.h"

namespace {
struct EncounterEntry { uint16_t species; uint8_t level; uint8_t weight; };
struct SiteData {
  const char* name;
  const char* tagline;
  uint16_t unlockCost;
  const EncounterEntry* encounters;
  uint8_t count;
};

constexpr EncounterEntry kLakeVerity[] = {
  {387,5,12},{388,18,5},{389,32,2},{390,5,12},{391,14,5},{392,36,2},
  {393,5,12},{394,16,5},{395,36,2},{396,4,15},{397,14,6},{398,34,2},
  {399,4,15},{400,15,6},{403,4,15},{404,15,6},{405,30,2},{418,5,13},{419,26,3},
  {479,25,1},{480,50,1},
};
constexpr EncounterEntry kEternaForest[] = {
  {401,5,13},{402,10,5},{406,5,8},{407,28,2},{412,5,11},{413,20,3},{414,20,3},
  {415,5,10},{416,21,2},{420,5,10},{421,25,3},{427,5,12},{428,24,3},{430,30,2},
  {479,25,1},{481,50,1},{492,60,1},
};
constexpr EncounterEntry kOreburghMine[] = {
  {408,20,9},{409,30,2},{410,20,9},{411,30,2},{436,12,10},{437,33,3},
  {438,10,10},{439,10,10},{443,16,12},{444,24,5},{445,48,2},{446,8,10},
  {447,5,11},{448,30,3},{449,18,10},{450,34,3},
  {479,25,1},{482,50,1},{483,55,1},
};
constexpr EncounterEntry kGreatMarsh[] = {
  {431,10,11},{432,38,3},{433,10,10},{434,15,9},{435,34,2},{440,10,9},
  {451,18,11},{452,40,3},{453,16,11},{454,37,3},{455,20,8},{456,18,10},{457,31,3},
  {479,25,1},{488,50,1},{489,50,1},{490,50,1},
};
constexpr EncounterEntry kMtCoronet[] = {
  {441,10,8},{442,20,4},{446,8,9},{459,20,10},{460,40,3},{461,28,8},{462,38,3},
  {463,34,3},{464,42,2},{465,38,2},{466,40,2},{467,40,2},{468,38,2},{469,33,3},
  {470,30,3},{471,30,3},{472,33,3},{473,34,3},{474,38,2},{475,30,3},{476,35,2},{477,37,2},
  {479,25,1},{484,55,1},{487,60,1},
};
constexpr EncounterEntry kFuegoIron[] = {
  {422,12,9},{423,30,3},{436,16,10},{437,33,3},{449,18,10},{450,34,3},{451,18,10},
  {452,40,3},{453,16,10},{454,37,3},{458,20,9},{459,20,9},{460,40,3},{477,37,3},{478,38,3},
  {479,25,1},{485,55,1},
};
constexpr EncounterEntry kSnowpoint[] = {
  {458,20,10},{459,20,11},{460,40,4},{461,28,11},{462,38,4},{471,30,4},{472,33,3},
  {473,34,3},{478,38,4},{488,50,1},{489,50,1},{490,50,1},
  {479,25,1},{486,60,1},
};
constexpr EncounterEntry kDistortionRift[] = {
  // The Rift is the complete Sinnoh wild table. Common species have normal
  // encounter weight, Rotom is uncommon, and every legendary/mythical entry
  // is weight 1. Its authored levels remain for the encounter data, but the
  // Rift itself does not level-gate species.
  {387,5,4},{388,18,3},{389,32,2},{390,5,4},{391,14,3},{392,36,2},
  {393,5,4},{394,16,3},{395,36,2},{396,4,5},{397,14,3},{398,34,2},
  {399,4,5},{400,15,3},{401,5,4},{402,10,3},{403,4,4},{404,15,3},
  {405,30,2},{406,5,4},{407,28,2},{408,20,4},{409,30,2},{410,20,4},
  {411,30,2},{412,5,4},{413,20,2},{414,20,2},{415,5,4},{416,21,2},
  {417,5,4},{418,5,4},{419,26,2},{420,5,4},{421,25,2},{422,12,4},
  {423,30,2},{424,20,2},{425,15,4},{426,28,2},{427,5,4},{428,24,2},
  {429,30,2},{430,30,2},{431,10,4},{432,38,2},{433,10,4},{434,15,2},
  {435,34,2},{436,12,4},{437,33,2},{438,10,4},{439,10,4},{440,10,4},
  {441,10,4},{442,20,2},{443,16,4},{444,24,2},{445,48,1},{446,8,3},
  {447,5,4},{448,30,2},{449,18,4},{450,34,2},{451,18,4},{452,40,2},
  {453,16,4},{454,37,2},{455,20,3},{456,18,4},{457,31,2},{458,20,4},
  {459,20,4},{460,40,2},{461,28,3},{462,38,2},{463,34,2},{464,42,1},
  {465,38,2},{466,40,2},{467,40,2},{468,38,2},{469,33,2},{470,30,3},
  {471,30,3},{472,33,2},{473,34,2},{474,38,1},{475,30,3},{476,35,2},
  {477,37,2},{478,38,2},{479,25,2},
  {480,50,1},{481,50,1},{482,50,1},{483,55,1},{484,55,1},{485,55,1},
  {486,55,1},{487,60,1},{488,50,1},{489,50,1},{490,50,1},{491,60,1},
  {492,60,1},{493,70,1},
};
constexpr SiteData kSites[] = {
  {"LAKE VERITY", "STARTER SHORES", 0, kLakeVerity, sizeof(kLakeVerity)/sizeof(kLakeVerity[0])},
  {"ETERNA FOREST", "LIVING CANOPY", 20, kEternaForest, sizeof(kEternaForest)/sizeof(kEternaForest[0])},
  {"OREBURGH MINE", "FOSSIL STRATA", 45, kOreburghMine, sizeof(kOreburghMine)/sizeof(kOreburghMine[0])},
  {"GREAT MARSH", "MISTY WETLAND", 75, kGreatMarsh, sizeof(kGreatMarsh)/sizeof(kGreatMarsh[0])},
  {"MT. CORONET", "ANCIENT SUMMIT", 115, kMtCoronet, sizeof(kMtCoronet)/sizeof(kMtCoronet[0])},
  {"FUEGO / IRON", "VOLCANIC RELICS", 160, kFuegoIron, sizeof(kFuegoIron)/sizeof(kFuegoIron[0])},
  {"SNOWPOINT", "FROZEN RUINS", 210, kSnowpoint, sizeof(kSnowpoint)/sizeof(kSnowpoint[0])},
  {"DISTORTION RIFT", "MYTHIC ECHOES", 270, kDistortionRift, sizeof(kDistortionRift)/sizeof(kDistortionRift[0])},
};

constexpr MoveId move(uint16_t id) {
  return static_cast<MoveId>(id);
}

// Species and moves are the authored Pokemon Platinum leader teams. Levels
// continue Pokegochi's single-save postgame curve instead of resetting to
// Platinum's early-game values; the three-Pokemon cap matches every existing
// Pokegochi Gym and retains each leader's signature ace.
constexpr SinnohGymDefinition kSinnohGyms[] = {
  {SinnohSite::LakeVerity, "ROARK", "COAL", "ROCK", "sinnoh_roark",
   "WELCOME! I'M ROARK.", "SHOW ME YOUR ROCK-SOLID SPIRIT!",
   {{74,96,{move(446),move(88),MoveId::None,MoveId::None},HeldItem::None},
    {95,96,{move(446),move(88),move(103),MoveId::None},HeldItem::None},
    {408,97,{move(29),move(228),move(43),MoveId::None},HeldItem::None}}},
  {SinnohSite::EternaForest, "GARDENIA", "FOREST", "GRASS", "sinnoh_gardenia",
   "YOU KEPT ME WAITING!", "LET'S SEE YOUR WAY WITH POKEMON!",
   {{387,96,{move(447),move(75),move(241),move(115)},HeldItem::None},
    {421,97,{move(447),move(73),move(345),move(219)},HeldItem::None},
    {407,97,{move(447),move(345),move(40),move(78)},HeldItem::SitrusBerry}}},
  {SinnohSite::OreburghMine, "FANTINA", "RELIC", "GHOST", "sinnoh_fantina",
   "FINALLY, YOU HAVE ARRIVED!", "EN GARDE! LET US BATTLE!",
   {{355,97,{move(261),move(228),move(425),move(248)},HeldItem::None},
    {93,97,{move(421),move(389),move(95),move(109)},HeldItem::None},
    {429,98,{move(247),move(60),move(345),move(109)},HeldItem::SitrusBerry}}},
  {SinnohSite::GreatMarsh, "MAYLENE", "COBBLE", "FIGHTING", "sinnoh_maylene",
   "I TAKE BATTLES VERY SERIOUSLY.", "I WILL NOT HOLD BACK!",
   {{307,97,{move(409),move(93),move(317),move(252)},HeldItem::None},
    {67,98,{move(2),move(317),move(70),move(116)},HeldItem::None},
    {448,98,{move(409),move(395),move(232),move(198)},HeldItem::SitrusBerry}}},
  {SinnohSite::MtCoronet, "CRASHER WAKE", "FEN", "WATER", "sinnoh_wake",
   "MASKED ROYALTY HAS ARRIVED!", "LET THE CRASHER WAKE WAVE BEGIN!",
   {{130,98,{move(127),move(362),move(44),move(239)},HeldItem::None},
    {195,98,{move(341),move(317),move(352),move(281)},HeldItem::None},
    {419,99,{move(362),move(423),move(242),move(453)},HeldItem::SitrusBerry}}},
  {SinnohSite::FuegoIron, "BYRON", "MINE", "STEEL", "sinnoh_byron",
   "THAT'S OREBURGH'S BADGE!", "LET'S BATTLE FOR THE MINE BADGE!",
   {{82,98,{move(430),move(85),move(161),move(319)},HeldItem::None},
    {208,99,{move(430),move(423),move(89),move(201)},HeldItem::None},
    {411,99,{move(368),move(444),move(334),move(269)},HeldItem::SitrusBerry}}},
  {SinnohSite::Snowpoint, "CANDICE", "ICICLE", "ICE", "sinnoh_candice",
   "ARE YOU READY TO FREEZE?", "I'LL SHOW YOU MY FOCUS!",
   {{221,99,{move(419),move(444),move(89),move(258)},HeldItem::None},
    {460,99,{move(419),move(452),move(352),move(411)},HeldItem::None},
    {478,100,{move(59),move(104),move(247),move(94)},HeldItem::SitrusBerry}}},
  {SinnohSite::DistortionRift, "VOLKNER", "BEACON", "ELECTRIC", "sinnoh_volkner",
   "I'VE BEEN WAITING FOR A CHALLENGER.", "GIVE ME A BATTLE THAT ELECTRIFIES!",
   {{26,99,{move(451),move(411),move(324),move(98)},HeldItem::None},
    {405,100,{move(423),move(422),move(242),move(424)},HeldItem::None},
    {466,100,{move(9),move(7),move(98),move(416)},HeldItem::SitrusBerry}}},
};
static_assert(sizeof(kSinnohGyms) / sizeof(kSinnohGyms[0]) ==
                  SinnohExpedition::kSiteCount,
              "Every Sinnoh site must own exactly one Gym");

uint32_t nextRandom(uint32_t& state) {
  if (!state) state = 0x53494E4EU;
  state ^= state << 13U;
  state ^= state >> 17U;
  state ^= state << 5U;
  return state;
}

uint8_t siteIndex(SinnohSite site) {
  const uint8_t index = static_cast<uint8_t>(site);
  return index < SinnohExpedition::kSiteCount ? index : 0U;
}

bool isSinnohLegendary(uint16_t species) {
  return species >= 480U && species <= 493U;
}

// The retired selected-site byte used only values 0..7. A high-bit schema
// marker therefore separates the new ledger from every save produced by an
// older firmware without increasing the frozen V44 payload.
constexpr uint8_t kProgressSchemaMask = 0xF8U;
constexpr uint8_t kProgressSchema = 0xA0U;
constexpr uint8_t kTowerCompletedBit = 1U << 0U;
constexpr uint8_t kBossDefeatedBit = 1U << 1U;
constexpr uint8_t kDiscoveryAcknowledgedBit = 1U << 2U;
constexpr uint8_t kProgressBits = kTowerCompletedBit | kBossDefeatedBit |
    kDiscoveryAcknowledgedBit;

bool hasProgressSchema(const SinnohExpeditionState& state) {
  return (state.selectedSite & kProgressSchemaMask) == kProgressSchema;
}

uint8_t progressFlags(const SinnohExpeditionState& state) {
  return hasProgressSchema(state)
      ? static_cast<uint8_t>(state.selectedSite & kProgressBits) : 0U;
}

bool legacySinnohWasPlayed(const SinnohExpeditionState& state) {
  // Site zero was opened automatically by the former post-Mega gate, so it
  // alone is not evidence that the player entered Sinnoh. Research, battles,
  // a later site or either completion reward is durable evidence and is
  // grandfathered to avoid relocking an already-played region after update.
  return state.research != 0U || state.battles != 0U ||
      (state.unlockedSites & static_cast<uint8_t>(~1U)) != 0U ||
      state.rewardFlags != 0U;
}

void ensureProgressSchema(SinnohExpeditionState& state) {
  if (hasProgressSchema(state)) return;
  const uint8_t migrated = legacySinnohWasPlayed(state) ? kProgressBits : 0U;
  state.selectedSite = static_cast<uint8_t>(kProgressSchema | migrated);
}

bool setProgressBit(SinnohExpeditionState& state, uint8_t bit) {
  ensureProgressSchema(state);
  if ((state.selectedSite & bit) != 0U) return false;
  state.selectedSite = static_cast<uint8_t>(state.selectedSite | bit);
  return true;
}
}

bool SinnohExpedition::available(const GymProgress& progress,
                                 const SinnohExpeditionState& state) {
  const uint8_t flags = progressFlags(state);
  return MegaChallengeSystem::completed(progress) &&
      (flags & (kTowerCompletedBit | kBossDefeatedBit)) ==
          (kTowerCompletedBit | kBossDefeatedBit);
}

bool SinnohExpedition::goldenBallUnlocked(
    const GymProgress& progress, const SinnohExpeditionState& state) {
  return available(progress, state);
}

void SinnohExpedition::reconcile(SinnohExpeditionState& state, const GymProgress& progress) {
  if (!state.rngState) state.rngState = 0x53494E4EU;
  ensureProgressSchema(state);
  if ((state.rewardFlags & kGymSchemaBit) == 0U) {
    // Every-site completion was the old end condition. Preserve that durable
    // achievement for rewards and Unova, while leaving the eight newly added
    // leaders unclaimed so an existing player can still battle all of them.
    if (allSitesUnlocked(state))
      state.rewardFlags = static_cast<uint8_t>(
          state.rewardFlags | kLegacyCompletionBit);
    // This byte used to be the fossil ledger. None of those bits represented
    // Gym wins, so carrying them forward would award arbitrary badges.
    state.gymVictoryMask = 0U;
    state.rewardFlags = static_cast<uint8_t>(state.rewardFlags | kGymSchemaBit);
  }
  if ((state.rewardFlags & kExplorationSchemaBit) == 0U) {
    // Older builds stored only an aggregate encounter count, so no individual
    // site clear can be inferred safely. Preserve that count (capped to the
    // low byte) and start the per-site ledger empty.
    state.battles = std::min<uint16_t>(state.battles, 0x00FFU);
    state.rewardFlags = static_cast<uint8_t>(
        state.rewardFlags | kExplorationSchemaBit);
  }
  if (!available(progress, state)) return;
  if (!state.unlockedSites) state.unlockedSites = 1U;
}

bool SinnohExpedition::towerCompleted(const SinnohExpeditionState& state) {
  return (progressFlags(state) & kTowerCompletedBit) != 0U;
}

bool SinnohExpedition::bossDefeated(const SinnohExpeditionState& state) {
  return (progressFlags(state) & kBossDefeatedBit) != 0U;
}

bool SinnohExpedition::recordTowerCompletion(SinnohExpeditionState& state) {
  return setProgressBit(state, kTowerCompletedBit);
}

bool SinnohExpedition::recordBossVictory(SinnohExpeditionState& state) {
  return setProgressBit(state, kBossDefeatedBit);
}

bool SinnohExpedition::discoveryNotificationPending(
    const GymProgress& progress, const SinnohExpeditionState& state) {
  return available(progress, state) &&
      (progressFlags(state) & kDiscoveryAcknowledgedBit) == 0U;
}

bool SinnohExpedition::acknowledgeDiscovery(SinnohExpeditionState& state) {
  return setProgressBit(state, kDiscoveryAcknowledgedBit);
}

bool SinnohExpedition::isUnlocked(const SinnohExpeditionState& state, SinnohSite site) {
  return (state.unlockedSites & (1U << siteIndex(site))) != 0U;
}

bool SinnohExpedition::allSitesUnlocked(const SinnohExpeditionState& state) {
  return (state.unlockedSites & kAllSitesMask) == kAllSitesMask;
}

bool SinnohExpedition::hasGymVictory(const SinnohExpeditionState& state,
                                     SinnohSite site) {
  return (state.gymVictoryMask & (1U << siteIndex(site))) != 0U;
}

uint8_t SinnohExpedition::gymVictoryCount(const SinnohExpeditionState& state) {
  uint8_t count = 0U;
  for (uint8_t index = 0U; index < kSiteCount; ++index)
    if ((state.gymVictoryMask & (1U << index)) != 0U) ++count;
  return count;
}

bool SinnohExpedition::allGymsDefeated(const SinnohExpeditionState& state) {
  return (state.gymVictoryMask & kAllSitesMask) == kAllSitesMask;
}

bool SinnohExpedition::hasExplored(const SinnohExpeditionState& state,
                                   SinnohSite site) {
  const uint8_t explored = static_cast<uint8_t>(state.battles >> 8U);
  return (explored & (1U << siteIndex(site))) != 0U;
}

uint8_t SinnohExpedition::exploredSiteCount(
    const SinnohExpeditionState& state) {
  const uint8_t explored = static_cast<uint8_t>(state.battles >> 8U);
  uint8_t count = 0U;
  for (uint8_t index = 0U; index < kSiteCount; ++index)
    if ((explored & (1U << index)) != 0U) ++count;
  return count;
}

bool SinnohExpedition::allSitesExplored(
    const SinnohExpeditionState& state) {
  return (static_cast<uint8_t>(state.battles >> 8U) & kAllSitesMask) ==
      kAllSitesMask;
}

bool SinnohExpedition::completed(const SinnohExpeditionState& state) {
  return allGymsDefeated(state) ||
      (state.rewardFlags & kLegacyCompletionBit) != 0U;
}

bool SinnohExpedition::fullyCompleted(const SinnohExpeditionState& state) {
  return allGymsDefeated(state) && allSitesExplored(state);
}

const SinnohGymDefinition* SinnohExpedition::gymDefinition(SinnohSite site) {
  const uint8_t index = static_cast<uint8_t>(site);
  return index < kSiteCount ? &kSinnohGyms[index] : nullptr;
}

bool SinnohExpedition::gymChallengeAvailable(
    const SinnohExpeditionState& state, SinnohSite site) {
  const uint8_t index = static_cast<uint8_t>(site);
  if (index >= kSiteCount || !isUnlocked(state, site) ||
      hasGymVictory(state, site)) return false;
  for (uint8_t previous = 0U; previous < index; ++previous)
    if ((state.gymVictoryMask & (1U << previous)) == 0U) return false;
  return true;
}

bool SinnohExpedition::startGym(
    const SinnohExpeditionState& state, BattleState& battle,
    PokemonCollection& collection, uint32_t playerUid, SinnohSite site,
    uint32_t seed) {
  const SinnohGymDefinition* gym = gymDefinition(site);
  OwnedPokemon* player = CollectionLogic::find(collection, playerUid);
  if (!gym || battle.active || !gymChallengeAvailable(state, site) || !player ||
      !CollectionLogic::isInParty(collection, playerUid) || !player->currentHp ||
      player->recoverySecondsRemaining) return false;

  BattleEngine::clear(battle);
  battle.active = true;
  battle.kind = BattleKind::SinnohGym;
  battle.outcome = BattleOutcome::Ongoing;
  battle.playerUid = playerUid;
  battle.unlockedGeneration = 4U;
  battle.rngState = seed ? seed :
      (0x53474D31U ^ (static_cast<uint32_t>(siteIndex(site)) << 16U));
  battle.gymId = siteIndex(site);
  battle.gymStage = 2U;
  battle.opponentCount = kOpponentTeamCapacity;
  battle.opponentIndex = 0U;
  battle.opponentItemUses = 2U;
  battle.rewardMoney = static_cast<uint32_t>(
      gym->team[kOpponentTeamCapacity - 1U].level) * 150U;
  for (uint8_t slot = 0U; slot < kOpponentTeamCapacity; ++slot) {
    const SinnohGymPokemon& authored = gym->team[slot];
    OwnedPokemon opponent = CollectionLogic::createPokemon(
        0U, authored.speciesId, authored.level, false,
        battle.rngState ^ (0x9E3779B9U * (slot + 1U)), 4U);
    for (uint8_t moveSlot = 0U; moveSlot < kMoveSlots; ++moveSlot) {
      opponent.moves[moveSlot] = authored.moves[moveSlot];
      const FullMoveData* moveData = findFullMove(authored.moves[moveSlot]);
      opponent.movePp[moveSlot] = moveData ? moveData->pp : 0U;
    }
    opponent.heldItem = authored.heldItem;
    battle.opponents[slot] = opponent;
  }
  BattleEngine::orderOpponentTeamWeakestFirst(battle);
  BattleEngine::ensureOpponentUids(battle);
  BattleEngine::applyEntryAbilities(battle, collection);
  return true;
}

bool SinnohExpedition::recordGymVictory(SinnohExpeditionState& state,
                                        SinnohSite site) {
  if (!gymChallengeAvailable(state, site)) return false;
  state.gymVictoryMask = static_cast<uint8_t>(
      state.gymVictoryMask | (1U << siteIndex(site)));
  const uint16_t available = state.research < kMaximumResearch
      ? static_cast<uint16_t>(kMaximumResearch - state.research) : 0U;
  state.research = static_cast<uint16_t>(
      state.research + std::min<uint16_t>(kGymResearchReward, available));
  return true;
}

bool SinnohExpedition::platinumThemeUnlocked(const SinnohExpeditionState& state) {
  return (state.rewardFlags & kPlatinumThemeRewardBit) != 0U;
}

SinnohExpedition::CompletionReward SinnohExpedition::reconcileCompletionReward(
    SinnohExpeditionState& state, Inventory& inventory) {
  CompletionReward reward{};
  if (!completed(state)) return reward;

  if (!platinumThemeUnlocked(state)) {
    state.rewardFlags = static_cast<uint8_t>(
        state.rewardFlags | kPlatinumThemeRewardBit);
    reward.platinumThemeUnlocked = true;
  }

  uint16_t& masterBalls =
      inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)];
  if (fullyCompleted(state) &&
      (state.rewardFlags & kMasterBallRewardBit) == 0U &&
      masterBalls < kInventoryStackLimit) {
    ++masterBalls;
    state.rewardFlags = static_cast<uint8_t>(
        state.rewardFlags | kMasterBallRewardBit);
    reward.masterBallGranted = true;
  }
  return reward;
}

uint16_t SinnohExpedition::unlockCost(SinnohSite site) {
  return kSites[siteIndex(site)].unlockCost;
}

bool SinnohExpedition::selectOrUnlock(SinnohExpeditionState& state, SinnohSite site) {
  const uint8_t index = siteIndex(site);
  const uint8_t bit = static_cast<uint8_t>(1U << index);
  if ((state.unlockedSites & bit) == 0U) {
    // Sites open in a readable expedition order; skipping straight to the
    // Rift would turn research into a menu tax rather than exploration.
    if (index && ((state.unlockedSites & (1U << (index - 1U))) == 0U ||
                  (state.gymVictoryMask & (1U << (index - 1U))) == 0U))
      return false;
    const uint16_t cost = kSites[index].unlockCost;
    if (state.research < cost) return false;
    state.research = static_cast<uint16_t>(state.research - cost);
    state.unlockedSites |= bit;
  }
  return true;
}

const char* SinnohExpedition::siteName(SinnohSite site) { return kSites[siteIndex(site)].name; }
const char* SinnohExpedition::siteTagline(SinnohSite site) { return kSites[siteIndex(site)].tagline; }

uint8_t SinnohExpedition::availableSpecies(SinnohSite site, uint16_t* output,
                                           uint8_t capacity) {
  if (!output || !capacity || site == SinnohSite::DistortionRift) return 0U;
  const SiteData& data = kSites[siteIndex(site)];
  const uint8_t count = std::min<uint8_t>(data.count, capacity);
  for (uint8_t index = 0; index < count; ++index)
    output[index] = data.encounters[index].species;
  return count;
}

uint16_t SinnohExpedition::recordBattle(SinnohExpeditionState& state,
                                        SinnohSite site, bool successful) {
  if (!successful) return 0U;
  const uint8_t total = static_cast<uint8_t>(std::min<uint16_t>(
      static_cast<uint16_t>(state.battles & 0x00FFU) + 1U, 0x00FFU));
  const uint8_t explored = static_cast<uint8_t>(
      static_cast<uint8_t>(state.battles >> 8U) |
      static_cast<uint8_t>(1U << siteIndex(site)));
  state.battles = static_cast<uint16_t>(
      total | (static_cast<uint16_t>(explored) << 8U));
  constexpr uint16_t kResearchPerPokemon = 1U;
  const uint16_t available = state.research < kMaximumResearch
      ? static_cast<uint16_t>(kMaximumResearch - state.research) : 0U;
  const uint16_t gained = std::min<uint16_t>(kResearchPerPokemon, available);
  state.research = static_cast<uint16_t>(state.research + gained);
  return gained;
}

uint16_t SinnohExpedition::chooseEncounterSpecies(const SinnohExpeditionState& state,
                                                  SinnohSite site, uint8_t teamLevel,
                                                  uint32_t seed) {
  const SiteData& data = kSites[siteIndex(site)];
  const uint8_t ceiling = std::max<uint8_t>(5U, teamLevel);
  uint32_t commonTotal = 0;
  uint32_t legendaryTotal = 0;
  for (uint8_t index = 0; index < data.count; ++index) {
    const EncounterEntry& entry = data.encounters[index];
    // Research is spent when a site is unlocked.  Once the Rift is open,
    // its full encounter table must remain available; checking research here
    // made the first post-unlock battle permanently collapse to Rotom (479).
    // The selected site's unlock bit is already enforced by startEncounter().
    const bool rareSinnohVisitor = entry.species >= 479U && entry.weight == 1U;
    if (site != SinnohSite::DistortionRift && entry.level > ceiling &&
        !rareSinnohVisitor && !isSinnohLegendary(entry.species))
      continue;
    if (isSinnohLegendary(entry.species)) legendaryTotal += entry.weight;
    else commonTotal += entry.weight;
  }
  if (!commonTotal && !legendaryTotal) return data.encounters[0].species;
  uint32_t random = seed ? seed : state.rngState;
  random = nextRandom(random);

  // The Rift used to make its fourteen weight-1 legendaries account for
  // 14/278 of all rolls, about thirteen times the normal wild rate. Split
  // the decision into a global rarity roll and a weighted species roll so
  // every Sinnoh site follows the same 1/265 baseline. Existing weights are
  // deliberately retained inside each group to preserve common/legendary
  // variety without letting the Rift's roster size inflate rarity.
  const bool chooseLegendary = legendaryTotal &&
      (!commonTotal || random % kLegendaryChanceDenominator == 0U);
  random = nextRandom(random);
  const uint32_t selectedTotal = chooseLegendary ? legendaryTotal : commonTotal;
  if (!selectedTotal) return data.encounters[0].species;
  uint32_t selected = random % selectedTotal;
  for (uint8_t index = 0; index < data.count; ++index) {
    const EncounterEntry& entry = data.encounters[index];
    const bool rareSinnohVisitor = entry.species >= 479U && entry.weight == 1U;
    if (site != SinnohSite::DistortionRift && entry.level > ceiling &&
        !rareSinnohVisitor && !isSinnohLegendary(entry.species)) continue;
    if (isSinnohLegendary(entry.species) != chooseLegendary) continue;
    if (selected < entry.weight) return entry.species;
    selected -= entry.weight;
  }
  return data.encounters[0].species;
}

bool SinnohExpedition::startEncounter(SinnohExpeditionState& state, BattleState& battle,
                                      PokemonCollection& collection, uint32_t playerUid,
                                      SinnohSite site, uint32_t seed) {
  if (!isUnlocked(state, site) || battle.active) return false;
  // Sinnoh is the first point at which Platinum learnsets and ability pairs
  // are legal. Existing party members keep their moves, but receive the
  // authentic Gen-IV ability pair before entering the first expedition.
  CollectionLogic::applyGenerationContent(collection, 4U);
  if (!BattleEngine::startWild(battle, collection, playerUid, seed, 4U, false)) return false;
  const uint8_t teamLevel = BattleEngine::highestPartyLevel(collection);
  const uint16_t species = chooseEncounterSpecies(state, site, teamLevel, nextRandom(state.rngState));
  // Sinnoh remains a postgame challenge, but its wild encounters never reach
  // the level-100 ceiling used by other late-game systems.
  const uint8_t baseline = std::min<uint8_t>(SinnohExpedition::kMaxEncounterLevel,
                                             std::max<uint8_t>(5U, teamLevel));
  const uint8_t spread = baseline > 8U ? static_cast<uint8_t>(1U + nextRandom(state.rngState) % 6U) : 1U;
  const uint8_t level = static_cast<uint8_t>(std::max<uint8_t>(5U, baseline > spread ? baseline - spread : baseline));
  const uint32_t shinyRoll = nextRandom(state.rngState);
  battle.opponents[0] = CollectionLogic::createPokemon(0, species, level,
      CollectionLogic::isShinyRoll(shinyRoll), nextRandom(state.rngState), 4U);
  BattleEngine::ensureOpponentUids(battle);
  BattleEngine::applyEntryAbilities(battle, collection);
  return true;
}
