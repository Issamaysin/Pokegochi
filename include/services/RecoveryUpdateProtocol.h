#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace PokegochiRecovery {

constexpr uint8_t kVersion = 1U;
constexpr char kNamespace[] = "pokegochi-rec";
constexpr char kMarkerKey[] = "marker";
constexpr char kFirmwarePartPath[] = "/pokegochi/update/firmware.part";
constexpr char kFirmwareReadyPath[] = "/pokegochi/update/firmware.ready";
constexpr char kPreviousImagePath[] = "/pokegochi/update/firmware.previous";
constexpr char kMainPartitionLabel[] = "app";
constexpr char kRecoveryPartitionLabel[] = "rescue";

enum class Phase : uint8_t {
  None = 0,
  Staged = 1,
  Candidate = 2,
  Restored = 3,
};

#pragma pack(push, 1)
struct Marker {
  char magic[4];
  uint8_t version;
  uint8_t phase;
  uint16_t structureSize;
  uint32_t packageSequence;
  uint32_t firmwareSize;
  uint32_t previousPartitionSize;
  uint8_t firmwareSha256[32];
  uint8_t previousSha256[32];
  uint32_t checksum;
};
#pragma pack(pop)

static_assert(sizeof(Marker) == 88U, "Recovery marker ABI changed");

inline uint32_t crc32(const void* data, size_t length) {
  uint32_t value = 0xFFFFFFFFUL;
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  while (length--) {
    value ^= *bytes++;
    for (uint8_t bit = 0; bit < 8U; ++bit)
      value = (value >> 1U) ^ (0xEDB88320UL & (0U - (value & 1U)));
  }
  return ~value;
}

inline void seal(Marker& marker) {
  marker.checksum = 0U;
  marker.checksum = crc32(&marker, offsetof(Marker, checksum));
}

inline bool valid(const Marker& marker) {
  return !std::memcmp(marker.magic, "PGR1", 4U) &&
         marker.version == kVersion && marker.structureSize == sizeof(Marker) &&
         marker.phase >= static_cast<uint8_t>(Phase::Staged) &&
         marker.phase <= static_cast<uint8_t>(Phase::Restored) &&
         marker.checksum == crc32(&marker, offsetof(Marker, checksum));
}

inline Marker makeMarker(uint32_t packageSequence, uint32_t firmwareSize,
                         const uint8_t firmwareSha256[32]) {
  Marker marker{};
  std::memcpy(marker.magic, "PGR1", 4U);
  marker.version = kVersion;
  marker.phase = static_cast<uint8_t>(Phase::Staged);
  marker.structureSize = sizeof(Marker);
  marker.packageSequence = packageSequence;
  marker.firmwareSize = firmwareSize;
  if (firmwareSha256)
    std::memcpy(marker.firmwareSha256, firmwareSha256, sizeof(marker.firmwareSha256));
  seal(marker);
  return marker;
}

}  // namespace PokegochiRecovery
