package com.pokegochi.app;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;

import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;

/**
 * Android BLE transport for the actual firmware Multiplayer instance.
 *
 * This class deliberately has no packet parser or game actions.  It only
 * advertises, scans, connects and moves raw packets between Android BLE and
 * the Multiplayer object running inside src/main.cpp.
 */
final class BleMultiplayer implements AutoCloseable {
    interface Listener {
        void onStatus(String status);

        default void onPermissionsRequired() {}
    }

    private static final int STATE_ADVERTISING = 1;
    private static final int STATE_SCANNING = 2;
    private static final int STATE_CONNECTING = 4;

    private static final UUID SERVICE = UUID.fromString(MultiplayerProtocol.SERVICE_UUID);
    private static final UUID RX = UUID.fromString(MultiplayerProtocol.RX_UUID);
    private static final UUID TX = UUID.fromString(MultiplayerProtocol.TX_UUID);
    private static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    private final Context context;
    private final BluetoothAdapter adapter;
    private final NativeFirmware firmware;
    private final Listener listener;
    private final HandlerThread transportThread = new HandlerThread("PokegochiBLE");
    private final Map<Integer, BluetoothDevice> peers = new HashMap<>();
    private final ArrayDeque<byte[]> pendingWrites = new ArrayDeque<>();

    private Handler handler;
    private BluetoothLeScanner scanner;
    private BluetoothLeAdvertiser advertiser;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic remoteRx;
    private BluetoothGattServer server;
    private BluetoothGattCharacteristic serverTx;
    private BluetoothDevice connectedDevice;
    private boolean running;
    private boolean active;
    private boolean centralConnecting;
    private boolean centralReady;
    private boolean writeInFlight;
    private boolean lastFirmwareEnabled;
    private int lastFirmwareState = -1;
    private boolean lastPermissionsMissing;

    BleMultiplayer(Context context, NativeFirmware firmware, Listener listener) {
        this.context = context.getApplicationContext();
        this.firmware = firmware;
        this.listener = listener;
        BluetoothManager manager = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        adapter = manager == null ? null : manager.getAdapter();
    }

    void start() {
        if (running) return;
        running = true;
        transportThread.start();
        handler = new Handler(transportThread.getLooper());
        handler.post(poll);
    }

    private final Runnable poll = new Runnable() {
        @Override public void run() {
            if (!running) return;
            boolean enabled = firmware.multiplayerEnabled();
            int firmwareState = firmware.multiplayerState();
            if (enabled != lastFirmwareEnabled || firmwareState != lastFirmwareState) {
                status("firmware multiplayer enabled=" + enabled + " state=" + firmwareState);
                lastFirmwareEnabled = enabled;
                lastFirmwareState = firmwareState;
            }
            final boolean permissionsMissing = enabled && adapter != null &&
                    (!hasClientPermissions() || !hasPeripheralPermissions());
            if (permissionsMissing && !lastPermissionsMissing && listener != null)
                listener.onPermissionsRequired();
            lastPermissionsMissing = permissionsMissing;
            if (enabled) ensureActive();
            else if (active) stopTransport();

            if (active) {
                drainFirmwareOutbound();
                int state = firmware.multiplayerState();
                if (state == STATE_SCANNING) startScan();
                else stopScan();
                if (state == STATE_CONNECTING) connectSelectedPeer();
            }
            handler.postDelayed(this, 40L);
        }
    };

