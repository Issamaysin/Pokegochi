package com.pokegochi.app;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.zip.CRC32;

/** Validates a portable PGS1 backup and installs it into the native save slots. */
final class PgsavSaveImporter {
    static final int BACKUP_VERSION = 1;
    static final int CURRENT_SAVE_VERSION = 44;
    static final int CONTAINER_HEADER_BYTES = 24;
    static final int RECORD_HEADER_BYTES = 16;
    static final int RECORD_TRAILER_BYTES = 8;
    static final int MAXIMUM_BACKUP_BYTES = 64 * 1024;
    static final int RECORD_MAGIC = 0x504f4b45;

    static final class Parsed {
        final byte[] container;
        final byte[] payload;
        final int saveVersion;
        final long sourcePlayerId;

        Parsed(byte[] container, byte[] payload, int saveVersion, long sourcePlayerId) {
            this.container = container;
            this.payload = payload;
            this.saveVersion = saveVersion;
            this.sourcePlayerId = sourcePlayerId;
        }
    }

    static final class InstallResult {
        final File backupDirectory;
        final int firstSequence;
        final int secondSequence;

        InstallResult(File backupDirectory, int firstSequence, int secondSequence) {
            this.backupDirectory = backupDirectory;
            this.firstSequence = firstSequence;
            this.secondSequence = secondSequence;
        }
    }

    private PgsavSaveImporter() {}

