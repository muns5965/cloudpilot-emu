#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include "Commands.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "CPEndian.h"
#include "Cli.h"
#include "FileUtil.h"
#include "SoC.h"
#include "app_launcher.h"
#include "cputil.h"
#include "db_backup.h"
#include "db_installer.h"
#include "db_list.h"
#include "md5.h"
#include "sdcard.h"
#include "session/session_file5.h"
#include "syscall_dispatch.h"

using namespace std;

namespace {
    Rotation rotate(Rotation rotation) {
        switch (rotation) {
            case Rotation::portrait_0:
                return Rotation::landscape_90;

            case Rotation::landscape_90:
                return Rotation::portrait_180;

            case Rotation::portrait_180:
                return Rotation::landscape_270;

            default:
                return Rotation::portrait_0;
        }
    }

    void CmdSetMips(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (args.size() != 1) return env.PrintUsage();

        uint32_t mips;
        istringstream s(args[0]);

        s >> mips;

        if (s.fail() || !s.eof()) {
            cout << "invalid argument" << endl;
            return;
        }

        static_cast<commands::Context*>(context)->mainLoop.SetCyclesPerSecondLimit(mips * 1000000);
    }

    void CmdEnableAudio(vector<string> args, cli::CommandEnvironment& env, void* context) {
        static_cast<commands::Context*>(context)->audioDriver.Start();
    }

    void CmdDisableAudio(vector<string> args, cli::CommandEnvironment& env, void* context) {
        static_cast<commands::Context*>(context)->audioDriver.Pause();
    }

    void CmdUnmount(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (!sdCardInitialized()) {
            cout << "no sd card mounted" << endl;
            return;
        }

        auto ctx = reinterpret_cast<commands::Context*>(context);

        ctx->soc->SdEject();
        sdCardReset();
    }

    void CmdMount(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (sdCardInitialized()) {
            cout << "sd card already mounted" << endl;
            return;
        }

        if (args.size() != 1) return env.PrintUsage();

        size_t len{0};
        unique_ptr<uint8_t[]> data;

        if (!util::ReadFile(args[0], data, len)) {
            cout << "failed to read " << args[0];
            return;
        }

        if (len % SD_SECTOR_SIZE) {
            cout << "sd card image has bad size" << endl;
            return;
        }

        auto ctx = reinterpret_cast<commands::Context*>(context);
        string key = md5(data.get(), len);

        sdCardInitializeWithData(len / SD_SECTOR_SIZE, data.release(), key.c_str());
        ctx->soc->SdInsert();
    }

    void CmdReset(vector<string> args, cli::CommandEnvironment& env, void* context) {
        auto ctx = reinterpret_cast<commands::Context*>(context);

        commands::ReleaseAllInput(*ctx, "reset");
        ctx->soc->Reset();
    }

    void CmdResetNoExtensions(vector<string> args, cli::CommandEnvironment& env, void* context) {
        auto ctx = reinterpret_cast<commands::Context*>(context);

        commands::ReleaseAllInput(*ctx, "reset");
        ctx->soc->JamKey(keyIdUp, 2000);
        ctx->soc->Reset();
    }

    void CmdResetHard(vector<string> args, cli::CommandEnvironment& env, void* context) {
        auto ctx = reinterpret_cast<commands::Context*>(context);

        commands::ReleaseAllInput(*ctx, "reset");
        ctx->soc->JamKey(keyIdPower, 2000);
        ctx->soc->Reset();
    }

    void CmdRotate(vector<string> args, cli::CommandEnvironment& env, void* context) {
        auto ctx = reinterpret_cast<commands::Context*>(context);

        ctx->rotation = rotate(ctx->rotation);
    }

