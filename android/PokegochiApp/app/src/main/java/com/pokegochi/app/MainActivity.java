package com.pokegochi.app;

import android.Manifest;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.ContentValues;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.AssetManager;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.net.Uri;
import android.os.Bundle;
import android.os.BatteryManager;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Environment;
import android.provider.MediaStore;
import android.content.pm.PackageManager;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.TimeZone;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.TimeUnit;
import java.nio.charset.StandardCharsets;

/** Displays the firmware's 320x240 framebuffer and forwards its touch input. */
public final class MainActivity extends Activity {
    // The asset archive is versioned separately for the ESP32 updater. The
    // Android app must also invalidate its private extracted copy when a new
    // APK ships the same pack format with corrected move sprites or firmware
    // rendering code.
    private static final String ANDROID_ASSET_REVISION =
            BuildConfig.VERSION_NAME + "-" + BuildConfig.VERSION_CODE;
    private static final String NATIVE_BUILD_ID =
            "android-app-" + BuildConfig.VERSION_NAME + "-r233-" + BuildConfig.BUILD_TYPE;
    // Release builds must not create private or public diagnostic artifacts.
    // Debug builds keep the existing dump path for development diagnostics.
    private static final boolean DIAGNOSTICS_ENABLED =
            !"release".equals(BuildConfig.BUILD_TYPE);
    private FirmwareView firmwareView;
    private volatile NativeFirmware firmware;
    private BleMultiplayer bleMultiplayer;
    private ExecutorService initializationExecutor;
    private volatile boolean destroyed;
    private BroadcastReceiver phoneBatteryReceiver;
    private boolean phoneBatteryReceiverRegistered;
    private BroadcastReceiver phoneClockReceiver;
    private boolean phoneClockReceiverRegistered;
    private File pendingDiagnosticDump;
    private boolean diagnosticSessionExported;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        // Android owns the mobile display lifecycle. The firmware's physical
        // screen timeout is disabled in the Android build, and this flag keeps
        // the APK visible while an authored battle animation is playing.
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        hideSystemUi();
        if (DIAGNOSTICS_ENABLED) pendingDiagnosticDump = capturePreviousDiagnosticDump();
        // Never keep Android's launch splash on screen while the firmware
        // copies its SD image and runs setup(). The real device can be much
        // slower than the emulator, so all of that work happens off the UI
        // thread and the framebuffer is attached only after boot completes.
        setContentView(new StartupView());
        initializationExecutor = Executors.newSingleThreadExecutor(runnable -> {
            Thread thread = new Thread(runnable, "PokegochiInit");
            thread.setPriority(Thread.NORM_PRIORITY);
            return thread;
        });
        initializationExecutor.execute(this::initializeFirmware);
        if (DIAGNOSTICS_ENABLED && pendingDiagnosticDump != null) {
            new Handler(getMainLooper()).postDelayed(
                    () -> offerDiagnosticShare(pendingDiagnosticDump), 1200L);
        }
    }

    private void initializeFirmware() {
        NativeFirmware opened = null;
        try {
            copyFirmwareAssets();
            final String runtimeLogPath = DIAGNOSTICS_ENABLED
                    ? runtimeLogFile().getAbsolutePath() : "";
            final String nativeCrashPath = DIAGNOSTICS_ENABLED
                    ? nativeCrashFile().getAbsolutePath() : "";
            opened = NativeFirmware.open(
                    new File(getFilesDir(), "sd").getAbsolutePath(),
                    new File(getFilesDir(), "preferences").getAbsolutePath(),
                    runtimeLogPath, nativeCrashPath, NATIVE_BUILD_ID);
            if (opened == null) throw new IllegalStateException("Runtime nativo indisponivel");
            // Reconcile the persisted game clock before the worker is exposed
            // to the UI. This applies closed-app progression without adding a
            // second Android-only copy of the game timers.
            syncPhoneClock(opened);
            final NativeFirmware readyFirmware = opened;
            runOnUiThread(() -> finishInitialization(readyFirmware));
            opened = null;
        } catch (Throwable error) {
            android.util.Log.e("Pokegochi", "Falha ao iniciar o runtime do firmware", error);
            if (opened != null) opened.close();
            runOnUiThread(() -> showInitializationError(error));
        }
    }

    private void finishInitialization(NativeFirmware readyFirmware) {
        if (destroyed || isFinishing()) {
            readyFirmware.close();
            return;
        }
        firmware = readyFirmware;
        updatePhoneBattery();
        registerPhoneClockReceiver();
        phoneBatteryReceiver = new BroadcastReceiver() {
            @Override public void onReceive(Context context, Intent intent) {
                updatePhoneBattery();
            }
        };
        try {
            if (android.os.Build.VERSION.SDK_INT >= 33) {
                registerReceiver(phoneBatteryReceiver,
                        new IntentFilter(Intent.ACTION_BATTERY_CHANGED),
                        Context.RECEIVER_EXPORTED);
            } else {
                registerReceiver(phoneBatteryReceiver,
                        new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
            }
            phoneBatteryReceiverRegistered = true;
        } catch (SecurityException error) {
            // Battery display is optional on Android. A device policy must
            // never turn a successful firmware boot into an app crash.
            android.util.Log.w("Pokegochi", "Nao foi possivel observar a bateria", error);
            phoneBatteryReceiver = null;
        }
        firmwareView = new FirmwareView();
        android.widget.FrameLayout gameRoot = new android.widget.FrameLayout(this);
        gameRoot.setBackgroundColor(Color.BLACK);
        android.widget.FrameLayout.LayoutParams gameLayout =
                new android.widget.FrameLayout.LayoutParams(
                        android.view.ViewGroup.LayoutParams.WRAP_CONTENT,
                        android.view.ViewGroup.LayoutParams.WRAP_CONTENT,
                        android.view.Gravity.CENTER);
        gameRoot.addView(firmwareView, gameLayout);
        setContentView(gameRoot);
        bleMultiplayer = new BleMultiplayer(this, readyFirmware,
                new BleMultiplayer.Listener() {
                    @Override public void onStatus(String status) {
                        android.util.Log.i("PokegochiBLE", status);
                    }

                    @Override public void onPermissionsRequired() {
                        runOnUiThread(MainActivity.this::requestBluetoothPermissions);
                    }
                });
        bleMultiplayer.start();
    }

    private static void syncPhoneClock(NativeFirmware target) {
        if (target == null) return;
        final long nowMillis = System.currentTimeMillis();
        final long utcEpochSeconds = nowMillis / 1000L;
        // Persist UTC for exact offline progression and apply the phone's
        // current zone only while rendering the local civil clock.
        final int utcOffsetSeconds = TimeZone.getDefault().getOffset(nowMillis) / 1000;
        target.syncPhoneTime(utcEpochSeconds, utcOffsetSeconds);
    }

    private void requestPhoneClockSync() {
        final NativeFirmware target = firmware;
        final ExecutorService executor = initializationExecutor;
        if (target == null || executor == null || executor.isShutdown()) return;
        executor.execute(() -> {
            if (!destroyed && firmware == target) syncPhoneClock(target);
        });
    }

    private void registerPhoneClockReceiver() {
        if (phoneClockReceiverRegistered) return;
        phoneClockReceiver = new BroadcastReceiver() {
            @Override public void onReceive(Context context, Intent intent) {
                requestPhoneClockSync();
            }
        };
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_TIME_CHANGED);
        filter.addAction(Intent.ACTION_TIMEZONE_CHANGED);
        filter.addAction(Intent.ACTION_DATE_CHANGED);
        try {
            if (android.os.Build.VERSION.SDK_INT >= 33) {
                registerReceiver(phoneClockReceiver, filter, Context.RECEIVER_EXPORTED);
            } else {
                registerReceiver(phoneClockReceiver, filter);
            }
            phoneClockReceiverRegistered = true;
        } catch (RuntimeException error) {
            android.util.Log.w("Pokegochi", "Nao foi possivel observar o relogio", error);
            phoneClockReceiver = null;
        }
    }

    private void showInitializationError(Throwable error) {
        if (destroyed || isFinishing()) return;
        setContentView(new StartupView("POKEGOCHI\nNAO FOI POSSIVEL INICIAR"));
        android.widget.Toast.makeText(this, "Pokegochi nao conseguiu iniciar", android.widget.Toast.LENGTH_LONG).show();
        if (!DIAGNOSTICS_ENABLED) return;
        File dump = writeDiagnosticDump("initialization failure: " + error);
        if (dump != null)
            new Handler(getMainLooper()).postDelayed(() -> offerDiagnosticShare(dump), 400L);
    }

    private void requestBluetoothPermissions() {
        if (android.os.Build.VERSION.SDK_INT >= 31) {
            java.util.ArrayList<String> missing = new java.util.ArrayList<>();
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED)
                missing.add(Manifest.permission.BLUETOOTH_SCAN);
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED)
                missing.add(Manifest.permission.BLUETOOTH_CONNECT);
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_ADVERTISE) != PackageManager.PERMISSION_GRANTED)
                missing.add(Manifest.permission.BLUETOOTH_ADVERTISE);
            if (!missing.isEmpty()) requestPermissions(missing.toArray(new String[0]), 1001);
        } else if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION}, 1001);
        }
    }

    private void copyFirmwareAssets() throws IOException {
        File root = new File(getFilesDir(), "sd");
        File archive = new File(root, "pokegochi/pokegochi.pak");
        File installedVersion = new File(root, "pokegochi/assets/pack_version.txt");
        File installedCatalog = new File(root, "pokegochi/assets/animation_catalog.txt");
        File installedManifest = new File(root, "pokegochi/assets/manifest.json");
        File installedAndroidRevision = new File(root,
                "pokegochi/assets/android_runtime_revision.txt");
        String packagedVersion;
        try (InputStream input = getAssets().open("pokegochi/assets/pack_version.txt")) {
            packagedVersion = readVersion(input);
        }
        if (archive.isFile() && archive.length() > 0 && installedVersion.isFile() &&
                installedCatalog.isFile() && installedManifest.isFile() &&
                installedAndroidRevision.isFile() &&
                packagedVersion.equals(readVersion(installedVersion)) &&
                ANDROID_ASSET_REVISION.equals(readVersion(installedAndroidRevision))) return;
        // The generated asset directory also contains thousands of loose PKG
        // mirrors for the ESP32 development card. Android must use the
        // canonical archive instead: copying 8,000+ individual files on the
        // first launch leaves the Android splash screen visible for minutes.
        copyAssetFile("pokegochi/pokegochi.pak", archive);
        copyAssetFile("pokegochi/assets/pack_version.txt", installedVersion);
        copyAssetFile("pokegochi/assets/animation_catalog.txt",
                new File(root, "pokegochi/assets/animation_catalog.txt"));
        copyAssetFile("pokegochi/assets/manifest.json",
                new File(root, "pokegochi/assets/manifest.json"));
        try (FileOutputStream output = new FileOutputStream(installedAndroidRevision)) {
            output.write(ANDROID_ASSET_REVISION.getBytes(StandardCharsets.US_ASCII));
            output.write('\n');
        }
    }

    private String readVersion(InputStream input) throws IOException {
        byte[] bytes = new byte[32];
        int count = 0;
        while (count < bytes.length) {
            int read = input.read(bytes, count, bytes.length - count);
            if (read < 0) break;
            if (read == 0) continue;
            count += read;
        }
        return new String(bytes, 0, count, StandardCharsets.US_ASCII).trim();
    }

    private String readVersion(File file) throws IOException {
        try (InputStream input = new FileInputStream(file)) {
            return readVersion(input);
        }
    }

    private void copyAssetFile(String assetPath, File destination) throws IOException {
        AssetManager assets = getAssets();
        File parent = destination.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("Nao foi possivel criar " + parent);
        }
        try (InputStream input = assets.open(assetPath);
             FileOutputStream output = new FileOutputStream(destination)) {
            byte[] buffer = new byte[64 * 1024];
            int count;
            while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
        }
    }

    private File diagnosticsDirectory() {
        if (!DIAGNOSTICS_ENABLED) return null;
        File directory = new File(getFilesDir(), "diagnostics");
        if (!directory.exists() && !directory.mkdirs()) return null;
        return directory;
    }

    private File runtimeLogFile() {
        return new File(getFilesDir(), "pokegochi_runtime.log");
    }

    private File nativeCrashFile() {
        return new File(getFilesDir(), "pokegochi_native_crash.log");
    }

    private File capturePreviousDiagnosticDump() {
        if (!DIAGNOSTICS_ENABLED) return null;
        File marker = nativeCrashFile();
        if (!marker.isFile() || marker.length() == 0) return null;
        return writeDiagnosticDump("previous native crash detected");
    }

    private File writeDiagnosticDump(String reason) {
        if (!DIAGNOSTICS_ENABLED) return null;
        File directory = diagnosticsDirectory();
        if (directory == null) return null;
        File dump = new File(directory, "Pokegochi-dump-" + NATIVE_BUILD_ID + ".txt");
        File marker = nativeCrashFile();
        int crashPid = readCrashPid(marker);
        if (crashPid <= 0) crashPid = android.os.Process.myPid();
        try (FileOutputStream output = new FileOutputStream(dump, false)) {
            String header = "POKEGOCHI ANDROID DIAGNOSTIC DUMP\n" +
                    "version=" + ANDROID_ASSET_REVISION + "\n" +
                    "versionCode=" + BuildConfig.VERSION_CODE + "\n" +
                    "nativeBuildId=" + NATIVE_BUILD_ID + "\n" +
                    "reason=" + reason + "\n" +
                    "crashPid=" + crashPid + "\n" +
                    "dumpPid=" + android.os.Process.myPid() + "\n" +
                    "device=" + android.os.Build.MANUFACTURER + " " + android.os.Build.MODEL + "\n" +
                    "sdk=" + android.os.Build.VERSION.SDK_INT + " abi=" +
                    android.os.Build.SUPPORTED_ABIS[0] + "\n\n";
            output.write(header.getBytes(StandardCharsets.UTF_8));
            appendDiagnosticFile(output, marker, "native crash marker");
            appendDiagnosticFile(output, runtimeLogFile(), "native runtime log");
            output.write("\n--- filtered logcat ---\n".getBytes(StandardCharsets.UTF_8));
            appendFilteredLogcat(output, crashPid);
            output.write(("\n--- diagnostic rule ---\n" +
                "the crash buffer is included with native process lines so debuggerd/tombstone frames are not discarded.\n")
                    .getBytes(StandardCharsets.UTF_8));
            output.flush();
        } catch (Throwable error) {
            android.util.Log.e("Pokegochi", "Nao foi possivel criar dump diagnostico", error);
            return null;
        }
        exportDiagnosticToDownloads(dump);
        return dump;
    }

    private int readCrashPid(File marker) {
        if (marker == null || !marker.isFile()) return 0;
        try (InputStream input = new FileInputStream(marker)) {
            byte[] bytes = new byte[4096];
            int count = input.read(bytes);
            if (count <= 0) return 0;
            String text = new String(bytes, 0, count, StandardCharsets.UTF_8);
            String key = "pid=";
            int start = text.indexOf(key);
            if (start < 0) return 0;
            start += key.length();
            int end = start;
            while (end < text.length() && Character.isDigit(text.charAt(end))) end++;
            return Integer.parseInt(text.substring(start, end));
        } catch (Throwable ignored) {
            return 0;
        }
    }

    private void appendFilteredLogcat(OutputStream output, int pid)
            throws IOException, InterruptedException {
        Process process = new ProcessBuilder(
                "logcat", "-d", "-v", "threadtime", "-b", "main", "-b", "system",
                "-b", "crash", "-s", "Pokegochi:*", "PokegochiFirmware:*",
                "AndroidRuntime:*", "libc:*", "DEBUG:*")
                .redirectErrorStream(true).start();
        int remaining = 768 * 1024;
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                process.getInputStream(), StandardCharsets.UTF_8))) {
            String line;
            while (remaining > 0 && (line = reader.readLine()) != null) {
                if (pid > 0 && !logcatLineBelongsToCrash(line, pid)) continue;
                byte[] bytes = (line + "\n").getBytes(StandardCharsets.UTF_8);
                int length = Math.min(bytes.length, remaining);
                output.write(bytes, 0, length);
                remaining -= length;
            }
        }
        if (!process.waitFor(4L, TimeUnit.SECONDS)) process.destroyForcibly();
    }

    private boolean logcatLineHasPid(String line, int pid) {
        String token = " " + pid + " ";
        return line != null && line.contains(token);
    }

    private boolean logcatLineBelongsToCrash(String line, int pid) {
        if (logcatLineHasPid(line, pid)) return true;
        if (line == null) return false;
        // debuggerd writes the tombstone from its own process. Keep the
        // native crash vocabulary even when its PID is not the app PID.
        String lower = line.toLowerCase(java.util.Locale.ROOT);
        return lower.contains("fatal signal") || lower.contains("backtrace") ||
                lower.contains("#00 pc") || lower.contains("pokegochiinit") ||
                lower.contains("libpokegochi_core.so") || lower.contains("tombstone");
    }

    private void appendDiagnosticFile(OutputStream output, File file, String title)
            throws IOException {
        output.write(("\n--- " + title + " ---\n").getBytes(StandardCharsets.UTF_8));
        if (!file.isFile()) {
            output.write("(arquivo ausente)\n".getBytes(StandardCharsets.UTF_8));
            return;
        }
        try (InputStream input = new FileInputStream(file)) {
            copyStream(input, output, 512 * 1024);
        }
    }

    private void copyStream(InputStream input, OutputStream output, int maximum)
            throws IOException {
        byte[] buffer = new byte[8192];
        int remaining = maximum;
        while (remaining > 0) {
            int read = input.read(buffer, 0, Math.min(buffer.length, remaining));
            if (read < 0) break;
            if (read == 0) continue;
            output.write(buffer, 0, read);
            remaining -= read;
        }
    }

    /**
     * Keep the private copy for the in-app share provider, but also publish a
     * user-visible copy on modern Android. MediaStore handles this without a
     * broad storage permission and makes the file available in Downloads.
     */
    private void exportDiagnosticToDownloads(File dump) {
        if (!DIAGNOSTICS_ENABLED || android.os.Build.VERSION.SDK_INT < 29 ||
                dump == null || !dump.isFile()) return;
        ContentValues values = new ContentValues();
        values.put(MediaStore.Downloads.DISPLAY_NAME, dump.getName());
        values.put(MediaStore.Downloads.MIME_TYPE, "text/plain");
        values.put(MediaStore.Downloads.RELATIVE_PATH,
                Environment.DIRECTORY_DOWNLOADS + "/Pokegochi");
        values.put(MediaStore.Downloads.IS_PENDING, 1);
        Uri uri = getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
        if (uri == null) {
            android.util.Log.w("Pokegochi", "Nao foi possivel criar dump em Downloads");
            return;
        }
        try (InputStream input = new FileInputStream(dump);
             OutputStream output = getContentResolver().openOutputStream(uri)) {
            if (output == null) throw new IOException("saida MediaStore indisponivel");
            copyStream(input, output, 2 * 1024 * 1024);
            output.flush();
            ContentValues ready = new ContentValues();
            ready.put(MediaStore.Downloads.IS_PENDING, 0);
            getContentResolver().update(uri, ready, null, null);
        } catch (Throwable error) {
            getContentResolver().delete(uri, null, null);
            android.util.Log.w("Pokegochi", "Nao foi possivel finalizar dump em Downloads", error);
        }
    }

    private void offerDiagnosticShare(File dump) {
        if (!DIAGNOSTICS_ENABLED || dump == null || destroyed || isFinishing() ||
                !dump.isFile()) return;
        new android.app.AlertDialog.Builder(this)
                .setTitle("Crash anterior detectado")
                .setMessage("O Pokegochi salvou um dump completo do runtime Android. " +
                        "Ele tambem esta em Downloads/Pokegochi. Compartilhe o arquivo " +
                        "para identificar a causa sem resetar o save.")
                .setNegativeButton("Agora nao", null)
                .setPositiveButton("Compartilhar log", (dialog, which) -> shareDiagnosticDump(dump))
                .show();
    }

    private void shareDiagnosticDump(File dump) {
        Uri uri = Uri.parse("content://" + getPackageName() + ".diagnostics/" +
                Uri.encode(dump.getName()));
        Intent share = new Intent(Intent.ACTION_SEND);
        share.setType("text/plain");
        share.putExtra(Intent.EXTRA_SUBJECT, "Pokegochi Android diagnostic dump");
        share.putExtra(Intent.EXTRA_STREAM, uri);
        share.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try {
            startActivity(Intent.createChooser(share, "Enviar dump do Pokegochi"));
        } catch (android.content.ActivityNotFoundException error) {
            android.widget.Toast.makeText(this, dump.getAbsolutePath(),
                    android.widget.Toast.LENGTH_LONG).show();
        }
    }

    private void hideSystemUi() {
        Window window = getWindow();
        View decor = window.getDecorView();
        if (android.os.Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController controller = decor.getWindowInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            window.getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                            View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemUi();
    }

    @Override protected void onResume() {
        super.onResume();
        requestPhoneClockSync();
    }

    @Override protected void onPause() {
        // onStop is not guaranteed when Android removes a task under memory
        // pressure. Capture at onPause as well, after the runtime has flushed
        // the current save, so a normal Home gesture still leaves a dump even
        // if the process is killed before the next lifecycle callback.
        exportSessionDiagnostic("manual session paused; animation timing capture");
        super.onPause();
    }

    @Override protected void onStop() {
        exportSessionDiagnostic("manual session stopped; animation timing capture");
        super.onStop();
    }

    private void exportSessionDiagnostic(String reason) {
        NativeFirmware target = firmware;
        if (target != null) target.flushSave();
        if (!DIAGNOSTICS_ENABLED) return;
        // A normal test session must produce the same complete artifact as a
        // crash session. This runs as soon as the user backgrounds/closes the app,
        // so the latest native timing log and filtered logcat are available
        // in Downloads/Pokegochi without requiring adb or a hidden path.
        if (!destroyed && target != null && firmwareView != null &&
                !diagnosticSessionExported) {
            diagnosticSessionExported = true;
            File dump = writeDiagnosticDump(reason);
            if (dump != null) {
                android.widget.Toast.makeText(this,
                        "Log salvo em Downloads/Pokegochi", android.widget.Toast.LENGTH_LONG).show();
            }
        }
    }

    @Override protected void onDestroy() {
        destroyed = true;
        if (initializationExecutor != null) {
            initializationExecutor.shutdownNow();
            initializationExecutor = null;
        }
        if (firmwareView != null) firmwareView.stop();
        if (bleMultiplayer != null) bleMultiplayer.close();
        if (phoneBatteryReceiverRegistered && phoneBatteryReceiver != null) {
            unregisterReceiver(phoneBatteryReceiver);
            phoneBatteryReceiverRegistered = false;
            phoneBatteryReceiver = null;
        }
        if (phoneClockReceiverRegistered && phoneClockReceiver != null) {
            unregisterReceiver(phoneClockReceiver);
            phoneClockReceiverRegistered = false;
            phoneClockReceiver = null;
        }
        if (firmware != null) {
            firmware.flushSave();
            firmware.close();
        }
        super.onDestroy();
    }

    private void updatePhoneBattery() {
        NativeFirmware target = firmware;
        if (target == null) return;
        int percent = readPhoneBatteryPercent();
        if (percent >= 0) target.setPhoneBatteryPercent(percent);
    }

    private int readPhoneBatteryPercent() {
        BatteryManager manager = (BatteryManager) getSystemService(BATTERY_SERVICE);
        if (manager != null) {
            int capacity = manager.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY);
            if (capacity >= 0 && capacity <= 100) return capacity;
        }
        Intent status;
        if (android.os.Build.VERSION.SDK_INT >= 33) {
            status = registerReceiver(null,
                    new IntentFilter(Intent.ACTION_BATTERY_CHANGED),
                    Context.RECEIVER_EXPORTED);
        } else {
            status = registerReceiver(null,
                    new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        }
        if (status == null) return -1;
        int level = status.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        int scale = status.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
        if (level < 0 || scale <= 0) return -1;
        return Math.max(0, Math.min(100, Math.round(level * 100f / scale)));
    }

    private final class StartupView extends View {
        private final Paint startupPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final String message;

        StartupView() {
            this("POKEGOCHI\nINICIALIZANDO");
        }

        StartupView(String message) {
            super(MainActivity.this);
            this.message = message;
            startupPaint.setColor(Color.WHITE);
            startupPaint.setTextAlign(Paint.Align.CENTER);
            startupPaint.setTypeface(android.graphics.Typeface.create("sans", android.graphics.Typeface.BOLD));
            setBackgroundColor(Color.BLACK);
        }

        @Override protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            String[] lines = message.split("\\n");
            float centerX = getWidth() * 0.5f;
            float centerY = getHeight() * 0.5f;
            startupPaint.setTextSize(Math.max(18f, getWidth() / 32f));
            canvas.drawText(lines[0], centerX, centerY, startupPaint);
            if (lines.length > 1) {
                startupPaint.setTextSize(Math.max(12f, getWidth() / 55f));
                canvas.drawText(lines[1], centerX, centerY + startupPaint.getTextSize() * 2.2f, startupPaint);
            }
        }
    }

    private final class FirmwareView extends SurfaceView {
        private static final int WIDTH = 320;
        private static final int HEIGHT = 240;
        private final HandlerThread frameThread = new HandlerThread("PokegochiFrames");
        private final Handler frameHandler;
        private final SurfaceHolder surfaceHolder;
        private final Thread renderThread;
        private volatile boolean running = true;
        private volatile boolean surfaceReady;
        private volatile float left;
        private volatile float top;
        private volatile float scale;
        // The firmware worker can spend hundreds of milliseconds rendering a
        // move. Do not enqueue every Android ACTION_MOVE during that time:
        // retain the two semantic edges and only the newest held coordinate.
        private final Object touchDispatchLock = new Object();
        private boolean touchDispatchScheduled;
        private boolean pendingTouchPress;
        private boolean pendingTouchRelease;
        private boolean pendingTouchMove;
        private boolean logicalTouchDown;
        private int pendingTouchX;
        private int pendingTouchY;

        FirmwareView() {
            super(MainActivity.this);
            setFocusable(true);
            surfaceHolder = getHolder();
            // Keep the producer buffer at the firmware's native resolution.
            // SurfaceFlinger scales this 4:3 layer to the centered view; Java
            // never uploads or mutates a Bitmap in the animation hot path.
            surfaceHolder.setFixedSize(WIDTH, HEIGHT);
            surfaceHolder.addCallback(new SurfaceHolder.Callback() {
                @Override public void surfaceCreated(SurfaceHolder holder) {
                    surfaceReady = true;
                    configureSurfaceFrameRate(holder);
                    NativeFirmware target = firmware;
                    if (target != null) target.setSurface(holder.getSurface());
                }

                @Override public void surfaceChanged(SurfaceHolder holder, int format,
                                                      int width, int height) {
                    surfaceReady = true;
                    configureSurfaceFrameRate(holder);
                    NativeFirmware target = firmware;
                    if (target != null) target.setSurface(holder.getSurface());
                }

                @Override public void surfaceDestroyed(SurfaceHolder holder) {
                    surfaceReady = false;
                    NativeFirmware target = firmware;
                    if (target != null) target.setSurface(null);
                }
            });
            frameThread.start();
            frameHandler = new Handler(frameThread.getLooper());
            renderThread = new Thread(this::renderLoop, "PokegochiRender");
            renderThread.setPriority(Thread.NORM_PRIORITY);
            renderThread.start();
        }

        void stop() {
            running = false;
            surfaceReady = false;
            renderThread.interrupt();
            NativeFirmware target = firmware;
            if (target != null) target.setSurface(null);
            frameThread.quitSafely();
            try {
                renderThread.join(500L);
            } catch (InterruptedException ignored) {
                Thread.currentThread().interrupt();
            }
        }

        private void renderLoop() {
            try {
                android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_DISPLAY);
            } catch (Throwable ignored) {
                // A vendor may reject the display priority; normal priority
                // still keeps the renderer functional.
            }
            while (running) {
                NativeFirmware target = firmware;
                final boolean presented = surfaceReady && target != null &&
                        target.presentFrame();
                try {
                    // Poll faster than the authored 60 Hz clock. The native
                    // presenter performs the only RGB565 copy and posts the
                    // Surface buffer directly, so no Java Bitmap can stall the
                    // queue while waiting on a vendor GPU texture fence.
                    Thread.sleep(presented ? 1L : 2L);
                } catch (InterruptedException ignored) {
                    if (!running) break;
                }
            }
        }

        private void configureSurfaceFrameRate(SurfaceHolder holder) {
            if (android.os.Build.VERSION.SDK_INT >= 30) {
                try {
                    holder.getSurface().setFrameRate(60.0f,
                            Surface.FRAME_RATE_COMPATIBILITY_DEFAULT,
                            Surface.CHANGE_FRAME_RATE_ALWAYS);
                } catch (Throwable ignored) {
                    // Frame-rate hints are optional and unavailable on some
                    // vendor Surface implementations.
                }
            }
        }

        private void queueTouch(int x, int y, boolean pressed) {
            boolean schedule = false;
            synchronized (touchDispatchLock) {
                pendingTouchX = x;
                pendingTouchY = y;
                if (pressed) {
                    if (!logicalTouchDown) {
                        logicalTouchDown = true;
                        pendingTouchPress = true;
                    } else {
                        pendingTouchMove = true;
                    }
                } else {
                    pendingTouchMove = false;
                    if (logicalTouchDown) {
                        logicalTouchDown = false;
                        pendingTouchRelease = true;
                    }
                }
                if (!touchDispatchScheduled) {
                    touchDispatchScheduled = true;
                    schedule = true;
                }
            }
            if (schedule) frameHandler.post(this::dispatchPendingTouches);
        }

        private void dispatchPendingTouches() {
            while (running) {
                final int x;
                final int y;
                final boolean pressed;
                synchronized (touchDispatchLock) {
                    if (pendingTouchPress) {
                        pendingTouchPress = false;
                        pressed = true;
                    } else if (pendingTouchRelease) {
                        pendingTouchRelease = false;
                        pressed = false;
                    } else if (pendingTouchMove) {
                        pendingTouchMove = false;
                        pressed = true;
                    } else {
                        touchDispatchScheduled = false;
                        return;
                    }
                    x = pendingTouchX;
                    y = pendingTouchY;
                }
                NativeFirmware target = firmware;
                if (target != null) target.touch(x, y, pressed);
            }
            synchronized (touchDispatchLock) {
                touchDispatchScheduled = false;
            }
        }

        @Override protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int availableWidth = MeasureSpec.getSize(widthMeasureSpec);
            int availableHeight = MeasureSpec.getSize(heightMeasureSpec);
            int measuredWidth = availableWidth;
            int measuredHeight = Math.round(measuredWidth * HEIGHT / (float) WIDTH);
            if (measuredHeight > availableHeight) {
                measuredHeight = availableHeight;
                measuredWidth = Math.round(measuredHeight * WIDTH / (float) HEIGHT);
            }
            setMeasuredDimension(measuredWidth, measuredHeight);
        }

        @Override protected void onSizeChanged(int width, int height,
                                               int oldWidth, int oldHeight) {
            super.onSizeChanged(width, height, oldWidth, oldHeight);
            scale = Math.min(width / (float) WIDTH, height / (float) HEIGHT);
            left = (width - WIDTH * scale) * 0.5f;
            top = (height - HEIGHT * scale) * 0.5f;
        }

        @Override public boolean onTouchEvent(MotionEvent event) {
            if (firmware == null) return true;
            // The first frame can arrive after Android has already delivered
            // a tap. Compute the same letterbox transform here so START is
            // not lost while the firmware is still presenting its boot frame.
            if (scale <= 0) {
                scale = Math.min(getWidth() / (float) WIDTH, getHeight() / (float) HEIGHT);
                left = (getWidth() - WIDTH * scale) * 0.5f;
                top = (getHeight() - HEIGHT * scale) * 0.5f;
            }
            if (scale <= 0) return true;
            float x = (event.getX() - left) / scale;
            float y = (event.getY() - top) / scale;
            int gameX = Math.max(0, Math.min(WIDTH - 1, Math.round(x)));
            int gameY = Math.max(0, Math.min(HEIGHT - 1, Math.round(y)));
            boolean inside = x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
            final int action = event.getActionMasked();
            final int deliveredX = gameX;
            final int deliveredY = gameY;
            final boolean deliveredInside = inside;
            switch (action) {
                case MotionEvent.ACTION_DOWN:
                    if (inside) queueTouch(deliveredX, deliveredY, true);
                    return true;
                case MotionEvent.ACTION_MOVE:
                    queueTouch(deliveredX, deliveredY, deliveredInside);
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    queueTouch(deliveredX, deliveredY, false);
                    return true;
                default:
                    return true;
            }
        }
    }
}
