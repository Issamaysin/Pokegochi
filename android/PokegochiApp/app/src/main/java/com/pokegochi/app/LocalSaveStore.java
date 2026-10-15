package com.pokegochi.app;

import android.content.Context;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.zip.CRC32;

/** Private app save. It deliberately has no export, import or reset operation. */
final class LocalSaveStore {
    private static final int MAGIC = 0x50474131; // PGA1
    private static final int VERSION = 1;
    private final File file;

    LocalSaveStore(Context context) { file = new File(context.getFilesDir(), "pokegochi.save"); }

    SaveState load() {
        if (!file.isFile()) return SaveState.newGame();
        try (DataInputStream input = new DataInputStream(new FileInputStream(file))) {
            if (input.readInt() != MAGIC || input.readInt() != VERSION) return SaveState.newGame();
            int length = input.readInt();
            if (length < 32) return SaveState.newGame();
            byte[] payload = input.readNBytes(length);
            long expected = Integer.toUnsignedLong(input.readInt());
            CRC32 crc = new CRC32(); crc.update(payload);
            if (crc.getValue() != expected) return SaveState.newGame();
            return SaveState.decode(payload);
        } catch (IOException | RuntimeException ignored) {
            return SaveState.newGame();
        }
    }

    boolean commit(SaveState state) {
        File temporary = new File(file.getParentFile(), file.getName() + ".tmp");
        byte[] payload = state.encode();
        CRC32 crc = new CRC32(); crc.update(payload);
        try (FileOutputStream stream = new FileOutputStream(temporary);
             DataOutputStream output = new DataOutputStream(stream)) {
            output.writeInt(MAGIC); output.writeInt(VERSION); output.writeInt(payload.length);
            output.write(payload); output.writeInt((int) crc.getValue()); output.flush();
            stream.getFD().sync();
        } catch (IOException error) { return false; }
        return temporary.renameTo(file);
    }

    static final class SaveState {
        int starterSpecies;
        int level;
        int experience;
        int currentHp;
        int maximumHp;
        int money;
        int badges;
        int playerId;
        long playTimeSeconds;
        byte[] nativeState;

        static SaveState newGame() {
            SaveState state = new SaveState();
            state.level = 5; state.currentHp = state.maximumHp = 20; state.money = 3000;
            state.playerId = 10000 + (int) (Math.random() * 90000);
            return state;
        }

        byte[] encode() {
            byte[] nativeBytes = nativeState == null ? new byte[0] : nativeState;
            java.nio.ByteBuffer buffer = java.nio.ByteBuffer.allocate(36 + nativeBytes.length).order(java.nio.ByteOrder.LITTLE_ENDIAN);
            buffer.putInt(starterSpecies).putInt(level).putInt(experience).putInt(currentHp)
                    .putInt(maximumHp).putInt(money).putInt(badges).putInt(playerId);
            buffer.putInt(nativeBytes.length).put(nativeBytes);
            return buffer.array();
        }

        static SaveState decode(byte[] bytes) {
            java.nio.ByteBuffer buffer = java.nio.ByteBuffer.wrap(bytes).order(java.nio.ByteOrder.LITTLE_ENDIAN);
            SaveState state = new SaveState();
            state.starterSpecies = buffer.getInt(); state.level = buffer.getInt(); state.experience = buffer.getInt();
            state.currentHp = buffer.getInt(); state.maximumHp = buffer.getInt(); state.money = buffer.getInt();
            state.badges = buffer.getInt(); state.playerId = buffer.getInt();
            if (buffer.remaining() >= 4) {
                int nativeLength = buffer.getInt();
                if (nativeLength >= 0 && nativeLength <= buffer.remaining()) {
                    state.nativeState = new byte[nativeLength]; buffer.get(state.nativeState);
                }
            }
            return state;
        }
    }
}
