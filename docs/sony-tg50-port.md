# Sony CLIÉ PEG-TG50 port (work in progress)

This branch runs the **Sony CLIÉ PEG-TG50 (Palm OS 5)** in CloudpilotEmu's **native** build. As of 2026-10-23 the TG50 ROM boots, completes Setup including pen calibration, and reaches the CLIÉ Launcher, which responds to taps. Browser/WASM support does not exist yet. No ROM files are included.

## Current status (2026-10-23)

Tested with a native Linux SDL2 build of `feature/sony-tg50` and a TG50 NOR ROM whose SHA-256 matches the uARM README reference (`c6d5985cf183bae970e711430e51ae491088a5c173df555b9177e69ac860b94f`). Launch: `./src/uarm/cp-uarm --tg50 <rom>`.

| Area | Status | Notes |
| --- | --- | --- |
| Native build (`libcommon.a` + `cp-uarm`) | Builds locally, no warnings | GitHub Actions result not checked |
| ROM selection | `--tg50` override | No automatic TG50 ROM identification |
| Boot | Works | Needed GPIO13/14 idle levels and the GPIO0 power-key fix (below) |
| Setup 1–4, pen calibration | Works | Needed the touch-translation patch fix (below) |
| Launcher, on-screen touch (AD7873) | Works | Calibrated by Palm OS itself |
| Sony controller keyboard matrix | Works (typing in Memo Pad) | `mmiodev_TG50uc.cpp`, `sony_tg50_hardware.h` |
| Host key mapping | Works: letters, Space, Enter, Backspace, Shift, Ctrl, Tab, Caps, `,` `.` `-`, Blue (F7), Red (F9), Home, PageUp/Down, End (jog back), F11 (jog select), F5 (voice), `` ` `` (Graffiti) | New `KeyId` values in `keys.h`; SDL mapping only in TG50 mode. PC digits unmapped. ~40 ms key presses drop some keys (original uARM does not) |
| Host power key, sleep/resume | Works | Esc sleeps and wakes; see "Sleep and resume" below |
| Display configuration / skin | Uses E2 `mode_320x320` incl. a 120 px Graffiti silkscreen | TG50 has a QWERTY keyboard and **no silkscreen**; the drawn silkscreen does nothing |
| AN32502A power, AK4534 codec, Memory Stick | Attached; register models only | No PCM output, no media |
| Savestates / sessions | Not implemented for TG50 | `--tg50` rejects session files |
| Browser/WASM | Not implemented | — |

## Remaining work

1. **Sleep follow-ups:** RTC alarm wake is not checked (the ROM's sleep PWER has no RTC bit).
2. **Display configuration:** a TG50 display mode without the E2 silkscreen (or with a keyboard image), without changing E2's shared configuration.
3. **Savestates:** serialize Sony controller, AD7873, AN32502A, AK4534 and Memory Stick state.
4. **Optional:** a TG50-specific touch-translation patch (inverse of `3200 − 10·x/y`, after verifying the X/Y cross-wiring) to make touch independent of calibration, as on the E2.
5. **Browser/WASM:** device selection, frame output and input bridge; then audio and Memory Stick media.

## Scope / safety

No ROM files or proprietary Sony firmware are committed. Do not advertise TG50 as a supported browser device until the remaining work above is done. Existing E2, FrankenE2 and PV behavior must stay unchanged; TG50-specific behavior is gated by `deviceTypeSonyTG50` / `tg50Mode`.

## Debugging method

All boot and Setup blockers so far were found by running the same ROM in original uARM (TG50 build) side by side: comparing register reads via the gdb stub, and logging AD7873 SSP commands and results in both emulators. The gdb stub in `cp-uarm` needs `-DGDB_STUB_ENABLED` (at the time of writing it also needed a local fix of a mistyped `<unicstd>` include); keep such diagnostic changes out of commits.

---

# Development history

The sections below are kept in chronological order. Status lines in older sections reflect the time they were written; the current status is above.

## Original source survey

- Original uARM: [deviceSonyTG50.c](https://github.com/uARM-Palm/uARM/blob/master/deviceSonyTG50.c), [mmiodev_TG50uc.c](https://github.com/uARM-Palm/uARM/blob/master/mmiodev_TG50uc.c), [Makefile](https://github.com/uARM-Palm/uARM/blob/master/Makefile).
- CloudpilotEmu: `src/uarm/uarm/devicePalmTungstenE2.cpp`, `src/common/device_type5.h`, `src/uarm/uarm/device_configuration.cpp`, `src/uarm/Makefile`.
- Original uARM ships a TG50 device model (its standard Makefile has the TG50 configuration commented out and enables T3 by default).
- The original TG50 code uses PXA25x, a Sony controller mapped at `0x08000000` (size `0x220`), AD7873 touch, AN32502A power, AK4534 audio, a Memory Stick controller, and an 8x8 keyboard matrix on the Sony controller's GPIO port C.
- Key symbols in `sony_tg50_hardware.h` are semantic; **SDL key constants must not be copied into the web emulator**.

## Latest controller validation work

- Fixed a keyboard matrix scan error: previously every row could overwrite the selected column's state; the implementation now calculates each column after checking all driven rows (commit `d53cd7f`).
- Added `.github/workflows/tg50-controller.yml` to perform C++ syntax checks on the controller and hardware header (`f3f9a09`). The presence of this workflow is **not** evidence of a passing run.
- TG50 device registration, runtime MMIO tests, complete build and ROM boot remain **unverified**.

## AD7873 touchscreen port (staged)

- Added `src/uarm/uarm/sspdev_AD7873.cpp` and `.h` from original uARM's AD7873 implementation, adapted to `PxaSsp` / `PxaGpio`.
- Preserved the 12-bit pipelined SSP conversion responses (X/Y, battery, auxiliary, Z1), TG50's reversed X/Y wiring, and GPIO 9 pen-interrupt update.
- Added source to `src/uarm/Makefile` and `.github/workflows/tg50-controller.yml` syntax checks.
- **No actual compile success, ROM startup, or touch input is yet verified.** The port is not connected to `deviceSetup` because TG50 device registration and other hardware dependencies remain incomplete.

## AN32502A I²C controller (staged)

- Added `src/uarm/uarm/i2cdev_AN32502A.cpp` and `.h` using CloudpilotEmu's `PxaI2c` interface.
- Original uARM behavior preserved: 7-bit address `0x73`, ACK for register-pointer writes and writes to registers 0–2, zero-valued register reads, start/restart/stop state handling.
- Registered the source in `src/uarm/Makefile` and the syntax-check workflow.
- **Unverified:** a native compile success, I²C transaction test, TG50 device attachment, and ROM boot. The model is not yet instantiated in a device.

## AK4534 codec (staged)

- Added `src/uarm/uarm/i2sdev_AK4534.cpp` and `.h`, adapted to CloudpilotEmu's `PxaI2c` API. The original uARM model registers an I²C client but does not attach a real I²S audio stream; this port likewise emulates register access only.
- I²C address `0x10`, default register values and register read/write state handling were preserved; the model is included in `src/uarm/Makefile` and the TG50 syntax-check workflow.
- **Not verified:** passing native build or CI checks, live I²C register access, sound output, and TG50 ROM boot. This peripheral has not been attached to a TG50 device instance.

## Memory Stick controller (staged)

- Ported the original uARM 0x400-byte MMIO register-only model to `src/uarm/uarm/mmiodev_MemoryStickController.cpp` and `.h`.
- Default system register `0x4455`, 16-bit register accesses, write-one-to-clear status flags, and two 32-bit reply registers are represented.
- Registered in `src/uarm/Makefile` and the TG50 C++ syntax-check workflow.
- **Limitations:** no card insertion, storage protocol or media transfer; no actual compilation success, TG50 device attachment or ROM boot has been verified.

## Device peripheral assembly (staged, not active)

- Added `src/uarm/uarm/deviceSonyTG50.cpp/.h`. The module initializes all five staged peripheral devices, sets GPIO reset/card/interrupt defaults and battery voltage, and provides stylus and generic host hardware key forwarding into the Sony keyboard matrix.
- Added to `src/uarm/Makefile` and the TG50 syntax-check workflow. There is **no evidence yet of a passing compile or runtime test**.
- **Not integrated:** the current `deviceSetup` factory remains E2-specific; TG50 is not registered in `DeviceType5` or ROM/RAM configuration, and no TG50 ROM boot is possible.
- **Important design issue:** callbacks/MMIO registration is not reversible. A partial initialization failure currently returns null while registered clients may remain; robust cleanup/rollback must be addressed before selecting TG50 at runtime.
- **Additional integration work:** Sony's extra 1 KiB SRAM at `0xac000000`, RAM mirror boundaries, savestates, keyboard host input including QWERTY, and the actual device type/factory dispatch all remain outstanding.

## Device type registration (staged)

- Reserved `deviceTypeSonyTG50 = 3` in `src/common/device_type5.h` (existing E2/E3/PV numeric IDs unchanged).
- Added 17 MiB RAM-size acceptance and a preliminary 320×320 display capability to `src/uarm/uarm/device_configuration.cpp`. Display geometry requires validation: current generic 320×320 mode also carries a Graffiti area size that may not suit TG50.
- Added an explicit guard in `devicePalmTungstenE2.cpp`: selecting TG50 fails instead of incorrectly attaching E2 NAND/AC97 hardware. This is a **temporary safety guard**, not the TG50 device factory.
- **Still blocked:** separate TG50 `deviceSetup` dispatch and `Device` lifecycle, ROM mapping, 1 KiB extra SRAM, ROM boot and save-state handling. Neither native build nor CI success has been independently verified.

## First native factory dispatch (experimental, unverified)

- `devicePalmTungstenE2.cpp` now recognizes `deviceTypeSonyTG50`, allocates a TG50-flavored Device wrapper and calls `sonyTG50Initialize` instead of E2 NAND/AC97/WM9712L setup.
- Device periodic/touch/hardware-key operations dispatch to the Sony model, and `SocPXA::OnReset` skips the absent NAND page buffer.
- **Not boot validated:** ROM format/mapping, 1 KiB SRAM mapping, full QWERTY host input and LCD geometry remain outstanding. A source-only GitHub update is not proof of a successful build.
- **Savestates incomplete:** the Sony-specific controller state is not serialized. The temporary factory branch skips Sony peripheral save/load and should not be considered safe for persistent sessions. Do not distribute as working TG50 support.
- Generic keypad events still pass through the common keypad interface; review duplicate or unintended input when TG50 host mapping is brought online.

## TG50 RAM and extra SRAM mapping (staged, not boot-verified)

- `SocPXA` now defaults TG50 to 17 MiB RAM when the caller requests size zero. Existing E2 and other defaults are unchanged.
- `SocPXA::AllocateBuffers` reserves a separate 1 KiB `MemoryBuffer` for the original TG50's otherwise-unknown RAM region; `ramInit` maps that buffer at `0xAC000000`.
- The existing `RamTerminationMirror` path remains in use and has not been validated with a real ROM's address probes. The model is not ready for use with TG50 firmware.
- Expanded TG50 syntax-check CI to include the SoC and E2/TG50 dispatch sources. This is **not** proof the workflow passed, nor does a syntax check cover linker errors or runtime.
- **Remaining:** TG50 ROM format/startup checks, physical keyboard host input, savestate serialization, proper card/no-card behavior, a full native build, and a real ROM boot test.

## Native build CI added (verification pending)

- Added `.github/workflows/tg50-native-build.yml` for pushes to the TG50 development branch and manual dispatch. It recursively checks out git submodules, installs SDL2/SDL2-image, readline and curl development dependencies, builds `src/common/libcommon.a`, then builds/links `src/uarm/cp-uarm`.
- On success the workflow uploads an **experimental Linux binary**, which does not imply a working TG50 ROM boot.
- **CI run status and build result have not been independently confirmed.** Check [TG50 native build Actions](https://github.com/muns5965/cloudpilot-emu/actions/workflows/tg50-native-build.yml) for errors before proceeding with additional emulation claims.

## Controller code review: GPIO direction write fix

- Fixed `mmiodev_TG50uc.cpp`: on writes to GPIO direction registers `0x20–0x2c`, immediately recalculate output/input state and rising/falling edge flags. Port C (`0x28`) also recalculates the scanned keyboard matrix. Previously recalculation incorrectly happened on **reads**, which could postpone keyboard transitions and their interrupt detection.
- This patch is grounded in source review but **does not establish that the native CI build succeeds**. The connected GitHub workflow interface does not currently provide a list of push-triggered runs; check the GitHub Actions page for the authoritative result.

## Critical SRAM backing-buffer fix

- Fixed a deterministic initialization failure: `memoryBufferAllocate` and `memoryBufferGetSubBuffer` require sizes and offsets aligned to `MEMORY_BUFFER_GRANULARITY` (32 KiB). The previous TG50 code attempted to reserve a 1 KiB buffer, so memory allocation/subbuffer creation failed.
- `SocPXA` now reserves a 32 KiB backing buffer for Sony's extra SRAM and maps **only 1 KiB** into the physical address space at `0xAC000000` via `ramInit`. This intentionally retains the real device-visible size.
- GitHub Actions build result is **still unverified**; this is a source-confirmed defect correction, not a claim that the emulator compiles or boots.

## Keypad initial-state regression fix

- Recalculated TG50 GPIOC keyboard columns on controller initialization and on a controller reset (`0xff` at byte offset `0x210`). Columns with no pressed/driven row idle high in this model; previously they could remain zero until the first host key or GPIO write.
- This is a source-level correction only, **not a confirmed CI pass**. The GitHub connector available here exposes logs only after a workflow run/job ID is known; it cannot enumerate push-triggered runs for this repository.

## GPIO13 / GPIO14 wiring: upstream implementation is authoritative

The original `uARM-Palm/uARM/deviceSonyTG50.c` **uses both signal lines**:

- **GPIO13**: `socGpioSetState(sp->gpio, 13, true)` holds a separate key-related line inactive. The upstream comment describing an overall UC IRQ on GPIO13 says *seems*, not established behavior.
- **GPIO14**: `tg50ucInit(sp->mem, sp->gpio, 14, keyMap)` connects the controller-driven active-low interrupt to GPIO14. This is the original implemented behavior associated with the hard-key interrupt.

Our previous changes `a8c63f3` (initializing only GPIO14) and `6531cd9` (moving the UC IRQ onto GPIO13) each erroneously combined two distinct roles and are superseded. The fork now uses `kKeyRelatedIdleGpio = 13` and `kUcIrqGpio = 14`, initializes **both** high, and allows the UC to drive only GPIO14. The correct physical edge handling still needs ROM comparison and PXA GRER/GFER tracing; no boot verification is claimed.

## Optional interrupt transition tracing

- TG50 UC GPIO IRQ recalculation now tracks the last asserted state to avoid writing an unchanged line repeatedly.
- Compile `mmiodev_TG50uc.cpp` with `-DTG50_TRACE_IRQ` to emit assert/release messages and per-port pending/mask registers to stderr.
- This supports future ROM testing of the separate GPIO13 idle line and GPIO14 controller-driven IRQ. It does **not** independently validate the real hardware.
- Full native build and ROM boot remain unverified.

## PXA GPIO13/GPIO14 IRQ diagnostics

- Added optional `TG50_TRACE_GPIO` tracing to `src/uarm/uarm/pxa_GPIO.cpp`. It logs changes to GPIO13/14 input levels and firmware writes to PXA GPIO bank 0 rising/falling edge configuration (GRER0/GFER0), together with current edge-detected flags (GEDR0). The trace is **disabled by default**.
- Compile with `-DTG50_TRACE_GPIO -DTG50_TRACE_IRQ` to combine the PXA-level observations with controller assert/release logs.
- Compare both lines' idle states, GRER0/GFER0 settings, and pressed/released key transitions against upstream uARM using the same TG50 ROM.
- **No ROM run or passing build is claimed.** This patch instruments the existing implementation; it does not assert that GPIO13 and GPIO14 have interchangeable roles.

## IRQ diagnostic compile coverage

- Updated `.github/workflows/tg50-controller.yml` to check `pxa_GPIO.cpp` in both standard and `TG50_TRACE_GPIO` builds, and `mmiodev_TG50uc.cpp` with `TG50_TRACE_IRQ`.
- Added `pxa_GPIO.cpp` to the workflow's push path filters, so changes to the diagnostic implementation request a syntax-check run.
- This is a CI configuration change only; successful GitHub Actions execution and TG50 ROM boot are **not yet verified**.

## Confirmed native startup blocker: ROM identification

- Native `src/uarm/native/main.cpp` calls `RomInfo5::GetDeviceType()`, rejects `deviceTypeInvalid`, and chooses the SoC from the returned type.
- `src/common/rom_info5.cpp` currently recognizes only PV, Tungsten E2, and FrankenE2 identifiers, **not Sony PEG-TG50**. Merely adding TG50 to the enum and `SocPXA` dispatch does not make ROM boot accessible.
- Do **not** guess a Sony company/HAL identifier or accept every unrecognized ROM as TG50. Obtain the TG50 ROM's actual header IDs or the original uARM ROM detection behavior, then implement a narrow TG50 mapping and recommended 17 MiB RAM.
- This is a source-confirmed **startup integration gap**, not a native compilation failure. The CI result remains unverified.
- *Resolved for native use by the explicit `--tg50` option (next section). Automatic identification is still not implemented.*

## Native TG50 ROM launch entrypoint (experimental)

The native executable now accepts `--tg50` explicitly:

```sh
./src/uarm/cp-uarm --tg50 /path/to/your/TG50-ROM.bin
```

This bypasses only the **device-type identification decision**; the image must still pass the existing Palm OS 5 NOR ROM parser. It selects `deviceTypeSonyTG50`, uses 17 MiB RAM by default, and rejects an explicit NAND image or a session-file input. Other devices still use their normal `RomInfo5::GetDeviceType()` identification path. This is not auto-detection. ROM images are not included in the repository.
- *Later verified: the reference TG50 ROM passes the parser and boots (see the following sections).*

## Boot blocker fixed: power key GPIO0 idle level

- Symptom: with `--tg50`, the ROM is accepted and PACE is patched, but the screen stays blank. A gdb remote probe showed the CPU spinning at `0x20036DBC`–`0x20036DC4`, polling `GPLR0` (`0x40E00000`) bit 0 and debouncing it.
- Reference: the original uARM TG50 build, run on the same ROM, reaches the same function, reads `GPLR0 = 0x0800E68B` (bit 0 = 1) and leaves the loop within 0.2 s; this port read `0x0800E68A` (bit 0 = 0), the only differing bit.
- Cause: original `deviceSetup()` calls `keypadAddGpioKey(kp, SDLK_ESCAPE, 0, false)`, registering the power key on GPIO0 as active low and thereby driving the line high (released). The port omitted it, so GPIO0 stayed low and the firmware waited for the power key to be released.
- Fix: `sonyTG50Initialize()` sets GPIO0 high (released).
- Verified natively (Linux, SDL2): the TG50 ROM now boots to the **Setup 1 of 4** screen, and controller keypad accesses continue as in the original.
- Done later: the host power key now also drives GPIO0 low while pressed (see "Sleep and resume").


## Setup blocker fixed: pen calibration (E2 touch translation patch)

- Symptom: in Setup's pen calibration, tapping the second (bottom-right) target always restarted calibration at the first target.
- Comparison: the original uARM TG50 build, on the same ROM and with identical AD7873 ADC values (`3200 - 10 * x/y`), passes calibration.
- Cause: `registerPatches()` installs `PATCH_FEATURE_TOUCH_TRANSLATION` for every PXA device. Its `PenRawToScreen`/`PenScreenToRaw` head patches replace the HAL calls with the fixed Tungsten E2 (WM9712L) mapping `280 + 10x`, `210 + 7y`, which does not match the TG50 digitizer.
- Fix: `SocPXA` registers the patches without `PATCH_FEATURE_TOUCH_TRANSLATION` in TG50 mode, so Palm OS uses its own calibration. E2 behavior is unchanged.
- Verified natively (Linux, SDL2): calibration (3 targets), Setup 3/4 Next and Setup 4/4 Done complete, and the CLIÉ Launcher responds to taps.
- Note: the TG50 has a hardware QWERTY keyboard and no Graffiti silkscreen. The port still uses the E2 `mode_320x320` display configuration (320x320 plus a 120 px silkscreen area), so the drawn E2 silkscreen does nothing on TG50.
- Possible follow-up: a TG50-specific translation patch (inverse of `3200 - 10 * x/y`, with the TG50 X/Y cross-wiring verified first) would make touch independent of calibration, like on the E2.

## QWERTY keyboard wired to host keys

- Diagnosis: on original uARM with the same ROM, typing in Memo Pad works, so the ROM's keyboard driver reads the Sony controller matrix directly. In this port the matrix model was present, but host keys never reached it: the SDL handler mapped only F1–F4, arrows, Return (jog select) and Esc, and the common `KeyId` had 10 values.
- Change: `keys.h` adds keyboard key IDs after `keyIdPower` (existing values unchanged). `SdlEventHandler` maps letters, Space, Return, Backspace, Shift, Ctrl, Tab, Caps Lock, `,` `.` `-`, F7 (Blue), F9 (Red), Home, PageUp/PageDown, End (jog back), F11 (jog select), F5 (voice) and `` ` `` (Graffiti) **only in TG50 mode**; other devices keep the old mapping. `sonyTG50HostKey()` translates the IDs to `kMatrix` keys. E2/PV keypads ignore unknown IDs.
- Verified natively: lower/upper case, Shift, Blue+q/w/e = `123`, Red layer, comma/period, Backspace, Enter, Home back to the launcher.
- Open: PC digit keys are not mapped. Fast-typing drops are fixed: the event queue applies one key event per 1/30 s tick, so a quick press was down for only ~33 ms while the TG50 keyboard scan needs ~60 ms; key-ups are now delayed until the key has been down for 3 ticks (~100 ms). Zero-hold a–z: 5/26 before, 26/26 after.

