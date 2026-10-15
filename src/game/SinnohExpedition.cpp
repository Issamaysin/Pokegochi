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

void SinnohExpedition::reconcile(SinnohExpeditionState& state, const GymProgress& progress) {
  if (!state.rngState) state.rngState = 0x53494E4EU;
  ensureProgressSchema(state);
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

bool SinnohExpedition::platinumThemeUnlocked(const SinnohExpeditionState& state) {
  return (state.rewardFlags & kPlatinumThemeRewardBit) != 0U;
}

SinnohExpedition::CompletionReward SinnohExpedition::reconcileCompletionReward(
    SinnohExpeditionState& state, Inventory& inventory) {
  CompletionReward reward{};
  if (!allSitesUnlocked(state)) return reward;

  if (!platinumThemeUnlocked(state)) {
    state.rewardFlags = static_cast<uint8_t>(
        state.rewardFlags | kPlatinumThemeRewardBit);
    reward.platinumThemeUnlocked = true;
  }

  uint16_t& masterBalls =
      inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)];
  if ((state.rewardFlags & kMasterBallRewardBit) == 0U &&
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
    if (index && (state.unlockedSites & (1U << (index - 1U))) == 0U) return false;
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

uint16_t SinnohExpedition::recordBattle(SinnohExpeditionState& state, bool successful) {
  if (!successful) return 0U;
  ++state.battles;
  constexpr uint16_t kResearchPerPokemon = 1U;
  const uint16_t available = state.research < 999U
      ? static_cast<uint16_t>(999U - state.research) : 0U;
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
