#ifndef _SDL_EVENT_HANDLER_
#define _SDL_EVENT_HANDLER_

#include "Rotation.h"
#include "SoC.h"
#include "device_configuration.h"

class SdlEventHandler {
   public:
    SdlEventHandler(SoC* soc, int scale, DisplayConfiguration& deviceDisplayConfiguration,
                    Rotation rotation, bool hardwareKeyboard = false);

    void HandleEvents();

    bool RedrawRequested() const;
    bool QuitRequested() const;
    void ClearRedrawRequested();

    void SetRotation(Rotation rotation);

   private:
    int RotateX(int x, int y);
    int RotateY(int x, int y);

   private:
    SoC* soc;
    bool hardwareKeyboard{false};
    // Blue is shared by F7 and the digit chords: release it only when neither holds it.
    bool blueKeyHeld{false};
    int digitChordsHeld{0};

    bool penDown{false};
    int scale{1};
    bool redrawRequested{false};
    bool quitRequested{false};

    int width;
    int height;
    Rotation rotation;

   private:
    SdlEventHandler();
    SdlEventHandler(const SdlEventHandler&);
    SdlEventHandler(SdlEventHandler&&);
    SdlEventHandler& operator=(const SdlEventHandler&);
    SdlEventHandler& operator=(SdlEventHandler&&);
};

#endif  // _SDL_EVENT_HANDLER_
