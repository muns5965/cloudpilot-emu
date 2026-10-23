#include <stdexcept>
#pragma GCC diagnostic ignored "-Wmultichar"

#include <SDL.h>
#include <SDL_image.h>
#include <unistd.h>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include "Cli.h"
#include "Commands.h"
#include "FileUtil.h"
#include "Logging.h"
#include "MainLoop.h"
#include "Rotation.h"
#include "SdlAudioDriver.h"
#include "SdlEventHandler.h"
#include "SdlRenderer.h"
#include "argparse.h"
#include "audio_queue.h"
#include "buffer.h"
#include "cputil.h"
#include "device.h"
#include "device_configuration.h"
#include "device_type5.h"
#include "md5.h"
#include "rom_info5.h"
#include "sdcard.h"
#include "session/session_file5.h"
#include "soc_PXA.h"
#include "soc_pv.h"

using namespace std;

struct Options {
    string norOrSession;
    optional<string> nand;
    optional<string> sd;
    optional<unsigned int> gdbPort;
    unsigned int mips;
    bool disableAudio;
    optional<string> script;
    optional<unsigned int> ramSize;
    bool smallWindow;
    bool forceTg50;
    bool forceNx70v;
    optional<string> bootRom;
    optional<DisplayMode> displayMode;
};

namespace {
    constexpr size_t AUDIO_QUEUE_SIZE = 44100 / MAIN_LOOP_FPS * 10;

    // PEG-NX70V: the known dump holds device data, not the small boot ROM, at 0x0-0x1FFFF.
    // --boot-rom copies the first 0x18000 bytes of another Sony NOR image over it, in memory only.
    constexpr size_t NX70V_BOOT_ROM_SIZE = 0x18000;
    constexpr uint32_t PALM_CARD_SIGNATURE = 0xFEEDBEEF;

    uint32_t readLe32(const uint8_t* p) {
        return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
    }

    bool hasPalmCardHeaderAt(const uint8_t* data, size_t size, size_t offset) {
        return size >= offset + 12 && readLe32(data + offset + 8) == PALM_CARD_SIGNATURE;
    }

    bool applyNx70vBootRom(const Options& options, uint8_t* nor, size_t norSize) {
        if (!hasPalmCardHeaderAt(nor, norSize, 0x20000)) {
            cerr << "--nx70v: no Palm OS ROM header at 0x20000" << endl;
            return false;
        }

        if (hasPalmCardHeaderAt(nor, norSize, 0)) {
            if (options.bootRom) cerr << "--nx70v: ROM already has a boot ROM at 0x0; --boot-rom ignored" << endl;
            return true;
        }

        if (!options.bootRom) {
            cerr << "--nx70v: this ROM has no boot ROM at 0x0 (device data instead)." << endl
                 << "Pass --boot-rom <Sony NOR image>; its first 0x18000 bytes are used in memory only."
                 << endl;
            return false;
        }

        size_t bootLen{0};
        unique_ptr<uint8_t[]> bootData;
        if (!util::ReadFile(*options.bootRom, bootData, bootLen)) return false;

        if (!hasPalmCardHeaderAt(bootData.get(), bootLen, 0) || bootLen < NX70V_BOOT_ROM_SIZE) {
            cerr << "--boot-rom: no boot ROM (Palm OS card header) at 0x0" << endl;
            return false;
        }

        memcpy(nor, bootData.get(), NX70V_BOOT_ROM_SIZE);
        cerr << "using boot ROM from " << *options.bootRom << " (0x0-0x17FFF, in memory only)" << endl;

        return true;
    }

    uint32_t ramSizeFromMemorySize(uint32_t memorySize) {
        return 1 << (31 - __builtin_clz(memorySize));
    }

    int windowWidth(DisplayConfiguration& displayConfiguration, Rotation rotation) {
        switch (rotation) {
            case Rotation::landscape_90:
            case Rotation::landscape_270:
                return displayConfiguration.height + displayConfiguration.graffitiHeight;

            default:
                return displayConfiguration.width;
        }
    }