    void CmdSaveSession(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (args.size() != 1 && args.size() != 2) return env.PrintUsage();

        auto ctx = reinterpret_cast<commands::Context*>(context);

        if (sdCardInitialized()) {
            string key = md5(reinterpret_cast<uint8_t*>(sdCardData().data), sdCardData().size);
            sdCardRekey(key.c_str());
        }

        if (!ctx->soc->Save()) {
            cout << "failed to save state" << endl;
            return;
        }

        SessionFile5 sessionFile;

        const Buffer rom = ctx->soc->GetRomData();
        const Buffer nand = ctx->soc->GetNandData();
        const Buffer memory = ctx->soc->GetMemoryData();
        const Buffer savestate = ctx->soc->GetSavestate();
        const Buffer sd = sdCardData();

        sessionFile.SetDeviceType(static_cast<int32_t>(ctx->soc->GetDeviceType()))
            .SetDisplayMode(static_cast<int32_t>(ctx->soc->GetDisplayMode()))
            .SetRamSize(ctx->soc->GetRamSize())
            .SetNor(rom.size, reinterpret_cast<uint8_t*>(rom.data))
            .SetNand(nand.size, reinterpret_cast<uint8_t*>(nand.data))
            .SetMemory(memory.size, reinterpret_cast<uint8_t*>(memory.data))
            .SetSavestate(savestate.size, reinterpret_cast<uint8_t*>(savestate.data));

        if (!sessionFile.Serialize()) {
            cout << "failed to serialize session" << endl;
            return;
        }

        if (!util::WriteFile(args[0],
                             reinterpret_cast<const uint8_t*>(sessionFile.GetSerializedSession()),
                             sessionFile.GetSerializedSessionSize())) {
            cout << "failed to write session to " << args[0] << endl;
        } else {
            cout << "wrote session to " << args[0] << endl;
        }

        if (args.size() == 2 && sdCardInitialized()) {
            if (!util::WriteFile(args[1], reinterpret_cast<uint8_t*>(sd.data), sd.size)) {
                cout << "failed to write SD image to " << args[1] << endl;
            } else {
                cout << "wrote SD image to " << args[1] << endl;
            }
        }
    }

    void CmdSaveSd(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (args.size() != 1) return env.PrintUsage();

        if (!sdCardInitialized()) {
            cout << "no SD card mounted" << endl;
            return;
        }

        const Buffer sd = sdCardData();

        if (!util::WriteFile(args[0], reinterpret_cast<uint8_t*>(sd.data), sd.size)) {
            cout << "failed to write SD image to " << args[0] << endl;
        } else {
            cout << "wrote SD image to " << args[0] << endl;
        }
    }

    void CmdDumpMMU(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (args.size() != 0) return env.PrintUsage();
        auto ctx = reinterpret_cast<commands::Context*>(context);

        ctx->soc->DumpMMU();
    }

    void CmdOsVersion(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (args.size() != 0) return env.PrintUsage();
        auto ctx = reinterpret_cast<commands::Context*>(context);
        SyscallDispatch* sd = ctx->soc->GetSyscallDispatch();

        if (!syscallDispatchM68kSupport(sd)) {
            cout << "m68k syscalls not supported" << endl;
            return;
        }

        if (!syscallDispatchPrepare(sd)) {
            cout << "unable to prepare save environment for syscalls" << endl;
            return;
        }

        char version[32];
        const uint32_t ptr = syscall68k_SysGetOsVersionString(sd, SC_EXECUTE_FULL);

        if (!ptr) {
            cout << "failed to retrieve OS version string" << endl;
            return;
        }

        syscallDispatch_strncpy_toHost(sd, version, ptr, sizeof(version));
        version[sizeof(version) - 1] = '\0';

        if (syscall68k_MemPtrFree(sd, SC_EXECUTE_PURE, ptr) != 0) {
            cout << "very bad: failed to release m68k pointer" << endl;
            return;
        }

        cout << version << endl;
    }

    void CmdSetAutoOff(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (args.size() != 1) return env.PrintUsage();

        uint32_t timeout;
        if (!(istringstream(args[0]) >> timeout)) {
            cout << "invalid timeout" << endl;
            return env.PrintUsage();
        }

        auto ctx = reinterpret_cast<commands::Context*>(context);
        SyscallDispatch* sd = ctx->soc->GetSyscallDispatch();

        if (!syscallDispatchM68kSupport(sd)) {
            cout << "m68k syscalls not supported" << endl;
            return;
        }

        if (!syscallDispatchPrepare(sd)) {
            cout << "unable to prepare save environment for syscalls" << endl;
            return;
        }

        syscall_SysSetAutoOffTime(sd, SC_EXECUTE_PURE, timeout);
    }

