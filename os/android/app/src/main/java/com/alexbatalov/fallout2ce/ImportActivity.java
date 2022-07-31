package com.alexbatalov.fallout2ce;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.documentfile.provider.DocumentFile;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class ImportActivity extends Activity {
    private static final int IMPORT_REQUEST_CODE = 1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        startActivityForResult(intent, IMPORT_REQUEST_CODE);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent resultData) {
        if (requestCode == IMPORT_REQUEST_CODE) {
            if (resultCode == Activity.RESULT_OK) {
                final Uri treeUri = resultData.getData();
                if (treeUri != null) {
                    final DocumentFile treeDocument = DocumentFile.fromTreeUri(this, treeUri);
                    if (treeDocument != null) {
                        copyRecursively(treeDocument, getExternalFilesDir(null));

                        final Intent intent = new Intent(this, MainActivity.class);
                        startActivity(intent);
                    }
                }
            }

            finish();
        } else {
            super.onActivityResult(requestCode, resultCode, resultData);
        }
    }

    private boolean copyRecursively(DocumentFile src, File dest) {
        final DocumentFile[] documentFiles = src.listFiles();
        for (final DocumentFile documentFile : documentFiles) {
            if (documentFile.isFile()) {
                if (!copyFile(documentFile, new File(dest, documentFile.getName()))) {
                    return false;
                }
            } else if (documentFile.isDirectory()) {
                final File subdirectory = new File(dest, documentFile.getName());
                if (!subdirectory.exists()) {
                    subdirectory.mkdir();
                }

                if (!copyRecursively(documentFile, subdirectory)) {
                    return false;
                }
            }
        }
        return true;
    }

    private boolean copyFile(DocumentFile src, File dest) {
        try {
            final InputStream inputStream = getContentResolver().openInputStream(src.getUri());
            final OutputStream outputStream = new FileOutputStream(dest);

            final byte[] buffer = new byte[16384];
            int bytesRead;
            while ((bytesRead = inputStream.read(buffer)) != -1) {
                outputStream.write(buffer, 0, bytesRead);
            }

            inputStream.close();
            outputStream.close();
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }

        return true;
    }
}
