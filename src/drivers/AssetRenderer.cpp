#include "drivers/AssetRenderer.h"

#include "config/AssetPackVersion.h"

#include <SD.h>
#include <algorithm>
#include <cstring>
#include <new>
#include <esp_heap_caps.h>

namespace {

// PKG2 is deliberately a very small indexed-image container.  FireRed art is
// palette based, so keeping the original pixels as RGB565 on the card (and
// expanding every frame) wastes both SPI bandwidth and RAM.
//
//   PKG1: [magic,width,height,transparent][RGB565 pixels]        (legacy)
//   PKG2: [magic,width,height,transparent][bits,palette count]
//         [RGB565 palette][4-, 6- or 8-bit palette indices]
//
// The firmware still renders PKG1 files directly.  It therefore fails
// gracefully on an older card, but all scene caching is designed for PKG2.
struct AssetHeader {
  char magic[4];
  uint16_t width;
  uint16_t height;
  uint16_t transparent;
} __attribute__((packed));

struct Pkg2Tail {
  uint8_t bitsPerPixel;
  uint8_t paletteCount;
} __attribute__((packed));

struct AssetInfo {
  AssetHeader header{};
  uint8_t bitsPerPixel = 0;  // 16 for legacy RGB565
  uint16_t paletteCount = 0;
  uint32_t paletteOffset = 0;
  uint32_t pixelOffset = 0;
  size_t pixelBytes = 0;
};

struct IndexedImage {
  bool valid = false;
  AssetHeader header{};
  uint8_t bitsPerPixel = 0;
  uint16_t paletteCount = 0;
  uint16_t* palette = nullptr;
  uint8_t* indices = nullptr;
  size_t indexBytes = 0;
};

struct CacheEntry {
  bool valid = false;
  char path[112]{};
  IndexedImage image{};
  uint32_t stamp = 0;
};

class AssetFile {
 public:
  AssetFile() = default;
  explicit AssetFile(File source) : source_(source), valid_(static_cast<bool>(source_)) {}
  AssetFile(File* source, uint32_t base, uint32_t length)
      : borrowed_(source), base_(base), length_(length), valid_(source && *source) {}

  explicit operator bool() const {
    return valid_ && (borrowed_ ? static_cast<bool>(*borrowed_) : static_cast<bool>(source_));
  }
  size_t read(uint8_t* destination, size_t length) {
    if (!*this) return 0;
    File& source = borrowed_ ? *borrowed_ : source_;
    const uint32_t absolute = source.position();
    const uint32_t relative = absolute >= base_ ? absolute - base_ : length_;
    if (relative >= size()) return 0;
    return source.read(destination, std::min<size_t>(length, size() - relative));
  }
  bool seek(uint32_t offset) {
    if (!*this || offset > size()) return false;
    File& source = borrowed_ ? *borrowed_ : source_;
    return source.seek(base_ + offset);
  }
  uint32_t size() const {
    if (borrowed_) return length_;
    return source_ ? source_.size() : 0;
  }
  void close() {
    if (!borrowed_ && source_) source_.close();
    valid_ = false;
  }

