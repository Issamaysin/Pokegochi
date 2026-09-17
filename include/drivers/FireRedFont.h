#pragma once
#include <Arduino.h>
#include "drivers/PixelCanvas.h"

// Pixel renderer backed by the original FireRed Latin glyph atlas. The style
// argument mirrors the TFT_eSPI fonts used by the existing layouts: 1=readable
// body, 2=normal and 4=double-height heading. Style 3 remains available for
// legacy art only; interface text must never shrink to it.
class FireRedFont {
 public:
  explicit FireRedFont(PixelCanvas& display) : display_(display) {}
  void setTextColor(uint16_t foreground);
  void setTextColor(uint16_t foreground, uint16_t background);
  void setTextDatum(uint8_t datum) { datum_ = datum; }
  void drawString(const char* text, int16_t x, int16_t y, uint8_t style = 2);
  // Draw a single line that is mathematically guaranteed not to exceed the
  // supplied content width. Unlike the legacy Auto helper this also supports
  // compact labels and large headings without changing their intended style.
  void drawStringFitted(const char* text, int16_t x, int16_t y,
                        int16_t maxWidth, uint8_t style = 2);
  // Always use readable 16px FireRed glyphs. Short labels retain every whole
  // character that fits; prose belongs in drawTextBox so it wraps and scrolls.
  void drawStringAuto(const char* text, int16_t x, int16_t y, int16_t maxWidth);
  // Word-wrap within a strict rectangular text area. `scrollLines` selects
  // the first logical line to render and the return value is the number of
  // logical lines, so screens can expose a down-arrow only when needed.
  uint8_t drawTextBox(const char* text, int16_t x, int16_t y, int16_t maxWidth,
                      int16_t maxHeight, uint8_t scrollLines = 0);

 private:
  static uint8_t glyphCode(char character);
  static int16_t textWidth(const char* text, uint8_t style);
  static int16_t lineHeight(uint8_t style);
  void drawGlyph(uint8_t code, int16_t x, int16_t y, uint8_t style);

  PixelCanvas& display_;
  uint16_t foreground_ = TFT_WHITE;
  uint8_t datum_ = TL_DATUM;
  bool clipEnabled_ = false;
  int16_t clipLeft_ = 0;
  int16_t clipTop_ = 0;
  int16_t clipRight_ = 320;
  int16_t clipBottom_ = 240;
};
