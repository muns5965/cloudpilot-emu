#include "AndroidInstall.h"

#include <SDL.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>

#include "AndroidSession.h"
#include "SoC.h"
#include "db_installer.h"
#include "db_list.h"
#include "system_state.h"

namespace {
    constexpr size_t kMaxFileSize = 16u << 20;  // as the start screen
    constexpr size_t kMinFileSize = 78;         // PDB / PRC header
    constexpr int kMaxAttempts = 15;
    constexpr uint64_t kRetryUsec = 2000000;
    constexpr uint64_t kSaveAfterResetUsec = 10000000;
    constexpr size_t kMaxHistory = 50;
    constexpr const char* kStatusHeader = "# cloudpilot install status 1";

    uint64_t wallMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    // Palm database names are Latin-1 (or another 8-bit encoding): keep ASCII only for the status
    // file, which the start screen reads as UTF-8.
    std::string asciiName(const char* name, size_t maxLen) {
        std::string result;
        for (size_t i = 0; i < maxLen && name[i]; i++) {
            const unsigned char c = name[i];
            result += c >= 0x20 && c < 0x7f && c != '\t' ? static_cast<char>(c) : '?';
        }
        return result;
    }

    bool readFile(const std::string& path, std::vector<uint8_t>& data, size_t maxSize) {
        FILE* file = fopen(path.c_str(), "rb");
        if (!file) return false;

        uint8_t chunk[65536];
        size_t n;
        bool ok = true;
        while ((n = fread(chunk, 1, sizeof(chunk), file)) > 0) {
            if (data.size() + n > maxSize) {
                ok = false;
                break;
            }
            data.insert(data.end(), chunk, chunk + n);
        }
        if (ferror(file)) ok = false;
        fclose(file);

        return ok;
    }

    const char* resultReason(int32_t result) {
        switch (result) {
            case DB_INSTALL_RESULT_ERR_CORRUPT:
                return "corrupt";
            case DB_INSTALL_RESULT_ERR_OOM:
                return "out of memory";
            case DB_INSTALL_RESULT_ERR_IS_OPEN:
                return "open";
            case DB_INSTALL_RESULT_ERR_COULD_NOT_OVERWRITE:
                return "could not overwrite";
            case DB_INSTALL_RESULT_ERR_NOT_SUPPORTED:
                return "not supported";
            case DB_INSTALL_RESULT_ERR_NOT_CURRENTLY_POSSIBLE:
                return "not currently possible";
            default:
                return "unknown";
        }
    }
}  // namespace

AndroidInstall::AndroidInstall(SoC* soc, const std::string& dir, const std::string& sessionPath)
    : soc(soc), dir(dir), sessionPath(sessionPath) {}

const char* AndroidInstall::StateName(State state) {
    switch (state) {
        case State::pending:
            return "pending";
        case State::retrying:
            return "retrying";
        case State::deferred:
            return "deferred";
        case State::unsaved:
            return "unsaved";
        case State::resetUnsaved:
            return "reset-unsaved";
        case State::saved:
            return "saved";
        case State::resetNeeded:
            return "reset-needed";
        case State::failed:
            return "failed";
    }
    return "?";
}

void AndroidInstall::Set(Entry& entry, State state, const std::string& reason) {
    entry.state = state;
    entry.reason = reason;
    entry.timeMs = wallMs();

    SDL_Log("cp-android: install: %s %s%s%s", entry.key.c_str(), StateName(state),
            reason.empty() ? "" : ": ", reason.c_str());
    WriteStatus();
}

// Earlier results (shown by the start screen until the session is deleted).
void AndroidInstall::ReadStatus() {
    FILE* file = fopen((dir + "/status.txt").c_str(), "r");
    if (!file) return;

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#') continue;
        line[strcspn(line, "\r\n")] = '\0';

        // key \t state \t time \t db name \t reason
        std::vector<std::string> fields;
        for (char *p = line, *tab; p; p = tab ? tab + 1 : nullptr) {
            tab = strchr(p, '\t');
            fields.emplace_back(p, tab ? tab - p : strlen(p));
        }
        if (fields.size() != 5 || fields[0].empty()) continue;

        Entry entry;
        entry.key = fields[0];
        entry.timeMs = strtoull(fields[2].c_str(), nullptr, 10);
        entry.dbName = fields[3];
        entry.reason = fields[4];

        const std::string& state = fields[1];
        if (state == "saved")
            entry.state = State::saved;
        else if (state == "reset-needed")
            entry.state = State::resetNeeded;
        else if (state == "failed")
            entry.state = State::failed;
        else
            entry.state = State::pending;  // not finished: installed again if the file is there

        entries.push_back(entry);
    }
    fclose(file);
}

void AndroidInstall::WriteStatus() {
    std::string text = std::string(kStatusHeader) + "\n";
    for (const Entry& entry : entries) {
        text += entry.key + "\t" + StateName(entry.state) + "\t" + std::to_string(entry.timeMs) +
                "\t" + entry.dbName + "\t" + entry.reason + "\n";
    }

    if (!WriteFileDurable(dir + "/status.txt", reinterpret_cast<const uint8_t*>(text.data()),
                          text.size()))
        SDL_Log("cp-android: install: cannot write status");
}

