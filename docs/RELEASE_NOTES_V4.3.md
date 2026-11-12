# Pokegochi Firmware 4.3 / Android App 3.3

This coordinated release promotes the complete battle-engine, move-animation,
Ability, multiplayer and renderer audit to a new stable firmware and Android
version.

## Battle, renderer and collection corrections

- All 673 move IDs are covered on both battle sides across multiple seeds and
  states. The generated visual catalog covers 710 programs plus conditional
  variants without empty animations, default sprites or missing arenas.
- Shared animation state, full-screen transitions, HP presentation, weather,
  switching and multiplayer teardown use the audited engine paths rather than
  move-specific patches.
- Ability behavior, held items and move mechanics remain covered by the native
  audit and specialized scenario suites.
- Spinda now uses the four original GBA masks, personality-byte placement and
  normal/shiny body palettes. The renderer supports both canonical and legacy
  compacted asset palettes.
- The Android replica contains the same canonical asset pack and game core as
  the firmware release.

## Versions

- Android replica: `3.3` (`versionCode 121`, build `r442`)
- ESP32 firmware: `4.3`
- Persistent save format: `53`, with automatic migration from older formats
- Required asset pack: `55`
- BLE multiplayer protocol: `10`
- Online envelope protocol: `2` (server also accepts legacy v1 clients)
- Signed PGOTA protocol: `1`

## Upgrade compatibility

- The Android APK keeps the same application ID and signing certificate, so it
  installs over earlier releases and preserves the Android save.
- `Pokegochi-Firmware-v4.3.pgota` carries the complete canonical asset pack.
  The updater hashes installed SD objects and skips byte-identical payloads.
- The PGOTA never contains the game save. NVS and SPIFFS remain in place while
  firmware, SD objects, signature, hashes and rollback journal are validated.
- Existing save formats migrate atomically. Devices already offering Wireless
  Update can install this package directly; very old one-slot firmware still
  requires the documented one-time save-preserving USB bootstrap.

## Release artifacts

- [PokegochiApp-v3.3.apk](https://drive.google.com/file/d/1wA6P70FUgiOU_qtQs5vnNUUqpmpAKIRN/view?usp=drivesdk)
- [Pokegochi-Firmware-v4.3.pgota](https://drive.google.com/file/d/1Ms7y-OmD3TuPg3oNbY3Jz-3mwrHMwzXi/view?usp=drivesdk)

The PGOTA is stored as `application/x-pokegochi-pgota`, keeping its `.pgota`
suffix on Android downloads. Both artifacts are re-opened and hash-verified
locally before publication; Drive metadata and raw-download sizes are checked
again after the in-place upload.
