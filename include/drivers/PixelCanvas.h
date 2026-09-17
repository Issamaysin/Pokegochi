#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <algorithm>
#include <cmath>

// A 16-bit, 320px-wide scratch canvas used by the retained presenter.  The
// game paints into this RAM canvas, never directly into the ILI9341.  Its
// owner compares every 16x16 tile with the previously presented tile and
// transfers only tiles whose pixels changed.
//
// TFT_eSprite deliberately does not override several composite primitives in
// TFT_eSPI.  Those inherited primitives would write to the physical display,
// so PixelCanvas provides RAM-only versions for every primitive used by the
// UI.  `startWrite/endWrite` are intentionally no-ops: SPI ownership belongs
// exclusively to the presenter when it flushes a changed tile.
class PixelCanvas final : public TFT_eSprite {
 public:
  explicit PixelCanvas(TFT_eSPI* panel) : TFT_eSprite(panel) {}

  void setDamageTracking(bool enabled) { trackDamage_ = enabled; }
  void setLogicalClip(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    logicalClipLeft_ = std::clamp<int16_t>(left, 0, 320);
    logicalClipTop_ = std::clamp<int16_t>(top, 0, 240);
    logicalClipRight_ = std::clamp<int16_t>(right, logicalClipLeft_, 320);
    logicalClipBottom_ = std::clamp<int16_t>(bottom, logicalClipTop_, 240);
  }
  void resetLogicalClip() { setLogicalClip(0, 0, 320, 240); }
  int16_t logicalClipLeft() const { return logicalClipLeft_; }
  int16_t logicalClipTop() const { return logicalClipTop_; }
  int16_t logicalClipRight() const { return logicalClipRight_; }
  int16_t logicalClipBottom() const { return logicalClipBottom_; }
  void resetDamage() { damageLeft_ = 320; damageTop_ = 240; damageRight_ = damageBottom_ = 0; }
  bool hasDamage() const { return damageRight_ > damageLeft_ && damageBottom_ > damageTop_; }
  int16_t damageLeft() const { return damageLeft_; }
  int16_t damageTop() const { return damageTop_; }
  int16_t damageRight() const { return damageRight_; }
  int16_t damageBottom() const { return damageBottom_; }