    void CmdInstall(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (args.size() != 1) return env.PrintUsage();

        size_t len;
        unique_ptr<uint8_t[]> data;
        if (!util::ReadFile(args[0], data, len)) {
            cout << "unable to read " << args[0] << endl;
            return;
        }

        auto ctx = reinterpret_cast<commands::Context*>(context);

        const auto result = dbInstallerInstall(ctx->soc->GetSyscallDispatch(), len, data.get());

        switch (result) {
            case DB_INSTALL_RESULT_NEEDS_RESET:
                cout << "installation successful, needs reset" << endl;
                break;

            case DB_INSTALL_RESULT_OK:
                cout << "installation successful" << endl;
                break;

            case DB_INSTALL_RESULT_ERR_CORRUPT:
                cout << "database corrupt" << endl;
                break;

            case DB_INSTALL_RESULT_ERR_OOM:
                cout << "out of memory" << endl;
                break;

            case DB_INSTALL_RESULT_ERR_IS_OPEN:
                cout << "db cannot is open and cannot be deleted" << endl;
                break;

            case DB_INSTALL_RESULT_ERR_COULD_NOT_OVERWRITE:
                cout << "unable to overwrite db" << endl;
                break;

            case DB_INSTALL_RESULT_ERR_NOT_SUPPORTED:
                cout << "db installation not supported on this ROM" << endl;
                break;

            case DB_INSTALL_RESULT_ERR_NOT_CURRENTLY_POSSIBLE:
                cout << "db installation not currently possible" << endl;
                break;

            case DB_INSTALL_RESULT_ERR_UNKNOWN:
                cout << "unknown error during installation" << endl;
                break;
        }
    }

    void CmdDbList(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (args.size() != 0) return env.PrintUsage();
        auto ctx = reinterpret_cast<commands::Context*>(context);
        SyscallDispatch* sd = ctx->soc->GetSyscallDispatch();

        DbMetadataList metadataList;
        if (!dbListGet(sd, metadataList)) return;

        for (const auto& meta : metadataList) {
            union {
                uint32_t numeric;
                char text[5];
            } type, creator;

            type.text[4] = creator.text[4] = '\0';
            type.numeric = be32toh(meta.type);
            creator.numeric = be32toh(meta.creator);

            printf("ID 0x%08x cardNo %u index %u %s %s %s %s\n", meta.localID, meta.cardNo,
                   meta.index, meta.isRom ? "ROM" : "ram", type.text, creator.text, meta.name);
        }
    }

    void CmdDbExport(vector<string> args, cli::CommandEnvironment& env, void* context, int type) {
        if (args.size() != 1) return env.PrintUsage();

        auto ctx = reinterpret_cast<commands::Context*>(context);
        SyscallDispatch* sd = ctx->soc->GetSyscallDispatch();

        DbBackup backup(sd, type);

        if (!backup.Init()) {
            cout << "failed to initialize export" << endl;
            return;
        }

        while (backup.GetState() == DbBackup::STATE_IN_PROGRESS) {
            if (backup.Continue()) {
                if (backup.HasLastProcessedDb()) {
                    cout << "OK " << backup.GetLastProcessedDb() << endl;
                }
            } else {
                cout << "FAILED " << backup.GetLastProcessedDb() << endl;
            }
        }

        cout << endl;

        if (backup.GetState() != DbBackup::STATE_DONE) {
            cout << "export FAILED" << endl;
            return;
        }

        if (util::WriteFile(args[0], reinterpret_cast<const uint8_t*>(backup.GetArchiveData()),
                            backup.GetArchiveSize())) {
            cout << "export written to " << args[0] << endl;
        } else {
            cout << "failed to write export to " << args[0] << endl;
        }
    }

