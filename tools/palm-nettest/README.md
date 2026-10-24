# NetTest (68k Palm OS)

A tiny Palm OS application that calls NetLib step by step and prints each result:
`SysLibFind("Net.lib")` → `NetLibOpen` → `NetLibGetHostByName` → `NetLibSocketOpen` →
`NetLibSocketConnect` → `NetLibSend` (HTTP GET) → `NetLibReceive` → close.

Tap for page 2: `NetLibGetServByName("http")`, `NetLibSocketAddr`, `NetLibSocketOptionGet`
(socket type), the HTTP request again with `NetLibSendPB` (two pieces) and `NetLibReceivePB` (two
buffers), `NetLibSocketShutdown`, and `Bind`/`Listen`/`Accept` with a connection to itself on
127.0.0.1. Tap again to go back to page 1.

It is used to test NetLib redirection on Palm OS 5 ([docs/os5-network.md](../../docs/os5-network.md)).

## Build

Needs [prc-tools-remix](https://github.com/jichu4n/prc-tools-remix) (`m68k-palmos-gcc`,
`build-prc`) and the Palm OS 5 SDK headers ([jichu4n/palm-os-sdk](https://github.com/jichu4n/palm-os-sdk),
`sdk-5r3`). The SDK may not be redistributed and is not part of this repository.

```sh
make PALMOS_SDK=/opt/palmdev/sdk-5r3/include
# another host or port:
make NETTEST_DEFS='-DHOST=\"localhost\" -DPORT=8080'
```

## Run (native)

```sh
./src/uarm/cp-uarm --net /path/to/TG50-ROM.bin
# in the terminal:
install tools/palm-nettest/NetTest.prc
launch NetTest
```
