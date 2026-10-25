# Sony CLIÉ PEG-TG50 (experimental)

CloudpilotEmu can run the Sony CLIÉ PEG-TG50 (Palm OS 5.0, PXA250) with the uARM engine, natively and in the browser.
No ROM or Sony firmware is included; you need a dump of your own device's NOR ROM.

## Origin

The hardware model comes from Dmitry Grinberg's [uARM](https://github.com/uARM-Palm/uARM), which ships a TG50 device
(`deviceSonyTG50.c`). Its peripherals were ported to CloudpilotEmu's C++ uARM and keep the uARM copyright header:

| CloudpilotEmu file | uARM source | Part |
| --- | --- | --- |
| `deviceSonyTG50.cpp/.h`, `sony_tg50_hardware.h` | `deviceSonyTG50.c` | peripheral setup, addresses, GPIOs |
| `mmiodev_TG50uc.cpp/.h` | `mmiodev_TG50uc.c` | Sony embedded controller, 8×8 keyboard matrix |
| `sspdev_AD7873.cpp/.h` | `sspdev_AD7873.c` | touchscreen |
| `i2cdev_AN32502A.cpp/.h` | `i2cdev_AN32502A.c` | power management (I2C registers) |
| `i2sdev_AK4534.cpp/.h` | `i2sdev_AK4534.c` | audio codec (I2C registers only) |
| `mmiodev_MemoryStickController.cpp/.h` | `mmiodev_MemoryStickController.c` | Memory Stick controller (registers only) |

Changes made while integrating:

- Device type `deviceTypeSonyTG50`; ROMs are identified from the header (company `sony`, HAL `vrna`), 17 MiB RAM.
- Boot: GPIO13/14 idle levels and GPIO0 (power key) released at start; the E2 touch-translation patch is not applied, so
  Palm OS calibrates the pen itself.
- Controller edge detect: registers `0x80`–`0x8c` are rising and `0xa0`–`0xac` falling (the other way round in uARM),
  so the first key press after boot is seen.
- Deep sleep (TG50 only): `PWRMODE 3` halts the CPU until a `PWER`/`PRER`/`PFER` GPIO wakes it; the power key also pulls
  GPIO13; GPIO5 idles high (serial EEPROM read on resume); the controller ignores the reset command written on resume.
- Savestates: one `sonyTG50` chunk for the peripherals and a `pxaDeepSleep` chunk; E2 sessions are unchanged.
- Keys: new `KeyId` values for the QWERTY keyboard and TG50 buttons; keys are held for at least 3 event ticks
  (the ROM scans the matrix slowly). The power key is held for at least ~330 ms.
- Web: device `PEG-TG50` (320×320, no silkscreen), PC keyboard mapping and an on-screen keyboard below the hard buttons
  (can be hidden; includes the power key).
- PC digit keys send Blue + Q…P (as on the device), natively and on the web.
- An armed Date Book alarm wakes the device from sleep: shortly before the alarm the power key is pressed, so the
  device is awake when the alarm fires. In deep sleep the RTC alarm is compared only after the wake (the ROM
  clears a pending alarm on resume), so an alarm that passed while asleep, e.g. in a session restored later, is
  still shown once; an alarm in the past that already fired does not wake the device.
- The native window shows the screen only (no Graffiti area).
- The [PEG-NX70V](sony-nx70v.md) uses the same peripherals.

## Running

Native: `./src/uarm/cp-uarm <rom>` (TG50 ROMs are detected; `--tg50` forces the mode). Keys: letters, Space, Enter,
Backspace, Shift, Ctrl, Tab, Caps, `,` `.` `-`, digits (Blue + Q…P), Blue = F7, Red = F9, Home, PageUp/Down, jog back = End, jog select = F11,
voice = F5, Graffiti = `` ` ``, power = Esc.

Web: import the ROM; the device is detected as PEG-TG50.

## Status

| Tested | Result |
| --- | --- |
| Boot, Setup 1–4, pen calibration, CLIÉ Launcher (native) | works |
| Touch, typing in Memo Pad, hard buttons (native) | works |
| Sleep and resume with the power key (native) | works |
| Session save and restore (native) | works |
| Digit keys; Date Book alarm and Snooze wake the sleeping device; restored after the alarm time (native) | works |
| Browser: ROM import, boot, touch, hard buttons | works |

Not done or not tested:

- Audio output (codec registers only) and Memory Stick media.
- Browser PC keyboard, on-screen keyboard and session save/restore: implemented, not hand-tested.
- No comparison with a physical TG50.