 private:
  File source_;
  File* borrowed_ = nullptr;
  uint32_t base_ = 0;
  uint32_t length_ = 0;
  bool valid_ = false;
};

bool assetIoFailed = false;

// PGA1 is emitted by build_sd_asset_pack.py. Its index is sorted by the same
// FNV-1a hash used here and every PKG payload is concatenated in that order.
// This lets a complete move animation use one FAT open and bounded seeks
// inside one contiguous file instead of opening every cel separately.
struct AssetArchiveHeader {
  char magic[4];
  uint16_t version;
  uint16_t entrySize;
  uint16_t packVersion;
  uint16_t reserved;
  uint32_t assetCount;
  uint32_t indexOffset;
  uint32_t dataOffset;
} __attribute__((packed));

struct AssetArchiveEntry {
  uint32_t key;
  uint32_t offset;
  uint32_t length;
} __attribute__((packed));

struct ArchiveRangeCacheEntry {
  uint32_t key = 0;
  uint32_t offset = 0;
  uint32_t length = 0;
  bool valid = false;
};
constexpr uint8_t kArchiveRangeCacheCapacity = 64;
ArchiveRangeCacheEntry archiveRangeCache[kArchiveRangeCacheCapacity]{};
uint8_t archiveRangeCacheCount = 0;

constexpr const char* kAssetArchivePath = "/pokegochi/pokegochi.pak";
File archiveBatchFile;
AssetArchiveHeader archiveHeader{};
bool archiveAvailable = false;
uint8_t archiveBatchDepth = 0;

uint32_t assetPathHash(const char* path) {
  static constexpr const char* kPrefix = "/pokegochi/assets/";
  if (!path) return 0;
  if (std::strncmp(path, kPrefix, std::strlen(kPrefix)) == 0) path += std::strlen(kPrefix);
  uint32_t value = 2166136261UL;
  while (*path) value = (value ^ static_cast<uint8_t>(*path++)) * 16777619UL;
  return value;
}

bool readFileFully(File& file, uint8_t* destination, size_t length, bool latchFailure = true) {
  size_t received = 0;
  uint8_t emptyReads = 0;
  while (received < length && emptyReads < 4U) {
    const size_t count = file.read(destination + received, length - received);
    if (count) {
      received += count;
      emptyReads = 0;
    } else {
      ++emptyReads;
      yield();
      delay(1);
    }
  }
  if (received == length) return true;
  if (latchFailure) assetIoFailed = true;
  return false;
}

bool readArchiveHeader(File& archive, AssetArchiveHeader& header,
                       bool latchFailure = true) {
  header = AssetArchiveHeader{};
  if (!archive || !archive.seek(0) ||
      !readFileFully(archive, reinterpret_cast<uint8_t*>(&header), sizeof(header), latchFailure))
    return false;
  const uint64_t indexEnd = static_cast<uint64_t>(header.indexOffset) +
      static_cast<uint64_t>(header.assetCount) * header.entrySize;
  return !std::memcmp(header.magic, "PGA1", 4) && header.version == 3U &&
      header.entrySize == sizeof(AssetArchiveEntry) && header.assetCount != 0U &&
      header.packVersion == kRequiredAssetPackVersion && header.reserved == 0U &&
      header.indexOffset >= sizeof(AssetArchiveHeader) &&
      indexEnd <= header.dataOffset && header.dataOffset < archive.size();
}

bool archiveAssetRange(const char* path, uint32_t& offset, uint32_t& length) {
  offset = length = 0;
  if (!archiveBatchFile || !archiveBatchDepth || !archiveAvailable) return false;
  const uint32_t key = assetPathHash(path);
  for (uint8_t index = 0; index < archiveRangeCacheCount; ++index) {
    const ArchiveRangeCacheEntry& cached = archiveRangeCache[index];
    if (cached.valid && cached.key == key) {
      offset = cached.offset;
      length = cached.length;
      return true;
    }
  }
  uint32_t low = 0;
  uint32_t high = archiveHeader.assetCount;
  AssetArchiveEntry entry{};
  while (low < high) {
    const uint32_t middle = low + (high - low) / 2U;
    if (!archiveBatchFile.seek(archiveHeader.indexOffset + middle * archiveHeader.entrySize) ||
        !readFileFully(archiveBatchFile, reinterpret_cast<uint8_t*>(&entry), sizeof(entry)))
      return false;
    if (entry.key < key) low = middle + 1U;
    else high = middle;
  }
  if (low >= archiveHeader.assetCount ||
      !archiveBatchFile.seek(archiveHeader.indexOffset + low * archiveHeader.entrySize) ||
      !readFileFully(archiveBatchFile, reinterpret_cast<uint8_t*>(&entry), sizeof(entry)) ||
      entry.key != key || entry.offset < archiveHeader.dataOffset ||
      entry.offset >= archiveBatchFile.size())
    return false;
  if (!entry.length || static_cast<uint64_t>(entry.offset) + entry.length > archiveBatchFile.size()) {
    assetIoFailed = true;
    return false;
  }
  offset = entry.offset;
  length = entry.length;
  if (archiveRangeCacheCount < kArchiveRangeCacheCapacity) {
    ArchiveRangeCacheEntry& cached = archiveRangeCache[archiveRangeCacheCount++];
    cached = {key, offset, length, true};
  }
  return true;
}

// Storage/Bag/Summary scenes reference many tiny FireRed assets.  The arena
// still caps their pixel memory at 64 KiB, but 56 descriptors was too small:
// it filled before the arena did and every retained compositor pass reopened
// the missing sprites from SD.  Keep enough descriptors for one complete
// scene so subsequent bands are RAM-only.
constexpr uint8_t kCacheSlots = 96;
// The cache is an arena, never a collection of `new[]` allocations.  Scene
// changes reset it in one operation, which removes the heap fragmentation
// that made the old renderer degrade after opening a few menus.
// 48 KiB holds the complete manifests observed on Home, Box, Bag, Summary and
// Battle. A 64 KiB arena prevented the separate 80 KiB Home/terrain workspace
// from obtaining one contiguous ESP32 DRAM block once NVS was initialized.
constexpr size_t kCacheArenaBytes = 12000;
// A Home scene keeps two 304x134 indexed layers resident: the walkable map
// and its roof/tree/building foreground. PKG2 stores multi-palette maps at
// 6bpp, so the worst pair consumes 61,104 bytes without losing a source
// colour.
// The largest audited terrain + FireRed move-BG pair occupies 62,240 bytes.
// A 61 KiB bank holds it losslessly and returns 3 KiB to the contiguous heap.
// Together with the 1 KiB saved by halving the retained-band hash rows, that
// funds a twelve-scanline presenter without increasing total RAM.
constexpr size_t kSceneWorkspaceBytes = 61U * 1024U;
constexpr uint16_t kRegionBufferWidth = 96;
// Region restoration is streamed in short strips.  Keeping a complete 96x96
// RGB565 tile consumed 18 KiB even though the retained presenter itself works
// in bands.  A 96x32 strip is sufficient and releases 12 KiB for the taller
// presenter without changing any decoded pixel.
constexpr uint16_t kRegionBufferHeight = 32;
constexpr uint16_t kDecodeRows = 4;

CacheEntry cache[kCacheSlots];
// These two arenas are part of the renderer's fixed memory budget, not
// transient heap allocations. On ESP32, allocating either side of SD.begin()
// left FAT or the sprite cache without a sufficiently large contiguous heap
// block. Static storage makes boot order irrelevant and prevents render-time
// heap fragmentation from ever evicting scene pixels.
alignas(4) uint8_t cacheArenaStorage[kCacheArenaBytes]{};
uint8_t* cacheArena = cacheArenaStorage;
size_t cacheArenaUsed = 0;
size_t sceneCacheSpillUsed = 0;
size_t largeWorkspaceUsed = 0;
uint32_t cacheStamp = 0;

uint8_t* sceneWorkspace = nullptr;
size_t sceneWorkspaceCapacity = 0;
enum class SceneWorkspaceOwner : uint8_t { None, Home, Large };
SceneWorkspaceOwner sceneWorkspaceOwner = SceneWorkspaceOwner::None;

struct LargeAssetCache {
  char path[112]{};
  uint16_t palette[256]{};
  IndexedImage image{};
};
LargeAssetCache largeAsset;

// Battle needs two independently addressable full-scene indexed images at
// once: the terrain and either a changebg tilemap or a whole-field task
// plane.  Treating the second plane as an ordinary small sprite made its
// placement depend on cache order and allowed later sprite preloads to evict
// or overwrite it.  Its palette and packed indices now own a deterministic
// range immediately after the primary terrain inside the existing 61 KiB
// scene arena (no second framebuffer and no extra large heap allocation).
struct BattlePlaneCache {
  char path[112]{};
  IndexedImage image{};
};
BattlePlaneCache battlePlaneAsset;
size_t battlePlaneWorkspaceEnd = 0;

struct HomeLayerCache {
  char path[112]{};
  uint16_t palette[256]{};
  IndexedImage image{};
};
HomeLayerCache homeLayers[2];
size_t homeWorkspaceUsed = 0;

// TFT transfer and compositing buffers are deliberately fixed-size.  A full
// 320x240 framebuffer would require 153.6 KiB and leaves too little RAM for
// the game/save; these buffers are enough for dirty regions and 16-row strips.
// Region compositing and SD row decoding are mutually exclusive operations.
// Overlaying their scratch storage saves ~15 KiB of scarce internal DRAM.
union RenderScratch {
  uint16_t region[kRegionBufferWidth * kRegionBufferHeight];
  struct {
    uint16_t pixels[320U * kDecodeRows];
    uint8_t indices[320U * kDecodeRows];
  } stream;
};
RenderScratch scratch;
uint16_t directPalette[256]{};

bool compositionActive = false;
uint32_t assetOpenCount = 0;

// TFT_eSPI normally opens and closes an SPI transaction for every pushImage.
// An indexed 304x134 background is deliberately sent in 16-row strips, so
// without this scope one visual frame incurred nine separate transactions.
// Keeping CS asserted for the complete asset makes the screen update as one
// continuous transfer instead of visibly revealing each strip.
class TftWriteScope {
 public:
  explicit TftWriteScope(PixelCanvas&) {}
  ~TftWriteScope() = default;
  TftWriteScope(const TftWriteScope&) = delete;
  TftWriteScope& operator=(const TftWriteScope&) = delete;

};

AssetFile openAsset(const char* path) {
  if (!path || !path[0]) return AssetFile();
  if (archiveBatchDepth) {
    // A preload batch is an explicit request to use the canonical PGA
    // archive. Never fall through to stale loose files when the archive
    // could not be opened: OTAP replaces pokegochi.pak atomically, not the
    // several thousand development-time PKG mirrors on an existing card.
    if (!archiveAvailable || !archiveBatchFile) return AssetFile();
    uint32_t offset = 0;
    uint32_t length = 0;
    if (archiveAssetRange(path, offset, length))
      return AssetFile(&archiveBatchFile, offset, length);
    // Every file in a current asset pack is present in the archive. A missing
    // optional form is a normal miss; do not attempt a second FAT open while
    // the archive owns the mount's single descriptor.
    return AssetFile();
  }
  ++assetOpenCount;
  File file = SD.open(path, FILE_READ);
  if (file) return AssetFile(file);
  // SD.open() also fails normally when an optional/form-specific asset is
  // absent.  That is not evidence that the FAT volume or the socket failed:
  // treating it as such used to unmount a healthy card when an older asset
  // pack lacked (for example) a Deoxys form icon.  Short reads from an asset
  // that did open are still latched by readFully() as genuine I/O failures.
  Serial.printf("[GFX] open failed: %s\n", path);
  return AssetFile();
}

bool readFully(AssetFile& file, uint8_t* destination, size_t length) {
  size_t received = 0;
  uint8_t emptyReads = 0;
  while (received < length && emptyReads < 4U) {
    const size_t count = file.read(destination + received, length - received);
    if (count) {
      received += count;
      emptyReads = 0;
      continue;
    }
    ++emptyReads;
    yield();
    delay(1);
  }
  if (received == length) return true;
  assetIoFailed = true;
  return false;
}

bool readAssetInfo(AssetFile& file, AssetInfo& info) {
  info = AssetInfo{};
  if (!file.seek(0) || !readFully(file, reinterpret_cast<uint8_t*>(&info.header), sizeof(info.header))) return false;
  if (!std::memcmp(info.header.magic, "PKG1", 4)) {
    info.bitsPerPixel = 16;
    info.pixelOffset = sizeof(AssetHeader);
    info.pixelBytes = static_cast<size_t>(info.header.width) * info.header.height * sizeof(uint16_t);
  } else if (!std::memcmp(info.header.magic, "PKG2", 4)) {
    Pkg2Tail tail{};
    if (!readFully(file, reinterpret_cast<uint8_t*>(&tail), sizeof(tail)) ||
        (tail.bitsPerPixel != 4U && tail.bitsPerPixel != 6U && tail.bitsPerPixel != 8U) || tail.paletteCount == 0 ||
        (tail.bitsPerPixel == 4U && tail.paletteCount > 16U)) return false;
    info.bitsPerPixel = tail.bitsPerPixel;
    info.paletteCount = tail.paletteCount;
    info.paletteOffset = sizeof(AssetHeader) + sizeof(Pkg2Tail);
    info.pixelOffset = info.paletteOffset + static_cast<uint32_t>(info.paletteCount) * sizeof(uint16_t);
    const size_t pixelCount = static_cast<size_t>(info.header.width) * info.header.height;
    info.pixelBytes = info.bitsPerPixel == 4U ? (pixelCount + 1U) / 2U
                    : info.bitsPerPixel == 6U ? (pixelCount * 6U + 7U) / 8U : pixelCount;
  } else {
    return false;
  }
  // FireRed's scrolling BG1 effects (Surf and Muddy Water) are authored as
  // 512x112 tile planes. They are kept resident and cropped by the battle
  // compositor, so rejecting every source wider than the 320px LCD made the
  // valid plane fail at `battle-plane-header` before playback could begin.
  // 512 is the largest generated source plane; the visible destination is
  // still clipped to the 320x240 panel by the renderer.
  if (info.header.width == 0 || info.header.width > 512 || info.header.height == 0 || info.header.height > 320 ||
      file.size() < info.pixelOffset || static_cast<size_t>(file.size() - info.pixelOffset) < info.pixelBytes) return false;
  return true;
}

bool containsTransparency(const IndexedImage& image) {
  for (uint16_t index = 0; index < image.paletteCount; ++index)
    if (image.palette[index] == image.header.transparent) return true;
  return false;
}

uint8_t indexedValue(const IndexedImage& image, size_t pixel) {
  if (image.bitsPerPixel == 4U) {
    const uint8_t packed = image.indices[pixel >> 1U];
    return static_cast<uint8_t>((pixel & 1U) ? (packed & 0x0FU) : (packed >> 4U));
  }
  if (image.bitsPerPixel == 6U) {
    const size_t bit = pixel * 6U;
    const size_t byte = bit >> 3U;
    const uint8_t shift = static_cast<uint8_t>(bit & 7U);
    const uint16_t pair = static_cast<uint16_t>(image.indices[byte]) << 8U |
                          (byte + 1U < image.indexBytes ? image.indices[byte + 1U] : 0U);
    return static_cast<uint8_t>((pair >> (10U - shift)) & 0x3FU);
  }
  return image.indices[pixel];
}

uint16_t indexedPixel(const IndexedImage& image, size_t pixel) {
  const uint8_t color = indexedValue(image, pixel);
  return color < image.paletteCount ? image.palette[color] : image.header.transparent;
}

int16_t paletteIndex(const IndexedImage& image, uint16_t color) {
  for (uint16_t index = 0; index < image.paletteCount; ++index)
    if (image.palette[index] == color) return static_cast<int16_t>(index);
  return -1;
}

// Convert an older raw RGB565 package only when needed.  New PKG2 packages
// are copied exactly as stored, so scene loading does no palette discovery.
bool convertLegacyToIndexed(AssetFile& file, const AssetInfo& info, uint8_t* destination,
                            size_t capacity, uint16_t* palette, IndexedImage& result) {
  const size_t pixelCount = static_cast<size_t>(info.header.width) * info.header.height;
  if (capacity < pixelCount || !palette || !file.seek(info.pixelOffset)) return false;
  result = IndexedImage{};
  result.header = info.header;
  result.bitsPerPixel = 4;
  result.palette = palette;
  result.indices = destination;
  result.indexBytes = (pixelCount + 1U) / 2U;
  std::memset(destination, 0, result.indexBytes);

  size_t outputPixel = 0;
  for (uint16_t line = 0; line < info.header.height; line += kDecodeRows) {
    const uint16_t rows = static_cast<uint16_t>(std::min<uint16_t>(kDecodeRows, info.header.height - line));
    const size_t pixels = static_cast<size_t>(info.header.width) * rows;
    if (!readFully(file, reinterpret_cast<uint8_t*>(scratch.stream.pixels), pixels * sizeof(uint16_t))) return false;
    for (size_t input = 0; input < pixels; ++input, ++outputPixel) {
      int16_t color = paletteIndex(result, scratch.stream.pixels[input]);
      if (color < 0) {
        if (result.paletteCount == 256U) return false;
        if (result.bitsPerPixel == 4U && result.paletteCount == 16U) {
          // Destination was reserved at the 8-bit worst case.  Promote the
          // already-written nibbles backwards without a second buffer/pass.
          for (size_t previous = outputPixel; previous-- > 0;) {
            const uint8_t packed = destination[previous >> 1U];
            destination[previous] = (previous & 1U) ? (packed & 0x0FU) : (packed >> 4U);
          }
          result.bitsPerPixel = 8;
          result.indexBytes = pixelCount;
        }
        color = result.paletteCount;
        result.palette[result.paletteCount++] = scratch.stream.pixels[input];
      }
      if (result.bitsPerPixel == 4U) {
        uint8_t& packed = destination[outputPixel >> 1U];
        if ((outputPixel & 1U) == 0) packed = static_cast<uint8_t>(color << 4U);
        else packed = static_cast<uint8_t>(packed | static_cast<uint8_t>(color));
      } else {
        destination[outputPixel] = static_cast<uint8_t>(color);
      }
    }
  }
  result.valid = true;
  return true;
}

bool loadIndexed(AssetFile& file, const AssetInfo& info, uint8_t* destination,
                 size_t capacity, uint16_t* palette, IndexedImage& result) {
  if (info.bitsPerPixel == 16U)
    return convertLegacyToIndexed(file, info, destination, capacity, palette, result);
  if (capacity < info.pixelBytes || !palette || !file.seek(info.paletteOffset)) return false;
  result = IndexedImage{};
  result.header = info.header;
  result.bitsPerPixel = info.bitsPerPixel;
  result.paletteCount = info.paletteCount;
  result.palette = palette;
  result.indices = destination;
  result.indexBytes = info.pixelBytes;
  if (!readFully(file, reinterpret_cast<uint8_t*>(palette),
                 static_cast<size_t>(info.paletteCount) * sizeof(uint16_t)) ||
      !readFully(file, destination, info.pixelBytes)) return false;
  result.valid = true;
  return true;
}

bool drawIndexed(PixelCanvas& display, const IndexedImage& image, int16_t x, int16_t y) {
  if (!image.valid || !image.indices) return false;
  // The retained presenter owns a short stripe sprite and moves its logical origin
  // down the 240px scene for each stripe. Decoding all 134 rows of a Home
  // layer (or all 184 rows of battle terrain) for every stripe made a
  // complete frame perform the same palette work dozens of times. Record the
  // complete logical damage first, then decode only source rows intersecting
  // the current canvas stripe. Composition still produces byte-identical
  // pixels; it simply stops spending time on rows TFT_eSprite would clip.
  display.damage(x, y, image.header.width, image.header.height);
  const int32_t logicalCanvasTop = -display.getOriginY();
  const int32_t logicalCanvasBottom = logicalCanvasTop + display.height();
  const int32_t firstVisibleRow = std::max<int32_t>(0, logicalCanvasTop - y);
  const int32_t lastVisibleRow = std::min<int32_t>(image.header.height,
                                                   logicalCanvasBottom - y);
  const int32_t firstVisibleColumn = std::max<int32_t>(
      0, display.logicalClipLeft() - x);
  const int32_t lastVisibleColumn = std::min<int32_t>(
      image.header.width, display.logicalClipRight() - x);
  if (lastVisibleRow <= firstVisibleRow ||
      lastVisibleColumn <= firstVisibleColumn) return true;
  const uint16_t visibleWidth = static_cast<uint16_t>(
      lastVisibleColumn - firstVisibleColumn);
  const bool transparent = containsTransparency(image);
  TftWriteScope write(display);
  display.setSwapBytes(true);
  for (uint16_t line = static_cast<uint16_t>(firstVisibleRow);
       line < static_cast<uint16_t>(lastVisibleRow); line += kDecodeRows) {
    const uint16_t rows = static_cast<uint16_t>(std::min<int32_t>(
        kDecodeRows, lastVisibleRow - line));
    for (uint16_t row = 0; row < rows; ++row) {
      const size_t sourceRow = static_cast<size_t>(line + row) *
          image.header.width + firstVisibleColumn;
      const size_t destinationRow = static_cast<size_t>(row) * visibleWidth;
      for (uint16_t column = 0; column < visibleWidth; ++column)
        scratch.stream.pixels[destinationRow + column] =
            indexedPixel(image, sourceRow + column);
    }
    if (transparent)
      display.pushImage(x + firstVisibleColumn, y + line, visibleWidth, rows,
                        scratch.stream.pixels, image.header.transparent);
    else
      display.pushImage(x + firstVisibleColumn, y + line, visibleWidth, rows,
                        scratch.stream.pixels);
  }
  return true;
}

uint16_t positiveModulo(int32_t value, uint16_t modulus) {
  if (!modulus) return 0U;
  const int32_t result = value % static_cast<int32_t>(modulus);
  return static_cast<uint16_t>(result < 0 ? result + modulus : result);
}

uint16_t indexedPaletteIndexPixel(const IndexedImage& image, uint8_t index,
                                  uint8_t paletteFirst,
                                  uint8_t paletteCount,
                                  uint8_t paletteShift) {
  if (paletteCount && paletteFirst < image.paletteCount &&
      index >= paletteFirst &&
      index < static_cast<uint16_t>(paletteFirst + paletteCount)) {
    const uint8_t count = static_cast<uint8_t>(std::min<uint16_t>(
        paletteCount, image.paletteCount - paletteFirst));
    if (count)
      index = static_cast<uint8_t>(paletteFirst +
          (index - paletteFirst + paletteShift % count) % count);
  }
  return index < image.paletteCount ? image.palette[index]
                                    : image.header.transparent;
}

uint16_t indexedPalettePixel(const IndexedImage& image, uint16_t sourceX,
                             uint16_t sourceY, uint8_t paletteFirst,
                             uint8_t paletteCount, uint8_t paletteShift) {
  return indexedPaletteIndexPixel(image, indexedValue(
      image, static_cast<size_t>(sourceY) * image.header.width + sourceX),
      paletteFirst, paletteCount, paletteShift);
}

bool orderedAlphaVisible(int16_t screenX, int16_t screenY,
                         uint8_t opacity16) {
  static constexpr uint8_t kBayer4x4[16] = {
       0,  8,  2, 10, 12,  4, 14,  6,
       3, 11,  1,  9, 15,  7, 13,  5,
  };
  return opacity16 >= 16U || kBayer4x4[
      ((static_cast<uint16_t>(screenY) & 3U) << 2U) |
       (static_cast<uint16_t>(screenX) & 3U)] < opacity16;
}

uint16_t alphaBlend565(uint16_t foreground, uint16_t background,
                       uint8_t foregroundAmount) {
  foregroundAmount = std::min<uint8_t>(16U, foregroundAmount);
  if (!foregroundAmount) return background;
  if (foregroundAmount == 16U) return foreground;
  const uint8_t backgroundAmount = static_cast<uint8_t>(16U - foregroundAmount);
  const uint16_t red = static_cast<uint16_t>(std::min<uint32_t>(31U,
      ((foreground >> 11U) * foregroundAmount +
       (background >> 11U) * backgroundAmount + 8U) >> 4U));
  const uint16_t green = static_cast<uint16_t>(std::min<uint32_t>(63U,
      (((foreground >> 5U) & 0x3FU) * foregroundAmount +
       ((background >> 5U) & 0x3FU) * backgroundAmount + 8U) >> 4U));
  const uint16_t blue = static_cast<uint16_t>(std::min<uint32_t>(31U,
      ((foreground & 0x1FU) * foregroundAmount +
       (background & 0x1FU) * backgroundAmount + 8U) >> 4U));
  return static_cast<uint16_t>((red << 11U) | (green << 5U) | blue);
}

bool drawIndexedScrolled(PixelCanvas& display, const IndexedImage& image,
                         int16_t x, int16_t y, uint16_t width,
                         uint16_t height, int16_t sourceOffsetX,
                         int16_t sourceOffsetY, uint8_t opacity16,
                         uint8_t paletteFirst, uint8_t paletteCount,
                         uint8_t paletteShift, uint8_t scaleNumerator,
                         uint8_t scaleDenominator, bool wrapX, bool wrapY,
                         uint16_t virtualSourceWidth,
                         uint16_t virtualSourceHeight,
                         uint8_t virtualFillIndex) {
  if (!image.valid || !image.indices || !width || !height) return false;
  if (!scaleNumerator || !scaleDenominator) return false;
  opacity16 = std::min<uint8_t>(16U, opacity16);
  if (!opacity16) return true;
  display.damage(x, y, width, height);
  const int32_t logicalCanvasTop = -display.getOriginY();
  const int32_t logicalCanvasBottom = logicalCanvasTop + display.height();
  const int32_t firstVisibleRow = std::max<int32_t>(0, logicalCanvasTop - y);
  const int32_t lastVisibleRow = std::min<int32_t>(height,
                                                   logicalCanvasBottom - y);
  const int32_t firstVisibleColumn = std::max<int32_t>(
      0, display.logicalClipLeft() - x);
  const int32_t lastVisibleColumn = std::min<int32_t>(
      width, display.logicalClipRight() - x);
  if (lastVisibleRow <= firstVisibleRow ||
      lastVisibleColumn <= firstVisibleColumn) return true;
  const uint16_t visibleWidth = static_cast<uint16_t>(
      lastVisibleColumn - firstVisibleColumn);
  TftWriteScope write(display);
  display.setSwapBytes(true);
  const uint16_t sourceWidth = std::max<uint16_t>(
      image.header.width, virtualSourceWidth);
  const uint16_t sourceHeight = std::max<uint16_t>(
      image.header.height, virtualSourceHeight);
  const bool hasVirtualFill = virtualFillIndex < image.paletteCount;
  const bool identityScale = scaleNumerator == scaleDenominator;
  const bool transparent = opacity16 < 16U || containsTransparency(image);
  // X only depends on the clipped destination column, never on the row.
  // Keep its wrapped/scaled source coordinate in the otherwise independent
  // decode-index scratch.  Native FireRed camera planes use 4:3 scaling;
  // calculating x*3/4 once per stripe instead of once per pixel removes
  // tens of thousands of divisions from each scrolling animation frame.
  static_assert(sizeof(scratch.stream.indices) >=
                320U * sizeof(uint16_t), "scroll x-map scratch too small");
  uint16_t* sourceXMap = reinterpret_cast<uint16_t*>(scratch.stream.indices);
  for (uint16_t column = 0; column < visibleWidth; ++column) {
    const int32_t destinationX = firstVisibleColumn + column;
    const int32_t sourceXUnbounded = identityScale
        ? destinationX + sourceOffsetX
        : destinationX * scaleDenominator / scaleNumerator + sourceOffsetX;
    sourceXMap[column] = wrapX
        ? positiveModulo(sourceXUnbounded, sourceWidth)
        : static_cast<uint16_t>(std::clamp<int32_t>(
              sourceXUnbounded, 0, sourceWidth - 1U));
  }
  for (uint16_t line = static_cast<uint16_t>(firstVisibleRow);
       line < static_cast<uint16_t>(lastVisibleRow); line += kDecodeRows) {
    const uint16_t rows = static_cast<uint16_t>(std::min<int32_t>(
        kDecodeRows, lastVisibleRow - line));
    for (uint16_t row = 0; row < rows; ++row) {
      // Move scale division and wrap reduction out of the inner pixel loop.
      // Most FireRed changebg planes (including Blizzard) are 1:1 images
      // scrolled by a few pixels.  The old generic path performed a signed
      // division and modulo for every one of the 58,880 arena pixels, making
      // a 60 Hz BG task take ~130 ms per physical frame on the ESP32.
      const int32_t sourceYUnbounded = identityScale
          ? static_cast<int32_t>(line + row) + sourceOffsetY
          : static_cast<int32_t>(line + row) * scaleDenominator /
                scaleNumerator + sourceOffsetY;
      const uint16_t sourceY = wrapY
          ? positiveModulo(sourceYUnbounded, sourceHeight)
          : static_cast<uint16_t>(std::clamp<int32_t>(
                sourceYUnbounded, 0, sourceHeight - 1U));
      for (uint16_t column = static_cast<uint16_t>(firstVisibleColumn);
           column < static_cast<uint16_t>(lastVisibleColumn); ++column) {
        const uint16_t sourceX = sourceXMap[column - firstVisibleColumn];
        uint16_t pixel = (sourceX < image.header.width &&
                          sourceY < image.header.height)
            ? indexedPalettePixel(image, sourceX, sourceY,
                                  paletteFirst, paletteCount, paletteShift)
            : hasVirtualFill
                ? indexedPaletteIndexPixel(image, virtualFillIndex,
                                           paletteFirst, paletteCount,
                                           paletteShift)
                : image.header.transparent;
        if (pixel != image.header.transparent &&
            !orderedAlphaVisible(static_cast<int16_t>(x + column),
                                 static_cast<int16_t>(y + line + row),
                                 opacity16))
          pixel = image.header.transparent;
        scratch.stream.pixels[static_cast<size_t>(row) * visibleWidth +
                              column - firstVisibleColumn] = pixel;
      }
    }
    if (transparent)
      display.pushImage(x + firstVisibleColumn, y + line,
                        visibleWidth, rows, scratch.stream.pixels,
                        image.header.transparent);
    else
      display.pushImage(x + firstVisibleColumn, y + line,
                        visibleWidth, rows, scratch.stream.pixels);
  }
  return true;
}

bool drawIndexedHorizontalMirror(PixelCanvas& display, const IndexedImage& image,
                                 int16_t x, int16_t y) {
  if (!image.valid || !image.indices) return false;
  display.damage(x, y, image.header.width, image.header.height);
  const int32_t logicalCanvasTop = -display.getOriginY();
  const int32_t logicalCanvasBottom = logicalCanvasTop + display.height();
  const int32_t firstVisibleRow = std::max<int32_t>(0, logicalCanvasTop - y);
  const int32_t lastVisibleRow = std::min<int32_t>(image.header.height,
                                                   logicalCanvasBottom - y);
  const int32_t firstVisibleColumn = std::max<int32_t>(
      0, display.logicalClipLeft() - x);
  const int32_t lastVisibleColumn = std::min<int32_t>(
      image.header.width, display.logicalClipRight() - x);
  if (lastVisibleRow <= firstVisibleRow ||
      lastVisibleColumn <= firstVisibleColumn) return true;
  const uint16_t visibleWidth = static_cast<uint16_t>(
      lastVisibleColumn - firstVisibleColumn);
  const bool transparent = containsTransparency(image);
  TftWriteScope write(display);
  display.setSwapBytes(true);
  for (uint16_t line = static_cast<uint16_t>(firstVisibleRow);
       line < static_cast<uint16_t>(lastVisibleRow); line += kDecodeRows) {
    const uint16_t rows = static_cast<uint16_t>(std::min<int32_t>(
        kDecodeRows, lastVisibleRow - line));
    for (uint16_t row = 0; row < rows; ++row) {
      const size_t sourceRow = static_cast<size_t>(line + row) * image.header.width;
      const size_t destinationRow = static_cast<size_t>(row) * visibleWidth;
      for (uint16_t column = static_cast<uint16_t>(firstVisibleColumn);
           column < static_cast<uint16_t>(lastVisibleColumn); ++column) {
        scratch.stream.pixels[destinationRow + column - firstVisibleColumn] = indexedPixel(
            image, sourceRow + image.header.width - 1U - column);
      }
    }
    if (transparent)
      display.pushImage(x + firstVisibleColumn, y + line, visibleWidth, rows,
                        scratch.stream.pixels, image.header.transparent);
    else
      display.pushImage(x + firstVisibleColumn, y + line, visibleWidth, rows,
                        scratch.stream.pixels);
  }
  return true;
}

uint16_t metallicPixel(uint16_t color, uint16_t localX, uint16_t localY,
                       uint8_t phase) {
  const uint16_t red = static_cast<uint16_t>(((color >> 11U) & 0x1FU) * 255U / 31U);
  const uint16_t green = static_cast<uint16_t>(((color >> 5U) & 0x3FU) * 255U / 63U);
  const uint16_t blue = static_cast<uint16_t>((color & 0x1FU) * 255U / 31U);
  uint16_t grey = static_cast<uint16_t>((red * 30U + green * 59U + blue * 11U) / 100U);

  // The original 32x32 metal_shine mask is a narrow diagonal white band and
  // its BG scrolls horizontally over the battler. Map one complete sweep to
  // phases 0..15; the caller performs the two passes used by FireRed.
  const int16_t sweep = static_cast<int16_t>(-10 +
      static_cast<int16_t>(std::min<uint8_t>(phase, 15U)) * 10);
  const int16_t diagonal = static_cast<int16_t>(localX + localY);
  const int16_t delta = static_cast<int16_t>(diagonal - sweep);
  const uint16_t distance = static_cast<uint16_t>(delta < 0 ? -delta : delta);
  if (distance <= 2U) grey = 255U;
  else if (distance <= 6U) grey = static_cast<uint16_t>(grey + (255U - grey) * (7U - distance) / 7U);
  else grey = static_cast<uint16_t>(grey * 4U / 5U + 24U);

  const uint16_t r5 = static_cast<uint16_t>(grey * 31U / 255U);
  const uint16_t g6 = static_cast<uint16_t>(grey * 63U / 255U);
  return static_cast<uint16_t>((r5 << 11U) | (g6 << 5U) | r5);
}

bool drawIndexedMetallic(PixelCanvas& display, const IndexedImage& image,
                         int16_t x, int16_t y, uint8_t phase) {
  if (!image.valid || !image.indices) return false;
  display.damage(x, y, image.header.width, image.header.height);
  const int32_t logicalCanvasTop = -display.getOriginY();
  const int32_t logicalCanvasBottom = logicalCanvasTop + display.height();
  const int32_t firstVisibleRow = std::max<int32_t>(0, logicalCanvasTop - y);
  const int32_t lastVisibleRow = std::min<int32_t>(image.header.height,
                                                   logicalCanvasBottom - y);
  const int32_t firstVisibleColumn = std::max<int32_t>(
      0, display.logicalClipLeft() - x);
  const int32_t lastVisibleColumn = std::min<int32_t>(
      image.header.width, display.logicalClipRight() - x);
  if (lastVisibleRow <= firstVisibleRow ||
      lastVisibleColumn <= firstVisibleColumn) return true;
  const uint16_t visibleWidth = static_cast<uint16_t>(
      lastVisibleColumn - firstVisibleColumn);
  const bool transparent = containsTransparency(image);
  TftWriteScope write(display);
  display.setSwapBytes(true);
  for (uint16_t line = static_cast<uint16_t>(firstVisibleRow);
       line < static_cast<uint16_t>(lastVisibleRow); line += kDecodeRows) {
    const uint16_t rows = static_cast<uint16_t>(std::min<int32_t>(
        kDecodeRows, lastVisibleRow - line));
    for (uint16_t row = 0; row < rows; ++row) {
      const uint16_t localY = static_cast<uint16_t>(line + row);
      const size_t sourceRow = static_cast<size_t>(localY) * image.header.width;
      const size_t destinationRow = static_cast<size_t>(row) * visibleWidth;
      for (uint16_t localX = static_cast<uint16_t>(firstVisibleColumn);
           localX < static_cast<uint16_t>(lastVisibleColumn); ++localX) {
        const uint16_t original = indexedPixel(image, sourceRow + localX);
        scratch.stream.pixels[destinationRow + localX - firstVisibleColumn] =
            original == image.header.transparent
                ? original : metallicPixel(original, localX, localY, phase);
      }
    }
    if (transparent)
      display.pushImage(x + firstVisibleColumn, y + line, visibleWidth, rows,
                        scratch.stream.pixels, image.header.transparent);
    else
      display.pushImage(x + firstVisibleColumn, y + line, visibleWidth, rows,
                        scratch.stream.pixels);
  }
  return true;
}

uint16_t blendRgb565(uint16_t source, uint16_t tint, uint8_t amount) {
  amount = std::min<uint8_t>(16U, amount);
  const uint8_t inverse = static_cast<uint8_t>(16U - amount);
  const uint16_t red = static_cast<uint16_t>(
      ((((source >> 11U) & 0x1FU) * inverse) +
       (((tint >> 11U) & 0x1FU) * amount)) / 16U);
  const uint16_t green = static_cast<uint16_t>(
      ((((source >> 5U) & 0x3FU) * inverse) +
       (((tint >> 5U) & 0x3FU) * amount)) / 16U);
  const uint16_t blue = static_cast<uint16_t>(
      (((source & 0x1FU) * inverse + (tint & 0x1FU) * amount) / 16U));
  return static_cast<uint16_t>((red << 11U) | (green << 5U) | blue);
}

uint16_t transformedColor(uint16_t source, AssetColorEffect effect,
                          uint16_t tint, uint8_t amount) {
  switch (effect) {
    case AssetColorEffect::Blend: return blendRgb565(source, tint, amount);
    case AssetColorEffect::Grayscale: {
      const uint16_t red = static_cast<uint16_t>(((source >> 11U) & 0x1FU) * 255U / 31U);
      const uint16_t green = static_cast<uint16_t>(((source >> 5U) & 0x3FU) * 255U / 63U);
      const uint16_t blue = static_cast<uint16_t>((source & 0x1FU) * 255U / 31U);
      const uint16_t grey = static_cast<uint16_t>((red * 30U + green * 59U + blue * 11U) / 100U);
      return static_cast<uint16_t>(((grey * 31U / 255U) << 11U) |
                                   ((grey * 63U / 255U) << 5U) |
                                   (grey * 31U / 255U));
    }
    case AssetColorEffect::Invert:
      return static_cast<uint16_t>((~source) & 0xFFFFU);
    case AssetColorEffect::None:
    default: return source;
  }
}

bool drawIndexedTransformed(PixelCanvas& display, const IndexedImage& image,
                            int16_t centerX, int16_t centerY,
                            uint8_t scaleXPercent, uint8_t scaleYPercent,
                            AssetColorEffect colorEffect, uint16_t blendColor,
                            uint8_t blendAmount) {
  if (!image.valid || !image.indices) return false;
  scaleXPercent = std::max<uint8_t>(25U, std::min<uint8_t>(175U, scaleXPercent));
  scaleYPercent = std::max<uint8_t>(25U, std::min<uint8_t>(175U, scaleYPercent));
  const uint16_t width = static_cast<uint16_t>(std::max<uint32_t>(1U,
      (static_cast<uint32_t>(image.header.width) * scaleXPercent + 50U) / 100U));
  const uint16_t height = static_cast<uint16_t>(std::max<uint32_t>(1U,
      (static_cast<uint32_t>(image.header.height) * scaleYPercent + 50U) / 100U));
  if (width > 320U) return false;
  const int16_t x = static_cast<int16_t>(centerX - static_cast<int16_t>(width / 2U));
  const int16_t y = static_cast<int16_t>(centerY - static_cast<int16_t>(height / 2U));
  display.damage(x, y, width, height);
  const int32_t logicalCanvasTop = -display.getOriginY();
  const int32_t logicalCanvasBottom = logicalCanvasTop + display.height();
  const int32_t firstVisibleRow = std::max<int32_t>(0, logicalCanvasTop - y);
  const int32_t lastVisibleRow = std::min<int32_t>(height, logicalCanvasBottom - y);
  const int32_t firstVisibleColumn = std::max<int32_t>(
      0, display.logicalClipLeft() - x);
  const int32_t lastVisibleColumn = std::min<int32_t>(
      width, display.logicalClipRight() - x);
  if (lastVisibleRow <= firstVisibleRow ||
      lastVisibleColumn <= firstVisibleColumn) return true;
  const uint16_t visibleWidth = static_cast<uint16_t>(
      lastVisibleColumn - firstVisibleColumn);
  const bool transparent = containsTransparency(image);
  TftWriteScope write(display);
  display.setSwapBytes(true);
  for (uint16_t line = static_cast<uint16_t>(firstVisibleRow);
       line < static_cast<uint16_t>(lastVisibleRow); line += kDecodeRows) {
    const uint16_t rows = static_cast<uint16_t>(std::min<int32_t>(
        kDecodeRows, lastVisibleRow - line));
    for (uint16_t row = 0; row < rows; ++row) {
      const uint16_t sourceY = static_cast<uint16_t>(std::min<uint32_t>(
          image.header.height - 1U,
          static_cast<uint32_t>(line + row) * image.header.height / height));
      for (uint16_t column = static_cast<uint16_t>(firstVisibleColumn);
           column < static_cast<uint16_t>(lastVisibleColumn); ++column) {
        const uint16_t sourceX = static_cast<uint16_t>(std::min<uint32_t>(
            image.header.width - 1U,
            static_cast<uint32_t>(column) * image.header.width / width));
        const uint16_t original = indexedPixel(
            image, static_cast<size_t>(sourceY) * image.header.width + sourceX);
        scratch.stream.pixels[static_cast<size_t>(row) * visibleWidth +
                              column - firstVisibleColumn] =
            original == image.header.transparent ? original :
            transformedColor(original, colorEffect, blendColor, blendAmount);
      }
    }
    if (transparent)
      display.pushImage(x + firstVisibleColumn, y + line, visibleWidth, rows,
                        scratch.stream.pixels,
                        image.header.transparent);
    else
      display.pushImage(x + firstVisibleColumn, y + line, visibleWidth, rows,
                        scratch.stream.pixels);
  }
  return true;
}

int16_t affineSin256(uint8_t angle) {
  static constexpr int16_t kSin64[] = {
      0, 25, 50, 74, 98, 120, 142, 162, 181, 198, 213, 226, 237, 245, 251, 255,
      256, 255, 251, 245, 237, 226, 213, 198, 181, 162, 142, 120, 98, 74, 50, 25,
      0, -25, -50, -74, -98, -120, -142, -162, -181, -198, -213, -226, -237,
      -245, -251, -255, -256, -255, -251, -245, -237, -226, -213, -198, -181,
      -162, -142, -120, -98, -74, -50, -25};
  return kSin64[(angle >> 2U) & 0x3FU];
}

bool drawIndexedAffine(PixelCanvas& display, const IndexedImage& image,
                       int16_t centerX, int16_t centerY,
                       int16_t scaleXPercent, int16_t scaleYPercent,
                       uint8_t rotation, uint8_t opacity16,
                       AssetColorEffect colorEffect,
                       uint16_t blendColor, uint8_t blendAmount) {
  if (!image.valid || !image.indices) return false;
  opacity16 = std::min<uint8_t>(16U, opacity16);
  if (!opacity16) return true;
  scaleXPercent = std::max<int16_t>(-250, std::min<int16_t>(250, scaleXPercent));
  scaleYPercent = std::max<int16_t>(-250, std::min<int16_t>(250, scaleYPercent));
  if (!scaleXPercent || !scaleYPercent) return true;
  const uint16_t scaledWidth = static_cast<uint16_t>(std::max<int32_t>(
      1, static_cast<int32_t>(image.header.width) * std::abs(scaleXPercent) / 100));
  const uint16_t scaledHeight = static_cast<uint16_t>(std::max<int32_t>(
      1, static_cast<int32_t>(image.header.height) * std::abs(scaleYPercent) / 100));
  const int16_t sine = affineSin256(rotation);
  const int16_t cosine = affineSin256(static_cast<uint8_t>(rotation + 64U));
  const uint16_t width = static_cast<uint16_t>(std::max<int32_t>(1,
      (static_cast<int32_t>(std::abs(cosine)) * scaledWidth +
       static_cast<int32_t>(std::abs(sine)) * scaledHeight + 255) / 256));
  const uint16_t height = static_cast<uint16_t>(std::max<int32_t>(1,
      (static_cast<int32_t>(std::abs(sine)) * scaledWidth +
       static_cast<int32_t>(std::abs(cosine)) * scaledHeight + 255) / 256));
  if (width > 320U) return false;
  const int16_t x = static_cast<int16_t>(centerX - width / 2);
  const int16_t y = static_cast<int16_t>(centerY - height / 2);
  display.damage(x, y, width, height);
  const int32_t logicalCanvasTop = -display.getOriginY();
  const int32_t logicalCanvasBottom = logicalCanvasTop + display.height();
  const int32_t firstVisibleRow = std::max<int32_t>(0, logicalCanvasTop - y);
  const int32_t lastVisibleRow = std::min<int32_t>(height, logicalCanvasBottom - y);
  const int32_t firstVisibleColumn = std::max<int32_t>(
      0, display.logicalClipLeft() - x);
  const int32_t lastVisibleColumn = std::min<int32_t>(
      width, display.logicalClipRight() - x);
  if (lastVisibleRow <= firstVisibleRow ||
      lastVisibleColumn <= firstVisibleColumn) return true;
  const uint16_t visibleWidth = static_cast<uint16_t>(
      lastVisibleColumn - firstVisibleColumn);
  TftWriteScope write(display);
  display.setSwapBytes(true);
  for (uint16_t line = static_cast<uint16_t>(firstVisibleRow);
       line < static_cast<uint16_t>(lastVisibleRow); line += kDecodeRows) {
    const uint16_t rows = static_cast<uint16_t>(std::min<int32_t>(
        kDecodeRows, lastVisibleRow - line));
    for (uint16_t row = 0; row < rows; ++row) {
      const int32_t dy = static_cast<int32_t>(line + row) - height / 2;
      for (uint16_t column = static_cast<uint16_t>(firstVisibleColumn);
           column < static_cast<uint16_t>(lastVisibleColumn); ++column) {
        const int32_t dx = static_cast<int32_t>(column) - width / 2;
        const int32_t unrotatedX = dx * cosine + dy * sine;
        const int32_t unrotatedY = -dx * sine + dy * cosine;
        const int32_t sourceX = static_cast<int32_t>(image.header.width) / 2 +
            unrotatedX * 100 / (256 * scaleXPercent);
        const int32_t sourceY = static_cast<int32_t>(image.header.height) / 2 +
            unrotatedY * 100 / (256 * scaleYPercent);
        uint16_t color = image.header.transparent;
        if (sourceX >= 0 && sourceY >= 0 &&
            sourceX < image.header.width && sourceY < image.header.height)
          color = indexedPixel(image, static_cast<size_t>(sourceY) *
                                      image.header.width + sourceX);
        if (color != image.header.transparent &&
            colorEffect != AssetColorEffect::None)
          color = transformedColor(color, colorEffect, blendColor, blendAmount);
        if (color != image.header.transparent && opacity16 < 16U) {
          static constexpr uint8_t kBayer4x4[16] = {
               0,  8,  2, 10, 12,  4, 14,  6,
               3, 11,  1,  9, 15,  7, 13,  5,
          };
          const uint8_t threshold = kBayer4x4[
              ((static_cast<uint16_t>(y + line + row) & 3U) << 2U) |
              (static_cast<uint16_t>(x + column) & 3U)];
          if (threshold >= opacity16) color = image.header.transparent;
        }
        scratch.stream.pixels[static_cast<size_t>(row) * visibleWidth +
                              column - firstVisibleColumn] = color;
      }
    }
    // Rotation creates transparent corners even when every source pixel is
    // opaque, so the affine destination always uses the transparent key.
    display.pushImage(x + firstVisibleColumn, y + line, visibleWidth, rows,
                      scratch.stream.pixels,
                      image.header.transparent);
  }
  return true;
}

bool drawIndexedPaletteCycled(PixelCanvas& display, const IndexedImage& image,
                              int16_t x, int16_t y, uint8_t firstColor,
                              uint8_t colorCount, uint8_t shift) {
  if (!image.valid || !image.indices || !colorCount ||
      firstColor >= image.paletteCount)
    return false;
  colorCount = static_cast<uint8_t>(std::min<uint16_t>(
      colorCount, image.paletteCount - firstColor));
  shift = static_cast<uint8_t>(shift % colorCount);
  display.damage(x, y, image.header.width, image.header.height);
  const int32_t logicalCanvasTop = -display.getOriginY();
  const int32_t logicalCanvasBottom = logicalCanvasTop + display.height();
  const int32_t firstVisibleRow = std::max<int32_t>(0, logicalCanvasTop - y);
  const int32_t lastVisibleRow = std::min<int32_t>(image.header.height,
                                                   logicalCanvasBottom - y);
  const int32_t firstVisibleColumn = std::max<int32_t>(
      0, display.logicalClipLeft() - x);
  const int32_t lastVisibleColumn = std::min<int32_t>(
      image.header.width, display.logicalClipRight() - x);
  if (lastVisibleRow <= firstVisibleRow ||
      lastVisibleColumn <= firstVisibleColumn) return true;
  const uint16_t visibleWidth = static_cast<uint16_t>(
      lastVisibleColumn - firstVisibleColumn);
  const bool transparent = containsTransparency(image);
  TftWriteScope write(display);
  display.setSwapBytes(true);
  for (uint16_t line = static_cast<uint16_t>(firstVisibleRow);
       line < static_cast<uint16_t>(lastVisibleRow); line += kDecodeRows) {
    const uint16_t rows = static_cast<uint16_t>(std::min<int32_t>(
        kDecodeRows, lastVisibleRow - line));
    for (uint16_t row = 0; row < rows; ++row) {
      const size_t sourceRow = static_cast<size_t>(line + row) * image.header.width;
      const size_t destinationRow = static_cast<size_t>(row) * visibleWidth;
      for (uint16_t column = static_cast<uint16_t>(firstVisibleColumn);
           column < static_cast<uint16_t>(lastVisibleColumn); ++column) {
        uint8_t paletteEntry = indexedValue(image, sourceRow + column);
        const uint16_t original = paletteEntry < image.paletteCount
            ? image.palette[paletteEntry] : image.header.transparent;
        if (original != image.header.transparent && paletteEntry >= firstColor &&
            paletteEntry < static_cast<uint16_t>(firstColor + colorCount)) {
          paletteEntry = static_cast<uint8_t>(firstColor +
              (paletteEntry - firstColor + shift) % colorCount);
        }
        scratch.stream.pixels[destinationRow + column - firstVisibleColumn] =
            paletteEntry < image.paletteCount ? image.palette[paletteEntry]
                                              : image.header.transparent;
      }
    }
    if (transparent)
      display.pushImage(x + firstVisibleColumn, y + line, visibleWidth, rows,
                        scratch.stream.pixels, image.header.transparent);
    else
      display.pushImage(x + firstVisibleColumn, y + line, visibleWidth, rows,
                        scratch.stream.pixels);
  }
  return true;
}

bool drawIndexedDithered(PixelCanvas& display, const IndexedImage& image,
                         int16_t x, int16_t y, uint8_t opacity16) {
  if (!image.valid || !image.indices) return false;
  opacity16 = std::min<uint8_t>(16U, opacity16);
  if (opacity16 >= 16U) return drawIndexed(display, image, x, y);
  if (opacity16 == 0U) return true;

  // Bayer order is fixed in screen space. Two consecutive retained frames
  // therefore choose the same pixels and alpha never turns into shimmer.
  static constexpr uint8_t kBayer4x4[16] = {
       0,  8,  2, 10,
      12,  4, 14,  6,
       3, 11,  1,  9,
      15,  7, 13,  5,
  };
  display.damage(x, y, image.header.width, image.header.height);
  const int32_t logicalCanvasTop = -display.getOriginY();
  const int32_t logicalCanvasBottom = logicalCanvasTop + display.height();
  const int32_t firstVisibleRow = std::max<int32_t>(0, logicalCanvasTop - y);
  const int32_t lastVisibleRow = std::min<int32_t>(image.header.height,
                                                   logicalCanvasBottom - y);
  const int32_t firstVisibleColumn = std::max<int32_t>(
      0, display.logicalClipLeft() - x);
  const int32_t lastVisibleColumn = std::min<int32_t>(
      image.header.width, display.logicalClipRight() - x);
  if (lastVisibleRow <= firstVisibleRow ||
      lastVisibleColumn <= firstVisibleColumn) return true;
  const uint16_t visibleWidth = static_cast<uint16_t>(
      lastVisibleColumn - firstVisibleColumn);
  TftWriteScope write(display);
  display.setSwapBytes(true);
  for (uint16_t line = static_cast<uint16_t>(firstVisibleRow);
       line < static_cast<uint16_t>(lastVisibleRow); line += kDecodeRows) {
    const uint16_t rows = static_cast<uint16_t>(std::min<int32_t>(
        kDecodeRows, lastVisibleRow - line));
    for (uint16_t row = 0; row < rows; ++row) {
      const uint16_t localY = static_cast<uint16_t>(line + row);
      const size_t sourceRow = static_cast<size_t>(localY) * image.header.width;
      const size_t destinationRow = static_cast<size_t>(row) * visibleWidth;
      for (uint16_t localX = static_cast<uint16_t>(firstVisibleColumn);
           localX < static_cast<uint16_t>(lastVisibleColumn); ++localX) {
        const uint16_t original = indexedPixel(image, sourceRow + localX);
        const uint8_t threshold = kBayer4x4[
            ((static_cast<uint16_t>(y + localY) & 3U) << 2U) |
            (static_cast<uint16_t>(x + localX) & 3U)];
        scratch.stream.pixels[destinationRow + localX - firstVisibleColumn] =
            original != image.header.transparent && threshold < opacity16
                ? original : image.header.transparent;
      }
    }
    display.pushImage(x + firstVisibleColumn, y + line, visibleWidth, rows,
                      scratch.stream.pixels, image.header.transparent);
  }
  return true;
}

bool drawIndexedPatternMasked(PixelCanvas& display,
                              const IndexedImage& mask,
                              int16_t maskX, int16_t maskY,
                              const IndexedImage& pattern,
                              int16_t sourceOffsetX,
                              int16_t sourceOffsetY,
                              uint8_t opacity16) {
  if (!mask.valid || !mask.indices || !pattern.valid || !pattern.indices ||
      !pattern.header.width || !pattern.header.height) return false;
  opacity16 = std::min<uint8_t>(16U, opacity16);
  if (!opacity16) return true;
  static constexpr uint8_t kBayer4x4[16] = {
       0,  8,  2, 10, 12,  4, 14,  6,
       3, 11,  1,  9, 15,  7, 13,  5,
  };
  display.damage(maskX, maskY, mask.header.width, mask.header.height);
  const int32_t logicalCanvasTop = -display.getOriginY();
  const int32_t logicalCanvasBottom = logicalCanvasTop + display.height();
  const int32_t firstVisibleRow = std::max<int32_t>(0, logicalCanvasTop - maskY);
  const int32_t lastVisibleRow = std::min<int32_t>(mask.header.height,
                                                   logicalCanvasBottom - maskY);
  const int32_t firstVisibleColumn = std::max<int32_t>(
      0, display.logicalClipLeft() - maskX);
  const int32_t lastVisibleColumn = std::min<int32_t>(
      mask.header.width, display.logicalClipRight() - maskX);
  if (lastVisibleRow <= firstVisibleRow ||
      lastVisibleColumn <= firstVisibleColumn) return true;
  const uint16_t visibleWidth = static_cast<uint16_t>(
      lastVisibleColumn - firstVisibleColumn);
  const uint16_t transparent = pattern.header.transparent;
  const auto wrapped = [](int32_t value, uint16_t modulus) -> uint16_t {
    value %= static_cast<int32_t>(modulus);
    if (value < 0) value += modulus;
    return static_cast<uint16_t>(value);
  };
  TftWriteScope write(display);
  display.setSwapBytes(true);
  for (uint16_t line = static_cast<uint16_t>(firstVisibleRow);
       line < static_cast<uint16_t>(lastVisibleRow); line += kDecodeRows) {
    const uint16_t rows = static_cast<uint16_t>(std::min<int32_t>(
        kDecodeRows, lastVisibleRow - line));
    for (uint16_t row = 0; row < rows; ++row) {
      const uint16_t localY = static_cast<uint16_t>(line + row);
      const size_t destinationRow = static_cast<size_t>(row) * visibleWidth;
      for (uint16_t localX = static_cast<uint16_t>(firstVisibleColumn);
           localX < static_cast<uint16_t>(lastVisibleColumn); ++localX) {
        const uint16_t maskColor = indexedPixel(
            mask, static_cast<size_t>(localY) * mask.header.width + localX);
        const uint16_t threshold = kBayer4x4[
            ((static_cast<uint16_t>(maskY + localY) & 3U) << 2U) |
            (static_cast<uint16_t>(maskX + localX) & 3U)];
        uint16_t color = transparent;
        if (maskColor != mask.header.transparent && threshold < opacity16) {
          const uint16_t patternX = wrapped(
              static_cast<int32_t>(localX) + sourceOffsetX,
              pattern.header.width);
          const uint16_t patternY = wrapped(
              static_cast<int32_t>(localY) + sourceOffsetY,
              pattern.header.height);
          const uint16_t candidate = indexedPixel(
              pattern, static_cast<size_t>(patternY) * pattern.header.width + patternX);
          if (candidate != pattern.header.transparent) color = candidate;
        }
        scratch.stream.pixels[destinationRow + localX - firstVisibleColumn] = color;
      }
    }
    display.pushImage(maskX + firstVisibleColumn, maskY + line,
                      visibleWidth, rows,
                      scratch.stream.pixels, transparent);
  }
  return true;
}

bool drawDirectFile(PixelCanvas& display, AssetFile& file, const AssetInfo& info, int16_t x, int16_t y) {
  TftWriteScope write(display);
  if (info.bitsPerPixel == 16U) {
    if (!file.seek(info.pixelOffset)) return false;
    display.setSwapBytes(true);
    for (uint16_t line = 0; line < info.header.height; line += kDecodeRows) {
      const uint16_t rows = static_cast<uint16_t>(std::min<uint16_t>(kDecodeRows, info.header.height - line));
      const size_t count = static_cast<size_t>(info.header.width) * rows;
      if (!readFully(file, reinterpret_cast<uint8_t*>(scratch.stream.pixels), count * sizeof(uint16_t))) return false;
      display.pushImage(x, y + line, info.header.width, rows, scratch.stream.pixels, info.header.transparent);
    }
    return true;
  }

  uint16_t* const palette = directPalette;
  std::memset(palette, 0, sizeof(directPalette));
  if (!file.seek(info.paletteOffset) ||
      !readFully(file, reinterpret_cast<uint8_t*>(palette), static_cast<size_t>(info.paletteCount) * sizeof(uint16_t)) ||
      !file.seek(info.pixelOffset)) return false;
  const bool transparent = std::find(palette, palette + info.paletteCount, info.header.transparent) != palette + info.paletteCount;
  display.setSwapBytes(true);
  size_t pixelStart = 0;
  for (uint16_t line = 0; line < info.header.height; line += kDecodeRows) {
    const uint16_t rows = static_cast<uint16_t>(std::min<uint16_t>(kDecodeRows, info.header.height - line));
    const size_t pixelCount = static_cast<size_t>(info.header.width) * rows;
    const size_t byteCount = info.bitsPerPixel == 4U ? (pixelCount + 1U) / 2U
                           : info.bitsPerPixel == 6U ? (pixelCount * 6U + 7U) / 8U : pixelCount;
    if (!readFully(file, scratch.stream.indices, byteCount)) return false;
    IndexedImage chunk{};
    chunk.valid = true;
    chunk.header = info.header;
    chunk.bitsPerPixel = info.bitsPerPixel;
    chunk.paletteCount = info.paletteCount;
    chunk.palette = palette;
    chunk.indices = scratch.stream.indices;
    chunk.indexBytes = byteCount;
    for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
      const uint8_t color = indexedValue(chunk, pixel);
      scratch.stream.pixels[pixel] = color < info.paletteCount ? palette[color] : info.header.transparent;
    }
    if (transparent) display.pushImage(x, y + line, info.header.width, rows, scratch.stream.pixels, info.header.transparent);
    else display.pushImage(x, y + line, info.header.width, rows, scratch.stream.pixels);
    pixelStart += pixelCount;
  }
  return true;
}

