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
#include <vector>

namespace netlib_redirect {
    // Synchronous RPC: send an encoded MsgRequest, receive an encoded MsgResponse.
    using RpcT = std::function<bool(const uint8_t* request, size_t requestSize,
                                    std::vector<uint8_t>& response)>;

    // Session handling of the host backend (one session per open NetLib).
    using SessionT = std::function<void(bool open)>;

    // Enables redirection. Pass empty functions to disable.
    void Configure(RpcT rpc, SessionT session);

    bool IsEnabled();

    // Called for every 68k trap that PACE hands to the host. Returns true if the trap was handled
    // (result in D0/A0, outputs written to 68k memory).
    bool HandleTrap(uint16_t trapWord);

    // Forget library state (after a reset or a session load).
    void Reset();
}  // namespace netlib_redirect

#endif  // _NETLIB_REDIRECT_H_
