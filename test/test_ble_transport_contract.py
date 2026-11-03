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
assert "MAX_CENTRAL_ATTEMPTS = 5" in policy
assert "GATT_OPERATION_TIMEOUT_MS = 2_500L" in policy
assert "GATT_SERVER_TO_CENTRAL_SETTLE_MS = 280L" in policy
assert "openCentralGatt" in android
assert "PHY_LE_1M_MASK" in android
assert "adapter.cancelDiscovery()" in android
assert "closeGattServer(false);" in android
assert "restartGattServer();" in android
assert "checkGattOperationTimeouts();" in android
assert "checkPeripheralHandshakeTimeout();" in android
assert "replayHandshakeIfNeeded(state);" in android
assert "finishPeripheralClose();" in android
assert "createScanCallback" in android and "scanRetryAtMs" in android
assert "createAdvertiseCallback" in android and "advertisingGeneration" in android
assert "centralGeneration != generation" in android
assert "remoteRx != null && gatt != null && centralReady" in android
assert "MINIMUM_GATT_MTU = 177" in policy
assert "BLE MTU negotiation callback timed out" in android
assert android.index("closeGattServer(false);") < android.index(
    "openCentralGattAfterServerRelease"
)

assert "gLatestDiscovery" in firmware and "gDiscoveryPending" in firmware
assert "event.kind == TransportEventKind::Discovered" in firmware
assert "xQueueCreate(8, sizeof(TransportEvent))" in firmware
assert "state_ == State::Connecting && header.sourceId == peerId_" in firmware
assert "playerId_ < header.sourceId" in firmware
assert "GATT attempt=%u/2" in firmware

print("BLE transport reliability contract: OK")
