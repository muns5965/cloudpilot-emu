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
| Touch | `sspdev_AD7873.c/.h` | Port SSP device, stylus coordinates and GPIO 9 IRQ |
| Power management | `i2cdev_AN32502A.c/.h` | Port I2C responses needed by ROM boot |
| Audio | `i2sdev_AK4534.c/.h` | Port or establish which initialization accesses need stubs |
| Memory Stick | `mmiodev_MemoryStickController.c/.h` | Port required register behavior; removable media can follow later |
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
