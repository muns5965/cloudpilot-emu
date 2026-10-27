#ifndef _ANDROID_SESSION_H_
#define _ANDROID_SESSION_H_

// TG50 session file for the Android build: the same SessionFile5 format and contents as the
// native save-session command (device type, display mode, RAM size, NOR, RAM, savestate). The
// file contains the ROM; it stays in app storage.

#include <cstdint>
#include <string>
#include <vector>

#include "device_configuration.h"

class SoC;

struct Tg50Image {
    std::vector<uint8_t> nor;
    std::vector<uint8_t> memory;     // empty: cold boot
    std::vector<uint8_t> savestate;  // empty: cold boot
    uint32_t ramSize{0};             // 0: default
    DisplayMode displayMode{};
    bool fromSession{false};

    void ColdBoot();  // keep the NOR, drop the saved state
};

enum class SessionLoad {
    none,         // no session file
    loaded,       // image filled from the session
    quarantined,  // unusable session moved to <path>.bad
    keptBad,      // unusable session could not be moved: do not save over it
};

// Reads and validates a session (SessionFile5, TG50, valid ROM header). An unusable file is moved
// to <path>.bad, never overwritten.
SessionLoad LoadSession(const std::string& path, Tg50Image& image);

// Moves an unusable session to <path>.bad. Returns false if it could not be moved.
bool QuarantineSession(const std::string& path);

// Saves the machine state like save-session, on the emulator thread with the CPU stopped. Writes
// <path>.tmp, fsyncs it, renames it over <path> and fsyncs the directory; on failure the previous
// session is left as it was.
bool SaveSession(SoC* soc, const std::string& path);

// Writes <path>.tmp, fsyncs it, renames it over <path> and fsyncs the directory. On failure
// <path> is left as it was. Also used for the install status file (AndroidInstall).
bool WriteFileDurable(const std::string& path, const uint8_t* data, size_t size);

#endif  // _ANDROID_SESSION_H_
