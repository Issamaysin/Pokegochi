#pragma once

#include <cstdint>

// Compatibility-only layout. Releases V42 and newer serialized these 32
// bytes inside GameSave. The former optional game has been removed, but this
// frozen reservation must remain so existing saves keep their exact offsets.
namespace TowerDefense {

struct CampaignState {
  uint32_t rngState = 0;
  uint16_t totalScore = 0;
  uint16_t totalDefeated = 0;
  uint16_t bestScore[6]{};
  uint8_t stageStars[6]{};
  uint8_t unlockedStages = 0;
  uint8_t selectedStage = 0;
  uint8_t enabledDefendersMask = 0;
  // Former padding. The retired 32-byte block stays ABI-identical; Settings
  // now stores its three-byte EN/PT marker here.
  uint8_t reserved[3]{};
};

static_assert(sizeof(CampaignState) == 32,
              "Legacy save reservation layout changed");

}  // namespace TowerDefense
