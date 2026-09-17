#include "drivers/FireRedFont.h"
#include "drivers/AssetRenderer.h"
#include <algorithm>
#include <cstring>

namespace {
#include "FireRedFontData.inc"

// Render a complete short string as one transparent bitmap transfer. The
// original per-pixel path was visually correct, but a menu with dozens of
// characters issued thousands of ILI9341 window commands. One 320x16 staging
// line keeps the exact FireRed glyphs while making every UI screen faster.
constexpr uint16_t kFontTransparent = 0xF81F;
// Keep the original FireRed glyphs clean. Colour contrast is handled by the
// UI palettes; even a one-pixel shadow looks artificial at this resolution.
constexpr uint16_t kTextRasterWidth = 320U;
constexpr uint16_t kTextRasterHeight = 32U;
constexpr size_t kTextRasterPixels = static_cast<size_t>(kTextRasterWidth) * kTextRasterHeight;
uint16_t textRaster[kTextRasterPixels];

constexpr uint8_t kTextBoxMaximumLines = 24;
constexpr size_t kTextBoxLineCapacity = 96;
constexpr size_t kTextBoxScratchBytes =
    (kTextBoxMaximumLines + 2U) * kTextBoxLineCapacity;
}

void FireRedFont::setTextColor(uint16_t foreground) {
  // TFT_DARKGREY disappears against FireRed's grey, blue and parchment panels
  // on the real display. Keep it for frames and artwork, but render every
  // text request that used it with solid black ink.
  foreground_ = foreground == TFT_DARKGREY ? TFT_BLACK : foreground;
}
void FireRedFont::setTextColor(uint16_t foreground, uint16_t) { setTextColor(foreground); }

uint8_t FireRedFont::glyphCode(char character) {
  if (character >= 'A' && character <= 'Z') return static_cast<uint8_t>(0xBB + character - 'A');
  if (character >= 'a' && character <= 'z') return static_cast<uint8_t>(0xBB + character - 'a');
  if (character >= '0' && character <= '9') return static_cast<uint8_t>(0xA1 + character - '0');
  switch (character) {
    case ' ': return 0x00; case '!': return 0xAB; case '?': return 0xAC; case '.': return 0xAD;
    case '-': return 0xAE; case ',': return 0xB8; case '/': return 0xBA; case ':': return 0xF0;
    case '+': return 0x2E; case '&': return 0x2D; case '=': return 0x35; case '%': return 0x5B;
    case '(': return 0x5C; case ')': return 0x5D; case '$': return 0xB7; case '*': return 0xB9;
    case '\'': return 0xB3; case '\"': return 0xB1;
    default: return 0xAC;
  }
}

// Style 1 is the firmware's readable body font. Style 3 is kept solely for
// small legacy artwork; interactive text must not use it automatically.
int16_t FireRedFont::lineHeight(uint8_t style) { return style >= 4 ? 32 : style == 3 ? 8 : 16; }

int16_t FireRedFont::textWidth(const char* text, uint8_t style) {
  if (!text) return 0;
  int16_t width = 0;
  const bool small = style == 3;
  const int16_t multiplier = style >= 4 ? 2 : 1;
  // Preserve a small natural gap while keeping the original compact GBA look.
  const int16_t tracking = 1;
  for (const char* current = text; *current && *current != '\n'; ++current) {
    const uint8_t code = glyphCode(*current);
    width += (small ? kFireRedSmallWidths[code] : kFireRedNormalWidths[code]) * multiplier;
    if (current[1] && current[1] != '\n') width += tracking;
  }
  return width;
}

void FireRedFont::drawGlyph(uint8_t code, int16_t x, int16_t y, uint8_t style) {
  const bool small = style == 3;
  const uint8_t height = small ? 8 : 16;
  const uint8_t width = small ? 8 : 16;
  const uint8_t scale = style >= 4 ? 2 : 1;
  for (uint8_t row = 0; row < height; ++row) {
    const uint16_t mask = small ? kFireRedSmallGlyphs[code][row] : kFireRedNormalGlyphs[code][row];
    // Sending a separate drawPixel command for every opaque font pixel made
    // normal menu screens take seconds to paint on the shared SPI display.
    // Emit each horizontal run as one address window instead.  This keeps the
    // exact FireRed glyph bitmap, but cuts the command count by an order of
    // magnitude (and makes headings with the 2x style inexpensive too).
    uint8_t column = 0;
    while (column < width) {
      const uint16_t bit = 1U << ((small ? 7 : 15) - column);
      if (!(mask & bit)) { ++column; continue; }
      const uint8_t runStart = column;
      do {
        ++column;
      } while (column < width && (mask & (1U << ((small ? 7 : 15) - column))));
      const uint8_t runLength = static_cast<uint8_t>(column - runStart);
      if (scale == 1)
        display_.drawFastHLine(x + runStart, y + row, runLength, foreground_);
      else
        display_.fillRect(x + runStart * scale, y + row * scale,
                          runLength * scale, scale, foreground_);
    }
  }
}