    void CmdDbExportAll(vector<string> args, cli::CommandEnvironment& env, void* context) {
        CmdDbExport(args, env, context, DbBackup::BACKUP_TYPE_EVERYTHING);
    }

    void CmdDbExportRamRom(vector<string> args, cli::CommandEnvironment& env, void* context) {
        CmdDbExport(args, env, context, DbBackup::BACKUP_TYPE_RAM_ROM);
    }

    void CmdDbExportRam(vector<string> args, cli::CommandEnvironment& env, void* context) {
        CmdDbExport(args, env, context, DbBackup::BACKUP_TYPE_RAM);
    }

    void CmdLaunch(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (args.size() != 1) return env.PrintUsage();

        auto ctx = reinterpret_cast<commands::Context*>(context);
        SyscallDispatch* sd = ctx->soc->GetSyscallDispatch();

        launchAppByName(sd, args[0].c_str());
    }

    // One line each, "key=value" fields, for test scripts. Read-only: no emulator state changes.
    void CmdRtc(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (args.size() != 0) return env.PrintUsage();

        SoC::RtcAlarmStatus s;
        if (!static_cast<commands::Context*>(context)->soc->GetRtcAlarmStatus(s)) {
            cout << "rtc n/a" << endl;
            return;
        }

        cout << "rtc rtcNow=" << s.rtcNow << " RTAR=" << s.alarm
             << " RTSR=" << static_cast<unsigned>(s.status) << " AL=" << (s.status & 1)
             << " ALE=" << ((s.status >> 2) & 1) << " lastCheck=" << s.lastCheck
             << " due=" << s.wakeDue << endl;
    }

    void CmdSleepState(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (args.size() != 0) return env.PrintUsage();

        const SoC* soc = static_cast<commands::Context*>(context)->soc;
        const int wakeRequested = soc->GetAlarmWakeRequested();

        cout << "sleep-state deep=" << soc->IsInDeepSleep() << " cpuIdle=" << soc->IsCpuIdle()
             << " wakeRequested=" << (wakeRequested < 0 ? string("n/a") : to_string(wakeRequested))
             << endl;
    }

    // "tap" and "key": press through the same SoC calls as the SDL input (PenDown/PenUp,
    // KeyDown/KeyUp) and release after a hold time, from the main loop (non-blocking).
    // "ok" means the press was handed to the SoC, "released" that PenUp()/KeyUp() was called;
    // neither says that the device or the app reacted.
    struct HeldInput {
        bool pen;
        KeyId key;
        uint64_t releaseAtUsec;
    };

    vector<HeldInput> heldInputs;

    const char* const KEY_NAMES[] = {
        nullptr,  "hard1",   "hard2",    "hard3",     "hard4",         "up",       "down",
        "left",   "right",   "select",   "power",     "kbdA",          "kbdB",     "kbdC",
        "kbdD",   "kbdE",    "kbdF",     "kbdG",      "kbdH",          "kbdI",     "kbdJ",
        "kbdK",   "kbdL",    "kbdM",     "kbdN",      "kbdO",          "kbdP",     "kbdQ",
        "kbdR",   "kbdS",    "kbdT",     "kbdU",      "kbdV",          "kbdW",     "kbdX",
        "kbdY",   "kbdZ",    "kbdSpace", "kbdEnter",  "kbdBackspace",  "kbdShift", "kbdCtrl",
        "kbdTab", "kbdCaps", "kbdComma", "kbdPeriod", "kbdUnderscore", "kbdBlue",  "kbdRed",
        "home",   "pageUp",  "pageDown", "jogBack",   "voice",         "graffiti"};
    static_assert(sizeof(KEY_NAMES) / sizeof(KEY_NAMES[0]) == keyIdGraffiti + 1,
                  "KEY_NAMES must follow enum KeyId");

    string lower(const string& s) {
        string r(s);
        for (auto& c : r) c = tolower(static_cast<unsigned char>(c));
        return r;
    }

