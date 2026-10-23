# Network on Palm OS 5 (uARM): NetLib redirection

**Status (2026-10-24): native only, experimental.** Start `cp-uarm` with `--net`. The browser
version does not have it yet.

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
3. The native build runs the requests on the host network with CloudpilotEmu's network backend
   (`src/cloudpilot/native/network-backend`, `native/NetlibNative.cpp`). The emulator waits for
   the answer; the backend applies the call's timeout.

Redirected calls: `NetLibOpen`, `NetLibClose`, `NetLibOpenCount`, sleep/wake, `SocketOpen`,
`SocketClose`, `SocketConnect`, `Send`, `Receive`, `GetHostByName`. Other NetLib calls go to the
ROM and are logged as "not redirected".

## Limits

- Only 68k callers pass through PACE. ARM-native code that calls NetLib directly is not seen; that
  needs a hook on the ARM side (`patches.cpp` / `patch_dispatch`), a later step.
- No `NetLibSelect`, `SocketBind`/`Listen`/`Accept`, `SocketOptionSet/Get`, `SettingGet`,
  `GetServByName`, `SendPB`/`ReceivePB` yet; applications that need them fail at that call.
- HTTPS depends on the Palm application's own TLS.

## Test

`tools/palm-nettest` (68k, built with prc-tools-remix). Native TG50, `--net`, NetTest built for
`localhost:8790` with a local `python3 -m http.server 8790`: NetLibOpen ok → DNS 127.0.0.1 →
connected, request sent → `HTTP/1.0 200 OK` and the server's headers on screen; the server logged
`GET / HTTP/1.0` from the emulator. Without `--net`, `NetLibOpen` shows Palm OS's "Incomplete
setup" alert (no connection configured).
