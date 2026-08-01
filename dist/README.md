# Pokegochi beta firmware

Build verified for the PlatformIO environment `esp32-2432S028R` on
2026-07-31.

- `pokegochi-beta.factory.bin`: complete first-install image, flash at
  address `0x0000`; SHA-256
  `CB35BE7997FF7375C3DF94F26C653D91B59EA4B2414131C49919124CDC499896`
- `pokegochi-beta.update.bin`: application-only image, flash at address
  `0x10000`; SHA-256
  `3256CE7F2CE9B4823C42A44198F35213E315653E619EF2CD96DF3481D27C2D02`

The factory image is the recommended artifact for a new board. Hardware touch
calibration still needs to be performed on the physical device.
# Pokegochi beta binaries

- `pokegochi-beta.factory.bin`: complete image for first flashing at address `0x0`.
- `pokegochi-beta.update.bin`: application-only image for address `0x10000`.

The FireRed graphic pack is intentionally not distributed in this folder. Generate it
locally with `scripts/build_sd_asset_pack.py` and copy `.generated/sdcard` to the root
of the FAT32 microSD card.