int homeLayerIndex(const char* path) {
  if (!path) return -1;
  if (std::strstr(path, "/home_background_base_")) return 0;
  if (std::strstr(path, "/home_background_foreground_")) return 1;
  return -1;
}

CacheEntry* findCached(const char* path);

// Returns an image only when its complete indexed data is already resident
// in RAM.  Dirty-region rendering must never touch the SD card: a partial
// stream can leave one frame visible long enough to look like a full-screen
// flash on the TFT.
const IndexedImage* residentImage(const char* path) {
  if (!path || !path[0]) return nullptr;
  const int home = homeLayerIndex(path);
  if (home >= 0 && homeLayers[home].image.valid && std::strcmp(homeLayers[home].path, path) == 0)
    return &homeLayers[home].image;
  if (largeAsset.image.valid && std::strcmp(largeAsset.path, path) == 0)
    return &largeAsset.image;
  if (battlePlaneAsset.image.valid &&
      std::strcmp(battlePlaneAsset.path, path) == 0)
    return &battlePlaneAsset.image;
  if (CacheEntry* cached = findCached(path)) return &cached->image;
  return nullptr;
}

bool isBattlePlaneAsset(const char* path) {
  return path && (std::strstr(path, "/battle_anims/backgrounds/") ||
                  std::strstr(path, "/battle_anims/surf_") ||
                  std::strstr(path, "/battle_anims/muddy_water_"));
}

