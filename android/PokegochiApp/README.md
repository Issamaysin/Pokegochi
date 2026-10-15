# PokegochiApp

Android version of Pokegochi. The phone runs the actual firmware runtime from
`src/main.cpp`: the same 320x240 renderer, screens, input handling, save format,
game rules, assets and `Multiplayer` state machine. Android supplies only the
display surface, touch coordinates, storage roots and BLE transport.

The app deliberately has no OTA, sync, delete-save or firmware-update UI. Its
save is local to the phone. A release build requires the native core.

The Android packaging task stages only `pokegochi.pak` and its runtime metadata
from `.generated/sdcard`; the thousands of loose PKG mirrors remain available
for the ESP32 SD-card image without being duplicated in the APK. Bluetooth
permissions are requested only when the firmware enables multiplayer, so the
single-player boot flow is not interrupted.

Build the native firmware runtime with:

```powershell
cd android\PokegochiApp
.\build.ps1
```

For a release APK:

```powershell
.\build.ps1 -Release
```

`-EnableNativeCore` is accepted for compatibility with older commands, but is
no longer necessary: a build without the firmware runtime is not supported.
