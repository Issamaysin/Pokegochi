#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

#include "Arduino.h"

constexpr uint16_t TFT_BLACK = 0x0000;
constexpr uint16_t TFT_WHITE = 0xFFFF;
constexpr uint16_t TFT_RED = 0xF800;
constexpr uint16_t TFT_GREEN = 0x07E0;
constexpr uint16_t TFT_BLUE = 0x001F;
constexpr uint16_t TFT_CYAN = 0x07FF;
constexpr uint16_t TFT_YELLOW = 0xFFE0;
constexpr uint16_t TFT_ORANGE = 0xFD20;
constexpr uint16_t TFT_PURPLE = 0x780F;
constexpr uint16_t TFT_MAGENTA = 0xF81F;
constexpr uint16_t TFT_DARKGREEN = 0x03E0;
constexpr uint16_t TFT_DARKCYAN = 0x03EF;
constexpr uint16_t TFT_DARKGREY = 0x7BEF;
constexpr uint16_t TFT_LIGHTGREY = 0xC618;

inline uint16_t androidSwap16(uint16_t value) {
  return static_cast<uint16_t>((value >> 8U) | (value << 8U));
}

class TFT_eSPI {
 public:
  TFT_eSPI() : framebuffer_(320U * 240U, TFT_BLACK) {}

  void init() {}
  void end() {}
  void setRotation(uint8_t) {}
  void invertDisplay(bool) {}
  void setColorDepth(uint8_t) {}
  void setSwapBytes(bool enabled) { swapBytes_ = enabled; }
  bool getSwapBytes() const { return swapBytes_; }
  void startWrite() { writeActive_ = true; }
  void endWrite() { writeActive_ = false; }
  void setAddrWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    windowX_ = x; windowY_ = y; windowWidth_ = width; windowHeight_ = height;
    cursor_ = 0;
  }
  void pushPixels(const uint16_t* pixels, uint32_t length) {
    if (!pixels || !windowWidth_ || !windowHeight_) return;
    const uint32_t windowPixels = static_cast<uint32_t>(windowWidth_) * windowHeight_;
    const uint32_t count = std::min(length, windowPixels - std::min(cursor_, windowPixels));
    uint32_t sourceIndex = 0;
    while (sourceIndex < count) {
      const uint32_t row = cursor_ / windowWidth_;
      const uint32_t column = cursor_ % windowWidth_;
      const uint32_t rowCount = std::min<uint32_t>(count - sourceIndex, windowWidth_ - column);
      if (windowX_ + column < 320U && windowY_ + row < 240U) {
        const uint32_t visible = std::min<uint32_t>(rowCount, 320U - windowX_ - column);
        uint16_t* destination = framebuffer_.data() +
            (windowY_ + row) * 320U + windowX_ + column;
        for (uint32_t offset = 0; offset < visible; ++offset)
          destination[offset] = androidSwap16(pixels[sourceIndex + offset]);
      }
      sourceIndex += rowCount;
      cursor_ += rowCount;
    }
    if (count) ++frameGeneration_;
  }
  void pushImage(int32_t x, int32_t y, int32_t width, int32_t height, const uint16_t* pixels) {
    if (!pixels || width <= 0 || height <= 0) return;
    const int32_t firstColumn = std::max<int32_t>(0, -x);
    const int32_t lastColumn = std::min<int32_t>(width, 320 - x);
    const int32_t firstRow = std::max<int32_t>(0, -y);
    const int32_t lastRow = std::min<int32_t>(height, 240 - y);
    if (firstColumn >= lastColumn || firstRow >= lastRow) return;
    for (int32_t row = firstRow; row < lastRow; ++row) {
      uint16_t* destination = framebuffer_.data() + (y + row) * 320 + x + firstColumn;
      const uint16_t* source = pixels + row * width + firstColumn;
      for (int32_t column = firstColumn; column < lastColumn; ++column)
        destination[column - firstColumn] = androidSwap16(source[column - firstColumn]);
    }
    ++frameGeneration_;
  }
  void writecommand(uint8_t) {}
  const uint16_t* frame() const { return framebuffer_.data(); }
  uint16_t* mutableFrame() { return framebuffer_.data(); }
  uint32_t frameGeneration() const { return frameGeneration_; }

 private:
  std::vector<uint16_t> framebuffer_;
  uint32_t frameGeneration_ = 0;
  bool swapBytes_ = false;
  bool writeActive_ = false;
  uint16_t windowX_ = 0, windowY_ = 0, windowWidth_ = 0, windowHeight_ = 0;
  uint32_t cursor_ = 0;
};

