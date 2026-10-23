#ifndef _NETLIB_REDIRECT_H_
#define _NETLIB_REDIRECT_H_

// NetLib redirection for Palm OS 5 (uARM): 68k NetLib calls are handled on the host instead of
// the ROM's network stack, like CloudpilotEmu's OS 4 network integration. Requests use the same
// protobuf messages (proto/networking.proto); the host side executes them.
//
// Only calls that pass through PACE (68k applications) are seen. ARM-native callers of NetLib
// are not redirected.

#include <cstddef>
#include <cstdint>
#include <functional>

namespace netlib_redirect {
    // Sends an encoded MsgRequest. The encoded MsgResponse comes back later through Deliver().
    using SendT = std::function<bool(const uint8_t* request, size_t size)>;

    // Session handling of the host backend (one session while NetLib is open).
    using SessionT = std::function<void(bool open)>;

    // Enables redirection. Pass empty functions to disable.
    void Configure(SendT send, SessionT session);

    bool IsEnabled();

    // Hands over the response to the request in flight. May be called from another thread.
    void Deliver(const uint8_t* response, size_t size);

    // A call is waiting for its response (the 68k caller re-executes its trap until it arrives).
    bool IsWaiting();

    // Called for every 68k trap that PACE hands to the host. Returns true if the trap was handled
    // (result in D0/A0, outputs written to 68k memory, or the trap set up to run again).
    bool HandleTrap(uint16_t trapWord);

    // Called while a call waits for its response (the SoC idles until the next interrupt).
    void SetIdleHandler(std::function<void()> idle);

    // Forget library state (after a reset or a session load).
    void Reset();
}  // namespace netlib_redirect

#endif  // _NETLIB_REDIRECT_H_
