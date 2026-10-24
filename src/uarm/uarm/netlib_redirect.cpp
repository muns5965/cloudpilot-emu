#include "netlib_redirect.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "networking.pb.h"
#include "pace.h"
#include "pb_decode.h"
#include "pb_encode.h"

// 68k NetLib calls arrive here as library traps (trap #15, 0xA800 + n) with the library
// reference number as the first stack argument. Arguments are read from the 68k stack: UInt16
// and enum (byte) arguments take 2 bytes each (a byte sits in the first of them), pointers and
// 32-bit values 4 bytes. The result goes to D0 (A0 for pointers).

namespace {
    constexpr uint16_t TRAP_SYS_LIB_FIND = 0xA0BA;

    constexpr uint16_t SYS_LIB_TRAP_OPEN = 0xA801;
    constexpr uint16_t SYS_LIB_TRAP_CLOSE = 0xA802;
    constexpr uint16_t SYS_LIB_TRAP_SLEEP = 0xA803;
    constexpr uint16_t SYS_LIB_TRAP_WAKE = 0xA804;
    constexpr uint16_t SYS_LIB_TRAP_CUSTOM = 0xA805;

    // NetMgr.h: netLibTrap* = sysLibTrapCustom + n
    constexpr uint16_t NET_TRAP_SOCKET_OPEN = SYS_LIB_TRAP_CUSTOM + 2;
    constexpr uint16_t NET_TRAP_SOCKET_CLOSE = SYS_LIB_TRAP_CUSTOM + 3;
    constexpr uint16_t NET_TRAP_SOCKET_OPTION_SET = SYS_LIB_TRAP_CUSTOM + 4;
    constexpr uint16_t NET_TRAP_SOCKET_BIND = SYS_LIB_TRAP_CUSTOM + 6;
    constexpr uint16_t NET_TRAP_SOCKET_CONNECT = SYS_LIB_TRAP_CUSTOM + 7;
    constexpr uint16_t NET_TRAP_SEND = SYS_LIB_TRAP_CUSTOM + 12;
    constexpr uint16_t NET_TRAP_RECEIVE = SYS_LIB_TRAP_CUSTOM + 14;
    constexpr uint16_t NET_TRAP_GET_HOST_BY_NAME = SYS_LIB_TRAP_CUSTOM + 22;
    constexpr uint16_t NET_TRAP_SETTING_GET = SYS_LIB_TRAP_CUSTOM + 23;
    constexpr uint16_t NET_TRAP_IF_SETTING_GET = SYS_LIB_TRAP_CUSTOM + 28;
    constexpr uint16_t NET_TRAP_IF_UP = SYS_LIB_TRAP_CUSTOM + 30;
    constexpr uint16_t NET_TRAP_FINISH_CLOSE_WAIT = SYS_LIB_TRAP_CUSTOM + 37;
    constexpr uint16_t NET_TRAP_OPEN_COUNT = SYS_LIB_TRAP_CUSTOM + 41;
    constexpr uint16_t NET_TRAP_OPEN_IF_CLOSE_WAIT = SYS_LIB_TRAP_CUSTOM + 44;
    constexpr uint16_t NET_TRAP_HANDLE_POWER_OFF = SYS_LIB_TRAP_CUSTOM + 45;
    constexpr uint16_t NET_TRAP_CONNECTION_REFRESH = SYS_LIB_TRAP_CUSTOM + 46;
    constexpr uint16_t NET_TRAP_LAST = SYS_LIB_TRAP_CUSTOM + 64;

    constexpr uint16_t netErrorClass = 0x1200;
    constexpr uint16_t netErrNotOpen = netErrorClass | 2;
    constexpr uint16_t netErrParamErr = netErrorClass | 4;
    constexpr uint16_t netErrBufTooSmall = netErrorClass | 13;
    constexpr uint16_t netErrUnimplemented = netErrorClass | 14;
    constexpr uint16_t netErrInternal = netErrorClass | 17;
    constexpr uint16_t netErrSocketClosedByRemote = netErrorClass | 20;

    constexpr uint8_t netSocketAddrINET = 2;

    constexpr uint16_t netIFSettingUp = 1;

    // NetSettingEnum
    constexpr uint16_t netSettingPrimaryDNS = 1;
    constexpr uint16_t netSettingSecondaryDNS = 2;
    constexpr uint16_t netSettingHostName = 6;
    constexpr uint16_t netSettingRTPrimaryDNS = 0x1004;
    constexpr uint16_t netSettingRTSecondaryDNS = 0x1005;

