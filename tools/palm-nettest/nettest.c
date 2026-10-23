// NetTest: a tiny 68k Palm OS app that exercises NetLib step by step and prints each
// result on screen. Used to test network integration in the emulator (OS 5 / uARM).
//
// Steps: SysLibFind("Net.lib") -> NetLibOpen -> NetLibGetHostByName -> NetLibSocketOpen ->
//        NetLibSocketConnect -> NetLibSend (HTTP GET) -> NetLibReceive -> close.
// Tap the screen or press a key to run again; the app exits normally via the launcher.

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

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags) {
    EventType event;

    if (cmd != sysAppLaunchCmdNormalLaunch) return 0;

    RunTest();

    do {
        EvtGetEvent(&event, evtWaitForever);
        if (SysHandleEvent(&event)) continue;
        if (event.eType == penDownEvent ||
            (event.eType == keyDownEvent && !(event.data.keyDown.modifiers & commandKeyMask))) {
            RunTest();
        }
    } while (event.eType != appStopEvent);

    return 0;
}
