#include "android_runtime.h"

#include <atomic>
#include <algorithm>
#include <cstdio>
#include <mutex>
#include <string>
#include <unistd.h>

#include "Arduino.h"
#include "SD.h"
#include "Preferences.h"
#include "drivers/TouchDriver.h"

HardwareSerial Serial;
Esp32Compat ESP;
SDClass SD;

namespace {
std::atomic<int16_t> touchX{0};
std::atomic<int16_t> touchY{0};
std::atomic<bool> touchDown{false};
// Android can deliver ACTION_DOWN and ACTION_UP in the same frame-thread
// turn while the firmware worker is inside a long retained render. Keep one
// pending press so a quick tap is consumed by loop() instead of disappearing
// between two polls. Deliver release as its own edge as well; otherwise the
// firmware's wasTouched latch stays true forever after the first Android tap.
// Keep the press and release edges even when both arrive between firmware
// polls. While the finger remains down, however, expose the contact as a level:
// sliders and paint need continuity between ACTION_MOVE events. The firmware's
// Android-only tap throttle prevents that continuous level from rapid-firing
// ordinary buttons.
std::atomic<bool> touchPressPending{false};
std::atomic<bool> touchMovePending{false};
std::atomic<bool> touchReleasePending{false};
std::atomic<int16_t> deliveredTouchX{0};
std::atomic<int16_t> deliveredTouchY{0};
std::mutex logMutex;
std::FILE* runtimeLog = nullptr;
std::string runtimeLogPath;
std::string runtimeBuildId;
constexpr long kRuntimeLogLimit = 512L * 1024L;
}

extern "C" void pokegochi_android_set_storage_root(const char* path) {
  android_sd::setRoot(path);
}

extern "C" void pokegochi_android_set_preferences_root(const char* path) {
  android_preferences::setRoot(path);
}

extern "C" void pokegochi_android_set_log_path(const char* path) {
  std::lock_guard<std::mutex> lock(logMutex);
  if (runtimeLog) {
    std::fclose(runtimeLog);
    runtimeLog = nullptr;
  }
#if defined(POKEGOCHI_RELEASE)
  (void)path;
  runtimeLogPath.clear();
  return;
#else
  runtimeLogPath = path ? path : "";
  if (!runtimeLogPath.empty()) {
    runtimeLog = std::fopen(runtimeLogPath.c_str(), "wb");
    if (runtimeLog) {
      std::fprintf(runtimeLog, "[ANDROID] native runtime log started build=%s pid=%d\n",
                   runtimeBuildId.empty() ? "unknown" : runtimeBuildId.c_str(),
                   static_cast<int>(::getpid()));
      std::fflush(runtimeLog);
    }
  }
#endif
}

extern "C" void pokegochi_android_set_build_id(const char* buildId) {
  std::lock_guard<std::mutex> lock(logMutex);
  runtimeBuildId = buildId ? buildId : "";
}

extern "C" void pokegochi_android_append_log(const char* value) {
  if (!value || !value[0]) return;
  std::lock_guard<std::mutex> lock(logMutex);
  if (!runtimeLog) return;
  const long position = std::ftell(runtimeLog);
  if (position >= kRuntimeLogLimit) {
    std::fclose(runtimeLog);
    runtimeLog = runtimeLogPath.empty() ? nullptr :
        std::fopen(runtimeLogPath.c_str(), "wb");
    if (!runtimeLog) return;
    std::fputs("[ANDROID] native runtime log truncated at 512 KiB\n", runtimeLog);
  }
  std::fputs(value, runtimeLog);
  std::fputc('\n', runtimeLog);
  std::fflush(runtimeLog);
}

extern "C" void pokegochi_android_set_touch(int16_t x, int16_t y, bool touched) {
  const int16_t boundedX = std::clamp<int16_t>(x, 0, 319);
  const int16_t boundedY = std::clamp<int16_t>(y, 0, 239);
  const bool wasDown = touchDown.exchange(touched);
  touchX.store(boundedX);
  touchY.store(boundedY);
  if (!touched) {
    touchMovePending.store(false);
    if (wasDown) touchReleasePending.store(true);
    return;
  }
  if (!wasDown) {
    touchReleasePending.store(false);
    touchPressPending.store(true);
    return;
  }
  if (boundedX != deliveredTouchX.load() || boundedY != deliveredTouchY.load())
    touchMovePending.store(true);
}

bool pokegochi_android_take_touch(TouchPoint& point) {
  point = TouchPoint{};
  const bool pendingPress = touchPressPending.exchange(false);
  const bool pendingMove = touchMovePending.exchange(false);
  if (pendingPress || pendingMove) {
    point.touched = true;
  } else if (touchDown.load()) {
    // ACTION_MOVE is not guaranteed every frame. Preserve the held contact so
    // a drag cannot be mistaken for a release and reset midway across a track.
    point.touched = true;
  } else if (touchReleasePending.exchange(false)) {
    point.touched = false;
  } else {
    return false;
  }
  const int16_t x = touchX.load();
  const int16_t y = touchY.load();
  deliveredTouchX.store(x);
  deliveredTouchY.store(y);
  point.x = point.rawX = static_cast<uint16_t>(x);
  point.y = point.rawY = static_cast<uint16_t>(y);
  return true;
}

extern "C" int pokegochi_android_digital_read(uint8_t pin) {
  // The CYD firmware uses GPIO36 as the active-low touch IRQ.
  if (pin == 36U) return touchDown.load() ? LOW : HIGH;
  return HIGH;
}
