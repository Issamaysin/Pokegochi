package com.pokegochi.updater;

import android.content.Context;
import android.net.Uri;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

final class UpdatePackage implements AutoCloseable {
    static final int MANIFEST_SIZE = 276;
    static final String FIRMWARE_ENTRY = "payload/firmware.bin";

    static final class Payload {
        final int kind;
        final int assetId;
        final String entryName;
        final long size;
        final byte[] sha256;

        Payload(int kind, int assetId, String entryName, long size, byte[] sha256) {
            this.kind = kind;
            this.assetId = assetId;
            this.entryName = entryName;
            this.size = size;
            this.sha256 = sha256;
        }
    }

    final byte[] manifest;
    final long packageSequence;
    final int assetPackVersion;
    final List<Payload> payloads;
    final long totalBytes;
    final String displayName;
    private final File localFile;
    private final ZipFile zip;

    private UpdatePackage(File localFile, ZipFile zip, byte[] manifest, long sequence,
                          int assetPackVersion, List<Payload> payloads, long totalBytes) {
        this.localFile = localFile;
        this.zip = zip;
        this.manifest = manifest;
        this.packageSequence = sequence;
        this.assetPackVersion = assetPackVersion;
        this.payloads = payloads;
        this.totalBytes = totalBytes;
        this.displayName = "Update " + sequence + " / assets " + assetPackVersion;
    }

    static UpdatePackage load(Context context, Uri source, Progress progress) throws Exception {
        File local = new File(context.getCacheDir(), "selected-update.pgota");
        try (InputStream input = context.getContentResolver().openInputStream(source);
             FileOutputStream output = new FileOutputStream(local, false)) {
            if (input == null) throw new IOException("The selected package could not be opened");
            byte[] buffer = new byte[64 * 1024];
            int count;
            while ((count = input.read(buffer)) >= 0) output.write(buffer, 0, count);
        }
        ZipFile zip = new ZipFile(local);
        try {
            byte[] manifest = readExact(zip, "manifest.bin", MANIFEST_SIZE);
            ByteBuffer value = ByteBuffer.wrap(manifest).order(ByteOrder.LITTLE_ENDIAN);
            byte[] magic = new byte[4];
            value.get(magic);
            int protocol = value.get() & 0xff;
            int model = value.get() & 0xff;
            int structureSize = value.getShort() & 0xffff;
            long sequence = Integer.toUnsignedLong(value.getInt());
            long firmwareSize = Integer.toUnsignedLong(value.getInt());
            int packVersion = value.getShort() & 0xffff;
            int assetCount = value.get() & 0xff;
            value.get(); // flags
            byte[] firmwareSha = new byte[32];
            value.get(firmwareSha);
            if (!Arrays.equals(magic, new byte[]{'P', 'G', 'U', '1'}) ||
                    protocol != UpdateProtocol.VERSION || model != UpdateProtocol.DEVICE_MODEL ||
                    structureSize != MANIFEST_SIZE || assetCount != 4) {
                throw new IOException("This is not a compatible Pokegochi update package");
            }
            List<Payload> payloads = new ArrayList<>();
            boolean[] seen = new boolean[5];
            long total = firmwareSize;
            String[] names = {null, "payload/assets/pokegochi.pak",
                    "payload/assets/pack_version.txt", "payload/assets/animation_catalog.txt",
                    "payload/assets/manifest.json"};
            for (int index = 0; index < 4; ++index) {
                int id = value.get() & 0xff;
                value.position(value.position() + 3);
                long size = Integer.toUnsignedLong(value.getInt());
                byte[] hash = new byte[32];
                value.get(hash);
                if (id < 1 || id > 4 || seen[id] || size == 0) throw new IOException("Invalid asset table");
                seen[id] = true;
                payloads.add(new Payload(UpdateProtocol.OBJECT_ASSET, id, names[id], size, hash));
                total += size;
            }
            // Assets are staged first. Firmware is deliberately last, so a
            // cancelled transfer never erases an OTA partition unnecessarily.
            payloads.add(new Payload(UpdateProtocol.OBJECT_FIRMWARE, 0,
                    FIRMWARE_ENTRY, firmwareSize, firmwareSha));
            int verified = 0;
            for (Payload payload : payloads) {
                progress.message("Checking " + payload.entryName + "...");
                verifyEntry(zip, payload);
                progress.value(++verified, payloads.size());
            }
            return new UpdatePackage(local, zip, manifest, sequence, packVersion, payloads, total);
        } catch (Exception error) {
            zip.close();
            local.delete();
            throw error;
        }
    }

    InputStream open(Payload payload, long offset) throws IOException {
        ZipEntry entry = zip.getEntry(payload.entryName);
        if (entry == null) throw new IOException("Missing " + payload.entryName);
        InputStream input = zip.getInputStream(entry);
        long remaining = offset;
        while (remaining > 0) {
            long skipped = input.skip(remaining);
            if (skipped > 0) { remaining -= skipped; continue; }
            if (input.read() < 0) throw new IOException("Could not resume " + payload.entryName);
            --remaining;
        }
        return input;
    }

    @Override public void close() throws IOException {
        zip.close();
        localFile.delete();
    }

    private static byte[] readExact(ZipFile zip, String name, int expected) throws IOException {
        ZipEntry entry = zip.getEntry(name);
        if (entry == null || entry.getSize() != expected) throw new IOException("Missing or invalid " + name);
        byte[] bytes = new byte[expected];
        try (InputStream input = zip.getInputStream(entry)) {
            int offset = 0;
            while (offset < bytes.length) {
                int count = input.read(bytes, offset, bytes.length - offset);
                if (count < 0) throw new IOException("Truncated " + name);
                offset += count;
            }
        }
        return bytes;
    }

    private static void verifyEntry(ZipFile zip, Payload payload) throws Exception {
        ZipEntry entry = zip.getEntry(payload.entryName);
        if (entry == null || entry.getSize() != payload.size) throw new IOException("Missing " + payload.entryName);
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        long bytes = 0;
        try (InputStream input = zip.getInputStream(entry)) {
            byte[] buffer = new byte[64 * 1024];
            int count;
            while ((count = input.read(buffer)) >= 0) {
                digest.update(buffer, 0, count);
                bytes += count;
            }
        }
        if (bytes != payload.size || !MessageDigest.isEqual(digest.digest(), payload.sha256))
            throw new IOException("SHA-256 failed for " + payload.entryName);
    }

    interface Progress {
        void message(String text);
        void value(int current, int total);
    }
}
