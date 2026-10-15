#include <atomic>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <mutex>
#include <new>
#include <csignal>
#include <fcntl.h>
#include <string>
#include <thread>
#include <ucontext.h>
#include <unistd.h>

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include "android_runtime.h"

extern void setup();
extern void loop();
extern "C" const uint16_t* pokegochi_android_framebuffer();

namespace {
constexpr size_t kFramePixels = 320U * 240U;
constexpr size_t kFrameBytes = kFramePixels * sizeof(uint16_t);
// The firmware publishes from inside blocking battle/capture playback. Keep a
// small FIFO of complete RGB565 snapshots. The Android presenter must consume
// authored animation cels in order; replacing the queue with its newest frame
// made a phone that missed one upload discard every intermediate cel and show
// a visibly broken jump from the first pose to the last pose.
// Android has enough heap to retain a short animation burst. Twenty-four full
// RGB565 frames cost about 3.6 MiB and absorb compositor jitter without
// deleting authored cels. The old 12/90 ms budget still dropped three Ember
// frames after the main monitor starvation bug was removed.
constexpr size_t kFrameQueueCapacity = 24U;
// Twenty-four frames span 400 ms at 60 Hz. A 500 ms lifetime lets the whole
// allocated FIFO do its job; the capacity guard still bounds latency and RAM
// if Android is backgrounded or a vendor Surface stops consuming frames.
constexpr uint64_t kMaximumQueuedFrameAgeNs = 500ULL * 1000ULL * 1000ULL;
// A real battle source is authored at 60 Hz. Presenting backlog at the former
// 12 ms interval made a recovered animation run about 39% too fast. Keep the
// display clock at one exact 60 Hz period: source frames with a slower cadence
// still arrive at their authored interval, while a burst can no longer race.
// It is intentionally based only on the native clock; Java and native clocks
// must never be compared to decide whether a frame is deliverable.
constexpr uint64_t kMinimumFrameCopyIntervalNs = 16666667ULL;
constexpr uint64_t kMaximumPacingCatchupNs = 4ULL * kMinimumFrameCopyIntervalNs;

struct FrameSnapshot {
  std::array<uint16_t, kFramePixels> pixels{};
  uint32_t generation = 0;
  uint64_t publishedAtNs = 0;
};

struct FirmwareSession {
  std::atomic<bool> running{false};
  std::mutex runtimeMutex;
  std::mutex frameMutex;
  std::mutex surfaceMutex;
  std::array<FrameSnapshot, kFrameQueueCapacity> frameQueue{};
  std::array<uint16_t, kFramePixels> presentationPixels{};
  size_t frameQueueHead = 0;
  size_t frameQueueCount = 0;
  uint32_t publishedGeneration = 0;
  uint64_t publishedFrames = 0;
  uint64_t copiedFrames = 0;
  uint32_t queueFullDrops = 0;
  uint32_t staleQueueDrops = 0;
  uint32_t pacingWaits = 0;
  uint64_t nextFrameCopyAtNs = 0;
  std::atomic<uint64_t> presentedFrames{0};
  size_t maximumQueueDepth = 0;
  ANativeWindow* surface = nullptr;
  bool hasPresentationFrame = false;
  bool surfaceNeedsRedraw = false;
  std::thread worker;
};

std::atomic<FirmwareSession*> activeSession{nullptr};
char nativeCrashPath[512]{};
char nativeBuildId[128]{};
size_t nativeBuildIdLength = 0;

void writeCrashHex(int descriptor, uint64_t value) {
  char digits[16]{};
  unsigned count = 0;
  do {
    const unsigned nibble = static_cast<unsigned>(value & 0xFU);
    digits[count++] = static_cast<char>(nibble < 10U ? '0' + nibble
                                                     : 'a' + nibble - 10U);
    value >>= 4U;
  } while (value && count < sizeof(digits));
  static constexpr char prefix[] = "0x";
  ::write(descriptor, prefix, sizeof(prefix) - 1U);
  for (unsigned index = 0; index < count; ++index)
    ::write(descriptor, &digits[count - index - 1U], 1U);
}

uintptr_t crashProgramCounter(void* context) {
  if (!context) return 0;
  const auto* machine = reinterpret_cast<const ucontext_t*>(context);
#if defined(__aarch64__)
  return static_cast<uintptr_t>(machine->uc_mcontext.pc);
#elif defined(__x86_64__) && defined(REG_RIP)
  return static_cast<uintptr_t>(machine->uc_mcontext.gregs[REG_RIP]);
#else
  return 0;
#endif
}

void writeNativeCrashMarker(int signalNumber, siginfo_t* info, void* context) {
  if (!nativeCrashPath[0]) return;
  const int descriptor = ::open(nativeCrashPath,
                                O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                                0600);
  if (descriptor >= 0) {
    static constexpr char prefix[] =
        "POKEGOCHI_NATIVE_CRASH build=";
    ::write(descriptor, prefix, sizeof(prefix) - 1U);
    ::write(descriptor, nativeBuildId,
            static_cast<size_t>(nativeBuildIdLength));
    static constexpr char pidPrefix[] = " pid=";
    ::write(descriptor, pidPrefix, sizeof(pidPrefix) - 1U);
    char pidDigits[12]{};
    unsigned pidValue = static_cast<unsigned>(::getpid());
    unsigned pidCount = 0;
    do {
      pidDigits[pidCount++] = static_cast<char>('0' + (pidValue % 10U));
      pidValue /= 10U;
    } while (pidValue && pidCount < sizeof(pidDigits));
    for (unsigned index = 0; index < pidCount; ++index)
      ::write(descriptor, &pidDigits[pidCount - index - 1U], 1U);
    static constexpr char signalPrefix[] = " signal=";
    ::write(descriptor, signalPrefix, sizeof(signalPrefix) - 1U);
    char digits[12]{};
    unsigned value = static_cast<unsigned>(signalNumber);
    unsigned count = 0;
    do {
      digits[count++] = static_cast<char>('0' + (value % 10U));
      value /= 10U;
    } while (value && count < sizeof(digits));
    for (unsigned index = 0; index < count; ++index)
      ::write(descriptor, &digits[count - index - 1U], 1U);
    static constexpr char faultPrefix[] = " fault=";
    ::write(descriptor, faultPrefix, sizeof(faultPrefix) - 1U);
    writeCrashHex(descriptor, info ? reinterpret_cast<uintptr_t>(info->si_addr) : 0U);
    static constexpr char pcPrefix[] = " pc=";
    ::write(descriptor, pcPrefix, sizeof(pcPrefix) - 1U);
    writeCrashHex(descriptor, crashProgramCounter(context));
    static constexpr char suffix[] =
        "\nThe native runtime terminated before Java could collect a stack.\n";
    ::write(descriptor, suffix, sizeof(suffix) - 1U);
    ::close(descriptor);
  }
}

void nativeCrashSignalHandler(int signalNumber, siginfo_t* info, void* context) {
  writeNativeCrashMarker(signalNumber, info, context);
  // The previous handler called _exit() here. That preserved a tiny marker,
  // but it also suppressed Android's debuggerd/tombstone backtrace. Restore
  // the default action and re-raise so the next diagnostic dump contains the
  // exact native frame that faulted instead of only "signal=11".
  struct sigaction defaultAction{};
  sigemptyset(&defaultAction.sa_mask);
  defaultAction.sa_handler = SIG_DFL;
  sigaction(signalNumber, &defaultAction, nullptr);
  sigset_t unblocked{};
  sigemptyset(&unblocked);
  sigprocmask(SIG_SETMASK, &unblocked, nullptr);
  ::raise(signalNumber);
  _exit(128 + signalNumber);
}

void installNativeCrashHandler(const char* path, const char* buildId) {
  std::remove(path ? path : "");
  std::strncpy(nativeCrashPath, path ? path : "", sizeof(nativeCrashPath) - 1U);
  nativeCrashPath[sizeof(nativeCrashPath) - 1U] = 0;
  std::strncpy(nativeBuildId, buildId ? buildId : "unknown",
               sizeof(nativeBuildId) - 1U);
  nativeBuildId[sizeof(nativeBuildId) - 1U] = 0;
  nativeBuildIdLength = std::strlen(nativeBuildId);
  struct sigaction action{};
  sigemptyset(&action.sa_mask);
  action.sa_sigaction = nativeCrashSignalHandler;
  action.sa_flags = SA_SIGINFO | SA_RESETHAND;
  for (const int signalNumber : {SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV})
    sigaction(signalNumber, &action, nullptr);
}

void publishFrame(FirmwareSession& session, bool force = false) {
  const uint32_t generation = pokegochi_android_frame_generation();
  std::lock_guard<std::mutex> lock(session.frameMutex);
  if (!force && generation == session.publishedGeneration) return;
  if (session.frameQueueCount == kFrameQueueCapacity) {
    // If Android is paused or a vendor compositor is briefly slow, discard
    // only the oldest backlog entry. During normal play the 24-entry FIFO
    // preserves every authored cel without blocking the firmware worker.
    session.frameQueueHead =
        (session.frameQueueHead + 1U) % kFrameQueueCapacity;
    --session.frameQueueCount;
    ++session.queueFullDrops;
  }
  const size_t tail =
      (session.frameQueueHead + session.frameQueueCount) % kFrameQueueCapacity;
  const auto* source = pokegochi_android_framebuffer();
  std::memcpy(session.frameQueue[tail].pixels.data(), source, kFrameBytes);
  session.frameQueue[tail].generation = generation;
  session.frameQueue[tail].publishedAtNs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
  ++session.frameQueueCount;
  ++session.publishedFrames;
  session.maximumQueueDepth =
      std::max(session.maximumQueueDepth, session.frameQueueCount);
  session.publishedGeneration = generation;
}

std::string stringFromJava(JNIEnv* env, jstring value) {
  if (!value) return {};
  const char* bytes = env->GetStringUTFChars(value, nullptr);
  std::string result = bytes ? bytes : "";
  if (bytes) env->ReleaseStringUTFChars(value, bytes);
  return result;
}
}

