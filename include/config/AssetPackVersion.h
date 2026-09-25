#pragma once

#include <cstdint>

// Single source of truth shared with scripts/build_sd_asset_pack.py. Bumping
// the SD format here changes both the generated marker and the firmware boot
// requirement, preventing a freshly written card from being rejected by an
// older duplicated literal.
constexpr uint16_t kRequiredAssetPackVersion = 28;
