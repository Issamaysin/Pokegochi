package com.pokegochi.app;

/** Deterministic recovery policy for the TCP/Internet multiplayer bridge. */
final class OnlineTransportPolicy {
    static final int STATE_OFF = 0;
    static final int STATE_ADVERTISING = 1;
    static final int STATE_SCANNING = 2;
    static final int STATE_CONNECTING = 4;
    static final int STATE_CONNECTED = 6;
    static final int STATE_ERROR = 16;

    static final int MAX_INVITE_REPLAYS = 6;
    static final long INVITE_REPLAY_MS = 600L;

    private OnlineTransportPolicy() {}

    static boolean isInvite(byte[] packet) {
        if (packet == null || packet.length < MultiplayerProtocol.HEADER_BYTES)
            return false;
        return (packet[0] & 0xff) == (MultiplayerProtocol.PACKET_MAGIC & 0xff) &&
                (packet[1] & 0xff) == ((MultiplayerProtocol.PACKET_MAGIC >>> 8) & 0xff) &&
                (packet[2] & 0xff) == MultiplayerProtocol.VERSION &&
                (packet[3] & 0xff) == MultiplayerProtocol.INVITE;
    }

    static boolean shouldReplayInvite(boolean sessionReady, int firmwareState,
                                      boolean inviteAvailable, long elapsedMs,
                                      int completedReplays) {
        return sessionReady && inviteAvailable &&
                firmwareState == STATE_CONNECTING &&
                elapsedMs >= INVITE_REPLAY_MS &&
                completedReplays < MAX_INVITE_REPLAYS;
    }

    static boolean shouldReleaseAbandonedRoom(boolean sessionReady, int firmwareState) {
        if (!sessionReady) return false;
        return firmwareState == STATE_OFF || firmwareState == STATE_ADVERTISING ||
                firmwareState == STATE_SCANNING || firmwareState == STATE_ERROR;
    }
}