extern "C" void pokegochi_android_frame_presented() {
  // showBattleVisualFrame() calls this after the retained compositor has
  // completed a visual frame. The firmware animation can remain inside one
  // blocking loop() call for several hundred milliseconds, so waiting for the
  // outer loop() return would collapse the whole animation into one frame.
  FirmwareSession* session = activeSession.load(std::memory_order_acquire);
  if (session) publishFrame(*session);
}

extern "C" void pokegochi_android_trace_animation_frame(
    uint16_t move, uint16_t sourceFrame, uint32_t composeUs,
    uint32_t publishUs, bool firstFrame) {
  FirmwareSession* session = activeSession.load(std::memory_order_acquire);
  if (!session) return;
  uint64_t published = 0;
  uint64_t copied = 0;
  uint32_t fullDrops = 0;
  uint32_t staleDrops = 0;
  uint32_t pacingWaits = 0;
  size_t queueDepth = 0;
  size_t maximumDepth = 0;
  {
    std::lock_guard<std::mutex> lock(session->frameMutex);
    published = session->publishedFrames;
    copied = session->copiedFrames;
    fullDrops = session->queueFullDrops;
    staleDrops = session->staleQueueDrops;
    pacingWaits = session->pacingWaits;
    queueDepth = session->frameQueueCount;
    maximumDepth = session->maximumQueueDepth;
  }
  // Keep the diagnostic low overhead: retain every counter in the dump, but
  // emit one line every four source frames plus the first/slow frames. A
  // logger that writes on every frame would itself become a timing variable.
  if (!firstFrame && (sourceFrame & 3U) != 0U && composeUs < 10000U &&
      publishUs < 3000U)
    return;
  __android_log_print(
      ANDROID_LOG_INFO, "Pokegochi",
      "ANIM_TRACE move=%u src=%u first=%u compose_us=%u publish_us=%u "
       "queue=%u max_queue=%u published=%llu copied=%llu full_drop=%u "
       "stale_drop=%u pacing_wait=%u",
      static_cast<unsigned>(move), static_cast<unsigned>(sourceFrame),
      firstFrame ? 1U : 0U, static_cast<unsigned>(composeUs),
      static_cast<unsigned>(publishUs), static_cast<unsigned>(queueDepth),
      static_cast<unsigned>(maximumDepth),
      static_cast<unsigned long long>(published),
      static_cast<unsigned long long>(copied), static_cast<unsigned>(fullDrops),
       static_cast<unsigned>(staleDrops), static_cast<unsigned>(pacingWaits));
}

