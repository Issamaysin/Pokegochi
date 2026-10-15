#pragma once

#include <cstddef>
#include <cstdint>
#include "services/WirelessUpdateProtocol.h"

class WirelessUpdate {
 public:
  using DeviceState = PokegochiUpdateProtocol::DeviceState;
  using ErrorCode = PokegochiUpdateProtocol::ErrorCode;
  using SaveCommitCallback = bool (*)(void*);
  using SaveResetCallback = bool (*)(void*);
  using DiagnosticReadCallback = size_t (*)(void*, uint8_t, uint16_t, char*, size_t);

  bool recoverAtBoot(bool) { return true; }
  void confirmBoot(bool) {}
  bool begin(uint32_t, const char*, uint16_t, bool) { changed_ = true; return true; }
  void configureSaveManagement(void*, uint32_t, uint16_t, SaveCommitCallback, SaveResetCallback, void*) {}
  void configureDiagnostics(DiagnosticReadCallback, void*) {}
  void update() {}
  void shutdown(bool = false) { active_ = false; }
  void abort() { active_ = false; }
  bool active() const { return active_; }
  bool connected() const { return false; }
  bool busy() const { return false; }
  bool canAbort() const { return false; }
  DeviceState state() const { return DeviceState::Advertising; }
  ErrorCode error() const { return ErrorCode::None; }
  uint32_t pairingCode() const { return 0; }
  uint32_t playerId() const { return playerId_; }
  uint32_t bytesDone() const { return 0; }
  uint32_t bytesTotal() const { return 0; }
  uint8_t progressPercent() const { return 0; }
  const char* statusText() const { return "WIRELESS UPDATE IS NOT AVAILABLE IN APP."; }
  bool takeChanged() { const bool value = changed_; changed_ = false; return value; }
  bool takeSynchronizedTime(uint32_t&) { return false; }
  // The Android replica never exposes the ESP32 updater transport, but the
  // shared firmware UI still references the live-log lifecycle. Keep the
  // no-op implementation ABI-compatible so diagnostics cannot accidentally
  // pull updater behavior into the game app.
  bool liveLogActive() const { return false; }
  bool takeLiveLogStarted() { return false; }
  void detachLiveUi() {}
  void emitLiveLog(const char*, ...) {}
  void transportConnected() {}
  void transportDisconnected() {}
  void transportControl(const uint8_t*, size_t) {}
  void transportData(const uint8_t*, size_t) {}

 private:
  bool active_ = false;
  bool changed_ = true;
  uint32_t playerId_ = 0;
};
