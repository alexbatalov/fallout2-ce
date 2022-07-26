package com.alexbatalov.fallout2ce;

import android.os.Bundle;

import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {
    protected void onCreate(Bundle savedInstanceState) {
        // Needed to initialize `files` folder (and make it publicly accessible
        // for file managers) for user to upload assets.
        getExternalFilesDir(null);

        super.onCreate(savedInstanceState);
    }

    protected String[] getLibraries() {
        return new String[]{
            "fallout2-ce",
        };
    }
}
