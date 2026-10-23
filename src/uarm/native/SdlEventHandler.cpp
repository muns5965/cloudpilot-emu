#include "SdlEventHandler.h"

#include <SDL.h>

#include "keys.h"

namespace {
    // Hardware keyboard (TG50). Return selects Enter here, so the jog dial
    // select moves to F11 as in original uARM.
    enum KeyId mapKeyboardKey(SDL_Keycode keysym) {
        if (keysym >= SDLK_a && keysym <= SDLK_z)
            return static_cast<KeyId>(keyIdKbdA + (keysym - SDLK_a));

        switch (keysym) {
            case SDLK_SPACE:
                return keyIdKbdSpace;
            case SDLK_RETURN:
                return keyIdKbdEnter;
            case SDLK_BACKSPACE:
                return keyIdKbdBackspace;
            case SDLK_LSHIFT:
            case SDLK_RSHIFT:
                return keyIdKbdShift;
            case SDLK_LCTRL:
            case SDLK_RCTRL:
                return keyIdKbdCtrl;
            case SDLK_TAB:
                return keyIdKbdTab;
            case SDLK_CAPSLOCK:
                return keyIdKbdCaps;
            case SDLK_COMMA:
                return keyIdKbdComma;
            case SDLK_PERIOD:
                return keyIdKbdPeriod;
            case SDLK_MINUS:
                return keyIdKbdUnderscore;
            case SDLK_F7:
                return keyIdKbdBlue;
            case SDLK_F9:
                return keyIdKbdRed;
            case SDLK_HOME:
                return keyIdHome;
            case SDLK_PAGEUP:
                return keyIdPageUp;
            case SDLK_PAGEDOWN:
                return keyIdPageDown;
            case SDLK_END:
                return keyIdJogBack;
            case SDLK_F11:
                return keyIdSelect;
            case SDLK_F5:
                return keyIdVoice;
            case SDLK_BACKQUOTE:
                return keyIdGraffiti;
            default:
                return keyInvalid;
        }
    }

    enum KeyId mapKey(SDL_Keycode keysym, bool hardwareKeyboard) {
        if (hardwareKeyboard) {
            const enum KeyId key = mapKeyboardKey(keysym);
            if (key != keyInvalid) return key;
        }

        switch (keysym) {
            case SDLK_F1:
                return keyIdHard1;

            case SDLK_F2:
                return keyIdHard2;

            case SDLK_F3:
                return keyIdHard3;

            case SDLK_F4:
                return keyIdHard4;

            case SDLK_LEFT:
                return keyIdLeft;

            case SDLK_RIGHT:
                return keyIdRight;

            case SDLK_UP:
                return keyIdUp;

            case SDLK_DOWN:
                return keyIdDown;

            case SDLK_RETURN:
                return keyIdSelect;

            case SDLK_ESCAPE:
                return keyIdPower;

            default:
                return keyInvalid;
        }
    }
}  // namespace

SdlEventHandler::SdlEventHandler(SoC* soc, int scale,
                                 DisplayConfiguration& deviceDisplayConfiguration,
                                 Rotation rotation, bool hardwareKeyboard)
    : soc(soc), hardwareKeyboard(hardwareKeyboard), scale(scale), rotation(rotation) {
    width = deviceDisplayConfiguration.width;
    height = deviceDisplayConfiguration.height + deviceDisplayConfiguration.graffitiHeight;
}

void SdlEventHandler::HandleEvents() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                quitRequested = true;
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button != SDL_BUTTON_LEFT) break;
                penDown = true;
                soc->PenDown(RotateX(event.button.x, event.button.y) / scale,
                             RotateY(event.button.x, event.button.y) / scale);

                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button != SDL_BUTTON_LEFT) break;
                penDown = false;
                soc->PenUp();

                break;

            case SDL_MOUSEMOTION:
                if (!penDown) break;

                soc->PenDown(RotateX(event.motion.x, event.motion.y) / scale,
                             RotateY(event.motion.x, event.motion.y) / scale);

                break;

            case SDL_KEYDOWN: {
                enum KeyId key = mapKey(event.key.keysym.sym, hardwareKeyboard);
                if (key) soc->KeyDown(key);
                break;
            }

            case SDL_KEYUP: {
                enum KeyId key = mapKey(event.key.keysym.sym, hardwareKeyboard);
                if (key) soc->KeyUp(key);
                break;
            }

            case SDL_WINDOWEVENT_EXPOSED:
                redrawRequested = true;
                break;
        }
    }
}

bool SdlEventHandler::RedrawRequested() const { return redrawRequested; }

bool SdlEventHandler::QuitRequested() const { return quitRequested; }

void SdlEventHandler::ClearRedrawRequested() { redrawRequested = false; }

void SdlEventHandler::SetRotation(Rotation rotation) { this->rotation = rotation; }

int SdlEventHandler::RotateX(int x, int y) {
    switch (rotation) {
        case Rotation::landscape_90:
            return scale * width - y;

        case Rotation::portrait_180:
            return scale * width - x;

        case Rotation::landscape_270:
            return y;

        default:
            return x;
    }
}

int SdlEventHandler::RotateY(int x, int y) {
    switch (rotation) {
        case Rotation::landscape_90:
            return x;

        case Rotation::portrait_180:
            return scale * height - y;

        case Rotation::landscape_270:
            return scale * height - x;

        default:
            return y;
    }
}
