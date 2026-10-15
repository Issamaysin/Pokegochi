#pragma once
#include <cstdlib>
#include "Arduino.h"

constexpr uint32_t MALLOC_CAP_8BIT = 1U;
constexpr uint32_t MALLOC_CAP_INTERNAL = 2U;
inline void* heap_caps_malloc(size_t bytes, uint32_t) { return std::malloc(bytes); }
inline void* heap_caps_calloc(size_t count, size_t bytes, uint32_t) { return std::calloc(count, bytes); }
inline void heap_caps_free(void* value) { std::free(value); }
inline bool heap_caps_check_integrity_all(bool) { return true; }
inline size_t heap_caps_get_free_size(uint32_t) { return 64U * 1024U * 1024U; }
inline size_t heap_caps_get_largest_free_block(uint32_t) { return 64U * 1024U * 1024U; }
