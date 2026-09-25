#pragma once

#include <cstdint>
#include "game/Collection.h"

enum class WorldTimePeriod : uint8_t { Morning, Day, Evening, Night };

namespace WorldClock {
// Before the phone has supplied local civil time, a new save starts at 08:00.
// Once synchronized, localEpochSeconds is advanced alongside the persistent
// play clock and is therefore independent of an always-on Home clock widget.
uint32_t effectiveSeconds(uint32_t playTimeSeconds, uint32_t localEpochSeconds = 0);
uint16_t minuteOfDay(uint32_t playTimeSeconds, uint32_t localEpochSeconds = 0);
uint32_t day(uint32_t playTimeSeconds, uint32_t localEpochSeconds = 0);
WorldTimePeriod period(uint32_t playTimeSeconds, uint32_t localEpochSeconds = 0);
const char* periodName(WorldTimePeriod period);
bool speciesSleeps(uint16_t speciesId, WorldTimePeriod period);
bool encounterPreferred(uint16_t speciesId, WorldTimePeriod period);
}

constexpr uint8_t kBerryGardenPlotCount = 6;

struct BerryPlotState {
  HeldItem berry = HeldItem::None;
  uint8_t watered = 0;
  uint16_t reserved = 0;
  uint32_t remainingSeconds = 0;
};

struct BerryGardenState {
  BerryPlotState plots[kBerryGardenPlotCount]{};
  uint32_t rngState = 0x42455252UL;
};

namespace BerryGarden {
constexpr uint8_t kPlantableBerryCount = 9;
HeldItem plantableBerry(uint8_t index);
uint32_t growthSeconds(HeldItem berry);
uint8_t growthStage(const BerryPlotState& plot);
bool ready(const BerryPlotState& plot);
bool plant(BerryGardenState& garden, uint8_t plot, HeldItem berry,
           uint16_t heldInventory[]);
bool water(BerryGardenState& garden, uint8_t plot);
uint8_t harvest(BerryGardenState& garden, uint8_t plot,
                uint16_t heldInventory[]);
void advance(BerryGardenState& garden, uint32_t elapsedSeconds);
}

// Retired Map Maker save layout. Keep these bytes frozen: v38 records already
// contain this block, even though no runtime feature reads or writes it now.
constexpr uint8_t kCreativeBackgroundSlotCount = 5;
constexpr uint8_t kCreativeDecorationCapacity = 12;
constexpr uint8_t kCreativeBackgroundSchema = 1;

enum class CreativeDecorationKind : uint8_t {
  None = 0,
  Plant = 1,
  Flowers = 3,
  Ornament = 45,
  SurfMat = 11,
  Desk = 27,
  Doll = 53,
  Count = 103,
};

struct CreativeDecoration {
  uint8_t kind = static_cast<uint8_t>(CreativeDecorationKind::None);
  uint8_t gridX = 0;
  uint8_t gridY = 0;
};

struct CreativeBackgroundSlot {
  uint8_t used = 0;
  uint8_t theme = 0;
  uint8_t decorationCount = 0;
  uint8_t reserved = 0;
  CreativeDecoration decorations[kCreativeDecorationCapacity]{};
};

struct CreativeBackgroundState {
  uint8_t activeSlot = 0xFF;
  uint8_t reserved[3]{};
  CreativeBackgroundSlot slots[kCreativeBackgroundSlotCount]{};
};

namespace SpeciesHeldItems {
// Grants each species-linked item once when its matching Pokemon first exists
// in the collection. Returns the first newly granted item for UI messaging.
HeldItem reconcile(const PokemonCollection& collection,
                   SpecialHeldItemInventory& inventory);
}
