package io.github.muns5965.cloudpilot;

import org.libsdl.app.SDLActivity;

/** SDL screen. Loads SDL2, SDL2_image and libmain.so, then runs SDL_main (android_main.cpp). */
public class EmulatorActivity extends SDLActivity {
    public static final String EXTRA_ROM = "rom";

    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "SDL2_image", "main"};
    }

    @Override
    protected String[] getArguments() {
        String rom = getIntent().getStringExtra(EXTRA_ROM);
        return rom == null || rom.isEmpty() ? new String[0] : new String[] {rom};
    }
}
