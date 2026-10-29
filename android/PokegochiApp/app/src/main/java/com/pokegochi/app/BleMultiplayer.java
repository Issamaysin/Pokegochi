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

    private static final int STATE_ADVERTISING = BleTransportPolicy.STATE_ADVERTISING;
    private static final int STATE_SCANNING = BleTransportPolicy.STATE_SCANNING;
    private static final int STATE_CONNECTING = BleTransportPolicy.STATE_CONNECTING;

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
    private boolean centralConnectScheduled;
    private boolean centralReady;
    private boolean centralDiscoveryStarted;
    private boolean writeInFlight;
    private boolean serverNotifyInFlight;
    private boolean serverNotificationsEnabled;
    private boolean protocolSessionObserved;
    private boolean lastFirmwareEnabled;
    private int lastFirmwareState = -1;
    private boolean lastPermissionsMissing;
    private long idleLinkSinceMs;
    private long centralRetryAtMs;
    private int centralPeerId;
    private int centralAttempts;
    private int centralWriteAttempts;
    private int serverNotifyAttempts;

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
                if (BleTransportPolicy.hasObservedProtocolSession(state))
                    protocolSessionObserved = true;
                releaseStalePhysicalSession(state);
                if (BleTransportPolicy.isIdleProtocolState(state) && !physicalLinkPresent())
                    startAdvertisingIfReady();
                if (state == STATE_SCANNING && !physicalLinkPresent()) startScan();
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
        if (!active || !serverReady || advertising || physicalLinkPresent() ||
                !hasPeripheralPermissions()) return;
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
    private void stopAdvertising() {
        if (advertiser != null && advertising) advertiser.stopAdvertising(advertiseCallback);
        advertising = false;
    }

    private boolean physicalLinkPresent() {
        return gatt != null || connectedDevice != null || centralConnecting || centralReady;
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
        if (centralConnecting || centralConnectScheduled || gatt != null ||
                connectedDevice != null) return;
        int playerId = firmware.multiplayerPeerId();
        BluetoothDevice device = peers.get(playerId);
        if (device == null) {
            status("BLE selected player is no longer in the scan cache.");
            firmware.multiplayerDisconnected();
            return;
        }
        if (centralPeerId != playerId) {
            centralPeerId = playerId;
            centralAttempts = 0;
            centralRetryAtMs = 0L;
        }
        long now = SystemClock.elapsedRealtime();
        if (now < centralRetryAtMs ||
                centralAttempts >= BleTransportPolicy.MAX_CENTRAL_ATTEMPTS) return;
        stopScan();
        stopAdvertising();
        centralConnecting = true;
        centralConnectScheduled = true;
        centralDiscoveryStarted = false;
        centralReady = false;
        centralWriteAttempts = 0;
        ++centralAttempts;
        status("BLE central attempt " + centralAttempts + "/" +
                BleTransportPolicy.MAX_CENTRAL_ATTEMPTS + " player=" + playerId +
                "; settling radio roles");
        final int scheduledAttempt = centralAttempts;
        handler.postDelayed(() -> openCentralGatt(device, playerId, scheduledAttempt),
                BleTransportPolicy.CENTRAL_ROLE_SETTLE_MS);
    }

    @SuppressLint("MissingPermission")
    private void openCentralGatt(BluetoothDevice device, int playerId, int scheduledAttempt) {
        centralConnectScheduled = false;
        if (!active || firmware.multiplayerState() != STATE_CONNECTING ||
                centralPeerId != playerId || centralAttempts != scheduledAttempt ||
                gatt != null || connectedDevice != null) {
            centralConnecting = false;
            return;
        }
        // Classic discovery can be active independently of BluetoothLeScanner
        // and is a documented source of GATT connection failures.
        if (adapter != null && adapter.isDiscovering()) adapter.cancelDiscovery();
        if (Build.VERSION.SDK_INT >= 26) {
            gatt = device.connectGatt(context, false, gattCallback,
                    BluetoothDevice.TRANSPORT_LE, BluetoothDevice.PHY_LE_1M_MASK, handler);
        } else {
            gatt = device.connectGatt(context, false, gattCallback,
                    BluetoothDevice.TRANSPORT_LE);
        }
        if (gatt == null) {
            centralConnecting = false;
            scheduleCentralRetry("BLE connectGatt returned null");
            return;
        }
        BluetoothGatt attempt = gatt;
        handler.postDelayed(() -> {
            if (gatt == attempt && !centralReady)
                failCentral(attempt, "BLE central connection timed out");
        }, BleTransportPolicy.CENTRAL_CONNECT_TIMEOUT_MS);
    }

    private boolean scheduleCentralRetry(String reason) {
        int firmwareState = firmware.multiplayerState();
        if (BleTransportPolicy.canRetryCentral(firmwareState, centralAttempts)) {
            long delay = BleTransportPolicy.centralRetryDelayMs(centralAttempts);
            centralRetryAtMs = SystemClock.elapsedRealtime() + delay;
            status(reason + "; retrying in " + delay + "ms");
            return true;
        }
        status(reason + "; connection attempts exhausted");
        centralPeerId = 0;
        centralAttempts = 0;
        centralRetryAtMs = 0L;
        pendingWrites.clear();
        firmware.multiplayerDisconnected();
        startAdvertisingIfReady();
        return false;
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
        stopAdvertising();
        advertiser = null;
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
        centralConnectScheduled = false;
        centralReady = false;
        centralDiscoveryStarted = false;
        writeInFlight = false;
        serverNotifyInFlight = false;
        serverNotificationsEnabled = false;
        protocolSessionObserved = false;
        centralPeerId = 0;
        centralAttempts = 0;
        centralWriteAttempts = 0;
        serverNotifyAttempts = 0;
        centralRetryAtMs = 0L;
        pendingWrites.clear();
        idleLinkSinceMs = 0L;
        active = false;
    }

    @SuppressLint("MissingPermission")
    private void releaseStalePhysicalSession(int firmwareState) {
        final boolean idleProtocol = BleTransportPolicy.isIdleProtocolState(firmwareState);
        if (!idleProtocol || !physicalLinkPresent()) {
            idleLinkSinceMs = 0L;
            return;
        }

        // Service discovery + CCCD on an ESP32 commonly takes more than the
        // old 500 ms grace. A new inbound link gets a complete handshake
        // window; a link that reached the protocol is released quickly after
        // SessionClose or a declined invitation.
        final long now = SystemClock.elapsedRealtime();
        if (idleLinkSinceMs == 0L) {
            idleLinkSinceMs = now;
            return;
        }
        if (now - idleLinkSinceMs <
                BleTransportPolicy.staleLinkGraceMs(protocolSessionObserved)) return;

        status("BLE releasing " + (protocolSessionObserved ? "closed" : "unanswered") +
                " physical session.");
        BluetoothGatt oldGatt = gatt;
        BluetoothDevice oldDevice = connectedDevice;
        gatt = null;
        connectedDevice = null;
        remoteRx = null;
        centralConnecting = false;
        centralConnectScheduled = false;
        centralReady = false;
        centralDiscoveryStarted = false;
        writeInFlight = false;
        serverNotifyInFlight = false;
        serverNotificationsEnabled = false;
        protocolSessionObserved = false;
        centralPeerId = 0;
        centralAttempts = 0;
        centralWriteAttempts = 0;
        serverNotifyAttempts = 0;
        centralRetryAtMs = 0L;
        pendingWrites.clear();
        idleLinkSinceMs = 0L;
        if (oldGatt != null) {
            oldGatt.disconnect();
            oldGatt.close();
        }
        if (server != null && oldDevice != null) server.cancelConnection(oldDevice);
        firmware.multiplayerDisconnected();
        startAdvertisingIfReady();
    }

    private void drainFirmwareOutbound() {
        byte[] packet;
        while ((packet = firmware.multiplayerTakeOutgoing()) != null) enqueueTransportWrite(packet);
    }

    @SuppressLint("MissingPermission")
    private boolean enqueueTransportWrite(byte[] bytes) {
        if (bytes == null || bytes.length == 0) return false;
        if (pendingWrites.size() >= BleTransportPolicy.MAX_PENDING_PACKETS) {
            status("BLE packet queue overflow; resetting the physical link.");
            if (gatt != null) failCentral(gatt, "BLE packet queue overflow");
            else if (connectedDevice != null && server != null) {
                BluetoothDevice device = connectedDevice;
                connectedDevice = null;
                server.cancelConnection(device);
                firmware.multiplayerDisconnected();
            }
            return false;
        }
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
            if (started) {
                writeInFlight = true;
            } else {
                handleCentralWriteFailure("BLE GATT write could not start");
            }
            return;
        }
        if (server != null && connectedDevice != null && serverTx != null &&
                serverNotificationsEnabled) {
            if (serverNotifyInFlight || pendingWrites.isEmpty()) return;
            byte[] bytes = pendingWrites.peek();
            int result;
            if (Build.VERSION.SDK_INT >= 33) {
                result = server.notifyCharacteristicChanged(connectedDevice, serverTx, false, bytes);
            } else {
                serverTx.setValue(bytes);
                result = server.notifyCharacteristicChanged(connectedDevice, serverTx, false)
                        ? BluetoothGatt.GATT_SUCCESS : BluetoothGatt.GATT_FAILURE;
            }
            if (result == BluetoothGatt.GATT_SUCCESS) {
                serverNotifyInFlight = true;
            } else {
                handleServerNotifyFailure("BLE server notification could not start");
            }
        }
    }

    private void handleCentralWriteFailure(String reason) {
        writeInFlight = false;
        if (++centralWriteAttempts < BleTransportPolicy.MAX_WRITE_ATTEMPTS) {
            status(reason + "; retry " + centralWriteAttempts);
            handler.postDelayed(this::flushWrites, BleTransportPolicy.GATT_OPERATION_RETRY_MS);
            return;
        }
        BluetoothGatt connection = gatt;
        if (connection != null) failCentral(connection, reason);
    }

    private void handleServerNotifyFailure(String reason) {
        serverNotifyInFlight = false;
        if (++serverNotifyAttempts < BleTransportPolicy.MAX_WRITE_ATTEMPTS) {
            status(reason + "; retry " + serverNotifyAttempts);
            handler.postDelayed(this::flushWrites, BleTransportPolicy.GATT_OPERATION_RETRY_MS);
            return;
        }
        status(reason + "; closing peripheral link");
        BluetoothDevice device = connectedDevice;
        connectedDevice = null;
        pendingWrites.clear();
        if (server != null && device != null) server.cancelConnection(device);
        firmware.multiplayerDisconnected();
        startAdvertisingIfReady();
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
        if (handler == null) {
            transportThread.quitSafely();
            return;
        }
        handler.removeCallbacks(poll);
        handler.post(() -> {
            stopTransport();
            transportThread.quitSafely();
        });
    }

    private void processScanResult(ScanResult result) {
        if (!active || result == null) return;
        MultiplayerProtocol.Advertisement advertisement = null;
        int manufacturerId = -1;
        if (result.getScanRecord() != null) {
            android.util.SparseArray<byte[]> records =
                    result.getScanRecord().getManufacturerSpecificData();
            for (int i = 0; i < records.size(); i++) {
                MultiplayerProtocol.Advertisement parsed =
                        MultiplayerProtocol.parseManufacturerAdvertisement(
                                records.keyAt(i), records.valueAt(i));
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

    private final ScanCallback scanCallback = new ScanCallback() {
        @Override public void onScanResult(int callbackType, ScanResult result) {
            if (handler != null) handler.post(() -> processScanResult(result));
        }

        @Override public void onScanFailed(int errorCode) {
            if (handler != null) handler.post(() -> status("BLE scan failed: " + errorCode));
        }
    };

    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override public void onConnectionStateChange(BluetoothGatt g, int status, int newState) {
            if (handler != null)
                handler.post(() -> handleCentralConnectionState(g, status, newState));
        }

        @Override public void onMtuChanged(BluetoothGatt g, int mtu, int status) {
            if (handler != null) handler.post(() -> {
                if (gatt != g) return;
                BleMultiplayer.this.status("BLE MTU=" + mtu + " status=" + status);
                discoverServicesOnce(g);
            });
        }

        @Override public void onServicesDiscovered(BluetoothGatt g, int status) {
            if (handler != null)
                handler.post(() -> handleServicesDiscovered(g, status));
        }

        @Override public void onDescriptorWrite(BluetoothGatt g, BluetoothGattDescriptor descriptor, int status) {
            if (handler != null) handler.post(() -> {
                if (gatt != g || descriptor == null || !descriptor.getUuid().equals(CCCD)) return;
                if (status == BluetoothGatt.GATT_SUCCESS) beginCentralSession();
                else failCentral(g, "BLE CCCD write failed status=" + status);
            });
        }

        @Override public void onCharacteristicWrite(BluetoothGatt g,
                                                     BluetoothGattCharacteristic characteristic,
                                                     int status) {
            if (handler != null) handler.post(() -> {
                if (gatt != g || characteristic == null || !RX.equals(characteristic.getUuid())) return;
                writeInFlight = false;
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    if (!pendingWrites.isEmpty()) pendingWrites.poll();
                    centralWriteAttempts = 0;
                    flushWrites();
                } else {
                    handleCentralWriteFailure("BLE GATT write failed status=" + status);
                }
            });
        }

        @Override public void onCharacteristicChanged(BluetoothGatt g,
                                                       BluetoothGattCharacteristic characteristic,
                                                       byte[] value) {
            byte[] packet = value == null ? null : value.clone();
            if (handler != null) handler.post(() -> {
                if (gatt == g) handlePacket(packet);
            });
        }

        @Override public void onCharacteristicChanged(BluetoothGatt g,
                                                       BluetoothGattCharacteristic characteristic) {
            byte[] value = characteristic == null ? null : characteristic.getValue();
            byte[] packet = value == null ? null : value.clone();
            if (handler != null) handler.post(() -> {
                if (gatt == g) handlePacket(packet);
            });
        }
    };

    @SuppressLint("MissingPermission")
    private void handleCentralConnectionState(BluetoothGatt connection, int status, int newState) {
        if (gatt != connection) {
            connection.close();
            return;
        }
        BleMultiplayer.this.status("BLE central state=" + newState + " status=" + status);
        if (newState == BluetoothProfile.STATE_CONNECTED && status == BluetoothGatt.GATT_SUCCESS) {
            centralConnecting = true;
            centralDiscoveryStarted = false;
            connection.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH);
            // Negotiate enough room for complete roster/battle packets before
            // discovering the service exposed by the ESP32. Some stacks omit
            // the MTU callback, so discovery also has a bounded fallback.
            if (connection.requestMtu(185)) {
                handler.postDelayed(() -> discoverServicesOnce(connection), 900L);
            } else {
                discoverServicesOnce(connection);
            }
            return;
        }
        if (newState == BluetoothProfile.STATE_DISCONNECTED)
            failCentral(connection, "BLE central disconnected status=" + status);
        else if (newState != BluetoothProfile.STATE_CONNECTED || status != BluetoothGatt.GATT_SUCCESS)
            failCentral(connection, "BLE central connection failed status=" + status);
    }

    @SuppressLint("MissingPermission")
    private void handleServicesDiscovered(BluetoothGatt connection, int status) {
        if (gatt != connection) return;
        if (status != BluetoothGatt.GATT_SUCCESS) {
            failCentral(connection, "BLE service discovery failed status=" + status);
            return;
        }
        BluetoothGattService service = connection.getService(SERVICE);
        if (service == null) {
            failCentral(connection, "BLE multiplayer service missing on peer");
            return;
        }
        remoteRx = service.getCharacteristic(RX);
        BluetoothGattCharacteristic tx = service.getCharacteristic(TX);
        if (remoteRx == null || tx == null) {
            failCentral(connection, "BLE multiplayer characteristics missing on peer");
            return;
        }
        if (!connection.setCharacteristicNotification(tx, true)) {
            failCentral(connection, "BLE notification registration failed");
            return;
        }
        BluetoothGattDescriptor descriptor = tx.getDescriptor(CCCD);
        if (descriptor == null) {
            beginCentralSession();
            return;
        }
        boolean started;
        if (Build.VERSION.SDK_INT >= 33) {
            started = connection.writeDescriptor(descriptor,
                    BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothGatt.GATT_SUCCESS;
        } else {
            descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
            started = connection.writeDescriptor(descriptor);
        }
        if (!started) failCentral(connection, "BLE CCCD write could not start");
    }

    @SuppressLint("MissingPermission")
    private void discoverServicesOnce(BluetoothGatt connection) {
        if (connection == null || gatt != connection || centralDiscoveryStarted) return;
        centralDiscoveryStarted = true;
        if (!connection.discoverServices())
            failCentral(connection, "BLE service discovery could not start");
    }

    @SuppressLint("MissingPermission")
    private void failCentral(BluetoothGatt connection, String reason) {
        if (gatt != connection) {
            connection.close();
            return;
        }
        status(reason);
        centralConnecting = false;
        centralConnectScheduled = false;
        centralReady = false;
        centralDiscoveryStarted = false;
        remoteRx = null;
        gatt = null;
        writeInFlight = false;
        centralWriteAttempts = 0;
        connection.disconnect();
        connection.close();
        scheduleCentralRetry(reason);
    }

    private void beginCentralSession() {
        if (centralReady) return;
        centralReady = true;
        centralConnecting = false;
        centralConnectScheduled = false;
        protocolSessionObserved = false;
        idleLinkSinceMs = 0L;
        firmware.multiplayerTransportConnected(true);
        drainFirmwareOutbound();
        // The invitation may already have been drained while Android was
        // discovering services. Always flush the retained queue now that RX
        // and CCCD are ready.
        flushWrites();
    }

    private final BluetoothGattServerCallback serverCallback = new BluetoothGattServerCallback() {
        @Override public void onServiceAdded(int status, BluetoothGattService service) {
            if (handler != null) handler.post(() -> {
                if (server == null || !active || service == null || !SERVICE.equals(service.getUuid())) return;
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    BleMultiplayer.this.status("BLE GATT service registration failed status=" + status);
                    return;
                }
                serverReady = true;
                BleMultiplayer.this.status("BLE GATT service ready.");
                startAdvertisingIfReady();
            });
        }

        @Override public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
            if (handler != null)
                handler.post(() -> handleServerConnectionState(device, status, newState));
        }

        @Override public void onCharacteristicWriteRequest(BluetoothDevice device, int requestId,
                BluetoothGattCharacteristic characteristic, boolean preparedWrite,
                boolean responseNeeded, int offset, byte[] value) {
            if (responseNeeded && server != null)
                server.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, null);
            byte[] packet = value == null ? null : value.clone();
            if (handler != null) handler.post(() -> {
                if (device.equals(connectedDevice) && characteristic != null &&
                        characteristic.getUuid().equals(RX) && offset == 0) handlePacket(packet);
            });
        }

        @Override public void onDescriptorWriteRequest(BluetoothDevice device, int requestId,
                BluetoothGattDescriptor descriptor, boolean preparedWrite, boolean responseNeeded,
                int offset, byte[] value) {
            if (responseNeeded && server != null)
                server.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, null);
            byte[] descriptorValue = value == null ? null : value.clone();
            if (handler != null) handler.post(() -> {
                if (!device.equals(connectedDevice) || descriptor == null ||
                        !CCCD.equals(descriptor.getUuid()) || offset != 0) return;
                serverNotificationsEnabled = descriptorValue != null && descriptorValue.length >= 2 &&
                        (descriptorValue[0] & 0x03) != 0;
                serverNotifyAttempts = 0;
                BleMultiplayer.this.status("BLE peripheral notifications=" +
                        serverNotificationsEnabled);
                if (serverNotificationsEnabled) flushWrites();
            });
        }

        @Override public void onNotificationSent(BluetoothDevice device, int status) {
            if (handler != null) handler.post(() -> {
                if (!device.equals(connectedDevice) || !serverNotifyInFlight) return;
                serverNotifyInFlight = false;
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    if (!pendingWrites.isEmpty()) pendingWrites.poll();
                    serverNotifyAttempts = 0;
                    flushWrites();
                } else {
                    handleServerNotifyFailure("BLE notification failed status=" + status);
                }
            });
        }
    };

    @SuppressLint("MissingPermission")
    private void handleServerConnectionState(BluetoothDevice device, int status, int newState) {
        BleMultiplayer.this.status("BLE peripheral state=" + newState + " status=" + status);
        if (newState == BluetoothProfile.STATE_CONNECTED && status == BluetoothGatt.GATT_SUCCESS) {
            if (!serverReady) {
                BleMultiplayer.this.status("BLE client connected before GATT service was ready.");
                if (server != null) server.cancelConnection(device);
                return;
            }
            // If both peers initiate at the same time, keep the inbound link
            // and retire our half-open central link. The protocol resolves the
            // two crossing invitations deterministically by player id.
            if (gatt != null) {
                BluetoothGatt superseded = gatt;
                gatt = null;
                remoteRx = null;
                centralConnecting = false;
                centralConnectScheduled = false;
                centralReady = false;
                centralDiscoveryStarted = false;
                writeInFlight = false;
                superseded.disconnect();
                superseded.close();
            }
            stopScan();
            stopAdvertising();
            connectedDevice = device;
            centralConnectScheduled = false;
            serverNotificationsEnabled = false;
            serverNotifyInFlight = false;
            serverNotifyAttempts = 0;
            protocolSessionObserved = false;
            idleLinkSinceMs = 0L;
            firmware.multiplayerTransportConnected(false);
            drainFirmwareOutbound();
            return;
        }
        if (newState == BluetoothProfile.STATE_DISCONNECTED && device.equals(connectedDevice)) {
            connectedDevice = null;
            serverNotificationsEnabled = false;
            serverNotifyInFlight = false;
            serverNotifyAttempts = 0;
            pendingWrites.clear();
            idleLinkSinceMs = 0L;
            firmware.multiplayerDisconnected();
            startAdvertisingIfReady();
        }
    }

    private final AdvertiseCallback advertiseCallback = new AdvertiseCallback() {
        @Override public void onStartSuccess(AdvertiseSettings settingsInEffect) {
            if (handler != null) handler.post(() -> {
                advertising = true;
                if (!active || physicalLinkPresent()) {
                    stopAdvertising();
                    return;
                }
                status("BLE advertising started manufacturer=0x" +
                        Integer.toHexString(MultiplayerProtocol.MANUFACTURER_ID));
            });
        }

        @Override public void onStartFailure(int errorCode) {
            if (handler != null) handler.post(() -> {
                advertising = false;
                status("BLE advertising failed: " + errorCode);
                if (active && !physicalLinkPresent())
                    handler.postDelayed(BleMultiplayer.this::startAdvertisingIfReady, 750L);
            });
        }
    };
}