bool shouldBankLargeAsset(const char* path) {
  return path && (std::strstr(path, "/battle_terrain/") ||
                  // Some generated Home themes use one legacy opaque scene
                  // instead of the base/foreground pair. It is far too large
                  // for the 12 KiB icon arena and belongs in the scene bank.
                  std::strstr(path, "/ui/home_background_") ||
                  std::strstr(path, "/shop/background.pkg") ||
                  // Each Pokecenter variant is a full-width scene. Treating
                  // one as an icon exhausts the 12 KiB small-asset arena and
                  // leaves only the procedural fallback visible.
                  std::strstr(path, "/pokecenter/") ||
                  std::strstr(path, "/summary_screen/") ||
                  std::strstr(path, "/evolution_scene/scene.pkg") ||
                  std::strstr(path, "/ui/wild_transition.pkg"));
}

void clearHomeLayers() {
  homeLayers[0] = HomeLayerCache{};
  homeLayers[1] = HomeLayerCache{};
  homeWorkspaceUsed = 0;
  if (sceneWorkspaceOwner == SceneWorkspaceOwner::Home) sceneWorkspaceOwner = SceneWorkspaceOwner::None;
}

void clearLargeAsset() {
  battlePlaneAsset = BattlePlaneCache{};
  battlePlaneWorkspaceEnd = 0;
  largeAsset = LargeAssetCache{};
  largeWorkspaceUsed = 0;
  if (sceneWorkspaceOwner == SceneWorkspaceOwner::Large) sceneWorkspaceOwner = SceneWorkspaceOwner::None;
}