    // Socket options (NetSocketOptLevelEnum, NetSocketOptEnum)
    constexpr uint16_t netSocketOptLevelIP = 0;
    constexpr uint16_t netSocketOptLevelTCP = 6;
    constexpr uint16_t netSocketOptLevelSocket = 0xffff;
    constexpr uint16_t netSocketOptTCPNoDelay = 1;
    constexpr uint16_t netSocketOptSockLinger = 0x0080;
    constexpr uint16_t netSocketOptSockSndBufSize = 0x1001;
    constexpr uint16_t netSocketOptSockSocketType = 0x1008;
    constexpr uint16_t netSocketOptSockRequireErrClear = 0x2001;
    constexpr uint16_t netSocketOptSockMultiPktAddr = 0x2002;
    constexpr size_t SOCKET_OPTION_BUF_MAX = 40;  // networking.options: bufval max_size

    // NetHostInfoBufType (68k layout)
    constexpr uint32_t HOSTBUF_NAME = 16;
    constexpr uint32_t HOSTBUF_ALIAS_LIST = 272;
    constexpr uint32_t HOSTBUF_ALIASES = 280;
    constexpr uint32_t HOSTBUF_ADDRESS_LIST = 536;
    constexpr uint32_t HOSTBUF_ADDRESS = 552;
    constexpr uint32_t HOSTBUF_MAX_ADDRESSES = 3;  // the proto carries up to 3

    netlib_redirect::SendT send;
    std::function<void()> idleHandler;
    netlib_redirect::SessionT session;

    int32_t netRefNum = -1;
    uint32_t pendingRefNumP = 0;
    uint32_t openCount = 0;
    uint32_t requestId = 0;

    struct Args {
        uint32_t sp;

        uint8_t u8(uint32_t offset) const { return paceGet8(sp + offset); }
        uint16_t u16(uint32_t offset) const { return paceGet16(sp + offset); }
        int16_t i16(uint32_t offset) const { return static_cast<int16_t>(u16(offset)); }
        uint32_t u32(uint32_t offset) const { return paceGet32(sp + offset); }
        int32_t i32(uint32_t offset) const { return static_cast<int32_t>(u32(offset)); }
    };

    void setResult(int32_t value) { paceSetDreg(0, static_cast<uint32_t>(value)); }

    void setResultPtr(uint32_t ptr) {
        paceSetAreg(0, ptr);
        paceSetDreg(0, ptr);
    }

    void putErr(uint32_t errP, uint16_t err) {
        if (errP) paceSet16(errP, err);
    }

    void copyFrom68k(uint8_t* dest, uint32_t src, size_t len) {
        for (size_t i = 0; i < len; i++) dest[i] = paceGet8(src + i);
    }

    void copyTo68k(uint32_t dest, const uint8_t* src, size_t len) {
        for (size_t i = 0; i < len; i++) paceSet8(dest + i, src[i]);
    }

    std::string readString(uint32_t ptr, size_t maxLen) {
        std::string result;

        for (size_t i = 0; ptr && i < maxLen; i++) {
            const char c = static_cast<char>(paceGet8(ptr + i));
            if (!c) break;

            result += c;
        }

        return result;
    }

    // Milliseconds for the backend; Palm OS 5 uses 100 ticks per second.
    int32_t convertTimeout(int32_t timeout) { return timeout > 0 ? timeout * 10 : timeout; }

    // NetSocketAddrINType: family (2), port (2, network order), addr (4, network order)
    bool readAddress(uint32_t sockAddrP, Address& address) {
        if (!sockAddrP || paceGet16(sockAddrP) != netSocketAddrINET) return false;

        address.port = paceGet16(sockAddrP + 2);
        address.ip = paceGet32(sockAddrP + 4);

        return true;
    }

    void writeAddress(uint32_t sockAddrP, const Address& address) {
        paceSet16(sockAddrP, netSocketAddrINET);
        paceSet16(sockAddrP + 2, static_cast<uint16_t>(address.port));
        paceSet32(sockAddrP + 4, address.ip);
    }

    struct Buffer {
        const uint8_t* data{nullptr};
        size_t len{0};
    };

    struct ReceivedBuffer {
        std::unique_ptr<uint8_t[]> data;
        size_t len{0};
    };

    bool bufferEncodeCb(pb_ostream_t* stream, const pb_field_iter_t* field, void* const* arg) {
        const Buffer* buffer = static_cast<const Buffer*>(*arg);

        return pb_encode_tag_for_field(stream, field) &&
               pb_encode_string(stream, buffer->data, buffer->len);
    }

