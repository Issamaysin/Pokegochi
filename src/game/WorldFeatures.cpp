#include "game/WorldFeatures.h"
#include <algorithm>

namespace {
constexpr uint32_t kDaySeconds = 86400UL;
constexpr uint32_t kClockStartSeconds = 8UL * 3600UL;

bool inList(uint16_t speciesId, const uint16_t* values, uint8_t count) {
  for (uint8_t index = 0; index < count; ++index)
    if (values[index] == speciesId) return true;
  return false;
}

constexpr HeldItem kPlantableBerries[BerryGarden::kPlantableBerryCount] = {
  HeldItem::OranBerry, HeldItem::CheriBerry, HeldItem::ChestoBerry,
  HeldItem::PechaBerry, HeldItem::RawstBerry, HeldItem::AspearBerry,
  HeldItem::PersimBerry, HeldItem::SitrusBerry, HeldItem::LumBerry,
};
}

namespace WorldClock {
uint32_t effectiveSeconds(uint32_t playTimeSeconds,uint32_t localEpochSeconds){
  return localEpochSeconds ? localEpochSeconds : playTimeSeconds + kClockStartSeconds;
}
uint16_t minuteOfDay(uint32_t playTimeSeconds,uint32_t localEpochSeconds) {
  return static_cast<uint16_t>((effectiveSeconds(playTimeSeconds,localEpochSeconds)%kDaySeconds)/60UL);
}
uint32_t day(uint32_t playTimeSeconds,uint32_t localEpochSeconds) {
  return effectiveSeconds(playTimeSeconds,localEpochSeconds)/kDaySeconds;
}
WorldTimePeriod period(uint32_t playTimeSeconds,uint32_t localEpochSeconds) {
  const uint16_t minute = minuteOfDay(playTimeSeconds,localEpochSeconds);
  if (minute >= 5U * 60U && minute < 10U * 60U) return WorldTimePeriod::Morning;
  if (minute < 17U * 60U) return WorldTimePeriod::Day;
  if (minute < 20U * 60U) return WorldTimePeriod::Evening;
  return WorldTimePeriod::Night;
}
const char* periodName(WorldTimePeriod value) {
  switch (value) {
    case WorldTimePeriod::Morning: return "MORNING";
    case WorldTimePeriod::Day: return "DAY";
    case WorldTimePeriod::Evening: return "EVENING";
    case WorldTimePeriod::Night: return "NIGHT";
  }
  return "DAY";
}
bool speciesSleeps(uint16_t speciesId, WorldTimePeriod value) {
  static constexpr uint16_t kNocturnal[] = {
    41,42,92,93,94,163,164,167,168,198,200,215,228,229,261,262,302,353,354,
  };
  static constexpr uint16_t kSleepless[] = {63,64,65,96,97,150,151,201,243,244,245,386};
  if (inList(speciesId,kSleepless,static_cast<uint8_t>(sizeof(kSleepless)/sizeof(kSleepless[0]))))
    return false;
  const bool nocturnal = inList(
      speciesId,kNocturnal,static_cast<uint8_t>(sizeof(kNocturnal)/sizeof(kNocturnal[0])));
  return nocturnal ? (value == WorldTimePeriod::Day) : (value == WorldTimePeriod::Night);
}
bool encounterPreferred(uint16_t speciesId, WorldTimePeriod value) {
  static constexpr uint16_t kMorning[] = {
    10,11,12,16,17,18,21,22,84,85,161,162,165,166,177,178,276,277,278,279,
  };
  static constexpr uint16_t kEvening[] = {
    19,20,43,44,46,47,48,49,118,119,129,130,190,191,192,193,265,266,267,268,
  };
  static constexpr uint16_t kNight[] = {
    41,42,92,93,94,163,164,167,168,198,200,215,228,229,261,262,302,353,354,
  };
  const uint16_t* list = nullptr; uint8_t count = 0;
  if (value == WorldTimePeriod::Morning) {
    list=kMorning;count=static_cast<uint8_t>(sizeof(kMorning)/sizeof(kMorning[0]));
  } else if (value == WorldTimePeriod::Evening) {
    list=kEvening;count=static_cast<uint8_t>(sizeof(kEvening)/sizeof(kEvening[0]));
  } else if (value == WorldTimePeriod::Night) {
    list=kNight;count=static_cast<uint8_t>(sizeof(kNight)/sizeof(kNight[0]));
  } else return !inList(speciesId,kNight,static_cast<uint8_t>(sizeof(kNight)/sizeof(kNight[0])));
  return inList(speciesId,list,count);
}
}

