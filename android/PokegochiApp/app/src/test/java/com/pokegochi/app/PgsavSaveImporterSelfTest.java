package com.pokegochi.app;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.zip.CRC32;

public final class PgsavSaveImporterSelfTest {
    public static void main(String[] args) throws Exception {
        byte[] payload = new byte[33584];
        for (int index = 0; index < payload.length; ++index) payload[index] = (byte) (index * 17);
        byte[] container = container(payload, 77736);
        PgsavSaveImporter.Parsed parsed = PgsavSaveImporter.parse(container);
        check(parsed.saveVersion == 44 && parsed.sourcePlayerId == 77736, "header parse");
        check(Arrays.equals(payload, parsed.payload), "payload parse");

        byte[] corrupted = container.clone();
        corrupted[corrupted.length - 1] ^= 1;
        try {
            PgsavSaveImporter.parse(corrupted);
            throw new AssertionError("corrupt payload accepted");
        } catch (IOException expected) {}

        File root = Files.createTempDirectory("pokegochi-save-import").toFile();
        File preferences = new File(root, "preferences");
        File namespace = new File(preferences, "pokegochi");
        check(namespace.mkdirs(), "namespace creation");
        Files.write(new File(namespace, "save_a.bin").toPath(),
                PgsavSaveImporter.buildRecord(payload, 44, 8));
        PgsavSaveImporter.InstallResult installed = PgsavSaveImporter.install(
                preferences, new File(root, "backups"), parsed);
        check(installed.firstSequence == 9 && installed.secondSequence == 10, "sequence advance");
        check(new File(namespace, "save_a.bin").length() == container.length, "record A size");
        check(new File(namespace, "save_b.bin").length() == container.length, "record B size");
        check(new File(installed.backupDirectory, "save_a.bin").isFile(), "previous save backup");
        check(new File(installed.backupDirectory, "imported-source.pgsav").isFile(), "source backup");
        if (args.length > 0) {
            try (FileInputStream input = new FileInputStream(args[0])) {
                PgsavSaveImporter.Parsed actual = PgsavSaveImporter.read(input);
                check(actual.saveVersion == 44, "actual save version");
                System.out.println("Actual PGS1: player=" + actual.sourcePlayerId +
                        " payload=" + actual.payload.length + " bytes");
            }
        }
        System.out.println("PGS1 Android import vectors: OK");
    }

    private static byte[] container(byte[] payload, int playerId) {
        byte[] bytes = new byte[24 + payload.length];
        ByteBuffer buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        buffer.put((byte) 'P').put((byte) 'G').put((byte) 'S').put((byte) '1');
        buffer.putShort((short) 1).putShort((short) 44).putInt(payload.length).putInt(playerId);
        buffer.putInt((int) crc32(payload, 0, payload.length));
        buffer.putInt((int) crc32(bytes, 0, 20));
        buffer.put(payload);
        return bytes;
    }

    private static long crc32(byte[] bytes, int offset, int length) {
        CRC32 crc = new CRC32();
        crc.update(bytes, offset, length);
        return crc.getValue();
    }

    private static void check(boolean value, String label) {
        if (!value) throw new AssertionError(label);
    }
}
