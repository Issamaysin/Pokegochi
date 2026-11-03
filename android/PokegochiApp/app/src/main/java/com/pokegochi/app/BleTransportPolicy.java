package com.pokegochi.app;

/** Deterministic timing/retry policy shared by the BLE transport and its tests. */
final class BleTransportPolicy {
    static final int STATE_OFF = 0;
    static final int STATE_ADVERTISING = 1;
    static final int STATE_SCANNING = 2;
    static final int STATE_CONFIRM_OUTGOING_PEER = 3;
    static final int STATE_CONNECTING = 4;
    static final int STATE_CONFIRM_INCOMING_PEER = 5;
    static final int STATE_CONNECTED = 6;
    static final int STATE_WAITING_ACTIVITY_REPLY = 7;
    static final int STATE_CONFIRM_INCOMING_ACTIVITY = 8;
    static final int STATE_TRADE_SELECTING = 9;
    static final int STATE_TRADE_CONFIRMING = 10;
    static final int STATE_BATTLE_PREPARING = 11;
    static final int STATE_BATTLE_WAITING_READY = 12;
    static final int STATE_BATTLE_CHOOSING_LEAD = 13;
    static final int STATE_BATTLE_LAUNCHING = 14;
    static final int STATE_BATTLE_ACTIVE = 15;
    static final int STATE_ERROR = 16;

    static final int MAX_CENTRAL_ATTEMPTS = 5;
    static final int MAX_WRITE_ATTEMPTS = 3;
    static final int MAX_PENDING_PACKETS = 40;
    static final int MAX_HANDSHAKE_REPLAYS = 4;
    static final int MINIMUM_GATT_MTU = 177; // 174-byte packet + ATT header
    static final long CENTRAL_CONNECT_TIMEOUT_MS = 14_000L;
    static final long INBOUND_HANDSHAKE_TIMEOUT_MS = 12_000L;
    static final long ESTABLISHED_CLOSE_GRACE_MS = 900L;
    static final long GATT_OPERATION_RETRY_MS = 160L;
    static final long GATT_OPERATION_TIMEOUT_MS = 2_500L;
    static final long HANDSHAKE_REPLAY_MS = 700L;
    static final long SCAN_RETRY_MS = 1_200L;
    static final long ADVERTISE_RETRY_MS = 900L;
    static final long SERVER_RESTART_SETTLE_MS = 450L;
    static final long PERIPHERAL_CLOSE_TIMEOUT_MS = 1_800L;
    // Android's controller needs a short role-transition window after an LE
    // scan/advertising stop before it starts a central connection. Without
    // it, some phones fail locally with status 133 and the ESP32 never sees
    // an inbound physical link.
    static final long CENTRAL_ROLE_SETTLE_MS = 420L;
    // A GATT server allocation is a separate Android controller role. Closing
    // it before connectGatt avoids the central path inheriting a stale CCCD or
    // application registration from the previous peripheral session.
    static final long GATT_SERVER_TO_CENTRAL_SETTLE_MS = 280L;

    private static final long[] CENTRAL_RETRY_DELAYS_MS = {
            450L, 850L, 1_400L, 2_100L, 3_000L};

    private BleTransportPolicy() {}

    static boolean isIdleProtocolState(int state) {
        return state == STATE_ADVERTISING || state == STATE_SCANNING;
    }

    static boolean hasObservedProtocolSession(int state) {
        return state >= STATE_CONFIRM_INCOMING_PEER && state < STATE_ERROR;
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

    static boolean shouldAbortUnreadyCentral(int firmwareState, boolean centralReady) {
        return !centralReady && isIdleProtocolState(firmwareState);
    }

    static boolean shouldReplayHandshake(int packetType, int firmwareState,
                                         int completedReplays) {
        if (completedReplays >= MAX_HANDSHAKE_REPLAYS) return false;
        if (packetType == MultiplayerProtocol.INVITE)
            return firmwareState == STATE_CONNECTING;
        // InviteAccept has no acknowledgement in protocol v7. A bounded replay
        // is safe: a connected peer answers a duplicate Invite idempotently,
        // while every later state ignores the duplicate acceptance.
        return packetType == MultiplayerProtocol.INVITE_ACCEPT &&
                firmwareState >= STATE_CONNECTED && firmwareState < STATE_ERROR;
    }
}
