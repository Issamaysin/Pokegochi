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
import android.bluetooth.BluetoothStatusCodes;
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
import java.util.Arrays;
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
    private ScanCallback scanCallback;
    private BluetoothLeAdvertiser advertiser;
    private AdvertiseCallback advertiseCallback;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic remoteRx;
    private BluetoothGattServer server;
    private BluetoothGattCharacteristic serverTx;
    private BluetoothDevice connectedDevice;
    private BluetoothDevice closingPeripheralDevice;
    private boolean serverReady;
    private boolean serverRestartScheduled;
    private boolean advertising;
    private boolean advertisingStarting;
    private boolean scanActive;
    private boolean running;
    private boolean active;
    private boolean centralConnecting;
    private boolean centralConnectScheduled;
    private boolean centralReady;
    private boolean centralDiscoveryStarted;
    private boolean writeInFlight;
    private boolean serverNotifyInFlight;
    private boolean serverNotificationsEnabled;
    private boolean peripheralTransportAnnounced;
    private boolean protocolSessionObserved;
    private boolean lastFirmwareEnabled;
    private int lastFirmwareState = -1;
    private boolean lastPermissionsMissing;
    private long idleLinkSinceMs;
    private long centralWriteStartedMs;
    private long serverNotifyStartedMs;
    private long scanRetryAtMs;
    private long advertisingRetryAtMs;
    private long peripheralHandshakeDeadlineMs;
    private long peripheralCloseDeadlineMs;
    private long centralRetryAtMs;
    private int serverGeneration;
    private int scanGeneration;
    private int advertisingGeneration;
    private int centralGeneration;
    private int centralPeerId;
    private int centralAttempts;
    private int centralMtu = 23;
    private int centralWriteAttempts;
    private int serverNotifyAttempts;
    private byte[] handshakeReplayPacket;
    private int handshakeReplayType;
    private int handshakeReplayCount;
    private long handshakeReplayAtMs;

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
                if (firmwareState == STATE_SCANNING && lastFirmwareState != STATE_SCANNING)
                    peers.clear();
                lastFirmwareEnabled = enabled;
                lastFirmwareState = firmwareState;
            }
            final boolean permissionsMissing = enabled && adapter != null &&
                    (!hasClientPermissions() || !hasPeripheralPermissions());
            if (permissionsMissing && !lastPermissionsMissing && listener != null)
                listener.onPermissionsRequired();
            lastPermissionsMissing = permissionsMissing;
            final boolean transportAvailable = supported() && hasClientPermissions() &&
                    hasPeripheralPermissions();
            if (enabled && transportAvailable) ensureActive();
            else if (active) {
                // A Bluetooth toggle or permission revocation must not leave
                // the native lobby in CONNECTING while every Android GATT
                // object has become invalid.
                if (enabled) firmware.multiplayerDisconnected();
                stopTransport();
            }

            if (active) {
                drainFirmwareOutbound();
                int state = firmware.multiplayerState();
                if (BleTransportPolicy.hasObservedProtocolSession(state))
                    protocolSessionObserved = true;
                checkGattOperationTimeouts();
                checkPeripheralHandshakeTimeout();
                replayHandshakeIfNeeded(state);
                releaseStalePhysicalSession(state);
                if (BleTransportPolicy.isIdleProtocolState(state) && !physicalLinkPresent())
                    ensurePeripheralRole();
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
        if (!hasClientPermissions() || !hasPeripheralPermissions() || !supported()) return;
        active = true;
        ensurePeripheralRole();
        status("BLE transport ready for player " + firmware.multiplayerPlayerId() + ".");
    }

    private void ensurePeripheralRole() {
        if (!active || physicalLinkPresent()) return;
        if (server == null) startServer();
        else startAdvertisingIfReady();
    }

    @SuppressLint("MissingPermission")
    private void startAdvertisingIfReady() {
        if (!active || !serverReady || advertising || advertisingStarting ||
                physicalLinkPresent() || closingPeripheralDevice != null ||
                !hasPeripheralPermissions()) return;
        if (SystemClock.elapsedRealtime() < advertisingRetryAtMs) return;
        if (advertiser == null) advertiser = adapter.getBluetoothLeAdvertiser();
        if (advertiser == null) {
            status("BLE advertiser unavailable.");
            advertisingRetryAtMs = SystemClock.elapsedRealtime() +
                    BleTransportPolicy.ADVERTISE_RETRY_MS;
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
        final int generation = ++advertisingGeneration;
        AdvertiseCallback callback = createAdvertiseCallback(generation);
        advertiseCallback = callback;
        advertisingStarting = true;
        try {
            advertiser.startAdvertising(settings, data, callback);
        } catch (RuntimeException error) {
            if (advertisingGeneration == generation) {
                advertisingStarting = false;
                advertiseCallback = null;
                advertisingRetryAtMs = SystemClock.elapsedRealtime() +
                        BleTransportPolicy.ADVERTISE_RETRY_MS;
            }
            status("BLE advertising start exception: " + error.getClass().getSimpleName());
        }
    }

    @SuppressLint("MissingPermission")
    private void stopAdvertising() {
        AdvertiseCallback callback = advertiseCallback;
        ++advertisingGeneration;
        advertiseCallback = null;
        if (advertiser != null && callback != null) {
            try {
                advertiser.stopAdvertising(callback);
            } catch (RuntimeException ignored) {}
        }
        advertising = false;
        advertisingStarting = false;
    }

    private boolean physicalLinkPresent() {
        return gatt != null || connectedDevice != null || closingPeripheralDevice != null ||
                centralConnecting || centralConnectScheduled || centralReady;
    }

    @SuppressLint("MissingPermission")
    private void startScan() {
        if (scanActive || !hasClientPermissions() || !supported() || physicalLinkPresent()) return;
        if (SystemClock.elapsedRealtime() < scanRetryAtMs) return;
        scanner = adapter.getBluetoothLeScanner();
        if (scanner == null) return;
        ScanSettings settings = new ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .setReportDelay(0L)
                .build();
        final int generation = ++scanGeneration;
        ScanCallback callback = createScanCallback(generation);
        scanCallback = callback;
        try {
            scanner.startScan(null, settings, callback);
            scanActive = true;
        } catch (RuntimeException error) {
            scanCallback = null;
            scanner = null;
            scanActive = false;
            scanRetryAtMs = SystemClock.elapsedRealtime() + BleTransportPolicy.SCAN_RETRY_MS;
            status("BLE scan start exception: " + error.getClass().getSimpleName());
        }
    }

    @SuppressLint("MissingPermission")
    private void stopScan() {
        BluetoothLeScanner currentScanner = scanner;
        ScanCallback callback = scanCallback;
        ++scanGeneration;
        scanner = null;
        scanCallback = null;
        scanActive = false;
        if (currentScanner != null && callback != null) {
            try {
                currentScanner.stopScan(callback);
            } catch (RuntimeException ignored) {}
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
        centralMtu = 23;
        ++centralAttempts;
        final int generation = ++centralGeneration;
        status("BLE central attempt " + centralAttempts + "/" +
                BleTransportPolicy.MAX_CENTRAL_ATTEMPTS + " player=" + playerId +
                "; settling radio roles");
        final int scheduledAttempt = centralAttempts;
        handler.postDelayed(() -> openCentralGatt(
                        device, playerId, scheduledAttempt, generation),
                BleTransportPolicy.CENTRAL_ROLE_SETTLE_MS);
    }

    @SuppressLint("MissingPermission")
    private void openCentralGatt(BluetoothDevice device, int playerId, int scheduledAttempt,
                                 int generation) {
        if (!active || firmware.multiplayerState() != STATE_CONNECTING ||
                centralPeerId != playerId || centralAttempts != scheduledAttempt ||
                centralGeneration != generation || gatt != null || connectedDevice != null) {
            centralConnectScheduled = false;
            centralConnecting = false;
            return;
        }
        // Give a simultaneously-initiating ESP32 a short chance to complete
        // its inbound connection. If no inbound link won, remove Android's
        // peripheral GATT registration before becoming central. Several phone
        // stacks return status 133 indefinitely when both roles reuse the same
        // application registration after a cancelled session.
        closeGattServer(false);
        handler.postDelayed(() -> openCentralGattAfterServerRelease(
                        device, playerId, scheduledAttempt, generation),
                BleTransportPolicy.GATT_SERVER_TO_CENTRAL_SETTLE_MS);
    }

    @SuppressLint("MissingPermission")
    private void openCentralGattAfterServerRelease(BluetoothDevice device, int playerId,
                                                   int scheduledAttempt, int generation) {
        centralConnectScheduled = false;
        if (!active || firmware.multiplayerState() != STATE_CONNECTING ||
                centralPeerId != playerId || centralAttempts != scheduledAttempt ||
                centralGeneration != generation || gatt != null || connectedDevice != null) {
            centralConnecting = false;
            ensurePeripheralRole();
            return;
        }
        // Classic discovery can be active independently of BluetoothLeScanner
        // and is a documented source of GATT connection failures.
        if (adapter != null && adapter.isDiscovering()) adapter.cancelDiscovery();
        try {
            if (Build.VERSION.SDK_INT >= 26) {
                gatt = device.connectGatt(context, false, gattCallback,
                        BluetoothDevice.TRANSPORT_LE, BluetoothDevice.PHY_LE_1M_MASK, handler);
            } else {
                gatt = device.connectGatt(context, false, gattCallback,
                        BluetoothDevice.TRANSPORT_LE);
            }
        } catch (RuntimeException error) {
            gatt = null;
            centralConnecting = false;
            scheduleCentralRetry("BLE connectGatt exception " +
                    error.getClass().getSimpleName());
            return;
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
        centralMtu = 23;
        centralRetryAtMs = 0L;
        pendingWrites.clear();
        clearHandshakeReplay();
        firmware.multiplayerDisconnected();
        restartGattServer();
        return false;
    }

    @SuppressLint("MissingPermission")
    private void startServer() {
        if (server != null || serverRestartScheduled || !active ||
                physicalLinkPresent() || !hasPeripheralPermissions()) return;
        BluetoothManager manager = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        if (manager == null) return;
        final int generation = ++serverGeneration;
        BluetoothGattServer opened;
        try {
            opened = manager.openGattServer(context, createServerCallback(generation));
        } catch (RuntimeException error) {
            status("BLE GATT server exception: " + error.getClass().getSimpleName());
            restartGattServer();
            return;
        }
        if (opened == null) {
            status("BLE GATT server allocation failed.");
            restartGattServer();
            return;
        }
        server = opened;
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
            closeGattServer(false);
            restartGattServer();
            return;
        }
        status("BLE GATT service registration pending.");
    }

    @SuppressLint("MissingPermission")
    private void closeGattServer(boolean cancelConnectedDevice) {
        stopAdvertising();
        ++serverGeneration;
        BluetoothGattServer oldServer = server;
        BluetoothDevice oldDevice = connectedDevice;
        server = null;
        serverReady = false;
        serverTx = null;
        serverNotificationsEnabled = false;
        serverNotifyInFlight = false;
        serverNotifyStartedMs = 0L;
        peripheralTransportAnnounced = false;
        peripheralHandshakeDeadlineMs = 0L;
        if (oldServer == null) return;
        if (cancelConnectedDevice && oldDevice != null) {
            try { oldServer.cancelConnection(oldDevice); } catch (RuntimeException ignored) {}
        }
        try { oldServer.close(); } catch (RuntimeException ignored) {}
    }

    private void restartGattServer() {
        if (!active || serverRestartScheduled || centralConnecting ||
                centralConnectScheduled || gatt != null || centralReady) return;
        closeGattServer(false);
        serverRestartScheduled = true;
        handler.postDelayed(() -> {
            serverRestartScheduled = false;
            if (!active || physicalLinkPresent()) return;
            startServer();
        }, BleTransportPolicy.SERVER_RESTART_SETTLE_MS);
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
        active = false;
        stopScan();
        stopAdvertising();
        advertiser = null;
        if (gatt != null) {
            gatt.disconnect();
            gatt.close();
            gatt = null;
        }
        closeGattServer(true);
        remoteRx = null;
        connectedDevice = null;
        closingPeripheralDevice = null;
        ++centralGeneration;
        centralConnecting = false;
        centralConnectScheduled = false;
        centralReady = false;
        centralDiscoveryStarted = false;
        writeInFlight = false;
        serverNotifyInFlight = false;
        serverNotificationsEnabled = false;
        peripheralTransportAnnounced = false;
        scanActive = false;
        advertisingStarting = false;
        serverRestartScheduled = false;
        protocolSessionObserved = false;
        centralPeerId = 0;
        centralAttempts = 0;
        centralMtu = 23;
        centralWriteAttempts = 0;
        serverNotifyAttempts = 0;
        centralWriteStartedMs = 0L;
        serverNotifyStartedMs = 0L;
        scanRetryAtMs = 0L;
        advertisingRetryAtMs = 0L;
        peripheralHandshakeDeadlineMs = 0L;
        peripheralCloseDeadlineMs = 0L;
        centralRetryAtMs = 0L;
        pendingWrites.clear();
        clearHandshakeReplay();
        idleLinkSinceMs = 0L;
    }

    @SuppressLint("MissingPermission")
    private void releaseStalePhysicalSession(int firmwareState) {
        final long now = SystemClock.elapsedRealtime();
        if (closingPeripheralDevice != null) {
            if (now >= peripheralCloseDeadlineMs) {
                status("BLE peripheral close timed out; rebuilding GATT server.");
                finishPeripheralClose();
            }
            return;
        }
        final boolean idleProtocol = BleTransportPolicy.isIdleProtocolState(firmwareState);
        if (!idleProtocol || !physicalLinkPresent()) {
            idleLinkSinceMs = 0L;
            return;
        }

        // Service discovery + CCCD on an ESP32 commonly takes more than the
        // old 500 ms grace. A new inbound link gets a complete handshake
        // window; a link that reached the protocol is released quickly after
        // SessionClose or a declined invitation.
        if (idleLinkSinceMs == 0L) {
            idleLinkSinceMs = now;
        }
        final boolean centralPhysical = gatt != null || centralConnecting ||
                centralConnectScheduled || centralReady;
        final long grace = centralPhysical &&
                BleTransportPolicy.shouldAbortUnreadyCentral(firmwareState, centralReady)
                ? 0L : BleTransportPolicy.staleLinkGraceMs(protocolSessionObserved);
        if (now - idleLinkSinceMs < grace) return;

        status("BLE releasing " + (protocolSessionObserved ? "closed" : "unanswered") +
                " physical session.");
        if (centralPhysical) {
            closeCentralPhysicalSession(false);
        } else if (connectedDevice != null) {
            beginPeripheralClose(false);
        } else {
            resetPhysicalSessionFields();
            restartGattServer();
        }
    }

    @SuppressLint("MissingPermission")
    private void closeCentralPhysicalSession(boolean notifyFirmware) {
        BluetoothGatt oldGatt = gatt;
        ++centralGeneration;
        gatt = null;
        remoteRx = null;
        centralConnecting = false;
        centralConnectScheduled = false;
        centralReady = false;
        centralDiscoveryStarted = false;
        writeInFlight = false;
        centralWriteStartedMs = 0L;
        centralPeerId = 0;
        centralAttempts = 0;
        centralMtu = 23;
        centralWriteAttempts = 0;
        centralRetryAtMs = 0L;
        pendingWrites.clear();
        clearHandshakeReplay();
        protocolSessionObserved = false;
        idleLinkSinceMs = 0L;
        if (oldGatt != null) {
            try { oldGatt.disconnect(); } catch (RuntimeException ignored) {}
            try { oldGatt.close(); } catch (RuntimeException ignored) {}
        }
        if (notifyFirmware) firmware.multiplayerDisconnected();
        restartGattServer();
    }

    @SuppressLint("MissingPermission")
    private void beginPeripheralClose(boolean notifyFirmware) {
        BluetoothDevice device = connectedDevice;
        connectedDevice = null;
        serverNotificationsEnabled = false;
        serverNotifyInFlight = false;
        serverNotifyStartedMs = 0L;
        serverNotifyAttempts = 0;
        peripheralTransportAnnounced = false;
        peripheralHandshakeDeadlineMs = 0L;
        pendingWrites.clear();
        clearHandshakeReplay();
        protocolSessionObserved = false;
        idleLinkSinceMs = 0L;
        if (notifyFirmware) firmware.multiplayerDisconnected();
        if (device == null || server == null) {
            finishPeripheralClose();
            return;
        }
        closingPeripheralDevice = device;
        peripheralCloseDeadlineMs = SystemClock.elapsedRealtime() +
                BleTransportPolicy.PERIPHERAL_CLOSE_TIMEOUT_MS;
        try {
            server.cancelConnection(device);
        } catch (RuntimeException error) {
            status("BLE peripheral cancel exception: " + error.getClass().getSimpleName());
            finishPeripheralClose();
        }
    }

    private void finishPeripheralClose() {
        closingPeripheralDevice = null;
        peripheralCloseDeadlineMs = 0L;
        closeGattServer(false);
        restartGattServer();
    }

    private void resetPhysicalSessionFields() {
        remoteRx = null;
        centralConnecting = false;
        centralConnectScheduled = false;
        centralReady = false;
        centralDiscoveryStarted = false;
        writeInFlight = false;
        serverNotifyInFlight = false;
        serverNotificationsEnabled = false;
        peripheralTransportAnnounced = false;
        peripheralHandshakeDeadlineMs = 0L;
        centralWriteStartedMs = 0L;
        serverNotifyStartedMs = 0L;
        protocolSessionObserved = false;
        centralPeerId = 0;
        centralAttempts = 0;
        centralMtu = 23;
        centralWriteAttempts = 0;
        serverNotifyAttempts = 0;
        centralRetryAtMs = 0L;
        pendingWrites.clear();
        clearHandshakeReplay();
        idleLinkSinceMs = 0L;
    }

    private void drainFirmwareOutbound() {
        byte[] packet;
        while ((packet = firmware.multiplayerTakeOutgoing()) != null) {
            trackHandshakePacket(packet);
            enqueueTransportWrite(packet);
        }
    }

    @SuppressLint("MissingPermission")
    private boolean enqueueTransportWrite(byte[] bytes) {
        if (bytes == null || bytes.length == 0) return false;
        if (pendingWrites.size() >= BleTransportPolicy.MAX_PENDING_PACKETS) {
            status("BLE packet queue overflow; resetting the physical link.");
            if (gatt != null) failCentral(gatt, "BLE packet queue overflow");
            else if (connectedDevice != null) beginPeripheralClose(true);
            return false;
        }
        pendingWrites.add(bytes);
        flushWrites();
        return true;
    }

    @SuppressLint("MissingPermission")
    private void flushWrites() {
        if (remoteRx != null && gatt != null && centralReady) {
            if (writeInFlight || pendingWrites.isEmpty()) return;
            byte[] bytes = pendingWrites.peek();
            boolean started;
            if (Build.VERSION.SDK_INT >= 33) {
                started = gatt.writeCharacteristic(remoteRx, bytes,
                        BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) ==
                        BluetoothStatusCodes.SUCCESS;
            } else {
                remoteRx.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                remoteRx.setValue(bytes);
                started = gatt.writeCharacteristic(remoteRx);
            }
            if (started) {
                writeInFlight = true;
                centralWriteStartedMs = SystemClock.elapsedRealtime();
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
                serverNotifyStartedMs = SystemClock.elapsedRealtime();
            } else {
                handleServerNotifyFailure("BLE server notification could not start");
            }
        }
    }

    private void handleCentralWriteFailure(String reason) {
        writeInFlight = false;
        centralWriteStartedMs = 0L;
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
        serverNotifyStartedMs = 0L;
        if (++serverNotifyAttempts < BleTransportPolicy.MAX_WRITE_ATTEMPTS) {
            status(reason + "; retry " + serverNotifyAttempts);
            handler.postDelayed(this::flushWrites, BleTransportPolicy.GATT_OPERATION_RETRY_MS);
            return;
        }
        status(reason + "; closing peripheral link");
        beginPeripheralClose(true);
    }

    private void handlePacket(byte[] bytes) {
        if (bytes == null || bytes.length == 0) return;
        MultiplayerProtocol.Packet packet = MultiplayerProtocol.parsePacket(
                bytes, firmware.multiplayerPlayerId());
        if (packet == null) {
            status("BLE rejected malformed or incompatible packet length=" + bytes.length);
            return;
        }
        if (packet.type == MultiplayerProtocol.INVITE_ACCEPT ||
                packet.type == MultiplayerProtocol.INVITE_REJECT)
            clearHandshakeReplay();
        firmware.multiplayerReceive(bytes);
        drainFirmwareOutbound();
    }

    private void trackHandshakePacket(byte[] bytes) {
        MultiplayerProtocol.Packet packet = MultiplayerProtocol.parsePacket(bytes, 0);
        if (packet == null || (packet.type != MultiplayerProtocol.INVITE &&
                packet.type != MultiplayerProtocol.INVITE_ACCEPT)) return;
        handshakeReplayPacket = bytes.clone();
        handshakeReplayType = packet.type;
        handshakeReplayCount = 0;
        handshakeReplayAtMs = SystemClock.elapsedRealtime() +
                BleTransportPolicy.HANDSHAKE_REPLAY_MS;
    }

    private void replayHandshakeIfNeeded(int firmwareState) {
        if (handshakeReplayPacket == null ||
                SystemClock.elapsedRealtime() < handshakeReplayAtMs) return;
        if (!BleTransportPolicy.shouldReplayHandshake(handshakeReplayType, firmwareState,
                handshakeReplayCount)) {
            clearHandshakeReplay();
            return;
        }
        if (!centralReady && !serverNotificationsEnabled) {
            handshakeReplayAtMs = SystemClock.elapsedRealtime() +
                    BleTransportPolicy.HANDSHAKE_REPLAY_MS;
            return;
        }
        boolean alreadyQueued = false;
        for (byte[] queued : pendingWrites) {
            if (Arrays.equals(queued, handshakeReplayPacket)) {
                alreadyQueued = true;
                break;
            }
        }
        if (!alreadyQueued) {
            pendingWrites.add(handshakeReplayPacket.clone());
            ++handshakeReplayCount;
            status("BLE replaying handshake packet type=" + handshakeReplayType +
                    " attempt=" + handshakeReplayCount);
            flushWrites();
        }
        handshakeReplayAtMs = SystemClock.elapsedRealtime() +
                BleTransportPolicy.HANDSHAKE_REPLAY_MS;
    }

    private void clearHandshakeReplay() {
        handshakeReplayPacket = null;
        handshakeReplayType = 0;
        handshakeReplayCount = 0;
        handshakeReplayAtMs = 0L;
    }

    private void checkGattOperationTimeouts() {
        final long now = SystemClock.elapsedRealtime();
        if (writeInFlight && centralWriteStartedMs != 0L &&
                now - centralWriteStartedMs >= BleTransportPolicy.GATT_OPERATION_TIMEOUT_MS) {
            handleCentralWriteFailure("BLE GATT write callback timed out");
        }
        if (serverNotifyInFlight && serverNotifyStartedMs != 0L &&
                now - serverNotifyStartedMs >= BleTransportPolicy.GATT_OPERATION_TIMEOUT_MS) {
            handleServerNotifyFailure("BLE notification callback timed out");
        }
    }

    private void checkPeripheralHandshakeTimeout() {
        if (connectedDevice == null || serverNotificationsEnabled ||
                peripheralHandshakeDeadlineMs == 0L ||
                SystemClock.elapsedRealtime() < peripheralHandshakeDeadlineMs) return;
        status("BLE peripheral subscription timed out; rebuilding the link.");
        beginPeripheralClose(true);
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

    private ScanCallback createScanCallback(int generation) {
        return new ScanCallback() {
            @Override public void onScanResult(int callbackType, ScanResult result) {
                if (handler != null) handler.post(() -> {
                    if (generation == scanGeneration && scanCallback == this)
                        processScanResult(result);
                });
            }

            @Override public void onBatchScanResults(java.util.List<ScanResult> results) {
                if (handler != null) handler.post(() -> {
                    if (generation != scanGeneration || scanCallback != this) return;
                    for (ScanResult result : results) processScanResult(result);
                });
            }

            @Override public void onScanFailed(int errorCode) {
                if (handler != null) handler.post(() -> {
                    if (generation != scanGeneration || scanCallback != this) return;
                    status("BLE scan failed: " + errorCode + "; restarting cleanly");
                    stopScan();
                    scanRetryAtMs = SystemClock.elapsedRealtime() +
                            BleTransportPolicy.SCAN_RETRY_MS;
                });
            }
        };
    }

    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override public void onConnectionStateChange(BluetoothGatt g, int status, int newState) {
            if (handler != null)
                handler.post(() -> handleCentralConnectionState(g, status, newState));
        }

        @Override public void onMtuChanged(BluetoothGatt g, int mtu, int status) {
            if (handler != null) handler.post(() -> {
                if (gatt != g) return;
                BleMultiplayer.this.status("BLE MTU=" + mtu + " status=" + status);
                if (status != BluetoothGatt.GATT_SUCCESS ||
                        mtu < BleTransportPolicy.MINIMUM_GATT_MTU) {
                    failCentral(g, "BLE MTU too small for multiplayer packets: " + mtu);
                    return;
                }
                centralMtu = mtu;
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
                centralWriteStartedMs = 0L;
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
                handler.postDelayed(() -> {
                    if (gatt == connection && !centralDiscoveryStarted &&
                            centralMtu < BleTransportPolicy.MINIMUM_GATT_MTU)
                        failCentral(connection, "BLE MTU negotiation callback timed out");
                }, 1_500L);
            } else {
                failCentral(connection, "BLE MTU negotiation could not start");
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
                    BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) ==
                    BluetoothStatusCodes.SUCCESS;
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
        centralWriteStartedMs = 0L;
        centralWriteAttempts = 0;
        connection.disconnect();
        connection.close();
        scheduleCentralRetry(reason);
    }

    private void beginCentralSession() {
        if (centralReady) return;
        if (firmware.multiplayerState() != STATE_CONNECTING) {
            status("BLE central became ready after protocol cancellation; releasing it.");
            closeCentralPhysicalSession(false);
            return;
        }
        centralReady = true;
        centralConnecting = false;
        centralConnectScheduled = false;
        protocolSessionObserved = false;
        idleLinkSinceMs = 0L;
        centralWriteStartedMs = 0L;
        firmware.multiplayerTransportConnected(true);
        drainFirmwareOutbound();
        // The invitation may already have been drained while Android was
        // discovering services. Always flush the retained queue now that RX
        // and CCCD are ready.
        flushWrites();
    }

    @SuppressLint("MissingPermission")
    private BluetoothGattServerCallback createServerCallback(int generation) {
        return new BluetoothGattServerCallback() {
        @Override public void onServiceAdded(int status, BluetoothGattService service) {
            if (handler != null) handler.post(() -> {
                if (generation != serverGeneration || server == null || !active ||
                        service == null || !SERVICE.equals(service.getUuid())) return;
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    BleMultiplayer.this.status("BLE GATT service registration failed status=" + status);
                    restartGattServer();
                    return;
                }
                serverReady = true;
                BleMultiplayer.this.status("BLE GATT service ready.");
                startAdvertisingIfReady();
            });
        }

        @Override public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
            if (handler != null)
                handler.post(() -> handleServerConnectionState(
                        device, status, newState, generation));
        }

        @Override public void onCharacteristicWriteRequest(BluetoothDevice device, int requestId,
                BluetoothGattCharacteristic characteristic, boolean preparedWrite,
                boolean responseNeeded, int offset, byte[] value) {
            BluetoothGattServer currentServer = server;
            if (responseNeeded && generation == serverGeneration && currentServer != null)
                currentServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, null);
            byte[] packet = value == null ? null : value.clone();
            if (handler != null) handler.post(() -> {
                if (generation != serverGeneration || !device.equals(connectedDevice) ||
                        characteristic == null || !characteristic.getUuid().equals(RX) ||
                        preparedWrite || offset != 0) return;
                // A valid write proves service discovery completed even if a
                // phone delivered the CCCD callback late. Announce the
                // transport before handing Invite to the native protocol.
                announcePeripheralTransport();
                handlePacket(packet);
            });
        }

        @Override public void onDescriptorWriteRequest(BluetoothDevice device, int requestId,
                BluetoothGattDescriptor descriptor, boolean preparedWrite, boolean responseNeeded,
                int offset, byte[] value) {
            BluetoothGattServer currentServer = server;
            if (responseNeeded && generation == serverGeneration && currentServer != null)
                currentServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, null);
            byte[] descriptorValue = value == null ? null : value.clone();
            if (handler != null) handler.post(() -> {
                if (generation != serverGeneration || !device.equals(connectedDevice) ||
                        descriptor == null || !CCCD.equals(descriptor.getUuid()) ||
                        preparedWrite || offset != 0) return;
                serverNotificationsEnabled = descriptorValue != null && descriptorValue.length >= 2 &&
                        (descriptorValue[0] & 0x03) != 0;
                serverNotifyAttempts = 0;
                BleMultiplayer.this.status("BLE peripheral notifications=" +
                        serverNotificationsEnabled);
                if (serverNotificationsEnabled) {
                    peripheralHandshakeDeadlineMs = 0L;
                    announcePeripheralTransport();
                    flushWrites();
                } else {
                    beginPeripheralClose(true);
                }
            });
        }

        @Override public void onNotificationSent(BluetoothDevice device, int status) {
            if (handler != null) handler.post(() -> {
                if (generation != serverGeneration || !device.equals(connectedDevice) ||
                        !serverNotifyInFlight) return;
                serverNotifyInFlight = false;
                serverNotifyStartedMs = 0L;
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
    }

    @SuppressLint("MissingPermission")
    private void handleServerConnectionState(BluetoothDevice device, int status, int newState,
                                             int generation) {
        if (generation != serverGeneration) return;
        BleMultiplayer.this.status("BLE peripheral state=" + newState + " status=" + status);
        if (newState == BluetoothProfile.STATE_CONNECTED && status == BluetoothGatt.GATT_SUCCESS) {
            if (!serverReady || closingPeripheralDevice != null ||
                    (connectedDevice != null && !device.equals(connectedDevice))) {
                BleMultiplayer.this.status("BLE client connected before GATT service was ready.");
                if (server != null) server.cancelConnection(device);
                return;
            }
            // If both peers initiate at the same time, keep the inbound link
            // and retire our half-open central link. The protocol resolves the
            // two crossing invitations deterministically by player id.
            ++centralGeneration;
            if (gatt != null) {
                BluetoothGatt superseded = gatt;
                gatt = null;
                remoteRx = null;
                centralConnecting = false;
                centralConnectScheduled = false;
                centralReady = false;
                centralDiscoveryStarted = false;
                writeInFlight = false;
                centralWriteStartedMs = 0L;
                superseded.disconnect();
                superseded.close();
            }
            stopScan();
            stopAdvertising();
            connectedDevice = device;
            closingPeripheralDevice = null;
            peripheralTransportAnnounced = false;
            peripheralHandshakeDeadlineMs = SystemClock.elapsedRealtime() +
                    BleTransportPolicy.INBOUND_HANDSHAKE_TIMEOUT_MS;
            centralConnecting = false;
            centralConnectScheduled = false;
            centralPeerId = 0;
            centralAttempts = 0;
            centralRetryAtMs = 0L;
            serverNotificationsEnabled = false;
            serverNotifyInFlight = false;
            serverNotifyStartedMs = 0L;
            serverNotifyAttempts = 0;
            protocolSessionObserved = false;
            idleLinkSinceMs = 0L;
            return;
        }
        if (newState == BluetoothProfile.STATE_DISCONNECTED &&
                device.equals(closingPeripheralDevice)) {
            finishPeripheralClose();
            return;
        }
        if (newState == BluetoothProfile.STATE_DISCONNECTED && device.equals(connectedDevice)) {
            connectedDevice = null;
            serverNotificationsEnabled = false;
            serverNotifyInFlight = false;
            serverNotifyStartedMs = 0L;
            serverNotifyAttempts = 0;
            pendingWrites.clear();
            clearHandshakeReplay();
            idleLinkSinceMs = 0L;
            peripheralTransportAnnounced = false;
            peripheralHandshakeDeadlineMs = 0L;
            firmware.multiplayerDisconnected();
            restartGattServer();
        }
    }

    private void announcePeripheralTransport() {
        if (peripheralTransportAnnounced || connectedDevice == null) return;
        peripheralTransportAnnounced = true;
        firmware.multiplayerTransportConnected(false);
        drainFirmwareOutbound();
    }

    private AdvertiseCallback createAdvertiseCallback(int generation) {
        return new AdvertiseCallback() {
            @Override public void onStartSuccess(AdvertiseSettings settingsInEffect) {
                if (handler != null) handler.post(() -> {
                    if (generation != advertisingGeneration || advertiseCallback != this) return;
                    advertisingStarting = false;
                    advertising = true;
                    advertisingRetryAtMs = 0L;
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
                    if (generation != advertisingGeneration || advertiseCallback != this) return;
                    advertisingStarting = false;
                    advertising = false;
                    advertiseCallback = null;
                    advertisingRetryAtMs = SystemClock.elapsedRealtime() +
                            BleTransportPolicy.ADVERTISE_RETRY_MS;
                    status("BLE advertising failed: " + errorCode + "; retry scheduled");
                });
            }
        };
    }
}
