# Sony CLIÉ PEG-TG50 port (work in progress)

This branch is an **early hardware-port staging area**, not a bootable TG50 emulator. No device has been registered in `DeviceType5`, and no TG50 ROM is currently supported.

## Verified upstream foundations

- Original uARM: [deviceSonyTG50.c](https://github.com/uARM-Palm/uARM/blob/master/deviceSonyTG50.c), [mmiodev_TG50uc.c](https://github.com/uARM-Palm/uARM/blob/master/mmiodev_TG50uc.c), [Makefile](https://github.com/uARM-Palm/uARM/blob/master/Makefile).
- CloudpilotEmu: `src/uarm/uarm/devicePalmTungstenE2.cpp`, `src/common/device_type5.h`, `src/uarm/uarm/device_configuration.cpp`, `src/uarm/Makefile`.
- Original uARM ships a TG50 device model, though its standard Makefile has the TG50 configuration **commented out** and enables T3 by default. Upstream's support listing alone does not prove that current source/ROM boots in the Cloudpilot fork.
- The original TG50 code uses PXA25x, a Sony controller mapped at `0x08000000` (size `0x220`), AD7873 touch, AN32502A power, AK4534 audio, a Memory Stick controller, and an 8x8 keyboard matrix on the Sony controller's GPIO port C.
- Initial addresses and keyboard matrix are staged in `src/uarm/uarm/sony_tg50_hardware.h`. An initial C++ port of the Sony controller is now present in `src/uarm/uarm/mmiodev_TG50uc.cpp` and `.h` and listed in `src/uarm/Makefile`, but **compilation and hardware behavior have not been verified**. Key symbols are semantic; **SDL key constants must not be copied into the web emulator**.

## Remaining dependencies and blockers

| Area | Original uARM | CloudpilotEmu work required |
| --- | --- | --- |
| Sony controller / physical keys | `mmiodev_TG50uc.c/.h` | Initial MMIO and row/column keyboard scan port added; verify build, IRQ and matrix correctness, then map host key IDs |
| Touch | `sspdev_AD7873.c/.h` | Initial C++ SSP port added (`sspdev_AD7873.cpp/.h`); compile, 12-bit pipelined command response, stylus coordinates and GPIO 9 IRQ remain unverified |
| Power management | `i2cdev_AN32502A.c/.h` | Initial C++ I²C model staged in `i2cdev_AN32502A.cpp/.h`; build and startup behavior unverified |
| Audio | `i2sdev_AK4534.c/.h` | I²C register model port staged in `i2sdev_AK4534.cpp/.h`; no PCM output; compile and startup behavior unverified |
| Memory Stick | `mmiodev_MemoryStickController.c/.h` | Register-only C++ model staged; card data transfer not supported; build and startup unverified |
| Device boot / GPIO | `deviceSonyTG50.c` | Integrate with modern `deviceSetup(DeviceType5, ...)` and per-device state |
| ROM and memory | `deviceGetRomMemType`, `deviceGetRamSize` | Check actual TG50 ROM layout, NOR handling, RAM mirror and startup mapping |
| Web/native integration | N/A | Add device type/config, native selection, frame and input bridge; WASM later |
| Savestates | N/A | Add TG50 chunks **after** first native boot; existing E2 chunks are not interchangeable |

## Implementation checkpoints

1. **Upstream baseline:** independently build and boot upstream TG50 using a legitimately obtained TG50 ROM. Log whether the original code really boots and accepts touch/keys.
2. **Native device:** port dependencies to C++, add TG50 device type and correct PXA initialization; only then enable TG50 in the device factory. Existing E2 and PV selection must remain unchanged.
3. **Boot checkpoint:** verify ROM execution, LCD frame output, stylus, keyboard, power/reset, and minimal launcher use. Failures must be attributed to MMIO/ROM/CPU before modifying web UI.
4. **Browser checkpoint:** add TG50 to the web device selector and input adapter after native boot succeeds.
5. **Regression:** build existing E2/FrankenE2/PV targets and compare their startup behavior; implement serialization and extended peripherals separately.

## Scope / safety

No ROM files or proprietary Sony firmware are committed. Do not advertise TG50 as supported until ROM boot is observed. Hardware constants in the new header are not integrated yet; they should not change existing emulator behavior.

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

## Native TG50 ROM launch entrypoint (experimental)

The native executable now accepts `--tg50` explicitly:

```sh
./src/uarm/cp-uarm --tg50 /path/to/your/TG50-ROM.bin
```

This bypasses only the **device-type identification decision**; the image must still pass the existing Palm OS 5 NOR ROM parser. It selects `deviceTypeSonyTG50`, uses 17 MiB RAM by default, and rejects an explicit NAND image or a session-file input. Other devices still use their normal `RomInfo5::GetDeviceType()` identification path. This is not auto-detection and is **not proof that a genuine TG50 image passes the existing parser**. Build and ROM boot remain unverified. ROM images are not included in the repository.
