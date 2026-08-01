#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

class AssetRenderer {
 public:
  static bool draw(TFT_eSPI& display, const char* path, int16_t x, int16_t y);
};
