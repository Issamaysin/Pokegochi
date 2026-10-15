#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "Arduino.h"
#include "SPI.h"

constexpr int FILE_READ = 0;
constexpr int FILE_WRITE = 1;
constexpr int FILE_APPEND = 2;
constexpr uint8_t CARD_NONE = 0;

namespace android_sd {
inline SPIClass& defaultSpi() { static SPIClass spi; return spi; }
struct Handle {
  std::fstream stream;
  std::filesystem::path path;
  bool directory = false;
  bool writable = false;
  std::shared_ptr<std::vector<std::filesystem::path>> entries;
  size_t entryIndex = 0;
};
inline std::filesystem::path& root() {
  static std::filesystem::path value = std::filesystem::current_path();
  return value;
}
inline std::filesystem::path resolve(const char* raw) {
  std::string value = raw ? raw : "";
  while (!value.empty() && (value.front() == '/' || value.front() == '\\')) value.erase(value.begin());
  return root() / value;
}
inline void setRoot(const char* path) { if (path && path[0]) root() = std::filesystem::path(path); }
}

class File;
namespace android_sd { File openPath(const std::filesystem::path& path, int mode); }

class File {
 public:
  File() = default;
  explicit File(std::shared_ptr<android_sd::Handle> handle) : handle_(std::move(handle)) {}
  explicit operator bool() const { return static_cast<bool>(handle_) && (handle_->directory || handle_->stream.is_open()); }
  bool operator!() const { return !static_cast<bool>(*this); }
  size_t read(uint8_t* destination, size_t length) {
    if (!*this || handle_->directory || !destination) return 0;
    handle_->stream.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(length));
    return static_cast<size_t>(handle_->stream.gcount());
  }
  size_t readBytes(char* destination, size_t length) { return read(reinterpret_cast<uint8_t*>(destination), length); }
  size_t write(const uint8_t* source, size_t length) {
    if (!*this || handle_->directory || !source) return 0;
    handle_->stream.write(reinterpret_cast<const char*>(source), static_cast<std::streamsize>(length));
    return handle_->stream ? length : 0;
  }
  size_t write(const char* source, size_t length) { return write(reinterpret_cast<const uint8_t*>(source), length); }
  bool seek(uint32_t offset) {
    if (!*this || handle_->directory) return false;
    handle_->stream.clear();
    handle_->stream.seekg(offset);
    if (!handle_->stream) return false;
    if (handle_->writable) {
      handle_->stream.clear();
      handle_->stream.seekp(offset);
    }
    return static_cast<bool>(handle_->stream);
  }
  uint32_t position() const {
    if (!*this || handle_->directory) return 0;
    auto& stream = const_cast<std::fstream&>(handle_->stream);
    const auto position = stream.tellg();
    return position >= 0 ? static_cast<uint32_t>(position) : 0;
  }
  size_t size() const {
    if (!*this) return 0;
    if (handle_->directory) return 0;
    std::error_code error; const auto bytes = std::filesystem::file_size(handle_->path, error);
    return error ? 0 : static_cast<size_t>(bytes);
  }
  int available() const { const auto at = position(); const auto total = size(); return at < total ? static_cast<int>(total - at) : 0; }
  void flush() { if (*this && !handle_->directory) handle_->stream.flush(); }
  void close() { if (handle_ && handle_->stream.is_open()) handle_->stream.close(); handle_.reset(); }
  bool isDirectory() const { return static_cast<bool>(handle_) && handle_->directory; }
  const char* name() const { nameCache_ = handle_ ? handle_->path.string() : std::string(); return nameCache_.c_str(); }
  File openNextFile() {
    if (!isDirectory() || !handle_->entries || handle_->entryIndex >= handle_->entries->size()) return File();
    const auto path = (*handle_->entries)[handle_->entryIndex++];
    return android_sd::openPath(path, FILE_READ);
  }

 private:
  friend class SDClass;
  friend File android_sd::openPath(const std::filesystem::path&, int);
  std::shared_ptr<android_sd::Handle> handle_;
  mutable std::string nameCache_;
};

namespace android_sd {
inline File openPath(const std::filesystem::path& path, int mode) {
  auto handle = std::make_shared<Handle>(); handle->path = path;
  std::error_code error;
  if (std::filesystem::is_directory(path, error)) {
    handle->directory = true; handle->entries = std::make_shared<std::vector<std::filesystem::path>>();
    for (const auto& entry : std::filesystem::directory_iterator(path, error)) handle->entries->push_back(entry.path());
    return File(std::move(handle));
  }
  if (mode == FILE_WRITE) {
    std::filesystem::create_directories(path.parent_path(), error);
    handle->writable = true;
    handle->stream.open(path, std::ios::binary | std::ios::out | std::ios::trunc | std::ios::in);
  } else if (mode == FILE_APPEND) {
    std::filesystem::create_directories(path.parent_path(), error);
    handle->writable = true;
    handle->stream.open(path, std::ios::binary | std::ios::out | std::ios::app | std::ios::in);
  } else {
    handle->stream.open(path, std::ios::binary | std::ios::in);
  }
  return File(std::move(handle));
}
}

class SDClass {
 public:
  bool begin(uint8_t = 0, SPIClass& spi = android_sd::defaultSpi(), uint32_t = 0,
             const char* = "/sd", uint8_t = 1, bool = false) { return std::filesystem::exists(android_sd::root()); }
  void end() {}
  File open(const char* path, int mode = FILE_READ) { return android_sd::openPath(android_sd::resolve(path), mode); }
  bool exists(const char* path) const { return std::filesystem::exists(android_sd::resolve(path)); }
  bool remove(const char* path) { std::error_code error; return std::filesystem::remove(android_sd::resolve(path), error); }
  bool mkdir(const char* path) { std::error_code error; return std::filesystem::create_directories(android_sd::resolve(path), error) || std::filesystem::exists(android_sd::resolve(path)); }
  bool rename(const char* from, const char* to) { std::error_code error; std::filesystem::rename(android_sd::resolve(from), android_sd::resolve(to), error); return !error; }
  uint8_t cardType() const { return std::filesystem::exists(android_sd::root()) ? 1 : CARD_NONE; }
  uint64_t totalBytes() const { return 64ULL * 1024ULL * 1024ULL; }
  uint64_t usedBytes() const { return 0; }
};

extern SDClass SD;