    bool bufferDecodeCb(pb_istream_t* stream, const pb_field_iter_t*, void** arg) {
        ReceivedBuffer* buffer = static_cast<ReceivedBuffer*>(*arg);

        buffer->len = stream->bytes_left;
        buffer->data = std::make_unique<uint8_t[]>(buffer->len);

        return pb_read(stream, buffer->data.get(), buffer->len);
    }

    bool setupPayloadDecodeCb(pb_istream_t*, const pb_field_iter_t* field, void** arg) {
        if (field->tag == MsgResponse_socketReceiveResponse_tag) {
            auto* response = static_cast<MsgSocketReceiveResponse*>(field->pData);

            response->data.arg = *arg;
            response->data.funcs.decode = bufferDecodeCb;
        }

        return true;
    }

    MsgRequest newRequest(pb_size_t payloadTag) {
        MsgRequest request = MsgRequest_init_zero;

        request.id = ++requestId;
        request.which_payload = payloadTag;

        return request;
    }

    enum class CallResult { done, failed, pending };

    // A request whose response has not arrived yet. The trap is re-executed (PC rewound) until
    // the response is there; the call is identified by trap word and stack pointer.
    struct Pending {
        bool active{false};
        uint16_t trapWord{0};
        uint32_t sp{0};
        uint32_t id{0};
    } pending;

    uint16_t currentTrap{0};
    uint32_t currentSp{0};
    bool rewindRequested{false};

    std::mutex responseMutex;
    bool responseReady{false};
    std::vector<uint8_t> responseData;

    CallResult call(MsgRequest& request, size_t extraSize, MsgResponse& response,
                    ReceivedBuffer* received = nullptr) {
        if (!send || openCount == 0) return CallResult::failed;

        if (pending.active && (pending.trapWord != currentTrap || pending.sp != currentSp)) {
            // Another 68k thread calls NetLib while a call is in flight: let it wait its turn.
            rewindRequested = true;
            return CallResult::pending;
        }

        if (!pending.active) {
            std::vector<uint8_t> encoded(512 + extraSize);
            pb_ostream_t ostream = pb_ostream_from_buffer(encoded.data(), encoded.size());
            if (!pb_encode(&ostream, MsgRequest_fields, &request)) {
                fprintf(stderr, "netlib: encode failed: %s\n", PB_GET_ERROR(&ostream));
                return CallResult::failed;
            }

            {
                std::lock_guard lock(responseMutex);
                responseReady = false;
            }

            if (!send(encoded.data(), ostream.bytes_written)) return CallResult::failed;

            pending = {true, currentTrap, currentSp, request.id};
            rewindRequested = true;

            return CallResult::pending;
        }

        std::vector<uint8_t> reply;
        {
            std::lock_guard lock(responseMutex);
            if (!responseReady) {
                rewindRequested = true;
                return CallResult::pending;
            }

            reply.swap(responseData);
            responseReady = false;
        }

        const uint32_t id = pending.id;
        pending.active = false;

        response = MsgResponse_init_zero;
        if (received) {
            response.cb_payload.arg = received;
            response.cb_payload.funcs.decode = setupPayloadDecodeCb;
        }

        pb_istream_t istream = pb_istream_from_buffer(reply.data(), reply.size());
        if (!pb_decode(&istream, MsgResponse_fields, &response)) {
            fprintf(stderr, "netlib: decode failed: %s\n", PB_GET_ERROR(&istream));
            return CallResult::failed;
        }

        return response.id == id && response.which_payload == request.which_payload
                   ? CallResult::done
                   : CallResult::failed;
    }

    // NetLibOpen(UInt16 libRefNum, UInt16 *netIFErrsP)
    void netLibOpen(const Args& args) {
        if (openCount++ == 0 && session) session(true);

        if (args.u32(2)) paceSet16(args.u32(2), 0);
        setResult(0);
    }

    // NetLibClose(UInt16 libRefNum, UInt16 immediate)
    void netLibClose(const Args&) {
        if (openCount == 0) return setResult(netErrNotOpen);

        if (--openCount == 0 && session) session(false);
        setResult(0);
    }

    // NetLibOpenCount(UInt16 refNum, UInt16 *countP)
    void netLibOpenCount(const Args& args) {
        if (args.u32(2)) paceSet16(args.u32(2), static_cast<uint16_t>(openCount));
        setResult(0);
    }

