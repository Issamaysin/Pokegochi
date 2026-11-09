#include <Arduino.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <mbedtls/sha256.h>

#include "config/BoardConfig.h"
#include "services/RecoveryUpdateProtocol.h"

namespace {
using namespace PokegochiRecovery;

SPIClass gSdSpi(VSPI);

[[noreturn]] void haltWithLed(bool success) {
  pinMode(board::kRgbLedRedPin, OUTPUT);
  pinMode(board::kRgbLedGreenPin, OUTPUT);
  const uint8_t active = board::kRgbLedActiveLow ? LOW : HIGH;
  const uint8_t inactive = board::kRgbLedActiveLow ? HIGH : LOW;
  digitalWrite(board::kRgbLedRedPin, success ? inactive : active);
  digitalWrite(board::kRgbLedGreenPin, success ? active : inactive);
  while (true) delay(1000U);
}

bool loadMarker(Marker& marker) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) return false;
  const size_t bytes = preferences.getBytesLength(kMarkerKey);
  const bool okay = bytes == sizeof(marker) &&
      preferences.getBytes(kMarkerKey, &marker, sizeof(marker)) == sizeof(marker) &&
      valid(marker);
  preferences.end();
  return okay;
}

bool saveMarker(Marker& marker) {
  seal(marker);
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return false;
  const bool okay = preferences.putBytes(kMarkerKey, &marker, sizeof(marker)) == sizeof(marker);
  preferences.end();
  return okay;
}

bool beginSd() {
  pinMode(board::kSdCsPin, OUTPUT);
  digitalWrite(board::kSdCsPin, HIGH);
  pinMode(board::kSdMisoPin, INPUT_PULLUP);
  gSdSpi.begin(board::kSdSckPin, board::kSdMisoPin,
               board::kSdMosiPin, board::kSdCsPin);
  constexpr uint32_t kRates[] = {10000000UL, 4000000UL, 2000000UL};
  for (const uint32_t rate : kRates) {
    digitalWrite(board::kSdCsPin, HIGH);
    if (SD.begin(board::kSdCsPin, gSdSpi, rate, "/sd", 1, false) &&
        SD.cardType() != CARD_NONE) return true;
    SD.end();
    delay(80U);
  }
  return false;
}

bool hashFile(const char* path, uint8_t output[32], uint32_t expectedBytes) {
  File file = SD.open(path, FILE_READ);
  if (!file || file.size() != expectedBytes) {
    if (file) file.close();
    return false;
  }
  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  if (mbedtls_sha256_starts(&context, 0) != 0) {
    mbedtls_sha256_free(&context);
    file.close();
    return false;
  }
  uint8_t buffer[4096];
  uint32_t done = 0U;
  bool okay = true;
  while (done < expectedBytes) {
    const size_t count = file.read(buffer, min<size_t>(sizeof(buffer), expectedBytes - done));
    if (!count || mbedtls_sha256_update(&context, buffer, count) != 0) {
      okay = false;
      break;
    }
    done += count;
  }
  if (okay) okay = mbedtls_sha256_finish(&context, output) == 0;
  mbedtls_sha256_free(&context);
  file.close();
  return okay && done == expectedBytes;
}

bool hashPartition(const esp_partition_t* partition, uint32_t bytes,
                   uint8_t output[32]) {
  if (!partition || bytes > partition->size) return false;
  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  if (mbedtls_sha256_starts(&context, 0) != 0) {
    mbedtls_sha256_free(&context);
    return false;
  }
  uint8_t buffer[4096];
  bool okay = true;
  for (uint32_t offset = 0; offset < bytes; offset += sizeof(buffer)) {
    const size_t count = min<size_t>(sizeof(buffer), bytes - offset);
    if (esp_partition_read(partition, offset, buffer, count) != ESP_OK ||
        mbedtls_sha256_update(&context, buffer, count) != 0) {
      okay = false;
      break;
    }
  }
  if (okay) okay = mbedtls_sha256_finish(&context, output) == 0;
  mbedtls_sha256_free(&context);
  return okay;
}

bool backupMain(const esp_partition_t* mainPartition, Marker& marker) {
  SD.remove(kPreviousImagePath);
  File output = SD.open(kPreviousImagePath, FILE_WRITE);
  if (!output) return false;
  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  bool okay = mbedtls_sha256_starts(&context, 0) == 0;
  uint8_t buffer[4096];
  for (uint32_t offset = 0; okay && offset < mainPartition->size;
       offset += sizeof(buffer)) {
    const size_t count = min<size_t>(sizeof(buffer), mainPartition->size - offset);
    okay = esp_partition_read(mainPartition, offset, buffer, count) == ESP_OK &&
        output.write(buffer, count) == count &&
        mbedtls_sha256_update(&context, buffer, count) == 0;
  }
  if (okay) okay = mbedtls_sha256_finish(&context, marker.previousSha256) == 0;
  mbedtls_sha256_free(&context);
  output.flush();
  output.close();
  marker.previousPartitionSize = okay ? mainPartition->size : 0U;
  return okay && saveMarker(marker);
}

