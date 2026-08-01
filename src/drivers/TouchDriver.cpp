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

TouchPoint TouchDriver::read() {
  TouchPoint point;
  if (digitalRead(board::kTouchIrqPin) != LOW) return point;
  digitalWrite(board::kTouchCsPin, LOW);
  const uint16_t x1 = readChannel(kReadX), y1 = readChannel(kReadY);
  const uint16_t x2 = readChannel(kReadX), y2 = readChannel(kReadY);
  const uint16_t x3 = readChannel(kReadX), y3 = readChannel(kReadY);
  digitalWrite(board::kTouchCsPin, HIGH);
  point.touched = true; point.rawX = median3(x1, x2, x3); point.rawY = median3(y1, y2, y3);
  point.x = map(point.rawY, board::kTouchRawYMin, board::kTouchRawYMax, 0, board::kScreenWidth - 1);
  point.y = map(point.rawX, board::kTouchRawXMin, board::kTouchRawXMax, board::kScreenHeight - 1, 0);
  point.x = constrain(point.x, 0, board::kScreenWidth - 1);
  point.y = constrain(point.y, 0, board::kScreenHeight - 1);
  return point;
}

