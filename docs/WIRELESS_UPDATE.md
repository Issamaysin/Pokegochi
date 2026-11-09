# Wireless firmware + microSD update

Pokegochi can update both OTA application firmware and the versioned microSD
asset pack through Bluetooth Low Energy. The persistent game save lives in
internal SPIFFS and is not present in the update bundle. The same authenticated
BLE session can separately export, restore, or reset that save through the
Android tools dashboard.

## Safety model

- Every `.pgota` manifest is signed with ECDSA P-256 and verified by the ESP32.
- Firmware and every SD object have an individual SHA-256 digest.
- The new firmware is staged and hashed on microSD. A 339 KiB immutable
  factory recovery image backs up the current main partition and installs the
  candidate without loading the game or either radio stack.
- SD objects are staged under `/pokegochi/update` and become active only after
  all objects pass verification.
- The previous SD files remain as backups until the new firmware boots and the
  normal save and asset diagnostics pass.
- A journal plus an internal-flash recovery marker resumes interrupted
  commits, Bluetooth disconnects and power loss. If the candidate fails before
  the normal save/asset self-test accepts it, recovery restores the previous
  main image and the matching SD backups.
- Package sequence numbers prevent installing an older signed bundle by
  accident.

Closing the update screen preserves partial SD downloads, including the staged
firmware file. Select the same package later to resume; the running application
partition remains untouched until recovery has validated the complete hash.

## One-time USB bootstrap

Boards using either older one-slot layouts or the former symmetric two-slot
layout need one cable migration to the recovery layout:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\gravar_firmware.ps1 -Port COM6
```

The script identifies the physical ESP32, backs up NVS and SPIFFS, preserves or
migrates the save, then writes recovery at `0x10000` and the main game at
`0xC0000`. Both images are read back with `verify-flash` before success is
reported. SPIFFS remains at `0x3D0000`, so boards already on the latest layout
do not move or rewrite their save. If an older save layout needs migration and
USB flashing is interrupted, the next run resumes that migration for the same
chip. Backups remain under `backups/`.

For a genuinely blank board with no save worth preserving, explicitly use:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\gravar_firmware.ps1 -Port COM6 -FreshDevice
```

## Build a signed package

First build firmware and the current SD pack, then create the package:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\gravar_firmware.ps1 -BuildOnly
powershell -ExecutionPolicy Bypass -File .\scripts\build_wireless_update.ps1
```

The result is `dist/pokegochi-wireless-update.pgota`. On first use, the builder
creates `.keys/pokegochi-update-private.key` and embeds only its public key in
the firmware. Back up that private key securely. Never distribute or commit it;
losing it prevents producing updates accepted by already-installed devices.

The package contains one indexed `pokegochi.pak`, its three small metadata
files, and the application image. It intentionally does not duplicate the
unpacked asset tree.

## Build and install the Android updater

The current ready-to-install APK is available as
[Pokegochi-Updater.apk](https://drive.google.com/file/d/1dNQ5woQwcwu1yLnsyEQXe8Dk4vn2bl7B/view?usp=drivesdk).

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_android_updater.ps1
adb install -r .\dist\PokegochiUpdater.apk
```

The Android project is at `android/PokegochiUpdater`. It is a native offline
app: it needs Bluetooth permissions but no Internet or filesystem-wide access.

## Update from the phone

1. Keep the console on stable power and open **Settings > Wireless Update >
   Open Updater**.
2. On Android, select the `.pgota` file.
3. Scan and choose only the Pokegochi player ID shown by the app.
4. Enter the six-digit code displayed on the console.
5. Start the update and keep the devices nearby until the console restarts.
6. The candidate firmware runs the ordinary microSD, asset, and save checks.
   Only then are the old firmware/assets retired.

The current bundle is roughly 34 MiB, but the console hashes installed SD
objects against the signed manifest and skips every byte-identical object.
Consequently a normal firmware-only release transfers only the main image.
When assets really changed, the complete changed objects are transferred.

Current consoles advertise their supported data block size. The updater stays
compatible with the original 184-byte transport, while updated consoles use a
517-byte ATT MTU and 508-byte acknowledged payloads. The Android app requests
the high-throughput connection priority only for the transfer and restores the
balanced priority at completion. Every transmitted block remains acknowledged,
and every completed object still receives a full SHA-256 verification.

