#include "game/TrainerData.h"

namespace {
#include "TrainerDataGenerated.inc"
}

const TrainerProfile* trainerProfile(uint8_t id) {
  return id < sizeof(kProfiles) / sizeof(kProfiles[0]) ? &kProfiles[id] : nullptr;
}
uint8_t trainerProfileCount() { return sizeof(kProfiles) / sizeof(kProfiles[0]); }
const TrainerProfile* trainerProfileForGeneration(uint8_t unlockedGeneration, uint32_t roll) {
  uint8_t count=0;for(const auto& profile:kProfiles)if(profile.generation<=unlockedGeneration)++count;
  if(!count)return nullptr;uint8_t selected=static_cast<uint8_t>(roll%count);
  for(const auto& profile:kProfiles)if(profile.generation<=unlockedGeneration){if(!selected--)return &profile;}
  return nullptr;
}
