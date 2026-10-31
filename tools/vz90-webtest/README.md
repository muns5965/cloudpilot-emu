# VZ90 web test runner (local)

Automated tests of the web build with an original VZ90 ROM and a pv-vz90-dal bundle, driven by Playwright in
headless Chromium. Nothing here contains ROM or DAL data; both are given on the command line.

## Builds

- Regular web build: `make -Csrc emscripten`, then `yarn build-preview` in `web/`.
- Diagnostic build (same, plus `CXXFLAGS_EMCC_EXTRA=-DUARM_DIAG` for `make`): the uarm module prints
  `[diag]` lines (memory aborts per address/pc site with region and code location, undefined instructions, and a
  summary every 10 s with instructions per second and wasm heap size). Regular builds compile none of it.

## Runs

```
node vz90-webtest.js setup  --build web/build-pwa --rom <VZ90 ROM> --dal <bundle> --work <dir> [--port 8790]
node vz90-webtest.js rounds --build web/build-pwa --work <dir> [--rounds N] [--label name]
node vz90-webtest.js soak   --build web/build-pwa --work <dir> [--loops N] [--label name]
```

- `setup`: registers ROM + bundle, measures boot time, runs Welcome (calibration targets read from the screen),
  unassigns Home from the Media Launcher, ends on the standard launcher and copies the browser profile as baseline.
- `rounds`: each round starts from a copy of the baseline profile: T0 restore equals the baseline, T2 Prefs/General
  and launching ten launcher entries (each must answer and home must lead back to the launcher), T4 leave and
  relaunch equals the screen before.
- `soak`: one browser kept open, the launcher cycle repeated `--loops` times; memory and speed per loop.
- The port is fixed per work directory: the saved session lives in the browser storage of that origin.

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
