#include "NetlibNative.h"

#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <optional>

#include "netlib_redirect.h"
#include "networkBackend.h"

// Native backend for NetLib redirection: requests run on the host network stack through
// CloudpilotEmu's network backend (src/cloudpilot/native/network-backend). Calls block the
// emulator until the backend answers (the backend applies the call's timeout).

namespace {
    std::mutex mutex;
    std::condition_variable cv;
    std::optional<uint32_t> currentSession;
    std::vector<uint8_t> pendingResponse;
    bool hasResponse{false};

    void rpcResultCb(uint32_t sessionId, const uint8_t* data, size_t len, void*) {
        std::unique_lock lock(mutex);

        if (!currentSession || *currentSession != sessionId) return;

        pendingResponse.assign(data, data + len);
        hasResponse = true;
        cv.notify_one();
    }

    void session(bool open) {
        std::unique_lock lock(mutex);

        if (currentSession) net_closeSession(*currentSession);
        currentSession = open ? std::optional<uint32_t>(net_openSession()) : std::nullopt;
    }

    bool rpc(const uint8_t* request, size_t requestSize, std::vector<uint8_t>& response) {
        std::unique_lock lock(mutex);

        if (!currentSession) return false;

        hasResponse = false;
        if (!net_dispatchRpc(*currentSession, request, requestSize)) return false;

        cv.wait(lock, [] { return hasResponse; });

        response.swap(pendingResponse);
        hasResponse = false;

        return true;
    }
}  // namespace

void netlibNativeEnable() {
    net_setRpcCallback(rpcResultCb, nullptr);
    netlib_redirect::Configure(rpc, session);

    std::cerr << "NetLib redirection enabled (68k NetLib calls use the host network)" << std::endl;
}
