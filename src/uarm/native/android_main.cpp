// Android entry: SDLActivity (EmulatorActivity) calls this as SDL_main.
//
// Stage 2-B: a ROM whose header identifies a Sony PEG-TG50 is booted, following the order of
// native/main.cpp run() (SocPXA, MainLoop, SdlRenderer, SdlEventHandler, SdlAudioDriver) without
// the console, debugger, NetLib, SD card and sessions. Anything else (no ROM, unreadable, invalid,
// another device) shows the stage 2-A test screen, whose colour gives the result.
// Logs go to logcat: this file under SDL/APP, the core's stdout/stderr under cp-core.

#include <SDL.h>
#include <SDL_image.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>

#include <cstdio>
#include <memory>
#include <sstream>
#include <vector>

#include "MainLoop.h"
#include "Rotation.h"
#include "SdlAudioDriver.h"
#include "SdlEventHandler.h"
#include "SdlRenderer.h"
#include "audio_queue.h"
#include "cputil.h"
#include "device.h"
#include "device_configuration.h"
#include "device_type5.h"
#include "rom_info5.h"
#include "soc_PXA.h"

namespace {
    constexpr size_t AUDIO_QUEUE_SIZE = 44100 / MAIN_LOOP_FPS * 10;  // as native/main.cpp
    constexpr unsigned int MIPS = 100;                               // native --mips default

    enum class RomState { none, unreadable, invalid, otherDevice, tg50 };

    // ---- core output to logcat ---------------------------------------------------------------

    void* forwardOutput(void* arg) {
        const int fd = static_cast<int>(reinterpret_cast<intptr_t>(arg));
        FILE* in = fdopen(fd, "r");
        char line[512];
        while (in && fgets(line, sizeof(line), in))
            __android_log_write(ANDROID_LOG_INFO, "cp-core", line);
        return nullptr;
    }

    void redirectOutputToLogcat() {
        int fds[2];
        if (pipe(fds) != 0) return;
        setvbuf(stdout, nullptr, _IOLBF, 0);
        setvbuf(stderr, nullptr, _IOLBF, 0);
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[1]);

