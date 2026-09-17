# Pokegochi V2

Pokegochi is a persistent Pokemon-inspired virtual pet and battle game for the
ESP32-2432S028R (CYD). It adapts the FireRed visual language and Generation III
battle rules to a 320x240 landscape touchscreen.

The ready-to-install V2 distribution is in
[`POKEGOCHI_V2/`](POKEGOCHI_V2/README.md). It contains the firmware images,
FAT32 microSD package, Android Bluetooth updater, signed OTA package,
standalone Windows installation scripts, hardware documentation, and the
battery/charging/soldering guide.

## Current game

### Progression and collection

- Professor Oak introduces the game before the permanent Kanto starter choice:
  Bulbasaur, Charmander, or Squirtle.
- The single persistent collection supports all 386 Generation I-III species,
  a three-Pokemon Party, a 386-slot Box, sorting, summaries, and regional
  Pokedex completion rewards.
- Kanto, Johto, and Hoenn unlock sequentially. Each region requires its eight
  Gyms, Elite Four, and Champion before the next regional starter is offered.
- Regional starters do not appear in wild encounters. The Pokedex expands only
  after the player chooses the newly unlocked starter.
- The post-Hoenn Mega Stone challenge unlocks Mega Evolution and the repeatable
  Battle Tower.

### Battles

- Wild encounters are always available from Home, have no charge or cooldown,
  and are the only battles in which Pokemon can be captured.
- The VS Seeker starts trainer battles. It stores three charges; each spent
  charge independently returns after a randomized 30-60 minute interval.
- Gyms unlock deterministically from the highest Party level and remain
  available until defeated. Each Gym is a sequence of three battles.
- Regional Leagues contain the Elite Four and Champion. The Battle Tower uses
  three consecutive Emerald-derived trainers and is permanently available once
  unlocked.
- Experience is divided equally among the selected Party Pokemon. The unique
  Lucky Egg applies its party-wide multiplier before that split.
- The engine covers all 354 Generation III moves, their special effects,
  abilities, persistent status, held items, trainer items, weather, switching,
  capture, evolution, and FireRed-style battle event ordering.

### Individual Pokemon

- Every Pokemon persists its IVs, naturally earned EVs, Nature, Ability,
  personality, gender, friendship, shiny state, moves, PP, held item, and form.
- EVs follow the Generation III per-stat and total limits. Shiny Pokemon receive
  the intentional extra 255-point total pool defined by Pokegochi. The Summary
  EV page is visible at every level and becomes editable at Lv.90.
- Alternate forms include personality-derived Unown and Spinda variants,
  battle-local Castform forms, selectable Deoxys forms, and supported Mega and
  Primal forms.
- The fourth Summary page can rebuild a moveset from the moves legally learned
  by that Pokemon and its pre-evolutions up to its current level.

### Recovery, Eggs, Bag, and Mart

- The Pokemon Center stores five charges and restores one charge every hour.
  One use fully restores the selected Party's HP, PP, and status.
- Party Pokemon through Lv.20 recover passively in 10 minutes; higher-level
  Party Pokemon recover in 30 minutes. Box Pokemon recover in three hours.
  Fainted Pokemon remain unavailable until fully healed or revived.
- The Day Care offers regional Eggs after the third Badge. Incubation uses real
  elapsed time, is accelerated by Flame Body or Magma Armor, and hatch level is
  based on the strongest completed Gym, capped at Lv.89.
- The Bag supports battle and field use, Party or Box targets, held items,
  TMs/HMs, Trainer Card, Badge Case, and Bluetooth multiplayer.
- The Mart has twenty stocked offers across four pages, guaranteed essentials,
  unique TMs/HMs, quantity purchasing, Badge-gated items, and a six-hour
  rotation.

### Interface and connectivity

- Home uses 39 collision-aware FireRed/Emerald map backgrounds. Party Pokemon
  patrol valid terrain and may play type-based ambient effects.
- Sapphire, Ruby, and Emerald color themes apply throughout the interface.
- Brightness, automatic screen timeout, battle-text advance, background,
  player statistics, and the manual pocket lock are available in Settings.
- The lock screen uses a swipeable Poke Ball so an accidental pocket touch does
  not activate the game.
- Bluetooth multiplayer supports discovery between Pokegochi devices, confirmed
  PvP battles, and confirmed Pokemon trades. Closing the Bag switches the radio
  off and releases its RAM.
- Settings also provides signed Bluetooth updates for both firmware and the
  microSD asset pack through the Android updater.

## Permanent save policy

Production firmware exposes exactly one game per device. There is no New Game,
second save, or player-accessible reset. Critical actions are committed to
redundant persistent records, and invalid copies open Save Recovery instead of
silently replacing the player's game.

Factory reset code is excluded from production builds. Bench firmware must
explicitly define `POKEGOCHI_DEV_ALLOW_FACTORY_RESET`.

## Hardware

- ESP32-2432S028R with ILI9341 TFT and XPT2046 resistive touch
- FAT32 microSD card
- During development: 5 V power bank through Micro-USB
- Final portable build: protected 1S Li-ion/LiPo cell, charger with power-path,
  regulated 5 V converter, and the documented battery-sense divider

See
[`POKEGOCHI_V2/docs/BATERIA_E_SOLDAGEM.md`](POKEGOCHI_V2/docs/BATERIA_E_SOLDAGEM.md)
before connecting a cell. Never connect a Li-ion cell directly to a GPIO or
feed raw cell voltage into the board's 5 V input.

## Installation

Read [`POKEGOCHI_V2/docs/INSTALACAO.md`](POKEGOCHI_V2/docs/INSTALACAO.md), or
use the standalone scripts from the V2 distribution:

```powershell
# Prepare and verify a FAT32 microSD card.
powershell -ExecutionPolicy Bypass -File .\POKEGOCHI_V2\instalar_sd.ps1 -Drive D:

# First installation on a board. Replace COM6 when necessary.
powershell -ExecutionPolicy Bypass -File .\POKEGOCHI_V2\instalar_firmware.ps1 -Mode Factory -Port COM6

# Prepare both components in sequence.
powershell -ExecutionPolicy Bypass -File .\POKEGOCHI_V2\instalar_tudo.ps1 -Drive D: -Port COM6 -Mode Factory
```

Use `-Mode Update` only on a board that already has the V2 two-slot partition
layout and whose persistent save must be preserved.

## Build and verification

The firmware uses PlatformIO and the Arduino framework. The repository also
contains isolated PowerShell build scripts so the machine-wide Python, Git, and
PATH configuration do not need to be changed.

```powershell
# Firmware build
.\scripts\build.ps1

# Native gameplay, save, data, and package verification
.\scripts\test.ps1

# Android updater
.\scripts\build_android_updater.ps1

# Signed firmware + SD wireless package
.\scripts\build_wireless_update.ps1
```

The release checksums can be verified separately:

```powershell
powershell -ExecutionPolicy Bypass -File .\POKEGOCHI_V2\verificar_release.ps1
```

## Documentation

- [Current gameplay specification](docs/GAME_DESIGN.md)
- [Hardware and power system](docs/POWER_SYSTEM.md)
- [Bluetooth wireless update](docs/WIRELESS_UPDATE.md)
- [Implementation status](docs/IMPLEMENTATION_PLAN.md)
- [Battle animation audit](docs/BATTLE_ANIMATION_AUDIT.md)
- [Move-effect compatibility](docs/EFFECT_COMPATIBILITY.md)

## ROM and asset policy

ROM images are local development inputs and are excluded from Git. The private
V2 distribution contains the generated microSD package used by the project,
but never includes an original ROM image or save dump.
