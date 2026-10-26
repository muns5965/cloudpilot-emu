// Android entry: SDLActivity (EmulatorActivity) calls this as SDL_main.
//
// Stage 2-B: a ROM whose header identifies a Sony PEG-TG50 is booted, following the order of
// native/main.cpp run() (SocPXA, MainLoop, SdlRenderer, SdlEventHandler, SdlAudioDriver) without
// the console, debugger, NetLib and SD card, with on-screen buttons below the LCD
// (AndroidButtons) and the session saved and resumed (AndroidSession, stage 2-E). Anything else
// (no ROM, unreadable, invalid, another device) shows the stage 2-A test screen, whose colour
// gives the result.
// Logs go to logcat: this file under SDL/APP, the core's stdout/stderr under cp-core.

#include <SDL.h>
#include <SDL_image.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "AndroidButtons.h"
#include "AndroidSession.h"
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

    // Reads the pipe with read(), not stdio: a thread blocked in fgets() holds its FILE lock,
    // which exit() needs to flush all streams.
    void* forwardOutput(void* arg) {
        const int fd = static_cast<int>(reinterpret_cast<intptr_t>(arg));
        char line[512];
        size_t used = 0;

        while (true) {
            const ssize_t n = read(fd, line + used, sizeof(line) - 1 - used);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) break;
            used += static_cast<size_t>(n);

            // Write out every complete line, and a full buffer as it is.
            size_t start = 0;
            for (size_t i = 0; i < used; i++) {
                if (line[i] != '\n') continue;
                line[i] = 0;
                __android_log_write(ANDROID_LOG_INFO, "cp-core", line + start);
                start = i + 1;
            }
            if (start == 0 && used == sizeof(line) - 1) {
                line[used] = 0;
                __android_log_write(ANDROID_LOG_INFO, "cp-core", line);
                start = used;
            }
            memmove(line, line + start, used - start);
            used -= start;
        }

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

    // ---- app lifecycle ------------------------------------------------------------------------

    // SDL_HINT_ANDROID_BLOCK_ON_PAUSE is off (see main()): SDL does not block the event loop in
    // the background, so the emulator loop can save the session there, with the CPU stopped, and
    // then idle (no emulation, no drawing) until the app is in the foreground again. The watch
    // runs on the thread that sends the app events (SDL's event pump, the emulator thread).
    std::atomic<bool> inBackground{false};

    int SDLCALL watchLifecycle(void*, SDL_Event* event) {
        if (event->type == SDL_APP_WILLENTERBACKGROUND) inBackground = true;
        if (event->type == SDL_APP_DIDENTERFOREGROUND) inBackground = false;
        return 0;
    }

    // ---- TG50 boot (stage 2-B) and session (stage 2-E) -----------------------------------------

    enum class BootResult { ok, failed, badSession };

    // Boots image (cold from its NOR, or from the session state) in the order of native/main.cpp
    // run(). sessionPath empty: no saving. The session is saved when the app goes to the background
    // and when the emulator quits.
    BootResult bootTg50(Tg50Image& image, const std::string& sessionPath) {
        const DeviceType5 deviceType = DeviceType5::deviceTypeSonyTG50;
        // native/main.cpp: the session's RAM size, else 17 MiB for Sony devices
        const uint32_t ramSize = image.fromSession && image.ramSize ? image.ramSize : 17ul << 20;

        if (!deviceConfigurationSupportsRamSize(deviceType, ramSize)) {
            SDL_Log("cp-android: unsupported RAM size %u", ramSize);
            return image.fromSession ? BootResult::badSession : BootResult::failed;
        }

        const DisplayMode displayMode = image.fromSession
                                            ? image.displayMode
                                            : deviceConfigurationDefaultDisplayMode(deviceType);
        if (!deviceConfigurationDeviceSupportsDisplayMode(deviceType, displayMode)) {
            SDL_Log("cp-android: unsupported display mode");
            return image.fromSession ? BootResult::badSession : BootResult::failed;
        }

        if (deviceConfigurationGetNandSize(deviceType) != 0) {
            SDL_Log("cp-android: unexpected NAND size for TG50");
            return BootResult::failed;
        }

        DisplayConfiguration displayConfiguration =
            deviceConfigurationDisplayConfigForMode(displayMode);
        // The TG50 has a keyboard instead of a silkscreen (as in native/main.cpp).
        displayConfiguration.graffitiHeight = 0;
        if (displayConfiguration.width != AndroidButtons::kWidth ||
            displayConfiguration.height != AndroidButtons::kLcdHeight) {
            SDL_Log("cp-android: unexpected LCD size %ux%u", displayConfiguration.width,
                    displayConfiguration.height);
            return image.fromSession ? BootResult::badSession : BootResult::failed;
        }

        SDL_Log("cp-android: booting TG50 (%s), RAM %u bytes, LCD %ux%u",
                image.fromSession ? "session" : "cold", ramSize, displayConfiguration.width,
                displayConfiguration.height);

        // The NOR buffer stays alive until the process ends; the SoC keeps pointers into it.
        // The SoC is not deleted, as in native/main.cpp: the :emu process ends after SDL_main.
        SoC* soc = new SocPXA(deviceType, ramSize, image.nor.data(), image.nor.size(), nullptr, 0,
                              -1, deviceGetSocRev());

        // Session state, as native/main.cpp after readSession().
        if (image.fromSession) {
            if (image.memory.size() > soc->GetMemoryData().size) {
                SDL_Log("cp-android: session RAM size mismatch");
                return BootResult::badSession;
            }
            memcpy(soc->GetMemoryData().data, image.memory.data(), image.memory.size());

            // Unlike native/main.cpp (which continues from the initial state), a failed restore
            // is a bad session: continuing would save a fresh state over it.
            if (!soc->Load(image.savestate.size(), image.savestate.data())) {
                SDL_Log("cp-android: failed to restore savestate");
                return BootResult::badSession;
            }
        }

        AudioQueue* audioQueue = audioQueueCreate(AUDIO_QUEUE_SIZE);
        soc->SetAudioQueue(audioQueue);

        MainLoop mainLoop(soc);
        mainLoop.SetCyclesPerSecondLimit(MIPS * 1000000);

        const Rotation rotation = Rotation::portrait_0;
        // SDL scales the logical size (LCD at 0,0 and the on-screen buttons below it) to the
        // phone screen; SdlRenderer draws the LCD at scale 1.
        const int scale = 1;

        // Portrait only in this stage (the Palm screen is square; rotation is not supported yet).
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait");
        SDL_Window* window = SDL_CreateWindow("CloudpilotEmu", SDL_WINDOWPOS_UNDEFINED,
                                              SDL_WINDOWPOS_UNDEFINED, AndroidButtons::kWidth,
                                              AndroidButtons::kHeight, SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window) {
            SDL_Log("cp-android: window failed: %s", SDL_GetError());
            return BootResult::failed;
        }

        auto sdlRenderer =
            std::make_unique<SdlRenderer>(window, soc, scale, displayConfiguration, rotation);
        SDL_Renderer* renderer = SDL_GetRenderer(window);
        if (!renderer || SDL_RenderSetLogicalSize(renderer, AndroidButtons::kWidth,
                                                  AndroidButtons::kHeight) != 0) {
            SDL_Log("cp-android: renderer setup failed: %s", SDL_GetError());
            return BootResult::failed;
        }

        int windowW, windowH, outputW, outputH;
        SDL_GetWindowSize(window, &windowW, &windowH);
        SDL_GetRendererOutputSize(renderer, &outputW, &outputH);
        SDL_Rect viewport;
        float scaleX, scaleY;
        SDL_RenderGetViewport(renderer, &viewport);
        SDL_RenderGetScale(renderer, &scaleX, &scaleY);
        SDL_Log("cp-android: window %dx%d, output %dx%d, viewport %d,%d %dx%d, scale %.3f x %.3f",
                windowW, windowH, outputW, outputH, viewport.x, viewport.y, viewport.w, viewport.h,
                scaleX, scaleY);

        // On-screen buttons: drawn before each present, presses recorded by the event filter
        // (LCD presses pass through as pen input; anything else outside the LCD is dropped).
        AndroidButtons buttons(renderer);
        sdlRenderer->SetBeforePresent([&buttons](SDL_Renderer* r) { buttons.Draw(r); });
        SDL_SetEventFilter(AndroidButtons::Filter, &buttons);

        SdlEventHandler sdlEventHandler(soc, scale, displayConfiguration, rotation, true);

        SdlAudioDriver audioDriver(soc, audioQueue);
        audioDriver.Start();

        uint64_t lastSpeedDump = timestampUsec();
        bool background = false;

        while (true) {
            // In the background: save once with the CPU stopped, then only handle events.
            if (inBackground) {
                if (!background) {
                    background = true;
                    SDL_Log("cp-android: background");
                    if (!sessionPath.empty()) SaveSession(soc, sessionPath);
                }

                buttons.PushKeyEvents();
                sdlEventHandler.HandleEvents();
                if (sdlEventHandler.QuitRequested()) break;

                SDL_Delay(50);
                continue;
            }

            bool forceRedraw = false;
            if (background) {
                background = false;
                forceRedraw = true;
                SDL_Log("cp-android: foreground");
            }

            const uint64_t now = timestampUsec();

            soc->SetPcmSuspended(audioDriver.GetAudioBackpressure());

            mainLoop.Cycle(now);

            // Button presses become SDL key events, handled by HandleEvents() below.
            const bool buttonsChanged = buttons.PushKeyEvents();

            sdlRenderer->Draw(sdlEventHandler.RedrawRequested() || buttonsChanged || forceRedraw);
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
        sdlRenderer->SetBeforePresent(nullptr);
        audioDriver.Pause();

        // Quit (back, or the activity finishing): save with the CPU stopped.
        if (!sessionPath.empty()) SaveSession(soc, sessionPath);

        return BootResult::ok;
    }
}  // namespace