## Memory Stick extended-register diagnostic comparison (KOSPI)

A reported KOSPI 2.3.0 TG50 test accessed the additional MMIO offsets `0x10c0` (16-bit read), `0x10d0` (16-bit read/write), `0x10e0` (16-bit write), and `0x10f0`/`0x10f8` (32-bit reads). These are **reported observations**, not hardware documentation. The original controller maps only `0x400` bytes.

The controller now reserves `0x1400` bytes, while **the default build rejects all accesses outside the original `0x400` region**. Two mutually exclusive compile-time experiments admit only the five observed extended addresses and access widths:

- `-DTG50_MS_EXT_ALIAS`: translate observed extra offsets by `-0xe00` and use existing register state.
- `-DTG50_MS_EXT_ZERO`: return zero for observed reads and ignore observed writes.
- Optional `-DTG50_MS_TRACE` prints zero-mode accesses; diagnostics are off by default.

Build each variant separately; compare clean boot, KOSPI installation and reset, memo text persistence, and boot without KOSPI. If both modes boot, this alone does **not** prove that the hardware uses aliases. The observed null-address writes under KOSPI require a separate PC-level investigation. No actual A/B boot run is claimed in this commit.

## Sleep and resume (2026-10-23)

Previously the power key reached only the controller matrix, and driving GPIO0 ended in a Fatal Exception. Five causes were found with a diagnostic build:

| # | Problem | Cause | Fix |
| --- | --- | --- | --- |
| 1 | No sleep state | `PWRMODE 3` (PXA sleep) reset the CPU at once ("pretend we woke up") | Deep sleep for TG50/NX70V only (`pxaPwrClkSetDeepSleepEnabled`): the CPU stays halted, interrupts do not wake it, and every 1/30 s `pxaPwrClkCheckWake()` compares GPIO0–15 with `PWER`/`PRER`/`PFER`; on a match it sets `PEDR`, `RCSR` bit 2 and resets the CPU. E2 keeps the old behavior |
| 2 | Woke and slept again | The ROM sleeps with `PWER` = GPIO0 rising (power-key release), then with `0x608B` (GPIO0/1/3/7/13/14 falling). A GPIO0 wake only counts the press (`0x200323C8`) and sleeps again; a full wake needs GPIO13 | The power key also pulls GPIO13 low; a press while asleep is kept off GPIO0 but still reaches the matrix (the ROM turns the screen on) |
| 3 | Fatal Exception after wake (`R of 4 bytes to 0x02200000`) | On resume the ROM bit-bangs a 64-word serial EEPROM on GPIO4–6 (`0x20033268`); data out GPIO5 read 0, so the record walker (`0x20033A00`) never met its `0xFFFF` end and ran off memory | GPIO5 idles high (erased EEPROM) |
| 4 | Keyboard dead after wake | The boot code writes the controller reset command (`0x210` = `0xFF`) on resume; the model reset all port/IRQ configuration, which the ROM does not reprogram on resume | `tg50ucIgnoreNextReset()` after each wake |
| 5 | Device pointer missing | `SocPeriphs.soc` was never set, so the device could not see the sleep state | `soc_PXA.cpp` sets `sp.soc = this` |

Verified: three sleep/wake cycles (screen off, then back to the Launcher), hard buttons and fast typing after resume, Setup and pen calibration unchanged.