  void damage(int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!trackDamage_ || w <= 0 || h <= 0) return;
    x += getOriginX(); y += getOriginY();
    damageLeft_ = std::min<int16_t>(damageLeft_, std::max<int32_t>(0, x));
    damageTop_ = std::min<int16_t>(damageTop_, std::max<int32_t>(0, y));
    damageRight_ = std::max<int16_t>(damageRight_, std::min<int32_t>(320, x + w));
    damageBottom_ = std::max<int16_t>(damageBottom_, std::min<int32_t>(240, y + h));
  }

  void startWrite() {}
  void endWrite() {}
  void fillScreen(uint32_t color) { fillRect(0, 0, width(), height(), color); }
  void drawPixel(int32_t x, int32_t y, uint32_t color) {
    damage(x, y, 1, 1);
    // TFT_eSprite::drawPixel performs the complete viewport/bpp dispatch for
    // every pixel.  Battle palette fades can issue tens of thousands of these
    // calls per second, even though this canvas is permanently RGB565.  Write
    // the same byte-swapped value directly after the two bounds checks.
    if (!_created || _vpOoB || _bpp != 16) return;
    x += _xDatum;
    y += _yDatum;
    if (x < _vpX || y < _vpY || x >= _vpW || y >= _vpH) return;
    _img[x + y * _iwidth] = static_cast<uint16_t>((color >> 8U) | (color << 8U));
  }
  void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color) {
    damage(x, y, w, 1); TFT_eSprite::drawFastHLine(x, y, w, color);
  }
  void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) {
    damage(x, y, 1, h); TFT_eSprite::drawFastVLine(x, y, h, color);
  }
  void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    damage(x, y, w, h); TFT_eSprite::fillRect(x, y, w, h, color);
  }

  void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    damage(x, y, w, h);
    TFT_eSprite::drawFastHLine(x, y, w, color);
    TFT_eSprite::drawFastHLine(x, y + h - 1, w, color);
    TFT_eSprite::drawFastVLine(x, y, h, color);
    TFT_eSprite::drawFastVLine(x + w - 1, y, h, color);
  }

  void fillCircle(int32_t x0, int32_t y0, int32_t radius, uint32_t color) {
    if (radius < 0) return;
    damage(x0 - radius, y0 - radius, radius * 2 + 1, radius * 2 + 1);
    for (int32_t dy = -radius; dy <= radius; ++dy) {
      const int32_t span = static_cast<int32_t>(sqrtf(static_cast<float>(radius * radius - dy * dy)));
      TFT_eSprite::drawFastHLine(x0 - span, y0 + dy, span * 2 + 1, color);
    }
  }

  void drawCircle(int32_t x0, int32_t y0, int32_t radius, uint32_t color) {
    if (radius < 0) return;
    damage(x0 - radius, y0 - radius, radius * 2 + 1, radius * 2 + 1);
    int32_t x = -radius, y = 0, error = 2 - 2 * radius;
    do {
      TFT_eSprite::drawPixel(x0 - x, y0 + y, color);
      TFT_eSprite::drawPixel(x0 - y, y0 - x, color);
      TFT_eSprite::drawPixel(x0 + x, y0 - y, color);
      TFT_eSprite::drawPixel(x0 + y, y0 + x, color);
      const int32_t saved = error;
      if (saved <= y) error += ++y * 2 + 1;
      if (saved > x || error > y) error += ++x * 2 + 1;
    } while (x < 0);
  }

  void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    damage(x, y, w, h);
    radius = std::max<int32_t>(0, std::min<int32_t>(radius, std::min(w, h) / 2));
    if (!radius) { TFT_eSprite::fillRect(x, y, w, h, color); return; }
    // Rasterize every row as one contiguous span. The former implementation
    // filled a central rectangle and then approximated only `radius` corner
    // rows; rounding left one-pixel holes where those two pieces met. Those
    // holes appeared as broken black/gold borders on every screen, including
    // the diagnostic START button.
    for (int32_t row = 0; row < h; ++row) {
      int32_t inset = 0;
      if (row < radius) {
        const int32_t dy = radius - row;
        inset = radius - static_cast<int32_t>(sqrtf(static_cast<float>(radius * radius - dy * dy)));
      } else if (row >= h - radius) {
        const int32_t dy = row - (h - 1 - radius);
        inset = radius - static_cast<int32_t>(sqrtf(static_cast<float>(radius * radius - dy * dy)));
      }
      inset = std::max<int32_t>(0, std::min<int32_t>(inset, w / 2));
      TFT_eSprite::drawFastHLine(x + inset, y + row, w - inset * 2, color);
    }
  }

  void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    damage(x, y, w, h);
    radius = std::max<int32_t>(0, std::min<int32_t>(radius, std::min(w, h) / 2));
    if (!radius) { drawRect(x, y, w, h, color); return; }
    // Use the same row geometry as fillRoundRect and draw the two endpoints
    // of each row. This guarantees a connected one-pixel contour; independent
    // horizontal/vertical/arc primitives used to disagree at their joins.
    for (int32_t row = 0; row < h; ++row) {
      int32_t inset = 0;
      if (row < radius) {
        const int32_t dy = radius - row;
        inset = radius - static_cast<int32_t>(sqrtf(static_cast<float>(radius * radius - dy * dy)));
      } else if (row >= h - radius) {
        const int32_t dy = row - (h - 1 - radius);
        inset = radius - static_cast<int32_t>(sqrtf(static_cast<float>(radius * radius - dy * dy)));
      }
      inset = std::max<int32_t>(0, std::min<int32_t>(inset, w / 2));
      const int32_t left = x + inset, right = x + w - 1 - inset;
      if (row == 0 || row == h - 1) TFT_eSprite::drawFastHLine(left, y + row, right - left + 1, color);
      else {
        TFT_eSprite::drawPixel(left, y + row, color);
        if (right != left) TFT_eSprite::drawPixel(right, y + row, color);
      }
    }
  }

  void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                    int32_t x2, int32_t y2, uint32_t color) {
    damage(std::min(x0, std::min(x1, x2)), std::min(y0, std::min(y1, y2)),
           std::max(x0, std::max(x1, x2)) - std::min(x0, std::min(x1, x2)) + 1,
           std::max(y0, std::max(y1, y2)) - std::min(y0, std::min(y1, y2)) + 1);
    const int32_t minY = std::min<int32_t>(y0, std::min<int32_t>(y1, y2));
    const int32_t maxY = std::max<int32_t>(y0, std::max<int32_t>(y1, y2));
    for (int32_t y = minY; y <= maxY; ++y) {
      int32_t intersections[3]{}; uint8_t count = 0;
      const auto cross = [&](int32_t ax, int32_t ay, int32_t bx, int32_t by) {
        if (ay == by || y < std::min(ay, by) || y >= std::max(ay, by) || count >= 3) return;
        intersections[count++] = ax + (y - ay) * (bx - ax) / (by - ay);
      };
      cross(x0, y0, x1, y1); cross(x1, y1, x2, y2); cross(x2, y2, x0, y0);
      if (count < 2) continue;
      if (intersections[0] > intersections[1]) std::swap(intersections[0], intersections[1]);
      TFT_eSprite::drawFastHLine(intersections[0], y, intersections[1] - intersections[0] + 1, color);
    }
  }

  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t* data) {
    damage(x, y, w, h);
    TFT_eSprite::pushImage(x, y, w, h, data);
  }
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
    damage(x, y, w, h);
    TFT_eSprite::pushImage(x, y, w, h, data);
  }
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t* data, uint16_t transparent) {
    if (!data || w <= 0 || h <= 0) return;
    damage(x, y, w, h);
    if (!_created || _vpOoB || _bpp != 16) return;

    // This is the hot path for every Pokemon and battle OBJ.  The former
    // implementation called TFT_eSprite::drawPixel once per source pixel;
    // each call repeated datum, viewport and colour-depth checks.  Clip the
    // rectangle once, then blend directly into the 16-bit backing store.
    int32_t destinationX = x + _xDatum;
    int32_t destinationY = y + _yDatum;
    int32_t sourceX = 0;
    int32_t sourceY = 0;
    int32_t clippedWidth = w;
    int32_t clippedHeight = h;
    if (destinationX < _vpX) {
      sourceX = _vpX - destinationX;
      clippedWidth -= sourceX;
      destinationX = _vpX;
    }
    if (destinationY < _vpY) {
      sourceY = _vpY - destinationY;
      clippedHeight -= sourceY;
      destinationY = _vpY;
    }
    if (destinationX + clippedWidth > _vpW)
      clippedWidth = _vpW - destinationX;
    if (destinationY + clippedHeight > _vpH)
      clippedHeight = _vpH - destinationY;
    if (clippedWidth <= 0 || clippedHeight <= 0) return;

    for (int32_t row = 0; row < clippedHeight; ++row) {
      uint16_t* destination = _img +
          static_cast<size_t>(destinationY + row) * _iwidth + destinationX;
      const uint16_t* source = data +
          static_cast<size_t>(sourceY + row) * w + sourceX;
      for (int32_t column = 0; column < clippedWidth; ++column) {
        const uint16_t color = source[column];
        if (color != transparent)
          destination[column] = static_cast<uint16_t>((color >> 8U) |
                                                       (color << 8U));
      }
    }
  }
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data, uint16_t transparent) {
    pushImage(x, y, w, h, const_cast<uint16_t*>(data), transparent);
  }

 private:
  bool trackDamage_ = true;
  int16_t damageLeft_ = 320, damageTop_ = 240, damageRight_ = 0, damageBottom_ = 0;
  int16_t logicalClipLeft_ = 0, logicalClipTop_ = 0;
  int16_t logicalClipRight_ = 320, logicalClipBottom_ = 240;
};