    KeyId keyFromName(const string& name) {
        for (size_t i = 1; i < sizeof(KEY_NAMES) / sizeof(KEY_NAMES[0]); i++)
            if (lower(name) == lower(KEY_NAMES[i])) return static_cast<KeyId>(i);

        return keyInvalid;
    }

    bool parseUint(const string& arg, uint32_t& value) {
        istringstream s(arg);
        s >> value;

        return !s.fail() && s.eof();
    }

    bool parseHoldMs(const vector<string>& args, size_t index, uint32_t defaultMs, uint32_t& ms) {
        ms = defaultMs;
        if (args.size() <= index) return true;

        return parseUint(args[index], ms) && ms >= 1 && ms <= 60000;
    }

    bool isHeld(bool pen, KeyId key) {
        for (auto& held : heldInputs)
            if (held.pen == pen && (pen || held.key == key)) return true;

        return false;
    }

    void CmdTap(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (args.size() < 2 || args.size() > 3) return env.PrintUsage();
        auto ctx = static_cast<commands::Context*>(context);

        uint32_t x, y, ms;
        if (!parseUint(args[0], x) || !parseUint(args[1], y) || !parseHoldMs(args, 2, 250, ms)) {
            cout << "tap error invalid-argument" << endl;
            return;
        }

        if (static_cast<int>(x) >= ctx->screenWidth || static_cast<int>(y) >= ctx->screenHeight) {
            cout << "tap error out-of-range screen=" << ctx->screenWidth << "x" << ctx->screenHeight
                 << endl;
            return;
        }

        if (isHeld(true, keyInvalid)) {
            cout << "tap busy" << endl;
            return;
        }

        ctx->soc->PenDown(x, y);
        heldInputs.push_back(
            {.pen = true, .key = keyInvalid, .releaseAtUsec = timestampUsec() + ms * 1000ull});

        cout << "tap ok x=" << x << " y=" << y << " release_in=" << ms << endl;
    }

    void CmdKey(vector<string> args, cli::CommandEnvironment& env, void* context) {
        if (args.size() < 1 || args.size() > 2) return env.PrintUsage();
        auto ctx = static_cast<commands::Context*>(context);

        const KeyId key = keyFromName(args[0]);
        uint32_t ms;
        if (key == keyInvalid) {
            cout << "key error unknown-key " << args[0] << endl;
            return;
        }
        if (!parseHoldMs(args, 1, 200, ms)) {
            cout << "key error invalid-argument" << endl;
            return;
        }

        if (isHeld(false, key)) {
            cout << "key busy " << KEY_NAMES[key] << endl;
            return;
        }

        ctx->soc->KeyDown(key);
        heldInputs.push_back(
            {.pen = false, .key = key, .releaseAtUsec = timestampUsec() + ms * 1000ull});

        cout << "key ok " << KEY_NAMES[key] << " release_in=" << ms << endl;
    }

    void release(commands::Context& ctx, const HeldInput& held, const char* reason) {
        if (held.pen) {
            ctx.soc->PenUp();
            cout << "tap released";
        } else {
            ctx.soc->KeyUp(held.key);
            cout << "key " << KEY_NAMES[held.key] << " released";
        }

        if (reason) cout << " (" << reason << ")";
        cout << endl;
    }

