#pragma once

#include <filesystem>
#include <fstream>
#include <string>

namespace android_preferences {
inline std::filesystem::path& root() {
  static std::filesystem::path value = std::filesystem::current_path() / "preferences";
  return value;
}
inline void setRoot(const char* path) { if (path && path[0]) root() = std::filesystem::path(path); }
inline std::filesystem::path keyPath(const std::string& name, const std::string& key) {
  return root() / name / (key + ".bin");
}
}

class Preferences {
 public:
  bool begin(const char* name, bool = false) { namespace_ = name ? name : ""; std::error_code error; return std::filesystem::create_directories(android_preferences::root() / namespace_, error) || !error; }
  void end() {}
  size_t getBytesLength(const char* key) const {
    std::error_code error; const auto bytes = std::filesystem::file_size(android_preferences::keyPath(namespace_, key ? key : ""), error);
    return error ? 0 : static_cast<size_t>(bytes);
  }
  size_t getBytes(const char* key, void* destination, size_t length) const {
    if (!destination) return 0; std::ifstream file(android_preferences::keyPath(namespace_, key ? key : ""), std::ios::binary);
    if (!file) return 0; file.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(length)); return static_cast<size_t>(file.gcount());
  }
  size_t putBytes(const char* key, const void* source, size_t length) {
    if (!source) return 0; std::error_code error; std::filesystem::create_directories(android_preferences::root() / namespace_, error);
    std::ofstream file(android_preferences::keyPath(namespace_, key ? key : ""), std::ios::binary | std::ios::trunc);
    if (!file) return 0; file.write(reinterpret_cast<const char*>(source), static_cast<std::streamsize>(length)); return file ? length : 0;
  }
  bool getBool(const char* key, bool fallback) const { bool value = fallback; return getBytes(key, &value, sizeof(value)) == sizeof(value) ? value : fallback; }
  size_t putBool(const char* key, bool value) { return putBytes(key, &value, sizeof(value)); }
  uint32_t getUInt(const char* key, uint32_t fallback) const { uint32_t value = fallback; return getBytes(key, &value, sizeof(value)) == sizeof(value) ? value : fallback; }
  size_t putUInt(const char* key, uint32_t value) { return putBytes(key, &value, sizeof(value)); }
  bool remove(const char* key) { std::error_code error; return std::filesystem::remove(android_preferences::keyPath(namespace_, key ? key : ""), error); }
  bool clear() { std::error_code error; std::filesystem::remove_all(android_preferences::root() / namespace_, error); return !error; }
  private:
  std::string namespace_;
};