extern "C" void pokegochi_android_trace_evolution_frame(
    uint8_t sequenceFrame, uint8_t stage, uint16_t fromSpecies,
    uint16_t toSpecies, uint16_t visibleSpecies, bool lightSilhouette,
    bool cancelPrompt, bool canceledMessage, uint16_t authoredWaitMs) {
  FirmwareSession* session = activeSession.load(std::memory_order_acquire);
  size_t queueDepth = 0;
  size_t maximumDepth = 0;
  uint64_t published = 0;
  uint64_t copied = 0;
  uint32_t fullDrops = 0;
  uint32_t staleDrops = 0;
  if (session) {
    std::lock_guard<std::mutex> lock(session->frameMutex);
    queueDepth = session->frameQueueCount;
    maximumDepth = session->maximumQueueDepth;
    published = session->publishedFrames;
    copied = session->copiedFrames;
    fullDrops = session->queueFullDrops;
    staleDrops = session->staleQueueDrops;
  }
  __android_log_print(
      ANDROID_LOG_INFO, "Pokegochi",
      "EVOLUTION_TRACE seq=%u stage=%u from=%u to=%u visible=%u light=%u "
      "prompt=%u canceled=%u wait_ms=%u queue=%u max_queue=%u published=%llu "
      "copied=%llu full_drop=%u stale_drop=%u",
      static_cast<unsigned>(sequenceFrame), static_cast<unsigned>(stage),
      static_cast<unsigned>(fromSpecies), static_cast<unsigned>(toSpecies),
      static_cast<unsigned>(visibleSpecies), lightSilhouette ? 1U : 0U,
      cancelPrompt ? 1U : 0U, canceledMessage ? 1U : 0U,
      static_cast<unsigned>(authoredWaitMs), static_cast<unsigned>(queueDepth),
      static_cast<unsigned>(maximumDepth),
      static_cast<unsigned long long>(published),
      static_cast<unsigned long long>(copied), static_cast<unsigned>(fullDrops),
      static_cast<unsigned>(staleDrops));
}