    @SuppressLint("MissingPermission")
    private void ensureActive() {
        if (active) return;
        if (!hasClientPermissions() || !supported()) return;
        active = true;
        startServer();
        advertiser = adapter.getBluetoothLeAdvertiser();
        if (advertiser != null && hasPeripheralPermissions()) {
            AdvertiseSettings settings = new AdvertiseSettings.Builder()
                    .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
                    .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM)
                    .setConnectable(true).build();
            AdvertiseData data = new AdvertiseData.Builder()
                    .setIncludeDeviceName(false)
                    .addManufacturerData(MultiplayerProtocol.MANUFACTURER_ID,
                            MultiplayerProtocol.androidManufacturerPayload(
                                    firmware.multiplayerPlayerId()))
                    .build();
            advertiser.startAdvertising(settings, data, advertiseCallback);
        }
        status("BLE transport ready for player " + firmware.multiplayerPlayerId() + ".");
    }

    @SuppressLint("MissingPermission")
    private void startScan() {
        if (scanner != null || !hasClientPermissions() || !supported()) return;
        scanner = adapter.getBluetoothLeScanner();
        if (scanner == null) return;
        scanner.startScan(scanCallback);
    }

    @SuppressLint("MissingPermission")
    private void stopScan() {
        if (scanner != null) {
            scanner.stopScan(scanCallback);
            scanner = null;
        }
    }

    @SuppressLint("MissingPermission")
    private void connectSelectedPeer() {
        if (centralConnecting || gatt != null || connectedDevice != null) return;
        int playerId = firmware.multiplayerPeerId();
        BluetoothDevice device = peers.get(playerId);
        if (device == null) return;
        stopScan();
        centralConnecting = true;
        gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE);
        if (gatt == null) {
            centralConnecting = false;
            firmware.multiplayerDisconnected();
        }
    }

    @SuppressLint("MissingPermission")
    private void startServer() {
        if (server != null || !hasPeripheralPermissions()) return;
        BluetoothManager manager = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        if (manager == null) return;
        server = manager.openGattServer(context, serverCallback);
        if (server == null) return;
        BluetoothGattService service = new BluetoothGattService(
                SERVICE, BluetoothGattService.SERVICE_TYPE_PRIMARY);
        BluetoothGattCharacteristic rx = new BluetoothGattCharacteristic(RX,
                BluetoothGattCharacteristic.PROPERTY_WRITE |
                        BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE,
                BluetoothGattCharacteristic.PERMISSION_WRITE);
        serverTx = new BluetoothGattCharacteristic(TX,
                BluetoothGattCharacteristic.PROPERTY_NOTIFY,
                BluetoothGattCharacteristic.PERMISSION_READ);
        serverTx.addDescriptor(new BluetoothGattDescriptor(CCCD,
                BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE));
        service.addCharacteristic(rx);
        service.addCharacteristic(serverTx);
        server.addService(service);
    }

    private boolean supported() {
        return adapter != null && adapter.isEnabled() && context.getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_BLUETOOTH_LE);
    }

    private boolean hasClientPermissions() {
        if (Build.VERSION.SDK_INT < 31)
            return context.checkSelfPermission(Manifest.permission.BLUETOOTH) == PackageManager.PERMISSION_GRANTED &&
                    (Build.VERSION.SDK_INT < 23 || context.checkSelfPermission(
                            Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED);
        return context.checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
                context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
    }

    private boolean hasPeripheralPermissions() {
        if (Build.VERSION.SDK_INT < 31)
            return context.checkSelfPermission(Manifest.permission.BLUETOOTH) == PackageManager.PERMISSION_GRANTED;
        return context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED &&
                context.checkSelfPermission(Manifest.permission.BLUETOOTH_ADVERTISE) == PackageManager.PERMISSION_GRANTED;
    }

    @SuppressLint("MissingPermission")
    private void stopTransport() {
        stopScan();
        if (advertiser != null) {
            advertiser.stopAdvertising(advertiseCallback);
            advertiser = null;
        }
        if (gatt != null) {
            gatt.disconnect();
            gatt.close();
            gatt = null;
        }
        if (server != null) {
            server.close();
            server = null;
        }
        remoteRx = null;
        serverTx = null;
        connectedDevice = null;
        centralConnecting = false;
        centralReady = false;
        writeInFlight = false;
        pendingWrites.clear();
        active = false;
    }

    private void drainFirmwareOutbound() {
        byte[] packet;
        while ((packet = firmware.multiplayerTakeOutgoing()) != null) enqueueTransportWrite(packet);
    }

    @SuppressLint("MissingPermission")
    private boolean enqueueTransportWrite(byte[] bytes) {
        if (bytes == null || bytes.length == 0) return false;
        pendingWrites.add(bytes);
        flushWrites();
        return true;
    }

    @SuppressLint("MissingPermission")
    private void flushWrites() {
        if (remoteRx != null && gatt != null) {
            if (writeInFlight || pendingWrites.isEmpty()) return;
            byte[] bytes = pendingWrites.peek();
            boolean started;
            if (Build.VERSION.SDK_INT >= 33) {
                started = gatt.writeCharacteristic(remoteRx, bytes,
                        BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothGatt.GATT_SUCCESS;
            } else {
                remoteRx.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                remoteRx.setValue(bytes);
                started = gatt.writeCharacteristic(remoteRx);
            }
            if (started) writeInFlight = true;
            else pendingWrites.poll();
            return;
        }
        if (server != null && connectedDevice != null && serverTx != null) {
            while (!pendingWrites.isEmpty()) {
                byte[] bytes = pendingWrites.peek();
                int result;
                if (Build.VERSION.SDK_INT >= 33) {
                    result = server.notifyCharacteristicChanged(connectedDevice, serverTx, false, bytes);
                } else {
                    serverTx.setValue(bytes);
                    result = server.notifyCharacteristicChanged(connectedDevice, serverTx, false)
                            ? BluetoothGatt.GATT_SUCCESS : BluetoothGatt.GATT_FAILURE;
                }
                if (result != BluetoothGatt.GATT_SUCCESS) break;
                pendingWrites.poll();
            }
        }
    }

    private void handlePacket(byte[] bytes) {
        if (bytes == null || bytes.length == 0) return;
        firmware.multiplayerReceive(bytes);
        drainFirmwareOutbound();
    }

    private void status(String value) {
        if (listener != null) listener.onStatus(value);
    }

    @SuppressLint("MissingPermission")
    @Override public void close() {
        running = false;
        if (handler != null) handler.removeCallbacks(poll);
        stopTransport();
        transportThread.quitSafely();
    }

    private final ScanCallback scanCallback = new ScanCallback() {
        @Override public void onScanResult(int callbackType, ScanResult result) {
            MultiplayerProtocol.Advertisement advertisement = null;
            int manufacturerId = -1;
            if (result.getScanRecord() != null) {
                android.util.SparseArray<byte[]> records = result.getScanRecord().getManufacturerSpecificData();
                for (int i = 0; i < records.size(); i++) {
                    byte[] candidate = records.valueAt(i);
                    MultiplayerProtocol.Advertisement parsed =
                            MultiplayerProtocol.parseManufacturerAdvertisement(
                                    records.keyAt(i), candidate);
                    if (parsed != null) {
                        advertisement = parsed;
                        manufacturerId = records.keyAt(i);
                        break;
                    }
                }
            }
            if (advertisement == null || advertisement.playerId == firmware.multiplayerPlayerId()) return;
            peers.put(advertisement.playerId, result.getDevice());
            firmware.multiplayerDiscovered(advertisement.playerId, result.getRssi(),
                    result.getDevice().getAddress(), 0);
            status("BLE discovered player " + advertisement.playerId + " rssi=" +
                    result.getRssi() + " manufacturer=0x" + Integer.toHexString(manufacturerId));
        }

        @Override public void onScanFailed(int errorCode) {
            status("BLE scan failed: " + errorCode);
        }
    };

    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        @Override public void onConnectionStateChange(BluetoothGatt g, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                g.discoverServices();
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                centralConnecting = false;
                firmware.multiplayerDisconnected();
                if (gatt == g) {
                    g.close();
                    gatt = null;
                }
                remoteRx = null;
                centralReady = false;
            }
        }

        @SuppressLint("MissingPermission")
        @Override public void onServicesDiscovered(BluetoothGatt g, int status) {
            if (status != BluetoothGatt.GATT_SUCCESS) return;
            BluetoothGattService service = g.getService(SERVICE);
            if (service == null) return;
            remoteRx = service.getCharacteristic(RX);
            BluetoothGattCharacteristic tx = service.getCharacteristic(TX);
            if (remoteRx == null || tx == null) return;
            g.setCharacteristicNotification(tx, true);
            BluetoothGattDescriptor descriptor = tx.getDescriptor(CCCD);
            if (descriptor != null) {
                descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                g.writeDescriptor(descriptor);
            } else beginCentralSession();
        }

        @Override public void onDescriptorWrite(BluetoothGatt g, BluetoothGattDescriptor descriptor, int status) {
            if (descriptor.getUuid().equals(CCCD) && status == BluetoothGatt.GATT_SUCCESS)
                beginCentralSession();
        }

        @Override public void onCharacteristicWrite(BluetoothGatt g,
                                                     BluetoothGattCharacteristic characteristic,
                                                     int status) {
            writeInFlight = false;
            if (!pendingWrites.isEmpty()) pendingWrites.poll();
            flushWrites();
        }

        @Override public void onCharacteristicChanged(BluetoothGatt g,
                                                       BluetoothGattCharacteristic characteristic,
                                                       byte[] value) {
            handlePacket(value);
        }

        @Override public void onCharacteristicChanged(BluetoothGatt g,
                                                       BluetoothGattCharacteristic characteristic) {
            handlePacket(characteristic.getValue());
        }
    };

    private void beginCentralSession() {
        if (centralReady) return;
        centralReady = true;
        firmware.multiplayerTransportConnected(true);
        drainFirmwareOutbound();
    }

    private final BluetoothGattServerCallback serverCallback = new BluetoothGattServerCallback() {
        @Override public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                connectedDevice = device;
                firmware.multiplayerTransportConnected(false);
                drainFirmwareOutbound();
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED && device.equals(connectedDevice)) {
                connectedDevice = null;
                firmware.multiplayerDisconnected();
            }
        }

        @Override public void onCharacteristicWriteRequest(BluetoothDevice device, int requestId,
                BluetoothGattCharacteristic characteristic, boolean preparedWrite,
                boolean responseNeeded, int offset, byte[] value) {
            if (responseNeeded && server != null)
                server.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, null);
            if (characteristic.getUuid().equals(RX) && offset == 0) handlePacket(value);
        }

        @Override public void onDescriptorWriteRequest(BluetoothDevice device, int requestId,
                BluetoothGattDescriptor descriptor, boolean preparedWrite, boolean responseNeeded,
                int offset, byte[] value) {
            if (responseNeeded && server != null)
                server.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, null);
        }
    };

    private final AdvertiseCallback advertiseCallback = new AdvertiseCallback() {
        @Override public void onStartSuccess(AdvertiseSettings settingsInEffect) {
            status("BLE advertising started manufacturer=0x" +
                    Integer.toHexString(MultiplayerProtocol.MANUFACTURER_ID));
        }

        @Override public void onStartFailure(int errorCode) {
            status("BLE advertising failed: " + errorCode);
        }
    };
}
