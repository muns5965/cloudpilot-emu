package io.github.muns5965.cloudpilot;

import android.os.Process;
import android.util.Log;

import org.libsdl.app.SDLActivity;

/**
 * SDL screen, in its own process (":emu", see AndroidManifest.xml). Loads SDL2, SDL2_image and
 * libmain.so, then runs SDL_main (android_main.cpp): a TG50 ROM is booted, anything else shows
 * the test screen.
 *
 * The native core keeps global state in libmain.so, which is not reloaded within a process. So
 * when the activity is finished for good (back, or SDL_main returning), the :emu process ends
 * after SDLActivity.onDestroy() has stopped the SDL thread (nativeSendQuit + join) and called
 * nativeQuit(); the next start gets a fresh process. A configuration change does not end it.
 */
public class EmulatorActivity extends SDLActivity {
    public static final String EXTRA_ROM = "rom";
    public static final String EXTRA_SESSION = "session";
    private static final String TAG = "cp-android";

    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "SDL2_image", "main"};
    }

    @Override
    protected String[] getArguments() {
        // SDL_main arguments: [ROM path or ""] [session path]; none: the test screen.
        String rom = getIntent().getStringExtra(EXTRA_ROM);
        String session = getIntent().getStringExtra(EXTRA_SESSION);
        if (session == null) return rom == null || rom.isEmpty() ? new String[0] : new String[] {rom};
        return new String[] {rom == null ? "" : rom, session};
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (isFinishing() && !isChangingConfigurations()) {
            Log.i(TAG, "emulator finished, ending process " + Process.myPid());
            Process.killProcess(Process.myPid());
        }
    }
}
