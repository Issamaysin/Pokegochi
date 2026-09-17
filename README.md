# Pokegochi V2

Virtual-pet game firmware for the ESP32-2432S028R (CYD) board.

The ready-to-install 2.0 distribution is in
[`POKEGOCHI_V2/`](POKEGOCHI_V2/README.md). It contains the firmware images,
microSD package, Android BLE updater, standalone Windows installation scripts,
complete hardware list, and the battery/charging/soldering guide.

## Hardware

- ESP32-2432S028R with ILI9341 TFT and XPT2046 resistive touch
- microSD card formatted as FAT32
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

## Bluetooth wireless updates

The firmware now includes a Settings entry for signed Bluetooth updates of
both the inactive firmware slot and the microSD asset pack. A small native
Android updater, package builder, one-time safe partition migration, resumable
SD staging, SHA-256 verification, and automatic rollback are included. The
persistent one-save data is never copied into or replaced by an update bundle.

Build the phone APK with `./scripts/build_android_updater.ps1` and a signed
bundle with `./scripts/build_wireless_update.ps1`. Existing boards require one
USB installation of the new two-slot layout. The exact workflow and recovery
guarantees are in [docs/WIRELESS_UPDATE.md](docs/WIRELESS_UPDATE.md).

## Gravacao rapida no Windows

Os dois scripts abaixo evitam reconstruir ambientes que nao fazem parte do
firmware normal e podem ser usados sem o Codex.

Para compilar somente o firmware de producao, detectar a placa CH340 e gravar
sem apagar o save persistente:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\gravar_firmware.ps1
```

Se houver mais de uma placa conectada, informe a porta explicitamente:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\gravar_firmware.ps1 -Port COM6
```

Para apenas compilar, sem gravar:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\gravar_firmware.ps1 -BuildOnly
```

Para copiar e verificar o pacote de assets ja pronto em um microSD (troque
`D:` pela letra correta):

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\gravar_sd.ps1 -Drive D:
```

Use `-RebuildAssets` somente quando sprites, fundos ou outros arquivos graficos
forem alterados. Use `-Format` apenas quando quiser apagar e preparar o cartao
como FAT32; por seguranca o script exige uma confirmacao adicional:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\gravar_sd.ps1 -Drive D: -RebuildAssets
powershell -ExecutionPolicy Bypass -File .\scripts\gravar_sd.ps1 -Drive D: -Format
```

O script do SD valida a unidade, espaco livre e o SHA-256 do pacote copiado.

Asset pack v22 stores the PKG2 payloads in one indexed `pokegochi.pak`, with
numbered animation cels adjacent in the stream. At boot the firmware qualifies
10, 8, 4 and 2 MHz using repeated 64 KiB reads (with 1 MHz/400 kHz fallbacks),
then keeps the fastest stable rate. A complete animation preload uses one
bounded archive open and closes it before rendering; the visible move dialogue
is presented before that preload, so playback itself remains RAM-only.

The firmware starts with a hardware diagnostic for display, touch, microSD,
backlight, and persistent storage. On the first start, the
player permanently chooses Bulbasaur, Charmander, or Squirtle. The touch UI
then provides FEED, BATHE, PLAY, random wild encounters, charged trainer battles,
Poké Ball capture, a rotating Mart, three-stage Gyms, regional Pokemon Leagues,
a 386-slot persistent Box, an active team of up to three Pokémon, and a regional
Pokédex. BATHE clears
persistent battle status and restores PP; cleanliness is not a timed need.

Each individual Pokemon has permanent six-stat IVs, one of the 25 FireRed
Natures, and its personality-selected species Ability. Battle stats use the
Generation III formulas without EVs. These traits persist through capture,
level-up, evolution, Box movement, eggs, and save/load; evolution never rerolls them.

The Mart and Bag also support 37 Generation III held items. Berries and White
Herb are consumed automatically, while permanent equipment covers healing,
turn order, critical hits, survival, prize money, escape, Choice Band locking,
and all 17 type boosts. Held-item offers unlock with Badges and use weighted
rarity; EV, breeding, permanent-stat, and bonus-EXP items are intentionally absent.

Trainer battles use a maximum of three charges. One charge returns every three
hours, up to the cap. Wild encounters consume no charge and are the only
capturable battles. Both use the highest selected-team level for difficulty.
Species, moves, learnsets, encounter progression, trainer identities and Pokedex
metadata are generated from the supplied local FireRed data. Original graphics
are converted into a local-only microSD asset pack and are not distributed.

The collection and Box support all 386 Generation I-III species. The Pokedex is
strictly regional: it exposes 001-151 at first, 001-251 only after the Johto
starter choice, and 001-386 only after the Hoenn starter choice. All nine
regional starters are exclusive choices and are excluded from wild encounters.
Each regional starter choice is gated by eight Badges followed by that region's
Elite Four and Champion; clearing the Gym sequence alone never expands the Pokedex.
Completing each unlocked generation's visible Pokedex awards one Master Ball through
a persistent on-screen reward flow; it cannot be collected twice after a reboot.

The consolidated rules, including sequential fixed-level Gyms and planned
Bluetooth PvP/trading, are documented in `docs/GAME_DESIGN.md`.

Care needs advance in deterministic 15-minute ticks. Important actions save
immediately; dirty background state is consolidated every 60 seconds to limit
flash wear.

Display Settings provide 20/40/60/80/100% brightness and automatic screen-off
choices of 1, 2, 5, or 10 minutes. With the panel off, the ESP32 enters light
sleep and wakes once per second to preserve game clocks. Pressing the resistive
screen wakes through the XPT2046 IRQ; that first touch is consumed and never
activates a UI control. A waiting wild encounter flashes the onboard RGB LED for
one minute while the display remains asleep.

All game text is drawn with a compact mask conversion of the original FireRed
Latin font rather than TFT_eSPI's bundled fonts. The local generator is
`scripts/generate_firered_font.py`; it reads the same local FireRed asset set as
the rest of the non-distributable graphics pipeline.

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
8. Let the display time out, touch once to wake it, and confirm no UI action is
   triggered by the wake touch.
9. Record raw touch values at all four corners for final calibration.

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
