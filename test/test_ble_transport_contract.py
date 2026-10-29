from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ANDROID = ROOT / "android/PokegochiApp/app/src/main/java/com/pokegochi/app/BleMultiplayer.java"
POLICY = ROOT / "android/PokegochiApp/app/src/main/java/com/pokegochi/app/BleTransportPolicy.java"
FIRMWARE = ROOT / "src/services/Multiplayer.cpp"


android = ANDROID.read_text(encoding="utf-8")
policy = POLICY.read_text(encoding="utf-8")
firmware = FIRMWARE.read_text(encoding="utf-8")

central_ready = android.split("private void beginCentralSession()", 1)[1].split(
    "private final BluetoothGattServerCallback", 1
)[0]
assert "drainFirmwareOutbound();" in central_ready
assert "flushWrites();" in central_ready
assert central_ready.index("drainFirmwareOutbound();") < central_ready.index("flushWrites();")

assert "onNotificationSent" in android
assert "serverNotifyInFlight" in android
assert "onDescriptorWriteRequest" in android and "serverNotificationsEnabled" in android
assert "scheduleCentralRetry" in android
assert "INBOUND_HANDSHAKE_TIMEOUT_MS = 12_000L" in policy
assert "MAX_CENTRAL_ATTEMPTS = 3" in policy
assert "CENTRAL_ROLE_SETTLE_MS = 320L" in policy
assert "openCentralGatt" in android
assert "PHY_LE_1M_MASK" in android
assert "adapter.cancelDiscovery()" in android

assert "gLatestDiscovery" in firmware and "gDiscoveryPending" in firmware
assert "event.kind == TransportEventKind::Discovered" in firmware
assert "xQueueCreate(8, sizeof(TransportEvent))" in firmware
assert "state_ == State::Connecting && header.sourceId == peerId_" in firmware
assert "playerId_ < header.sourceId" in firmware
assert "GATT attempt=%u/2" in firmware

print("BLE transport reliability contract: OK")
