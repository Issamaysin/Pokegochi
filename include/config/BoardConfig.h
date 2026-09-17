#pragma once

#include <Arduino.h>

namespace board {

constexpr uint8_t kBacklightPin = 21;
constexpr bool kBacklightOnLevel = HIGH;

constexpr uint8_t kSdCsPin = 5;
constexpr uint8_t kSdSckPin = 18;
constexpr uint8_t kSdMisoPin = 19;
constexpr uint8_t kSdMosiPin = 23;

constexpr uint8_t kTouchCsPin = 33;
constexpr uint8_t kTouchIrqPin = 36;
constexpr uint8_t kTouchSckPin = 25;
constexpr uint8_t kTouchMisoPin = 39;
constexpr uint8_t kTouchMosiPin = 32;

// Common ESP32-2432S028R (CYD) onboard RGB LED wiring. Confirm polarity on the
// physical board during bring-up; these pins are otherwise unused by Pokegochi.
constexpr uint8_t kRgbLedRedPin = 4;
constexpr uint8_t kRgbLedGreenPin = 16;
constexpr uint8_t kRgbLedBluePin = 17;
constexpr bool kRgbLedActiveLow = true;

// Battery sense input on the P3 header. The cell must reach this pin through
// a 100k/100k divider; never connect a Li-ion cell directly to a GPIO.
constexpr uint8_t kBatterySensePin = 35;
constexpr uint32_t kBatteryDividerTopOhms = 100000U;
constexpr uint32_t kBatteryDividerBottomOhms = 100000U;

constexpr uint8_t kDisplayRotation = 1;  // 320 x 240 landscape
constexpr int16_t kScreenWidth = 320;
constexpr int16_t kScreenHeight = 240;

// Provisional XPT2046 values. Calibrate every physical board.
constexpr uint16_t kTouchRawXMin = 250;
constexpr uint16_t kTouchRawXMax = 3850;
constexpr uint16_t kTouchRawYMin = 250;
constexpr uint16_t kTouchRawYMax = 3850;

}  // namespace board
