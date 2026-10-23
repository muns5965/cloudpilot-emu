#include "NetlibNative.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>

#include "netlib_redirect.h"
#include "networkBackend.h"

// Native backend for NetLib redirection: requests run on the host network stack through
// CloudpilotEmu's network backend (src/cloudpilot/native/network-backend). The backend answers
// on its own thread; the 68k caller re-executes its trap until the response is delivered.

namespace {
    std::mutex mutex;
    std::optional<uint32_t> currentSession;
    unsigned int testDelayMsec = 0;

    void rpcResultCb(uint32_t sessionId, const uint8_t* data, size_t len, void*) {
        {
            std::lock_guard lock(mutex);
            if (!currentSession || *currentSession != sessionId) return;
        }

        // Test hook: answer late (NETLIB_TEST_DELAY_MS) to exercise the waiting path.
        if (testDelayMsec > 0) std::this_thread::sleep_for(std::chrono::milliseconds(testDelayMsec));

        netlib_redirect::Deliver(data, len);
    }

    void session(bool open) {
        std::lock_guard lock(mutex);

        if (currentSession) net_closeSession(*currentSession);
        currentSession = open ? std::optional<uint32_t>(net_openSession()) : std::nullopt;
    }

    bool send(const uint8_t* request, size_t size) {
        std::lock_guard lock(mutex);

        return currentSession && net_dispatchRpc(*currentSession, request, size);
    }
}  // namespace

void netlibNativeEnable() {
    if (const char* delay = getenv("NETLIB_TEST_DELAY_MS")) testDelayMsec = atoi(delay);

    net_setRpcCallback(rpcResultCb, nullptr);
    netlib_redirect::Configure(send, session);

    std::cerr << "NetLib redirection enabled (68k NetLib calls use the host network)" << std::endl;
}