void AndroidInstall::Scan() {
    scanned = true;
    ReadStatus();

    std::vector<std::string> files;
    if (DIR* d = opendir((dir + "/pending").c_str())) {
        while (dirent* e = readdir(d)) {
            const std::string name = e->d_name;
            const bool tmp = name.size() > 4 && name.compare(name.size() - 4, 4, ".tmp") == 0;
            if (name.empty() || name[0] == '.' || tmp) continue;  // being copied
            files.push_back(name);
        }
        closedir(d);
    }
    std::sort(files.begin(), files.end());

    // Unfinished entries without a file are dropped (removed by the start screen).
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [&](const Entry& entry) {
                                     return entry.state == State::pending &&
                                            std::find(files.begin(), files.end(), entry.key) ==
                                                files.end();
                                 }),
                  entries.end());

    for (const std::string& file : files) {
        auto it = std::find_if(entries.begin(), entries.end(),
                               [&](const Entry& entry) { return entry.key == file; });
        if (it == entries.end()) {
            Entry entry;
            entry.key = file;
            entries.push_back(entry);
        } else {
            // A file is still there: not finished in an earlier run, or finished but not
            // removed. Install it (again).
            it->state = State::pending;
            it->reason.clear();
        }
    }

    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) { return a.key < b.key; });
    if (entries.size() > kMaxHistory) entries.erase(entries.begin(), entries.end() - kMaxHistory);

    if (!files.empty()) SDL_Log("cp-android: install: %zu file(s) queued", files.size());
    WriteStatus();
}

void AndroidInstall::InstallOne(Entry& entry, uint64_t nowUsec) {
    const std::string path = dir + "/pending/" + entry.key;
    std::vector<uint8_t> data;

    if (!readFile(path, data, kMaxFileSize)) {
        unlink(path.c_str());
        return Set(entry, State::failed, data.size() > 0 ? "too large" : "cannot read");
    }
    if (data.size() < kMinFileSize) {
        unlink(path.c_str());
        return Set(entry, State::failed, "corrupt");
    }
    entry.dbName = asciiName(reinterpret_cast<const char*>(data.data()), 32);

    const int32_t result =
        dbInstallerInstall(soc->GetSyscallDispatch(), data.size(), data.data());

    switch (result) {
        case DB_INSTALL_RESULT_OK:
        case DB_INSTALL_RESULT_NEEDS_RESET: {
            // Check that the database is there (by the name in its header).
            std::string note;
            DbMetadataList list;
            if (dbListGet(soc->GetSyscallDispatch(), list)) {
                const bool found = std::any_of(list.begin(), list.end(), [&](const DbMetadata& m) {
                    return strncmp(m.name, reinterpret_cast<const char*>(data.data()), 32) == 0;
                });
                if (!found) note = "not in database list";
            } else {
                note = "database list not available";
            }

            return Set(entry,
                       result == DB_INSTALL_RESULT_OK ? State::unsaved : State::resetUnsaved, note);
        }

        case DB_INSTALL_RESULT_ERR_NOT_CURRENTLY_POSSIBLE:
            if (++entry.attempts >= kMaxAttempts) {
                entry.attempts = 0;
                return Set(entry, State::deferred, "not currently possible");
            }
            entry.nextTryUsec = nowUsec + kRetryUsec;
            if (entry.state != State::retrying) Set(entry, State::retrying);
            return;

        default:
            unlink(path.c_str());
            return Set(entry, State::failed, resultReason(result));
    }
}

// The installed databases are in RAM: save the session, then remove their files.
void AndroidInstall::SaveAfterInstall() {
    const bool saved = !sessionPath.empty() && SaveSession(soc, sessionPath);

    for (Entry& entry : entries) {
        if (entry.state != State::unsaved && entry.state != State::resetUnsaved) continue;
        const bool needsReset = entry.state == State::resetUnsaved;

        if (saved) {
            unlink((dir + "/pending/" + entry.key).c_str());
            if (needsReset) resetPending = true;
            Set(entry, needsReset ? State::resetNeeded : State::saved, entry.reason);
        } else {
            // File kept: installed again on the next start.
            Set(entry, State::deferred, "session not saved");
        }
    }
}

void AndroidInstall::AskReset() {
    resetPending = false;

    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "나중에"},
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "지금 리셋"},
    };
    const SDL_MessageBoxData box = {
        SDL_MESSAGEBOX_INFORMATION,
        nullptr,
        "PRC/PDB 설치",
        "설치한 프로그램을 쓰려면 리셋(소프트 리셋)이 필요합니다. 지금 리셋할까요?",
        SDL_arraysize(buttons),
        buttons,
        nullptr,
    };

    int button = 0;
    if (SDL_ShowMessageBox(&box, &button) != 0) {
        SDL_Log("cp-android: install: reset question failed: %s", SDL_GetError());
        return;
    }
    if (button != 1) {
        SDL_Log("cp-android: install: reset later");
        return;
    }

    SDL_Log("cp-android: install: soft reset");
    soc->Reset();
    saveAfterResetUsec = 1;  // scheduled from the next step's time

    for (Entry& entry : entries)
        if (entry.state == State::resetNeeded) Set(entry, State::saved, "reset");
}

void AndroidInstall::Step(uint64_t nowUsec) {
    if (saveAfterResetUsec == 1) saveAfterResetUsec = nowUsec + kSaveAfterResetUsec;
    if (saveAfterResetUsec && nowUsec >= saveAfterResetUsec) {
        saveAfterResetUsec = 0;
        if (!sessionPath.empty()) SaveSession(soc, sessionPath);
    }

    if (!systemStateIsUiInitialized(soc->GetSystemState())) return;
    if (!scanned) Scan();

    bool waiting = false;
    for (Entry& entry : entries) {
        if (entry.state != State::pending && entry.state != State::retrying) continue;
        if (entry.state == State::retrying && nowUsec < entry.nextTryUsec) {
            waiting = true;
            continue;
        }

        return InstallOne(entry, nowUsec);  // one attempt per step
    }
    if (waiting) return;

    if (std::any_of(entries.begin(), entries.end(), [](const Entry& entry) {
            return entry.state == State::unsaved || entry.state == State::resetUnsaved;
        }))
        return SaveAfterInstall();

    if (resetPending) AskReset();
}