extern "C" void pokegochi_android_trace_screen_transition(
    const char* fromName, uint8_t fromScreen, const char* toName,
    uint8_t toScreen, uint32_t frameGeneration) {
  FirmwareSession* session = activeSession.load(std::memory_order_acquire);
  size_t queueDepth = 0;
  uint32_t fullDrops = 0;
  uint32_t staleDrops = 0;
  if (session) {
    std::lock_guard<std::mutex> lock(session->frameMutex);
    queueDepth = session->frameQueueCount;
    fullDrops = session->queueFullDrops;
    staleDrops = session->staleQueueDrops;
  }
  __android_log_print(
      ANDROID_LOG_INFO, "Pokegochi",
      "SCREEN_TRACE from=%s(%u) to=%s(%u) generation=%u queue=%u "
      "full_drop=%u stale_drop=%u",
      fromName ? fromName : "Unknown", static_cast<unsigned>(fromScreen),
      toName ? toName : "Unknown", static_cast<unsigned>(toScreen),
      static_cast<unsigned>(frameGeneration), static_cast<unsigned>(queueDepth),
      static_cast<unsigned>(fullDrops), static_cast<unsigned>(staleDrops));
}

extern "C" void pokegochi_android_trace_game_health(
    uint32_t uptimeMs, const char* screenName, uint8_t screen,
    const char* presentedName, uint8_t presentedScreen,
    bool sdReady, bool assetsReady, bool saveReady, bool saveDirty,
    bool battleActive, uint8_t battleKind, uint16_t battleTurn,
    uint8_t battleFlowPhase, uint8_t battleEventIndex,
    uint8_t moveLearningIndex, uint8_t moveLearningCount,
    uint8_t evolutionIndex, uint8_t evolutionCount,
    bool retainedFrameRequested, bool renderingRetainedFrame,
    bool displaySleeping, uint8_t batteryPercent) {
  FirmwareSession* session = activeSession.load(std::memory_order_acquire);
  size_t queueDepth = 0;
  size_t maximumDepth = 0;
  uint64_t published = 0;
  uint64_t copied = 0;
  uint64_t presented = 0;
  uint32_t fullDrops = 0;
  uint32_t staleDrops = 0;
  uint32_t pacingWaits = 0;
  if (session) {
    std::lock_guard<std::mutex> lock(session->frameMutex);
    queueDepth = session->frameQueueCount;
    maximumDepth = session->maximumQueueDepth;
    published = session->publishedFrames;
    copied = session->copiedFrames;
    presented = session->presentedFrames.load(std::memory_order_relaxed);
    fullDrops = session->queueFullDrops;
    staleDrops = session->staleQueueDrops;
    pacingWaits = session->pacingWaits;
  }
  __android_log_print(
      ANDROID_LOG_INFO, "Pokegochi",
      "GAME_HEALTH uptime_ms=%u screen=%s(%u) presented_screen=%s(%u) "
      "storage=%u/%u save=%u dirty=%u battle=%u kind=%u turn=%u flow=%u "
      "event=%u move_queue=%u/%u evolution_queue=%u/%u retained=%u/%u "
      "sleep=%u battery=%u queue=%u max_queue=%u published=%llu copied=%llu "
      "presented=%llu full_drop=%u stale_drop=%u pacing_wait=%u",
      static_cast<unsigned>(uptimeMs), screenName ? screenName : "Unknown",
      static_cast<unsigned>(screen), presentedName ? presentedName : "Unknown",
      static_cast<unsigned>(presentedScreen), sdReady ? 1U : 0U,
      assetsReady ? 1U : 0U, saveReady ? 1U : 0U, saveDirty ? 1U : 0U,
      battleActive ? 1U : 0U, static_cast<unsigned>(battleKind),
      static_cast<unsigned>(battleTurn), static_cast<unsigned>(battleFlowPhase),
      static_cast<unsigned>(battleEventIndex),
      static_cast<unsigned>(moveLearningIndex),
      static_cast<unsigned>(moveLearningCount),
      static_cast<unsigned>(evolutionIndex),
      static_cast<unsigned>(evolutionCount), retainedFrameRequested ? 1U : 0U,
      renderingRetainedFrame ? 1U : 0U, displaySleeping ? 1U : 0U,
      static_cast<unsigned>(batteryPercent), static_cast<unsigned>(queueDepth),
      static_cast<unsigned>(maximumDepth),
      static_cast<unsigned long long>(published),
      static_cast<unsigned long long>(copied),
      static_cast<unsigned long long>(presented),
      static_cast<unsigned>(fullDrops), static_cast<unsigned>(staleDrops),
      static_cast<unsigned>(pacingWaits));
}

