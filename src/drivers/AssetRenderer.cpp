#include "drivers/AssetRenderer.h"
#include <SD.h>
#include <cstring>
#include <new>

namespace {
struct AssetHeader { char magic[4]; uint16_t width; uint16_t height; uint16_t transparent; } __attribute__((packed));
}

bool AssetRenderer::draw(TFT_eSPI& display, const char* path, int16_t x, int16_t y) {
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  AssetHeader header{};
  if (file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header) ||
      memcmp(header.magic, "PKG1", 4) != 0 || header.width == 0 || header.width > 320 || header.height > 320) {
    file.close(); return false;
  }
  uint16_t* row = new (std::nothrow) uint16_t[header.width];
  if (!row) { file.close(); return false; }
  display.setSwapBytes(false);
  bool okay = true;
  for (uint16_t line = 0; line < header.height; ++line) {
    if (file.read(reinterpret_cast<uint8_t*>(row), header.width * sizeof(uint16_t)) != header.width * sizeof(uint16_t)) {
      okay = false; break;
    }
    display.pushImage(x, y + line, header.width, 1, row, header.transparent);
  }
  delete[] row; file.close(); return okay;
}
