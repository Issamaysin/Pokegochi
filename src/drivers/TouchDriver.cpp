#include "drivers/TouchDriver.h"
#include "config/BoardConfig.h"

namespace { constexpr uint8_t kReadX = 0xD0; constexpr uint8_t kReadY = 0x90; }

void TouchDriver::begin() {
  pinMode(board::kTouchCsPin, OUTPUT); pinMode(board::kTouchSckPin, OUTPUT);
  pinMode(board::kTouchMosiPin, OUTPUT); pinMode(board::kTouchMisoPin, INPUT);
  pinMode(board::kTouchIrqPin, INPUT);
  digitalWrite(board::kTouchCsPin, HIGH); digitalWrite(board::kTouchSckPin, LOW);
}

uint8_t TouchDriver::transfer(uint8_t value) {
  uint8_t result = 0;
  for (int bit = 7; bit >= 0; --bit) {
    digitalWrite(board::kTouchMosiPin, (value >> bit) & 1U);
    digitalWrite(board::kTouchSckPin, HIGH);
    result = static_cast<uint8_t>((result << 1U) | digitalRead(board::kTouchMisoPin));
    digitalWrite(board::kTouchSckPin, LOW);
  }
  return result;
}

uint16_t TouchDriver::readChannel(uint8_t command) {
  transfer(command);
  const uint16_t high = transfer(0); const uint16_t low = transfer(0);
  return static_cast<uint16_t>(((high << 8U) | low) >> 3U) & 0x0FFFU;
}

uint16_t TouchDriver::median3(uint16_t a, uint16_t b, uint16_t c) {
  if (a > b) { const uint16_t t = a; a = b; b = t; }
  if (b > c) { const uint16_t t = b; b = c; c = t; }
  if (a > b) { const uint16_t t = a; a = b; b = t; }
  return b;
}

uint16_t TouchDriver::median5(uint16_t values[5]) {
  for (uint8_t index = 1; index < 5U; ++index) {
    const uint16_t value = values[index];
    uint8_t insertion = index;
    while (insertion && values[insertion - 1U] > value) {
      values[insertion] = values[insertion - 1U];
      --insertion;
    }
    values[insertion] = value;
  }
  return values[2];
}

TouchPoint TouchDriver::read() {
  TouchPoint point;
  if (digitalRead(board::kTouchIrqPin) != LOW) return point;
  digitalWrite(board::kTouchCsPin, LOW);
  uint16_t rawX[5]{}, rawY[5]{};
  const uint8_t sampleCount = fingerMode_ ? 5U : 3U;
  for (uint8_t sample = 0; sample < sampleCount; ++sample) {
    rawX[sample] = readChannel(kReadX);
    rawY[sample] = readChannel(kReadY);
  }
  digitalWrite(board::kTouchCsPin, HIGH);
  point.touched = true;
  point.rawX = fingerMode_ ? median5(rawX) : median3(rawX[0], rawX[1], rawX[2]);
  point.rawY = fingerMode_ ? median5(rawY) : median3(rawY[0], rawY[1], rawY[2]);
  point.x = map(point.rawY, board::kTouchRawYMin, board::kTouchRawYMax, 0, board::kScreenWidth - 1);
  // The physical panel is landscape rotation 1. Raw X increases from the
  // display's top edge to its bottom edge on this board, so it must not be
  // inverted for the on-screen Y coordinate.
  point.y = map(point.rawX, board::kTouchRawXMin, board::kTouchRawXMax, 0, board::kScreenHeight - 1);
  point.x = constrain(point.x, 0, board::kScreenWidth - 1);
  point.y = constrain(point.y, 0, board::kScreenHeight - 1);
  return point;
}
