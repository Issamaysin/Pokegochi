#pragma once

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#if defined(__ANDROID__)
#include <android/log.h>
extern "C" void pokegochi_android_append_log(const char* value);
#endif

using byte = uint8_t;
using boolean = bool;
using UBaseType_t = uint32_t;

constexpr int HIGH = 1;
constexpr int LOW = 0;
constexpr int INPUT = 0;
constexpr int OUTPUT = 1;
constexpr int INPUT_PULLUP = 2;

constexpr uint8_t TL_DATUM = 0;
constexpr uint8_t TC_DATUM = 1;
constexpr uint8_t TR_DATUM = 2;
constexpr uint8_t ML_DATUM = 3;
constexpr uint8_t MC_DATUM = 4;
constexpr uint8_t MR_DATUM = 5;
constexpr uint8_t BL_DATUM = 6;
constexpr uint8_t BC_DATUM = 7;
constexpr uint8_t BR_DATUM = 8;

inline long map(long value, long fromLow, long fromHigh, long toLow, long toHigh) {
  if (fromHigh == fromLow) return toLow;
  return (value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;
}

template <typename T, typename L, typename H>
inline T constrain(T value, L low, H high) {
  return value < static_cast<T>(low) ? static_cast<T>(low)
      : value > static_cast<T>(high) ? static_cast<T>(high) : value;
}

inline uint64_t androidMonotonicMicros() {
  static const auto start = std::chrono::steady_clock::now();
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - start).count());
}

inline uint32_t millis() { return static_cast<uint32_t>(androidMonotonicMicros() / 1000ULL); }
inline uint32_t micros() { return static_cast<uint32_t>(androidMonotonicMicros()); }
inline void delay(uint32_t value) { std::this_thread::sleep_for(std::chrono::milliseconds(value)); }
inline void yield() { std::this_thread::yield(); }

inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
extern "C" int pokegochi_android_digital_read(uint8_t pin);
inline int digitalRead(uint8_t pin) { return pokegochi_android_digital_read(pin); }
inline void analogWrite(uint8_t, int) {}
inline int analogRead(uint8_t) { return 3000; }
inline uint16_t analogReadMilliVolts(uint8_t) { return 3700; }
inline void analogReadResolution(uint8_t) {}
inline void analogSetPinAttenuation(uint8_t, int) {}
constexpr int ADC_11db = 0;

inline uint32_t esp_random() {
  static uint32_t state = 0xA341316CU;
  state ^= state << 13U;
  state ^= state >> 17U;
  state ^= state << 5U;
  return state;
}

class HardwareSerial {
 public:
  void begin(unsigned long) {}
  void flush() {}
  int available() const { return 0; }
  int read() { return -1; }
  void print(const char* value) {
    if (!value) return;
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, "PokegochiFirmware", "%s", value);
    pokegochi_android_append_log(value);
#else
    std::fputs(value, stderr);
#endif
  }
  void println(const char* value = "") {
    if (!value) return;
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, "PokegochiFirmware", "%s", value);
    pokegochi_android_append_log(value);
#else
    std::fprintf(stderr, "%s\n", value);
#endif
  }
  template <typename T> void println(T value) {
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, "PokegochiFirmware", "%lld", static_cast<long long>(value));
#else
    std::fprintf(stderr, "%lld\n", static_cast<long long>(value));
#endif
  }
  void printf(const char* format, ...) {
    va_list arguments; va_start(arguments, format);
#if defined(__ANDROID__)
    char buffer[1024]{};
    std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    __android_log_print(ANDROID_LOG_INFO, "PokegochiFirmware", "%s", buffer);
    pokegochi_android_append_log(buffer);
#else
    std::vfprintf(stderr, format, arguments);
#endif
    va_end(arguments);
  }
};

extern HardwareSerial Serial;

class Esp32Compat {
 public:
  uint32_t getFreeHeap() const { return 64U * 1024U * 1024U; }
  uint32_t getMaxAllocHeap() const { return 64U * 1024U * 1024U; }
  uint64_t getEfuseMac() const { return 0x504F4B45474F4348ULL; }
  void restart() const {}
};

extern Esp32Compat ESP;

inline uint32_t uxTaskGetStackHighWaterMark(void*) { return 64U * 1024U; }
