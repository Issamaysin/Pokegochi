package com.pokegochi.updater;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.LinkedHashMap;
import java.util.Map;

final class BleUpdateClient implements AutoCloseable {
    interface Listener {
        void onDevice(BluetoothDevice device, long playerId, int rssi);
        void onConnection(boolean connected, String message);
        void onTransfer(long done, long total, String message);
        void onComplete();
        void onError(String message);
    }

    private enum Step {
        IDLE, AUTH, MANIFEST_BEGIN, MANIFEST_CHUNK, MANIFEST_FINISH,
        OBJECT_BEGIN, DATA, OBJECT_FINISH, COMMIT, COMPLETE
    }

    private final Context context;
    private final Listener listener;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final BluetoothAdapter adapter;
    private final Map<String, Long> playerIds = new LinkedHashMap<>();
    private final Map<String, Integer> advertisedDataBytes = new LinkedHashMap<>();
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic control;
    private BluetoothGattCharacteristic data;
    private BluetoothGattCharacteristic status;
    private int mtu = 23;
    private int maximumDataBytes = UpdateProtocol.LEGACY_MAXIMUM_DATA_BYTES;
    private volatile Step step = Step.IDLE;
    private volatile int expectedStatus = 0;
    private final Runnable operationTimeout = () -> {
        if (expectedStatus != 0)
            fail("The console stopped responding. Reconnect to resume safely.");
    };
    private UpdatePackage updatePackage;
    private int manifestOffset;
    private int payloadIndex;
    private long payloadOffset;
    private InputStream payloadStream;
    private volatile boolean closed;
    // Android may deliver the ESP32 notification before it delivers the
    // onCharacteristicWrite callback for the packet that triggered it.  The
    // protocol legitimately wants to send the next acknowledged block at
    // that point, but Android's GATT layer still reports BUSY.  Keep exactly
    // one next write queued and release it from the write callback.
    private final Object writeLock = new Object();
    private boolean characteristicWriteInFlight;
    private PendingWrite pendingWrite;

    private static final class PendingWrite {
        final BluetoothGattCharacteristic characteristic;
        final byte[] value;

        PendingWrite(BluetoothGattCharacteristic characteristic, byte[] value) {
            this.characteristic = characteristic;
            this.value = value.clone();
        }
    }

    BleUpdateClient(Context context, Listener listener) {
        this.context = context.getApplicationContext();
        this.listener = listener;
        BluetoothManager manager = context.getSystemService(BluetoothManager.class);
        adapter = manager == null ? null : manager.getAdapter();
    }

    boolean bluetoothAvailable() { return adapter != null && adapter.isEnabled(); }

    @SuppressLint("MissingPermission")
    void scan() {
        if (!hasPermissions()) { listener.onError("Bluetooth permission is required"); return; }
        if (!bluetoothAvailable()) { listener.onError("Turn on Android Bluetooth first"); return; }
        stopScan();
        playerIds.clear();
        advertisedDataBytes.clear();
        scanner = adapter.getBluetoothLeScanner();
        ScanSettings settings = new ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build();
        scanner.startScan(null, settings, scanCallback);
        listener.onConnection(false, "Searching for Pokegochi update mode...");
        main.postDelayed(this::stopScan, 10000);
    }

    @SuppressLint("MissingPermission")
    void stopScan() {
        if (scanner != null && hasPermissions()) scanner.stopScan(scanCallback);
        scanner = null;
    }

