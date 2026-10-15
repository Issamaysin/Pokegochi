#include <cassert>
#include <cstring>

#include "game/HomeBackgrounds.h"

namespace {

void assertRoutePointIsWalkable(uint8_t backgroundId, int16_t x, int16_t y) {
  assert(x >= 8 && y >= 8);
  assert((x - 8) % 16 == 0);
  assert((y - 8) % 16 == 0);
  const uint8_t gridX = static_cast<uint8_t>((x - 8) / 16);
  const uint8_t gridY = static_cast<uint8_t>((y - 8) / 16);
  assert(gridX < HomeBackgrounds::kGridWidth);
  assert(gridY < HomeBackgrounds::kGridHeight);
  assert(HomeBackgrounds::walkable(backgroundId, gridX, gridY));
}

}  // namespace

int main() {
  using namespace HomeBackgrounds;

  static_assert(kCount == 47, "Home background ids are persisted in saves");
  assert(std::strcmp(name(39), "PALLET TOWN") == 0);
  assert(std::strcmp(name(40), "CERULEAN CAVE") == 0);
  assert(std::strcmp(name(41), "NAVEL ROCK SUMMIT") == 0);
  assert(std::strcmp(name(42), "ABANDONED SHIP") == 0);
  assert(std::strcmp(name(43), "BATTLE FACTORY ARENA") == 0);
  assert(std::strcmp(name(44), "BATTLE PIKE PATHS") == 0);
  assert(std::strcmp(name(45), "BATTLE ARENA RING") == 0);
  assert(std::strcmp(name(46), "ARTISAN CAVE") == 0);
  assert(normalize(kCount) == 0);

  // Narrow battle rooms are centered in the 304 px Home viewport. The black
  // margins must never wrap into valid metatiles from the opposite map edge.
  for (uint8_t y = 0; y < kGridHeight; ++y) {
    for (uint8_t x = 0; x < 3; ++x) {
      assert(!walkable(43, x, y));
    }
    for (uint8_t x = 16; x < kGridWidth; ++x) {
      assert(!walkable(43, x, y));
    }
    assert(!walkable(44, 0, y));
    assert(!walkable(44, 1, y));
    assert(!walkable(44, 17, y));
    assert(!walkable(44, 18, y));
    assert(!walkable(45, 0, y));
    assert(!walkable(45, 17, y));
    assert(!walkable(45, 18, y));
  }

  for (uint8_t id = 0; id < kCount; ++id) {
    bool hasWalkableCell = false;
    for (uint8_t y = 0; y < kGridHeight; ++y) {
      for (uint8_t x = 0; x < kGridWidth; ++x) {
        hasWalkableCell = hasWalkableCell || walkable(id, x, y);
      }
    }
    assert(hasWalkableCell);

    for (uint8_t slot = 0; slot < 3; ++slot) {
      const PetRoute& patrol = route(id, slot);
      assertRoutePointIsWalkable(id, patrol.x0, patrol.y0);
      assertRoutePointIsWalkable(id, patrol.x1, patrol.y1);
    }
  }

  return 0;
}
