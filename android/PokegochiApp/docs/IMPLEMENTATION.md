# PokegochiApp implementation

The app is kept inside the firmware repository because it executes the same
native firmware runtime instead of maintaining a second game implementation.

Current implementation:

- native Android build of the firmware `setup()`/`loop()` runtime;
- exact firmware framebuffer at 320x240, nearest-neighbor scaled on Android;
- firmware `TouchDriver` fed from Android touch coordinates;
- firmware `PersistentSave` and `Preferences` redirected to app-private paths;
- firmware asset pack copied into the same SD-style path used by the renderer;
- Android BLE central/peripheral transport only; all multiplayer state and
  packets remain owned by the firmware `Multiplayer` object;
- no OTA, sync, delete-save or firmware-update UI in the game app.
- the Android asset task stages the canonical `.pak` plus version/catalog/
  manifest metadata instead of packaging the ESP32's loose PKG mirrors;
- Bluetooth permissions are deferred until the firmware actually enables
  multiplayer.

Build:

```powershell
cd android\PokegochiApp
.\build.ps1
```

The native runtime is mandatory. The script always builds the firmware-backed
APK, including both `arm64-v8a` and `x86_64`.

The Android transport can operate as a central/client for Android-to-physical-
Pokegochi links and as a peripheral/GATT server for app-to-app links. The
firmware state machine drives when scanning, advertising, connecting and raw
packet exchange happen.
