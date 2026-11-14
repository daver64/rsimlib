package com.daver64.simlib.android;

import android.content.res.AssetManager;
import android.os.Bundle;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import org.libsdl.app.SDLActivity;

public class SimlibActivity extends SDLActivity {
    private static final String ASSET_ROOT = "data";
    private static final String STAMP_FILE = ".simlib-assets-stamp";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        unpackAssets();
        super.onCreate(savedInstanceState);
    }

    // Games open data files by relative path, so the APK's assets/data tree is copied into the app's files directory,
    // which is the working directory.
    private void unpackAssets() {
        try {
            File filesDir = getFilesDir();
            File stamp = new File(filesDir, STAMP_FILE);
            String current = String.valueOf(getPackageManager().getPackageInfo(getPackageName(), 0).lastUpdateTime);
            if (stamp.isFile() && current.equals(readText(stamp))) {
                return;
            }
            copyTree(getAssets(), ASSET_ROOT, filesDir);
            try (OutputStream out = new FileOutputStream(stamp)) {
                out.write(current.getBytes("UTF-8"));
            }
        } catch (Exception e) {
            android.util.Log.e("simlib", "Unable to unpack bundled assets", e);
        }
    }

    private static String readText(File file) throws IOException {
        try (java.io.BufferedReader in = new java.io.BufferedReader(new java.io.FileReader(file))) {
            return in.readLine();
        }
    }

    private static void copyTree(AssetManager assets, String path, File target) throws IOException {
        String[] children = assets.list(path);
        if (children != null && children.length > 0) {
            if (!target.isDirectory() && !target.mkdirs()) {
                throw new IOException("Cannot create " + target);
            }
            for (String child : children) {
                copyTree(assets, path + "/" + child, new File(target, child));
            }
            return;
        }
        File parent = target.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("Cannot create " + parent);
        }
        try (InputStream in = assets.open(path); OutputStream out = new FileOutputStream(target)) {
            byte[] buffer = new byte[64 * 1024];
            int count;
            while ((count = in.read(buffer)) > 0) {
                out.write(buffer, 0, count);
            }
        }
    }
}