    @SuppressLint("MissingPermission")
    void connect(BluetoothDevice device) {
        if (!hasPermissions()) { listener.onError("Bluetooth permission is required"); return; }
        stopScan();
        if (gatt != null) gatt.close();
        maximumDataBytes = advertisedDataBytes.getOrDefault(
                device.getAddress(), UpdateProtocol.LEGACY_MAXIMUM_DATA_BYTES);
        listener.onConnection(false, "Connecting...");
        gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE);
    }

    void start(UpdatePackage updatePackage, int pairingCode) {
        if (control == null || data == null || status == null || gatt == null) {
            listener.onError("Connect to the Pokegochi first"); return;
        }
        if (pairingCode < 100000 || pairingCode > 999999) {
            listener.onError("Enter the six-digit code shown on the Pokegochi"); return;
        }
        this.updatePackage = updatePackage;
        manifestOffset = payloadIndex = 0;
        payloadOffset = 0;
        closePayloadStream();
        step = Step.AUTH;
        sendControl(UpdateProtocol.authenticate(pairingCode), UpdateProtocol.STATUS_AUTHENTICATED);
    }

    @SuppressLint("MissingPermission")
    void abort() {
        main.removeCallbacks(operationTimeout);
        if (control != null && gatt != null) {
            step = Step.IDLE;
            expectedStatus = 0;
            write(control, UpdateProtocol.command(UpdateProtocol.ABORT));
        }
        closePayloadStream();
    }

    private final ScanCallback scanCallback = new ScanCallback() {
        @Override public void onScanResult(int callbackType, ScanResult result) {
            UpdateAdvertisement advertisement = parseAdvertisement(result);
            if (advertisement == null || advertisement.playerId == 0) return;
            String address = result.getDevice().getAddress();
            advertisedDataBytes.put(address, advertisement.maximumDataBytes);
            if (playerIds.put(address, advertisement.playerId) == null)
                listener.onDevice(result.getDevice(), advertisement.playerId, result.getRssi());
        }

        @Override public void onScanFailed(int errorCode) {
            listener.onError("Bluetooth scan failed (" + errorCode + ")");
        }
    };

    private final BluetoothGattCallback callback = new BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        @Override public void onConnectionStateChange(BluetoothGatt bluetoothGatt, int statusCode, int newState) {
            if (statusCode != BluetoothGatt.GATT_SUCCESS || newState == BluetoothProfile.STATE_DISCONNECTED) {
                boolean transferInterrupted = step != Step.IDLE && step != Step.COMPLETE;
                step = Step.IDLE;
                expectedStatus = 0;
                synchronized (writeLock) {
                    characteristicWriteInFlight = false;
                    pendingWrite = null;
                }
                main.removeCallbacks(operationTimeout);
                control = data = status = null;
                closePayloadStream();
                listener.onConnection(false, statusCode == BluetoothGatt.GATT_SUCCESS ? "Disconnected" :
                        "Connection failed (" + statusCode + ")");
                if (transferInterrupted)
                    listener.onError("Bluetooth disconnected. Reconnect with the same package to resume.");
                return;
            }
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                listener.onConnection(false, "Connected; negotiating link...");
                // Android explicitly recommends HIGH priority while moving a
                // large amount of LE data. Restore BALANCED after completion.
                bluetoothGatt.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH);
                int requestedMtu = Math.min(517, Math.max(200, maximumDataBytes + 7));
                if (!bluetoothGatt.requestMtu(requestedMtu)) bluetoothGatt.discoverServices();
            }
        }

        @SuppressLint("MissingPermission")
        @Override public void onMtuChanged(BluetoothGatt bluetoothGatt, int negotiated, int statusCode) {
            mtu = statusCode == BluetoothGatt.GATT_SUCCESS ? negotiated : 23;
            bluetoothGatt.discoverServices();
        }

        @SuppressLint("MissingPermission")
        @Override public void onServicesDiscovered(BluetoothGatt bluetoothGatt, int statusCode) {
            BluetoothGattService service = bluetoothGatt.getService(UpdateProtocol.SERVICE);
            if (statusCode != BluetoothGatt.GATT_SUCCESS || service == null) {
                listener.onError("This device is not in Pokegochi update mode"); return;
            }
            control = service.getCharacteristic(UpdateProtocol.CONTROL);
            data = service.getCharacteristic(UpdateProtocol.DATA);
            status = service.getCharacteristic(UpdateProtocol.STATUS);
            BluetoothGattDescriptor cccd = status == null ? null : status.getDescriptor(UpdateProtocol.CCCD);
            if (control == null || data == null || status == null || cccd == null ||
                    !bluetoothGatt.setCharacteristicNotification(status, true)) {
                listener.onError("The update service is incomplete"); return;
            }
            if (Build.VERSION.SDK_INT >= 33)
                bluetoothGatt.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
            else {
                cccd.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                bluetoothGatt.writeDescriptor(cccd);
            }
        }

        @Override public void onDescriptorWrite(BluetoothGatt bluetoothGatt,
                                                BluetoothGattDescriptor descriptor, int statusCode) {
            if (statusCode != BluetoothGatt.GATT_SUCCESS) {
                listener.onError("Could not enable update acknowledgements"); return;
            }
            step = Step.IDLE;
            listener.onConnection(true, "Connected. Enter the code shown on the device.");
            sendUntrackedControl(UpdateProtocol.command(UpdateProtocol.QUERY_STATUS));
        }

        @Override public void onCharacteristicWrite(BluetoothGatt bluetoothGatt,
                BluetoothGattCharacteristic characteristic, int statusCode) {
            PendingWrite next = null;
            synchronized (writeLock) {
                characteristicWriteInFlight = false;
                if (statusCode == BluetoothGatt.GATT_SUCCESS) {
                    next = pendingWrite;
                    pendingWrite = null;
                } else {
                    pendingWrite = null;
                }
            }
            if (statusCode != BluetoothGatt.GATT_SUCCESS) {
                fail("Bluetooth write failed (" + statusCode + ")");
                return;
            }
            if (next != null && !startCharacteristicWrite(next.characteristic, next.value))
                fail("Could not release queued Bluetooth data");
            // The ESP32 sends a StatusPacket only after flash/SD processing.
            // Advancing here would outrun that storage queue, so the state
            // machine deliberately advances from onCharacteristicChanged.
        }

        @Override public void onCharacteristicChanged(BluetoothGatt bluetoothGatt,
                                                       BluetoothGattCharacteristic characteristic) {
            handleStatus(characteristic.getValue());
        }

        @Override public void onCharacteristicChanged(BluetoothGatt bluetoothGatt,
                BluetoothGattCharacteristic characteristic, byte[] value) {
            handleStatus(value);
        }
    };

    @SuppressLint("MissingPermission")
    private void handleStatus(byte[] bytes) {
        if (closed) return;
        UpdateProtocol.Status reply = UpdateProtocol.parseStatus(bytes);
        if (reply == null || reply.version != UpdateProtocol.VERSION) return;
        if (reply.code == UpdateProtocol.STATUS_ERROR) {
            fail("Device rejected the update (error " + reply.error + ")"); return;
        }
        long done = reply.packageDone;
        long total = reply.packageTotal > 0 ? reply.packageTotal :
                (updatePackage == null ? 0 : updatePackage.totalBytes);
        if (total > 0) listener.onTransfer(done, total, transferLabel());
        if (expectedStatus == 0 || reply.code != expectedStatus) return;
        main.removeCallbacks(operationTimeout);
        expectedStatus = 0;
        try {
            switch (step) {
                case AUTH:
                    step = Step.MANIFEST_BEGIN;
                    sendControl(UpdateProtocol.manifestBegin(updatePackage.manifest.length),
                            UpdateProtocol.STATUS_DATA_ACCEPTED);
                    break;
                case MANIFEST_BEGIN:
                case MANIFEST_CHUNK:
                    sendNextManifestChunk();
                    break;
                case MANIFEST_FINISH:
                    payloadIndex = 0;
                    beginCurrentPayload();
                    break;
                case OBJECT_BEGIN:
                    UpdatePackage.Payload payload = updatePackage.payloads.get(payloadIndex);
                    if (reply.objectSize != payload.size || reply.nextOffset > payload.size)
                        throw new IOException("Device resume offset is invalid");
                    payloadOffset = reply.nextOffset;
                    closePayloadStream();
                    // A current firmware can prove that an installed SD file
                    // already matches the signed package and return size as
                    // its resume offset. Do not even traverse that ZIP entry.
                    if (payloadOffset < payload.size)
                        payloadStream = updatePackage.open(payload, payloadOffset);
                    sendNextDataChunk();
                    break;
                case DATA:
                    if (reply.nextOffset != payloadOffset)
                        throw new IOException("Device and phone offsets no longer match");
                    sendNextDataChunk();
                    break;
                case OBJECT_FINISH:
                    closePayloadStream();
                    ++payloadIndex;
                    if (payloadIndex < updatePackage.payloads.size()) beginCurrentPayload();
                    else {
                        step = Step.COMMIT;
                        sendControl(UpdateProtocol.command(UpdateProtocol.COMMIT),
                                UpdateProtocol.STATUS_COMMIT_ACCEPTED);
                    }
                    break;
                case COMMIT:
                    step = Step.COMPLETE;
                    if (gatt != null && hasPermissions())
                        gatt.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_BALANCED);
                    listener.onTransfer(total, total, "Installed. Pokegochi is restarting...");
                    listener.onComplete();
                    break;
                default:
                    break;
            }
        } catch (Exception error) {
            fail(error.getMessage());
        }
    }

    private void sendNextManifestChunk() {
        int maximum = Math.max(1, Math.min(189, mtu - 6));
        if (manifestOffset >= updatePackage.manifest.length) {
            step = Step.MANIFEST_FINISH;
            sendControl(UpdateProtocol.command(UpdateProtocol.MANIFEST_FINISH),
                    UpdateProtocol.STATUS_MANIFEST_ACCEPTED);
            return;
        }
        int count = Math.min(maximum, updatePackage.manifest.length - manifestOffset);
        byte[] packet = UpdateProtocol.manifestChunk(manifestOffset, updatePackage.manifest, count);
        manifestOffset += count;
        step = Step.MANIFEST_CHUNK;
        sendControl(packet, UpdateProtocol.STATUS_DATA_ACCEPTED);
    }

    private void beginCurrentPayload() {
        UpdatePackage.Payload payload = updatePackage.payloads.get(payloadIndex);
        payloadOffset = 0;
        step = Step.OBJECT_BEGIN;
        sendControl(UpdateProtocol.objectBegin(payload.kind, payload.assetId),
                UpdateProtocol.STATUS_OBJECT_READY);
    }

    private void sendNextDataChunk() throws IOException {
        UpdatePackage.Payload payload = updatePackage.payloads.get(payloadIndex);
        if (payloadOffset >= payload.size) {
            step = Step.OBJECT_FINISH;
            sendControl(UpdateProtocol.command(UpdateProtocol.OBJECT_FINISH),
                    UpdateProtocol.STATUS_OBJECT_VERIFIED);
            return;
        }
        int maximum = Math.max(1, Math.min(maximumDataBytes, mtu - 7));
        byte[] chunk = new byte[(int) Math.min(maximum, payload.size - payloadOffset)];
        int offset = 0;
        while (offset < chunk.length) {
            int count = payloadStream.read(chunk, offset, chunk.length - offset);
            if (count < 0) throw new IOException("Package payload ended unexpectedly");
            offset += count;
        }
        byte[] packet = UpdateProtocol.data((int) payloadOffset, chunk, chunk.length);
        payloadOffset += chunk.length;
        step = Step.DATA;
        sendData(packet, UpdateProtocol.STATUS_DATA_ACCEPTED);
    }

    private String transferLabel() {
        if (updatePackage == null || payloadIndex >= updatePackage.payloads.size()) return "Preparing...";
        UpdatePackage.Payload payload = updatePackage.payloads.get(payloadIndex);
        return payload.kind == UpdateProtocol.OBJECT_FIRMWARE ? "Updating firmware..." : "Updating microSD...";
    }

    private void sendControl(byte[] packet, int expected) {
        expectedStatus = expected;
        if (!write(control, packet)) fail("Could not queue a Bluetooth command");
        else armOperationTimeout();
    }

    private void sendData(byte[] packet, int expected) {
        expectedStatus = expected;
        if (!write(data, packet)) fail("Could not queue update data");
        else armOperationTimeout();
    }

    private void armOperationTimeout() {
        main.removeCallbacks(operationTimeout);
        main.postDelayed(operationTimeout, 120000);
    }

    private void sendUntrackedControl(byte[] packet) {
        write(control, packet);
    }

    @SuppressLint("MissingPermission")
    private boolean write(BluetoothGattCharacteristic characteristic, byte[] value) {
        if (gatt == null || characteristic == null || !hasPermissions()) return false;
        synchronized (writeLock) {
            if (characteristicWriteInFlight) {
                // There can be only one protocol response per request, hence
                // at most one legitimate successor while the callback lags.
                if (pendingWrite != null) return false;
                pendingWrite = new PendingWrite(characteristic, value);
                return true;
            }
            return startCharacteristicWriteLocked(characteristic, value);
        }
    }

    @SuppressLint("MissingPermission")
    private boolean startCharacteristicWrite(BluetoothGattCharacteristic characteristic, byte[] value) {
        synchronized (writeLock) {
            if (characteristicWriteInFlight) return false;
            return startCharacteristicWriteLocked(characteristic, value);
        }
    }

    @SuppressLint("MissingPermission")
    private boolean startCharacteristicWriteLocked(BluetoothGattCharacteristic characteristic, byte[] value) {
        if (gatt == null || characteristic == null || !hasPermissions()) return false;
        final boolean accepted;
        if (Build.VERSION.SDK_INT >= 33)
            accepted = gatt.writeCharacteristic(characteristic, value,
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothGatt.GATT_SUCCESS;
        else {
            characteristic.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
            characteristic.setValue(value);
            accepted = gatt.writeCharacteristic(characteristic);
        }
        if (accepted) characteristicWriteInFlight = true;
        return accepted;
    }

    private void fail(String message) {
        main.removeCallbacks(operationTimeout);
        step = Step.IDLE;
        expectedStatus = 0;
        synchronized (writeLock) { pendingWrite = null; }
        closePayloadStream();
        listener.onError(message == null ? "Update failed" : message);
    }

    private void closePayloadStream() {
        if (payloadStream != null) try { payloadStream.close(); } catch (IOException ignored) {}
        payloadStream = null;
    }

    private static final class UpdateAdvertisement {
        final long playerId;
        final int maximumDataBytes;

        UpdateAdvertisement(long playerId, int maximumDataBytes) {
            this.playerId = playerId;
            this.maximumDataBytes = maximumDataBytes;
        }
    }

    private UpdateAdvertisement parseAdvertisement(ScanResult result) {
        if (result.getScanRecord() == null) return null;
        byte[] record = result.getScanRecord().getBytes();
        for (int cursor = 0; cursor < record.length;) {
            int length = record[cursor] & 0xff;
            if (length == 0 || cursor + length >= record.length) break;
            int type = record[cursor + 1] & 0xff;
            if (type == 0xff) {
                int start = cursor + 2;
                int end = cursor + 1 + length;
                for (int index = start; index + 10 <= end; ++index) {
                    if (record[index] == 'P' && record[index + 1] == 'G' &&
                            record[index + 2] == 'O' && record[index + 3] == 'T' &&
                            (record[index + 4] & 0xff) == UpdateProtocol.VERSION &&
                            (record[index + 5] & 0xff) == UpdateProtocol.DEVICE_MODEL) {
                        long playerId = Integer.toUnsignedLong(ByteBuffer.wrap(record, index + 6, 4)
                                .order(ByteOrder.LITTLE_ENDIAN).getInt());
                        int maximum = UpdateProtocol.LEGACY_MAXIMUM_DATA_BYTES;
                        if (index + 12 <= end) {
                            int advertised = ByteBuffer.wrap(record, index + 10, 2)
                                    .order(ByteOrder.LITTLE_ENDIAN).getShort() & 0xffff;
                            if (advertised >= UpdateProtocol.LEGACY_MAXIMUM_DATA_BYTES &&
                                    advertised <= UpdateProtocol.MAXIMUM_DATA_BYTES)
                                maximum = advertised;
                        }
                        return new UpdateAdvertisement(playerId, maximum);
                    }
                }
            }
            cursor += length + 1;
        }
        return null;
    }

    private boolean hasPermissions() {
        if (Build.VERSION.SDK_INT >= 31)
            return context.checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
                    context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
        return context.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
    }

    @SuppressLint("MissingPermission")
    @Override public void close() {
        closed = true;
        main.removeCallbacks(operationTimeout);
        stopScan();
        closePayloadStream();
        if (gatt != null) {
            if (hasPermissions()) gatt.disconnect();
            gatt.close();
            gatt = null;
        }
    }
}
