// NetTest: a tiny 68k Palm OS app that exercises NetLib step by step and prints each
// result on screen. Used to test network integration in the emulator (OS 5 / uARM).
//
// Steps: SysLibFind("Net.lib") -> NetLibOpen -> NetLibGetHostByName -> NetLibSocketOpen ->
//        NetLibSocketConnect -> NetLibSend (HTTP GET) -> NetLibReceive -> close.
// Then, page 2 (after a tap): further NetLib calls — GetServByName, SocketAddr,
// SocketOptionGet, SendPB/ReceivePB, Shutdown, Bind/Listen/Accept on localhost.
// Tap the screen or press a key to go on; the app exits normally via the launcher.

#include <PalmOS.h>
#include <NetMgr.h>

// Override with e.g. make NETTEST_DEFS='-DHOST=\"localhost\" -DPORT=8080'
#ifndef HOST
#define HOST "example.com"
#endif
#ifndef PORT
#define PORT 80
#endif

static Coord y;

static void Log(const char *text) {
    if (y > 150) {
        WinEraseWindow();
        y = 0;
    }

    WinDrawChars(text, StrLen(text), 2, y);
    y += 11;
}

static void LogErr(const char *step, Err err) {
    char line[64];

    StrPrintF(line, "%s: err 0x%x", step, err);
    Log(line);
}

static void RunTest(void) {
    UInt16 refNum = 0, ifErr = 0;
    Err err;
    Int32 timeout = SysTicksPerSecond() * 15;
    NetHostInfoBufType hostBuf;
    NetHostInfoPtr host;
    NetSocketRef sock;
    NetSocketAddrINType addr;
    static const char request[] = "GET / HTTP/1.0\r\nHost: " HOST "\r\n\r\n";
    char reply[121];
    Int16 n;

    WinEraseWindow();
    y = 0;
    {
        char line[64];

        StrPrintF(line, "NetTest %s:%d", HOST, PORT);
        Log(line);
    }

    err = SysLibFind("Net.lib", &refNum);
    if (err) return LogErr("SysLibFind", err);

    err = NetLibOpen(refNum, &ifErr);
    if (err == netErrAlreadyOpen) err = 0;
    if (err || ifErr) {
        LogErr("NetLibOpen", err ? err : ifErr);
        if (err) return;
    }
    Log("NetLibOpen ok");

    host = NetLibGetHostByName(refNum, HOST, &hostBuf, timeout, &err);
    if (!host) {
        LogErr("GetHostByName", err);
        NetLibClose(refNum, false);
        return;
    }
    {
        char line[64];
        UInt8 *a = (UInt8 *)hostBuf.address;

        StrPrintF(line, "DNS: %d.%d.%d.%d", a[0], a[1], a[2], a[3]);
        Log(line);
    }

    sock = NetLibSocketOpen(refNum, netSocketAddrINET, netSocketTypeStream, netSocketProtoIPTCP,
                            timeout, &err);
    if (sock < 0) {
        LogErr("SocketOpen", err);
        NetLibClose(refNum, false);
        return;
    }

    MemSet(&addr, sizeof(addr), 0);
    addr.family = netSocketAddrINET;
    addr.port = NetHToNS(PORT);
    addr.addr = hostBuf.address[0];

    if (NetLibSocketConnect(refNum, sock, (NetSocketAddrType *)&addr, sizeof(addr), timeout,
                            &err) < 0) {
        LogErr("Connect", err);
    } else if (NetLibSend(refNum, sock, (void *)request, StrLen(request), 0, NULL, 0, timeout,
                          &err) < 0) {
        LogErr("Send", err);
    } else {
        Log("Connected, request sent");

        n = NetLibReceive(refNum, sock, reply, sizeof(reply) - 1, 0, NULL, NULL, timeout, &err);
        if (n < 0) {
            LogErr("Receive", err);
        } else {
            Int16 i, start = 0;

            reply[n] = 0;
            // Print the reply line by line (first lines of the HTTP header).
            for (i = 0; i <= n; i++) {
                if (reply[i] == '\r' || reply[i] == '\n' || reply[i] == 0) {
                    if (i > start) {
                        reply[i] = 0;
                        Log(reply + start);
                    }
                    start = i + 1;
                }
            }
        }
    }

    NetLibSocketClose(refNum, sock, timeout, &err);
    NetLibClose(refNum, false);
    Log("Done. Tap to run again.");
}

