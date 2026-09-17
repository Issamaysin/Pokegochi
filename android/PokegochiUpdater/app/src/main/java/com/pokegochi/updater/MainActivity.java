package com.pokegochi.updater;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothDevice;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
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

import java.text.DecimalFormat;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class MainActivity extends Activity implements BleUpdateClient.Listener {
    private static final int PICK_PACKAGE = 100;
    private static final int BLE_PERMISSION = 101;
    private static final int STATUS_COLOR = Color.rgb(72, 88, 110);
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final DecimalFormat megabytes = new DecimalFormat("0.0");
    private BleUpdateClient client;
    private UpdatePackage updatePackage;
    private TextView packageLabel, connectionLabel, statusLabel, progressLabel;
    private LinearLayout deviceList;
    private EditText pairingCode;
    private ProgressBar progress;
    private Button scanButton, updateButton;

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        client = new BleUpdateClient(this, this);
        setContentView(buildUi());
        ensurePermissions();
    }

    private View buildUi() {
        int pad = dp(18);
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(Color.rgb(238, 244, 252));
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(pad, pad, pad, pad);
        scroll.addView(root, new ScrollView.LayoutParams(-1, -2));

        TextView title = text("POKEGOCHI UPDATER", 26, Color.rgb(16, 40, 76), true);
        root.addView(title);
        TextView subtitle = text("Signed firmware + microSD updates over Bluetooth", 14,
                Color.rgb(72, 88, 110), false);
        subtitle.setPadding(0, dp(2), 0, dp(18));
        root.addView(subtitle);

        LinearLayout packageCard = card();
        packageCard.addView(text("1  UPDATE PACKAGE", 16, Color.rgb(16, 40, 76), true));
        packageLabel = text("No .pgota package selected", 14, Color.DKGRAY, false);
        packageLabel.setPadding(0, dp(10), 0, dp(10));
        packageCard.addView(packageLabel);
        Button pick = action("SELECT .PGOTA FILE", Color.rgb(38, 111, 190));
        pick.setOnClickListener(v -> selectPackage());
        packageCard.addView(pick);
        root.addView(packageCard);

        LinearLayout deviceCard = card();
        deviceCard.addView(text("2  FIND POKEGOCHI", 16, Color.rgb(16, 40, 76), true));
        connectionLabel = text("On the console: Settings > Wireless Update", 14, Color.DKGRAY, false);
        connectionLabel.setPadding(0, dp(10), 0, dp(8));
        deviceCard.addView(connectionLabel);
        scanButton = action("SCAN NEARBY", Color.rgb(38, 111, 190));
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

        LinearLayout installCard = card();
        installCard.addView(text("3  VERIFY AND INSTALL", 16, Color.rgb(16, 40, 76), true));
        pairingCode = new EditText(this);
        pairingCode.setHint("6-digit code shown on console");
        pairingCode.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_VARIATION_PASSWORD);
        pairingCode.setGravity(Gravity.CENTER);
        pairingCode.setTextSize(20);
        pairingCode.setMaxLines(1);
        pairingCode.setPadding(dp(12), dp(10), dp(12), dp(10));
        LinearLayout.LayoutParams codeParams = new LinearLayout.LayoutParams(-1, dp(55));
        codeParams.setMargins(0, dp(10), 0, dp(10));
        installCard.addView(pairingCode, codeParams);
        updateButton = action("START UPDATE", Color.rgb(30, 142, 76));
        updateButton.setOnClickListener(v -> startUpdate());
        installCard.addView(updateButton);
        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(1000);
        progress.setProgress(0);
        LinearLayout.LayoutParams progressParams = new LinearLayout.LayoutParams(-1, dp(18));
        progressParams.setMargins(0, dp(16), 0, dp(4));
        installCard.addView(progress, progressParams);
        progressLabel = text("0%", 13, Color.DKGRAY, false);
        installCard.addView(progressLabel);
        statusLabel = text("The save stored inside the console is not included or replaced.",
                13, Color.rgb(72, 88, 110), false);
        statusLabel.setPadding(0, dp(10), 0, 0);
        installCard.addView(statusLabel);
        root.addView(installCard);

        TextView safety = text("Keep both devices close and powered. If Bluetooth drops while assets are " +
                "being sent, reconnect and select the same package to resume. The console installs only " +
                "after every SHA-256 hash and the package signature pass.", 13,
                Color.rgb(72, 88, 110), false);
        safety.setPadding(dp(4), dp(8), dp(4), dp(24));
        root.addView(safety);
        return scroll;
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

    private void selectPackage() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"application/zip", "application/octet-stream"});
        startActivityForResult(intent, PICK_PACKAGE);
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_PACKAGE || resultCode != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        packageLabel.setText("Checking package hashes...");
        updateButton.setEnabled(false);
        worker.execute(() -> {
            try {
                UpdatePackage loaded = UpdatePackage.load(this, uri, new UpdatePackage.Progress() {
                    @Override public void message(String text) { runOnUiThread(() -> packageLabel.setText(text)); }
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
                    updateButton.setText("START UPDATE");
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

    private void startUpdate() {
        if (updatePackage == null) { showError("Select a verified .pgota package first"); return; }
        String text = pairingCode.getText().toString().trim();
        if (text.length() != 6) { showError("Enter the six-digit code from the console"); return; }
        statusLabel.setTextColor(STATUS_COLOR);
        statusLabel.setText("Authenticating signed update...");
        updateButton.setText("UPDATING...");
        updateButton.setEnabled(false);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        client.start(updatePackage, Integer.parseInt(text));
    }

    @Override public void onDevice(BluetoothDevice device, long playerId, int rssi) {
        runOnUiThread(() -> {
            Button choice = action(String.format("Pokegochi %05d   (%d dBm)", playerId, rssi),
                    Color.rgb(63, 86, 119));
            choice.setOnClickListener(v -> client.connect(device));
            deviceList.addView(choice);
        });
    }

    @Override public void onConnection(boolean connected, String message) {
        runOnUiThread(() -> {
            connectionLabel.setText(message);
            scanButton.setEnabled(!connected);
        });
    }

    @Override public void onTransfer(long done, long total, String message) {
        runOnUiThread(() -> {
            int value = total == 0 ? 0 : (int) Math.min(1000, done * 1000L / total);
            progress.setProgress(value);
            progressLabel.setText(String.format("%.1f%%   %s / %s MiB", value / 10.0,
                    megabytes.format(done / 1048576.0), megabytes.format(total / 1048576.0)));
            statusLabel.setTextColor(STATUS_COLOR);
            statusLabel.setText(message);
        });
    }

    @Override public void onComplete() {
        runOnUiThread(() -> {
            updateButton.setEnabled(true);
            updateButton.setText("UPDATE COMPLETE");
            pairingCode.setText("");
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        });
    }

    @Override public void onError(String message) { runOnUiThread(() -> showError(message)); }

    private void showError(String message) {
        statusLabel.setText(message);
        statusLabel.setTextColor(Color.rgb(190, 35, 45));
        updateButton.setEnabled(true);
        updateButton.setText("RETRY UPDATE");
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
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

    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }

    @Override protected void onDestroy() {
        client.close();
        worker.shutdownNow();
        if (updatePackage != null) try { updatePackage.close(); } catch (Exception ignored) {}
        super.onDestroy();
    }
}