extern "C" JNIEXPORT void JNICALL
Java_com_pokegochi_app_NativeFirmware_setPhoneBatteryPercent(
    JNIEnv*, jclass, jlong handle, jint percent) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return;
  const jint bounded = std::max<jint>(0, std::min<jint>(100, percent));
  std::lock_guard<std::mutex> lock(session->runtimeMutex);
  pokegochi_android_set_phone_battery_percent(static_cast<uint8_t>(bounded));
}

extern "C" JNIEXPORT void JNICALL
Java_com_pokegochi_app_NativeFirmware_syncPhoneTime(
    JNIEnv*, jclass, jlong handle, jlong epochSeconds) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session || epochSeconds <= 0) return;
  std::lock_guard<std::mutex> lock(session->runtimeMutex);
  pokegochi_android_sync_phone_time(static_cast<uint64_t>(epochSeconds));
}

extern "C" JNIEXPORT void JNICALL
Java_com_pokegochi_app_NativeFirmware_flushSave(JNIEnv*, jclass, jlong handle) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return;
  std::lock_guard<std::mutex> lock(session->runtimeMutex);
  pokegochi_android_flush_save();
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_pokegochi_app_NativeFirmware_create(JNIEnv* env, jclass,
                                               jstring storageRoot,
                                               jstring preferencesRoot,
                                               jstring logPath,
                                               jstring crashPath,
                                               jstring buildId) {
  const std::string storage = stringFromJava(env, storageRoot);
  const std::string preferences = stringFromJava(env, preferencesRoot);
  const std::string runtimeLog = stringFromJava(env, logPath);
  const std::string nativeCrash = stringFromJava(env, crashPath);
  const std::string nativeBuild = stringFromJava(env, buildId);
  pokegochi_android_set_storage_root(storage.c_str());
  pokegochi_android_set_preferences_root(preferences.c_str());
  pokegochi_android_set_build_id(nativeBuild.c_str());
#if defined(POKEGOCHI_RELEASE)
  // The release APK keeps Android logcat available to the OS, but never
  // creates the private runtime/crash files used by diagnostic builds.
  pokegochi_android_set_log_path(nullptr);
#else
  pokegochi_android_set_log_path(runtimeLog.c_str());
  installNativeCrashHandler(nativeCrash.c_str(), nativeBuild.c_str());
#endif

  // The queue is deliberately large enough for animation playback, so do not
  // let a constrained phone turn a recoverable allocation failure into an
  // uncaught std::bad_alloc and an immediate process crash at app entry.
  auto* session = new (std::nothrow) FirmwareSession();
  if (!session) {
    __android_log_print(ANDROID_LOG_ERROR, "Pokegochi",
                         "Native runtime allocation failed at startup");
    return 0;
  }
  activeSession.store(session, std::memory_order_release);
  try {
    std::lock_guard<std::mutex> lock(session->runtimeMutex);
    setup();
    publishFrame(*session, true);
  } catch (...) {
    activeSession.store(nullptr, std::memory_order_release);
    delete session;
    __android_log_print(ANDROID_LOG_ERROR, "Pokegochi",
                         "Native runtime threw during startup");
    return 0;
  }
  session->running.store(true);
  try {
    session->worker = std::thread([session] {
      while (session->running.load()) {
        {
          std::lock_guard<std::mutex> lock(session->runtimeMutex);
          loop();
          publishFrame(*session);
        }
        // Keep the native firmware task cooperative with Android's compositor
        // on phones that have only a few big cores. The firmware's own delays
        // and timing remain unchanged; this only gives the UI a scheduling turn.
        std::this_thread::yield();
      }
    });
  } catch (...) {
    session->running.store(false);
    activeSession.store(nullptr, std::memory_order_release);
    delete session;
    __android_log_print(ANDROID_LOG_ERROR, "Pokegochi",
                         "Native worker thread creation failed at startup");
    return 0;
  }
  return reinterpret_cast<jlong>(session);
}

