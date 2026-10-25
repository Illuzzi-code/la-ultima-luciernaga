package com.ericson.luciernaga;

import android.content.Intent;

import org.libsdl.app.SDLActivity;

/**
 * Actividad del juego. Además de SDLActivity, permite pasar argumentos al
 * binario nativo con:
 *
 *   adb shell am start -n com.ericson.luciernaga/.LuciernagaActivity \
 *       --es app_args "--autoplay"
 *
 * Sin extra, arranca como siempre.
 */
public class LuciernagaActivity extends SDLActivity {
    @Override
    protected String[] getArguments() {
        Intent intent = getIntent();
        String args = intent != null ? intent.getStringExtra("app_args") : null;
        if (args == null || args.trim().isEmpty()) {
            return new String[0];
        }
        return args.trim().split("\\s+");
    }
}
