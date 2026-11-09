# Pokegochi Privacy Policy

**Effective date:** November 4, 2026

Pokegochi is an offline-first Android game. This policy explains what data the
Pokegochi Android application handles.

## Data collection

Pokegochi does not collect, sell, rent, or send personal information to a
developer-operated server. The app has no advertising SDK, analytics SDK, or
remote account system.

## Data stored on the device

The game save, settings, progress, and local preferences are stored on the
device in the app's private storage. They remain there until the user clears
the app data or uninstalls the app.

When the user selects a `.pgsav` save file for import, the file is processed
locally. The imported save and its local backup are not uploaded to Pokegochi
or to a third-party service.

## Bluetooth multiplayer

Bluetooth multiplayer uses Bluetooth Low Energy only when the user opens and
uses that multiplayer page. Nearby devices exchange the gameplay/session data
needed for that connection directly.

On Android versions that require it for Bluetooth scanning, the operating
system may request location permission. Pokegochi uses that permission only to
perform Bluetooth discovery and does not collect, store, or transmit the
device's physical location.

## Wi-Fi multiplayer

Wi-Fi multiplayer is optional and uses the Android device's already active
network connection. The app does not scan for Wi-Fi networks, ask for a Wi-Fi
password, change the selected network, or request a Wi-Fi/location permission.
It discovers the Pokegochi server automatically.

The server receives a random five-digit player ID and relays only the temporary
packets needed for player discovery and a battle. It does not receive the game
save, account information, physical location, Wi-Fi password, or device
identifiers. The bundled home ESP32 server keeps clients and battle rooms only
in memory and clears them when disconnected or restarted.

The connection uses ordinary TCP and is not end-to-end encrypted. Users should
connect only to a network and server they trust.

## Diagnostics

The public release build does not create or export diagnostic dump files. A
development/debug build may create local diagnostic files when enabled for
testing; those files stay on the device unless the user explicitly shares
them.

## Third-party services

The release app does not use third-party advertising, analytics, crash
reporting, or cloud-storage services.

## Children's privacy

Pokegochi does not knowingly collect personal information from children or
from any other user.

## Changes and contact

This policy may be updated together with a future app release. Questions or
requests can be submitted through the project's public repository:

https://github.com/Issamaysin/Pokegochi/issues

