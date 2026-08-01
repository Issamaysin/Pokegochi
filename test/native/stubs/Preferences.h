#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

class Preferences {
 public:
  bool begin(const char* name, bool) {
    namespace_ = name ? name : "";
    return true;
  }

  size_t getBytesLength(const char* key) const {
    const auto iterator = bytes_.find(fullKey(key));
    return iterator == bytes_.end() ? 0 : iterator->second.size();
  }

  size_t getBytes(const char* key, void* destination, size_t length) const {
    const auto iterator = bytes_.find(fullKey(key));
    if (iterator == bytes_.end() || iterator->second.size() != length) return 0;
    std::memcpy(destination, iterator->second.data(), length);
    return length;
  }

  size_t putBytes(const char* key, const void* source, size_t length) {
    const auto* first = static_cast<const uint8_t*>(source);
    bytes_[fullKey(key)] = std::vector<uint8_t>(first, first + length);
    return length;
  }

  bool getBool(const char* key, bool fallback = false) const {
    const auto iterator = bools_.find(fullKey(key));
    return iterator == bools_.end() ? fallback : iterator->second;
  }

  size_t putBool(const char* key, bool value) {
    bools_[fullKey(key)] = value;
    return 1;
  }

  bool clear() {
    const std::string prefix = namespace_ + ":";
    for (auto iterator = bytes_.begin(); iterator != bytes_.end();) {
      iterator = iterator->first.rfind(prefix, 0) == 0 ? bytes_.erase(iterator) : ++iterator;
    }
    for (auto iterator = bools_.begin(); iterator != bools_.end();) {
      iterator = iterator->first.rfind(prefix, 0) == 0 ? bools_.erase(iterator) : ++iterator;
    }
    return true;
  }

  static void resetTestStorage() {
    bytes_.clear();
    bools_.clear();
  }

  static void corruptTestByte(const char* name, const char* key) {
    auto& value = bytes_[std::string(name) + ":" + key];
    if (!value.empty()) value[value.size() / 2] ^= 0x5A;
  }

 private:
  std::string fullKey(const char* key) const { return namespace_ + ":" + (key ? key : ""); }

  std::string namespace_;
  inline static std::unordered_map<std::string, std::vector<uint8_t>> bytes_{};
  inline static std::unordered_map<std::string, bool> bools_{};
};
