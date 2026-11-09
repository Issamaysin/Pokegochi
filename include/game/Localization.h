#pragma once

#include <cstdint>

enum class UiLanguage : uint8_t {
  English = 0,
  Portuguese = 1,
};

// Runtime localization shared by the ESP32 firmware and the Android replica.
// English remains the zero-cost/default path. Portuguese first resolves a
// complete generated sentence catalogue, then translates tokens in formatted
// runtime messages (Pokemon names and player-provided values remain intact).
namespace Localization {

void setLanguage(UiLanguage language);
UiLanguage language();
bool isPortuguese();

// The returned pointer is either the original input, immutable catalogue
// storage, or a shared scratch buffer valid until the next translate() call.
const char* translate(const char* english);

}  // namespace Localization
