#pragma once

#include <cstdint>

#include "game/BattleEngine.h"
#include "game/GymSystem.h"

// Sinnoh is a self-contained final endgame expedition. The compact state also
// records whether the player has completed one full Battle Tower run, defeated
// one Boss and acknowledged the newly unlocked region. Those flags reuse an
// obsolete cursor byte, preserving the V44 save ABI and every Pokemon record.
enum class SinnohSite : uint8_t {
  LakeVerity,
  EternaForest,
  OreburghMine,
  GreatMarsh,
  MtCoronet,
  FuegoIron,
  Snowpoint,
  DistortionRift,
  Count,
};

struct SinnohExpeditionState {
  uint32_t rngState = 0x53494E4EU;  // "SINN"
  uint16_t research = 0;
  // This occupies the former excavations slot for save compatibility.
  uint16_t battles = 0;
  uint8_t unlockedSites = 0;
  // The UI now uses its visible carousel cursor instead of a persisted active
  // site. This byte carries a schema marker plus the three endgame flags; old
  // values 0..7 are distinguishable and migrated safely by reconcile().
  uint8_t selectedSite = 0;
  uint8_t legacyFossilMask = 0;
  // Reuses the final reserved byte from the original Sinnoh save record.
  // These flags make the all-sites reward idempotent across reboot/migration.
  uint8_t rewardFlags = 0;
};
static_assert(sizeof(SinnohExpeditionState) == 12,
              "Sinnoh expedition save layout changed");

class SinnohExpedition {
 public:
  static constexpr uint8_t kSiteCount = static_cast<uint8_t>(SinnohSite::Count);
  static constexpr uint8_t kAllSitesMask =
      static_cast<uint8_t>((1U << kSiteCount) - 1U);
  static constexpr uint8_t kPlatinumThemeRewardBit = 1U << 0;
  static constexpr uint8_t kMasterBallRewardBit = 1U << 1;
  static constexpr uint8_t kMaxEncounterLevel = 95U;
  // Match the measured baseline wild rarity: roughly 0.378%, or one
  // legendary/mythical encounter in every 265 Sinnoh encounters.
  static constexpr uint16_t kLegendaryChanceDenominator = 265U;

  static bool available(const GymProgress& progress,
                        const SinnohExpeditionState& state);
  static void reconcile(SinnohExpeditionState& state, const GymProgress& progress);
  static bool towerCompleted(const SinnohExpeditionState& state);
  static bool bossDefeated(const SinnohExpeditionState& state);
  static bool recordTowerCompletion(SinnohExpeditionState& state);
  static bool recordBossVictory(SinnohExpeditionState& state);
  static bool discoveryNotificationPending(
      const GymProgress& progress, const SinnohExpeditionState& state);
  static bool acknowledgeDiscovery(SinnohExpeditionState& state);
  static bool isUnlocked(const SinnohExpeditionState& state, SinnohSite site);
  static bool allSitesUnlocked(const SinnohExpeditionState& state);
  static bool platinumThemeUnlocked(const SinnohExpeditionState& state);
  struct CompletionReward {
    bool platinumThemeUnlocked = false;
    bool masterBallGranted = false;
  };
  // Grants the one-time reward after all eight sites are open. The Platinum
  // theme is always recorded immediately; the Master Ball remains pending if
  // its inventory stack is full and is granted on the next reconciliation.
  static CompletionReward reconcileCompletionReward(SinnohExpeditionState& state,
                                                    Inventory& inventory);
  static uint16_t unlockCost(SinnohSite site);
  static bool selectOrUnlock(SinnohExpeditionState& state, SinnohSite site);
  static const char* siteName(SinnohSite site);
  static const char* siteTagline(SinnohSite site);
  // Returns the ordinary encounter roster for the compact expedition card.
  // The Distortion Rift is intentionally represented by a single "all
  // Sinnoh" label in the UI instead of trying to render 107 icons.
  static uint8_t availableSpecies(SinnohSite site, uint16_t* output,
                                  uint8_t capacity);
  static uint16_t recordBattle(SinnohExpeditionState& state, bool successful);
  static uint16_t chooseEncounterSpecies(const SinnohExpeditionState& state,
                                         SinnohSite site, uint8_t teamLevel,
                                         uint32_t seed);
  static bool startEncounter(SinnohExpeditionState& state, BattleState& battle,
                             PokemonCollection& collection, uint32_t playerUid,
                             SinnohSite site, uint32_t seed);
};
