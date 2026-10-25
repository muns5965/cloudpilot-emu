#ifndef _CLI_UARM_COMMANDS_H_
#define _CLI_UARM_COMMANDS_H_

#include <functional>
#include <string>
#include <vector>

#include "MainLoop.h"
#include "Rotation.h"
#include "SdlAudioDriver.h"
#include "SoC.h"

namespace commands {
    struct Context {
        SoC* soc;
        MainLoop& mainLoop;
        SdlAudioDriver& audioDriver;
        Rotation rotation;
        // Device screen size (pen coordinates of the "tap" command)
        int screenWidth;
        int screenHeight;
        // "screenshot": saves the device screen (see SdlRenderer::SaveScreenshot)
        std::function<bool(const std::string& path, bool& lcdOn, std::string& error)> screenshot;
    };

    void Register();

    // "tap" / "key": releases the inputs whose hold time has passed. Call once per main loop
    // iteration.
    void ProcessScheduledInput(Context& context, uint64_t nowUsec);
    // Releases all inputs held by "tap" / "key" and cancels their scheduled releases.
    void ReleaseAllInput(Context& context, const char* reason);
}  // namespace commands

#endif  // _CLI_UARM_COMMANDS_H_