    // NetLibOpenIfCloseWait(UInt16 libRefnum): reopens a library that is waiting to close. The
    // host session closes immediately, so it is open only while openCount > 0.
    void netLibOpenIfCloseWait(const Args&) {
        if (openCount == 0) return setResult(netErrNotOpen);

        openCount++;
        setResult(0);
    }

    // NetLibConnectionRefresh(UInt16 refNum, Boolean refresh, UInt8 *allInterfacesUpP,
    //     UInt16 *netIFErrP): the host connection is always up while the library is open.
    void netLibConnectionRefresh(const Args& args) {
        if (openCount == 0) return setResult(netErrNotOpen);

        if (args.u32(4)) paceSet8(args.u32(4), 1);
        if (args.u32(8)) paceSet16(args.u32(8), 0);
        setResult(0);
    }

    // NetLibIFSettingGet(UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance, UInt16 setting,
    //     void *valueP, UInt16 *valueLenP): only "interface up" is answered (like the OS 4
    //     integration); other settings go to the ROM.
    bool netLibIFSettingGet(const Args& args) {
        const uint32_t valueP = args.u32(10);
        const uint32_t valueLenP = args.u32(14);

        if (args.u16(8) != netIFSettingUp || !valueP || !valueLenP || paceGet16(valueLenP) == 0)
            return false;

        paceSet8(valueP, 1);
        paceSet16(valueLenP, 1);
        setResult(0);

        return true;
    }

    // Int16 NetLibSocketBind(UInt16 libRefNum, NetSocketRef socket, NetSocketAddrType *sockAddrP,
    //     Int16 addrLen, Int32 timeout, Err *errP)
    void netLibSocketBind(const Args& args) {
        const uint32_t errP = args.u32(14);

        MsgRequest request = newRequest(MsgRequest_socketBindRequest_tag);
        request.payload.socketBindRequest.handle = args.i16(2);
        request.payload.socketBindRequest.timeout = convertTimeout(args.i32(10));

        if (args.i16(8) < 8 || !readAddress(args.u32(4), request.payload.socketBindRequest.address)) {
            putErr(errP, netErrParamErr);
            return setResult(-1);
        }

        MsgResponse response;
        const CallResult result = call(request, 0, response);
        if (result == CallResult::pending) return;
        if (result == CallResult::failed) {
            putErr(errP, netErrInternal);
            return setResult(-1);
        }

        putErr(errP, response.payload.socketBindResponse.err);
        setResult(response.payload.socketBindResponse.err ? -1 : 0);
    }

    // Int16 NetLibSocketOptionSet(UInt16 libRefNum, NetSocketRef socket, UInt16 level,
    //     UInt16 option, void *optValueP, UInt16 optValueLen, Int32 timeout, Err *errP)
    // Value encoding as in the OS 4 integration (NetworkProxy::SocketOptionSet).
    void netLibSocketOptionSet(const Args& args) {
        const uint16_t level = args.u16(4);
        const uint16_t option = args.u16(6);
        const uint32_t valueP = args.u32(8);
        const uint16_t len = args.u16(12);
        const uint32_t errP = args.u32(18);

        auto fail = [&](uint16_t err) {
            putErr(errP, err);
            setResult(-1);
        };

        if (level == netSocketOptLevelSocket && option == netSocketOptSockMultiPktAddr)
            return fail(netErrUnimplemented);

        if (level == netSocketOptLevelSocket && option == netSocketOptSockRequireErrClear) {
            putErr(errP, 0);
            return setResult(0);
        }

        MsgRequest request = newRequest(MsgRequest_socketOptionSetRequest_tag);
        auto& r = request.payload.socketOptionSetRequest;
        r.handle = args.i16(2);
        r.level = level;
        r.option = option;
        r.timeout = convertTimeout(args.i32(14));

        if (!valueP) return fail(netErrParamErr);

        const bool isLinger = level == netSocketOptLevelSocket && option == netSocketOptSockLinger;
        const bool isBuffer = level == netSocketOptLevelIP;
        const bool isInt = (level == netSocketOptLevelTCP && option != netSocketOptTCPNoDelay) ||
                           (level == netSocketOptLevelSocket && option >= netSocketOptSockSndBufSize &&
                            option <= netSocketOptSockSocketType);

        if (isLinger) {
            if (len != 4) return fail(netErrParamErr);

            r.which_value = MsgSocketOptionSetRequest_intval_tag;
            r.value.intval = paceGet16(valueP) | (paceGet16(valueP + 2) << 16);
        } else if (isBuffer) {
            if (len > SOCKET_OPTION_BUF_MAX) return fail(netErrParamErr);

            r.which_value = MsgSocketOptionSetRequest_bufval_tag;
            copyFrom68k(r.value.bufval.bytes, valueP, len);
            r.value.bufval.size = len;
        } else {
            int32_t value;
            switch (len) {
                case 1:
                    value = paceGet8(valueP);
                    break;

                case 2:
                    value = paceGet16(valueP);
                    break;

                case 4:
                    value = static_cast<int32_t>(paceGet32(valueP));
                    break;

                default:
                    return fail(netErrParamErr);
            }

            if (!isInt) value = value != 0 ? 1 : 0;
            if (isInt && len == 1) return fail(netErrParamErr);

            r.which_value = MsgSocketOptionSetRequest_intval_tag;
            r.value.intval = value;
        }

        MsgResponse response;
        const CallResult result = call(request, 0, response);
        if (result == CallResult::pending) return;
        if (result == CallResult::failed) return fail(netErrInternal);

        const int32_t err = response.payload.socketOptionSetResponse.err;
        putErr(errP, err);
        setResult(err ? -1 : 0);
    }