    int windowHeight(DisplayConfiguration& displayConfiguration, Rotation rotation) {
        switch (rotation) {
            case Rotation::landscape_90:
            case Rotation::landscape_270:
                return displayConfiguration.width;

            default:
                return displayConfiguration.height + displayConfiguration.graffitiHeight;
        }
    }

    SDL_Window* initSdl(DisplayConfiguration& displayConfiguration, int scale, Rotation rotation) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) < 0) {
            cerr << "could not initialize SDL: " << SDL_GetError() << endl;
            return nullptr;
        }

        IMG_Init(IMG_INIT_PNG);

        atexit(SDL_Quit);

        auto window = SDL_CreateWindow("cp-uarm", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                       scale * windowWidth(displayConfiguration, rotation),
                                       scale * windowHeight(displayConfiguration, rotation),
                                       SDL_WINDOW_ALLOW_HIGHDPI);

        return window;
    }

    void sdlResizeWindow(SDL_Window* window, DisplayConfiguration& displayConfiguration, int scale,
                         Rotation rotation) {
        SDL_SetWindowSize(window, scale * windowWidth(displayConfiguration, rotation),
                          scale * windowHeight(displayConfiguration, rotation));
    }

    void copy(Buffer& buffer, size_t size, const void* data) {
        if (size == 0 || data == nullptr) {
            buffer.data = nullptr;
            buffer.size = 0;

            return;
        }

        buffer.data = malloc(size);
        buffer.size = size;

        memcpy(buffer.data, data, size);
    }

    bool readSession(const Options& options, Buffer& nor, Buffer& nand, Buffer& ram,
                     Buffer& savestate, uint32_t& ramSize, RomInfo5& romInfo,
                     optional<DisplayMode>& displayMode, optional<DeviceType5>& sessionDeviceType) {
        SessionFile5 sessionFile;

        size_t norOrSessionLen{0};
        unique_ptr<uint8_t[]> norOrSessionData;
        if (!util::ReadFile(options.norOrSession, norOrSessionData, norOrSessionLen)) return false;

        if (SessionFile5::IsSessionFile(norOrSessionLen, norOrSessionData.get()) &&
            sessionFile.Deserialize(norOrSessionLen, norOrSessionData.get())) {
            if (options.forceTg50 || options.forceNx70v)
                cerr << "--tg50/--nx70v ignored: the session file records its device type" << endl;
            if (options.nand) {
                cerr << "separate NAND image cannot be used with session file" << endl;
                return false;
            }

            sessionDeviceType = static_cast<DeviceType5>(sessionFile.GetDeviceType());

            ramSize = sessionFile.GetRamSize();
            if (ramSize == 0) ramSize = ramSizeFromMemorySize(sessionFile.GetMemorySize());

            displayMode = static_cast<DisplayMode>(sessionFile.GetDisplayMode());
            copy(nor, sessionFile.GetNorSize(), sessionFile.GetNor());
            copy(nand, sessionFile.GetNandSize(), sessionFile.GetNand());
            copy(ram, sessionFile.GetMemorySize(), sessionFile.GetMemory());
            copy(savestate, sessionFile.GetSavestateSize(), sessionFile.GetSavestate());

            romInfo = RomInfo5(nor.data, nor.size);
        } else {
            romInfo = RomInfo5(norOrSessionData.get(), norOrSessionLen);
            if (!romInfo.IsValid()) {
                cerr << "provided file is neither a support ROM nor a session image" << endl;
                return false;
            }

            size_t nandLen{0};
            unique_ptr<uint8_t[]> nandData;

            if (options.nand) {
                if (!util::ReadFile(options.nand, nandData, nandLen)) return false;
            } else {
                nandLen = deviceConfigurationGetNandSize(romInfo.GetDeviceType());
                nandData = nandLen > 0 ? make_unique<uint8_t[]>(nandLen) : nullptr;
                memset(nandData.get(), 0xff, nandLen);
            }

            ramSize = 0;

            if (options.forceNx70v &&
                !applyNx70vBootRom(options, norOrSessionData.get(), norOrSessionLen))
                return false;

            nor.size = norOrSessionLen;
            nor.data = norOrSessionData.release();

            nand.size = nandLen;
            nand.data = nandData.release();

            ram.size = savestate.size = 0;
            ram.data = savestate.data = nullptr;
        }

        return true;
    }

    bool run(const Options& options) {
        if (options.mips == 0) {
            cerr << "MIPS must be finite" << endl;
            return false;
        }

        Buffer nor, nand, memory, savestate;
        RomInfo5 romInfo;
        optional<DisplayMode> displayMode = nullopt;
        optional<DeviceType5> sessionDeviceType = nullopt;
        uint32_t ramSize{0};

        if (!readSession(options, nor, nand, memory, savestate, ramSize, romInfo, displayMode,
                         sessionDeviceType))
            return false;
        cerr << romInfo;

        if (!romInfo.IsValid()) return false;
        // A session records its device type (Sony ROM headers are not identified).
        const DeviceType5 deviceType = sessionDeviceType      ? *sessionDeviceType
                                       : options.forceNx70v ? DeviceType5::deviceTypeSonyNX70V
                                       : options.forceTg50  ? DeviceType5::deviceTypeSonyTG50
                                                            : romInfo.GetDeviceType();
        const bool sonyDevice = deviceType == DeviceType5::deviceTypeSonyTG50 ||
                                deviceType == DeviceType5::deviceTypeSonyNX70V;
        if (deviceType == DeviceType5::deviceTypeInvalid) return false;
        if (sonyDevice && options.nand) {
            cerr << "TG50/NX70V have no E2-style NAND; omit --nand" << endl;
            return false;
        }

        if (options.ramSize) {
            if (ramSize == 0) {
                ramSize = *options.ramSize << 20;
            } else {
                cerr << "cannot specify RAM size for an existing session" << endl;
                return false;
            }
        }

        if (ramSize == 0)
            ramSize = sonyDevice ? (17ul << 20) : romInfo.GetRecommendedRamSize();

        if (!deviceConfigurationSupportsRamSize(deviceType, ramSize)) {
            cerr << "unsupported RAM size: " << ramSize << " bytes" << endl;
            return false;
        }

        cerr << "using RAM size: " << ramSize << " bytes" << endl << endl;

        if (!displayMode.has_value())
            displayMode = options.displayMode.value_or(
                deviceConfigurationDefaultDisplayMode(deviceType));

        if (!deviceConfigurationDeviceSupportsDisplayMode(deviceType, *displayMode)) {
            cerr << "unsupported display mode for device" << endl;
            return false;
        }

        const uint32_t nandSize = deviceConfigurationGetNandSize(deviceType);
        if (nand.size != nandSize) {
            cerr << "invalid NAND size; expected " << nandSize << " bytes" << endl;
            return false;
        }

        size_t sdLen{0};
        unique_ptr<uint8_t[]> sdData;
        if (options.sd && !util::ReadFile(options.sd, sdData, sdLen)) return false;

        if (sdData) {
            if (sdLen % SD_SECTOR_SIZE) {
                cout << "sd card image has bad size" << endl;
                return false;
            }

            string key = md5(sdData.get(), sdLen);
            sdCardInitializeWithData(sdLen / SD_SECTOR_SIZE, sdData.release(), key.c_str());
        }

        const int gdbPort = options.gdbPort.value_or(-1);

        DisplayConfiguration displayConfiguration =
            deviceConfigurationDisplayConfigForMode(*displayMode);

        SoC* soc =
            (deviceType == DeviceType5::deviceTypePV)
                ? static_cast<SoC*>(new SocPV(ramSize, nor.data, nor.size, *displayMode, gdbPort))
                : static_cast<SoC*>(new SocPXA(deviceType, ramSize, nor.data, nor.size,
                                               reinterpret_cast<uint8_t*>(nand.data), nand.size,
                                               gdbPort, deviceGetSocRev()));

        if (memory.data && memory.size > soc->GetMemoryData().size) {
            cerr << "RAM size mismatch" << endl;
            return false;
        }

        if (memory.data) {
            memcpy(soc->GetMemoryData().data, memory.data, memory.size);
            free(memory.data);
        }

        if (!soc->Load(savestate.size, savestate.data)) {
            cerr << "failed to restore savestate" << endl;
        }

        if (savestate.data) free(savestate.data);

        if (soc->SdInserted()) {
            if (!soc->SdRemount()) {
                cerr << "failed to remount SD card" << endl;
                sdCardReset();
            }
        } else if (sdCardInitialized()) {
            soc->SdInsert();
        }

        AudioQueue* audioQueue = audioQueueCreate(AUDIO_QUEUE_SIZE);
        soc->SetAudioQueue(audioQueue);

        MainLoop mainLoop(soc);
        mainLoop.SetCyclesPerSecondLimit(options.mips * 1000000);

        Rotation rotation = Rotation::portrait_0;

        int scale;
        if (displayConfiguration.density > 200) {
            scale = 1;
        } else if (displayConfiguration.density > 100) {
            scale = options.smallWindow ? 1 : 2;
        } else {
            scale = options.smallWindow ? 2 : 3;
        }

        SDL_Window* window = initSdl(displayConfiguration, scale, rotation);
        if (!window) {
            cerr << "failed to init SDL" << endl;
            return false;
        }

        auto sdlRenderer =
            make_unique<SdlRenderer>(window, soc, scale, displayConfiguration, rotation);
        SdlEventHandler sdlEventHandler(soc, scale, displayConfiguration, rotation,
                                        deviceType == DeviceType5::deviceTypeSonyTG50 ||
                                            deviceType == DeviceType5::deviceTypeSonyNX70V);

        SdlAudioDriver audioDriver(soc, audioQueue);
        if (!options.disableAudio) audioDriver.Start();

        commands::Register();
        cli::Start(options.script);
        commands::Context commandContext{
            .soc = soc, .mainLoop = mainLoop, .audioDriver = audioDriver, .rotation = rotation};

        uint64_t lastSpeedDump = timestampUsec();

        while (true) {
            uint64_t now = timestampUsec();

            if (!options.disableAudio) soc->SetPcmSuspended(audioDriver.GetAudioBackpressure());

            mainLoop.Cycle(now);

            sdlRenderer->Draw(sdlEventHandler.RedrawRequested());
            sdlEventHandler.ClearRedrawRequested();

            sdlEventHandler.HandleEvents();
            if (sdlEventHandler.QuitRequested()) break;

            if (now - lastSpeedDump > 1000000) {
                const uint64_t currentIps = mainLoop.GetCurrentIps();
                const uint64_t currentIpsMax = mainLoop.GetCurrentIpsMax();
                lastSpeedDump = now;

                ostringstream s;
                s << "cp-uarm @ " << fixed << setprecision(2)
                  << static_cast<float>(currentIps) / 1000000 << " MIPS, limit "
                  << static_cast<float>(currentIpsMax) / 1000000 << " MIPS -> "
                  << (100 * currentIps) / currentIpsMax << "%" << endl
                  << flush;

                SDL_SetWindowTitle(window, s.str().c_str());
            }

            const int64_t timesliceRemaining =
                mainLoop.GetTimesliceSizeUsec() - static_cast<int64_t>(timestampUsec() - now);

            if (cli::Execute(&commandContext)) break;

            if (commandContext.rotation != rotation) {
                rotation = commandContext.rotation;

                sdlRenderer.reset();
                sdlResizeWindow(window, displayConfiguration, scale, rotation);

                sdlRenderer =
                    make_unique<SdlRenderer>(window, soc, scale, displayConfiguration, rotation);
                sdlEventHandler.SetRotation(rotation);

                soc->SetFramebufferDirty();
            }

            if (timesliceRemaining > 10) usleep(timesliceRemaining);
        }

        audioDriver.Pause();
        cli::Stop();

        return true;
    }

}  // namespace

