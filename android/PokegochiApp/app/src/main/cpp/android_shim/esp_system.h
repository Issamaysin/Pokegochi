#pragma once

// The Android build executes the firmware in a normal process.  Keep the
// ESP-IDF include available for shared firmware sources; the small runtime
// surface that the game uses is provided by the Arduino compatibility shim.
#include "Arduino.h"

enum esp_reset_reason_t : uint8_t {
  ESP_RST_UNKNOWN = 0,
  ESP_RST_POWERON = 1,
};

inline esp_reset_reason_t esp_reset_reason() { return ESP_RST_POWERON; }