extern "C" JNIEXPORT void JNICALL
Java_com_pokegochi_app_NativeFirmware_destroy(JNIEnv*, jclass, jlong handle) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return;
  if (activeSession.load(std::memory_order_acquire) == session)
    activeSession.store(nullptr, std::memory_order_release);
  session->running.store(false);
  if (session->worker.joinable()) session->worker.join();
  {
    std::lock_guard<std::mutex> lock(session->surfaceMutex);
    if (session->surface) {
      ANativeWindow_release(session->surface);
      session->surface = nullptr;
    }
  }
  delete session;
}

extern "C" JNIEXPORT void JNICALL
Java_com_pokegochi_app_NativeFirmware_touch(JNIEnv*, jclass, jlong handle,
                                             jint x, jint y, jboolean pressed) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return;
  // Touch arrives on Android's frame thread while the firmware worker may be
  // inside loop(). Keep the Android shim state change ordered with the runtime
  // so a tap that starts a battle cannot race the native scene transition.
  std::lock_guard<std::mutex> lock(session->runtimeMutex);
  pokegochi_android_set_touch(static_cast<int16_t>(x), static_cast<int16_t>(y), pressed == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_pokegochi_app_NativeFirmware_setSurface(JNIEnv* env, jclass,
                                                  jlong handle, jobject surface) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return;
  std::lock_guard<std::mutex> lock(session->surfaceMutex);
  if (session->surface) {
    ANativeWindow_release(session->surface);
    session->surface = nullptr;
  }
  if (!surface) return;
  session->surface = ANativeWindow_fromSurface(env, surface);
  if (!session->surface) {
    __android_log_print(ANDROID_LOG_ERROR, "Pokegochi",
                        "ANDROID_NATIVE_SURFACE attach failed");
    return;
  }
  const int geometryResult = ANativeWindow_setBuffersGeometry(
      session->surface, 320, 240, WINDOW_FORMAT_RGB_565);
  session->surfaceNeedsRedraw = true;
  __android_log_print(geometryResult == 0 ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                      "Pokegochi",
                      "ANDROID_NATIVE_SURFACE attach geometry=%d size=%dx%d format=%d",
                      geometryResult, ANativeWindow_getWidth(session->surface),
                      ANativeWindow_getHeight(session->surface),
                      ANativeWindow_getFormat(session->surface));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_pokegochi_app_NativeFirmware_presentFrame(JNIEnv*, jclass,
                                                    jlong handle) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return JNI_FALSE;

  const uint64_t startedNs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
  std::lock_guard<std::mutex> surfaceLock(session->surfaceMutex);
  if (!session->surface) return JNI_FALSE;

  bool newFrame = false;
  size_t queueDepth = 0;
  uint32_t fullDrops = 0;
  uint32_t staleDrops = 0;
  uint32_t pacingWaits = 0;
  uint64_t copiedFrames = 0;
  const uint64_t nowNs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
  {
    std::lock_guard<std::mutex> frameLock(session->frameMutex);
    while (session->frameQueueCount > 1U) {
      const FrameSnapshot& candidate =
          session->frameQueue[session->frameQueueHead];
      if (!candidate.publishedAtNs || nowNs <= candidate.publishedAtNs ||
          nowNs - candidate.publishedAtNs <= kMaximumQueuedFrameAgeNs)
        break;
      session->frameQueueHead =
          (session->frameQueueHead + 1U) % kFrameQueueCapacity;
      --session->frameQueueCount;
      ++session->staleQueueDrops;
    }
    if (session->frameQueueCount) {
      if (session->nextFrameCopyAtNs && nowNs < session->nextFrameCopyAtNs) {
        ++session->pacingWaits;
      } else {
        const size_t oldest = session->frameQueueHead;
        const FrameSnapshot& frame = session->frameQueue[oldest];
        std::memcpy(session->presentationPixels.data(), frame.pixels.data(),
                    kFrameBytes);
        session->frameQueueHead = (oldest + 1U) % kFrameQueueCapacity;
        --session->frameQueueCount;
        ++session->copiedFrames;
        // Advance a fixed 60 Hz deadline instead of starting a fresh 16.67 ms
        // timer after every copy. The latter accumulated ANativeWindow post
        // and polling time and measured about 48 fps on the emulator. Small
        // misses are absorbed by the next deadline; after a real pause, reset
        // the clock so stale backlog is never replayed as a burst.
        if (!session->nextFrameCopyAtNs ||
            nowNs > session->nextFrameCopyAtNs + kMaximumPacingCatchupNs) {
          session->nextFrameCopyAtNs = nowNs + kMinimumFrameCopyIntervalNs;
        } else {
          session->nextFrameCopyAtNs += kMinimumFrameCopyIntervalNs;
        }
        session->hasPresentationFrame = true;
        newFrame = true;
      }
    }
    queueDepth = session->frameQueueCount;
    fullDrops = session->queueFullDrops;
    staleDrops = session->staleQueueDrops;
    pacingWaits = session->pacingWaits;
    copiedFrames = session->copiedFrames;
  }
  if (!newFrame && (!session->surfaceNeedsRedraw ||
                    !session->hasPresentationFrame))
    return JNI_FALSE;
  const uint64_t frameReadyNs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());

  ANativeWindow_Buffer buffer{};
  const int lockResult = ANativeWindow_lock(session->surface, &buffer, nullptr);
  const uint64_t bufferLockedNs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
  if (lockResult != 0 || !buffer.bits) {
    session->surfaceNeedsRedraw = true;
    __android_log_print(ANDROID_LOG_WARN, "Pokegochi",
                        "ANDROID_NATIVE_SURFACE lock failed result=%d", lockResult);
    return JNI_FALSE;
  }

  const int copyWidth = std::min<int>(320, buffer.width);
  const int copyHeight = std::min<int>(240, buffer.height);
  if (buffer.format == WINDOW_FORMAT_RGB_565) {
    auto* destination = static_cast<uint16_t*>(buffer.bits);
    for (int y = 0; y < copyHeight; ++y) {
      std::memcpy(destination + static_cast<size_t>(y) * buffer.stride,
                  session->presentationPixels.data() + static_cast<size_t>(y) * 320U,
                  static_cast<size_t>(copyWidth) * sizeof(uint16_t));
    }
  } else if (buffer.format == WINDOW_FORMAT_RGBA_8888 ||
             buffer.format == WINDOW_FORMAT_RGBX_8888) {
    auto* destination = static_cast<uint8_t*>(buffer.bits);
    for (int y = 0; y < copyHeight; ++y) {
      uint8_t* row = destination + static_cast<size_t>(y) * buffer.stride * 4U;
      const uint16_t* source =
          session->presentationPixels.data() + static_cast<size_t>(y) * 320U;
      for (int x = 0; x < copyWidth; ++x) {
        const uint16_t pixel = source[x];
        row[x * 4U + 0U] = static_cast<uint8_t>(((pixel >> 11U) & 0x1FU) * 255U / 31U);
        row[x * 4U + 1U] = static_cast<uint8_t>(((pixel >> 5U) & 0x3FU) * 255U / 63U);
        row[x * 4U + 2U] = static_cast<uint8_t>((pixel & 0x1FU) * 255U / 31U);
        row[x * 4U + 3U] = 0xFFU;
      }
    }
  } else {
    __android_log_print(ANDROID_LOG_ERROR, "Pokegochi",
                        "ANDROID_NATIVE_SURFACE unsupported format=%d",
                        buffer.format);
  }
  const uint64_t frameCopiedNs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
  const int postResult = ANativeWindow_unlockAndPost(session->surface);
  const uint64_t postedNs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
  if (postResult != 0) {
    session->surfaceNeedsRedraw = true;
    __android_log_print(ANDROID_LOG_WARN, "Pokegochi",
                        "ANDROID_NATIVE_SURFACE post failed result=%d", postResult);
    return JNI_FALSE;
  }
  session->surfaceNeedsRedraw = false;
  const uint64_t presented = ++session->presentedFrames;
  const uint64_t totalUs = (postedNs - startedNs) / 1000ULL;
  if (presented <= 2U || (newFrame && (presented & 3U) == 0U) ||
      totalUs > 30000ULL) {
    __android_log_print(
        ANDROID_LOG_INFO, "Pokegochi",
        "ANDROID_NATIVE_SURFACE n=%llu new=%u queue_copy_us=%llu lock_us=%llu "
        "buffer_copy_us=%llu post_us=%llu total_us=%llu queue=%u copied=%llu "
        "full_drop=%u stale_drop=%u pacing_wait=%u format=%d stride=%d",
        static_cast<unsigned long long>(presented), newFrame ? 1U : 0U,
        static_cast<unsigned long long>((frameReadyNs - startedNs) / 1000ULL),
        static_cast<unsigned long long>((bufferLockedNs - frameReadyNs) / 1000ULL),
        static_cast<unsigned long long>((frameCopiedNs - bufferLockedNs) / 1000ULL),
        static_cast<unsigned long long>((postedNs - frameCopiedNs) / 1000ULL),
        static_cast<unsigned long long>(totalUs), static_cast<unsigned>(queueDepth),
        static_cast<unsigned long long>(copiedFrames), static_cast<unsigned>(fullDrops),
        static_cast<unsigned>(staleDrops), static_cast<unsigned>(pacingWaits),
        buffer.format, buffer.stride);
  }
  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_pokegochi_app_NativeFirmware_isAlive(JNIEnv*, jclass, jlong handle) {
  const auto* session = reinterpret_cast<const FirmwareSession*>(handle);
  return session && session->running.load() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_pokegochi_app_NativeFirmware_multiplayerPlayerId(JNIEnv*, jclass, jlong handle) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return 0;
  std::lock_guard<std::mutex> lock(session->runtimeMutex);
  return static_cast<jint>(pokegochi_android_multiplayer_player_id());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_pokegochi_app_NativeFirmware_multiplayerEnabled(JNIEnv*, jclass, jlong handle) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return JNI_FALSE;
  std::lock_guard<std::mutex> lock(session->runtimeMutex);
  return pokegochi_android_multiplayer_enabled() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_pokegochi_app_NativeFirmware_multiplayerState(JNIEnv*, jclass, jlong handle) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return 0;
  std::lock_guard<std::mutex> lock(session->runtimeMutex);
  return static_cast<jint>(pokegochi_android_multiplayer_state());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_pokegochi_app_NativeFirmware_multiplayerPeerId(JNIEnv*, jclass, jlong handle) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return 0;
  std::lock_guard<std::mutex> lock(session->runtimeMutex);
  return static_cast<jint>(pokegochi_android_multiplayer_peer_id());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_pokegochi_app_NativeFirmware_multiplayerConnected(JNIEnv*, jclass, jlong handle) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return JNI_FALSE;
  std::lock_guard<std::mutex> lock(session->runtimeMutex);
  return pokegochi_android_multiplayer_is_connected() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_pokegochi_app_NativeFirmware_multiplayerTakeOutgoing(JNIEnv* env, jclass,
                                                               jlong handle) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return nullptr;
  uint8_t bytes[14U + 160U]{};
  uint16_t length = 0;
  {
    std::lock_guard<std::mutex> lock(session->runtimeMutex);
    if (!pokegochi_android_multiplayer_take_outgoing(bytes, &length)) return nullptr;
  }
  jbyteArray result = env->NewByteArray(static_cast<jsize>(length));
  if (result) env->SetByteArrayRegion(result, 0, static_cast<jsize>(length),
                                      reinterpret_cast<const jbyte*>(bytes));
  return result;
}