int main(int argc, char** argv) {
    argparse::ArgumentParser program("cp-uarm");

    program.add_description("cp-uarm emulates a Palm Tungsten E2");

    program.add_argument("nor_or_session").help("NOR rom or saved session").required();

    program.add_argument("--tg50")
        .help("experimental Sony PEG-TG50 mode (raw Palm OS 5 NOR ROM only)")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--nx70v")
        .help("experimental Sony PEG-NX70V mode (raw Palm OS 5 NOR ROM only)")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--boot-rom")
        .help("with --nx70v: Sony NOR image whose boot ROM (0x0-0x17FFF) is used in memory")
        .metavar("<NOR file>");

    program.add_argument("--nand", "-n").help("NAND rom file").metavar("<nand file>");

    program.add_argument("--sd", "-s").help("SD card file").metavar("<SD card file>");

    program.add_argument("--ram-size")
        .help("RAM size in MB (16 or 32)")
        .metavar("<size>")
        .scan<'u', unsigned int>();
    ;

    program.add_argument("--no-sound", "-q")
        .help("start with audio off")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--gdb")
        .help("listen for GDB")
        .metavar("<port>")
        .scan<'u', unsigned int>();

    program.add_argument("--mips")
        .help("target speed in MIPS")
        .metavar("<mips>")
        .scan<'u', unsigned int>()
        .default_value(100u);

    program.add_argument("--small-window")
        .help("use small window")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--display")
        .help(
            "display mode (if supported); valid are 160x160, 160x240, 240x240, 240x320, "
            "320x320, 320x480, 480x480, 480x720")
        .metavar("<resolution>")
        .action([](const string& value) {
            if (value == "160x160") return DisplayMode::mode_160x160;
            if (value == "160x220") return DisplayMode::mode_160x220;
            if (value == "240x240") return DisplayMode::mode_240x240;
            if (value == "240x320") return DisplayMode::mode_240x320;
            if (value == "320x320") return DisplayMode::mode_320x320;
            if (value == "320x480") return DisplayMode::mode_320x480;
            if (value == "480x480") return DisplayMode::mode_480x480;
            if (value == "480x720") return DisplayMode::mode_480x720;

            throw runtime_error("invalid display mode");
        });

    program.add_argument("--script").help("execute script on startup").metavar("<script file>");

    try {
        program.parse_args(argc, argv);
    } catch (const invalid_argument& e) {
        cerr << "invalid argument" << endl << endl;
        cerr << program;

        exit(1);
    } catch (const runtime_error& e) {
        cerr << e.what() << endl << endl;
        cerr << program;

        exit(1);
    }

    Options options = {.norOrSession = program.get("nor_or_session"),
                       .nand = program.present("--nand"),
                       .sd = program.present("--sd"),
                       .gdbPort = program.present<unsigned int>("--gdb"),
                       .mips = program.get<unsigned int>("--mips"),
                       .disableAudio = program.get<bool>("--no-sound"),
                       .script = program.present("--script"),
                       .ramSize = program.present<unsigned int>("--ram-size"),
                       .smallWindow = program.get<bool>("--small-window"),
                       .forceTg50 = program.get<bool>("--tg50"),
                       .forceNx70v = program.get<bool>("--nx70v"),
                       .bootRom = program.present("--boot-rom"),
                       .displayMode = program.present<DisplayMode>("--display")};

    if (options.forceTg50 && options.forceNx70v) {
        cerr << "--tg50 and --nx70v are mutually exclusive" << endl;
        exit(1);
    }
    if (options.bootRom && !options.forceNx70v) {
        cerr << "--boot-rom requires --nx70v" << endl;
        exit(1);
    }

    logEnable();

    if (!run(options)) exit(1);

    return 0;
}
