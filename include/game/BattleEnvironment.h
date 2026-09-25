#pragma once

#include "game/BattleEngine.h"
#include "game/HomeWeather.h"

namespace BattleEnvironment {
constexpr uint8_t kAmbientWeatherTurns = 5U;

constexpr BattleWeather battleWeather(HomeWeatherKind weather) {
  switch (weather) {
    case HomeWeatherKind::Rain:return BattleWeather::Rain;
    case HomeWeatherKind::Wind:return BattleWeather::StrongWinds;
    case HomeWeatherKind::Hail:return BattleWeather::Hail;
    case HomeWeatherKind::Sun:return BattleWeather::Sun;
    case HomeWeatherKind::Clear:break;
  }
  return BattleWeather::Clear;
}

constexpr const char* entryWeatherMessage(BattleWeather weather) {
  switch (weather) {
    case BattleWeather::Rain:return "RAIN IS FALLING!";
    case BattleWeather::Sun:return "THE SUNLIGHT IS STRONG!";
    case BattleWeather::Sandstorm:return "A SANDSTORM IS RAGING!";
    case BattleWeather::Hail:return "HAIL IS FALLING!";
    case BattleWeather::HeavyRain:return "HEAVY RAIN IS FALLING!";
    case BattleWeather::HarshSun:return "THE SUNLIGHT IS EXTREMELY HARSH!";
    case BattleWeather::StrongWinds:return "STRONG WINDS ARE BLOWING!";
    case BattleWeather::Clear:break;
  }
  return nullptr;
}

// Switch-in Abilities have already run when this is called. They retain
// priority; ambient weather only fills an otherwise clear local battle.
inline bool applyHomeWeather(BattleState& battle,HomeWeatherKind homeWeather) {
  if (battle.kind == BattleKind::Pvp || battle.kind == BattleKind::None ||
      battle.weather != BattleWeather::Clear) return false;
  const BattleWeather weather=battleWeather(homeWeather);
  if(weather==BattleWeather::Clear)return false;
  battle.weather=weather;
  battle.weatherTurns=kAmbientWeatherTurns;
  return true;
}
}