static NetSocketRef OpenConnected(UInt16 refNum, UInt32 ip, UInt16 port, Int32 timeout, Err *err) {
    NetSocketAddrINType addr;
    NetSocketRef sock = NetLibSocketOpen(refNum, netSocketAddrINET, netSocketTypeStream,
                                         netSocketProtoIPTCP, timeout, err);
    if (sock < 0) return sock;

    MemSet(&addr, sizeof(addr), 0);
    addr.family = netSocketAddrINET;
    addr.port = NetHToNS(port);
    addr.addr = ip;

    if (NetLibSocketConnect(refNum, sock, (NetSocketAddrType *)&addr, sizeof(addr), timeout, err) <
        0) {
        NetLibSocketClose(refNum, sock, timeout, err);
        return -1;
    }

    return sock;
}

static void RunExtraTest(void) {
    UInt16 refNum = 0, ifErr = 0;
    Err err;
    Int32 timeout = SysTicksPerSecond() * 15;
    NetHostInfoBufType hostBuf;
    NetServInfoBufType servBuf;
    NetServInfoPtr serv;
    NetSocketRef sock, lsock, csock, asock;
    NetSocketAddrINType loc, rem;
    Int16 locLen, remLen, n;
    UInt16 optLen, type = 0;
    UInt32 ip;
    char line[64], part1[12], part2[100];
    static const char request[] = "GET / HTTP/1.0\r\nHost: " HOST "\r\n\r\n";
    NetIOVecType iov[2];
    NetIOParamType pb;

    WinEraseWindow();
    y = 0;
    Log("NetTest page 2");

    err = SysLibFind("Net.lib", &refNum);
    if (err) return LogErr("SysLibFind", err);

    err = NetLibOpen(refNum, &ifErr);
    if (err && err != netErrAlreadyOpen) return LogErr("NetLibOpen", err);

    if (!NetLibGetHostByName(refNum, HOST, &hostBuf, timeout, &err)) {
        LogErr("DNS", err);
        NetLibClose(refNum, false);
        return;
    }
    ip = hostBuf.address[0];

    serv = NetLibGetServByName(refNum, "http", "tcp", &servBuf, timeout, &err);
    if (serv) {
        StrPrintF(line, "ServByName http: %u", serv->port);
        Log(line);
    } else
        LogErr("ServByName", err);

    sock = OpenConnected(refNum, ip, PORT, timeout, &err);
    if (sock < 0) {
        LogErr("Connect", err);
        NetLibClose(refNum, false);
        return;
    }

    locLen = remLen = sizeof(loc);
    if (NetLibSocketAddr(refNum, sock, (NetSocketAddrType *)&loc, &locLen,
                         (NetSocketAddrType *)&rem, &remLen, timeout, &err) < 0)
        LogErr("SocketAddr", err);
    else {
        UInt8 *a = (UInt8 *)&rem.addr;

        StrPrintF(line, "Addr rem %d.%d.%d.%d:%u", a[0], a[1], a[2], a[3], NetNToHS(rem.port));
        Log(line);
    }

    optLen = sizeof(type);
    if (NetLibSocketOptionGet(refNum, sock, netSocketOptLevelSocket, netSocketOptSockSocketType,
                              &type, &optLen, timeout, &err) < 0)
        LogErr("OptionGet", err);
    else {
        StrPrintF(line, "OptionGet type: %u", type);
        Log(line);
    }

    // SendPB: the request in two pieces
    iov[0].bufP = (UInt8 *)request;
    iov[0].bufLen = 4;
    iov[1].bufP = (UInt8 *)request + 4;
    iov[1].bufLen = StrLen(request) - 4;
    MemSet(&pb, sizeof(pb), 0);
    pb.iov = iov;
    pb.iovLen = 2;
    n = NetLibSendPB(refNum, sock, &pb, 0, timeout, &err);
    StrPrintF(line, "SendPB: %d", n);
    if (n < 0) LogErr("SendPB", err); else Log(line);

    // ReceivePB: into two buffers
    iov[0].bufP = (UInt8 *)part1;
    iov[0].bufLen = sizeof(part1) - 1;
    iov[1].bufP = (UInt8 *)part2;
    iov[1].bufLen = sizeof(part2) - 1;
    n = NetLibReceivePB(refNum, sock, &pb, 0, timeout, &err);
    if (n < 0)
        LogErr("ReceivePB", err);
    else {
        part1[n < (Int16)sizeof(part1) - 1 ? n : (Int16)sizeof(part1) - 1] = 0;
        StrPrintF(line, "ReceivePB %d: %s", n, part1);
        Log(line);
    }

    if (NetLibSocketShutdown(refNum, sock, netSocketDirBoth, timeout, &err) < 0)
        LogErr("Shutdown", err);
    else
        Log("Shutdown ok");
    NetLibSocketClose(refNum, sock, timeout, &err);

    // Bind/Listen/Accept on localhost
    lsock = NetLibSocketOpen(refNum, netSocketAddrINET, netSocketTypeStream, netSocketProtoIPTCP,
                             timeout, &err);
    MemSet(&loc, sizeof(loc), 0);
    loc.family = netSocketAddrINET;
    loc.addr = NetHToNL(0x7f000001);
    if (lsock < 0 ||
        NetLibSocketBind(refNum, lsock, (NetSocketAddrType *)&loc, sizeof(loc), timeout, &err) < 0 ||
        NetLibSocketListen(refNum, lsock, 1, timeout, &err) < 0) {
        LogErr("Bind/Listen", err);
    } else {
        locLen = sizeof(loc);
        NetLibSocketAddr(refNum, lsock, (NetSocketAddrType *)&loc, &locLen, NULL, NULL, timeout,
                         &err);
        StrPrintF(line, "Listening on %u", NetNToHS(loc.port));
        Log(line);

        csock = OpenConnected(refNum, NetHToNL(0x7f000001), NetNToHS(loc.port), timeout, &err);
        remLen = sizeof(rem);
        asock = csock < 0 ? -1
                          : NetLibSocketAccept(refNum, lsock, (NetSocketAddrType *)&rem, &remLen,
                                               timeout, &err);
        if (asock < 0)
            LogErr("Accept", err);
        else {
            NetLibSend(refNum, csock, "ping", 4, 0, NULL, 0, timeout, &err);
            n = NetLibReceive(refNum, asock, part2, sizeof(part2) - 1, 0, NULL, NULL, timeout, &err);
            part2[n > 0 ? n : 0] = 0;
            StrPrintF(line, "Accept ok, got '%s'", part2);
            Log(line);
            NetLibSocketClose(refNum, asock, timeout, &err);
        }
        if (csock >= 0) NetLibSocketClose(refNum, csock, timeout, &err);
    }
    if (lsock >= 0) NetLibSocketClose(refNum, lsock, timeout, &err);

    NetLibClose(refNum, false);
    Log("Page 2 done. Tap to repeat.");
}

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags) {
    Boolean page2 = false;
    EventType event;

    if (cmd != sysAppLaunchCmdNormalLaunch) return 0;

    RunTest();

    do {
        EvtGetEvent(&event, evtWaitForever);
        if (SysHandleEvent(&event)) continue;
        if (event.eType == penDownEvent ||
            (event.eType == keyDownEvent && !(event.data.keyDown.modifiers & commandKeyMask))) {
            page2 = !page2;
            if (page2)
                RunExtraTest();
            else
                RunTest();
        }
    } while (event.eType != appStopEvent);

    return 0;
}
