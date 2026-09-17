#include "game/HomeBackgrounds.h"

namespace HomeBackgrounds {
namespace {

struct Definition {
  const char* name;
  const char* game;
  PetRoute routes[3];
  uint32_t walkableRows[kGridHeight];
};

constexpr Definition kDefinitions[kCount] = {
#include "HomeBackgroundDataGenerated.inc"
};

}  // namespace

uint8_t normalize(uint8_t id) { return id < kCount ? id : 0; }
const char* name(uint8_t id) { return kDefinitions[normalize(id)].name; }
const char* game(uint8_t id) { return kDefinitions[normalize(id)].game; }
const PetRoute& route(uint8_t id, uint8_t partySlot) {
  return kDefinitions[normalize(id)].routes[partySlot < 3 ? partySlot : 0];
}

bool walkable(uint8_t id, uint8_t gridX, uint8_t gridY) {
  if (gridX >= kGridWidth || gridY >= kGridHeight) return false;
  return (kDefinitions[normalize(id)].walkableRows[gridY] & (1UL << gridX)) != 0U;
}

}  // namespace HomeBackgrounds