    // Err NetLibSettingGet(UInt16 libRefNum, UInt16 setting, void *valueP, UInt16 *valueLenP)
    // Only the host name and the DNS servers are answered by the host (as in the OS 4
    // integration); other settings go to the ROM.
    bool netLibSettingGet(const Args& args) {
        const uint16_t setting = args.u16(2);

        switch (setting) {
            case netSettingHostName:
            case netSettingPrimaryDNS:
            case netSettingSecondaryDNS:
            case netSettingRTPrimaryDNS:
            case netSettingRTSecondaryDNS:
                break;

            default:
                return false;
        }

        const uint32_t valueP = args.u32(4);
        const uint32_t valueLenP = args.u32(8);
        if (!valueP || !valueLenP) {
            setResult(netErrParamErr);
            return true;
        }

        MsgRequest request = newRequest(MsgRequest_settingGetRequest_tag);
        request.payload.settingGetRequest.setting = setting;

        MsgResponse response;
        const CallResult result = call(request, 0, response);
        if (result == CallResult::pending) return true;
        if (result == CallResult::failed) {
            setResult(openCount ? netErrInternal : netErrNotOpen);
            return true;
        }

        const auto& r = response.payload.settingGetResponse;
        if (r.err) {
            setResult(r.err);
            return true;
        }

        const uint16_t valueLen = paceGet16(valueLenP);

        switch (r.which_value) {
            case MsgSettingGetResponse_strval_tag: {
                const size_t len = strnlen(r.value.strval, sizeof(r.value.strval));
                if (len == sizeof(r.value.strval)) {
                    setResult(netErrInternal);
                    return true;
                }
                if (len >= valueLen) {
                    setResult(netErrBufTooSmall);
                    return true;
                }

                copyTo68k(valueP, reinterpret_cast<const uint8_t*>(r.value.strval), len + 1);
                paceSet16(valueLenP, static_cast<uint16_t>(len));
                break;
            }

            case MsgSettingGetResponse_uint32val_tag:
                if (valueLen < 4) {
                    setResult(netErrBufTooSmall);
                    return true;
                }

                paceSet32(valueP, r.value.uint32val);
                paceSet16(valueLenP, 4);
                break;

            case MsgSettingGetResponse_uint8val_tag:
                if (valueLen == 0) {
                    setResult(netErrBufTooSmall);
                    return true;
                }

                paceSet8(valueP, static_cast<uint8_t>(r.value.uint8val));
                paceSet16(valueLenP, 1);
                break;

            default:
                setResult(netErrInternal);
                return true;
        }

        setResult(0);
        return true;
    }

    // NetSocketRef NetLibSocketOpen(UInt16 libRefnum, NetSocketAddrEnum domain,
    //     NetSocketTypeEnum type, Int16 protocol, Int32 timeout, Err *errP)
    void netLibSocketOpen(const Args& args) {
        const uint32_t errP = args.u32(12);

        if (args.u8(2) != netSocketAddrINET) {
            putErr(errP, netErrParamErr);
            return setResult(-1);
        }

        MsgRequest request = newRequest(MsgRequest_socketOpenRequest_tag);
        request.payload.socketOpenRequest.type = args.u8(4);
        request.payload.socketOpenRequest.protocol = args.u16(6);

        MsgResponse response;
        const CallResult result = call(request, 0, response);
        if (result == CallResult::pending) return;
        if (result == CallResult::failed) {
            putErr(errP, openCount ? netErrInternal : netErrNotOpen);
            return setResult(-1);
        }

        const auto& r = response.payload.socketOpenResponse;
        putErr(errP, r.err);
        setResult(r.err ? -1 : r.handle);
    }