void FireRedFont::drawString(const char* text, int16_t x, int16_t y, uint8_t style) {
  if (!text) return;
  const int16_t width = textWidth(text, style), height = lineHeight(style);
  switch (datum_) {
    case TC_DATUM: x -= width / 2; break;
    case TR_DATUM: x -= width; break;
    case ML_DATUM: y -= height / 2; break;
    case MC_DATUM: x -= width / 2; y -= height / 2; break;
    case MR_DATUM: x -= width; y -= height / 2; break;
    case BL_DATUM: y -= height; break;
    case BC_DATUM: x -= width / 2; y -= height; break;
    case BR_DATUM: x -= width; y -= height; break;
    default: break;
  }
  const int16_t origin = x;
  const bool small = style == 3;
  const uint8_t scale = style >= 4 ? 2 : 1;
  const int16_t tracking = 1;
  const bool hasLineBreak = std::strchr(text, '\n') != nullptr;

  // Normal strings are at most one TFT line tall. Build their sparse glyph
  // pixels in RAM and push the result in a single transaction. Style-4
  // headings can be 32px tall, so retain the run-based fallback when they do
  // not fit the shared 320x16 raster.
  const int16_t rasterWidth = width;
  const int16_t rasterHeight = height;
  const size_t rasterPixels = width > 0 && height > 0
      ? static_cast<size_t>(rasterWidth) * rasterHeight : 0;
  if (!hasLineBreak && width > 0 && rasterWidth <= kTextRasterWidth &&
      rasterHeight <= kTextRasterHeight && rasterPixels <= kTextRasterPixels) {
    std::fill_n(textRaster, rasterPixels, kFontTransparent);
    int16_t penX = 0;
    for (const char* current = text; *current; ++current) {
      const uint8_t code = glyphCode(*current);
      const uint8_t glyphHeight = small ? 8 : 16;
      const uint8_t glyphWidth = small ? 8 : 16;
      for (uint8_t row = 0; row < glyphHeight; ++row) {
        const uint16_t mask = small ? kFireRedSmallGlyphs[code][row] : kFireRedNormalGlyphs[code][row];
        for (uint8_t column = 0; column < glyphWidth; ++column) {
          if (!(mask & (1U << ((small ? 7 : 15) - column)))) continue;
          const int16_t pixelX = penX + column * scale;
          const int16_t pixelY = row * scale;
          for (uint8_t dy = 0; dy < scale; ++dy)
            for (uint8_t dx = 0; dx < scale; ++dx)
              textRaster[static_cast<size_t>(pixelY + dy) * rasterWidth + pixelX + dx] = foreground_;
        }
      }
      penX += (small ? kFireRedSmallWidths[code] : kFireRedNormalWidths[code]) * scale;
      if (current[1]) penX += tracking;
    }
    int16_t outputX = x, outputY = y;
    int16_t sourceX = 0, sourceY = 0;
    int16_t outputWidth = rasterWidth, outputHeight = rasterHeight;
    if (clipEnabled_) {
      const int16_t clippedLeft = std::max<int16_t>(x, clipLeft_);
      const int16_t clippedTop = std::max<int16_t>(y, clipTop_);
      const int16_t clippedRight = std::min<int16_t>(x + rasterWidth, clipRight_);
      const int16_t clippedBottom = std::min<int16_t>(y + rasterHeight, clipBottom_);
      if (clippedRight <= clippedLeft || clippedBottom <= clippedTop) return;
      outputX = clippedLeft; outputY = clippedTop;
      sourceX = clippedLeft - x; sourceY = clippedTop - y;
      outputWidth = clippedRight - clippedLeft;
      outputHeight = clippedBottom - clippedTop;
      // Pack the clipped rows in place. Destinations always precede their
      // sources, so memmove safely turns the original stride into the narrow
      // contiguous bitmap expected by pushImage without another 20 KiB buffer.
      if (sourceX || sourceY || outputWidth != rasterWidth || outputHeight != rasterHeight) {
        for (int16_t row = 0; row < outputHeight; ++row)
          std::memmove(textRaster + static_cast<size_t>(row) * outputWidth,
                       textRaster + static_cast<size_t>(sourceY + row) * rasterWidth + sourceX,
                       static_cast<size_t>(outputWidth) * sizeof(uint16_t));
      }
    }
    display_.setSwapBytes(true);
    display_.pushImage(outputX, outputY, outputWidth, outputHeight, textRaster, kFontTransparent);
    return;
  }
  // Keep one SPI transaction for the whole string. Starting and ending a
  // transaction for every glyph made menu screens feel unresponsive on the
  // resistive-touch board, especially when several item labels were visible.
  display_.startWrite();
  for (const char* current = text; *current; ++current) {
    if (*current == '\n') { x = origin; y += height; continue; }
    const uint8_t code = glyphCode(*current);
    drawGlyph(code, x, y, style);
    x += (small ? kFireRedSmallWidths[code] : kFireRedNormalWidths[code]) * scale;
    if (current[1] && current[1] != '\n') x += tracking;
  }
  display_.endWrite();
}

