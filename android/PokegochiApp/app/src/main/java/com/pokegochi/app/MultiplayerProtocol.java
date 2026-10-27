package com.pokegochi.app;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;

/** Wire contract shared with src/services/Multiplayer.cpp. */
final class MultiplayerProtocol {
    static final int VERSION = 7;
    static final int PACKET_MAGIC = 0x4750;
    static final int MAX_PAYLOAD = 160;
    // Android removes these first two bytes from manufacturer data returned
    // by ScanRecord. They are the P,G prefix of the canonical PGCH record.
    static final int MANUFACTURER_ID = 0x4750;
    static final int LEGACY_ANDROID_MANUFACTURER_ID = 0xffff;
    static final String SERVICE_UUID = "70564743-4849-4d50-8000-504f4b45474f";
    static final String RX_UUID = "70564743-4849-4d50-8001-504f4b45474f";
    static final String TX_UUID = "70564743-4849-4d50-8002-504f4b45474f";
    static final byte[] SIGNATURE = new byte[] {'P', 'G', 'C', 'H'};
    static final int HEADER_BYTES = 14;

    static final int INVITE = 1;
    static final int INVITE_ACCEPT = 2;
    static final int INVITE_REJECT = 3;
    static final int SESSION_CLOSE = 4;
    static final int ACTIVITY_REQUEST = 5;
    static final int ACTIVITY_ACCEPT = 6;
    static final int ACTIVITY_REJECT = 7;
    static final int TRADE_OFFER = 8;
    static final int TRADE_CONFIRM = 9;
    static final int TRADE_REJECT = 10;
    static final int TRADE_COMMIT_REQUEST = 11;
    static final int TRADE_COMMIT_APPLIED = 12;
    static final int BATTLE_PARTY_MEMBER = 13;
    static final int BATTLE_PARTY_READY = 14;
    static final int BATTLE_LEAD = 15;
    static final int BATTLE_START = 16;
    static final int ACTIVITY_CANCEL = 17;
    static final int BATTLE_COMMAND = 18;
    static final int BATTLE_SYNC_POKEMON = 19;
    static final int BATTLE_SYNC_VOLATILE = 20;
    static final int BATTLE_SYNC_CORE = 21;
    static final int BATTLE_SYNC_ACK = 22;

    private MultiplayerProtocol() {}

    static byte[] advertisement(int playerId) {
        ByteBuffer buffer = ByteBuffer.allocate(9).order(ByteOrder.LITTLE_ENDIAN);
        buffer.put(SIGNATURE).put((byte) VERSION).putInt(playerId);
        return buffer.array();
    }

    /** Value passed to Android's addManufacturerData after its separate id. */
    static byte[] androidManufacturerPayload(int playerId) {
        return Arrays.copyOfRange(advertisement(playerId), 2, 9);
    }

    /** Decodes the id/value pair exposed by Android's ScanRecord API. */
    static Advertisement parseManufacturerAdvertisement(int manufacturerId, byte[] value) {
        if (value == null) return null;
        if (manufacturerId == MANUFACTURER_ID && value.length == 7) {
            byte[] complete = new byte[9];
            complete[0] = SIGNATURE[0];
            complete[1] = SIGNATURE[1];
            System.arraycopy(value, 0, complete, 2, value.length);
            return parseAdvertisement(complete);
        }
        // Compatibility with 0.2.33 and earlier, which asked Android to add
        // ffff before an already-complete nine-byte PGCH record.
        if (manufacturerId == LEGACY_ANDROID_MANUFACTURER_ID)
            return parseAdvertisement(value);
        return null;
    }

    static Advertisement parseAdvertisement(byte[] value) {
        if (value == null || value.length != 9) return null;
        if (!Arrays.equals(SIGNATURE, Arrays.copyOfRange(value, 0, 4))) return null;
        int version = value[4] & 0xff;
        if (version != VERSION) return null;
        int playerId = ByteBuffer.wrap(value, 5, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
        if (playerId <= 0) return null;
        return new Advertisement(playerId, version);
    }

    static byte[] packet(int type, int sourceId, int transaction, byte[] payload) {
        byte[] body = payload == null ? new byte[0] : payload;
        if (body.length > MAX_PAYLOAD) throw new IllegalArgumentException("BLE payload exceeds 160 bytes");
        ByteBuffer buffer = ByteBuffer.allocate(HEADER_BYTES + body.length).order(ByteOrder.LITTLE_ENDIAN);
        buffer.putShort((short) PACKET_MAGIC).put((byte) VERSION).put((byte) type);
        buffer.putInt(sourceId).putInt(transaction).putShort((short) body.length).put(body);
        return buffer.array();
    }

    static Packet parsePacket(byte[] bytes, int localPlayerId) {
        if (bytes == null || bytes.length < HEADER_BYTES) return null;
        ByteBuffer buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        int magic = buffer.getShort() & 0xffff;
        int version = buffer.get() & 0xff;
        int type = buffer.get() & 0xff;
        int sourceId = buffer.getInt();
        int transaction = buffer.getInt();
        int payloadLength = buffer.getShort() & 0xffff;
        if (magic != PACKET_MAGIC || version != VERSION || sourceId == localPlayerId ||
                payloadLength > MAX_PAYLOAD || bytes.length != HEADER_BYTES + payloadLength) return null;
        byte[] payload = new byte[payloadLength];
        buffer.get(payload);
        return new Packet(type, sourceId, transaction, payload);
    }

    static final class Advertisement {
        final int playerId;
        final int version;
        Advertisement(int playerId, int version) { this.playerId = playerId; this.version = version; }
    }

    static final class Packet {
        final int type;
        final int sourceId;
        final int transaction;
        final byte[] payload;
        Packet(int type, int sourceId, int transaction, byte[] payload) {
            this.type = type; this.sourceId = sourceId; this.transaction = transaction; this.payload = payload;
        }
    }
}
