package com.pokegochi.app;

import android.view.Surface;
import java.util.concurrent.locks.ReentrantReadWriteLock;

/** JNI handle for the actual firmware setup()/loop() runtime. */
final class NativeFirmware implements AutoCloseable {
    static {
        System.loadLibrary("pokegochi_core");
    }

    private long handle;
    // JNI subsystems already own independent native mutexes (runtime, frame,
    // surface and multiplayer). A synchronized Java wrapper serialized all of
    // them again: a touch waiting for a blocking battle loop held this monitor
    // for the whole move and prevented the renderer from consuming any frame.
    // Read locks keep the native calls alive concurrently; close() alone takes
    // the write lock so the session cannot be freed under an in-flight call.
    private final ReentrantReadWriteLock lifecycleLock = new ReentrantReadWriteLock();

    private NativeFirmware(long handle) {
        this.handle = handle;
    }

    static NativeFirmware open(String storageRoot, String preferencesRoot,
                               String logPath, String crashPath, String buildId) {
        long handle = create(storageRoot, preferencesRoot, logPath, crashPath, buildId);
        return handle == 0 ? null : new NativeFirmware(handle);
    }

    void touch(int x, int y, boolean pressed) {
        lifecycleLock.readLock().lock();
        try {
            if (handle != 0) touch(handle, x, y, pressed);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    void setSurface(Surface surface) {
        lifecycleLock.readLock().lock();
        try {
            if (handle != 0) setSurface(handle, surface);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    boolean presentFrame() {
        lifecycleLock.readLock().lock();
        try {
            return handle != 0 && presentFrame(handle);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    boolean isAlive() {
        lifecycleLock.readLock().lock();
        try {
            return handle != 0 && isAlive(handle);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    void setPhoneBatteryPercent(int percent) {
        lifecycleLock.readLock().lock();
        try {
            if (handle != 0) setPhoneBatteryPercent(handle, percent);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    void syncPhoneTime(long epochSeconds) {
        lifecycleLock.readLock().lock();
        try {
            if (handle != 0) syncPhoneTime(handle, epochSeconds);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    void flushSave() {
        lifecycleLock.readLock().lock();
        try {
            if (handle != 0) flushSave(handle);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    int multiplayerPlayerId() {
        lifecycleLock.readLock().lock();
        try {
            return handle == 0 ? 0 : multiplayerPlayerId(handle);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    boolean multiplayerEnabled() {
        lifecycleLock.readLock().lock();
        try {
            return handle != 0 && multiplayerEnabled(handle);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    int multiplayerState() {
        lifecycleLock.readLock().lock();
        try {
            return handle == 0 ? 0 : multiplayerState(handle);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    int multiplayerPeerId() {
        lifecycleLock.readLock().lock();
        try {
            return handle == 0 ? 0 : multiplayerPeerId(handle);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    boolean multiplayerConnected() {
        lifecycleLock.readLock().lock();
        try {
            return handle != 0 && multiplayerConnected(handle);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    byte[] multiplayerTakeOutgoing() {
        lifecycleLock.readLock().lock();
        try {
            return handle == 0 ? null : multiplayerTakeOutgoing(handle);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    void multiplayerReceive(byte[] packet) {
        lifecycleLock.readLock().lock();
        try {
            if (handle != 0 && packet != null) multiplayerReceive(handle, packet);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    void multiplayerDiscovered(int playerId, int rssi, String address, int addressType) {
        lifecycleLock.readLock().lock();
        try {
            if (handle != 0 && address != null)
                multiplayerDiscovered(handle, playerId, rssi, address, addressType);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    void multiplayerTransportConnected(boolean outbound) {
        lifecycleLock.readLock().lock();
        try {
            if (handle != 0) multiplayerTransportConnected(handle, outbound);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    void multiplayerDisconnected() {
        lifecycleLock.readLock().lock();
        try {
            if (handle != 0) multiplayerDisconnected(handle);
        } finally {
            lifecycleLock.readLock().unlock();
        }
    }

    @Override public void close() {
        lifecycleLock.writeLock().lock();
        try {
            if (handle != 0) {
                destroy(handle);
                handle = 0;
            }
        } finally {
            lifecycleLock.writeLock().unlock();
        }
    }

    private static native long create(String storageRoot, String preferencesRoot,
                                      String logPath, String crashPath, String buildId);
    private static native void destroy(long handle);
    private static native void touch(long handle, int x, int y, boolean pressed);
    private static native void setSurface(long handle, Surface surface);
    private static native boolean presentFrame(long handle);
    private static native boolean isAlive(long handle);
    private static native void setPhoneBatteryPercent(long handle, int percent);
    private static native void syncPhoneTime(long handle, long epochSeconds);
    private static native void flushSave(long handle);
    private static native int multiplayerPlayerId(long handle);
    private static native boolean multiplayerEnabled(long handle);
    private static native int multiplayerState(long handle);
    private static native int multiplayerPeerId(long handle);
    private static native boolean multiplayerConnected(long handle);
    private static native byte[] multiplayerTakeOutgoing(long handle);
    private static native void multiplayerReceive(long handle, byte[] packet);
    private static native void multiplayerDiscovered(long handle, int playerId, int rssi,
                                                     String address, int addressType);
    private static native void multiplayerTransportConnected(long handle, boolean outbound);
    private static native void multiplayerDisconnected(long handle);
}