void clearSpriteCache();

bool reserveSceneWorkspace(SceneWorkspaceOwner owner) {
  if (!sceneWorkspace) return false;
  if (sceneWorkspaceOwner != owner) {
    // Menu scenes may borrow the otherwise idle large-scene workspace for
    // icon-heavy manifests (Box/Bag). Before terrain/Home takes ownership,
    // invalidate every descriptor that may point into that borrowed memory.
    if (sceneCacheSpillUsed) clearSpriteCache();
    if (owner == SceneWorkspaceOwner::Home) clearLargeAsset();
    else clearHomeLayers();
    sceneWorkspaceOwner = owner;
  }
  return true;
}

void clearSpriteCache() {
  for (auto& entry : cache) entry = CacheEntry{};
  cacheArenaUsed = 0;
  sceneCacheSpillUsed = sceneWorkspaceOwner == SceneWorkspaceOwner::Large
      ? std::max(largeWorkspaceUsed, battlePlaneWorkspaceEnd) : 0;
  cacheStamp = 0;
}

CacheEntry* findCached(const char* path) {
  if (!path) return nullptr;
  for (auto& entry : cache)
    if (entry.valid && std::strcmp(entry.path, path) == 0) return &entry;
  return nullptr;
}

void preloadFailure(const char* path, const char* reason) {
  // A missing optional visual can be requested many times while one retained
  // scene is composed. Printing each request blocks the 115200-baud serial
  // port and turns a recoverable fallback into a multi-second UI stall.
  static uint32_t lastReportMs = 0;
  static uint16_t suppressed = 0;
  const uint32_t now = millis();
  if (now - lastReportMs < 500U) { ++suppressed; return; }
  Serial.printf("[GFX] scene asset failed: %s (%s), heap=%u block=%u\n", path ? path : "?", reason,
                static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()));
  if (suppressed) Serial.printf("[GFX] suppressed %u repeated asset failures\n", static_cast<unsigned>(suppressed));
  suppressed = 0;
  lastReportMs = now;
}

