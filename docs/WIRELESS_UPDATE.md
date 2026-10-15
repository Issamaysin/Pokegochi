# Wireless firmware + microSD update

Pokegochi can update both OTA application firmware and the versioned microSD
asset pack through Bluetooth Low Energy. The persistent game save lives in
internal SPIFFS and is not present in the update bundle. The same authenticated
BLE session can separately export, restore, or reset that save through the
Android tools dashboard.

## Safety model

- Every `.pgota` manifest is signed with ECDSA P-256 and verified by the ESP32.
- Firmware and every SD object have an individual SHA-256 digest.
- The new firmware is written to the inactive OTA slot.
- SD objects are staged under `/pokegochi/update` and become active only after
  all objects pass verification.
- The previous SD files remain as backups until the new firmware boots and the
  normal save and asset diagnostics pass.
- A journal plus an internal-flash boot marker recovers interrupted commits,
  Bluetooth disconnects, power loss, and a failed first boot.
- Package sequence numbers prevent installing an older signed bundle by
  accident.

Closing the update screen preserves partial SD downloads. Select the same
package later to resume. A partially written firmware image restarts from byte
zero; the previously running application remains untouched.

## One-time USB bootstrap

Older boards have only one application slot. Install this OTA-capable layout
once over USB before wireless updates can work:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\gravar_firmware.ps1 -Port COM6
```

The script identifies the physical ESP32, backs up the old SPIFFS partition,
migrates it to the new location, flashes the two-slot partition table, writes
the migrated save back, and verifies it byte-for-byte. Finally it resets only
the 8 KiB OTA-selection metadata, so the ESP32 boots the freshly flashed
`app0` rather than a stale `app1`; it never erases NVS or the saved game. If
USB flashing is interrupted, the next run resumes the pending migration for
that same ESP32. Rather than trusting the generic ESP32 uploader, the script
writes `app0` explicitly at `0x20000` and reads that exact location back
before it reports success. Backups remain under `backups/`.

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

The current bundle is roughly 17 MiB, but the console hashes installed SD
objects against the signed manifest and skips every byte-identical object.
Consequently a normal firmware-only release transfers only about 1.6 MiB.
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
| NVS | `0x9000` | `0x10000` |
| OTA metadata | `0x19000` | `0x2000` |
| Application A | `0x20000` | `0x1C0000` |
| Application B | `0x1E0000` | `0x1C0000` |
| SPIFFS save | `0x3A0000` | `0x60000` |

The package builder refuses firmware larger than an OTA slot.
