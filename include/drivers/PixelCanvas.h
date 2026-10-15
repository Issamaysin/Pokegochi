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

  // The ESP32 build keeps the retained stripe in a caller-owned buffer. The
  // normal TFT_eSprite allocator needs one large contiguous heap block; that
  // block competes directly with Sinnoh's largest scene during an animation.
  // A static backing store lets the renderer spend that heap block on the
  // scene while retaining exactly the same PixelCanvas API and compositor.
  ~PixelCanvas() {
    if (staticBuffer_) releaseStaticSprite();
    else if (_created) TFT_eSprite::deleteSprite();
  }

  void* createStaticSprite(uint16_t* storage, int16_t w, int16_t h) {
    if (!storage || w < 1 || h < 1) return nullptr;
    releaseSprite();
    _iwidth = _dwidth = _bitwidth = w;
    _iheight = _dheight = h;
    _img = storage;
    _img8 = _img8_1 = reinterpret_cast<uint8_t*>(storage);
    _img8_2 = _img8_1;
    _img4 = _img8_1;
    _colorMap = nullptr;
    _sw = w;
    _sh = h;
    _scolor = TFT_BLACK;
    _created = true;
    _vpOoB = true;
    staticBuffer_ = true;
    rotation = 0;
    setViewport(0, 0, w, h);
    setPivot(w / 2, h / 2);
    return _img8_1;
  }

  void releaseStaticSprite() {
    if (!staticBuffer_) return;
    _created = false;
    _img = nullptr;
    _img8 = _img8_1 = _img8_2 = _img4 = nullptr;
    _iwidth = _iheight = _dwidth = _dheight = _bitwidth = 0;
    _vpOoB = true;
    staticBuffer_ = false;
  }

  // Keep accidental callers from sending an RTC/static buffer to free().
  void releaseSprite() {
    if (staticBuffer_) releaseStaticSprite();
    else if (_created) TFT_eSprite::deleteSprite();
  }

  void deleteSprite() { releaseSprite(); }

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

  // Rendering fails closed when the backing store or viewport is invalid.
  // Android keeps this canvas alive across scene changes, so a bad sprite
  // must never become a native out-of-bounds write.
  bool bufferReady() const {
    if (!_created || !_img || _bpp != 16 || _iwidth <= 0 || _iheight <= 0 || _vpOoB)
      return false;
    // TFT_eSPI stores _vpW/_vpH as the exclusive right/bottom edges, not
    // viewport width/height. Adding _vpX/_vpY a second time made any stripe
    // whose origin was away from zero clip against the wrong rectangle and
    // let borders/contours bleed into the neighbouring stripe.
    return _vpX >= 0 && _vpY >= 0 && _vpW > _vpX && _vpH > _vpY &&
        _vpW <= _iwidth && _vpH <= _iheight;
  }

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
  void fillSprite(uint32_t color) {
    if (bufferReady()) fillRect(0, 0, width(), height(), color);
  }
  void fillScreen(uint32_t color) {
    if (bufferReady()) fillRect(0, 0, width(), height(), color);
  }
  // The retained renderer occasionally needs the already-composed pixel
  // beneath an indexed overlay.  TFT_eSprite::readPixel() is correct, but it
  // is a non-inlined generic path that repeats the same 16-bit checks for
  // every pixel.  Refresh samples a battler mask for many consecutive
  // frames, so keep the equivalent RGB565 read on this canvas as a tiny hot
  // path. PixelCanvas is permanently allocated at 16 bpp.
  inline uint16_t readPixel565Fast(int32_t x, int32_t y) const {
    if (!bufferReady()) return 0xFFFF;
    x += _xDatum;
    y += _yDatum;
    const int32_t viewportRight = _vpW;
    const int32_t viewportBottom = _vpH;
    if (x < _vpX || y < _vpY || x >= viewportRight || y >= viewportBottom ||
        x < 0 || y < 0 || x >= _iwidth || y >= _iheight)
      return 0xFFFF;
    const uint16_t color = _img[x + y * _iwidth];
    return static_cast<uint16_t>((color >> 8U) | (color << 8U));
  }
  // Resident indexed assets already know the exact stripe/column interval
  // that intersects this canvas.  Give their decoder a direct row pointer so
  // it can translate palette indices straight into the retained RGB565 band.
  // The former path first filled a second 320x4 RGB buffer and then walked it
  // again through pushImage(), doubling the memory traffic of every terrain,
  // scrolling BG and whole-field battle frame on the ESP32.
  inline uint16_t* writableRow565(int32_t x, int32_t y, int32_t width) {
    if (width <= 0 || !bufferReady()) return nullptr;
    x += _xDatum;
    y += _yDatum;
    if (x < _vpX || y < _vpY || x + width > _vpW || y >= _vpH ||
        x < 0 || y < 0 || x + width > _iwidth || y >= _iheight)
      return nullptr;
    return _img + static_cast<size_t>(y) * _iwidth + x;
  }
  static inline uint16_t encoded565(uint16_t color) {
    return static_cast<uint16_t>((color >> 8U) | (color << 8U));
  }
  void drawPixel(int32_t x, int32_t y, uint32_t color) {
    damage(x, y, 1, 1);
    // TFT_eSprite::drawPixel performs the complete viewport/bpp dispatch for
    // every pixel.  Battle palette fades can issue tens of thousands of these
    // calls per second, even though this canvas is permanently RGB565.  Write
    // the same byte-swapped value directly after the two bounds checks.
    if (!bufferReady()) return;
    x += _xDatum;
    y += _yDatum;
    const int32_t viewportRight = _vpW;
    const int32_t viewportBottom = _vpH;
    if (x < _vpX || y < _vpY || x >= viewportRight || y >= viewportBottom ||
        x < 0 || y < 0 || x >= _iwidth || y >= _iheight) return;
    _img[x + y * _iwidth] = static_cast<uint16_t>((color >> 8U) | (color << 8U));
  }
  void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color) {
    if (w <= 0 || w > 2048 || !bufferReady()) return;
    damage(x, y, w, 1);
    const int32_t destinationX = x + _xDatum;
    const int32_t destinationY = y + _yDatum;
    const int32_t left = std::max<int32_t>({destinationX, _vpX, 0});
    const int32_t right = std::min<int32_t>({destinationX + w, _vpW, _iwidth});
    if (destinationY < _vpY || destinationY >= _vpH ||
        destinationY < 0 || destinationY >= _iheight || left >= right) return;
    const uint16_t encoded = static_cast<uint16_t>((color >> 8U) | (color << 8U));
    std::fill(_img + static_cast<size_t>(destinationY) * _iwidth + left,
              _img + static_cast<size_t>(destinationY) * _iwidth + right,
              encoded);
  }
  void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) {
    if (h <= 0 || h > 2048 || !bufferReady()) return;
    damage(x, y, 1, h);
    const int32_t destinationX = x + _xDatum;
    const int32_t destinationY = y + _yDatum;
    const int32_t top = std::max<int32_t>({destinationY, _vpY, 0});
    const int32_t bottom = std::min<int32_t>({destinationY + h, _vpH, _iheight});
    if (destinationX < _vpX || destinationX >= _vpW ||
        destinationX < 0 || destinationX >= _iwidth || top >= bottom) return;
    const uint16_t encoded = static_cast<uint16_t>((color >> 8U) | (color << 8U));
    for (int32_t row = top; row < bottom; ++row)
      _img[static_cast<size_t>(row) * _iwidth + destinationX] = encoded;
  }
  void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048 || !bufferReady()) return;
    damage(x, y, w, h);
    const int32_t destinationX = x + _xDatum;
    const int32_t destinationY = y + _yDatum;
    const int32_t left = std::max<int32_t>({destinationX, _vpX, 0});
    const int32_t right = std::min<int32_t>({destinationX + w, _vpW, _iwidth});
    const int32_t top = std::max<int32_t>({destinationY, _vpY, 0});
    const int32_t bottom = std::min<int32_t>({destinationY + h, _vpH, _iheight});
    if (left >= right || top >= bottom) return;
    const uint16_t encoded = static_cast<uint16_t>((color >> 8U) | (color << 8U));
    for (int32_t row = top; row < bottom; ++row)
      std::fill(_img + static_cast<size_t>(row) * _iwidth + left,
                _img + static_cast<size_t>(row) * _iwidth + right,
                encoded);
  }

  void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048 || !bufferReady()) return;
    damage(x, y, w, h);
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
  }

  void fillCircle(int32_t x0, int32_t y0, int32_t radius, uint32_t color) {
    if (radius < 0 || radius > 512 || !bufferReady()) return;
    damage(x0 - radius, y0 - radius, radius * 2 + 1, radius * 2 + 1);
    const int32_t destinationY = y0 + _yDatum;
    const int32_t firstDy = std::max<int32_t>(
        -radius, std::max<int32_t>(_vpY, 0) - destinationY);
    const int32_t lastDy = std::min<int32_t>(
        radius, std::min<int32_t>(_vpH, _iheight) - destinationY - 1);
    for (int32_t dy = firstDy; dy <= lastDy; ++dy) {
      const int32_t span = static_cast<int32_t>(sqrtf(static_cast<float>(radius * radius - dy * dy)));
      drawFastHLine(x0 - span, y0 + dy, span * 2 + 1, color);
    }
  }

  void drawCircle(int32_t x0, int32_t y0, int32_t radius, uint32_t color) {
    if (radius < 0 || radius > 512 || !bufferReady()) return;
    damage(x0 - radius, y0 - radius, radius * 2 + 1, radius * 2 + 1);
    int32_t x = -radius, y = 0, error = 2 - 2 * radius;
    do {
      drawPixel(x0 - x, y0 + y, color);
      drawPixel(x0 - y, y0 - x, color);
      drawPixel(x0 + x, y0 - y, color);
      drawPixel(x0 + y, y0 + x, color);
      const int32_t saved = error;
      if (saved <= y) error += ++y * 2 + 1;
      if (saved > x || error > y) error += ++x * 2 + 1;
    } while (x < 0);
  }

  void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint32_t color) {
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048 || !bufferReady()) return;
    damage(x, y, w, h);
    radius = std::max<int32_t>(0, std::min<int32_t>(radius, std::min(w, h) / 2));
    if (!radius) { fillRect(x, y, w, h, color); return; }
    // Rasterize every row as one contiguous span. The former implementation
    // filled a central rectangle and then approximated only `radius` corner
    // rows; rounding left one-pixel holes where those two pieces met. Those
    // holes appeared as broken black/gold borders on every screen, including
    // the diagnostic START button.
    const int32_t destinationY = y + _yDatum;
    const int32_t firstRow = std::max<int32_t>(
        0, std::max<int32_t>(_vpY, 0) - destinationY);
    const int32_t lastRow = std::min<int32_t>(
        h, std::min<int32_t>(_vpH, _iheight) - destinationY);
    for (int32_t row = firstRow; row < lastRow; ++row) {
      int32_t inset = 0;
      if (row < radius) {
        const int32_t dy = radius - row;
        inset = radius - static_cast<int32_t>(sqrtf(static_cast<float>(radius * radius - dy * dy)));
      } else if (row >= h - radius) {
        const int32_t dy = row - (h - 1 - radius);
        inset = radius - static_cast<int32_t>(sqrtf(static_cast<float>(radius * radius - dy * dy)));
      }
      inset = std::max<int32_t>(0, std::min<int32_t>(inset, w / 2));
      drawFastHLine(x + inset, y + row, w - inset * 2, color);
    }
  }

  void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint32_t color) {
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048 || !bufferReady()) return;
    damage(x, y, w, h);
    radius = std::max<int32_t>(0, std::min<int32_t>(radius, std::min(w, h) / 2));
    if (!radius) { drawRect(x, y, w, h, color); return; }
    // Use the same row geometry as fillRoundRect and draw the two endpoints
    // of each row. This guarantees a connected one-pixel contour; independent
    // horizontal/vertical/arc primitives used to disagree at their joins.
    const int32_t destinationY = y + _yDatum;
    const int32_t firstRow = std::max<int32_t>(
        0, std::max<int32_t>(_vpY, 0) - destinationY);
    const int32_t lastRow = std::min<int32_t>(
        h, std::min<int32_t>(_vpH, _iheight) - destinationY);
    for (int32_t row = firstRow; row < lastRow; ++row) {
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
      if (row == 0 || row == h - 1) drawFastHLine(left, y + row, right - left + 1, color);
      else {
        drawPixel(left, y + row, color);
        if (right != left) drawPixel(right, y + row, color);
      }
    }
  }

  void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                    int32_t x2, int32_t y2, uint32_t color) {
    if (!bufferReady()) return;
    const int32_t minY = std::min<int32_t>(y0, std::min<int32_t>(y1, y2));
    const int32_t maxY = std::max<int32_t>(y0, std::max<int32_t>(y1, y2));
    if (maxY - minY > 2048) return;
    damage(std::min(x0, std::min(x1, x2)), std::min(y0, std::min(y1, y2)),
           std::max(x0, std::max(x1, x2)) - std::min(x0, std::min(x1, x2)) + 1,
           std::max(y0, std::max(y1, y2)) - std::min(y0, std::min(y1, y2)) + 1);
    const int32_t firstY = std::max<int32_t>(
        minY, std::max<int32_t>(_vpY, 0) - _yDatum);
    const int32_t lastY = std::min<int32_t>(
        maxY, std::min<int32_t>(_vpH, _iheight) - _yDatum - 1);
    for (int32_t y = firstY; y <= lastY; ++y) {
      int32_t intersections[3]{}; uint8_t count = 0;
      const auto cross = [&](int32_t ax, int32_t ay, int32_t bx, int32_t by) {
        if (ay == by || y < std::min(ay, by) || y >= std::max(ay, by) || count >= 3) return;
        intersections[count++] = ax + (y - ay) * (bx - ax) / (by - ay);
      };
      cross(x0, y0, x1, y1); cross(x1, y1, x2, y2); cross(x2, y2, x0, y0);
      if (count < 2) continue;
      if (intersections[0] > intersections[1]) std::swap(intersections[0], intersections[1]);
      drawFastHLine(intersections[0], y, intersections[1] - intersections[0] + 1, color);
    }
  }

  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t* data) {
    damage(x, y, w, h);
    copyImageClipped(x, y, w, h, data, false, 0);
  }
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
    damage(x, y, w, h);
    copyImageClipped(x, y, w, h, data, false, 0);
  }
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t* data, uint16_t transparent) {
    damage(x, y, w, h);
    copyImageClipped(x, y, w, h, data, true, transparent);
  }
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data, uint16_t transparent) {
    pushImage(x, y, w, h, const_cast<uint16_t*>(data), transparent);
  }

 private:
  void copyImageClipped(int32_t x, int32_t y, int32_t w, int32_t h,
                        const uint16_t* data, bool transparent,
                        uint16_t transparentColor) {
    if (!data || w <= 0 || h <= 0 || w > 2048 || h > 2048 || !bufferReady()) return;
    int32_t destinationX = x + _xDatum;
    int32_t destinationY = y + _yDatum;
    int32_t sourceX = 0;
    int32_t sourceY = 0;
    int32_t clippedWidth = w;
    int32_t clippedHeight = h;
    const int32_t clipLeft = std::max<int32_t>(0, _vpX);
    const int32_t clipTop = std::max<int32_t>(0, _vpY);
    const int32_t clipRight = std::min<int32_t>(_iwidth, _vpW);
    const int32_t clipBottom = std::min<int32_t>(_iheight, _vpH);
    if (destinationX < clipLeft) {
      sourceX = clipLeft - destinationX;
      clippedWidth -= sourceX;
      destinationX = clipLeft;
    }
    if (destinationY < clipTop) {
      sourceY = clipTop - destinationY;
      clippedHeight -= sourceY;
      destinationY = clipTop;
    }
    if (destinationX + clippedWidth > clipRight)
      clippedWidth = clipRight - destinationX;
    if (destinationY + clippedHeight > clipBottom)
      clippedHeight = clipBottom - destinationY;
    if (clippedWidth <= 0 || clippedHeight <= 0) return;

    for (int32_t row = 0; row < clippedHeight; ++row) {
      uint16_t* destination = _img +
          static_cast<size_t>(destinationY + row) * _iwidth + destinationX;
      const uint16_t* source = data +
          static_cast<size_t>(sourceY + row) * w + sourceX;
      for (int32_t column = 0; column < clippedWidth; ++column) {
        const uint16_t color = source[column];
        if (!transparent || color != transparentColor)
          destination[column] = static_cast<uint16_t>((color >> 8U) |
                                                       (color << 8U));
      }
    }
  }

  bool staticBuffer_ = false;
  bool trackDamage_ = true;
  int16_t damageLeft_ = 320, damageTop_ = 240, damageRight_ = 0, damageBottom_ = 0;
  int16_t logicalClipLeft_ = 0, logicalClipTop_ = 0;
  int16_t logicalClipRight_ = 320, logicalClipBottom_ = 240;
};