    // Int16 NetLibSocketClose(UInt16 libRefnum, NetSocketRef socket, Int32 timeout, Err *errP)
    void netLibSocketClose(const Args& args) {
        const uint32_t errP = args.u32(8);

        MsgRequest request = newRequest(MsgRequest_socketCloseRequest_tag);
        request.payload.socketCloseRequest.handle = args.i16(2);
        request.payload.socketCloseRequest.timeout = convertTimeout(args.i32(4));

        MsgResponse response;
        const CallResult result = call(request, 0, response);
        if (result == CallResult::pending) return;
        if (result == CallResult::failed) {
            putErr(errP, netErrInternal);
            return setResult(-1);
        }

        putErr(errP, response.payload.socketCloseResponse.err);
        setResult(response.payload.socketCloseResponse.err ? -1 : 0);
    }

    // Int16 NetLibSocketConnect(UInt16 libRefNum, NetSocketRef socket,
    //     NetSocketAddrType *sockAddrP, Int16 addrLen, Int32 timeout, Err *errP)
    void netLibSocketConnect(const Args& args) {
        const uint32_t errP = args.u32(14);

        MsgRequest request = newRequest(MsgRequest_socketConnectRequest_tag);
        request.payload.socketConnectRequest.handle = args.i16(2);
        request.payload.socketConnectRequest.timeout = convertTimeout(args.i32(10));

        if (args.i16(8) < 8 ||
            !readAddress(args.u32(4), request.payload.socketConnectRequest.address)) {
            putErr(errP, netErrParamErr);
            return setResult(-1);
        }

        MsgResponse response;
        const CallResult result = call(request, 0, response);
        if (result == CallResult::pending) return;
        if (result == CallResult::failed) {
            putErr(errP, netErrInternal);
            return setResult(-1);
        }

        putErr(errP, response.payload.socketConnectResponse.err);
        setResult(response.payload.socketConnectResponse.err ? -1 : 0);
    }

    // Int16 NetLibSend(UInt16 libRefNum, NetSocketRef socket, void *bufP, UInt16 bufLen,
    //     UInt16 flags, void *toAddrP, UInt16 toLen, Int32 timeout, Err *errP)
    void netLibSend(const Args& args) {
        const uint32_t errP = args.u32(22);
        const uint16_t len = args.u16(8);
        const uint32_t toAddrP = args.u32(12);

        MsgRequest request = newRequest(MsgRequest_socketSendRequest_tag);
        auto& send = request.payload.socketSendRequest;
        send.handle = args.i16(2);
        send.flags = args.u16(10);
        send.timeout = convertTimeout(args.i32(18));

        if (toAddrP) {
            if (args.u16(16) < 8 || !readAddress(toAddrP, send.address)) {
                putErr(errP, netErrParamErr);
                return setResult(-1);
            }

            send.has_address = true;
        }

        std::vector<uint8_t> data(len);
        copyFrom68k(data.data(), args.u32(4), len);

        Buffer buffer{data.data(), data.size()};
        send.data.arg = &buffer;
        send.data.funcs.encode = bufferEncodeCb;

        MsgResponse response;
        const CallResult result = call(request, len, response);
        if (result == CallResult::pending) return;
        if (result == CallResult::failed) {
            putErr(errP, netErrInternal);
            return setResult(-1);
        }

        const auto& r = response.payload.socketSendResponse;
        putErr(errP, r.err);
        setResult(r.err ? (r.err == netErrSocketClosedByRemote ? 0 : -1) : r.bytesSent);
    }

    // Int16 NetLibReceive(UInt16 libRefNum, NetSocketRef socket, void *bufP, UInt16 bufLen,
    //     UInt16 flags, void *fromAddrP, UInt16 *fromLenP, Int32 timeout, Err *errP)
    void netLibReceive(const Args& args) {
        const uint32_t errP = args.u32(24);
        const uint32_t bufP = args.u32(4);
        const uint16_t bufLen = args.u16(8);
        const uint32_t fromAddrP = args.u32(12);
        const uint32_t fromLenP = args.u32(16);

        MsgRequest request = newRequest(MsgRequest_socketReceiveRequest_tag);
        auto& receive = request.payload.socketReceiveRequest;
        receive.handle = args.i16(2);
        receive.flags = args.u16(10);
        receive.timeout = convertTimeout(args.i32(20));
        receive.maxLen = bufLen;
        receive.addressRequested = fromAddrP != 0;

        ReceivedBuffer received;
        MsgResponse response;
        const CallResult result = call(request, 0, response, &received);
        if (result == CallResult::pending) return;
        if (result == CallResult::failed) {
            putErr(errP, netErrInternal);
            return setResult(-1);
        }

        const auto& r = response.payload.socketReceiveResponse;
        if (r.err) {
            putErr(errP, r.err);
            return setResult(r.err == netErrSocketClosedByRemote ? 0 : -1);
        }

        if (received.len > bufLen) {
            putErr(errP, netErrInternal);
            return setResult(-1);
        }

        copyTo68k(bufP, received.data.get(), received.len);

        if (fromAddrP && fromLenP && r.has_address && paceGet16(fromLenP) >= 8) {
            writeAddress(fromAddrP, r.address);
            paceSet16(fromLenP, 8);
        }

        putErr(errP, 0);
        setResult(static_cast<int32_t>(received.len));
    }

