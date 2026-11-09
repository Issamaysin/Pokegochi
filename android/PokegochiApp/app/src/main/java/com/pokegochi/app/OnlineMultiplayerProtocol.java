package com.pokegochi.app;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/** Little-endian TCP envelope shared with the dedicated ESP32 relay. */
final class OnlineMultiplayerProtocol {
    static final int MAGIC = 0x4e4f4750; // PGON
    static final int VERSION = 2;
    static final int DEFAULT_PORT = 32145;
    static final int HEADER_BYTES = 20;
    static final int MAX_GAME_PACKET = MultiplayerProtocol.HEADER_BYTES + MultiplayerProtocol.MAX_PAYLOAD;

    static final int HELLO = 1;
    static final int WELCOME = 2;
    static final int REFRESH_PEERS = 3;
    static final int PEER_ENTRY = 4;
    static final int PEER_LIST_COMPLETE = 5;
    static final int OPEN_SESSION = 6;
    static final int SESSION_READY = 7;
    static final int SESSION_BUSY = 8;
    static final int CLOSE_SESSION = 9;
    static final int SESSION_CLOSED = 10;
    static final int RELAY = 11;
    static final int PING = 12;
    static final int PONG = 13;
    static final int ERROR = 14;

    private OnlineMultiplayerProtocol() {}

    static byte[] frame(int type, int sourceId, int targetId, int sessionId,
                        byte[] payload) {
        byte[] body = payload == null ? new byte[0] : payload;
        if (body.length > MAX_GAME_PACKET)
            throw new IllegalArgumentException("online payload exceeds protocol budget");
        return ByteBuffer.allocate(HEADER_BYTES + body.length).order(ByteOrder.LITTLE_ENDIAN)
                .putInt(MAGIC).put((byte) VERSION).put((byte) type)
                .putShort((short) body.length).putInt(sourceId).putInt(targetId)
                .putInt(sessionId).put(body).array();
    }

    static byte[] helloPayload(String accessCode, int balancedWins) {
        int code;
        try { code = Integer.parseInt(accessCode == null ? "" : accessCode); }
        catch (NumberFormatException error) { code = -1; }
        if (code < 0 || accessCode == null || accessCode.length() != 6)
            throw new IllegalArgumentException("access code must contain six digits");
        return ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN)
                .put((byte) MultiplayerProtocol.VERSION).put(new byte[3]).putInt(code)
                .putInt(balancedWins).array();
    }

    static int peerBalancedWins(byte[] payload) {
        if (payload == null || payload.length != 4)
            throw new IllegalArgumentException("peer entry payload must contain four bytes");
        return ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN).getInt();
    }

    static Header parseHeader(byte[] bytes) {
        if (bytes == null || bytes.length != HEADER_BYTES) return null;
        ByteBuffer input = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        int magic = input.getInt();
        int version = input.get() & 0xff;
        int type = input.get() & 0xff;
        int length = input.getShort() & 0xffff;
        int sourceId = input.getInt();
        int targetId = input.getInt();
        int sessionId = input.getInt();
        if (magic != MAGIC || version != VERSION || type < HELLO || type > ERROR ||
                length > MAX_GAME_PACKET) return null;
        return new Header(type, length, sourceId, targetId, sessionId);
    }

    static final class Header {
        final int type, payloadLength, sourceId, targetId, sessionId;
        Header(int type, int payloadLength, int sourceId, int targetId, int sessionId) {
            this.type = type; this.payloadLength = payloadLength; this.sourceId = sourceId;
            this.targetId = targetId; this.sessionId = sessionId;
        }
    }
}
