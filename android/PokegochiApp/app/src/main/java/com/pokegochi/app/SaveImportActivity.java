package com.pokegochi.app;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Process;
import android.os.SystemClock;
import android.provider.OpenableColumns;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.File;
import java.io.InputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Isolated importer launched from a .pgsav file, Share, or the launcher shortcut. */
public final class SaveImportActivity extends Activity {
    private static final int PICK_SAVE = 4401;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private TextView status;
    private Button choose;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        setContentView(buildView());
        Uri incoming = incomingUri(getIntent());
        if (incoming != null) inspect(incoming);
        else if (state == null) chooseSave();
    }

    private LinearLayout buildView() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setPadding(36, 24, 36, 24);
        root.setBackgroundColor(Color.rgb(16, 23, 34));

        TextView title = new TextView(this);
        title.setText("IMPORTAR SAVE DO POKEGOCHI");
        title.setTextColor(Color.WHITE);
        title.setTextSize(22);
        title.setGravity(Gravity.CENTER);
        root.addView(title, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        status = new TextView(this);
        status.setText("Escolha o arquivo .pgsav exportado do Pokegochi.");
        status.setTextColor(Color.rgb(210, 220, 232));
        status.setTextSize(16);
        status.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams statusLayout = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        statusLayout.setMargins(0, 18, 0, 18);
        root.addView(status, statusLayout);

        choose = new Button(this);
        choose.setText("ESCOLHER .PGSAV");
        choose.setOnClickListener(view -> chooseSave());
        root.addView(choose, new LinearLayout.LayoutParams(220,
                ViewGroup.LayoutParams.WRAP_CONTENT));
        return root;
    }

    private void chooseSave() {
        Intent picker = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        picker.addCategory(Intent.CATEGORY_OPENABLE);
        picker.setType("*/*");
        startActivityForResult(picker, PICK_SAVE);
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_SAVE) return;
        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            status.setText("Nenhum arquivo foi selecionado.");
            return;
        }
        inspect(data.getData());
    }

    private void inspect(Uri uri) {
        choose.setEnabled(false);
        status.setText("Validando " + displayName(uri) + "...");
        worker.execute(() -> {
            try (InputStream input = getContentResolver().openInputStream(uri)) {
                PgsavSaveImporter.Parsed parsed = PgsavSaveImporter.read(input);
                runOnUiThread(() -> confirmImport(parsed));
            } catch (Exception error) {
                runOnUiThread(() -> showError(error));
            }
        });
    }

    private void confirmImport(PgsavSaveImporter.Parsed save) {
        choose.setEnabled(true);
        status.setText("Save valido: player " + save.sourcePlayerId + ", formato V" +
                save.saveVersion + ".");
        new AlertDialog.Builder(this)
                .setTitle("Substituir o save do aplicativo?")
                .setMessage("O arquivo passou pelos CRCs. O save atual do app sera copiado para " +
                        "um backup privado antes da troca. O Pokegochi fisico nao sera alterado.")
                .setNegativeButton("CANCELAR", null)
                .setPositiveButton("IMPORTAR", (dialog, which) -> install(save))
                .show();
    }

    private void install(PgsavSaveImporter.Parsed save) {
        choose.setEnabled(false);
        status.setText("Fechando o jogo e instalando o save...");
        worker.execute(() -> {
            try {
                stopMainGameProcess();
                SystemClock.sleep(250L);
                File preferences = new File(getFilesDir(), "preferences");
                File backups = new File(getFilesDir(), "save_backups");
                PgsavSaveImporter.InstallResult result =
                        PgsavSaveImporter.install(preferences, backups, save);
                runOnUiThread(() -> showSuccess(result));
            } catch (Exception error) {
                runOnUiThread(() -> showError(error));
            }
        });
    }

    private void stopMainGameProcess() {
        ActivityManager manager = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
        if (manager == null || manager.getRunningAppProcesses() == null) return;
        String mainProcess = getPackageName();
        for (ActivityManager.RunningAppProcessInfo process : manager.getRunningAppProcesses()) {
            if (process.pid != Process.myPid() && mainProcess.equals(process.processName))
                Process.killProcess(process.pid);
        }
    }

    private void showSuccess(PgsavSaveImporter.InstallResult result) {
        status.setText("Save importado e verificado nos dois slots.");
        new AlertDialog.Builder(this)
                .setTitle("Save importado")
                .setMessage("Tudo certo. O backup anterior ficou preservado em " +
                        result.backupDirectory.getName() + ".")
                .setCancelable(false)
                .setPositiveButton("ABRIR O JOGO", (dialog, which) -> openGame())
                .show();
    }

    private void showError(Exception error) {
        choose.setEnabled(true);
        status.setText(error.getMessage() == null ? "Falha ao importar o save." : error.getMessage());
        new AlertDialog.Builder(this)
                .setTitle("Save nao importado")
                .setMessage(status.getText())
                .setPositiveButton("OK", null)
                .show();
    }

    private void openGame() {
        Intent game = new Intent(this, MainActivity.class);
        game.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK |
                Intent.FLAG_ACTIVITY_CLEAR_TOP);
        startActivity(game);
        finishAndRemoveTask();
    }

    private Uri incomingUri(Intent intent) {
        if (intent == null) return null;
        if (Intent.ACTION_VIEW.equals(intent.getAction())) return intent.getData();
        if (Intent.ACTION_SEND.equals(intent.getAction())) {
            if (android.os.Build.VERSION.SDK_INT >= 33)
                return intent.getParcelableExtra(Intent.EXTRA_STREAM, Uri.class);
            @SuppressWarnings("deprecation")
            Uri value = intent.getParcelableExtra(Intent.EXTRA_STREAM);
            return value;
        }
        return null;
    }

    private String displayName(Uri uri) {
        try (android.database.Cursor cursor = getContentResolver().query(uri,
                new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) return cursor.getString(0);
        } catch (Exception ignored) {}
        return "save selecionado";
    }

    @Override protected void onDestroy() {
        worker.shutdownNow();
        super.onDestroy();
    }
}