    static Parsed read(InputStream input) throws IOException {
        if (input == null) throw new IOException("Nao foi possivel abrir o arquivo selecionado.");
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] chunk = new byte[4096];
        int count;
        while ((count = input.read(chunk)) >= 0) {
            if (count == 0) continue;
            if (output.size() + count > MAXIMUM_BACKUP_BYTES)
                throw new IOException("O arquivo e maior que um save do Pokegochi.");
            output.write(chunk, 0, count);
        }
        return parse(output.toByteArray());
    }

    static Parsed parse(byte[] bytes) throws IOException {
        if (bytes == null || bytes.length < CONTAINER_HEADER_BYTES)
            throw new IOException("O arquivo de save esta incompleto.");
        ByteBuffer header = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        if (header.get() != 'P' || header.get() != 'G' || header.get() != 'S' || header.get() != '1')
            throw new IOException("Este arquivo nao e um save .pgsav do Pokegochi.");
        int backupVersion = header.getShort() & 0xffff;
        int saveVersion = header.getShort() & 0xffff;
        long payloadBytes = Integer.toUnsignedLong(header.getInt());
        long playerId = Integer.toUnsignedLong(header.getInt());
        long payloadCrc = Integer.toUnsignedLong(header.getInt());
        long headerCrc = Integer.toUnsignedLong(header.getInt());
        if (backupVersion != BACKUP_VERSION)
            throw new IOException("Versao do container .pgsav nao suportada: " + backupVersion + ".");
        if (saveVersion != CURRENT_SAVE_VERSION)
            throw new IOException("O app requer save V" + CURRENT_SAVE_VERSION + ", mas o arquivo e V" + saveVersion + ".");
        if (payloadBytes <= 0 || payloadBytes > 0xffff ||
                payloadBytes + CONTAINER_HEADER_BYTES != bytes.length)
            throw new IOException("O tamanho declarado no save nao confere.");
        if (crc32(bytes, 0, 20) != headerCrc)
            throw new IOException("O cabecalho do save falhou na verificacao CRC.");
        if (crc32(bytes, CONTAINER_HEADER_BYTES, (int) payloadBytes) != payloadCrc)
            throw new IOException("Os dados do save falharam na verificacao CRC.");
        byte[] payload = new byte[(int) payloadBytes];
        System.arraycopy(bytes, CONTAINER_HEADER_BYTES, payload, 0, payload.length);
        return new Parsed(bytes.clone(), payload, saveVersion, playerId);
    }

    static InstallResult install(File preferencesRoot, File backupRoot, Parsed save)
            throws IOException {
        if (preferencesRoot == null || backupRoot == null || save == null)
            throw new IOException("Destino de save invalido.");
        File namespace = new File(preferencesRoot, "pokegochi");
        if (!namespace.isDirectory() && !namespace.mkdirs())
            throw new IOException("Nao foi possivel abrir o armazenamento privado do app.");

        File backup = new File(backupRoot, "before-import-" + System.currentTimeMillis());
        if (!backup.mkdirs()) throw new IOException("Nao foi possivel criar o backup preventivo.");
        copyIfPresent(new File(namespace, "save_a.bin"), new File(backup, "save_a.bin"));
        copyIfPresent(new File(namespace, "save_b.bin"), new File(backup, "save_b.bin"));
        copyIfPresent(new File(namespace, "started.bin"), new File(backup, "started.bin"));
        copyIfPresent(new File(namespace, "sinnoh_tms.bin"), new File(backup, "sinnoh_tms.bin"));
        writeSynced(new File(backup, "imported-source.pgsav"), save.container);

        int latest = latestSequence(new File(namespace, "save_a.bin"),
                new File(namespace, "save_b.bin"), save.payload.length);
        int first = latest + 1;
        int second = first + 1;
        byte[] recordA = buildRecord(save.payload, save.saveVersion, first);
        byte[] recordB = buildRecord(save.payload, save.saveVersion, second);

        replaceAtomically(new File(namespace, "save_a.bin"), recordA);
        replaceAtomically(new File(namespace, "save_b.bin"), recordB);
        replaceAtomically(new File(namespace, "started.bin"), new byte[] {1});

        validateInstalled(new File(namespace, "save_a.bin"), save.payload, first);
        validateInstalled(new File(namespace, "save_b.bin"), save.payload, second);
        return new InstallResult(backup, first, second);
    }

    static byte[] buildRecord(byte[] payload, int saveVersion, int sequence) throws IOException {
        if (payload == null || payload.length <= 0 || payload.length > 0xffff)
            throw new IOException("Payload de save invalido.");
        byte[] record = new byte[RECORD_HEADER_BYTES + payload.length + RECORD_TRAILER_BYTES];
        ByteBuffer buffer = ByteBuffer.wrap(record).order(ByteOrder.LITTLE_ENDIAN);
        buffer.putInt(RECORD_MAGIC);
        buffer.putShort((short) saveVersion);
        buffer.putShort((short) payload.length);
        buffer.putInt(sequence);
        buffer.putInt(0); // Alignment padding before the 8-byte-aligned GameSave.
        buffer.put(payload);
        int crcOffset = RECORD_HEADER_BYTES + payload.length;
        buffer.putInt((int) crc32(record, 0, crcOffset));
        buffer.putInt(0); // Trailing ABI padding.
        return record;
    }

    private static int latestSequence(File first, File second, int payloadBytes) {
        Integer a = recordSequence(first, payloadBytes);
        Integer b = recordSequence(second, payloadBytes);
        if (a == null) return b == null ? 0 : b;
        if (b == null) return a;
        return (b - a) > 0 ? b : a;
    }

    private static Integer recordSequence(File file, int payloadBytes) {
        try {
            byte[] bytes = readFile(file, RECORD_HEADER_BYTES + payloadBytes + RECORD_TRAILER_BYTES);
            if (bytes == null) return null;
            ByteBuffer buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
            if (buffer.getInt() != RECORD_MAGIC || (buffer.getShort() & 0xffff) != CURRENT_SAVE_VERSION ||
                    (buffer.getShort() & 0xffff) != payloadBytes) return null;
            int sequence = buffer.getInt();
            int crcOffset = RECORD_HEADER_BYTES + payloadBytes;
            long stored = Integer.toUnsignedLong(ByteBuffer.wrap(bytes, crcOffset, 4)
                    .order(ByteOrder.LITTLE_ENDIAN).getInt());
            return stored == crc32(bytes, 0, crcOffset) ? sequence : null;
        } catch (IOException ignored) {
            return null;
        }
    }

    private static void validateInstalled(File file, byte[] expectedPayload, int expectedSequence)
            throws IOException {
        byte[] bytes = readFile(file,
                RECORD_HEADER_BYTES + expectedPayload.length + RECORD_TRAILER_BYTES);
        if (bytes == null) throw new IOException("O slot de save gravado nao pode ser relido.");
        ByteBuffer buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        if (buffer.getInt() != RECORD_MAGIC || (buffer.getShort() & 0xffff) != CURRENT_SAVE_VERSION ||
                (buffer.getShort() & 0xffff) != expectedPayload.length ||
                buffer.getInt() != expectedSequence)
            throw new IOException("O slot de save gravado possui cabecalho invalido.");
        int crcOffset = RECORD_HEADER_BYTES + expectedPayload.length;
        long stored = Integer.toUnsignedLong(ByteBuffer.wrap(bytes, crcOffset, 4)
                .order(ByteOrder.LITTLE_ENDIAN).getInt());
        if (stored != crc32(bytes, 0, crcOffset))
            throw new IOException("O slot de save gravado falhou na verificacao CRC.");
        for (int i = 0; i < expectedPayload.length; ++i) {
            if (bytes[RECORD_HEADER_BYTES + i] != expectedPayload[i])
                throw new IOException("O slot de save gravado nao corresponde ao arquivo escolhido.");
        }
    }

    private static byte[] readFile(File file, int expectedBytes) throws IOException {
        if (!file.isFile() || file.length() != expectedBytes) return null;
        byte[] bytes = new byte[expectedBytes];
        try (FileInputStream input = new FileInputStream(file)) {
            int offset = 0;
            while (offset < bytes.length) {
                int count = input.read(bytes, offset, bytes.length - offset);
                if (count < 0) return null;
                offset += count;
            }
        }
        return bytes;
    }

    private static void copyIfPresent(File source, File destination) throws IOException {
        if (source.isFile()) Files.copy(source.toPath(), destination.toPath(),
                StandardCopyOption.REPLACE_EXISTING);
    }

    private static void replaceAtomically(File destination, byte[] bytes) throws IOException {
        File temporary = new File(destination.getParentFile(), destination.getName() + ".importing");
        writeSynced(temporary, bytes);
        try {
            Files.move(temporary.toPath(), destination.toPath(), StandardCopyOption.REPLACE_EXISTING,
                    StandardCopyOption.ATOMIC_MOVE);
        } catch (AtomicMoveNotSupportedException ignored) {
            Files.move(temporary.toPath(), destination.toPath(), StandardCopyOption.REPLACE_EXISTING);
        } finally {
            if (temporary.exists()) temporary.delete();
        }
    }

    private static void writeSynced(File destination, byte[] bytes) throws IOException {
        File parent = destination.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs())
            throw new IOException("Nao foi possivel criar " + parent.getName() + ".");
        try (FileOutputStream output = new FileOutputStream(destination)) {
            output.write(bytes);
            output.flush();
            output.getFD().sync();
        }
    }

    private static long crc32(byte[] bytes, int offset, int length) {
        CRC32 crc = new CRC32();
        crc.update(bytes, offset, length);
        return crc.getValue();
    }
}
