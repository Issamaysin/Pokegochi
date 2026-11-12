package com.pokegochi.app;

import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.Map;

/** Raw TCP transport for Multiplayer; game packets remain owned by native code. */
final class WifiMultiplayer implements AutoCloseable {
    interface Listener {
        void onStatus(String status);
    }

    private static final int STATE_ADVERTISING = 1;
    private static final int STATE_SCANNING = 2;
    private static final int STATE_CONNECTING = 4;
    private static final long RECONNECT_MS = 5000L;
    private static final long PING_MS = 10000L;
    private static final long IDLE_TIMEOUT_MS = 30000L;
    private static final long SESSION_TIMEOUT_MS = 120000L;
    private static final long DISCOVERY_RETRY_MS = 5000L;
    private static final long LOCAL_DISCOVERY_WINDOW_MS = 1500L;
    private static final String SERVICE_TYPE = "_pokegochi._tcp.";
    private static final String ACCESS_CODE_KEY = "code";
    private static final String GAME_PROTOCOL_KEY = "game-protocol";

    private final NativeFirmware firmware;
    private final Listener listener;
    private final NsdManager nsdManager;
    private final HandlerThread thread = new HandlerThread("PokegochiWiFi");
    private final ArrayDeque<byte[]> pendingGamePackets = new ArrayDeque<>();
    private final MqttRelayConnection cloud = new MqttRelayConnection();
    private final byte[] headerBytes = new byte[OnlineMultiplayerProtocol.HEADER_BYTES];

    private Handler handler;
    private Socket socket;
    private InputStream input;
    private OutputStream output;
    private OnlineMultiplayerProtocol.Header incomingHeader;
    private byte[] incomingPayload;
    private int headerRead;
    private int payloadRead;
    private boolean running;
    private boolean authenticated;
    private boolean sessionReady;
    private int sessionId;
    private int sessionPeerId;
    private int requestedPeerId;
    private byte[] invitationPacket;
    private long lastInvitationSendMs;
    private int invitationReplayCount;
    private long lastConnectAttemptMs;
    private int lastNativeState = -1;
    private boolean peerRefreshSentForScan;
    private long lastPingMs;
    private long lastReceiveMs;
    private volatile boolean discoveryRunning;
    private volatile boolean resolveInFlight;
    private long lastDiscoveryAttemptMs;
    private long endpointSearchStartedMs;
    private boolean useCloud;
    private volatile String relayHost = "";
    private volatile int relayPort = OnlineMultiplayerProtocol.DEFAULT_PORT;
    private volatile String relayAccessCode = "";
    private volatile NsdManager.DiscoveryListener discoveryListener;

    WifiMultiplayer(Context context, NativeFirmware firmware, Listener listener) {
        Context applicationContext = context.getApplicationContext();
        this.firmware = firmware;
        this.listener = listener;
        nsdManager = applicationContext.getSystemService(NsdManager.class);
    }

    void start() {
        if (running) return;
        running = true;
        thread.start();
        handler = new Handler(thread.getLooper());
        handler.post(poll);
    }

    private final Runnable poll = new Runnable() {
        @Override public void run() {
            if (!running) return;
            final boolean selected = firmware.multiplayerEnabled() &&
                    firmware.multiplayerTransportKind() == 1;
            if (!selected) {
                closeSocket(false);
                handler.postDelayed(this, 80L);
                return;
            }
            if (!relayConnected()) connect();
            if (relayConnected()) {
                try {
                    if (useCloud) {
                        cloud.poll();
                        byte[] frame;
                        while ((frame = cloud.takeIncoming()) != null)
                            readCloudFrame(frame);
                    } else {
                        readAvailableFrames();
                    }
                    driveNativeState();
                } catch (IOException | RuntimeException error) {
                    status("WiFi relay lost: " + error.getClass().getSimpleName());
                    closeSocket(true);
                }
            }
            handler.postDelayed(this, 40L);
        }
    };

