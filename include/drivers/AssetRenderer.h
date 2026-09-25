#pragma once
#include <Arduino.h>
#include "drivers/PixelCanvas.h"

// A small cached asset placed in display coordinates.  This is deliberately
// graphics-only so scenes can compose their layers without depending on game
// types.
struct AssetOverlay {
  const char* path = nullptr;
  int16_t x = 0;
  int16_t y = 0;
  // Optional destination window. Zero keeps the asset's natural size for an
  // ordinary sprite, or the caller's complete dirty region for a legacy
  // wrapped plane.
  uint16_t width = 0;
  uint16_t height = 0;
  bool mirrorX = false;
  // Optional retained-BG sampling. Ordinary sprites keep every default.
  // Wrapped source offsets reproduce GBA tilemap scrolling without reading
  // the SD or rebuilding a framebuffer for each emulated tick.
  int16_t sourceOffsetX = 0;
  int16_t sourceOffsetY = 0;
  uint8_t opacity16 = 16;
  uint8_t paletteFirst = 0;
  uint8_t paletteCount = 0;
  uint8_t paletteShift = 0;
  bool wrap = false;
  // A GBA background can wrap on one axis while its scanline window clips
  // the other. Surf is the important case: the 512px water map repeats
  // horizontally, but rows outside its 112px exported crest must extend the
  // edge colour instead of wrapping the wave back from the opposite side.
  bool wrapX = true;
  bool wrapY = true;
  // Destination-to-source nearest-neighbour camera ratio. FireRed's large
  // 240px BG planes are presented at 320px on the TFT, while OBJ sprites
  // remain at their native pixel size. Keeping this ratio on the overlay
  // makes full draws and dirty-region restoration sample the same pixels.
  uint8_t scaleNumerator = 1;
  uint8_t scaleDenominator = 1;
  // Optional virtual source extent for a compact, lossless GBA plane.  The
  // stored image covers the authored region; coordinates in the remaining
  // virtual area resolve to ``virtualFillIndex``. FireRed's 512x256 Fissure
  // BG uses an entirely solid right screenblock, so this saves 32 KiB while
  // preserving its original scroll coordinate space exactly.
  uint16_t virtualSourceWidth = 0;
  uint16_t virtualSourceHeight = 0;
  uint8_t virtualFillIndex = 0xFFU;
};

enum class AssetColorEffect : uint8_t {
  None,
  Blend,
  Grayscale,
  Invert,
};

