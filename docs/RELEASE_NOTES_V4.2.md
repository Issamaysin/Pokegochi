# Pokegochi Firmware 4.2 / Android App 3.2

This coordinated release publishes the complete battle-engine, move-animation
and Ability audit on both the physical ESP32 Pokegochi and the Android replica.

## Battle and animation corrections

- All 673 move IDs were exercised on both battle sides across multiple seeds
  and states. The generated visual catalog covers 710 programs plus every
  conditional variant without empty animations, default sprites or missing
  arenas.
- Psychic, Psycho Cut, Night Slash, Bloom Doom, Confusion, Aurora Beam, Giga
  Impact and the complete Z-Move set use the corrected shared animation path.
- GBA script control-flow opcodes are preserved by the generator, restoring
  the conditional branches used by dozens of move animations.
- Whole-arena white, black and inverse blends are capped centrally and
  equivalent repaint requests are coalesced, reducing disruptive full-screen
  flashes while preserving projectiles and local impacts.
- The full mechanic matrix rejects accidental stat boosts and cross-move
  effects. Gale Wings now grants Flying priority only while the user has full
  HP, matching its intended modern behavior.
- Ability behavior and text were audited together, including corrected
  Portuguese strings and all specialized handlers.

## Versions

- Android replica: `3.2` (`versionCode 120`, build `r441`)
- ESP32 firmware: `4.2`
- Persistent save format: `53`, with automatic migration from older formats
- Required asset pack: `55`
- BLE multiplayer protocol: `10`
- Online envelope protocol: `2` (server also accepts legacy v1 clients)
- Signed PGOTA protocol: `1`

## Upgrade compatibility

- The Android APK keeps the same application ID and signing certificate, so it
  installs over earlier releases and preserves the Android save.
- `Pokegochi-Firmware-v4.2.pgota` carries the complete canonical asset pack.
  The updater hashes installed SD objects and skips byte-identical payloads.
- The PGOTA never contains the game save. NVS and SPIFFS remain in place while
  firmware, SD objects, signature, hashes and rollback journal are validated.
- Existing save formats migrate atomically. Devices already offering Wireless
  Update can install this package directly; very old one-slot firmware still
  requires the documented one-time save-preserving USB bootstrap.

## Release artifacts

- [PokegochiApp-v3.2.apk](https://drive.google.com/file/d/1wA6P70FUgiOU_qtQs5vnNUUqpmpAKIRN/view?usp=drivesdk)
- [Pokegochi-Firmware-v4.2.pgota](https://drive.google.com/file/d/1Ms7y-OmD3TuPg3oNbY3Jz-3mwrHMwzXi/view?usp=drivesdk)

The PGOTA is stored as `application/x-pokegochi-pgota`, keeping its `.pgota`
suffix on Android downloads. Both artifacts are re-opened and hash-verified
locally before publication; Drive metadata and raw-download sizes are checked
again after the in-place upload.