## Update directly from a Windows BLE adapter

The repository also includes a desktop client for PCs with Bluetooth Low
Energy support. It speaks the same authenticated protocol as the Android
updater, so the ESP32 still checks the six-digit pairing code, signed manifest,
SHA-256 hashes, and rollback-safe commit:

```powershell
.\.tools\python312\python.exe -m pip install bleak
.\.tools\python312\python.exe .\scripts\ble_wireless_update.py --scan
.\.tools\python312\python.exe .\scripts\ble_wireless_update.py `
  --address <BLE_ADDRESS> `
  --code <PAIRING_CODE> `
  --package .\dist\pokegochi-wireless-update-latest.pgota
```

Use the address and code shown by the current console. The client uses the
negotiated Windows BLE MTU when available; the reference Intel adapter reached
MTU 517 and 508-byte acknowledged data blocks.

## Read-only BLE diagnostics

The same updater screen can expose a read-only diagnostic report without a
cable. It uses the pairing code but never starts an update, changes the save,
or writes the SD card. With the console on **Settings > Wireless Update > Open
Updater**, run:

```powershell
.\.tools\python312\python.exe .\scripts\ble_wireless_diagnostic.py --scan
.\.tools\python312\python.exe .\scripts\ble_wireless_diagnostic.py --code <PAIRING_CODE>
.\.tools\python312\python.exe .\scripts\ble_wireless_diagnostic.py `
  --code <PAIRING_CODE> --section animation --move <MOVE_ID>
```

The summary reports firmware, SD archive size, pack version, animation catalog,
and runtime storage state. The animation report checks the selected move's
generated program and the actual frame files in the installed `.pak`; this is
the useful readback when a move or sprite is still appearing as a placeholder.

## Live renderer capture

For bugs that black-screen or reset the board, use the live capture instead of
trying to open the updater after the failure. Open **Settings > Wireless Update
> Open Updater**, keep the pairing code visible, and start this on the PC:

```powershell
.\.tools\python312\python.exe .\scripts\ble_live_diagnostic.py `
  --address <BLE_ADDRESS> `
  --code <PAIRING_CODE> `
  --output .\dist\live-render.log
```

The tool authenticates first, subscribes to the live-log characteristic, and
then the console leaves the updater screen automatically while keeping that BLE
connection alive. Play normally with the terminal running. Each animation,
field composition, cancelled frame, and completed move is printed and flushed
to the output file immediately. If the ESP32 resets or the adapter disconnects,
the last received event remains in the file; send that file with the next bug
report. A negotiated MTU of at least 50 is required; the normal high-throughput
Windows path reaches 517.

## Save file tools

After choosing the console and entering its six-digit code, the app opens a
separate authenticated dashboard:

- **Load file · Pokegochi → Android** creates a `.pgsav` backup with a suggested
  player-ID and timestamp filename. It does not change the console.
- **Save file · Android → Pokegochi** validates the selected backup locally,
  shows its source player and save format, asks for confirmation, then uploads
  it. The console validates the complete file again before atomically writing
  the next redundant save record and restarting.
- **Reset file · Erase progress** asks for destructive confirmation, commits a
  fresh default save as the next redundant record, and restarts. A power loss
  during the commit leaves the previous valid slot available.

The portable `PGS1` header carries the backup-format version, current save ABI,
payload length, source player ID, payload CRC-32, and header CRC-32. A whole-file
CRC also protects the BLE import. Files with the wrong size, ABI, or checksum
are rejected before the current durable save is replaced.

## Partition layout

| Partition | Offset | Size |
|---|---:|---:|
| NVS | `0x9000` | `0x5000` |
| OTA metadata | `0xE000` | `0x2000` |
| Factory recovery | `0x10000` | `0xB0000` |
| Main application | `0xC0000` | `0x310000` |
| SPIFFS save | `0x3D0000` | `0x30000` |

The package builder refuses firmware larger than the main partition. The
recovery build is separately limited to its 704 KiB factory partition.
