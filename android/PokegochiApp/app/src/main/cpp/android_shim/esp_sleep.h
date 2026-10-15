#pragma once
#include <cstdint>

using gpio_num_t = int;
enum esp_sleep_wakeup_cause_t : uint8_t {
  ESP_SLEEP_WAKEUP_UNDEFINED = 0,
  ESP_SLEEP_WAKEUP_EXT0 = 2,
  ESP_SLEEP_WAKEUP_TIMER = 4,
};
constexpr int ESP_SLEEP_WAKEUP_ALL = 0xFF;
inline void esp_sleep_enable_ext0_wakeup(gpio_num_t, int) {}
inline void esp_sleep_enable_timer_wakeup(uint64_t) {}
inline void esp_sleep_disable_wakeup_source(int) {}
inline void esp_light_sleep_start() {}
inline void esp_deep_sleep_start() {}
inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return ESP_SLEEP_WAKEUP_TIMER; }
