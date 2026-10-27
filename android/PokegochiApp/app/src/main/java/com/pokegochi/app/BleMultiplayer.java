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
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;

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
    private boolean serverReady;
    private boolean advertising;
    private boolean running;
    private boolean active;
    private boolean centralConnecting;
    private boolean centralReady;
    private boolean centralDiscoveryStarted;
    private boolean writeInFlight;
    private boolean lastFirmwareEnabled;
    private int lastFirmwareState = -1;
    private boolean lastPermissionsMissing;
    private long idleLinkSinceMs;

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
                releaseStalePhysicalSession(state);
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
        startAdvertisingIfReady();
        status("BLE transport ready for player " + firmware.multiplayerPlayerId() + ".");
    }

    @SuppressLint("MissingPermission")
    private void startAdvertisingIfReady() {
        if (!active || !serverReady || advertising || !hasPeripheralPermissions()) return;
        if (advertiser == null) advertiser = adapter.getBluetoothLeAdvertiser();
        if (advertiser == null) {
            status("BLE advertiser unavailable.");
            return;
        }
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
        advertising = true;
        advertiser.startAdvertising(settings, data, advertiseCallback);
    }

    @SuppressLint("MissingPermission")
    private void startScan() {
        if (scanner != null || !hasClientPermissions() || !supported()) return;
        scanner = adapter.getBluetoothLeScanner();
        if (scanner == null) return;
        ScanSettings settings = new ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .setReportDelay(0L)
                .build();
        scanner.startScan(null, settings, scanCallback);
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
        centralDiscoveryStarted = false;
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
        serverReady = false;
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
        if (!server.addService(service)) {
            status("BLE GATT service registration failed.");
            server.close();
            server = null;
            return;
        }
        status("BLE GATT service registration pending.");
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
        advertising = false;
        if (gatt != null) {
            gatt.disconnect();
            gatt.close();
            gatt = null;
        }
        if (server != null) {
            server.close();
            server = null;
        }
        serverReady = false;
        remoteRx = null;
        serverTx = null;
        connectedDevice = null;
        centralConnecting = false;
        centralReady = false;
        centralDiscoveryStarted = false;
        writeInFlight = false;
        pendingWrites.clear();
        idleLinkSinceMs = 0L;
        active = false;
    }

    @SuppressLint("MissingPermission")
    private void releaseStalePhysicalSession(int firmwareState) {
        final boolean idleProtocol = firmwareState == STATE_ADVERTISING ||
                firmwareState == STATE_SCANNING;
        final boolean physicalLinkPresent = gatt != null || connectedDevice != null ||
                centralConnecting || centralReady;
        if (!idleProtocol || !physicalLinkPresent) {
            idleLinkSinceMs = 0L;
            return;
        }

        // Give a freshly connected peripheral enough time to deliver Invite,
        // and give an outbound SessionClose enough time to leave the write
        // queue.  A real session moves out of ADVERTISING immediately; a stale
        // GATT link remains idle and is released on the next polls.
        final long now = SystemClock.elapsedRealtime();
        if (idleLinkSinceMs == 0L) {
            idleLinkSinceMs = now;
            return;
        }
        if (now - idleLinkSinceMs < 500L) return;

        status("BLE releasing stale physical session.");
        BluetoothGatt oldGatt = gatt;
        BluetoothDevice oldDevice = connectedDevice;
        gatt = null;
        connectedDevice = null;
        remoteRx = null;
        centralConnecting = false;
        centralReady = false;
        centralDiscoveryStarted = false;
        writeInFlight = false;
        pendingWrites.clear();
        idleLinkSinceMs = 0L;
        if (oldGatt != null) {
            oldGatt.disconnect();
            oldGatt.close();
        }
        if (server != null && oldDevice != null) server.cancelConnection(oldDevice);
        firmware.multiplayerDisconnected();
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
            BleMultiplayer.this.status("BLE central state=" + newState + " status=" + status);
            if (newState == BluetoothProfile.STATE_CONNECTED && status == BluetoothGatt.GATT_SUCCESS) {
                centralConnecting = true;
                centralDiscoveryStarted = false;
                // Negotiate enough room for complete roster/battle packets
                // before discovering the service exposed by the ESP32.
                if (Build.VERSION.SDK_INT >= 21 && g.requestMtu(185)) {
                    handler.postDelayed(() -> discoverServicesOnce(g), 1000L);
                } else {
                    discoverServicesOnce(g);
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                failCentral(g, "BLE central disconnected status=" + status);
            } else if (newState != BluetoothProfile.STATE_CONNECTED) {
                failCentral(g, "BLE central connection failed status=" + status);
            }
        }

        @Override public void onMtuChanged(BluetoothGatt g, int mtu, int status) {
            BleMultiplayer.this.status("BLE MTU=" + mtu + " status=" + status);
            discoverServicesOnce(g);
        }

        @SuppressLint("MissingPermission")
        @Override public void onServicesDiscovered(BluetoothGatt g, int status) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                failCentral(g, "BLE service discovery failed status=" + status);
                return;
            }
            BluetoothGattService service = g.getService(SERVICE);
            if (service == null) {
                failCentral(g, "BLE multiplayer service missing on peer");
                return;
            }
            remoteRx = service.getCharacteristic(RX);
            BluetoothGattCharacteristic tx = service.getCharacteristic(TX);
            if (remoteRx == null || tx == null) {
                failCentral(g, "BLE multiplayer characteristics missing on peer");
                return;
            }
            if (!g.setCharacteristicNotification(tx, true)) {
                failCentral(g, "BLE notification registration failed");
                return;
            }
            BluetoothGattDescriptor descriptor = tx.getDescriptor(CCCD);
            if (descriptor != null) {
                descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                if (!g.writeDescriptor(descriptor))
                    failCentral(g, "BLE CCCD write could not start");
            } else beginCentralSession();
        }

        @Override public void onDescriptorWrite(BluetoothGatt g, BluetoothGattDescriptor descriptor, int status) {
            if (descriptor.getUuid().equals(CCCD) && status == BluetoothGatt.GATT_SUCCESS)
                beginCentralSession();
            else if (descriptor.getUuid().equals(CCCD))
                failCentral(g, "BLE CCCD write failed status=" + status);
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

    @SuppressLint("MissingPermission")
    private void discoverServicesOnce(BluetoothGatt connection) {
        if (connection == null || gatt != connection || centralDiscoveryStarted) return;
        centralDiscoveryStarted = true;
        if (!connection.discoverServices())
            failCentral(connection, "BLE service discovery could not start");
    }

    @SuppressLint("MissingPermission")
    private void failCentral(BluetoothGatt connection, String reason) {
        status(reason);
        if (gatt == connection) {
            centralConnecting = false;
            centralReady = false;
            centralDiscoveryStarted = false;
            remoteRx = null;
            gatt = null;
            writeInFlight = false;
            pendingWrites.clear();
            connection.disconnect();
            connection.close();
            firmware.multiplayerDisconnected();
        }
    }

    private void beginCentralSession() {
        if (centralReady) return;
        centralReady = true;
        firmware.multiplayerTransportConnected(true);
        drainFirmwareOutbound();
    }

    private final BluetoothGattServerCallback serverCallback = new BluetoothGattServerCallback() {
        @Override public void onServiceAdded(int status, BluetoothGattService service) {
            if (server == null || !active || service == null || !SERVICE.equals(service.getUuid())) return;
            if (status != BluetoothGatt.GATT_SUCCESS) {
                BleMultiplayer.this.status("BLE GATT service registration failed status=" + status);
                return;
            }
            serverReady = true;
            BleMultiplayer.this.status("BLE GATT service ready.");
            startAdvertisingIfReady();
        }

        @Override public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                if (!serverReady) {
                    BleMultiplayer.this.status("BLE client connected before GATT service was ready.");
                    return;
                }
                connectedDevice = device;
                idleLinkSinceMs = 0L;
                firmware.multiplayerTransportConnected(false);
                drainFirmwareOutbound();
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED && device.equals(connectedDevice)) {
                connectedDevice = null;
                writeInFlight = false;
                pendingWrites.clear();
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
            advertising = true;
            status("BLE advertising started manufacturer=0x" +
                    Integer.toHexString(MultiplayerProtocol.MANUFACTURER_ID));
        }

        @Override public void onStartFailure(int errorCode) {
            advertising = false;
            status("BLE advertising failed: " + errorCode);
            if (active && handler != null)
                handler.postDelayed(BleMultiplayer.this::startAdvertisingIfReady, 750L);
        }
    };
}
