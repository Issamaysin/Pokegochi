# Pokegochi

Virtual-pet game firmware for the ESP32-2432S028R (CYD) board.

## Beta hardware

- ESP32-2432S028R with ILI9341 TFT and XPT2046 resistive touch
- microSD card formatted as FAT32
- momentary screen button on GPIO 35 with an external 10 kOhm pull-up to 3.3 V
- 5 V power bank connected through Micro-USB

## Build

The project uses PlatformIO and the Arduino framework.

```text
pio run
pio run --target upload
pio device monitor
```

This workspace also contains an isolated Windows build environment. From
PowerShell, run `./scripts/build.ps1` to reproduce the verified build without
changing the machine-wide Python, Git, or PATH configuration.

The firmware starts with a hardware diagnostic for display, touch, microSD,
screen button, backlight, and persistent storage. On the first start, the
player permanently chooses Bulbasaur, Charmander, or Squirtle. The touch UI
then provides FEED, BATHE, PLAY, random wild encounters, charged trainer battles,
Poké Ball capture, a
151-slot persistent Box, an active team of up to three Pokémon, and a 151-entry
Pokédex. BATHE clears a persistent battle status; cleanliness is not a timed
virtual-pet need.

Trainer battles use a maximum of three charges. One charge returns every three
hours, up to the cap. Wild encounters consume no charge and are the only
capturable battles. Both use the highest selected-team level for difficulty.
The current early-game encounter pool and graphics are a
vertical-slice implementation; FireRed ROM data and extracted assets will
replace these provisional tables and shapes in later content passes.

The consolidated rules, including sequential fixed-level Gyms and planned
Bluetooth PvP/trading, are documented in `docs/GAME_DESIGN.md`.

Care needs advance in deterministic 15-minute ticks. Important actions save
immediately; dirty background state is consolidated every 60 seconds to limit
flash wear.

## First-board checklist

1. Run `.tools/python312/python.exe scripts/build_sd_asset_pack.py`, format the
   microSD card as FAT32, and copy the contents of `.generated/sdcard` to its root.
   These local assets are generated from the supplied ROM/decomp and are not distributed.
2. Flash the `esp32-2432S028R` environment.
3. Confirm four green diagnostic statuses.
4. Touch START and verify the provisional home screen.
5. Choose a starter, start a manual battle, fight, switch party members, run,
   and capture with each available Poké Ball type.
6. Open BOX, assign captured Pokémon to all three team slots, select each one
   on HOME, then power-cycle and confirm all state persists.
7. Drain a manual encounter charge and confirm its timer advances.
8. Record raw touch values at all four corners for final calibration.

## Native verification

Run `./scripts/test.ps1` in PowerShell. The suite covers deterministic care,
manual encounter charges, capture and collection behavior, party assignment,
Pokédex flags, and the redundant one-save persistence policy.

## One-save policy

The production firmware has one permanent game per device. After the first
start it never offers New Game or Reset. If both redundant slots are invalid,
the firmware enters recovery instead of silently creating a new game.

Factory reset code is excluded from production builds. It can only be compiled
for bench tests by explicitly defining `POKEGOCHI_DEV_ALLOW_FACTORY_RESET`.

## Touch calibration

The initial calibration constants in `include/config/BoardConfig.h` are only
safe defaults. Raw touch readings are printed to the serial monitor. They must
be replaced with measurements from the actual board before game UI work.

## ROM files

ROM images are local inputs only and are ignored by Git. Extracted copyrighted
assets must not be committed or redistributed.