bool preloadHomeLayer(const char* path) {
  const int layer = homeLayerIndex(path);
  if (layer < 0) return false;
  if (homeLayers[layer].image.valid && std::strcmp(homeLayers[layer].path, path) == 0) return true;
  // A base layer starts a whole new Home scene.  This makes switching themes
  // atomic: the old foreground can never be composited over the new base.
  if (layer == 0 || (layer == 1 && homeLayers[1].image.valid)) clearHomeLayers();
  if (!reserveSceneWorkspace(SceneWorkspaceOwner::Home)) { preloadFailure(path, "home-workspace"); return false; }

  AssetFile file = openAsset(path);
  AssetInfo info{};
  if (!file || !readAssetInfo(file, info)) { file.close(); preloadFailure(path, "home-header"); return false; }
  const size_t reserve = info.bitsPerPixel == 16U
      ? static_cast<size_t>(info.header.width) * info.header.height : info.pixelBytes;
  if (homeWorkspaceUsed + reserve > sceneWorkspaceCapacity) {
    file.close(); preloadFailure(path, "home-capacity"); return false;
  }
  // Load directly into the already-reserved scene descriptor. A temporary
  // HomeLayerCache placed its 512-byte palette plus metadata on loopTask's
  // stack immediately above the deep FatFs open/read chain.
  HomeLayerCache& candidate = homeLayers[layer];
  candidate = HomeLayerCache{};
  if (!loadIndexed(file, info, sceneWorkspace + homeWorkspaceUsed, reserve,
                   candidate.palette, candidate.image)) {
    candidate = HomeLayerCache{};
    file.close(); preloadFailure(path, "home-read"); return false;
  }
  file.close();
  std::strncpy(candidate.path, path, sizeof(candidate.path) - 1);
  homeLayers[layer].image.palette = homeLayers[layer].palette;
  homeWorkspaceUsed += reserve;
  return true;
}

bool preloadLargeAsset(const char* path) {
  if (largeAsset.image.valid && std::strcmp(largeAsset.path, path) == 0) return true;
  clearLargeAsset();
  if (!reserveSceneWorkspace(SceneWorkspaceOwner::Large)) { preloadFailure(path, "large-workspace"); return false; }
  AssetFile file = openAsset(path);
  AssetInfo info{};
  if (!file || !readAssetInfo(file, info)) { file.close(); preloadFailure(path, "large-header"); return false; }
  const size_t reserve = info.bitsPerPixel == 16U
      ? static_cast<size_t>(info.header.width) * info.header.height : info.pixelBytes;
  if (reserve > sceneWorkspaceCapacity) { file.close(); preloadFailure(path, "large-capacity"); return false; }
  LargeAssetCache& candidate = largeAsset;
  candidate = LargeAssetCache{};
  if (!loadIndexed(file, info, sceneWorkspace, reserve, candidate.palette, candidate.image)) {
    candidate = LargeAssetCache{};
    file.close(); preloadFailure(path, "large-read"); return false;
  }
  file.close();
  std::strncpy(candidate.path, path, sizeof(candidate.path) - 1);
  largeAsset.image.palette = largeAsset.palette;
  largeWorkspaceUsed = reserve;
  sceneCacheSpillUsed = reserve;
  return true;
}

bool preloadBattlePlane(const char* path) {
  if (!path || !path[0]) return false;
  if (battlePlaneAsset.image.valid &&
      std::strcmp(battlePlaneAsset.path, path) == 0)
    return true;
  if (sceneWorkspaceOwner != SceneWorkspaceOwner::Large ||
      !largeAsset.image.valid || !sceneWorkspace) {
    preloadFailure(path, "battle-plane-without-terrain");
    return false;
  }

  // Any spilled small sprite lives above the previous plane and becomes
  // invalid when that plane changes size.  Discard those descriptors first;
  // the caller's scene manifest reloads the two battlers and all effect cels
  // before composition resumes.
  clearSpriteCache();
  battlePlaneAsset = BattlePlaneCache{};
  battlePlaneWorkspaceEnd = 0;
  sceneCacheSpillUsed = largeWorkspaceUsed;

  AssetFile file = openAsset(path);
  AssetInfo info{};
  if (!file || !readAssetInfo(file, info)) {
    file.close(); preloadFailure(path, "battle-plane-header"); return false;
  }
  const size_t paletteBytes = info.bitsPerPixel == 16U
      ? sizeof(uint16_t) * 256U
      : static_cast<size_t>(info.paletteCount) * sizeof(uint16_t);
  const size_t pixelBytes = info.bitsPerPixel == 16U
      ? static_cast<size_t>(info.header.width) * info.header.height
      : info.pixelBytes;
  const size_t offset = (largeWorkspaceUsed + 1U) & ~size_t(1U);
  const size_t end = offset + paletteBytes + pixelBytes;
  if (end > sceneWorkspaceCapacity) {
    file.close(); preloadFailure(path, "battle-plane-capacity"); return false;
  }
  uint16_t* palette = reinterpret_cast<uint16_t*>(sceneWorkspace + offset);
  if (!loadIndexed(file, info, sceneWorkspace + offset + paletteBytes,
                   pixelBytes, palette, battlePlaneAsset.image)) {
    battlePlaneAsset = BattlePlaneCache{};
    file.close(); preloadFailure(path, "battle-plane-read"); return false;
  }
  file.close();
  std::strncpy(battlePlaneAsset.path, path,
               sizeof(battlePlaneAsset.path) - 1U);
  battlePlaneWorkspaceEnd = end;
  sceneCacheSpillUsed = end;
  return true;
}

bool cacheSmallAsset(const char* path) {
  if (CacheEntry* cached = findCached(path)) { cached->stamp = ++cacheStamp; return true; }
  CacheEntry* slot = nullptr;
  for (auto& entry : cache) if (!entry.valid) { slot = &entry; break; }
  if (!slot || !cacheArena) { preloadFailure(path, "cache-slots"); return false; }

  AssetFile file = openAsset(path);
  AssetInfo info{};
  if (!file || !readAssetInfo(file, info)) { file.close(); preloadFailure(path, "header"); return false; }
  const size_t paletteBytes = info.bitsPerPixel == 16U ? sizeof(uint16_t) * 256U
      : static_cast<size_t>(info.paletteCount) * sizeof(uint16_t);
  const size_t indexBytes = info.bitsPerPixel == 16U
      ? static_cast<size_t>(info.header.width) * info.header.height : info.pixelBytes;
  const size_t reserve = paletteBytes + indexBytes;
  uint8_t* destinationArena = nullptr;
  size_t destinationOffset = 0;
  const size_t primaryOffset = (cacheArenaUsed + 1U) & ~size_t(1U);
  const size_t spillOffset = (sceneCacheSpillUsed + 1U) & ~size_t(1U);
  if (primaryOffset + reserve <= kCacheArenaBytes) {
    destinationArena = cacheArena;
    destinationOffset = primaryOffset;
  } else if ((sceneWorkspaceOwner == SceneWorkspaceOwner::None || sceneWorkspaceOwner == SceneWorkspaceOwner::Large) && sceneWorkspace &&
             spillOffset + reserve <= sceneWorkspaceCapacity) {
    destinationArena = sceneWorkspace;
    destinationOffset = spillOffset;
  } else {
    file.close(); preloadFailure(path, "scene-cache-full"); return false;
  }
  uint16_t* palette = reinterpret_cast<uint16_t*>(destinationArena + destinationOffset);
  IndexedImage image{};
  if (!loadIndexed(file, info, destinationArena + destinationOffset + paletteBytes,
                   indexBytes, palette, image)) {
    file.close(); preloadFailure(path, "read"); return false;
  }
  file.close();
  slot->valid = true;
  std::strncpy(slot->path, path, sizeof(slot->path) - 1);
  slot->image = image;
  slot->stamp = ++cacheStamp;
  if (destinationArena == cacheArena) cacheArenaUsed = destinationOffset + reserve;
  else sceneCacheSpillUsed = destinationOffset + reserve;
  return true;
}

bool drawHomeLayer(PixelCanvas& display, const char* path, int16_t x, int16_t y) {
  const int layer = homeLayerIndex(path);
  if (layer < 0 || !homeLayers[layer].image.valid || std::strcmp(homeLayers[layer].path, path) != 0) return false;
  return drawIndexed(display, homeLayers[layer].image, x, y);
}

bool drawLargeAsset(PixelCanvas& display, const char* path, int16_t x, int16_t y) {
  return path && largeAsset.image.valid && std::strcmp(largeAsset.path, path) == 0 &&
      drawIndexed(display, largeAsset.image, x, y);
}

}  // namespace

void AssetRenderer::storageUnmounted() {
  archiveBatchDepth = 0;
  archiveRangeCacheCount = 0;
  if (archiveBatchFile) archiveBatchFile.close();
  archiveAvailable = false;
  archiveHeader = AssetArchiveHeader{};
}

bool AssetRenderer::usingArchive() { return archiveAvailable; }

void AssetRenderer::storageMounted() {
  archiveBatchDepth = 0;
  archiveRangeCacheCount = 0;
  if (archiveBatchFile) archiveBatchFile.close();
  File archive = SD.open(kAssetArchivePath, FILE_READ);
  archiveAvailable = archive && readArchiveHeader(archive, archiveHeader, false);
  if (archive) archive.close();
  Serial.printf("[GFX] indexed scene renderer: %s asset source\n",
                archiveAvailable ? "PGA3 versioned" : "individual PKG fallback");
}

bool AssetRenderer::beginPreloadBatch() {
  if (archiveBatchDepth) {
    ++archiveBatchDepth;
    return archiveAvailable && archiveBatchFile;
  }
  archiveBatchDepth = 1;
  archiveRangeCacheCount = 0;
  if (!archiveAvailable) return false;
  ++assetOpenCount;
  archiveBatchFile = SD.open(kAssetArchivePath, FILE_READ);
  AssetArchiveHeader opened{};
  if (!archiveBatchFile || !readArchiveHeader(archiveBatchFile, opened) ||
      opened.assetCount != archiveHeader.assetCount || opened.dataOffset != archiveHeader.dataOffset) {
    if (archiveBatchFile) archiveBatchFile.close();
    archiveAvailable = false;
    Serial.println("[GFX] PGA3 batch unavailable; falling back after this transaction");
    return false;
  }
  return true;
}

bool AssetRenderer::primePreload(const char* path) {
  if (!path || !path[0]) return false;
  if (isPreloaded(path) || !archiveBatchDepth || !archiveAvailable) return true;
  uint32_t offset = 0;
  uint32_t length = 0;
  return archiveAssetRange(path, offset, length);
}

void AssetRenderer::endPreloadBatch() {
  if (!archiveBatchDepth) return;
  --archiveBatchDepth;
  if (!archiveBatchDepth && archiveBatchFile) archiveBatchFile.close();
  if (!archiveBatchDepth) archiveRangeCacheCount = 0;
}