bool installCandidate(const esp_partition_t* mainPartition, Marker& marker) {
  File input = SD.open(kFirmwareReadyPath, FILE_READ);
  if (!input || input.size() != marker.firmwareSize) {
    if (input) input.close();
    return false;
  }
  esp_ota_handle_t handle = 0;
  if (esp_ota_begin(mainPartition, marker.firmwareSize, &handle) != ESP_OK) {
    input.close();
    return false;
  }
  uint8_t buffer[4096];
  uint32_t done = 0U;
  bool okay = true;
  while (done < marker.firmwareSize) {
    const size_t count = input.read(buffer, min<size_t>(sizeof(buffer), marker.firmwareSize - done));
    if (!count || esp_ota_write(handle, buffer, count) != ESP_OK) {
      okay = false;
      break;
    }
    done += count;
  }
  input.close();
  if (!okay) {
    esp_ota_abort(handle);
    return false;
  }
  if (esp_ota_end(handle) != ESP_OK) return false;
  uint8_t digest[32]{};
  if (!hashPartition(mainPartition, marker.firmwareSize, digest) ||
      std::memcmp(digest, marker.firmwareSha256, sizeof(digest))) return false;
  marker.phase = static_cast<uint8_t>(Phase::Candidate);
  return saveMarker(marker) && esp_ota_set_boot_partition(mainPartition) == ESP_OK;
}

bool restorePrevious(const esp_partition_t* mainPartition, Marker& marker) {
  uint8_t digest[32]{};
  if (!marker.previousPartitionSize ||
      marker.previousPartitionSize != mainPartition->size ||
      !hashFile(kPreviousImagePath, digest, marker.previousPartitionSize) ||
      std::memcmp(digest, marker.previousSha256, sizeof(digest))) return false;
  File input = SD.open(kPreviousImagePath, FILE_READ);
  if (!input) return false;
  if (esp_partition_erase_range(mainPartition, 0U, mainPartition->size) != ESP_OK) {
    input.close();
    return false;
  }
  uint8_t buffer[4096];
  uint32_t done = 0U;
  bool okay = true;
  while (done < mainPartition->size) {
    const size_t count = input.read(buffer, sizeof(buffer));
    if (!count || esp_partition_write(mainPartition, done, buffer, count) != ESP_OK) {
      okay = false;
      break;
    }
    done += count;
  }
  input.close();
  if (!okay || done != mainPartition->size) return false;
  marker.phase = static_cast<uint8_t>(Phase::Restored);
  return saveMarker(marker) && esp_ota_set_boot_partition(mainPartition) == ESP_OK;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200U);
  Serial.println("[RECOVERY] Pokegochi factory recovery starting");
  const esp_partition_t* mainPartition = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0,
      PokegochiRecovery::kMainPartitionLabel);
  if (!mainPartition || !beginSd()) haltWithLed(false);

  Marker marker{};
  if (!loadMarker(marker)) {
    Serial.println("[RECOVERY] no pending update; starting main application");
    if (esp_ota_set_boot_partition(mainPartition) == ESP_OK) {
      delay(100U);
      ESP.restart();
    }
    haltWithLed(false);
  }

  const Phase phase = static_cast<Phase>(marker.phase);
  bool okay = false;
  if (phase == Phase::Staged) {
    uint8_t digest[32]{};
    okay = hashFile(kFirmwareReadyPath, digest, marker.firmwareSize) &&
        !std::memcmp(digest, marker.firmwareSha256, sizeof(digest)) &&
        backupMain(mainPartition, marker) && installCandidate(mainPartition, marker);
  } else if (phase == Phase::Candidate) {
    Serial.println("[RECOVERY] candidate failed before validation; restoring previous image");
    okay = restorePrevious(mainPartition, marker);
  } else if (phase == Phase::Restored) {
    okay = esp_ota_set_boot_partition(mainPartition) == ESP_OK;
  }
  if (!okay) haltWithLed(false);
  Serial.println("[RECOVERY] operation complete; rebooting main application");
  delay(200U);
  ESP.restart();
}

void loop() { delay(1000U); }