        pthread_t thread;
        if (pthread_create(&thread, nullptr, forwardOutput,
                           reinterpret_cast<void*>(static_cast<intptr_t>(fds[0]))) == 0)
            pthread_detach(thread);
    }

    // ---- ROM -----------------------------------------------------------------------------------

    RomState readRom(const char* path, std::vector<uint8_t>& buffer) {
        FILE* file = fopen(path, "rb");
        if (!file) {
            SDL_Log("cp-android: cannot open ROM %s", path);
            return RomState::unreadable;
        }

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

        if (info.GetDeviceType() != DeviceType5::deviceTypeSonyTG50) {
            SDL_Log("cp-android: device type %d is not a Sony PEG-TG50; not booting",
                    info.GetDeviceType());
            return RomState::otherDevice;
        }

        return RomState::tg50;
    }

    // ---- stage 2-A test screen -----------------------------------------------------------------

    SDL_Color stateColor(RomState state) {
        switch (state) {
            case RomState::tg50:
                return {0x2e, 0x7d, 0x32, 0xff};  // green (only if booting was refused)
            case RomState::otherDevice:
                return {0x15, 0x65, 0xc0, 0xff};  // blue: valid OS 5 ROM, not a TG50
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

    int runTestScreen(RomState romState) {
        SDL_Window* window =
            SDL_CreateWindow("CloudpilotEmu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0,
                             0, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI);
        SDL_Renderer* renderer =
            window ? SDL_CreateRenderer(window, -1,
                                        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
                   : nullptr;
        if (!renderer) {
            SDL_Log("cp-android: window/renderer failed: %s", SDL_GetError());
            if (window) SDL_DestroyWindow(window);
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
                            SDL_Log("cp-android: size changed %dx%d, output %dx%d",
                                    event.window.data1, event.window.data2, outputW, outputH);
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
                        SDL_Log("cp-android: key %s",
                                SDL_GetScancodeName(event.key.keysym.scancode));
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

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return 0;
    }

    // ---- stage 2-B TG50 boot -------------------------------------------------------------------

    struct LcdArea {
        SDL_Renderer* renderer;
        int width, height;
    };

    // The LCD is letterboxed with SDL_RenderSetLogicalSize. SoC::PenDown clamps coordinates
    // to the screen, so a press on the black border would land on the nearest LCD edge: drop
    // presses outside the LCD. The filter runs before SDL's renderer event watch converts mouse
    // coordinates, so it sees window coordinates. Motion and release are left alone.
    int SDLCALL dropPressOutsideLcd(void* userdata, SDL_Event* event) {
        if (event->type != SDL_MOUSEBUTTONDOWN) return 1;

        const LcdArea* lcd = static_cast<const LcdArea*>(userdata);
        float x, y;
        SDL_RenderWindowToLogical(lcd->renderer, event->button.x, event->button.y, &x, &y);
        if (x >= 0 && y >= 0 && x < lcd->width && y < lcd->height) return 1;

        SDL_Log("cp-android: press outside the LCD ignored (%.1f %.1f)", x, y);
        return 0;
    }

    int bootTg50(std::vector<uint8_t>& nor) {
        const DeviceType5 deviceType = DeviceType5::deviceTypeSonyTG50;
        const uint32_t ramSize = 17ul << 20;  // native/main.cpp default for Sony devices

        if (!deviceConfigurationSupportsRamSize(deviceType, ramSize)) {
            SDL_Log("cp-android: unsupported RAM size %u", ramSize);
            return 1;
        }

        const DisplayMode displayMode = deviceConfigurationDefaultDisplayMode(deviceType);
        if (!deviceConfigurationDeviceSupportsDisplayMode(deviceType, displayMode)) {
            SDL_Log("cp-android: unsupported display mode");
            return 1;
        }

        if (deviceConfigurationGetNandSize(deviceType) != 0) {
            SDL_Log("cp-android: unexpected NAND size for TG50");
            return 1;
        }

        DisplayConfiguration displayConfiguration =
            deviceConfigurationDisplayConfigForMode(displayMode);
        // The TG50 has a keyboard instead of a silkscreen (as in native/main.cpp).
        displayConfiguration.graffitiHeight = 0;

        SDL_Log("cp-android: booting TG50, RAM %u bytes, LCD %ux%u", ramSize,
                displayConfiguration.width, displayConfiguration.height);

        // The ROM buffer stays alive until the process ends; the SoC keeps pointers into it.
        // The SoC is not deleted, as in native/main.cpp: the :emu process ends after SDL_main.
        SoC* soc = new SocPXA(deviceType, ramSize, nor.data(), nor.size(), nullptr, 0, -1,
                              deviceGetSocRev());

        AudioQueue* audioQueue = audioQueueCreate(AUDIO_QUEUE_SIZE);
        soc->SetAudioQueue(audioQueue);

        MainLoop mainLoop(soc);
        mainLoop.SetCyclesPerSecondLimit(MIPS * 1000000);

        const Rotation rotation = Rotation::portrait_0;
        const int scale = 1;  // SDL scales the logical LCD size to the phone screen

        // Portrait only in this stage (the Palm screen is square; rotation is not supported yet).
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait");
        SDL_Window* window = SDL_CreateWindow(
            "CloudpilotEmu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            displayConfiguration.width, displayConfiguration.height, SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window) {
            SDL_Log("cp-android: window failed: %s", SDL_GetError());
            return 1;
        }

        auto sdlRenderer =
            std::make_unique<SdlRenderer>(window, soc, scale, displayConfiguration, rotation);
        SDL_Renderer* renderer = SDL_GetRenderer(window);
        if (!renderer || SDL_RenderSetLogicalSize(renderer, displayConfiguration.width,
                                                  displayConfiguration.height) != 0) {
            SDL_Log("cp-android: renderer setup failed: %s", SDL_GetError());
            return 1;
        }

        int windowW, windowH, outputW, outputH;
        SDL_GetWindowSize(window, &windowW, &windowH);
        SDL_GetRendererOutputSize(renderer, &outputW, &outputH);
        SDL_Rect viewport;
        float scaleX, scaleY;
        SDL_RenderGetViewport(renderer, &viewport);
        SDL_RenderGetScale(renderer, &scaleX, &scaleY);
        SDL_Log(
            "cp-android: window %dx%d, output %dx%d, LCD viewport %d,%d %dx%d, scale %.3f x %.3f",
            windowW, windowH, outputW, outputH, viewport.x, viewport.y, viewport.w, viewport.h,
            scaleX, scaleY);

        LcdArea lcdArea{renderer, static_cast<int>(displayConfiguration.width),
                        static_cast<int>(displayConfiguration.height)};
        SDL_SetEventFilter(dropPressOutsideLcd, &lcdArea);

        SdlEventHandler sdlEventHandler(soc, scale, displayConfiguration, rotation, true);

        SdlAudioDriver audioDriver(soc, audioQueue);
        audioDriver.Start();

        uint64_t lastSpeedDump = timestampUsec();

        while (true) {
            const uint64_t now = timestampUsec();

            soc->SetPcmSuspended(audioDriver.GetAudioBackpressure());

            mainLoop.Cycle(now);

            sdlRenderer->Draw(sdlEventHandler.RedrawRequested());
            sdlEventHandler.ClearRedrawRequested();

            sdlEventHandler.HandleEvents();
            if (sdlEventHandler.QuitRequested()) break;

            if (now - lastSpeedDump > 5000000) {
                lastSpeedDump = now;
                const uint64_t currentIps = mainLoop.GetCurrentIps();
                const uint64_t currentIpsMax = mainLoop.GetCurrentIpsMax();
                SDL_Log("cp-android: %.2f MIPS, limit %.2f MIPS",
                        static_cast<float>(currentIps) / 1000000,
                        static_cast<float>(currentIpsMax) / 1000000);
            }

            const int64_t timesliceRemaining =
                mainLoop.GetTimesliceSizeUsec() - static_cast<int64_t>(timestampUsec() - now);
            if (timesliceRemaining > 10) usleep(timesliceRemaining);
        }

        SDL_Log("cp-android: emulator loop ended");
        SDL_SetEventFilter(nullptr, nullptr);
        audioDriver.Pause();

        return 0;
    }
}  // namespace

int main(int argc, char* argv[]) {
    redirectOutputToLogcat();
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

    std::vector<uint8_t> rom;
    const RomState romState = (argc > 1 && argv[1][0]) ? readRom(argv[1], rom) : RomState::none;

    const int result = romState == RomState::tg50 ? bootTg50(rom) : runTestScreen(romState);

    SDL_Log("cp-android: exit %d", result);
    IMG_Quit();
    SDL_Quit();

    return result;
}