// Arguments (EmulatorActivity): [ROM path or ""] [session path]. A usable TG50 session is
// resumed (no ROM needed); an unusable one is moved to <session>.bad and the ROM, else the
// session's own NOR, is booted cold. Without a TG50 ROM or session: the test screen.
int main(int argc, char* argv[]) {
    redirectOutputToLogcat();
    SDL_Log("cp-android: start, %d argument(s)", argc - 1);

    // Must be set before SDL_Init (read when the Android video driver starts).
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) < 0) {
        SDL_Log("cp-android: SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_AddEventWatch(watchLifecycle, nullptr);

    SDL_version sdlVersion;
    SDL_GetVersion(&sdlVersion);
    const SDL_version* imgVersion = IMG_Linked_Version();
    const int imgFlags = IMG_Init(IMG_INIT_PNG);
    SDL_Log("cp-android: SDL %d.%d.%d, SDL_image %d.%d.%d, PNG %s", sdlVersion.major,
            sdlVersion.minor, sdlVersion.patch, imgVersion->major, imgVersion->minor,
            imgVersion->patch, (imgFlags & IMG_INIT_PNG) ? "ok" : "unavailable");

    const char* romPath = argc > 1 && argv[1][0] ? argv[1] : nullptr;
    std::string sessionPath = argc > 2 ? argv[2] : "";

    Tg50Image image;
    RomState romState = RomState::none;

    if (!sessionPath.empty()) {
        switch (LoadSession(sessionPath, image)) {
            case SessionLoad::loaded:
                romState = RomState::tg50;
                break;

            case SessionLoad::keptBad:
                sessionPath.clear();  // never save over a session that could not be moved
                break;

            default:
                break;
        }
    }

    if (romState != RomState::tg50 && romPath) {
        romState = readRom(romPath, image.nor);
        image.ColdBoot();
    }

    int result;
    if (romState == RomState::tg50) {
        BootResult boot = bootTg50(image, sessionPath);

        if (boot == BootResult::badSession) {
            // The session state is unusable: keep the file aside, boot its NOR (or the ROM) cold.
            if (!QuarantineSession(sessionPath)) sessionPath.clear();
            if (romPath) {
                image.nor.clear();
                if (readRom(romPath, image.nor) != RomState::tg50) image.nor.clear();
            }
            image.ColdBoot();
            boot = image.nor.empty() ? BootResult::failed : bootTg50(image, sessionPath);
        }

        result = boot == BootResult::ok ? 0 : 1;
    } else {
        result = runTestScreen(romState);
    }

    SDL_Log("cp-android: exit %d", result);
    IMG_Quit();
    SDL_Quit();

    return result;
}