class TFT_eSprite {
 public:
  explicit TFT_eSprite(TFT_eSPI* parent = nullptr) : parent_(parent) {}
  virtual ~TFT_eSprite() { delete[] _img; }

  void* createSprite(int16_t width, int16_t height) {
    delete[] _img;
    _img = nullptr;
    _created = false;
    _vpOoB = true;
    _iwidth = _iheight = _iwidth2 = 0;
    if (width <= 0 || height <= 0 || width > 320 || height > 240) return nullptr;
    _img = new (std::nothrow) uint16_t[static_cast<size_t>(width) * height];
    _iwidth = width; _iheight = height; _vpW = width; _vpH = height;
    _vpX = _vpY = _xDatum = _yDatum = 0; _vpOoB = !_img; _created = _img != nullptr;
    if (_img) std::fill(_img, _img + static_cast<size_t>(width) * height, androidSwap16(TFT_BLACK));
    return _img;
  }
  void deleteSprite() {
    delete[] _img;
    _img = nullptr;
    _created = false;
    _vpOoB = true;
    _iwidth = _iheight = _iwidth2 = 0;
    _vpX = _vpY = _vpW = _vpH = _xDatum = _yDatum = 0;
  }
  bool created() const { return _created; }
  void* getPointer() { return _img; }
  const void* getPointer() const { return _img; }
  int16_t width() const { return _iwidth; }
  int16_t height() const { return _iheight; }
  void setColorDepth(uint8_t depth) { _bpp = depth; }
  void setSwapBytes(bool enabled) { swapBytes_ = enabled; }
  bool getSwapBytes() const { return swapBytes_; }
  void setOrigin(int16_t x, int16_t y) { _xDatum = x; _yDatum = y; }
  void setPivot(int16_t x, int16_t y) { pivotX_ = x; pivotY_ = y; }
  int16_t getOriginX() const { return _xDatum; }
  int16_t getOriginY() const { return _yDatum; }
  void setViewport(int16_t x, int16_t y, int16_t width, int16_t height, bool = false) {
    // Match TFT_eSPI's internal contract used by PixelCanvas: _vpW/_vpH
    // are the exclusive right/bottom edges, not the viewport dimensions.
    // Keeping Android's shim in width/height form clipped every retained
    // frame whose damage rectangle started to the right or below zero.
    _vpX = x; _vpY = y; _vpW = static_cast<int16_t>(x + width);
    _vpH = static_cast<int16_t>(y + height);
    const int32_t right = static_cast<int32_t>(x) + width;
    const int32_t bottom = static_cast<int32_t>(y) + height;
    _vpOoB = !_created || !_img || width <= 0 || height <= 0 || x < 0 || y < 0 ||
        right > _iwidth || bottom > _iheight;
  }
  void resetViewport() {
    _vpX = _vpY = 0; _vpW = _iwidth; _vpH = _iheight;
    _vpOoB = !_created || !_img || _iwidth <= 0 || _iheight <= 0;
  }
  void fillSprite(uint32_t color) { fillRect(0, 0, _iwidth, _iheight, color); }

