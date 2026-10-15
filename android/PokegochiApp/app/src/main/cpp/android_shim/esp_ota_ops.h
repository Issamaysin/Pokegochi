#pragma once

#include <cstddef>

// Android runs the firmware as a normal application process, so it has no
// bootloader or OTA partitions. Keep the firmware's diagnostic setup path
// source-compatible while making the absence of device OTA explicit.
struct esp_partition_t {
  const char* label;
  std::size_t size;
};

inline const esp_partition_t* esp_ota_get_running_partition() {
  static const esp_partition_t partition{"android", 0};
  return &partition;
}

inline const esp_partition_t* esp_ota_get_boot_partition() {
  return esp_ota_get_running_partition();
}