    const vector<cli::Command> commandList(
        {{.name = "set-mips",
          .usage = "set-mips <mips>",
          .description = "Set target MIPS.",
          .cmd = CmdSetMips},
         {.name = "audio-on", .description = "Enable audio.", .cmd = CmdEnableAudio},
         {.name = "audio-off", .description = "Disable audio.", .cmd = CmdDisableAudio},
         {.name = "unmount", .description = "Unmount SD card.", .cmd = CmdUnmount},
         {.name = "mount",
          .usage = "mount <image>",
          .description = "Unmount SD card.",
          .cmd = CmdMount},
         {.name = "reset", .description = "Reset Pilot.", .cmd = CmdReset},
         {.name = "reset-no-ext",
          .description = "Reset Pilot w/o loading extensions.",
          .cmd = CmdResetNoExtensions},
         {.name = "reset-hard", .description = "Hard reset Pilot.", .cmd = CmdResetHard},
         {.name = "rotate", .description = "Rotate 90° CCW", .cmd = CmdRotate},
         {.name = "save-session",
          .usage = "save-session <session file> [card image]",
          .description = "Save session.",
          .cmd = CmdSaveSession},
         {.name = "save-sd",
          .usage = "save-sd <card image>",
          .description = "Save SD card.",
          .cmd = CmdSaveSd},
         {.name = "dump-mmu", .description = "Dump MMU mappings.", .cmd = CmdDumpMMU},
         {.name = "os-version", .description = "Retrieve OS version. ", .cmd = CmdOsVersion},
         {.name = "set-auto-off",
          .usage = "set-auto-off",
          .description = "Set auto off timeout (warning: wakeup will currently fail). ",
          .cmd = CmdSetAutoOff},
         {.name = "install",
          .usage = "install <database>",
          .description = "Install database.",
          .cmd = CmdInstall},
         {.name = "db-list", .description = "List databases.", .cmd = CmdDbList},
         {.name = "db-export-all",
          .usage = "db-export-all <file>",
          .description = "Export all databases.",
          .cmd = CmdDbExportAll},
         {.name = "db-export-rom",
          .usage = "db-export-rom <file>",
          .description = "Export RAM and ROM databases (excluding PACE cache).",
          .cmd = CmdDbExportRamRom},
         {.name = "db-export",
          .usage = "db-export <file>",
          .description = "Export all RAM databases (excluding PACE cache).",
          .cmd = CmdDbExportRam},
         {.name = "launch",
          .usage = "launch <name>",
          .description = "Launch app",
          .cmd = CmdLaunch},
         {.name = "rtc",
          .description =
              "Print the RTC alarm state, read-only. rtcNow: RTC time, seconds since 1904, "
              "host local time. due: result of the alarm-due test of the deep-sleep alarm "
              "wake (pxaRtcAlarmDue, 3 s early-wake lead). PXA devices only, else n/a.",
          .cmd = CmdRtc},
         {.name = "tap",
          .usage = "tap <x> <y> [ms]",
          .description = "Pen down at device screen coordinates, pen up after ms (default 250). "
                         "Prints \"tap ok\" when pressed and \"tap released\" when released.",
          .cmd = CmdTap},
         {.name = "key",
          .usage = "key <name> [ms]",
          .description =
              "Press a device key, release after ms (default 200). Names: power, home, "
              "up, down, left, right, select, hard1-4, kbdA-kbdZ, kbdSpace, kbdEnter, "
              "kbdBackspace, kbdShift, kbdCtrl, kbdTab, kbdCaps, kbdComma, kbdPeriod, "
              "kbdUnderscore, kbdBlue, kbdRed, pageUp, pageDown, jogBack, voice, graffiti "
              "(case-insensitive). Prints \"key ok\" / \"key <name> released\".",
          .cmd = CmdKey},
         {.name = "sleep-state",
          .description = "Print the sleep state, read-only. deep: PXA deep sleep (device asleep). "
                         "cpuIdle: CPU waiting for an interrupt (also while the device is on). "
                         "wakeRequested: alarm wake power-key press pending (PXA only, else n/a).",
          .cmd = CmdSleepState}});
}  // namespace

void commands::Register() { cli::AddCommands(commandList); }

void commands::ProcessScheduledInput(Context& context, uint64_t nowUsec) {
    for (auto it = heldInputs.begin(); it != heldInputs.end();) {
        if (it->releaseAtUsec > nowUsec) {
            it++;
            continue;
        }

        const HeldInput held = *it;
        it = heldInputs.erase(it);
        release(context, held, nullptr);
    }
}

void commands::ReleaseAllInput(Context& context, const char* reason) {
    // Clear first: no stale scheduled release may later hit a key pressed after the reset.
    const vector<HeldInput> held(heldInputs);
    heldInputs.clear();

    for (auto& input : held) release(context, input, reason);
}
