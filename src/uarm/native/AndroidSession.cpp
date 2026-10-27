#include "AndroidSession.h"

#include <SDL.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "SoC.h"
#include "device_type5.h"
#include "rom_info5.h"
#include "session/session_file5.h"
#include "timeutil.h"

namespace {
    bool readFile(const std::string& path, std::vector<uint8_t>& data) {
        FILE* file = fopen(path.c_str(), "rb");
        if (!file) return false;

        uint8_t chunk[65536];
        size_t n;
        while ((n = fread(chunk, 1, sizeof(chunk), file)) > 0)
            data.insert(data.end(), chunk, chunk + n);
        const bool error = ferror(file);
        fclose(file);

        return !error;
    }

    std::string directoryOf(const std::string& path) {
        const size_t slash = path.rfind('/');
        return slash == std::string::npos ? std::string(".") : path.substr(0, slash ? slash : 1);
    }
}  // namespace

bool WriteFileDurable(const std::string& path, const uint8_t* data, size_t size) {
    const std::string tmp = path + ".tmp";

    const int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) {
        SDL_Log("cp-android: session: cannot create %s: %s", tmp.c_str(), strerror(errno));
        return false;
    }

    bool ok = true;
    for (size_t written = 0; ok && written < size;) {
        const ssize_t n = write(fd, data + written, size - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            SDL_Log("cp-android: session: write failed: %s", strerror(errno));
            ok = false;
        } else {
            written += static_cast<size_t>(n);
        }
    }

    if (ok && fsync(fd) != 0) {
        SDL_Log("cp-android: session: fsync failed: %s", strerror(errno));
        ok = false;
    }
    if (close(fd) != 0) ok = false;

    if (!ok || rename(tmp.c_str(), path.c_str()) != 0) {
        if (ok) SDL_Log("cp-android: session: rename failed: %s", strerror(errno));
        unlink(tmp.c_str());
        return false;
    }

    // The new file is in place; a failed directory sync is only logged.
    const int dir = open(directoryOf(path).c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir < 0 || fsync(dir) != 0)
        SDL_Log("cp-android: session: directory sync failed: %s", strerror(errno));
    if (dir >= 0) close(dir);

    return true;
}

void Tg50Image::ColdBoot() {
    memory.clear();
    savestate.clear();
    ramSize = 0;
    fromSession = false;
}

bool QuarantineSession(const std::string& path) {
    const std::string bad = path + ".bad";
    if (rename(path.c_str(), bad.c_str()) == 0) {
        SDL_Log("cp-android: session: unusable session moved to %s", bad.c_str());
        return true;
    }

    SDL_Log("cp-android: session: cannot move unusable session (%s); it will not be overwritten",
            strerror(errno));
    return false;
}

SessionLoad LoadSession(const std::string& path, Tg50Image& image) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return SessionLoad::none;

    std::vector<uint8_t> data;
    SessionFile5 session;
    const char* problem = nullptr;

    if (!readFile(path, data)) {
        problem = "read error";
    } else if (!SessionFile5::IsSessionFile(data.size(), data.data()) ||
               !session.Deserialize(data.size(), data.data())) {
        problem = "not a valid session file";
    } else if (session.GetDeviceType() != DeviceType5::deviceTypeSonyTG50) {
        problem = "not a TG50 session";
    } else if (!RomInfo5(session.GetNor(), session.GetNorSize()).IsValid()) {
        problem = "invalid ROM in session";
    } else if (session.GetNandSize() != 0) {
        problem = "unexpected NAND in session";
    }

    if (problem) {
        SDL_Log("cp-android: session: %s: %s", path.c_str(), problem);
        return QuarantineSession(path) ? SessionLoad::quarantined : SessionLoad::keptBad;
    }

    auto bytes = [](const void* p, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        return std::vector<uint8_t>(b, b + n);
    };

    image.nor = bytes(session.GetNor(), session.GetNorSize());
    image.memory = bytes(session.GetMemory(), session.GetMemorySize());
    image.savestate = bytes(session.GetSavestate(), session.GetSavestateSize());
    image.ramSize = session.GetRamSize();
    image.displayMode = static_cast<DisplayMode>(session.GetDisplayMode());
    image.fromSession = true;

    SDL_Log("cp-android: session: loaded %s (%zu bytes)", path.c_str(), data.size());
    return SessionLoad::loaded;
}

bool SaveSession(SoC* soc, const std::string& path) {
    // As save-session: the saved RTC state is absolute time.
    if (palmEpochTestOffset() != 0) {
        SDL_Log("cp-android: session: not saved, time-skip offset active");
        return false;
    }

    const uint64_t start = SDL_GetTicks64();

    if (!soc->Save()) {
        SDL_Log("cp-android: session: failed to save state");
        return false;
    }

    SessionFile5 session;

    const Buffer rom = soc->GetRomData();
    const Buffer nand = soc->GetNandData();
    const Buffer memory = soc->GetMemoryData();
    const Buffer savestate = soc->GetSavestate();

    session.SetDeviceType(static_cast<int32_t>(soc->GetDeviceType()))
        .SetDisplayMode(static_cast<int32_t>(soc->GetDisplayMode()))
        .SetRamSize(soc->GetRamSize())
        .SetNor(rom.size, reinterpret_cast<uint8_t*>(rom.data))
        .SetNand(nand.size, reinterpret_cast<uint8_t*>(nand.data))
        .SetMemory(memory.size, reinterpret_cast<uint8_t*>(memory.data))
        .SetSavestate(savestate.size, reinterpret_cast<uint8_t*>(savestate.data));

    if (!session.Serialize()) {
        SDL_Log("cp-android: session: failed to serialize");
        return false;
    }

    const bool ok =
        WriteFileDurable(path, reinterpret_cast<const uint8_t*>(session.GetSerializedSession()),
                         session.GetSerializedSessionSize());

    SDL_Log("cp-android: session: %s %s (%zu bytes, %llu ms)", ok ? "saved" : "NOT saved",
            path.c_str(), static_cast<size_t>(session.GetSerializedSessionSize()),
            static_cast<unsigned long long>(SDL_GetTicks64() - start));

    return ok;
}
