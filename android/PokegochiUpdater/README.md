# Pokegochi Updater for Android

Native BLE client for signed Pokegochi `.pgota` packages. It validates all
payload lengths and SHA-256 hashes locally, discovers only Pokegochi update
advertisements, authenticates with the six-digit console code, and resumes SD
payloads after a disconnect. Consoles that advertise clock support also
receive the phone's current local time immediately after authentication.
The app also offers **Sync clock only** for the one-time synchronization right
after installing the first clock-capable firmware, without selecting or
retransmitting an update package.

Build from the repository root with:

```powershell
.\scripts\build_android_updater.ps1
```

The debug APK is copied to `dist/PokegochiUpdater.apk`. See
`docs/WIRELESS_UPDATE.md` for the complete console and signing workflow.
