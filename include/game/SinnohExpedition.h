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
  // One victory bit per site Gym. This reuses the retired fossil mask without
  // changing the twelve-byte Sinnoh record or any shipped save offset.
  uint8_t gymVictoryMask = 0;
  // Reuses the final reserved byte from the original Sinnoh save record.
  // These flags make the eight-Gym reward idempotent across reboot/migration.
  uint8_t rewardFlags = 0;
};
static_assert(sizeof(SinnohExpeditionState) == 12,
              "Sinnoh expedition save layout changed");

struct SinnohGymPokemon {
  uint16_t speciesId;
  uint8_t level;
  MoveId moves[kMoveSlots];
  HeldItem heldItem;
};

struct SinnohGymDefinition {
  SinnohSite site;
  const char* leader;
  const char* badge;
  const char* type;
  const char* frontAsset;
  const char* introLine1;
  const char* introLine2;
  SinnohGymPokemon team[kOpponentTeamCapacity];
};

class SinnohExpedition {
 public:
  static constexpr uint8_t kSiteCount = static_cast<uint8_t>(SinnohSite::Count);
  static constexpr uint8_t kAllSitesMask =
      static_cast<uint8_t>((1U << kSiteCount) - 1U);
  static constexpr uint8_t kPlatinumThemeRewardBit = 1U << 0;
  static constexpr uint8_t kMasterBallRewardBit = 1U << 1;
  // The schema marker distinguishes saves created before site Gyms existed.
  // A legacy-completion bit keeps an already completed region/Unova open,
  // while still leaving every new Gym available to play.
  static constexpr uint8_t kGymSchemaBit = 1U << 2;
  static constexpr uint8_t kLegacyCompletionBit = 1U << 3;
  static constexpr uint8_t kMaxEncounterLevel = 95U;
  static constexpr uint16_t kMaximumResearch = 999U;
  static constexpr uint16_t kGymResearchReward = 20U;
  // Match the measured baseline wild rarity: roughly 0.378%, or one
  // legendary/mythical encounter in every 265 Sinnoh encounters.
  static constexpr uint16_t kLegendaryChanceDenominator = 265U;

  static bool available(const GymProgress& progress,
                        const SinnohExpeditionState& state);
  // Golden Ball ownership is derived from the existing Sinnoh gate. This
  // makes it permanent and grants it automatically to saves that had already
  // unlocked the region before the item was introduced.
  static bool goldenBallUnlocked(const GymProgress& progress,
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
  static bool hasGymVictory(const SinnohExpeditionState& state, SinnohSite site);
  static uint8_t gymVictoryCount(const SinnohExpeditionState& state);
  static bool allGymsDefeated(const SinnohExpeditionState& state);
  // New saves complete Sinnoh by defeating all eight leaders. Saves which
  // had already opened every site before this system existed remain complete
  // for rewards and Unova, but may still challenge the new leaders.
  static bool completed(const SinnohExpeditionState& state);
  static const SinnohGymDefinition* gymDefinition(SinnohSite site);
  static bool gymChallengeAvailable(const SinnohExpeditionState& state,
                                    SinnohSite site);
  static bool startGym(const SinnohExpeditionState& state, BattleState& battle,
                       PokemonCollection& collection, uint32_t playerUid,
                       SinnohSite site, uint32_t seed);
  static bool recordGymVictory(SinnohExpeditionState& state, SinnohSite site);
  static bool platinumThemeUnlocked(const SinnohExpeditionState& state);
  struct CompletionReward {
    bool platinumThemeUnlocked = false;
    bool masterBallGranted = false;
  };
  // Grants the one-time reward after all eight Gyms are defeated. The Platinum
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
