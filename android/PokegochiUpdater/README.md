# Pokegochi Updater for Android

Native BLE client for signed Pokegochi `.pgota` packages and portable `.pgsav`
save backups. It validates all payload lengths and SHA-256 hashes locally,
discovers only Pokegochi update advertisements, authenticates with the
six-digit console code, and resumes SD payloads after a disconnect. After
authentication, a separate tools dashboard offers OTAP, clock sync, **Load
file** (Pokegochi to Android), **Save file** (Android to Pokegochi), and an
explicitly confirmed **Reset file** action.

`.pgsav` files contain the source player ID, save ABI version, header checksum,
and payload checksum. Android validates the container before a restore begins;
the console validates it again and commits through the normal redundant save
store only after the entire transfer succeeds. Interrupted or invalid imports
leave the previous durable save active.

Build from the repository root with:

```powershell
.\scripts\build_android_updater.ps1
```

The debug APK is copied to `dist/PokegochiUpdater.apk`. See
`docs/WIRELESS_UPDATE.md` for the complete console and signing workflow.
