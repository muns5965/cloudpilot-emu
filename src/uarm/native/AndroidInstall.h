#ifndef _ANDROID_INSTALL_H_
#define _ANDROID_INSTALL_H_

// PRC / PDB installation queue (Android build only, stage 2-F).
//
// The start screen (MainActivity) copies picked files to <dir>/pending/<key>, where the key starts
// with a timestamp so that the queue sorts in the order the files were added. The emulator thread
// installs them into the running Palm OS (dbInstallerInstall, as the web and native versions) once
// its UI is initialised, and records the result per key in <dir>/status.txt, which the start
// screen shows:
//
//   OK                       installed, waiting for the session save -> saved (file removed)
//   NEEDS_RESET              as OK; after the save the user is asked whether to reset now
//   NOT_CURRENTLY_POSSIBLE   retried every 2 s, 15 times; then deferred to the next start (file kept)
//   any other result         failed with the reason (file removed, not retried)
//
// A file is removed only after the session holding the installed database has been saved: if the
// emulator ends before that, the file is installed again on the next start (overwriting the same
// database). Installed databases are not reinstalled when the session is deleted later.

#include <cstdint>
#include <string>
#include <vector>

class SoC;

class AndroidInstall {
   public:
    // dir: the queue directory (files/install). sessionPath empty: nothing is saved, so no file is
    // ever removed after an install.
    AndroidInstall(SoC* soc, const std::string& dir, const std::string& sessionPath);

    // Emulator thread, in the foreground, between main loop cycles. Does at most one install
    // attempt (or one save) per call; returns quickly when there is nothing to do.
    void Step(uint64_t nowUsec);

   private:
    enum class State { pending, retrying, deferred, unsaved, resetUnsaved, saved, resetNeeded, failed };

    struct Entry {
        std::string key;     // file name in pending/
        State state{State::pending};
        std::string reason;  // failure or check note (ASCII)
        std::string dbName;  // from the database header, if readable
        int attempts{0};
        uint64_t nextTryUsec{0};
        uint64_t timeMs{0};  // last change, wall clock
    };

    void Scan();
    void InstallOne(Entry& entry, uint64_t nowUsec);
    void SaveAfterInstall();
    void AskReset();
    void WriteStatus();
    void ReadStatus();
    void Set(Entry& entry, State state, const std::string& reason = "");

    static const char* StateName(State state);

   private:
    SoC* soc;
    std::string dir;
    std::string sessionPath;

    std::vector<Entry> entries;  // this run's queue and earlier results, in key order
    bool scanned{false};
    bool resetPending{false};
    uint64_t saveAfterResetUsec{0};  // 0: none scheduled
};

#endif  // _ANDROID_INSTALL_H_
