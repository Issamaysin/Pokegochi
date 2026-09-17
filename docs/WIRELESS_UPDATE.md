# Wireless firmware + microSD update

Pokegochi can update both OTA application firmware and the versioned microSD
asset pack through Bluetooth Low Energy. The persistent game save lives in
internal SPIFFS and is not present in the update bundle.

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
the migrated save back, and verifies it byte-for-byte. If USB flashing is
interrupted, the next run resumes the pending migration for that same ESP32.
Backups remain under `backups/`.

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

## Partition layout

| Partition | Offset | Size |
|---|---:|---:|
| NVS | `0x9000` | `0x10000` |
| OTA metadata | `0x19000` | `0x2000` |
| Application A | `0x20000` | `0x1A0000` |
| Application B | `0x1C0000` | `0x1A0000` |
| SPIFFS save | `0x360000` | `0xA0000` |

The package builder refuses firmware larger than an OTA slot.
