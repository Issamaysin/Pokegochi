package com.pokegochi.updater;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.zip.CRC32;

/** Strict parser for the portable PGS1 save container produced by Pokegochi. */
final class SaveBackup {
    final byte[] bytes;
    final int saveFormatVersion;
    final long sourcePlayerId;
    final long payloadCrc32;
    final long fileCrc32;

    private SaveBackup(byte[] bytes, int saveFormatVersion, long sourcePlayerId,
                       long payloadCrc32, long fileCrc32) {
        this.bytes = bytes;
        this.saveFormatVersion = saveFormatVersion;
        this.sourcePlayerId = sourcePlayerId;
        this.payloadCrc32 = payloadCrc32;
        this.fileCrc32 = fileCrc32;
    }

    static SaveBackup read(InputStream input) throws IOException {
        if (input == null) throw new IOException("Could not open the selected save file");
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] chunk = new byte[4096];
        int count;
        while ((count = input.read(chunk)) >= 0) {
            if (count == 0) continue;
            if (output.size() + count > UpdateProtocol.MAXIMUM_SAVE_BACKUP_BYTES)
                throw new IOException("Save file is larger than Pokegochi supports");
            output.write(chunk, 0, count);
        }
        return parse(output.toByteArray());
    }

    static SaveBackup parse(byte[] bytes) throws IOException {
        if (bytes == null || bytes.length < UpdateProtocol.SAVE_HEADER_BYTES)
            throw new IOException("Save file is truncated");
        ByteBuffer header = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        if (header.get() != 'P' || header.get() != 'G' || header.get() != 'S' || header.get() != '1')
            throw new IOException("This is not a Pokegochi .pgsav file");
        int backupVersion = header.getShort() & 0xffff;
        int saveVersion = header.getShort() & 0xffff;
        long payloadBytes = Integer.toUnsignedLong(header.getInt());
        long playerId = Integer.toUnsignedLong(header.getInt());
        long payloadCrc = Integer.toUnsignedLong(header.getInt());
        long headerCrc = Integer.toUnsignedLong(header.getInt());
        if (backupVersion != UpdateProtocol.SAVE_BACKUP_VERSION)
            throw new IOException("Unsupported backup format version " + backupVersion);
        if (payloadBytes + UpdateProtocol.SAVE_HEADER_BYTES != bytes.length)
            throw new IOException("Save file length does not match its header");
        if (crc32(bytes, 0, 20) != headerCrc)
            throw new IOException("Save header checksum failed");
        if (crc32(bytes, UpdateProtocol.SAVE_HEADER_BYTES, (int) payloadBytes) != payloadCrc)
            throw new IOException("Save data checksum failed");
        return new SaveBackup(bytes, saveVersion, playerId, payloadCrc,
                crc32(bytes, 0, bytes.length));
    }

    private static long crc32(byte[] bytes, int offset, int length) {
        CRC32 crc = new CRC32();
        crc.update(bytes, offset, length);
        return crc.getValue();
    }
}
