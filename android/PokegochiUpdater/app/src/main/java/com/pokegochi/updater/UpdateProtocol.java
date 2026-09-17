package com.pokegochi.updater;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.UUID;

final class UpdateProtocol {
    static final int VERSION = 1;
    static final int DEVICE_MODEL = 1;
    static final int LEGACY_MAXIMUM_DATA_BYTES = 184;
    static final int MAXIMUM_DATA_BYTES = 508;
    static final UUID SERVICE = UUID.fromString("70564743-4849-5550-8000-504f4b45474f");
    static final UUID CONTROL = UUID.fromString("70564743-4849-5550-8001-504f4b45474f");
    static final UUID DATA = UUID.fromString("70564743-4849-5550-8002-504f4b45474f");
    static final UUID STATUS = UUID.fromString("70564743-4849-5550-8003-504f4b45474f");
    static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    static final int AUTHENTICATE = 1;
    static final int MANIFEST_BEGIN = 2;
    static final int MANIFEST_CHUNK = 3;
    static final int MANIFEST_FINISH = 4;
    static final int OBJECT_BEGIN = 5;
    static final int OBJECT_FINISH = 6;
    static final int COMMIT = 7;
    static final int ABORT = 8;
    static final int QUERY_STATUS = 9;

    static final int STATUS_HELLO = 1;
    static final int STATUS_AUTHENTICATED = 2;
    static final int STATUS_MANIFEST_ACCEPTED = 3;
    static final int STATUS_OBJECT_READY = 4;
    static final int STATUS_DATA_ACCEPTED = 5;
    static final int STATUS_OBJECT_VERIFIED = 6;
    static final int STATUS_COMMIT_ACCEPTED = 7;
    static final int STATUS_COMPLETE = 8;
    static final int STATUS_ABORTED = 9;
    static final int STATUS_ERROR = 10;

    static final int OBJECT_FIRMWARE = 0;
    static final int OBJECT_ASSET = 1;

    private UpdateProtocol() {}

    static byte[] authenticate(int code) {
        return little(6).put((byte) AUTHENTICATE).put((byte) VERSION).putInt(code).array();
    }

    static byte[] manifestBegin(int size) {
        return little(3).put((byte) MANIFEST_BEGIN).putShort((short) size).array();
    }

    static byte[] manifestChunk(int offset, byte[] manifest, int count) {
        ByteBuffer packet = little(3 + count);
        packet.put((byte) MANIFEST_CHUNK).putShort((short) offset).put(manifest, offset, count);
        return packet.array();
    }

    static byte[] command(int command) { return new byte[]{(byte) command}; }

    static byte[] objectBegin(int kind, int assetId) {
        return new byte[]{(byte) OBJECT_BEGIN, (byte) kind, (byte) assetId};
    }

    static byte[] data(int offset, byte[] payload, int count) {
        ByteBuffer packet = little(4 + count);
        packet.putInt(offset).put(payload, 0, count);
        return packet.array();
    }

    static Status parseStatus(byte[] bytes) {
        if (bytes == null || bytes.length != 24 || bytes[0] != 'U' || bytes[1] != 'P') return null;
        ByteBuffer value = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        value.position(2);
        Status status = new Status();
        status.version = value.get() & 0xff;
        status.code = value.get() & 0xff;
        status.state = value.get() & 0xff;
        status.error = value.get() & 0xff;
        status.objectKind = value.get() & 0xff;
        status.assetId = value.get() & 0xff;
        status.nextOffset = Integer.toUnsignedLong(value.getInt());
        status.objectSize = Integer.toUnsignedLong(value.getInt());
        status.packageDone = Integer.toUnsignedLong(value.getInt());
        status.packageTotal = Integer.toUnsignedLong(value.getInt());
        return status;
    }

    static ByteBuffer little(int size) {
        return ByteBuffer.allocate(size).order(ByteOrder.LITTLE_ENDIAN);
    }

    static final class Status {
        int version, code, state, error, objectKind, assetId;
        long nextOffset, objectSize, packageDone, packageTotal;
    }
}
