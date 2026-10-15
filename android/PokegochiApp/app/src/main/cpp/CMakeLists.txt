cmake_minimum_required(VERSION 3.22.1)
project(pokegochi_core)

get_filename_component(POKEGOCHI_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../../" ABSOLUTE)

file(GLOB POKEGOCHI_GAME_SOURCES CONFIGURE_DEPENDS
    "${POKEGOCHI_SOURCE_ROOT}/src/game/*.cpp")
list(REMOVE_ITEM POKEGOCHI_GAME_SOURCES
    "${POKEGOCHI_SOURCE_ROOT}/src/game/HomeBackgrounds.cpp")

set(POKEGOCHI_SERVICE_SOURCES
    "${POKEGOCHI_SOURCE_ROOT}/src/services/Multiplayer.cpp")

set(POKEGOCHI_FIRMWARE_RUNTIME_SOURCES
    "${POKEGOCHI_SOURCE_ROOT}/src/main.cpp"
    "${POKEGOCHI_SOURCE_ROOT}/src/drivers/AssetRenderer.cpp"
    "${POKEGOCHI_SOURCE_ROOT}/src/drivers/Backlight.cpp"
    "${POKEGOCHI_SOURCE_ROOT}/src/drivers/FireRedFont.cpp"
    "${POKEGOCHI_SOURCE_ROOT}/src/drivers/RgbLed.cpp"
    "${POKEGOCHI_SOURCE_ROOT}/src/drivers/TouchDriver.cpp"
    "${POKEGOCHI_SOURCE_ROOT}/src/services/PersistentSave.cpp")

list(APPEND POKEGOCHI_GAME_SOURCES
    "${POKEGOCHI_SOURCE_ROOT}/src/game/HomeBackgrounds.cpp")

add_library(pokegochi_core SHARED
    ${POKEGOCHI_GAME_SOURCES}
    ${POKEGOCHI_SERVICE_SOURCES}
    ${POKEGOCHI_FIRMWARE_RUNTIME_SOURCES}
    android_hal.cpp
    pokegochi_firmware_jni.cpp)

target_include_directories(pokegochi_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/android_shim"
    "${POKEGOCHI_SOURCE_ROOT}/include"
    "${POKEGOCHI_SOURCE_ROOT}/src")
target_compile_features(pokegochi_core PRIVATE cxx_std_17)
target_compile_definitions(pokegochi_core PRIVATE POKEGOCHI_APP_BUILD=1 POKEGOCHI_ANDROID=1)
if (CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    target_compile_definitions(pokegochi_core PRIVATE POKEGOCHI_RELEASE=1)
endif()
# The debug APK is the build installed on a physical phone during iteration.
# Android's default Debug CMake configuration otherwise emits this large
# firmware runtime without optimisation, which makes the 60 Hz animation VM
# look like a renderer bug even when the frame path itself is correct.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(pokegochi_core PRIVATE -O2)
endif()
# Android 15+ devices may use 16 KiB memory pages. Keep every PT_LOAD segment
# aligned for those devices while remaining compatible with 4 KiB phones.
target_link_options(pokegochi_core PRIVATE "-Wl,-z,max-page-size=16384")
target_link_libraries(pokegochi_core PRIVATE log android)
