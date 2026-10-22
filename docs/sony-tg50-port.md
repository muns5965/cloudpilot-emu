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
