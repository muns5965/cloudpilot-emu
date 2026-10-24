# Network on Palm OS 5 (uARM): NetLib redirection

**Status (2026-10-24): experimental.** Browser: enable "Network redirection" in the settings
(the same proxy as on Palm OS 4). Native: start `cp-uarm` with `--net`.

## How it works

On Palm OS 4, CloudpilotEmu handles NetLib calls on the host (`PatchModuleNetlib.cpp`,
`NetworkProxy.cpp`) and sends them as protobuf requests (`proto/networking.proto`) to a network
backend: a WebSocket proxy in the browser, the host network in the native build.

On Palm OS 5, 68k applications run through PACE, and every 68k system call reaches the host in
`cpuPrvPaceSyscall()` → `patch68kHandle()`. `uarm/netlib_redirect.cpp` uses this point:

1. It watches `SysLibFind("Net.lib", &refNum)` and remembers the NetLib reference number.
2. Library traps (`0xA801`–`0xA845`) with that reference number are handled on the host: the
   arguments are read from the 68k stack (UInt16 and byte-sized enum arguments take 2 bytes,
   pointers and 32-bit values 4), a `MsgRequest` is sent, and the `MsgResponse` is written back
   (result in D0, pointers in A0, output structures in 68k memory).
3. Requests are sent asynchronously (`netlib_redirect::Configure(send, session)`); the response
   comes back through `netlib_redirect::Deliver()`, possibly from another thread. Until it is
   there, the trap is re-executed: the 68k PC is set back to the `trap #15` instruction and the
   SoC idles until the next interrupt (`SoC::Sleep()`), so waiting costs no host CPU and the rest
   of Palm OS (timers, interrupts) keeps running. When the response is there, the same trap
   completes and writes its results. No CPU state or call stack has to be saved, which is what
   the browser version needs (a Web Worker cannot block while its WebSocket answers).
4. The native build runs the requests on the host network with CloudpilotEmu's network backend
   (`src/cloudpilot/native/network-backend`, `native/NetlibNative.cpp`; the backend applies the
   call's timeout). `NETLIB_TEST_DELAY_MS=<ms>` delays each response to test the waiting path.
5. The browser build queues session and request events in the core (`web/uarm.cpp`). The worker
   takes them after each timeslice and sends them to the main thread, where the network backend
   of the Palm OS 4 integration (proxy or native) runs them (`uarm-network.service.ts`). The
   responses go back through the worker. An empty response makes the call fail.

Redirected calls: `NetLibOpen`, `NetLibClose`, `NetLibOpenCount`, sleep/wake, `SocketOpen`,
`SocketClose`, `SocketConnect`, `SocketBind`, `SocketOptionSet`, `Send`, `Receive` (with the peer
address for UDP), `GetHostByName`, `SettingGet` (host name and DNS servers, as in the OS 4
integration; other settings go to the ROM), and the connection state
calls that browsers make before connecting (as in the OS 4 integration): `OpenIfCloseWait`,
`ConnectionRefresh`, `IFUp`, `IFSettingGet` (only "interface up"), `FinishCloseWait`,
`HandlePowerOff`. Other NetLib calls go to the ROM and are logged as "not redirected".

Session state: the NetLib reference number (from `SysLibFind`) and the open count are saved in
the session (`netlib` savestate chunk, optional for older sessions). An application that is
running when the session is saved stays redirected after it is restored; if NetLib was open, a
new host session is opened (sockets of the old one are gone).

## Limits

- Only 68k callers pass through PACE. ARM-native code that calls NetLib directly is not seen; that
  needs a hook on the ARM side (`patches.cpp` / `patch_dispatch`), a later step.
- No `NetLibSelect`, `SocketListen`/`Accept`, `SocketOptionGet`, `GetServByName`,
  `SendPB`/`ReceivePB` yet; applications that need them fail at that call. EudoraWeb 2.1 resolves
  names itself over UDP (DNS server from `SettingGet`) and waits for the answer with
  `NetLibSelect`.
- Xiino 3.4 contacts its vendor's server (`pds.mobirus.com`) first; that server no longer exists,
  so it reports "Could not find host" even though the network calls work.
- HTTPS depends on the Palm application's own TLS.

## Test

`tools/palm-nettest` (68k, built with prc-tools-remix). Native TG50, `--net`, NetTest built for
`localhost:8790` with a local `python3 -m http.server 8790`: NetLibOpen ok → DNS 127.0.0.1 →
connected, request sent → `HTTP/1.0 200 OK` and the server's headers on screen; the server logged
`GET / HTTP/1.0` from the emulator. With `NETLIB_TEST_DELAY_MS=1500` the steps appear one by one
and the run completes; host CPU while waiting stays at the idle level (~25 %, it was ~99 % before
the SoC idled during the wait). Without `--net`, `NetLibOpen` shows Palm OS's "Incomplete
setup" alert (no connection configured).

Browser (PWA build of the same commit, Chromium via Playwright): TG50 session, "Network
redirection" on, local proxy (`server/cloudpilot-server.py serve --insecure`), the same NetTest:
same result on screen; the proxy logged the handshake, `getHostByName localhost`, socket open,
connect to 127.0.0.1:8790, send, receive and close. Running it again opens a new proxy
connection each time.