    private void connect() {
        long now = SystemClock.elapsedRealtime();
        if (!ensureRelayEndpoint(now)) return;
        if (now - lastConnectAttemptMs < RECONNECT_MS) return;
        lastConnectAttemptMs = now;
        if ((!useCloud && relayHost.isEmpty()) || relayAccessCode.length() != 6) {
            status("Looking for the Pokegochi server automatically.");
            return;
        }
        if (useCloud) {
            try {
                cloud.connect(firmware.multiplayerPlayerId());
                resetConnectedState(now);
                send(OnlineMultiplayerProtocol.HELLO, 0, 0,
                        OnlineMultiplayerProtocol.helloPayload(
                                relayAccessCode, firmware.multiplayerOnlineBalancedWins()));
                status("Connected to the Pokegochi Internet relay.");
            } catch (IOException | RuntimeException error) {
                cloud.close();
                status("Internet relay unavailable; retrying.");
            }
            return;
        }
        Socket opened = new Socket();
        try {
            opened.setTcpNoDelay(true);
            opened.connect(new InetSocketAddress(relayHost, relayPort), 5000);
            socket = opened;
            input = opened.getInputStream();
            output = opened.getOutputStream();
            resetConnectedState(now);
            send(OnlineMultiplayerProtocol.HELLO, 0, 0,
                    OnlineMultiplayerProtocol.helloPayload(
                            relayAccessCode, firmware.multiplayerOnlineBalancedWins()));
            status("Connected to Pokegochi online relay.");
        } catch (IOException | RuntimeException error) {
            try { opened.close(); } catch (IOException ignored) {}
            relayHost = "";
            useCloud = true;
            relayAccessCode = releaseAccessCode();
            lastConnectAttemptMs = 0;
            status("Local relay unavailable; switching to Internet route.");
        }
    }

    private void resetConnectedState(long now) {
        resetFrameReader();
        authenticated = false;
        sessionReady = false;
        sessionId = sessionPeerId = requestedPeerId = 0;
        pendingGamePackets.clear();
        invitationPacket = null;
        lastInvitationSendMs = 0L;
        invitationReplayCount = 0;
        lastReceiveMs = lastPingMs = now;
    }

    private boolean relayConnected() {
        if (useCloud) return cloud.isConnected();
        return socket != null && socket.isConnected() && !socket.isClosed();
    }

    private String releaseAccessCode() {
        String code = firmware.multiplayerOnlineCode();
        return code != null && code.length() == 6 ? code : "579856";
    }

    private boolean ensureRelayEndpoint(long now) {
        if (useCloud && relayAccessCode.length() == 6) return true;
        if (!relayHost.isEmpty() && relayPort > 0 && relayAccessCode.length() == 6)
            return true;
        if (endpointSearchStartedMs == 0L) endpointSearchStartedMs = now;
        if (now - endpointSearchStartedMs >= LOCAL_DISCOVERY_WINDOW_MS) {
            stopRelayDiscovery();
            // The Android emulator cannot receive the LAN mDNS multicast used
            // by the ESP32 relay. Debug builds deliberately route through a
            // host-side TCP forwarder instead, allowing the exact app-hosted
            // battle protocol to be exercised against a physical Pokegochi.
            // Release phones never enter this branch.
            if (BuildConfig.DEBUG && (Build.FINGERPRINT.contains("generic") ||
                    Build.MODEL.contains("Emulator") ||
                    Build.PRODUCT.startsWith("sdk_") ||
                    Build.HARDWARE.contains("ranchu"))) {
                useCloud = false;
                relayHost = "10.0.2.2";
                relayPort = 32146;
                relayAccessCode = releaseAccessCode();
                status("Using the local emulator regression relay.");
                return true;
            }
            useCloud = true;
            relayAccessCode = releaseAccessCode();
            relayPort = MqttRelayConnection.BROKER_PORT;
            status("Using the Pokegochi Internet route.");
            return true;
        }
        if (discoveryRunning) {
            if (now - lastDiscoveryAttemptMs < DISCOVERY_RETRY_MS) return false;
            stopRelayDiscovery();
        }

        if (lastDiscoveryAttemptMs == 0L ||
                now - lastDiscoveryAttemptMs >= DISCOVERY_RETRY_MS)
            startRelayDiscovery(now);
        return false;
    }

