#include "game/TrainerData.h"

namespace {
#include "TrainerDataGenerated.inc"
}

const TrainerProfile* trainerProfile(uint8_t id) {
  return id < sizeof(kProfiles) / sizeof(kProfiles[0]) ? &kProfiles[id] : nullptr;
}
uint8_t trainerProfileCount() { return sizeof(kProfiles) / sizeof(kProfiles[0]); }
