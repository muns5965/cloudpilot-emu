// Android test entry, stage 2-A: SDLActivity (EmulatorActivity) calls this as SDL_main.
// No emulation yet. It opens a full-screen SDL window, logs renderer, lifecycle and touch events
// to logcat (tag SDL/APP), and, if a ROM was selected, parses its header with RomInfo5 (a pure
// parser: no device or memory setup). The ROM is not booted and its contents are not logged.

#include <SDL.h>
#include <SDL_image.h>

#include <cstdio>
#include <sstream>
#include <vector>

#include "rom_info5.h"

namespace {
    enum class RomState { none, unreadable, invalid, valid };

    RomState checkRom(const char* path) {
        FILE* file = fopen(path, "rb");
        if (!file) {
            SDL_Log("cp-android: cannot open ROM %s", path);
            return RomState::unreadable;
        }

        std::vector<uint8_t> buffer;
        uint8_t chunk[65536];
        size_t n;
        while ((n = fread(chunk, 1, sizeof(chunk), file)) > 0)
            buffer.insert(buffer.end(), chunk, chunk + n);
        const bool error = ferror(file);
        fclose(file);

        if (error) {
            SDL_Log("cp-android: read error on ROM");
            return RomState::unreadable;
        }

        RomInfo5 info(buffer.data(), buffer.size());
        SDL_Log("cp-android: ROM %zu bytes, RomInfo5 valid=%d", buffer.size(), info.IsValid());
        if (!info.IsValid()) return RomState::invalid;

        std::stringstream description;
        description << info;
        SDL_Log("cp-android: %s", description.str().c_str());

        return RomState::valid;
    }

    SDL_Color stateColor(RomState state) {
        switch (state) {
            case RomState::valid:
                return {0x2e, 0x7d, 0x32, 0xff};  // green
            case RomState::invalid:
                return {0xb2, 0x8a, 0x00, 0xff};  // amber
            case RomState::unreadable:
                return {0xb7, 0x1c, 0x1c, 0xff};  // red
            default:
                return {0x37, 0x47, 0x4f, 0xff};  // grey: no ROM selected
        }
    }

    const char* eventName(Uint32 type) {
        switch (type) {
            case SDL_APP_WILLENTERBACKGROUND:
                return "will enter background";
            case SDL_APP_DIDENTERBACKGROUND:
                return "did enter background";
            case SDL_APP_WILLENTERFOREGROUND:
                return "will enter foreground";
            case SDL_APP_DIDENTERFOREGROUND:
                return "did enter foreground";
            case SDL_APP_LOWMEMORY:
                return "low memory";
            case SDL_RENDER_TARGETS_RESET:
                return "render targets reset";
            case SDL_RENDER_DEVICE_RESET:
                return "render device reset";
            default:
                return nullptr;
        }
    }
}  // namespace

int main(int argc, char* argv[]) {
    SDL_Log("cp-android: start, %d argument(s)", argc - 1);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) < 0) {
        SDL_Log("cp-android: SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_version sdlVersion;
    SDL_GetVersion(&sdlVersion);
    const SDL_version* imgVersion = IMG_Linked_Version();
    const int imgFlags = IMG_Init(IMG_INIT_PNG);
    SDL_Log("cp-android: SDL %d.%d.%d, SDL_image %d.%d.%d, PNG %s", sdlVersion.major,
            sdlVersion.minor, sdlVersion.patch, imgVersion->major, imgVersion->minor,
            imgVersion->patch, (imgFlags & IMG_INIT_PNG) ? "ok" : "unavailable");

    const RomState romState = (argc > 1 && argv[1][0]) ? checkRom(argv[1]) : RomState::none;

    SDL_Window* window =
        SDL_CreateWindow("CloudpilotEmu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
                         SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer* renderer =
        window
            ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
            : nullptr;
    if (!renderer) {
        SDL_Log("cp-android: window/renderer failed: %s", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_RendererInfo rendererInfo;
    SDL_GetRendererInfo(renderer, &rendererInfo);
    int windowW, windowH, outputW, outputH;
    SDL_GetWindowSize(window, &windowW, &windowH);
    SDL_GetRendererOutputSize(renderer, &outputW, &outputH);
    SDL_Log("cp-android: renderer %s, window %dx%d, output %dx%d", rendererInfo.name, windowW,
            windowH, outputW, outputH);

    const SDL_Color background = stateColor(romState);
    bool foreground = true;
    bool running = true;
    bool touched = false;
    float touchX = 0, touchY = 0;  // normalised 0..1

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (const char* name = eventName(event.type)) SDL_Log("cp-android: event %s", name);

            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_APP_WILLENTERBACKGROUND:
                    foreground = false;
                    break;

                case SDL_APP_DIDENTERFOREGROUND:
                    foreground = true;
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        SDL_GetRendererOutputSize(renderer, &outputW, &outputH);
                        SDL_Log("cp-android: size changed %dx%d, output %dx%d", event.window.data1,
                                event.window.data2, outputW, outputH);
                    }
                    break;

                case SDL_FINGERDOWN:
                case SDL_FINGERMOTION:
                    touched = true;
                    touchX = event.tfinger.x;
                    touchY = event.tfinger.y;
                    if (event.type == SDL_FINGERDOWN)
                        SDL_Log("cp-android: finger down %.3f %.3f", touchX, touchY);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    SDL_Log("cp-android: mouse down %d %d (window coordinates)", event.button.x,
                            event.button.y);
                    break;

                case SDL_KEYDOWN:
                    SDL_Log("cp-android: key %s", SDL_GetScancodeName(event.key.keysym.scancode));
                    break;
            }
        }

        if (!foreground) {
            SDL_Delay(50);
            continue;
        }

        SDL_SetRenderDrawColor(renderer, background.r, background.g, background.b, 0xff);
        SDL_RenderClear(renderer);

        // Fixed frame inset by 5 % (shows the drawable area) and a marker at the last touch.
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
        const SDL_Rect frame{outputW / 20, outputH / 20, outputW - outputW / 10,
                             outputH - outputH / 10};
        SDL_RenderDrawRect(renderer, &frame);
        if (touched) {
            const int size = outputW / 12;
            const SDL_Rect marker{static_cast<int>(touchX * outputW) - size / 2,
                                  static_cast<int>(touchY * outputH) - size / 2, size, size};
            SDL_RenderFillRect(renderer, &marker);
        }

        SDL_RenderPresent(renderer);
    }

    SDL_Log("cp-android: exit");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