namespace BerryGarden {
HeldItem plantableBerry(uint8_t index) {
  return index < kPlantableBerryCount ? kPlantableBerries[index] : HeldItem::None;
}
uint32_t growthSeconds(HeldItem berry) {
  switch (berry) {
    case HeldItem::OranBerry:return 20U*60U;
    case HeldItem::CheriBerry:case HeldItem::ChestoBerry:case HeldItem::PechaBerry:
      return 30U*60U;
    case HeldItem::RawstBerry:case HeldItem::AspearBerry:case HeldItem::PersimBerry:
      return 40U*60U;
    case HeldItem::SitrusBerry:return 60U*60U;
    case HeldItem::LumBerry:return 90U*60U;
    default:return 0U;
  }
}
uint8_t growthStage(const BerryPlotState& plot) {
  if (plot.berry == HeldItem::None) return 0;
  const uint32_t total = growthSeconds(plot.berry);
  if (!plot.remainingSeconds || !total) return 4;
  const uint32_t elapsed = total > plot.remainingSeconds ? total - plot.remainingSeconds : 0U;
  return static_cast<uint8_t>(std::min<uint32_t>(3U, 1U + elapsed * 3U / total));
}
bool ready(const BerryPlotState& plot) {
  return plot.berry != HeldItem::None && plot.remainingSeconds == 0U;
}
bool plant(BerryGardenState& garden,uint8_t plot,HeldItem berry,uint16_t inventory[]) {
  if(plot>=kBerryGardenPlotCount||!inventory||garden.plots[plot].berry!=HeldItem::None)return false;
  const uint8_t raw=static_cast<uint8_t>(berry);
  const uint32_t duration=growthSeconds(berry);
  if(!duration||raw>=kHeldItemInventorySlots||!inventory[raw])return false;
  --inventory[raw];garden.plots[plot].berry=berry;
  garden.plots[plot].watered=0;garden.plots[plot].remainingSeconds=duration;return true;
}
bool water(BerryGardenState& garden,uint8_t plot){
  if(plot>=kBerryGardenPlotCount||garden.plots[plot].berry==HeldItem::None||
     !garden.plots[plot].remainingSeconds||garden.plots[plot].watered)return false;
  garden.plots[plot].watered=1;return true;
}
uint8_t harvest(BerryGardenState& garden,uint8_t plot,uint16_t inventory[]){
  if(plot>=kBerryGardenPlotCount||!inventory||!ready(garden.plots[plot]))return 0;
  BerryPlotState& state=garden.plots[plot];const uint8_t raw=static_cast<uint8_t>(state.berry);
  if(raw>=kHeldItemInventorySlots)return 0;
  uint32_t rng=garden.rngState?garden.rngState:0x42455252UL;
  rng^=rng<<13U;rng^=rng>>17U;rng^=rng<<5U;garden.rngState=rng;
  const uint8_t wanted=static_cast<uint8_t>(2U+state.watered+(rng&1U));
  const uint8_t granted=static_cast<uint8_t>(std::min<uint32_t>(wanted,UINT16_MAX-inventory[raw]));
  inventory[raw]=static_cast<uint16_t>(inventory[raw]+granted);state=BerryPlotState{};return granted;
}
void advance(BerryGardenState& garden,uint32_t elapsedSeconds){
  for(BerryPlotState& plot:garden.plots)if(plot.berry!=HeldItem::None&&plot.remainingSeconds)
    plot.remainingSeconds=elapsedSeconds>=plot.remainingSeconds?0U:plot.remainingSeconds-elapsedSeconds;
}
}

namespace SpeciesHeldItems {
HeldItem reconcile(const PokemonCollection& collection,SpecialHeldItemInventory& inventory){
  struct Reward{HeldItem item;uint16_t firstSpecies;uint16_t secondSpecies;};
  static constexpr Reward kRewards[kSpecialHeldItemCount]={
    {HeldItem::LightBall,25,0},{HeldItem::LuckyPunch,113,0},
    {HeldItem::ThickClub,104,105},{HeldItem::Stick,83,0},
    {HeldItem::MetalPowder,132,0},{HeldItem::QuickPowder,132,0},
    {HeldItem::DeepSeaTooth,366,0},{HeldItem::DeepSeaScale,366,0},
    {HeldItem::SoulDew,380,381},
  };
  HeldItem first=HeldItem::None;
  for(uint8_t reward=0;reward<kSpecialHeldItemCount;++reward){
    const uint16_t bit=static_cast<uint16_t>(1U<<reward);if(inventory.claimedMask&bit)continue;
    bool owned=false;for(const OwnedPokemon& pokemon:collection.box)if(
        pokemon.uid!=kEmptyPokemonUid&&(pokemon.speciesId==kRewards[reward].firstSpecies||
        (kRewards[reward].secondSpecies&&pokemon.speciesId==kRewards[reward].secondSpecies))){owned=true;break;}
    if(!owned)continue;inventory.claimedMask=static_cast<uint16_t>(inventory.claimedMask|bit);
    ++inventory.quantities[reward];if(first==HeldItem::None)first=kRewards[reward].item;
  }
  return first;
}
}
