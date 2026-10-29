package com.pokegochi.app;

/** Deterministic timing/retry policy shared by the BLE transport and its tests. */
final class BleTransportPolicy {
    static final int STATE_ADVERTISING = 1;
    static final int STATE_SCANNING = 2;
    static final int STATE_CONNECTING = 4;

    static final int MAX_CENTRAL_ATTEMPTS = 3;
    static final int MAX_WRITE_ATTEMPTS = 3;
    static final int MAX_PENDING_PACKETS = 40;
    static final long CENTRAL_CONNECT_TIMEOUT_MS = 12_000L;
    static final long INBOUND_HANDSHAKE_TIMEOUT_MS = 12_000L;
    static final long ESTABLISHED_CLOSE_GRACE_MS = 650L;
    static final long GATT_OPERATION_RETRY_MS = 160L;
    // Android's controller needs a short role-transition window after an LE
    // scan/advertising stop before it starts a central connection. Without
    // it, some phones fail locally with status 133 and the ESP32 never sees
    // an inbound physical link.
    static final long CENTRAL_ROLE_SETTLE_MS = 320L;

    private static final long[] CENTRAL_RETRY_DELAYS_MS = {350L, 900L, 1_800L};

    private BleTransportPolicy() {}

    static boolean isIdleProtocolState(int state) {
        return state == STATE_ADVERTISING || state == STATE_SCANNING;
    }

    static boolean hasObservedProtocolSession(int state) {
        return state > STATE_CONNECTING;
    }

    static long staleLinkGraceMs(boolean protocolSessionObserved) {
        return protocolSessionObserved
                ? ESTABLISHED_CLOSE_GRACE_MS : INBOUND_HANDSHAKE_TIMEOUT_MS;
    }

    static boolean canRetryCentral(int firmwareState, int completedAttempts) {
        return firmwareState == STATE_CONNECTING &&
                completedAttempts < MAX_CENTRAL_ATTEMPTS;
    }

    static long centralRetryDelayMs(int completedAttempts) {
        int index = Math.max(0, Math.min(completedAttempts - 1,
                CENTRAL_RETRY_DELAYS_MS.length - 1));
        return CENTRAL_RETRY_DELAYS_MS[index];
    }
}
