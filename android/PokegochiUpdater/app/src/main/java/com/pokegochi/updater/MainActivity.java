package com.pokegochi.updater;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothDevice;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.InputStream;
import java.io.OutputStream;
import java.text.DecimalFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class MainActivity extends Activity implements BleUpdateClient.Listener {
    private static final int PICK_PACKAGE = 100;
    private static final int BLE_PERMISSION = 101;
    private static final int PICK_SAVE_FILE = 102;
    private static final int CREATE_SAVE_FILE = 103;
    private static final int STATUS_COLOR = Color.rgb(72, 88, 110);
    private static final int BLUE = Color.rgb(38, 111, 190);
    private static final int GREEN = Color.rgb(30, 142, 76);
    private static final int RED = Color.rgb(180, 46, 52);

    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final DecimalFormat megabytes = new DecimalFormat("0.0");
    private BleUpdateClient client;
    private UpdatePackage updatePackage;
    private SaveBackup selectedSave;
    private Uri pendingExportUri;
    private boolean dashboardVisible;
    private boolean operationBusy;
    private boolean rebootExpected;
    private boolean saveManagementAvailable;
    private boolean timeSyncAvailable;
    private long authenticatedPlayerId;

    private TextView packageLabel, connectionLabel, statusLabel, progressLabel, saveLabel;
    private LinearLayout deviceList;
    private EditText pairingCode;
    private ProgressBar progress;
    private Button scanButton, authenticateButton, updateButton, syncButton;
    private Button loadButton, saveButton, resetButton, disconnectButton, cancelButton;

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        client = new BleUpdateClient(this, this);
        showConnectScreen();
        ensurePermissions();
    }

    private void showConnectScreen() {
        dashboardVisible = false;
        rebootExpected = false;
        int pad = dp(18);
        ScrollView scroll = shell();
        LinearLayout root = content(scroll, pad);
        root.addView(text("POKEGOCHI LINK", 27, Color.rgb(16, 40, 76), true));
        TextView subtitle = text("Connect securely, then choose what you want to do", 14,
                STATUS_COLOR, false);
        subtitle.setPadding(0, dp(2), 0, dp(18));
        root.addView(subtitle);

        LinearLayout deviceCard = card();
        deviceCard.addView(text("1  CONNECT TO POKEGOCHI", 16, Color.rgb(16, 40, 76), true));
        connectionLabel = text("On the console: Settings > Wireless Update", 14,
                Color.DKGRAY, false);
        connectionLabel.setPadding(0, dp(10), 0, dp(8));
        deviceCard.addView(connectionLabel);
        scanButton = action("SCAN NEARBY", BLUE);
        scanButton.setOnClickListener(v -> {
            deviceList.removeAllViews();
            if (ensurePermissions()) client.scan();
        });
        deviceCard.addView(scanButton);
        deviceList = new LinearLayout(this);
        deviceList.setOrientation(LinearLayout.VERTICAL);
        deviceList.setPadding(0, dp(8), 0, 0);
        deviceCard.addView(deviceList);
        root.addView(deviceCard);

        LinearLayout authCard = card();
        authCard.addView(text("2  ENTER THE CONSOLE CODE", 16, Color.rgb(16, 40, 76), true));
        pairingCode = new EditText(this);
        pairingCode.setHint("6-digit code shown on console");
        pairingCode.setInputType(InputType.TYPE_CLASS_NUMBER |
                InputType.TYPE_NUMBER_VARIATION_PASSWORD);
        pairingCode.setGravity(Gravity.CENTER);
        pairingCode.setTextSize(20);
        pairingCode.setMaxLines(1);
        pairingCode.setPadding(dp(12), dp(10), dp(12), dp(10));
        LinearLayout.LayoutParams codeParams = new LinearLayout.LayoutParams(-1, dp(55));
        codeParams.setMargins(0, dp(10), 0, dp(10));
        authCard.addView(pairingCode, codeParams);
        authenticateButton = action("UNLOCK TOOLS", GREEN);
        authenticateButton.setOnClickListener(v -> authenticate());
        authCard.addView(authenticateButton);
        root.addView(authCard);

        statusLabel = text("The code is valid only while the Wireless Update screen is open.",
                13, STATUS_COLOR, false);
        statusLabel.setPadding(dp(4), dp(4), dp(4), dp(24));
        root.addView(statusLabel);
        setContentView(scroll);
    }

    private void showDashboard(long playerId, boolean timeSync, boolean saveManagement) {
        dashboardVisible = true;
        operationBusy = false;
        rebootExpected = false;
        authenticatedPlayerId = playerId;
        saveManagementAvailable = saveManagement;
        timeSyncAvailable = timeSync;
        int pad = dp(18);
        ScrollView scroll = shell();
        LinearLayout root = content(scroll, pad);
        root.addView(text("POKEGOCHI TOOLS", 27, Color.rgb(16, 40, 76), true));
        TextView connected = text(String.format(Locale.US, "Pokegochi %05d authenticated", playerId),
                14, GREEN, true);
        connected.setPadding(0, dp(2), 0, dp(14));
        root.addView(connected);

        LinearLayout deviceCard = card();
        deviceCard.addView(text("DEVICE", 16, Color.rgb(16, 40, 76), true));
        syncButton = action("SYNC CLOCK", BLUE);
        syncButton.setEnabled(timeSync);
        syncButton.setOnClickListener(v -> synchronizeClock());
        deviceCard.addView(syncButton);
        TextView clockHelp = text(timeSync ?
                "Copies the phone's local date and time to the Pokegochi." :
                "Clock sync needs a newer Pokegochi firmware.", 13, STATUS_COLOR, false);
        clockHelp.setPadding(0, dp(7), 0, 0);
        deviceCard.addView(clockHelp);
        root.addView(deviceCard);

        LinearLayout updateCard = card();
        updateCard.addView(text("OTAP UPDATE", 16, Color.rgb(16, 40, 76), true));
        packageLabel = text(updatePackage == null ? "No .pgota package selected" :
                updatePackage.displayName + "\n" +
                        megabytes.format(updatePackage.totalBytes / 1048576.0) + " MiB verified",
                14, Color.DKGRAY, false);
        packageLabel.setPadding(0, dp(9), 0, dp(8));
        updateCard.addView(packageLabel);
        Button pick = action("SELECT .PGOTA FILE", BLUE);
        pick.setOnClickListener(v -> selectPackage());
        updateCard.addView(pick);
        updateButton = action("INSTALL UPDATE", GREEN);
        updateButton.setEnabled(updatePackage != null);
        updateButton.setOnClickListener(v -> startUpdate());
        updateCard.addView(updateButton);
        root.addView(updateCard);

        LinearLayout saveCard = card();
        saveCard.addView(text("SAVE FILE", 16, Color.rgb(16, 40, 76), true));
        saveLabel = text(saveManagement ?
                "Backups are verified before download or restore." :
                "Install the matching Pokegochi firmware before using save tools.",
                13, STATUS_COLOR, false);
        saveLabel.setPadding(0, dp(8), 0, dp(8));
        saveCard.addView(saveLabel);
        loadButton = action("LOAD FILE  ·  POKEGOCHI → ANDROID", BLUE);
        loadButton.setEnabled(saveManagement);
        loadButton.setOnClickListener(v -> chooseBackupDestination());
        saveCard.addView(loadButton);
        saveCard.addView(help("Downloads a .pgsav backup without changing the console."));
        saveButton = action("SAVE FILE  ·  ANDROID → POKEGOCHI", Color.rgb(105, 78, 166));
        saveButton.setEnabled(saveManagement);
        saveButton.setOnClickListener(v -> chooseSaveToRestore());
        saveCard.addView(saveButton);
        saveCard.addView(help("Selects a .pgsav file, verifies it, then replaces the console save."));
        resetButton = action("RESET FILE  ·  ERASE PROGRESS", RED);
        resetButton.setEnabled(saveManagement);
        resetButton.setOnClickListener(v -> confirmReset());
        saveCard.addView(resetButton);
        saveCard.addView(help("Creates a fresh save. This cannot be undone without a backup."));
        root.addView(saveCard);

        LinearLayout progressCard = card();
        progressCard.addView(text("STATUS", 16, Color.rgb(16, 40, 76), true));
        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(1000);
        progress.setProgress(0);
        LinearLayout.LayoutParams progressParams = new LinearLayout.LayoutParams(-1, dp(18));
        progressParams.setMargins(0, dp(12), 0, dp(4));
        progressCard.addView(progress, progressParams);
        progressLabel = text("Ready", 13, Color.DKGRAY, false);
        progressCard.addView(progressLabel);
        statusLabel = text("Choose an operation above.", 13, STATUS_COLOR, false);
        statusLabel.setPadding(0, dp(8), 0, 0);
        progressCard.addView(statusLabel);
        cancelButton = action("CANCEL OPERATION", RED);
        cancelButton.setVisibility(View.GONE);
        cancelButton.setOnClickListener(v -> cancelOperation());
        progressCard.addView(cancelButton);
        root.addView(progressCard);

        disconnectButton = action("DISCONNECT / CHANGE DEVICE", Color.rgb(83, 92, 108));
        disconnectButton.setOnClickListener(v -> {
            client.disconnect();
            showConnectScreen();
        });
        root.addView(disconnectButton);
        TextView safety = text("Keep both devices close and powered during writes. Restores are " +
                "transactional: the active save changes only after the complete file passes every check.",
                13, STATUS_COLOR, false);
        safety.setPadding(dp(4), dp(10), dp(4), dp(24));
        root.addView(safety);
        setContentView(scroll);
    }

    private ScrollView shell() {
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(Color.rgb(238, 244, 252));
        return scroll;
    }

    private LinearLayout content(ScrollView scroll, int pad) {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(pad, pad, pad, pad);
        scroll.addView(root, new ScrollView.LayoutParams(-1, -2));
        return root;
    }

    private TextView help(String value) {
        TextView result = text(value, 12, STATUS_COLOR, false);
        result.setPadding(dp(3), dp(5), dp(3), dp(8));
        return result;
    }

    private LinearLayout card() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(16), dp(15), dp(16), dp(15));
        GradientDrawable background = new GradientDrawable();
        background.setColor(Color.WHITE);
        background.setCornerRadius(dp(14));
        background.setStroke(dp(1), Color.rgb(197, 211, 229));
        card.setBackground(background);
        card.setElevation(dp(2));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2);
        params.setMargins(0, 0, 0, dp(14));
        card.setLayoutParams(params);
        return card;
    }

    private TextView text(String value, int sp, int color, boolean bold) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(sp);
        view.setTextColor(color);
        if (bold) view.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        return view;
    }

    private Button action(String value, int color) {
        Button button = new Button(this);
        button.setText(value);
        button.setTextColor(Color.WHITE);
        button.setTextSize(14);
        button.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        button.setAllCaps(false);
        GradientDrawable background = new GradientDrawable();
        background.setColor(color);
        background.setCornerRadius(dp(10));
        button.setBackground(background);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, dp(48));
        params.setMargins(0, dp(4), 0, 0);
        button.setLayoutParams(params);
        return button;
    }

    private void authenticate() {
        String value = pairingCode.getText().toString().trim();
        if (value.length() != 6) { showError("Enter the six-digit code from the console"); return; }
        authenticateButton.setEnabled(false);
        statusLabel.setTextColor(STATUS_COLOR);
        statusLabel.setText("Authenticating...");
        client.authenticate(Integer.parseInt(value));
    }

    private void selectPackage() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES,
                new String[]{"application/zip", "application/octet-stream"});
        startActivityForResult(intent, PICK_PACKAGE);
    }

    private void chooseBackupDestination() {
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/octet-stream");
        String stamp = new SimpleDateFormat("yyyyMMdd-HHmm", Locale.US).format(new Date());
        intent.putExtra(Intent.EXTRA_TITLE,
                String.format(Locale.US, "pokegochi-%05d-%s.pgsav", authenticatedPlayerId, stamp));
        startActivityForResult(intent, CREATE_SAVE_FILE);
    }

    private void chooseSaveToRestore() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/octet-stream");
        startActivityForResult(intent, PICK_SAVE_FILE);
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        if (requestCode == PICK_PACKAGE) loadPackage(uri);
        else if (requestCode == PICK_SAVE_FILE) inspectSave(uri);
        else if (requestCode == CREATE_SAVE_FILE) {
            pendingExportUri = uri;
            beginOperation("Preparing save backup...");
            client.exportSave();
        }
    }

    private void loadPackage(Uri uri) {
        packageLabel.setText("Checking package hashes...");
        updateButton.setEnabled(false);
        worker.execute(() -> {
            try {
                UpdatePackage loaded = UpdatePackage.load(this, uri, new UpdatePackage.Progress() {
                    @Override public void message(String value) {
                        runOnUiThread(() -> packageLabel.setText(value));
                    }
                    @Override public void value(int current, int total) {
                        runOnUiThread(() -> progress.setProgress(current * 1000 / total));
                    }
                });
                UpdatePackage previous = updatePackage;
                updatePackage = loaded;
                if (previous != null) previous.close();
                runOnUiThread(() -> {
                    packageLabel.setText(loaded.displayName + "\n" +
                            megabytes.format(loaded.totalBytes / 1048576.0) + " MiB verified");
                    progress.setProgress(0);
                    progressLabel.setText("Ready");
                    statusLabel.setTextColor(STATUS_COLOR);
                    statusLabel.setText("Package hashes passed. The console will verify its signature.");
                    updateButton.setEnabled(true);
                });
            } catch (Exception error) {
                runOnUiThread(() -> {
                    packageLabel.setText("Package rejected: " + error.getMessage());
                    progress.setProgress(0);
                    updateButton.setEnabled(updatePackage != null);
                });
            }
        });
    }

    private void inspectSave(Uri uri) {
        beginOperation("Verifying selected save file...");
        worker.execute(() -> {
            try (InputStream input = getContentResolver().openInputStream(uri)) {
                SaveBackup backup = SaveBackup.read(input);
                selectedSave = backup;
                runOnUiThread(() -> {
                    endOperation();
                    new AlertDialog.Builder(this)
                            .setTitle("Replace Pokegochi save?")
                            .setMessage(String.format(Locale.US,
                                    "Source: Pokegochi %05d\nSave format: %d\nSize: %s KiB\n\n" +
                                            "The current progress will be replaced after verification.",
                                    backup.sourcePlayerId, backup.saveFormatVersion,
                                    new DecimalFormat("0.0").format(backup.bytes.length / 1024.0)))
                            .setNegativeButton("CANCEL", null)
                            .setPositiveButton("SAVE TO POKEGOCHI", (dialog, which) -> {
                                beginOperation("Sending verified save to Pokegochi...");
                                client.importSave(backup);
                            }).show();
                });
            } catch (Exception error) {
                runOnUiThread(() -> showError("Save rejected: " + error.getMessage()));
            }
        });
    }

    private void confirmReset() {
        new AlertDialog.Builder(this)
                .setTitle("Reset the save file?")
                .setMessage("This permanently erases Pokémon, items and all progress on the Pokegochi. " +
                        "Load a backup to Android first if you may want it later.")
                .setNegativeButton("CANCEL", null)
                .setPositiveButton("RESET FILE", (dialog, which) -> {
                    beginOperation("Resetting save to a fresh game...");
                    client.resetSave();
                }).show();
    }

    private void startUpdate() {
        if (updatePackage == null) { showError("Select a verified .pgota package first"); return; }
        beginOperation("Authenticating signed update...");
        client.start(updatePackage);
    }

    private void synchronizeClock() {
        beginOperation("Synchronizing with the phone clock...");
        client.synchronizeClock();
    }

    private void beginOperation(String message) {
        operationBusy = true;
        setToolButtonsEnabled(false);
        if (cancelButton != null) {
            cancelButton.setVisibility(View.VISIBLE);
            cancelButton.setEnabled(true);
        }
        statusLabel.setTextColor(STATUS_COLOR);
        statusLabel.setText(message);
        progress.setProgress(0);
        progressLabel.setText("Starting...");
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    private void endOperation() {
        operationBusy = false;
        if (cancelButton != null) {
            cancelButton.setEnabled(false);
            cancelButton.setVisibility(View.GONE);
        }
        setToolButtonsEnabled(true);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    private void cancelOperation() {
        if (!operationBusy) return;
        cancelButton.setEnabled(false);
        statusLabel.setTextColor(STATUS_COLOR);
        statusLabel.setText("Cancelling safely and releasing Bluetooth...");
        client.cancelOperation();
    }

    private void setToolButtonsEnabled(boolean enabled) {
        if (!dashboardVisible) return;
        if (updateButton != null) updateButton.setEnabled(enabled && updatePackage != null);
        if (syncButton != null) syncButton.setEnabled(enabled && timeSyncAvailable);
        if (loadButton != null) loadButton.setEnabled(enabled && saveManagementAvailable);
        if (saveButton != null) saveButton.setEnabled(enabled && saveManagementAvailable);
        if (resetButton != null) resetButton.setEnabled(enabled && saveManagementAvailable);
        if (disconnectButton != null) disconnectButton.setEnabled(enabled);
    }

    @Override public void onDevice(BluetoothDevice device, long playerId, int rssi) {
        runOnUiThread(() -> {
            if (dashboardVisible || deviceList == null) return;
            Button choice = action(String.format(Locale.US, "Pokegochi %05d   (%d dBm)",
                    playerId, rssi), Color.rgb(63, 86, 119));
            choice.setOnClickListener(v -> client.connect(device));
            deviceList.addView(choice);
        });
    }

    @Override public void onConnection(boolean connected, String message) {
        runOnUiThread(() -> {
            if (!dashboardVisible) {
                if (connectionLabel != null) connectionLabel.setText(message);
                if (scanButton != null) scanButton.setEnabled(!connected);
                if (!connected && authenticateButton != null) authenticateButton.setEnabled(true);
            } else if (!connected && !rebootExpected) {
                showConnectScreen();
                statusLabel.setTextColor(RED);
                statusLabel.setText(message + " Scan and authenticate again.");
            }
        });
    }

    @Override public void onAuthenticated(long playerId, boolean timeSync, boolean saveManagement) {
        runOnUiThread(() -> showDashboard(playerId, timeSync, saveManagement));
    }

    @Override public void onTransfer(long done, long total, String message) {
        runOnUiThread(() -> {
            if (!dashboardVisible || progress == null) return;
            int value = total == 0 ? 0 : (int) Math.min(1000, done * 1000L / total);
            progress.setProgress(value);
            progressLabel.setText(String.format(Locale.US, "%.1f%%   %s / %s MiB", value / 10.0,
                    megabytes.format(done / 1048576.0), megabytes.format(total / 1048576.0)));
            statusLabel.setTextColor(STATUS_COLOR);
            statusLabel.setText(message);
        });
    }

    @Override public void onComplete(String expectedFirmwareVersion) {
        runOnUiThread(() -> {
            rebootExpected = true;
            endOperation();
            // A COMMIT acknowledgement only proves that the ESP32 accepted
            // the staged image. It can still roll back while booting, so do
            // not falsely promise a completed firmware update here.
            statusLabel.setTextColor(Color.rgb(193, 103, 0));
            statusLabel.setText("Transfer accepted. After reboot, confirm Settings > More shows FW " +
                    expectedFirmwareVersion + ".");
            progressLabel.setText("Awaiting console validation");
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        });
    }

    @Override public void onClockSynchronized() {
        runOnUiThread(() -> {
            endOperation();
            statusLabel.setTextColor(GREEN);
            statusLabel.setText("Clock synchronized with this phone.");
            progressLabel.setText("Clock ready");
        });
    }

    @Override public void onSaveExported(byte[] backup) {
        Uri destination = pendingExportUri;
        pendingExportUri = null;
        worker.execute(() -> {
            try (OutputStream output = destination == null ? null :
                    getContentResolver().openOutputStream(destination, "wt")) {
                if (output == null) throw new Exception("Could not open the Android destination");
                output.write(backup);
                output.flush();
                runOnUiThread(() -> {
                    endOperation();
                    statusLabel.setTextColor(GREEN);
                    statusLabel.setText("Save loaded to Android and verified successfully.");
                    progressLabel.setText("Backup complete");
                });
            } catch (Exception error) {
                runOnUiThread(() -> showError("Could not write backup: " + error.getMessage()));
            }
        });
    }

    @Override public void onSaveImported() {
        runOnUiThread(() -> {
            rebootExpected = true;
            endOperation();
            statusLabel.setTextColor(GREEN);
            statusLabel.setText("Save file restored. Pokegochi is restarting.");
            progressLabel.setText("Restore complete");
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        });
    }

    @Override public void onSaveReset() {
        runOnUiThread(() -> {
            rebootExpected = true;
            endOperation();
            statusLabel.setTextColor(GREEN);
            statusLabel.setText("Fresh save created. Pokegochi is restarting.");
            progressLabel.setText("Reset complete");
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        });
    }

    @Override public void onOperationCancelled() {
        runOnUiThread(() -> {
            endOperation();
            statusLabel.setTextColor(RED);
            statusLabel.setText("Operation cancelled. The installed game and save were kept unchanged.");
            progressLabel.setText("Cancelled — reconnect to resume safely");
        });
    }

    @Override public void onError(String message) { runOnUiThread(() -> showError(message)); }

    private void showError(String message) {
        if (statusLabel != null) {
            statusLabel.setText(message);
            statusLabel.setTextColor(RED);
        }
        if (dashboardVisible) endOperation();
        else if (authenticateButton != null) authenticateButton.setEnabled(true);
    }

    private boolean ensurePermissions() {
        if (Build.VERSION.SDK_INT >= 31) {
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
                    checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED)
                return true;
            requestPermissions(new String[]{Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT}, BLE_PERMISSION);
            return false;
        }
        if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED)
            return true;
        requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION}, BLE_PERMISSION);
        return false;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    @Override public void onBackPressed() {
        if (dashboardVisible && operationBusy) {
            cancelOperation();
        } else if (dashboardVisible) {
            client.disconnect();
            showConnectScreen();
        } else super.onBackPressed();
    }

    @Override protected void onDestroy() {
        client.close();
        worker.shutdownNow();
        if (updatePackage != null) try { updatePackage.close(); } catch (Exception ignored) {}
        super.onDestroy();
    }
}
