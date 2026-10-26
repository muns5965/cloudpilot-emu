#ifndef _ANDROID_BUTTONS_H_
#define _ANDROID_BUTTONS_H_

// On-screen TG50 buttons below the LCD (Android build only).
//
// Layout, in the logical units the LCD is drawn in (SDL_RenderSetLogicalSize, LCD at 0,0):
//   y   0..320  LCD (pen input, handled by SdlEventHandler)
//   y 320..380  hard buttons strip: the CLIE NR70 artwork the web version also uses for the TG50
//               (not TG50 artwork); regions as in the web version's determineButton():
//               hard 1 | hard 2 | up / down | hard 3 | hard 4
//   y 380..420  extra keys: power, home, back (jog back), select (jog select), page up, page down
//
// A press on a button becomes an SDL key event with the keycode SdlEventHandler already maps for
// the TG50 (F1-F4, Up, Down, Escape, Home, End, F11, PageUp, PageDown), so key handling stays in
// SdlEventHandler. The event filter (Filter(), called on the thread that sends the touch, the
// Android UI thread) only records presses and releases and drops the mouse event; the emulator
// thread pushes the key events (PushKeyEvents()), in order. Releases are never dropped: a press
// that moves off its button, is cancelled, or is still held when the app goes to the background is
// released.

#include <SDL.h>

#include <deque>
#include <mutex>

class AndroidButtons {
   public:
    static constexpr int kWidth = 320;
    static constexpr int kLcdHeight = 320;
    static constexpr int kStripTop = 320;
    static constexpr int kStripHeight = 60;
    static constexpr int kRowTop = kStripTop + kStripHeight;
    static constexpr int kRowHeight = 40;
    static constexpr int kHeight = kRowTop + kRowHeight;

    struct Button {
        SDL_Rect area;    // logical units
        SDL_Keycode key;  // as mapped by SdlEventHandler for the TG50
    };

    // Renderer thread. Rasterises the artwork at the renderer's output width.
    explicit AndroidButtons(SDL_Renderer* renderer);
    ~AndroidButtons();

    // Renderer thread, from SdlRenderer's before-present callback. Leaves the renderer's draw
    // colour and blend mode as they were.
    void Draw(SDL_Renderer* renderer);

    // SDL event filter (any thread). Returns 0 to drop the event.
    static int SDLCALL Filter(void* self, SDL_Event* event);

    // Emulator thread: pushes recorded presses / releases as SDL key events. Returns true if the
    // drawn pressed state changed.
    bool PushKeyEvents();

   private:
    struct KeyEvent {
        const Button* button;
        bool down;
    };

    int FilterEvent(SDL_Event* event);
    const Button* ButtonAt(float x, float y) const;
    void Release();  // with mutex held

   private:
    SDL_Renderer* renderer;
    SDL_Texture* stripTexture{nullptr};
    SDL_Texture* rowTexture{nullptr};

    std::mutex mutex;
    std::deque<KeyEvent> pending;  // guarded by mutex
    const Button* held{nullptr};   // guarded by mutex

    const Button* drawnPressed{nullptr};  // renderer / emulator thread

   private:
    AndroidButtons(const AndroidButtons&) = delete;
    AndroidButtons& operator=(const AndroidButtons&) = delete;
};

#endif  // _ANDROID_BUTTONS_H_
