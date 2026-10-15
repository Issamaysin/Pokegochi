package com.pokegochi.app;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

/** Shares only the generated diagnostic dump, without broad storage access. */
public final class DiagnosticDumpProvider extends ContentProvider {
    private File root() {
        Context context = getContext();
        if (context == null) throw new IllegalStateException("provider is detached");
        return new File(context.getFilesDir(), "diagnostics");
    }

    private File resolve(Uri uri) throws IOException {
        String name = uri.getLastPathSegment();
        if (name == null || name.isEmpty() || name.contains("/") || name.contains("\\") ||
                name.contains("..")) throw new IOException("invalid diagnostic path");
        File root = root().getCanonicalFile();
        File file = new File(root, name).getCanonicalFile();
        if (!file.getPath().startsWith(root.getPath() + File.separator))
            throw new IOException("diagnostic path escaped root");
        return file;
    }

    @Override public boolean onCreate() { return true; }

    @Override public String getType(Uri uri) { return "text/plain"; }

    @Override public ParcelFileDescriptor openFile(Uri uri, String mode)
            throws FileNotFoundException {
        try {
            return ParcelFileDescriptor.open(resolve(uri),
                    ParcelFileDescriptor.MODE_READ_ONLY);
        } catch (IOException error) {
            throw new FileNotFoundException(error.getMessage());
        }
    }

    @Override public Cursor query(Uri uri, String[] projection, String selection,
                                  String[] selectionArgs, String sortOrder) {
        MatrixCursor cursor = new MatrixCursor(new String[]{"_display_name", "_size"});
        try {
            File file = resolve(uri);
            cursor.addRow(new Object[]{file.getName(), file.length()});
        } catch (IOException ignored) { }
        return cursor;
    }

    @Override public int delete(Uri uri, String selection, String[] selectionArgs) { return 0; }
    @Override public int update(Uri uri, ContentValues values, String selection,
                                String[] selectionArgs) { return 0; }
    @Override public Uri insert(Uri uri, ContentValues values) { return null; }
}