    // NetHostInfoPtr NetLibGetHostByName(UInt16 libRefNum, const Char *nameP,
    //     NetHostInfoBufPtr bufP, Int32 timeout, Err *errP)
    void netLibGetHostByName(const Args& args) {
        const uint32_t bufP = args.u32(6);
        const uint32_t errP = args.u32(14);
        const std::string name = readString(args.u32(2), 256);

        MsgRequest request = newRequest(MsgRequest_getHostByNameRequest_tag);
        if (!bufP || name.empty() || name.size() >= sizeof(request.payload.getHostByNameRequest.name)) {
            putErr(errP, netErrParamErr);
            return setResultPtr(0);
        }

        strcpy(request.payload.getHostByNameRequest.name, name.c_str());

        MsgResponse response;
        const CallResult result = call(request, 0, response);
        if (result == CallResult::pending) return;
        if (result == CallResult::failed) {
            putErr(errP, netErrInternal);
            return setResultPtr(0);
        }

        const auto& r = response.payload.getHostByNameResponse;
        if (r.err || r.addresses_count > HOSTBUF_MAX_ADDRESSES) {
            putErr(errP, r.err ? r.err : netErrInternal);
            return setResultPtr(0);
        }

        const size_t nameLen = strnlen(r.name, 255);
        copyTo68k(bufP + HOSTBUF_NAME, reinterpret_cast<const uint8_t*>(r.name), nameLen);
        paceSet8(bufP + HOSTBUF_NAME + nameLen, 0);

        if (r.has_alias) {
            const size_t aliasLen = strnlen(r.alias, 255);
            copyTo68k(bufP + HOSTBUF_ALIASES, reinterpret_cast<const uint8_t*>(r.alias), aliasLen);
            paceSet8(bufP + HOSTBUF_ALIASES + aliasLen, 0);

            paceSet32(bufP + HOSTBUF_ALIAS_LIST, bufP + HOSTBUF_ALIASES);
            paceSet32(bufP + HOSTBUF_ALIAS_LIST + 4, 0);
        } else {
            paceSet32(bufP + HOSTBUF_ALIAS_LIST, 0);
        }

        for (pb_size_t i = 0; i < r.addresses_count; i++) {
            paceSet32(bufP + HOSTBUF_ADDRESS + 4 * i, r.addresses[i]);
            paceSet32(bufP + HOSTBUF_ADDRESS_LIST + 4 * i, bufP + HOSTBUF_ADDRESS + 4 * i);
        }
        paceSet32(bufP + HOSTBUF_ADDRESS_LIST + 4 * r.addresses_count, 0);

        // hostInfo: nameP, nameAliasesP, addrType, addrLen, addrListP
        paceSet32(bufP + 0, bufP + HOSTBUF_NAME);
        paceSet32(bufP + 4, bufP + HOSTBUF_ALIAS_LIST);
        paceSet16(bufP + 8, netSocketAddrINET);
        paceSet16(bufP + 10, 4);
        paceSet32(bufP + 12, bufP + HOSTBUF_ADDRESS_LIST);

        putErr(errP, 0);
        setResultPtr(bufP);
    }
}  // namespace