void FireRedFont::drawStringFitted(const char* text, int16_t x, int16_t y,
                                   int16_t maxWidth, uint8_t style) {
  if (!text || maxWidth <= 0) return;
  // Establish a hard raster clip matching the content width and the current
  // datum. This guards against malformed/custom glyph bearings as well as
  // arithmetic mistakes in a caller. Nested clips (notably prose boxes) are
  // intersected, never replaced.
  const bool previousClipEnabled = clipEnabled_;
  const int16_t previousLeft = clipLeft_, previousTop = clipTop_;
  const int16_t previousRight = clipRight_, previousBottom = clipBottom_;
  int16_t boundLeft = x;
  if (datum_ == TC_DATUM || datum_ == MC_DATUM || datum_ == BC_DATUM) boundLeft -= maxWidth / 2;
  else if (datum_ == TR_DATUM || datum_ == MR_DATUM || datum_ == BR_DATUM) boundLeft -= maxWidth;
  const int16_t height = lineHeight(style);
  int16_t boundTop = y;
  if (datum_ == ML_DATUM || datum_ == MC_DATUM || datum_ == MR_DATUM) boundTop -= height / 2;
  else if (datum_ == BL_DATUM || datum_ == BC_DATUM || datum_ == BR_DATUM) boundTop -= height;
  clipLeft_ = previousClipEnabled ? std::max<int16_t>(previousLeft, boundLeft) : boundLeft;
  clipTop_ = previousClipEnabled ? std::max<int16_t>(previousTop, boundTop) : boundTop;
  clipRight_ = previousClipEnabled ? std::min<int16_t>(previousRight, boundLeft + maxWidth) : boundLeft + maxWidth;
  clipBottom_ = previousClipEnabled ? std::min<int16_t>(previousBottom, boundTop + height) : boundTop + height;
  clipEnabled_ = true;

  const auto restoreClip = [&]() {
    clipEnabled_ = previousClipEnabled;
    clipLeft_ = previousLeft; clipTop_ = previousTop;
    clipRight_ = previousRight; clipBottom_ = previousBottom;
  };
  if (textWidth(text, style) <= maxWidth) {
    drawString(text, x, y, style);
    restoreClip();
    return;
  }

  // Accept every complete character that fits. Do not spend a large part of
  // a compact FireRed label on an ellipsis: prose wraps/scrolls through
  // drawTextBox(), while a short fixed label is simply clipped at a character
  // boundary and retains more useful information.
  char fitted[96]{};
  size_t accepted = 0;
  while (text[accepted] && accepted + 1U < sizeof(fitted)) {
    fitted[accepted] = text[accepted];
    fitted[accepted + 1U] = 0;
    if (textWidth(fitted, style) > maxWidth) {
      fitted[accepted] = 0;
      break;
    }
    ++accepted;
  }
  if (fitted[0]) drawString(fitted, x, y, style);
  restoreClip();
}

void FireRedFont::drawStringAuto(const char* text, int16_t x, int16_t y, int16_t maxWidth) {
  drawStringFitted(text, x, y, maxWidth, 2);
}

