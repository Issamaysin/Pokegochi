#pragma once

#include <Arduino.h>

namespace board {

constexpr uint8_t kBacklightPin = 21;
constexpr bool kBacklightOnLevel = HIGH;

// P3 input. GPIO35 has no internal pull-up; fit an external 10 kOhm pull-up.
constexpr uint8_t kScreenButtonPin = 35;
constexpr bool kScreenButtonPressedLevel = LOW;

constexpr uint8_t kSdCsPin = 5;
constexpr uint8_t kSdSckPin = 18;
constexpr uint8_t kSdMisoPin = 19;
constexpr uint8_t kSdMosiPin = 23;

constexpr uint8_t kTouchCsPin = 33;
constexpr uint8_t kTouchIrqPin = 36;
constexpr uint8_t kTouchSckPin = 25;
constexpr uint8_t kTouchMisoPin = 39;
constexpr uint8_t kTouchMosiPin = 32;

constexpr uint8_t kDisplayRotation = 1;  // 320 x 240 landscape
constexpr int16_t kScreenWidth = 320;
constexpr int16_t kScreenHeight = 240;

// Provisional XPT2046 values. Calibrate every physical board.
constexpr uint16_t kTouchRawXMin = 250;
constexpr uint16_t kTouchRawXMax = 3850;
constexpr uint16_t kTouchRawYMin = 250;
constexpr uint16_t kTouchRawYMax = 3850;

}  // namespace board

