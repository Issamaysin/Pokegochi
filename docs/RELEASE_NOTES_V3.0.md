# Pokegochi Firmware 3.0 / Android App 2.0

This coordinated release brings the ESP32 firmware and the Android replica to
the same current gameplay baseline.

## Highlights

- Full progression through Unova, including its regional activities,
  tournament flow, species, moves and alternative forms.
- Form-aware stats, types, abilities, sprites and learnsets, including Black
  Kyurem, White Kyurem, Zen Mode, the Therian trio, Keldeo and Meloetta.
- Generation V move data and battle animations with dedicated regression
  coverage.
- Bluetooth multiplayer interoperability between the Android app and the
  physical Pokegochi, including consecutive battles and trades.
- Android rendering, battle timing, offline clock progression and mobile input
  fixes accumulated since app 1.1.
- Save migrations remain automatic. Installing app 2.0 over the previous APK
  keeps the Android save when the signing certificate is unchanged.

## Versions

- Android: `2.0` (`versionCode 77`)
- ESP32 firmware: `3.0 UNOVA`
- Required asset pack format: `41`

## Release artifacts

The GitHub release contains:

- `Pokegochi-Android-2.0-v77.apk`
- `Pokegochi-Firmware-3.0-UNOVA.bin`
- `Pokegochi-Firmware-3.0-UNOVA.factory.bin`
- `Pokegochi-Firmware-3.0-UNOVA.pgota`
- `SHA256SUMS.txt`

The APK is signed with the same update certificate used by previous Android
releases. The `.pgota` bundle is signed with the project's P-256 update key and
contains firmware 3.0 plus asset pack 41.

For a direct USB firmware update that preserves an existing save, use the
project's `scripts/gravar_firmware.ps1` workflow. Do not erase the flash or
overwrite the persistent save partition.
