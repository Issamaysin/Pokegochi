# Pokegochi Firmware 4.0 / Android App 3.0

This coordinated release closes the post-Sun/Moon content line on the ESP32
Pokegochi and its Android replica. Both builds use the same gameplay core,
save format and canonical asset archive.

## Highlights

- Complete regional progression through Sinnoh, Unova, Kalos and Alola on top
  of the original Kanto, Johto and Hoenn game.
- Sinnoh site exploration with eight Gyms; Unova procedural areas, Pokemon
  World Tournament and Royal Unova; Kalos regional campaign; and Alola island
  trials with Sun and Moon themes.
- Generation I-VII species, evolutions, moves, abilities, TMs, held items,
  regional variants and form-specific stats, types, learnsets and sprites.
- Mega Evolution, Primal forms, Z-Moves, Ultra Necrozma, Zygarde Complete,
  Alolan species as independent Pokemon, and the unique form tools for Kyurem,
  the Forces of Nature, Genesect, Keldeo, Meloetta, Hoopa and Necrozma.
- Battle animations share the ROM-derived engine on ESP32 and Android, support
  1x/2x/3x speed and retain authored particles, backgrounds and event order.
- Bluetooth multiplayer supports repeated battles and trades between Android
  and the physical device, including recovery after cancellation or link loss.
- The expanded Box, Pokedex, Summary, Bag, Key Items and Market support the
  complete regional progression without deleting or rewriting older saves.
- Mega Stone targeting is non-destructive while browsing Party/Box, and Key
  Items use a three-row paginated layout with correctly centred artwork.

## Versions

- Android replica: `3.0` (`versionCode 104`, build `r317`)
- ESP32 firmware: `4.0 ALOLA`
- Persistent save format: `51` with automatic migration from older formats
- Required asset pack format: `54`
- Bluetooth multiplayer protocol: `7`

## Release artifacts

- `Pokegochi-Android-3.0-r317.apk`
- `Pokegochi-Firmware-4.0-ALOLA.bin`
- `Pokegochi-Firmware-4.0-ALOLA.factory.bin`
- `Pokegochi-Firmware-4.0-ALOLA.pgota`
- `pokegochi-assets-v54.zip`
- `SHA256SUMS.txt`

The APK is signed with the same update certificate as previous Android
releases so it can be installed over an existing copy without clearing its
save. The `.pgota` bundle is authenticated with the project's P-256 key and
contains the firmware plus the canonical asset-pack payload.

For an existing physical Pokegochi, use the signed wireless package or the
save-preserving USB update workflow. Do not erase flash or overwrite the save
partition when upgrading.