  void drawPixel(int32_t x, int32_t y, uint32_t color) { writePixel(x, y, static_cast<uint16_t>(color)); }
  void drawFastHLine(int32_t x, int32_t y, int32_t width, uint32_t color) {
    for (int32_t column = 0; column < width; ++column) drawPixel(x + column, y, color);
  }
  void drawFastVLine(int32_t x, int32_t y, int32_t height, uint32_t color) {
    for (int32_t row = 0; row < height; ++row) drawPixel(x, y + row, color);
  }
  void fillRect(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t color) {
    if (width <= 0 || height <= 0) return;
    for (int32_t row = 0; row < height; ++row) drawFastHLine(x, y + row, width, color);
  }
  void drawRect(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t color) {
    if (width <= 0 || height <= 0) return;
    drawFastHLine(x, y, width, color); drawFastHLine(x, y + height - 1, width, color);
    drawFastVLine(x, y, height, color); drawFastVLine(x + width - 1, y, height, color);
  }
  void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    int32_t dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int32_t dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int32_t error = dx + dy;
    while (true) {
      drawPixel(x0, y0, color);
      if (x0 == x1 && y0 == y1) break;
      const int32_t twice = 2 * error;
      if (twice >= dy) { error += dy; x0 += sx; }
      if (twice <= dx) { error += dx; y0 += sy; }
    }
  }
  void fillCircle(int32_t x0, int32_t y0, int32_t radius, uint32_t color) {
    for (int32_t y = -radius; y <= radius; ++y) {
      const int32_t span = static_cast<int32_t>(std::sqrt(static_cast<float>(radius * radius - y * y)));
      drawFastHLine(x0 - span, y0 + y, span * 2 + 1, color);
    }
  }
  void drawCircle(int32_t x0, int32_t y0, int32_t radius, uint32_t color) {
    int32_t x = -radius, y = 0, error = 2 - 2 * radius;
    do {
      drawPixel(x0 - x, y0 + y, color); drawPixel(x0 - y, y0 - x, color);
      drawPixel(x0 + x, y0 - y, color); drawPixel(x0 + y, y0 + x, color);
      const int32_t saved = error;
      if (saved <= y) error += ++y * 2 + 1;
      if (saved > x || error > y) error += ++x * 2 + 1;
    } while (x < 0);
  }
  void fillRoundRect(int32_t x, int32_t y, int32_t width, int32_t height,
                     int32_t radius, uint32_t color) {
    radius = std::max<int32_t>(0, std::min<int32_t>(radius, std::min(width, height) / 2));
    for (int32_t row = 0; row < height; ++row) {
      int32_t inset = 0;
      if (row < radius) { const int32_t dy = radius - row; inset = radius - static_cast<int32_t>(std::sqrt(static_cast<float>(radius * radius - dy * dy))); }
      else if (row >= height - radius) { const int32_t dy = row - (height - 1 - radius); inset = radius - static_cast<int32_t>(std::sqrt(static_cast<float>(radius * radius - dy * dy))); }
      drawFastHLine(x + inset, y + row, width - inset * 2, color);
    }
  }
  void drawRoundRect(int32_t x, int32_t y, int32_t width, int32_t height,
                     int32_t radius, uint32_t color) {
    if (width <= 0 || height <= 0) return;
    radius = std::max<int32_t>(0, std::min<int32_t>(radius, std::min(width, height) / 2));
    for (int32_t row = 0; row < height; ++row) {
      int32_t inset = 0;
      if (row < radius) { const int32_t dy = radius - row; inset = radius - static_cast<int32_t>(std::sqrt(static_cast<float>(radius * radius - dy * dy))); }
      else if (row >= height - radius) { const int32_t dy = row - (height - 1 - radius); inset = radius - static_cast<int32_t>(std::sqrt(static_cast<float>(radius * radius - dy * dy))); }
      const int32_t left = x + inset, right = x + width - 1 - inset;
      if (row == 0 || row == height - 1) drawFastHLine(left, y + row, right - left + 1, color);
      else { drawPixel(left, y + row, color); drawPixel(right, y + row, color); }
    }
  }
  void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                    int32_t x2, int32_t y2, uint32_t color) {
    const int32_t minY = std::min({y0, y1, y2}), maxY = std::max({y0, y1, y2});
    auto edge = [](int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t y) {
      return by == ay ? ax : ax + (y - ay) * (bx - ax) / (by - ay);
    };
    for (int32_t y = minY; y <= maxY; ++y) {
      int32_t values[3]{}; uint8_t count = 0;
      const auto add = [&](int32_t ax, int32_t ay, int32_t bx, int32_t by) {
        if (ay == by || y < std::min(ay, by) || y >= std::max(ay, by) || count >= 3) return;
        values[count++] = edge(ax, ay, bx, by, y);
      };
      add(x0, y0, x1, y1); add(x1, y1, x2, y2); add(x2, y2, x0, y0);
      if (count >= 2) { if (values[0] > values[1]) std::swap(values[0], values[1]); drawFastHLine(values[0], y, values[1] - values[0] + 1, color); }
    }
  }
  void pushImage(int32_t x, int32_t y, int32_t width, int32_t height, uint16_t* pixels) {
    if (!pixels || width <= 0 || height <= 0 || width > 2048 || height > 2048) return;
    for (int32_t row = 0; row < height; ++row)
      for (int32_t column = 0; column < width; ++column)
        writePixel(x + column, y + row, pixels[row * width + column]);
  }
  void pushImage(int32_t x, int32_t y, int32_t width, int32_t height,
                 const uint16_t* pixels) { pushImage(x, y, width, height, const_cast<uint16_t*>(pixels)); }
  void pushImage(int32_t x, int32_t y, int32_t width, int32_t height,
                 uint16_t* pixels, uint16_t transparent) {
    if (!pixels || width <= 0 || height <= 0 || width > 2048 || height > 2048) return;
    for (int32_t row = 0; row < height; ++row)
      for (int32_t column = 0; column < width; ++column)
        if (pixels[row * width + column] != transparent) writePixel(x + column, y + row, pixels[row * width + column]);
  }

  protected:
  void writePixel(int32_t x, int32_t y, uint16_t color) {
    x += _xDatum; y += _yDatum;
    const int32_t viewportRight = _vpW;
    const int32_t viewportBottom = _vpH;
    if (!_created || !_img || _vpOoB || x < _vpX || y < _vpY ||
        x >= viewportRight || y >= viewportBottom || x < 0 || y < 0 ||
        x >= _iwidth || y >= _iheight) return;
    _img[y * _iwidth + x] = androidSwap16(color);
  }

  TFT_eSPI* parent_ = nullptr;
  bool _created = false;
  bool _vpOoB = true;
  uint8_t _bpp = 16;
  uint16_t* _img = nullptr;
  int16_t _iwidth = 0, _iheight = 0, _iwidth2 = 0;
  // These aliases are part of TFT_eSPI's sprite layout and are accessed by
  // PixelCanvas when it binds the caller-owned retained framebuffer. Keep
  // them in the Android shim so the same canvas implementation is compiled
  // for both the TFT and mobile targets.
  int16_t _dwidth = 0, _dheight = 0, _bitwidth = 0;
  uint8_t* _img8 = nullptr;
  uint8_t* _img8_1 = nullptr;
  uint8_t* _img8_2 = nullptr;
  uint8_t* _img4 = nullptr;
  uint16_t* _colorMap = nullptr;
  int16_t _sw = 0, _sh = 0, _scolor = 0;
  uint8_t rotation = 0;
  int16_t _vpX = 0, _vpY = 0, _vpW = 0, _vpH = 0;
  int16_t _xDatum = 0, _yDatum = 0;
  int16_t pivotX_ = 0, pivotY_ = 0;
  bool swapBytes_ = false;
};
