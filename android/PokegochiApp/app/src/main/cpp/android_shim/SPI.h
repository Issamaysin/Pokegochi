#pragma once
#include <cstdint>

constexpr uint8_t VSPI = 3;
class SPIClass {
 public:
  explicit SPIClass(uint8_t = VSPI) {}
  void begin() {}
  void begin(int, int, int, int) {}
  void end() {}
};
