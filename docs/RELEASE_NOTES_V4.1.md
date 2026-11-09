# Pokegochi Firmware 4.1 / Android App 3.1

This coordinated release makes Wi-Fi multiplayer a supported public feature
on the physical ESP32 Pokegochi and the Android replica. The existing BLE
multiplayer remains available on its own Bag page.

## Wi-Fi multiplayer

- Separate `MULTIPLAYER BLE` and `MULTIPLAYER WIFI` pages, with battle and
  trade kept independent from the local game state.
- Automatic local-server discovery and Internet relay fallback. Android uses
  the phone's current connection without asking for an IP or port; the physical
  Pokegochi stores up to four Wi-Fi networks in most-recently-used order.
- The player who sends the invitation selects battle or trade and, for battle,
  `NORMAL` or `BALANCED`. The other player accepts or rejects that proposal.
- Balanced battles temporarily normalize the selected Pokemon to level 70.
  Both modes begin with full battle-local HP and PP and use the same temporary
  Bag: one Max Revive, three Hyper Potions, four Full Heals and one Max Elixir.
- Party selection remains editable until that local player presses READY.
  READY, lead selection, forced switching after faint, repeated battles,
  cancellation, disconnects and reconnect cleanup are synchronized explicitly.
- Wi-Fi trades preserve the received Pokemon, form and sprite identity without
  mutating the sender's normal Party until both sides confirm.
- The online club lists only player ID and balanced wins. A built-in server bot
  provides a complete three-Pokemon battle when no second player is available.
- The ESP32 renderer and radio now lend memory to each other safely, recover the
  Home/Summary/Pokemon Center scenes after radio shutdown and avoid the display
  corruption previously seen around Wi-Fi sessions.

## Versions

- Android replica: `3.1` (`versionCode 117`, build `r410`)
- ESP32 firmware: `4.1 WIFI`
- Persistent save format: `52`, with automatic migration from older formats
- Required asset pack: `55`
- BLE multiplayer protocol: `10`
- Online envelope protocol: `2` (server also accepts legacy v1 clients)
- Signed PGOTA protocol: `1`

## Upgrade compatibility

- The Android APK uses the same application ID and signing certificate as 3.0,
  so it installs in place and preserves the Android save.
- `Pokegochi-Firmware-v4.1.pgota` contains the complete canonical asset pack.
  A device with an old SD transfers every changed object; an up-to-date SD skips
  byte-identical objects after hash comparison.
- The package never contains the game save. Firmware, SD objects and rollback
  journal are verified before activation, while NVS and SPIFFS save slots stay
  in place. Legacy saves are migrated atomically by the firmware.
- Any Pokegochi already offering `Settings > Wireless Update` can install the
  package directly. Very old one-slot firmware still needs the documented
  one-time save-preserving USB bootstrap before its first PGOTA update.

## Release artifacts

- [PokegochiApp-v3.1.apk](https://drive.google.com/file/d/1wA6P70FUgiOU_qtQs5vnNUUqpmpAKIRN/view?usp=drivesdk)
- [Pokegochi-Firmware-v4.1.pgota](https://drive.google.com/file/d/1Ms7y-OmD3TuPg3oNbY3Jz-3mwrHMwzXi/view?usp=drivesdk)
- `Pokegochi-Firmware-v4.1.bin`
- `Pokegochi-Recovery-v4.1.bin`
- `Pokegochi-Online-Server-v1.0.bin`
- `SHA256SUMS.txt`

The APK and PGOTA links above point to the verified Google Drive mirror.
SHA-256 values are published with the GitHub release.