uint8_t FireRedFont::drawTextBox(const char* text, int16_t x, int16_t y, int16_t maxWidth,
                                 int16_t maxHeight, uint8_t scrollLines) {
  if (!text || maxWidth <= 0 || maxHeight <= 0) return 0;
  // Treat the supplied rectangle as the box interior and preserve a minimum
  // one-pixel horizontal safety inset. Vertical placement remains controlled
  // by the caller because a native FireRed row is exactly 16 pixels high.
  // This is deliberately enforced here
  // so every prose panel (Bag, Summary, battle dialogue, Mart, Pokédex) gets
  // the same containment rule instead of relying on screen-specific offsets.
  ++x; maxWidth -= 2;
  if (maxWidth <= 0 || maxHeight <= 0) return 0;
  const bool previousClipEnabled = clipEnabled_;
  const int16_t previousLeft = clipLeft_, previousTop = clipTop_;
  const int16_t previousRight = clipRight_, previousBottom = clipBottom_;
  clipLeft_ = previousClipEnabled ? std::max<int16_t>(previousLeft, x) : x;
  clipTop_ = previousClipEnabled ? std::max<int16_t>(previousTop, y) : y;
  clipRight_ = previousClipEnabled ? std::min<int16_t>(previousRight, x + maxWidth) : x + maxWidth;
  clipBottom_ = previousClipEnabled ? std::min<int16_t>(previousBottom, y + maxHeight) : y + maxHeight;
  clipEnabled_ = true;
  constexpr uint8_t kStyle = 2;
  // Text layout and asset decode are synchronous on the sole UI task. Reuse
  // the renderer's existing transient arena: this removes ~2.5 KiB from the
  // loop stack without growing .bss and starving FatFs of its contiguous
  // registration block during boot.
  size_t scratchCapacity = 0;
  char* const textBoxScratch = static_cast<char*>(AssetRenderer::transientScratch(scratchCapacity));
  if (!textBoxScratch || scratchCapacity < kTextBoxScratchBytes) return 0;
  auto* const textBoxLines = reinterpret_cast<char (*)[kTextBoxLineCapacity]>(textBoxScratch);
  char* const textBoxLine = textBoxScratch + kTextBoxMaximumLines * kTextBoxLineCapacity;
  char* const textBoxFragment = textBoxLine + kTextBoxLineCapacity;
  std::memset(textBoxScratch, 0, kTextBoxScratchBytes);
  uint8_t lineCount = 0;
  const char* cursor = text;
  while (*cursor && lineCount < kTextBoxMaximumLines) {
    while (*cursor == ' ') ++cursor;
    char* const line = textBoxLine;
    line[0] = 0;
    size_t length = 0;
    const char* lineStart = cursor;
    bool lineAlreadyStored = false;
    while (*cursor && *cursor != '\n') {
      const char* wordStart = cursor;
      while (*cursor && *cursor != ' ' && *cursor != '\n') ++cursor;
      const size_t wordLength = static_cast<size_t>(cursor - wordStart);
      const size_t separator = length ? 1U : 0U;
      if (length + separator + wordLength >= kTextBoxLineCapacity) break;
      if (separator) line[length++] = ' ';
      std::memcpy(line + length, wordStart, wordLength); length += wordLength; line[length] = 0;
      if (textWidth(line, kStyle) > maxWidth) {
        // Prose must never lose the tail of a long word. Split it at the
        // largest complete character boundary that fits, then resume from
        // that character on the next logical/scrollable line.
        if (length == wordLength) {
          size_t fittingLength = 0;
          char* const fragment = textBoxFragment;
          fragment[0] = 0;
          while (fittingLength < wordLength && fittingLength + 1U < kTextBoxLineCapacity) {
            fragment[fittingLength] = wordStart[fittingLength];
            fragment[fittingLength + 1U] = 0;
            if (textWidth(fragment, kStyle) > maxWidth) {
              fragment[fittingLength] = 0;
              break;
            }
            ++fittingLength;
          }
          // A box narrower than one glyph is unusable, but still advance one
          // source character to guarantee progress without an infinite loop.
          if (!fittingLength) fittingLength = 1;
          std::strncpy(textBoxLines[lineCount++], fragment, kTextBoxLineCapacity - 1U);
          cursor = wordStart + fittingLength;
          length = 0; line[0] = 0;
          lineAlreadyStored = true;
        } else {
          length -= separator + wordLength;
          line[length] = 0;
          std::strncpy(textBoxLines[lineCount++], line, kTextBoxLineCapacity - 1U);
          lineAlreadyStored = true;
          cursor = wordStart;
        }
        break;
      }
      while (*cursor == ' ') ++cursor;
    }
    // When the last word overflowed, the completed line was stored above and
    // cursor was rewound so that word starts the next line. Storing `line`
    // again here duplicated the first visible row in every narrow scrolling
    // panel (Bag descriptions and Summary memo/ability text).
    if (length && !lineAlreadyStored && lineCount < kTextBoxMaximumLines)
      std::strncpy(textBoxLines[lineCount++], line, kTextBoxLineCapacity - 1U);
    if (*cursor == '\n') ++cursor;
    if (cursor == lineStart && *cursor) ++cursor;
  }
  const uint8_t visibleLines = static_cast<uint8_t>(std::max<int16_t>(1, maxHeight / lineHeight(kStyle)));
  const uint8_t first = std::min<uint8_t>(scrollLines, lineCount ? lineCount - 1U : 0U);
  const uint8_t last = std::min<uint8_t>(lineCount, static_cast<uint8_t>(first + visibleLines));
  for (uint8_t line = first; line < last; ++line)
    drawStringAuto(textBoxLines[line], x, y + static_cast<int16_t>(line - first) * lineHeight(kStyle), maxWidth);
  clipEnabled_ = previousClipEnabled;
  clipLeft_ = previousLeft; clipTop_ = previousTop;
  clipRight_ = previousRight; clipBottom_ = previousBottom;
  return lineCount;
}
