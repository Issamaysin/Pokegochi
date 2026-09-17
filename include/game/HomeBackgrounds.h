#pragma once

#include <Arduino.h>

namespace HomeBackgrounds {

constexpr uint8_t kCount = 39;
constexpr uint8_t kGridWidth = 19;
constexpr uint8_t kGridHeight = 8;

struct PetRoute {
  int16_t x0;
  int16_t y0;
  int16_t x1;
  int16_t y1;
};

uint8_t normalize(uint8_t id);
const char* name(uint8_t id);
const char* game(uint8_t id);
const PetRoute& route(uint8_t id, uint8_t partySlot);
// Navigation is compiled from the collision attributes of the original
// FireRed/Emerald map.  Home movement can therefore stay completely in RAM:
// no sprite tick ever has to inspect or reopen a map on the SD card.
bool walkable(uint8_t id, uint8_t gridX, uint8_t gridY);

}  // namespace HomeBackgrounds
