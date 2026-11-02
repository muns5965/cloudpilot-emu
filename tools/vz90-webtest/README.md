# VZ90 web test runner (local)

Automated tests of the web build with an original VZ90 ROM and a pv-vz90-dal bundle, driven by Playwright in
headless Chromium. Nothing here contains ROM or DAL data; the ROM is given on the command line. The build's own DAL is used unless
`--dal` selects a bundle through the development option `?vz90dal=select`.

## Builds

- Regular web build: `make -Csrc emscripten`, then `yarn build-preview` in `web/`.
- Diagnostic build (same, plus `CXXFLAGS_EMCC_EXTRA=-DUARM_DIAG` for `make`): the uarm module prints
  `[diag]` lines (memory aborts per address/pc site with region and code location, undefined instructions, and a
  summary every 10 s with instructions per second and wasm heap size). Regular builds compile none of it.

## Runs

```
node vz90-webtest.js setup  --build web/build-pwa --rom <VZ90 ROM> [--dal <bundle>] [--profile jp|en-mini] --work <dir> [--port 8790]
node vz90-webtest.js rounds --build web/build-pwa --work <dir> [--rounds N] [--label name]
node vz90-webtest.js soak   --build web/build-pwa --work <dir> [--loops N] [--label name]
```

- `setup`: registers ROM + bundle, measures boot time, runs Welcome (calibration targets read from the screen),
  unassigns Home from the Media Launcher, ends on the standard launcher and copies the browser profile as baseline.
- `rounds`: each round starts from a copy of the baseline profile: T0 restore equals the baseline, T2 Prefs/General
  and launching ten launcher entries (each must answer and home must lead back to the launcher), T4 leave and
  relaunch equals the screen before.
- `--profile en-mini` (setup; kept in `baseline.json`, used by `rounds` and `soak`): the RomFlash.VZ English "mini"
  ROM. Setup ends on the standard launcher with the input area shown (nothing to unassign); rounds run T0, T2 (Prefs
  through the launcher scroll arrow, eleven launcher entries), T3, T4 and then data in the standard applications with
  the on-screen QWERTY keyboard: T6/T7 Address (add, edit to 2Test, delete), T8 To Do (checked), T9 Memo Pad, T10
  Date Book (event at 8:00), each checked right away, after reopening and after a session relaunch (golden images
  `golden-mini-*`, verified with `verify-golden mini-<name>`).
- `soak`: one browser kept open, the launcher cycle repeated `--loops` times; memory and speed per loop.
- The port is fixed per work directory: the saved session lives in the browser storage of that origin.
- `--date` (default `2026-11-02T09:00:00`, UTC, browser time zone UTC): the page clock, and so the emulated device
  clock, starts at this time on every page load, so screens with the date (Date Book) match their golden images on any
  day. `--date ''` uses the real clock.

## Verdict

Failures: DAL stop or fatal output (`g2d:`, `g2e:`, `HALErrDisplay`, `g1: EXCEPTION`, `g2: DATA ABORT`), invalid
instruction, wasm trap, page error, no screen change where expected, not back on the launcher, no diag summary for
30 s (diagnostic builds). Memory aborts are observations; on a failure the diag events before it are listed as
suspected origin. Each run writes `result.json`, `console.txt`, `diag.txt`, a screenshot per step and, per failed
step, a `fail-*` directory.

## Fault injection

`mkfault.py <bundle> <out> <hex address> <hex word>...` writes a copy of a bundle with instructions of the DAL
replaced (hashes recomputed), to check that the runner finds where a fault starts. Addresses come from the DAL's
symbol table (`arm-none-eabi-nm build/dal-fullrom.elf`).