class AssetRenderer {
 public:
  // Reserve the shared Home/battle scene workspace after persistent storage
  // is ready. Scene changes reuse it and never allocate a large block late.
  static bool begin();
  static bool beginCache();
  // BLE needs a sizeable contiguous block while its controller and host are
  // created. Release the large scene bank for that one transition, then keep
  // a smaller bank while the radio is active. Disabling BLE restores the
  // normal 64 KiB renderer bank.
  static void releaseSceneWorkspaceForRadio();
  static bool restoreSceneWorkspace(bool radioActive);
  // Shared short-lived workspace for synchronous UI algorithms. The renderer
  // and text layout never run concurrently on the Arduino loop task, allowing
  // prose wrapping to reuse the existing render scratch instead of consuming
  // another scarce DRAM block or several KiB of loop stack.
  static void* transientScratch(size_t& capacity);
  // During boot, before any scene has been loaded, persistent-save migration
  // may temporarily reuse this arena instead of allocating a second ~28 KiB
  // record. Runtime commits are streamed and never retain this pointer.
  static void* bootScratch(size_t& capacity);
  static bool draw(PixelCanvas& display, const char* path, int16_t x, int16_t y,
                   bool useCache = true);
  // Draw an already-resident indexed sprite with its columns reversed. Home
  // partners use this to face their horizontal walking direction without a
  // second asset or any extra SD access.
  static bool drawHorizontalMirror(PixelCanvas& display, const char* path,
                                   int16_t x, int16_t y);
  // Draw a resident indexed sprite using FireRed's metallic-shine palette
  // treatment. Transparent pixels remain transparent, while opaque pixels
  // become greyscale and receive a moving diagonal highlight. This is used
  // by HARDEN/IRON DEFENSE and deliberately never reads the SD while a
  // retained frame is being composed.
  static bool drawMetallic(PixelCanvas& display, const char* path,
                           int16_t x, int16_t y, uint8_t phase);
  // Draw a resident indexed sprite around a centre point with a small affine
  // scale and/or palette operation. Battle animation tasks use this for the
  // actual battler (Minimize, Withdraw, Acid Armor, Flash, etc.); it never
  // streams from SD during animation playback.
  static bool drawTransformed(PixelCanvas& display, const char* path,
                              int16_t centerX, int16_t centerY,
                              uint8_t scaleXPercent, uint8_t scaleYPercent,
                              AssetColorEffect colorEffect = AssetColorEffect::None,
                              uint16_t blendColor = 0,
                              uint8_t blendAmount = 0);
  // Draw the SpriteTemplate affine stream. Signed scales preserve FireRed's
  // horizontal/vertical mirroring and angle uses the GBA's 0..255 full turn.
  static bool drawAffine(PixelCanvas& display, const char* path,
                         int16_t centerX, int16_t centerY,
                         int16_t scaleXPercent, int16_t scaleYPercent,
                         uint8_t rotation, uint8_t opacity16 = 16U,
                         AssetColorEffect colorEffect = AssetColorEffect::None,
                         uint16_t blendColor = 0,
                         uint8_t blendAmount = 0);
  // Rotate a contiguous range of an indexed asset's resident palette without
  // modifying the cached image. Aurora Beam uses this for FireRed's original
  // seven-colour ring cycle; transparent and out-of-range entries are kept.
  static bool drawPaletteCycled(PixelCanvas& display, const char* path,
                                int16_t x, int16_t y,
                                uint8_t firstColor, uint8_t colorCount,
                                uint8_t shift);
  // Approximate the GBA OBJ alpha coefficients without a full framebuffer.
  // A stable 4x4 Bayer mask preserves the already-composited pixels below the
  // sprite and, unlike alternating whole frames, never flickers on the TFT.
  // opacity16 follows BLDALPHA's 0..16 coefficient range.
  static bool drawDithered(PixelCanvas& display, const char* path,
                           int16_t x, int16_t y, uint8_t opacity16);
  // Reproduce FireRed's OBJ-window technique: a scrolling BG pattern is
  // visible only where a resident battler sprite is opaque. Curse uses this
  // for the falling white lines without allocating a second framebuffer.
  static bool drawPatternMasked(PixelCanvas& display,
                                const char* maskPath,
                                int16_t maskX, int16_t maskY,
                                const char* patternPath,
                                int16_t sourceOffsetX,
                                int16_t sourceOffsetY,
                                uint8_t opacity16 = 16U);
  // Draw a resident tiled background with GBA-style wrapping, palette
  // rotation and stable ordered alpha. This is the full-arena counterpart of
  // AssetOverlay and never accesses storage.
  static bool drawScrolled(PixelCanvas& display, const char* path,
                           int16_t x, int16_t y, uint16_t width,
                           uint16_t height, int16_t sourceOffsetX,
                           int16_t sourceOffsetY, uint8_t opacity16 = 16,
                           uint8_t paletteFirst = 0,
                           uint8_t paletteCount = 0,
                           uint8_t paletteShift = 0,
                           uint8_t scaleNumerator = 1,
                           uint8_t scaleDenominator = 1,
                           bool wrapX = true,
                           bool wrapY = true,
                           uint16_t virtualSourceWidth = 0,
                           uint16_t virtualSourceHeight = 0,
                           uint8_t virtualFillIndex = 0xFFU);
  // Compose two resident indexed planes into one TFT pass.  The overlay uses
  // the same sampling fields as drawScrolled and is RGB565 alpha-blended
  // against the base with FireRed's 0..16 BLDALPHA coefficient.  This avoids
  // exposing the unblended terrain between two SPI transfers.
  static bool drawPlaneComposite(PixelCanvas& display,
                                 const AssetOverlay& base,
                                 const AssetOverlay& overlay,
                                 int16_t x, int16_t y,
                                 uint16_t width, uint16_t height);
  // Load a small reusable asset without drawing it. This lets interaction
  // screens prepare their effects before the user taps a move.
  static bool preload(const char* path);
  // Reserve the second large scene plane used by FireRed battle backgrounds
  // and whole-field task tilemaps.  The primary battle terrain remains
  // resident at the same time; switching this plane atomically invalidates
  // only small sprites which are reloaded by the battle manifest before the
  // next retained frame. Pass nullptr to release the plane.
  static bool prepareBattlePlane(const char* path);
  // Keep the generated read-only asset archive open for one bounded preload
  // transaction. Every preload() inside the pair is resolved from that one
  // file descriptor, so a multi-frame move does not repeatedly traverse FAT
  // and open dozens of tiny files. Calls may be nested; the outermost end
  // closes the descriptor. Cards made before the archive existed continue to
  // use the individual PKG files transparently.
  static bool beginPreloadBatch();
  // Resolve an archive index entry without decoding it. Priming every cel in
  // a numbered sequence first keeps the following payload reads contiguous
  // instead of bouncing between the archive index and image data.
  static bool primePreload(const char* path);
  static void endPreloadBatch();
  // True only when an asset already lives in the current RAM scene cache.
  // Battle scenes use this to detect that another screen (Bag, Party, etc.)
  // reclaimed their working set before an animation starts.
  static bool isPreloaded(const char* path);
  // Query the exact dimensions of a resident asset without touching the SD.
  // Animation code uses this to centre every original FireRed cel and dirty
  // its complete footprint instead of assuming that all effects are 32x32.
  static bool dimensions(const char* path, uint16_t& width, uint16_t& height);
  // A scene transition may deliberately discard the previous scene's assets.
  // This prevents an animated Home cache from competing with the complete
  // battle terrain on the ESP32's limited internal heap.
  static void clearCache();
  // Drop only small sprites while retaining the current large scene layer.
  // Battle uses this before a new move animation if its frame bank would
  // otherwise fall back to slow SD streaming.
  static void clearSmallCache();
  // Redraw only a cached rectangle from a larger asset. This is used for the
  // animated home playfield so the display never has to stream the whole
  // background on every frame.
  static bool drawRegion(PixelCanvas& display, const char* path, int16_t x, int16_t y,
                         uint16_t sourceX, uint16_t sourceY, uint16_t width,
                         uint16_t height, bool mirrorX = false);
  // Compose a resident base, optional transparent foreground and cached sprites into
  // one TFT transaction.  It is used by the Home animation so the user never
  // sees the intermediate "background -> Pokémon -> foreground" redraw.
  static bool drawCompositeRegion(PixelCanvas& display, const char* basePath,
                                  const char* foregroundPath, int16_t x, int16_t y,
                                  uint16_t sourceX, uint16_t sourceY,
                                  uint16_t width, uint16_t height,
                                  const AssetOverlay* overlays,
                                  uint8_t overlayCount,
                                  AssetColorEffect baseColorEffect = AssetColorEffect::None,
                                  uint16_t baseBlendColor = 0,
                                  uint8_t baseBlendAmount = 0);
  // Discard only the cached Home layers. Used when the player picks another
  // landscape so no fragment from the prior scenery can survive a switch.
  static void invalidateHomeSceneCache();
  // Returns and clears a latched low-level asset I/O failure. The main loop
  // owns SD remounting, so rendering never keeps retrying a dead card while
  // the user is waiting on a frozen screen.
  static bool takeIoFailure();
  // Retained composition is strictly RAM-only. While active, draw() may use
  // resident assets or return false, but it must never open the SD card.
  static void setCompositionActive(bool active);
  static uint32_t sdOpenCount();
  // Call after a successful SD mount. The renderer keeps no SD File open
  // between operations; the archive is opened only inside a bounded preload
  // batch and every decoded scene remains in the compact RAM cache.
  static void storageMounted();
  // Clear storage-side state before SD.end() or a controlled card remount.
  static void storageUnmounted();
  static bool usingArchive();
};
