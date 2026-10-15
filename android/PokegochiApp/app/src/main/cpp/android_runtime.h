#pragma once

#include <cstdint>
#include <cstddef>

struct TouchPoint;

extern "C" void pokegochi_android_set_storage_root(const char* path);
extern "C" void pokegochi_android_set_preferences_root(const char* path);
extern "C" void pokegochi_android_set_log_path(const char* path);
extern "C" void pokegochi_android_set_build_id(const char* buildId);
extern "C" void pokegochi_android_install_crash_handler(const char* path);
extern "C" void pokegochi_android_set_touch(int16_t x, int16_t y, bool touched);
// Publish a completed firmware framebuffer while a blocking firmware
// animation is still running inside loop().
extern "C" void pokegochi_android_frame_presented();
// Record the end-to-end timing of an authored animation frame. The Android
// build uses this only for diagnostics; the ESP32 renderer has no equivalent
// queue/presenter boundary.
extern "C" void pokegochi_android_trace_animation_frame(
    uint16_t move, uint16_t sourceFrame, uint32_t composeUs,
    uint32_t publishUs, bool firstFrame);
extern "C" void pokegochi_android_trace_evolution_frame(
    uint8_t sequenceFrame, uint8_t stage, uint16_t fromSpecies,
    uint16_t toSpecies, uint16_t visibleSpecies, bool lightSilhouette,
    bool cancelPrompt, bool canceledMessage, uint16_t authoredWaitMs);
extern "C" void pokegochi_android_trace_screen_transition(
    const char* fromName, uint8_t fromScreen, const char* toName,
    uint8_t toScreen, uint32_t frameGeneration);
extern "C" void pokegochi_android_trace_game_health(
    uint32_t uptimeMs, const char* screenName, uint8_t screen,
    const char* presentedName, uint8_t presentedScreen,
    bool sdReady, bool assetsReady, bool saveReady, bool saveDirty,
    bool battleActive, uint8_t battleKind, uint16_t battleTurn,
    uint8_t battleFlowPhase, uint8_t battleEventIndex,
    uint8_t moveLearningIndex, uint8_t moveLearningCount,
    uint8_t evolutionIndex, uint8_t evolutionCount,
    bool retainedFrameRequested, bool renderingRetainedFrame,
    bool displaySleeping, uint8_t batteryPercent);
extern "C" void pokegochi_android_set_phone_battery_percent(uint8_t percent);
extern "C" void pokegochi_android_flush_save();
extern "C" void pokegochi_android_sync_phone_time(uint64_t epochSeconds);
bool pokegochi_android_take_touch(TouchPoint& point);
extern "C" uint32_t pokegochi_android_frame_generation();

extern "C" uint32_t pokegochi_android_multiplayer_player_id();
extern "C" bool pokegochi_android_multiplayer_enabled();
extern "C" uint8_t pokegochi_android_multiplayer_state();
extern "C" uint32_t pokegochi_android_multiplayer_peer_id();
extern "C" bool pokegochi_android_multiplayer_is_connected();
extern "C" bool pokegochi_android_multiplayer_take_outgoing(
    uint8_t* bytes, uint16_t* length);
extern "C" void pokegochi_android_multiplayer_receive(
    const uint8_t* bytes, size_t length);
extern "C" void pokegochi_android_multiplayer_discovered(
    uint32_t playerId, int8_t rssi, const char* address, uint8_t addressType);
extern "C" void pokegochi_android_multiplayer_transport_connected(bool outbound);
extern "C" void pokegochi_android_multiplayer_disconnected();
