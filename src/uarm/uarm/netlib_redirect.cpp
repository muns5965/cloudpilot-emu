#include "netlib_redirect.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

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
    constexpr uint16_t NET_TRAP_SOCKET_CONNECT = SYS_LIB_TRAP_CUSTOM + 7;
    constexpr uint16_t NET_TRAP_SEND = SYS_LIB_TRAP_CUSTOM + 12;
    constexpr uint16_t NET_TRAP_RECEIVE = SYS_LIB_TRAP_CUSTOM + 14;
    constexpr uint16_t NET_TRAP_GET_HOST_BY_NAME = SYS_LIB_TRAP_CUSTOM + 22;
    constexpr uint16_t NET_TRAP_OPEN_COUNT = SYS_LIB_TRAP_CUSTOM + 41;
    constexpr uint16_t NET_TRAP_LAST = SYS_LIB_TRAP_CUSTOM + 64;

    constexpr uint16_t netErrorClass = 0x1200;
    constexpr uint16_t netErrNotOpen = netErrorClass | 2;
    constexpr uint16_t netErrParamErr = netErrorClass | 4;
    constexpr uint16_t netErrInternal = netErrorClass | 17;
    constexpr uint16_t netErrSocketClosedByRemote = netErrorClass | 20;

    constexpr uint8_t netSocketAddrINET = 2;

    // NetHostInfoBufType (68k layout)
    constexpr uint32_t HOSTBUF_NAME = 16;
    constexpr uint32_t HOSTBUF_ALIAS_LIST = 272;
    constexpr uint32_t HOSTBUF_ALIASES = 280;
    constexpr uint32_t HOSTBUF_ADDRESS_LIST = 536;
    constexpr uint32_t HOSTBUF_ADDRESS = 552;
    constexpr uint32_t HOSTBUF_MAX_ADDRESSES = 3;  // the proto carries up to 3

    netlib_redirect::RpcT rpc;
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

    bool call(MsgRequest& request, size_t extraSize, MsgResponse& response,
              ReceivedBuffer* received = nullptr) {
        if (!rpc || openCount == 0) return false;

        std::vector<uint8_t> encoded(512 + extraSize);
        pb_ostream_t ostream = pb_ostream_from_buffer(encoded.data(), encoded.size());
        if (!pb_encode(&ostream, MsgRequest_fields, &request)) {
            fprintf(stderr, "netlib: encode failed: %s\n", PB_GET_ERROR(&ostream));
            return false;
        }

        std::vector<uint8_t> reply;
        if (!rpc(encoded.data(), ostream.bytes_written, reply)) return false;

        response = MsgResponse_init_zero;
        if (received) {
            response.cb_payload.arg = received;
            response.cb_payload.funcs.decode = setupPayloadDecodeCb;
        }

        pb_istream_t istream = pb_istream_from_buffer(reply.data(), reply.size());
        if (!pb_decode(&istream, MsgResponse_fields, &response)) {
            fprintf(stderr, "netlib: decode failed: %s\n", PB_GET_ERROR(&istream));
            return false;
        }

        return response.id == request.id && response.which_payload == request.which_payload;
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
        if (!call(request, 0, response)) {
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
        if (!call(request, 0, response)) {
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
        if (!call(request, 0, response)) {
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
        if (!call(request, len, response)) {
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
        if (!call(request, 0, response, &received)) {
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
        if (!call(request, 0, response)) {
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

namespace netlib_redirect {
    void Configure(RpcT newRpc, SessionT newSession) {
        rpc = std::move(newRpc);
        session = std::move(newSession);

        Reset();
    }

    bool IsEnabled() { return static_cast<bool>(rpc); }

    void Reset() {
        if (openCount > 0 && session) session(false);

        netRefNum = -1;
        pendingRefNumP = 0;
        openCount = 0;
    }

    bool HandleTrap(uint16_t trapWord) {
        if (!rpc) return false;

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

            case NET_TRAP_SOCKET_OPEN:
                netLibSocketOpen(args);
                return true;

            case NET_TRAP_SOCKET_CLOSE:
                netLibSocketClose(args);
                return true;

            case NET_TRAP_SOCKET_CONNECT:
                netLibSocketConnect(args);
                return true;

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
}  // namespace netlib_redirect