bool AssetRenderer::begin() {
  // The large indexed scene is the sole large heap allocation. The compact
  // sprite arena is static, so this allocation cannot split the block later
  // needed by FAT registration.
  if (!sceneWorkspace) {
    sceneWorkspace = new (std::nothrow) uint8_t[kSceneWorkspaceBytes];
    sceneWorkspaceCapacity = sceneWorkspace ? kSceneWorkspaceBytes : 0;
  }
  const bool ready = sceneWorkspace != nullptr;
  Serial.printf("[GFX] workspace %s scene=%s/%u cache=PENDING/%u heap=%u block=%u\n", ready ? "READY" : "FAIL",
                sceneWorkspace ? "OK" : "FAIL", static_cast<unsigned>(kSceneWorkspaceBytes),
                static_cast<unsigned>(kCacheArenaBytes),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
  return ready;
}

void AssetRenderer::releaseSceneWorkspaceForRadio() {
  // No descriptor may survive deletion because cache entries are allowed to
  // spill into this same arena.
  clearCache();
  delete[] sceneWorkspace;
  sceneWorkspace = nullptr;
  sceneWorkspaceCapacity = 0;
  sceneWorkspaceOwner = SceneWorkspaceOwner::None;
  Serial.printf("[GFX] scene workspace released for BLE heap=%u block=%u\n",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
}

bool AssetRenderer::restoreSceneWorkspace(bool radioActive) {
  if (sceneWorkspace) return true;
  size_t requested = kSceneWorkspaceBytes;
  if (radioActive) {
    // Leave enough headroom for a connection, scan results and packet
    // assembly. The compact 12 KiB icon arena remains available regardless;
    // this smaller bank is primarily for Box/Party and battle assets while a
    // peer connection stays alive.
    constexpr size_t kRadioHeadroom = 12U * 1024U;
    const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    requested = largest > kRadioHeadroom ? largest - kRadioHeadroom : 0;
    requested = std::min(requested, kSceneWorkspaceBytes);
    requested &= ~size_t(1023U);
  } else {
    // NimBLE releases its memory in several adjacent blocks rather than
    // recreating the exact 64 KiB block reserved at boot. Keep only a tiny
    // allocator margin and reclaim the largest possible scene bank. 61,104
    // bytes is enough for the largest two-layer Home theme.
    const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (largest <= requested && largest > 256U) requested = (largest - 128U) & ~size_t(3U);
  }
  if (requested) sceneWorkspace = new (std::nothrow) uint8_t[requested];
  sceneWorkspaceCapacity = sceneWorkspace ? requested : 0;
  Serial.printf("[GFX] scene workspace %s mode=%s size=%u heap=%u block=%u\n",
                sceneWorkspace ? "READY" : "UNAVAILABLE", radioActive ? "BLE" : "NORMAL",
                static_cast<unsigned>(sceneWorkspaceCapacity),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
  return sceneWorkspace != nullptr;
}

bool AssetRenderer::beginCache() {
  Serial.printf("[GFX] sprite cache %s size=%u heap=%u block=%u\n", cacheArena ? "READY" : "FAIL",
                static_cast<unsigned>(kCacheArenaBytes),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
  return cacheArena != nullptr;
}

void* AssetRenderer::transientScratch(size_t& capacity) {
  capacity = sizeof(scratch);
  return &scratch;
}

void* AssetRenderer::bootScratch(size_t& capacity) {
  capacity = sceneWorkspaceCapacity;
  return sceneWorkspace;
}

void AssetRenderer::clearCache() {
  clearSpriteCache();
  clearHomeLayers();
  clearLargeAsset();
}

void AssetRenderer::clearSmallCache() { clearSpriteCache(); }

void AssetRenderer::invalidateHomeSceneCache() { clearHomeLayers(); }

bool AssetRenderer::takeIoFailure() {
  const bool failed = assetIoFailed;
  assetIoFailed = false;
  return failed;
}

void AssetRenderer::setCompositionActive(bool active) { compositionActive = active; }

uint32_t AssetRenderer::sdOpenCount() { return assetOpenCount; }

bool AssetRenderer::preload(const char* path) {
  if (!path || !path[0]) return false;
  // UI screens historically called preload() without opening an archive
  // batch. openAsset() consequently found their old loose PKG mirrors first,
  // even after OTAP had installed a newer canonical pokegochi.pak. Give every
  // preload the same archive semantics as battle animations. Multi-asset
  // scenes still open one outer batch and avoid one FAT open per sprite.
  const bool ownsBatch = archiveAvailable && archiveBatchDepth == 0U;
  if (ownsBatch && !beginPreloadBatch()) {
    endPreloadBatch();
    return false;
  }
  bool ready = false;
  if (homeLayerIndex(path) >= 0) ready = preloadHomeLayer(path);
  else if (isBattlePlaneAsset(path)) ready = preloadBattlePlane(path);
  else if (shouldBankLargeAsset(path)) ready = preloadLargeAsset(path);
  else ready = cacheSmallAsset(path);
  if (ownsBatch) endPreloadBatch();
  return ready;
}

bool AssetRenderer::prepareBattlePlane(const char* path) {
  if (path && path[0] && battlePlaneAsset.image.valid &&
      std::strcmp(battlePlaneAsset.path, path) == 0)
    return true;
  // A new move gets a deterministic small-asset arena.  Descriptors which
  // spilled above the old plane cannot survive a plane resize.
  clearSpriteCache();
  battlePlaneAsset = BattlePlaneCache{};
  battlePlaneWorkspaceEnd = 0;
  sceneCacheSpillUsed = sceneWorkspaceOwner == SceneWorkspaceOwner::Large
      ? largeWorkspaceUsed : 0;
  return !path || !path[0] || preloadBattlePlane(path);
}

bool AssetRenderer::isPreloaded(const char* path) {
  if (!path || !path[0]) return false;
  const int home = homeLayerIndex(path);
  if (home >= 0)
    return homeLayers[home].image.valid && std::strcmp(homeLayers[home].path, path) == 0;
  if (isBattlePlaneAsset(path))
    return battlePlaneAsset.image.valid &&
        std::strcmp(battlePlaneAsset.path, path) == 0;
  if (shouldBankLargeAsset(path))
    return largeAsset.image.valid && std::strcmp(largeAsset.path, path) == 0;
  return findCached(path) != nullptr;
}

bool AssetRenderer::dimensions(const char* path, uint16_t& width, uint16_t& height) {
  width = height = 0;
  if (!path || !path[0]) return false;
  const IndexedImage* image = residentImage(path);
  if (!image || !image->valid) return false;
  width = image->header.width;
  height = image->header.height;
  return width != 0 && height != 0;
}

bool AssetRenderer::draw(PixelCanvas& display, const char* path, int16_t x, int16_t y, bool useCache) {
  if (!path || !path[0]) return false;
  if (useCache && homeLayerIndex(path) >= 0) {
    if (drawHomeLayer(display, path, x, y)) return true;
    if (compositionActive) return false;
    if (preloadHomeLayer(path)) return drawHomeLayer(display, path, x, y);
    return false;
  }
  if (useCache && isBattlePlaneAsset(path)) {
    if (battlePlaneAsset.image.valid &&
        std::strcmp(battlePlaneAsset.path, path) == 0)
      return drawIndexed(display, battlePlaneAsset.image, x, y);
    if (compositionActive) return false;
    if (preloadBattlePlane(path))
      return drawIndexed(display, battlePlaneAsset.image, x, y);
    return false;
  }
  if (useCache && shouldBankLargeAsset(path)) {
    if (drawLargeAsset(display, path, x, y)) return true;
    if (compositionActive) return false;
    if (preloadLargeAsset(path)) return drawLargeAsset(display, path, x, y);
    return false;
  }
  // Even a caller that permits one-off streaming should always consume a
  // resident copy first. Box previously passed useCache=false for its grid
  // and consequently reopened already-preloaded icons once per band.
  if (CacheEntry* cached = findCached(path)) {
    cached->stamp = ++cacheStamp;
    return drawIndexed(display, cached->image, x, y);
  }
  // Compositing a retained band is a hard no-I/O phase. Missing optional art
  // uses the caller's fallback and is prepared on the next scene transition.
  if (compositionActive) return false;
  if (useCache) {
    if (preload(path)) {
      if (CacheEntry* cached = findCached(path)) return drawIndexed(display, cached->image, x, y);
    }
  }

  // A non-scene, one-off image may be streamed during its initial static
  // draw. No timed Home/Battle frame ever takes this path: those sprites are
  // required to be preloaded by their scene manifests.
  AssetFile file = openAsset(path);
  AssetInfo info{};
  const bool okay = file && readAssetInfo(file, info) && drawDirectFile(display, file, info, x, y);
  file.close();
  return okay;
}

bool AssetRenderer::drawMetallic(PixelCanvas& display, const char* path,
                                 int16_t x, int16_t y, uint8_t phase) {
  if (!path || !path[0]) return false;
  const IndexedImage* image = residentImage(path);
  if (!image || !image->valid) return false;
  return drawIndexedMetallic(display, *image, x, y, phase);
}

bool AssetRenderer::drawHorizontalMirror(PixelCanvas& display, const char* path,
                                         int16_t x, int16_t y) {
  if (!path || !path[0]) return false;
  const IndexedImage* image = residentImage(path);
  if (!image || !image->valid) return false;
  return drawIndexedHorizontalMirror(display, *image, x, y);
}

bool AssetRenderer::drawTransformed(PixelCanvas& display, const char* path,
                                    int16_t centerX, int16_t centerY,
                                    uint8_t scaleXPercent, uint8_t scaleYPercent,
                                    AssetColorEffect colorEffect,
                                    uint16_t blendColor, uint8_t blendAmount) {
  if (!path || !path[0]) return false;
  const IndexedImage* image = residentImage(path);
  if (!image || !image->valid) return false;
  return drawIndexedTransformed(display, *image, centerX, centerY,
                                scaleXPercent, scaleYPercent, colorEffect,
                                blendColor, blendAmount);
}

bool AssetRenderer::drawAffine(PixelCanvas& display, const char* path,
                               int16_t centerX, int16_t centerY,
                               int16_t scaleXPercent, int16_t scaleYPercent,
                               uint8_t rotation, uint8_t opacity16,
                               AssetColorEffect colorEffect,
                               uint16_t blendColor, uint8_t blendAmount) {
  if (!path || !path[0]) return false;
  const IndexedImage* image = residentImage(path);
  if (!image || !image->valid) return false;
  return drawIndexedAffine(display, *image, centerX, centerY,
                           scaleXPercent, scaleYPercent, rotation, opacity16,
                           colorEffect, blendColor, blendAmount);
}

bool AssetRenderer::drawPaletteCycled(PixelCanvas& display, const char* path,
                                      int16_t x, int16_t y,
                                      uint8_t firstColor, uint8_t colorCount,
                                      uint8_t shift) {
  if (!path || !path[0]) return false;
  const IndexedImage* image = residentImage(path);
  if (!image || !image->valid) return false;
  return drawIndexedPaletteCycled(display, *image, x, y, firstColor,
                                  colorCount, shift);
}

bool AssetRenderer::drawDithered(PixelCanvas& display, const char* path,
                                  int16_t x, int16_t y, uint8_t opacity16) {
  if (!path || !path[0]) return false;
  const IndexedImage* image = residentImage(path);
  if (!image || !image->valid) return false;
  return drawIndexedDithered(display, *image, x, y, opacity16);
}

bool AssetRenderer::drawPatternMasked(PixelCanvas& display,
                                      const char* maskPath,
                                      int16_t maskX, int16_t maskY,
                                      const char* patternPath,
                                      int16_t sourceOffsetX,
                                      int16_t sourceOffsetY,
                                      uint8_t opacity16) {
  if (!maskPath || !maskPath[0] || !patternPath || !patternPath[0]) return false;
  const IndexedImage* mask = residentImage(maskPath);
  const IndexedImage* pattern = residentImage(patternPath);
  if (!mask || !mask->valid || !pattern || !pattern->valid) return false;
  return drawIndexedPatternMasked(display, *mask, maskX, maskY, *pattern,
                                  sourceOffsetX, sourceOffsetY, opacity16);
}

bool AssetRenderer::drawScrolled(PixelCanvas& display, const char* path,
                                 int16_t x, int16_t y, uint16_t width,
                                 uint16_t height, int16_t sourceOffsetX,
                                 int16_t sourceOffsetY, uint8_t opacity16,
                                 uint8_t paletteFirst, uint8_t paletteCount,
                                 uint8_t paletteShift,
                                 uint8_t scaleNumerator,
                                 uint8_t scaleDenominator,
                                 bool wrapX, bool wrapY,
                                 uint16_t virtualSourceWidth,
                                 uint16_t virtualSourceHeight,
                                 uint8_t virtualFillIndex) {
  if (!path || !path[0]) return false;
  const IndexedImage* image = residentImage(path);
  if (!image || !image->valid) return false;
  return drawIndexedScrolled(display, *image, x, y, width, height,
                             sourceOffsetX, sourceOffsetY, opacity16,
                             paletteFirst, paletteCount, paletteShift,
                             scaleNumerator, scaleDenominator, wrapX, wrapY,
                             virtualSourceWidth, virtualSourceHeight,
                             virtualFillIndex);
}

bool AssetRenderer::drawPlaneComposite(PixelCanvas& display,
                                       const AssetOverlay& basePlane,
                                       const AssetOverlay& overlayPlane,
                                       int16_t x, int16_t y,
                                       uint16_t width, uint16_t height) {
  if (!basePlane.path || !overlayPlane.path || !width || !height ||
      width > 320U) return false;
  const IndexedImage* baseImage = residentImage(basePlane.path);
  const IndexedImage* overlayImage = residentImage(overlayPlane.path);
  if (!baseImage || !overlayImage || !baseImage->valid ||
      !overlayImage->valid) return false;

  const auto sourceCoordinate = [](int32_t screen, int16_t destination,
                                   int16_t offset, uint16_t sourceExtent,
                                   uint8_t numerator, uint8_t denominator,
                                   bool wrap) -> uint16_t {
    const int32_t unbounded = (screen - destination) *
        std::max<uint8_t>(1U, denominator) /
        std::max<uint8_t>(1U, numerator) + offset;
    return wrap ? positiveModulo(unbounded, sourceExtent)
                : static_cast<uint16_t>(std::clamp<int32_t>(
                      unbounded, 0, sourceExtent - 1U));
  };
  const uint16_t overlayWidth = overlayPlane.width
      ? overlayPlane.width : width;
  const uint16_t overlayHeight = overlayPlane.height
      ? overlayPlane.height : height;
  const uint16_t baseSourceWidth = std::max<uint16_t>(
      baseImage->header.width, basePlane.virtualSourceWidth);
  const uint16_t baseSourceHeight = std::max<uint16_t>(
      baseImage->header.height, basePlane.virtualSourceHeight);
  const uint16_t overlaySourceWidth = std::max<uint16_t>(
      overlayImage->header.width, overlayPlane.virtualSourceWidth);
  const uint16_t overlaySourceHeight = std::max<uint16_t>(
      overlayImage->header.height, overlayPlane.virtualSourceHeight);
  const int32_t logicalCanvasTop = -display.getOriginY();
  const int32_t logicalCanvasBottom = logicalCanvasTop + display.height();
  const int32_t firstVisibleRow = std::max<int32_t>(0, logicalCanvasTop - y);
  const int32_t lastVisibleRow = std::min<int32_t>(height,
                                                   logicalCanvasBottom - y);
  const int32_t firstVisibleColumn = std::max<int32_t>(
      0, display.logicalClipLeft() - x);
  const int32_t lastVisibleColumn = std::min<int32_t>(
      width, display.logicalClipRight() - x);
  if (lastVisibleRow <= firstVisibleRow ||
      lastVisibleColumn <= firstVisibleColumn) return true;
  const uint16_t visibleWidth = static_cast<uint16_t>(
      lastVisibleColumn - firstVisibleColumn);

  TftWriteScope write(display);
  display.setSwapBytes(true);
  for (uint16_t line = static_cast<uint16_t>(firstVisibleRow);
       line < static_cast<uint16_t>(lastVisibleRow); line += kDecodeRows) {
    const uint16_t rows = static_cast<uint16_t>(std::min<int32_t>(
        kDecodeRows, lastVisibleRow - line));
    for (uint16_t row = 0; row < rows; ++row) {
      const int32_t screenY = y + line + row;
      for (uint16_t column = static_cast<uint16_t>(firstVisibleColumn);
           column < static_cast<uint16_t>(lastVisibleColumn); ++column) {
        const int32_t screenX = x + column;
        const uint16_t baseX = sourceCoordinate(
            screenX, basePlane.x, basePlane.sourceOffsetX,
            baseSourceWidth, basePlane.scaleNumerator,
            basePlane.scaleDenominator,
            basePlane.wrap ? basePlane.wrapX : false);
        const uint16_t baseY = sourceCoordinate(
            screenY, basePlane.y, basePlane.sourceOffsetY,
            baseSourceHeight, basePlane.scaleNumerator,
            basePlane.scaleDenominator,
            basePlane.wrap ? basePlane.wrapY : false);
        uint16_t pixel = (baseX < baseImage->header.width &&
                          baseY < baseImage->header.height)
            ? indexedPalettePixel(*baseImage, baseX, baseY,
                                  basePlane.paletteFirst,
                                  basePlane.paletteCount,
                                  basePlane.paletteShift)
            : indexedPaletteIndexPixel(*baseImage,
                  basePlane.virtualFillIndex, basePlane.paletteFirst,
                  basePlane.paletteCount, basePlane.paletteShift);

        const bool insideOverlay =
            screenX >= overlayPlane.x && screenY >= overlayPlane.y &&
            screenX < static_cast<int32_t>(overlayPlane.x) + overlayWidth &&
            screenY < static_cast<int32_t>(overlayPlane.y) + overlayHeight;
        if (insideOverlay && overlayPlane.opacity16) {
          const uint16_t overlayX = sourceCoordinate(
              screenX, overlayPlane.x, overlayPlane.sourceOffsetX,
              overlaySourceWidth, overlayPlane.scaleNumerator,
              overlayPlane.scaleDenominator,
              overlayPlane.wrap ? overlayPlane.wrapX : false);
          const uint16_t overlayY = sourceCoordinate(
              screenY, overlayPlane.y, overlayPlane.sourceOffsetY,
              overlaySourceHeight, overlayPlane.scaleNumerator,
              overlayPlane.scaleDenominator,
              overlayPlane.wrap ? overlayPlane.wrapY : false);
          const uint16_t foreground =
              (overlayX < overlayImage->header.width &&
               overlayY < overlayImage->header.height)
                  ? indexedPalettePixel(*overlayImage, overlayX, overlayY,
                        overlayPlane.paletteFirst, overlayPlane.paletteCount,
                        overlayPlane.paletteShift)
                  : indexedPaletteIndexPixel(*overlayImage,
                        overlayPlane.virtualFillIndex,
                        overlayPlane.paletteFirst,
                        overlayPlane.paletteCount,
                        overlayPlane.paletteShift);
          if (foreground != overlayImage->header.transparent)
            pixel = alphaBlend565(foreground, pixel,
                                  overlayPlane.opacity16);
        }
        scratch.stream.pixels[static_cast<size_t>(row) * visibleWidth +
                              column - firstVisibleColumn] = pixel;
      }
    }
    display.pushImage(x + firstVisibleColumn, y + line,
                      visibleWidth, rows, scratch.stream.pixels);
  }
  return true;
}

bool AssetRenderer::drawRegion(PixelCanvas& display, const char* path, int16_t x, int16_t y,
                               uint16_t sourceX, uint16_t sourceY, uint16_t width,
                               uint16_t height, bool mirrorX) {
  if (!path || !width || !height || width > kRegionBufferWidth) return false;
  const IndexedImage* image = nullptr;
  const int home = homeLayerIndex(path);
  if (home >= 0 && homeLayers[home].image.valid && std::strcmp(homeLayers[home].path, path) == 0)
    image = &homeLayers[home].image;
  else if (largeAsset.image.valid && std::strcmp(largeAsset.path, path) == 0)
    image = &largeAsset.image;
  else if (CacheEntry* cached = findCached(path)) image = &cached->image;
  // Streaming partial regions was the root of Home flicker.  A dirty-region
  // draw is RAM-only by design; callers retry only after the complete scene
  // is prepared again.
  if (!image || sourceX >= image->header.width || sourceY >= image->header.height) return false;
  const uint16_t clippedWidth = static_cast<uint16_t>(std::min<uint32_t>(width, image->header.width - sourceX));
  const uint16_t clippedHeight = static_cast<uint16_t>(std::min<uint32_t>(height, image->header.height - sourceY));
  TftWriteScope write(display);
  display.setSwapBytes(true);
  const bool transparent = containsTransparency(*image);
  for (uint16_t line = 0; line < clippedHeight;
       line = static_cast<uint16_t>(line + kRegionBufferHeight)) {
    const uint16_t rows = static_cast<uint16_t>(std::min<uint16_t>(
        kRegionBufferHeight, clippedHeight - line));
    for (uint16_t row = 0; row < rows; ++row)
      for (uint16_t column = 0; column < clippedWidth; ++column)
        scratch.region[static_cast<size_t>(row) * clippedWidth + column] = indexedPixel(
            *image, static_cast<size_t>(sourceY + line + row) *
                        image->header.width + sourceX +
                        (mirrorX ? clippedWidth - 1U - column : column));
    if (transparent)
      display.pushImage(x, y + line, clippedWidth, rows, scratch.region,
                        image->header.transparent);
    else
      display.pushImage(x, y + line, clippedWidth, rows, scratch.region);
  }
  return true;
}

bool AssetRenderer::drawCompositeRegion(PixelCanvas& display, const char* basePath,
                                        const char* foregroundPath, int16_t x, int16_t y,
                                        uint16_t sourceX, uint16_t sourceY,
                                        uint16_t width, uint16_t height,
                                        const AssetOverlay* overlays, uint8_t overlayCount) {
  if (!basePath || !width || !height || width > kRegionBufferWidth) return false;
  const IndexedImage* baseImage = residentImage(basePath);
  const IndexedImage* foregroundImage = foregroundPath && foregroundPath[0] ? residentImage(foregroundPath) : nullptr;
  if (!baseImage || (foregroundPath && foregroundPath[0] && !foregroundImage)) return false;
  const IndexedImage& base = *baseImage;
  if (!base.valid || sourceX >= base.header.width || sourceY >= base.header.height) return false;
  if (foregroundImage && (!foregroundImage->valid || sourceX >= foregroundImage->header.width ||
                          sourceY >= foregroundImage->header.height)) return false;
  uint16_t maximumWidth = static_cast<uint16_t>(base.header.width - sourceX);
  uint16_t maximumHeight = static_cast<uint16_t>(base.header.height - sourceY);
  if (foregroundImage) {
    maximumWidth = std::min<uint16_t>(maximumWidth, foregroundImage->header.width - sourceX);
    maximumHeight = std::min<uint16_t>(maximumHeight, foregroundImage->header.height - sourceY);
  }
  const uint16_t clippedWidth = static_cast<uint16_t>(std::min<uint32_t>(width, maximumWidth));
  const uint16_t clippedHeight = static_cast<uint16_t>(std::min<uint32_t>(height, maximumHeight));
  if (!clippedWidth || !clippedHeight) return false;

  const int16_t right = x + clippedWidth;
  TftWriteScope write(display);
  display.setSwapBytes(true);
  for (uint16_t line = 0; line < clippedHeight;
       line = static_cast<uint16_t>(line + kRegionBufferHeight)) {
    const uint16_t rows = static_cast<uint16_t>(std::min<uint16_t>(
        kRegionBufferHeight, clippedHeight - line));
    const int16_t chunkY = static_cast<int16_t>(y + line);
    const int16_t bottom = static_cast<int16_t>(chunkY + rows);

    for (uint16_t row = 0; row < rows; ++row)
      for (uint16_t column = 0; column < clippedWidth; ++column)
        scratch.region[static_cast<size_t>(row) * clippedWidth + column] = indexedPixel(
            base, static_cast<size_t>(sourceY + line + row) *
                      base.header.width + sourceX + column);

    for (uint8_t item = 0; overlays && item < overlayCount; ++item) {
      const IndexedImage* resident = residentImage(overlays[item].path);
      if (!resident) return false;  // Never replace one frame with SD I/O.
      const IndexedImage& image = *resident;
      const uint16_t destinationWidth = overlays[item].width
          ? overlays[item].width : image.header.width;
      const uint16_t destinationHeight = overlays[item].height
          ? overlays[item].height : image.header.height;
      const int16_t left = std::max<int16_t>(x, overlays[item].x);
      const int16_t top = std::max<int16_t>(chunkY, overlays[item].y);
      const int16_t spriteRight = std::min<int16_t>(right,
          static_cast<int16_t>(overlays[item].x +
              (overlays[item].wrap && !overlays[item].width
                  ? right - overlays[item].x : destinationWidth)));
      const int16_t spriteBottom = std::min<int16_t>(bottom,
          static_cast<int16_t>(overlays[item].y +
              (overlays[item].wrap && !overlays[item].height
                  ? bottom - overlays[item].y : destinationHeight)));
      for (int16_t screenY = top; screenY < spriteBottom; ++screenY) {
        for (int16_t screenX = left; screenX < spriteRight; ++screenX) {
          const int32_t unboundedX =
              static_cast<int32_t>(screenX - overlays[item].x) *
                  overlays[item].scaleDenominator /
                  std::max<uint8_t>(1U, overlays[item].scaleNumerator) +
                  overlays[item].sourceOffsetX;
          const int32_t unboundedY =
              static_cast<int32_t>(screenY - overlays[item].y) *
                  overlays[item].scaleDenominator /
                  std::max<uint8_t>(1U, overlays[item].scaleNumerator) +
                  overlays[item].sourceOffsetY;
          uint16_t localX = overlays[item].wrap && overlays[item].wrapX
              ? positiveModulo(unboundedX, image.header.width)
              : static_cast<uint16_t>(std::clamp<int32_t>(
                    unboundedX, 0, image.header.width - 1U));
          const uint16_t localY = overlays[item].wrap && overlays[item].wrapY
              ? positiveModulo(unboundedY, image.header.height)
              : static_cast<uint16_t>(std::clamp<int32_t>(
                    unboundedY, 0, image.header.height - 1U));
          if (overlays[item].mirrorX)
            localX = static_cast<uint16_t>(image.header.width - 1U - localX);
          const uint16_t pixel = indexedPalettePixel(
              image, localX, localY, overlays[item].paletteFirst,
              overlays[item].paletteCount, overlays[item].paletteShift);
          if (pixel != image.header.transparent) {
            const size_t destination =
                static_cast<size_t>(screenY - chunkY) * clippedWidth +
                (screenX - x);
            scratch.region[destination] = alphaBlend565(
                pixel, scratch.region[destination], overlays[item].opacity16);
          }
        }
      }
    }

    if (foregroundImage) {
      const IndexedImage& foreground = *foregroundImage;
      for (uint16_t row = 0; row < rows; ++row) {
        for (uint16_t column = 0; column < clippedWidth; ++column) {
          const uint16_t pixel = indexedPixel(foreground,
              static_cast<size_t>(sourceY + line + row) *
                  foreground.header.width + sourceX + column);
          if (pixel != foreground.header.transparent)
            scratch.region[static_cast<size_t>(row) * clippedWidth + column] = pixel;
        }
      }
    }
    display.pushImage(x, chunkY, clippedWidth, rows, scratch.region);
  }
  return true;
}
