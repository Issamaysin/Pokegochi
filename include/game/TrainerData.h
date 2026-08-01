#pragma once
#include <cstdint>

struct TrainerProfile {
  uint8_t id;
  const char* trainerClass;
  const char* name;
  const char* frontAsset;
  const char* introLine1;
  const char* introLine2;
  uint16_t species[3];
  uint8_t speciesCount;
};

const TrainerProfile* trainerProfile(uint8_t id);
uint8_t trainerProfileCount();