extern "C" JNIEXPORT void JNICALL
Java_com_pokegochi_app_NativeFirmware_multiplayerReceive(JNIEnv* env, jclass,
                                                          jlong handle, jbyteArray input) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session || !input) return;
  const jsize length = env->GetArrayLength(input);
  if (length <= 0 || length > 14 + 160) return;
  uint8_t bytes[14U + 160U]{};
  env->GetByteArrayRegion(input, 0, length, reinterpret_cast<jbyte*>(bytes));
  std::lock_guard<std::mutex> lock(session->runtimeMutex);
  pokegochi_android_multiplayer_receive(bytes, static_cast<size_t>(length));
}

extern "C" JNIEXPORT void JNICALL
Java_com_pokegochi_app_NativeFirmware_multiplayerDiscovered(JNIEnv* env, jclass,
                                                             jlong handle, jint playerId,
                                                             jint rssi, jstring address,
                                                             jint addressType) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session || !address) return;
  const char* addressBytes = env->GetStringUTFChars(address, nullptr);
  if (!addressBytes) return;
  std::lock_guard<std::mutex> lock(session->runtimeMutex);
  pokegochi_android_multiplayer_discovered(
      static_cast<uint32_t>(playerId), static_cast<int8_t>(rssi), addressBytes,
      static_cast<uint8_t>(addressType));
  env->ReleaseStringUTFChars(address, addressBytes);
}

extern "C" JNIEXPORT void JNICALL
Java_com_pokegochi_app_NativeFirmware_multiplayerTransportConnected(
    JNIEnv*, jclass, jlong handle, jboolean outbound) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return;
  std::lock_guard<std::mutex> lock(session->runtimeMutex);
  pokegochi_android_multiplayer_transport_connected(outbound == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_pokegochi_app_NativeFirmware_multiplayerDisconnected(JNIEnv*, jclass, jlong handle) {
  auto* session = reinterpret_cast<FirmwareSession*>(handle);
  if (!session) return;
  std::lock_guard<std::mutex> lock(session->runtimeMutex);
  pokegochi_android_multiplayer_disconnected();
}
