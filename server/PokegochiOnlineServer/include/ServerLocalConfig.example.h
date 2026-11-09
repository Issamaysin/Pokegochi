#pragma once

// Copy this file to ServerLocalConfig.h and fill in the private 2.4 GHz
// network credentials. ServerLocalConfig.h is deliberately ignored by Git.
namespace PokegochiServerLocalConfig {
inline constexpr char kWifiSsid[] = "YOUR_WIFI_2G";
inline constexpr char kWifiPassword[] = "YOUR_PASSWORD";
}  // namespace PokegochiServerLocalConfig