    private void startRelayDiscovery(long now) {
        if (nsdManager == null) return;
        lastDiscoveryAttemptMs = now;
        discoveryRunning = true;
        discoveryListener = new NsdManager.DiscoveryListener() {
            @Override public void onDiscoveryStarted(String serviceType) {
                status("Looking for the Pokegochi server automatically.");
            }

            @Override public void onServiceFound(NsdServiceInfo service) {
                String type = service.getServiceType();
                if (resolveInFlight || type == null || !type.contains("_pokegochi._tcp"))
                    return;
                resolveInFlight = true;
                try {
                    nsdManager.resolveService(service, new NsdManager.ResolveListener() {
                        @Override public void onResolveFailed(NsdServiceInfo info, int error) {
                            resolveInFlight = false;
                        }

                        @Override public void onServiceResolved(NsdServiceInfo info) {
                            resolveInFlight = false;
                            Map<String, byte[]> attributes = info.getAttributes();
                            String code = textAttribute(attributes, ACCESS_CODE_KEY);
                            String protocol = textAttribute(attributes, GAME_PROTOCOL_KEY);
                            if (info.getHost() == null || info.getPort() <= 0 ||
                                    code.length() != 6 ||
                                    (!protocol.isEmpty() && !protocol.equals(
                                            Integer.toString(MultiplayerProtocol.VERSION)))) return;
                            relayHost = info.getHost().getHostAddress();
                            relayPort = info.getPort();
                            relayAccessCode = code;
                            useCloud = false;
                            lastConnectAttemptMs = 0;
                            status("Pokegochi server found automatically.");
                            stopRelayDiscovery();
                        }
                    });
                } catch (IllegalArgumentException | IllegalStateException error) {
                    resolveInFlight = false;
                }
            }

            @Override public void onServiceLost(NsdServiceInfo service) {}
            @Override public void onDiscoveryStopped(String serviceType) {
                discoveryRunning = false;
            }
            @Override public void onStartDiscoveryFailed(String serviceType, int error) {
                discoveryRunning = false;
            }
            @Override public void onStopDiscoveryFailed(String serviceType, int error) {
                discoveryRunning = false;
            }
        };
        try {
            nsdManager.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD,
                    discoveryListener);
        } catch (IllegalArgumentException | IllegalStateException error) {
            discoveryRunning = false;
        }
    }

    private static String textAttribute(Map<String, byte[]> attributes, String key) {
        if (attributes == null) return "";
        byte[] value = attributes.get(key);
        return value == null ? "" : new String(value, StandardCharsets.UTF_8).trim();
    }

    private void stopRelayDiscovery() {
        if (nsdManager != null && discoveryRunning && discoveryListener != null) {
            try { nsdManager.stopServiceDiscovery(discoveryListener); }
            catch (IllegalArgumentException | IllegalStateException ignored) {}
        }
        discoveryRunning = false;
        discoveryListener = null;
    }

    private void clearRelayEndpoint() {
        stopRelayDiscovery();
        relayHost = "";
        relayPort = OnlineMultiplayerProtocol.DEFAULT_PORT;
        relayAccessCode = "";
        useCloud = false;
        endpointSearchStartedMs = 0L;
        lastDiscoveryAttemptMs = 0;
    }

    private void driveNativeState() throws IOException {
        if (!authenticated) return;
        final long now = SystemClock.elapsedRealtime();
        final int state = firmware.multiplayerState();
        if (state != lastNativeState) {
            lastNativeState = state;
            if (state == STATE_SCANNING) peerRefreshSentForScan = false;
        }
        // One tap starts one server query. The old 2 s loop kept querying
        // after the UI had already said "not found", so a later player could
        // appear and be selected with no new tap. Leave the authenticated
        // relay connected, but keep player discovery explicitly bounded.
        if (state == STATE_SCANNING && !peerRefreshSentForScan) {
            send(OnlineMultiplayerProtocol.REFRESH_PEERS, 0, 0, null);
            peerRefreshSentForScan = true;
        }
        if (state == STATE_CONNECTING && !sessionReady) {
            int peer = firmware.multiplayerPeerId();
            if (peer != 0 && peer != requestedPeerId) {
                requestedPeerId = peer;
                send(OnlineMultiplayerProtocol.OPEN_SESSION, peer, 0, null);
            }
        }

        byte[] packet;
        while ((packet = firmware.multiplayerTakeOutgoing()) != null) {
            byte[] queued = Arrays.copyOf(packet, packet.length);
            if (OnlineTransportPolicy.isInvite(queued)) {
                invitationPacket = queued.clone();
                lastInvitationSendMs = 0L;
                invitationReplayCount = 0;
            }
            pendingGamePackets.add(queued);
        }
        if (sessionReady) {
            while ((packet = pendingGamePackets.poll()) != null) {
                send(OnlineMultiplayerProtocol.RELAY, sessionPeerId, sessionId, packet);
                if (OnlineTransportPolicy.isInvite(packet))
                    lastInvitationSendMs = now;
            }
            int currentState = firmware.multiplayerState();
            if (OnlineTransportPolicy.shouldReplayInvite(
                    true, currentState, invitationPacket != null,
                    now - lastInvitationSendMs, invitationReplayCount)) {
                send(OnlineMultiplayerProtocol.RELAY, sessionPeerId, sessionId,
                        invitationPacket);
                lastInvitationSendMs = now;
                invitationReplayCount++;
                status("Retrying the online room handshake.");
            }
            if (OnlineTransportPolicy.shouldReleaseAbandonedRoom(true, currentState) ||
                    !firmware.multiplayerEnabled()) {
                send(OnlineMultiplayerProtocol.CLOSE_SESSION, sessionPeerId, sessionId, null);
                clearSession();
            }
        }
        if (now - lastPingMs >= PING_MS) {
            send(OnlineMultiplayerProtocol.PING, sessionPeerId, sessionId, null);
            lastPingMs = now;
        }
        long timeoutMs = sessionReady ? SESSION_TIMEOUT_MS : IDLE_TIMEOUT_MS;
        if (now - lastReceiveMs >= timeoutMs) throw new IOException("relay timeout");
    }

    private void readAvailableFrames() throws IOException {
        while (input != null && input.available() > 0) {
            if (headerRead < headerBytes.length) {
                int count = input.read(headerBytes, headerRead, headerBytes.length - headerRead);
                if (count < 0) throw new IOException("relay closed");
                headerRead += count;
                if (headerRead < headerBytes.length) continue;
                incomingHeader = OnlineMultiplayerProtocol.parseHeader(headerBytes);
                if (incomingHeader == null) throw new IOException("invalid relay header");
                incomingPayload = new byte[incomingHeader.payloadLength];
            }
            if (payloadRead < incomingPayload.length) {
                int count = input.read(incomingPayload, payloadRead,
                        incomingPayload.length - payloadRead);
                if (count < 0) throw new IOException("relay closed");
                payloadRead += count;
                if (payloadRead < incomingPayload.length) continue;
            }
            lastReceiveMs = SystemClock.elapsedRealtime();
            handleFrame(incomingHeader, incomingPayload);
            resetFrameReader();
        }
    }

    private void readCloudFrame(byte[] frame) throws IOException {
        if (frame == null || frame.length < OnlineMultiplayerProtocol.HEADER_BYTES)
            throw new IOException("short Internet relay frame");
        byte[] encodedHeader = Arrays.copyOf(
                frame, OnlineMultiplayerProtocol.HEADER_BYTES);
        OnlineMultiplayerProtocol.Header header =
                OnlineMultiplayerProtocol.parseHeader(encodedHeader);
        if (header == null || frame.length !=
                OnlineMultiplayerProtocol.HEADER_BYTES + header.payloadLength)
            throw new IOException("invalid Internet relay frame");
        byte[] payload = Arrays.copyOfRange(frame,
                OnlineMultiplayerProtocol.HEADER_BYTES, frame.length);
        lastReceiveMs = SystemClock.elapsedRealtime();
        handleFrame(header, payload);
    }

    private void handleFrame(OnlineMultiplayerProtocol.Header header, byte[] payload)
            throws IOException {
        switch (header.type) {
            case OnlineMultiplayerProtocol.WELCOME:
                authenticated = true;
                // Connecting to the server is not the same as scanning for a
                // player. Native remains in Advertising until FIND PLAYER is
                // deliberately pressed, matching the physical Pokegochi.
                status("Online relay authenticated. Tap Find Player to scan.");
                break;
            case OnlineMultiplayerProtocol.PEER_ENTRY:
                firmware.multiplayerDiscovered(header.sourceId, 0,
                        "ONLINE-" + header.sourceId, 0,
                        OnlineMultiplayerProtocol.peerBalancedWins(payload));
                break;
            case OnlineMultiplayerProtocol.PEER_LIST_COMPLETE:
                firmware.multiplayerCompleteWifiPlayerSearch();
                break;
            case OnlineMultiplayerProtocol.SESSION_READY:
                if (payload.length != 1 || header.sessionId == 0 ||
                        header.sourceId == 0 ||
                        (sessionReady && (header.sessionId != sessionId ||
                                header.sourceId != sessionPeerId)))
                    throw new IOException("invalid session ready");
                sessionReady = true;
                sessionId = header.sessionId;
                sessionPeerId = header.sourceId;
                requestedPeerId = 0;
                firmware.multiplayerTransportConnected(payload[0] != 0);
                status("Online room ready with player " + sessionPeerId + ".");
                break;
            case OnlineMultiplayerProtocol.SESSION_BUSY:
                clearSession();
                firmware.multiplayerDisconnected();
                status("Online player is busy or unavailable.");
                break;
            case OnlineMultiplayerProtocol.SESSION_CLOSED:
                if (sessionReady && header.sessionId == sessionId &&
                        header.sourceId == sessionPeerId) {
                    firmware.multiplayerDisconnected();
                    clearSession();
                    status("Online room closed.");
                }
                break;
            case OnlineMultiplayerProtocol.RELAY:
                if (sessionReady && header.sessionId == sessionId &&
                        header.sourceId == sessionPeerId &&
                        MultiplayerProtocol.parsePacket(payload,
                                firmware.multiplayerPlayerId()) != null)
                    firmware.multiplayerReceive(payload);
                break;
            case OnlineMultiplayerProtocol.PING:
                send(OnlineMultiplayerProtocol.PONG, header.sourceId,
                        header.sessionId, null);
                break;
            case OnlineMultiplayerProtocol.PONG:
                break;
            case OnlineMultiplayerProtocol.ERROR:
                throw new IOException("relay rejected request");
            default:
                break;
        }
    }

    private void send(int type, int targetId, int frameSessionId, byte[] payload)
            throws IOException {
        byte[] frame = OnlineMultiplayerProtocol.frame(type,
                firmware.multiplayerPlayerId(), targetId, frameSessionId, payload);
        if (useCloud) {
            cloud.publish(frame);
            return;
        }
        if (output == null) throw new IOException("relay not connected");
        output.write(frame);
        output.flush();
    }

    private void clearSession() {
        sessionReady = false;
        sessionId = sessionPeerId = requestedPeerId = 0;
        pendingGamePackets.clear();
        invitationPacket = null;
        lastInvitationSendMs = 0L;
        invitationReplayCount = 0;
    }

    private void resetFrameReader() {
        headerRead = payloadRead = 0;
        incomingHeader = null;
        incomingPayload = null;
    }

    private void closeSocket(boolean notifyNative) {
        boolean hadSession = sessionReady;
        Socket closing = socket;
        socket = null; input = null; output = null;
        cloud.close();
        authenticated = false;
        lastNativeState = -1;
        peerRefreshSentForScan = false;
        clearSession();
        resetFrameReader();
        if (closing != null) try { closing.close(); } catch (IOException ignored) {}
        if (notifyNative && hadSession) firmware.multiplayerDisconnected();
    }

    private void status(String text) {
        if (listener != null) listener.onStatus(text);
    }

    @Override public void close() {
        running = false;
        if (handler != null) handler.removeCallbacksAndMessages(null);
        closeSocket(false);
        stopRelayDiscovery();
        thread.quitSafely();
    }
}