namespace {
    bool handleTrap(uint16_t trapWord) {

        // SysLibFind("Net.lib", &refNum) has returned once the next trap arrives.
        if (pendingRefNumP) {
            netRefNum = paceGet16(pendingRefNumP);
            pendingRefNumP = 0;
        }

        const Args args{paceGetAreg(7)};

        if (trapWord == TRAP_SYS_LIB_FIND) {
            if (readString(args.u32(0), 16) == "Net.lib") pendingRefNumP = args.u32(4);
            return false;
        }

        if (trapWord < SYS_LIB_TRAP_OPEN || trapWord > NET_TRAP_LAST) return false;
        if (netRefNum < 0 || args.u16(0) != netRefNum) return false;

        switch (trapWord) {
            case SYS_LIB_TRAP_OPEN:
                netLibOpen(args);
                return true;

            case SYS_LIB_TRAP_CLOSE:
                netLibClose(args);
                return true;

            case SYS_LIB_TRAP_SLEEP:
            case SYS_LIB_TRAP_WAKE:
                setResult(0);
                return true;

            case NET_TRAP_OPEN_COUNT:
                netLibOpenCount(args);
                return true;

            case NET_TRAP_OPEN_IF_CLOSE_WAIT:
                netLibOpenIfCloseWait(args);
                return true;

            case NET_TRAP_CONNECTION_REFRESH:
                netLibConnectionRefresh(args);
                return true;

            case NET_TRAP_IF_SETTING_GET:
                if (netLibIFSettingGet(args)) return true;

                fprintf(stderr, "netlib: IFSettingGet setting %u not redirected\n", args.u16(8));
                return false;

            // The host connection is always up; there is no close wait or power state to handle.
            case NET_TRAP_IF_UP:
            case NET_TRAP_FINISH_CLOSE_WAIT:
            case NET_TRAP_HANDLE_POWER_OFF:
                setResult(0);
                return true;

            case NET_TRAP_SOCKET_OPEN:
                netLibSocketOpen(args);
                return true;

            case NET_TRAP_SOCKET_CLOSE:
                netLibSocketClose(args);
                return true;

            case NET_TRAP_SOCKET_CONNECT:
                netLibSocketConnect(args);
                return true;

            case NET_TRAP_SOCKET_BIND:
                netLibSocketBind(args);
                return true;

            case NET_TRAP_SOCKET_OPTION_SET:
                netLibSocketOptionSet(args);
                return true;

            case NET_TRAP_SETTING_GET:
                if (netLibSettingGet(args)) return true;

                fprintf(stderr, "netlib: SettingGet setting %#06x not redirected\n", args.u16(2));
                return false;

            case NET_TRAP_SEND:
                netLibSend(args);
                return true;

            case NET_TRAP_RECEIVE:
                netLibReceive(args);
                return true;

            case NET_TRAP_GET_HOST_BY_NAME:
                netLibGetHostByName(args);
                return true;

            default:
                // Not redirected yet: the ROM handles it (and fails while its stack is closed).
                fprintf(stderr, "netlib: call %#06x not redirected\n", trapWord);
                return false;
        }
    }
}  // namespace

namespace netlib_redirect {
    void Configure(SendT newSend, SessionT newSession) {
        send = std::move(newSend);
        session = std::move(newSession);

        // Keep the library state (it may come from a restored session); only a call in flight
        // is dropped. An open library gets a host session.
        pending.active = false;
        {
            std::lock_guard lock(responseMutex);
            responseReady = false;
        }

        if (send && openCount > 0 && session) session(true);
    }

    bool IsEnabled() { return static_cast<bool>(send); }

    void Deliver(const uint8_t* response, size_t size) {
        std::lock_guard lock(responseMutex);

        responseData.assign(response, response + size);
        responseReady = true;
    }

    bool IsWaiting() { return pending.active; }

    void SetIdleHandler(std::function<void()> idle) { idleHandler = std::move(idle); }

    void Reset() {
        if (openCount > 0 && session) session(false);

        netRefNum = -1;
        pendingRefNumP = 0;
        openCount = 0;
        pending.active = false;

        std::lock_guard lock(responseMutex);
        responseReady = false;
    }

    int32_t GetRefNum() { return netRefNum; }

    uint32_t GetOpenCount() { return openCount; }

    void RestoreState(int32_t refNum, uint32_t count) {
        Reset();

        netRefNum = refNum;
        openCount = refNum >= 0 ? count : 0;

        if (openCount > 0 && session) session(true);
    }

    bool HandleTrap(uint16_t trapWord) {
        if (!send) return false;

        currentTrap = trapWord;
        currentSp = paceGetAreg(7);
        rewindRequested = false;

        if (!handleTrap(trapWord)) return false;

        // Waiting for a response: execute the same trap again (back to the trap #15 instruction)
        // after idling until the next interrupt.
        if (rewindRequested) {
            paceSetPC(paceGetPC() - 4);
            if (idleHandler) idleHandler();
        }

        return true;
    }
}  // namespace netlib_redirect
