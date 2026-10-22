# CloudpilotEmu — Sony CLIÉ PEG-TG50 port

> **Work in progress · not bootable yet.** This is a development fork of [cloudpilot-emu/cloudpilot-emu](https://github.com/cloudpilot-emu/cloudpilot-emu), focused on running the **Sony CLIÉ PEG-TG50 (Palm OS 5)** in CloudpilotEmu. The existing emulator's Palm OS 1–4 and E2 implementations remain the upstream baseline; **TG50 is not yet a supported device**.

[**TG50 port plan and technical notes**](docs/sony-tg50-port.md) · [**Original CloudpilotEmu README**](doc/README-upstream.md) · [**Upstream repository**](https://github.com/cloudpilot-emu/cloudpilot-emu) · [**Original uARM TG50 code**](https://github.com/uARM-Palm/uARM/blob/master/deviceSonyTG50.c)

## Goal

Reuse CloudpilotEmu's existing ARM/PXA OS 5 engine and port Sony-specific hardware models from [uARM-Palm/uARM](https://github.com/uARM-Palm/uARM). First bring up a **native TG50 ROM boot**, then verify LCD, stylus and physical keyboard input, and only then add browser/WASM selection and display. A TG50 ROM must be obtained separately; no proprietary ROM is included.

## Current progress

| Component | Status | Code / next step |
| --- | --- | --- |
| Upstream uARM TG50 implementation reviewed | Done (source review) | `deviceSonyTG50.c`, `mmiodev_TG50uc.c` |
| TG50 hardware constants / 8×8 keyboard matrix | Added, not active | `src/uarm/uarm/sony_tg50_hardware.h` |
| Sony controller MMIO, GPIO and keyboard matrix | Initial C++ port, **unverified** | `src/uarm/uarm/mmiodev_TG50uc.cpp` and `.h` |
| Controller added to native/WASM build source list | Added; **build not verified** | `src/uarm/Makefile` |
| AD7873 touch / AN32502A power / AK4534 audio | Not ported | Port initialization-relevant peripherals |
| Memory Stick controller | Not ported | Determine minimum boot-time register behavior |
| TG50 device factory, ROM/RAM mapping | Not implemented | Add TG50 device class and configuration |
| Native compile / ROM boot | **Not verified / not attempted** | First validation milestone |
| Browser/WASM TG50 UI and input | Not implemented | After successful native boot |

**Important:** These commits stage the Sony controller but do not activate it in an emulated device. The current fork must **not** be presented as able to run a TG50 ROM.

## Development order

1. **Validate the controller port compiles** in the existing CloudpilotEmu build, without changing E2 or other supported devices. Address build errors and write a small MMIO/keyboard regression test.
2. **Port the remaining TG50 hardware** required for startup: touch (AD7873), power (AN32502A), audio initialization (AK4534), and Memory Stick register behavior.
3. **Register TG50 for native execution**: device type, ROM and RAM mapping, GPIO initialization, PXA25x revision and host keyboard mapping. Keep it out of the browser device list at this stage.
4. **Prove a real TG50 ROM boot** in the native emulator and test display, touch and QWERTY keyboard.
5. **Add browser/WASM support** after native boot, then handle savestates and optional peripherals.

Detailed file-level checklist: [docs/sony-tg50-port.md](docs/sony-tg50-port.md).

## Branch and upstream

Active development branch: `feature/sony-tg50`; `master` is the original fork baseline. This is an independent experimental fork, **not an official CloudpilotEmu release**.

The original project's emulator overview, device compatibility, existing build instructions and licensing information have been preserved in [doc/README-upstream.md](doc/README-upstream.md). Refer to that document for the underlying CloudpilotEmu project. Existing functionality and licensing obligations remain applicable to this fork.
