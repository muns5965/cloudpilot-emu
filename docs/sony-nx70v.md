# Sony CLIÉ PEG-NX70V (experimental)

The PEG-NX70V (Palm OS 5.0, PXA250, 320×480) runs on the TG50 peripheral set from [docs/sony-tg50.md](sony-tg50.md).
It is not part of uARM: the differences below were found by running the NX70V ROM on the TG50 model. No ROM or Sony
firmware is included.

## ROM and boot ROM

Known NX70V NOR dumps start with 128 KiB of device data (including a per-device record) instead of the small boot ROM;
the Palm OS ROM header is at `0x20000` (company `sony`, HAL `crdb`). The boot ROM of a TG50 NOR image (its first
`0x18000` bytes) boots it. The combined image only exists in memory (native) or in the browser's own storage (web);
the device data area should never be published.

## Differences from the TG50

| Area | NX70V |
| --- | --- |
| Device type | `deviceTypeSonyNX70V`, detected from `sony`/`crdb`; 17 MiB RAM; display `mode_320x480` (the ROM draws its own Graffiti area) |
| CF slot | Sony CF slot controller at `0x10000000` (CS4): a minimal model that always reports "no card"; without it the DAL faults and the ROM reboots in a loop |
| HOLD switch | Sony controller port A pin 4 has the opposite polarity |
| Touch | 480 lines share the TG50 raw range and both axes are reversed |

## Running

Native: `./src/uarm/cp-uarm --nx70v --boot-rom <TG50 NOR image> <NX70V NOR image>`. Sessions keep the boot ROM, so a
saved session starts without `--boot-rom`. Keys are the same as on the TG50.

Web: create a session with the NX70V ROM; the session settings ask for a TG50 ROM as boot ROM.

## Status

| Tested (native) | Result |
| --- | --- |
| Boot, Setup, pen calibration, Launcher | works |
| Typing (incl. digit keys), hard buttons | works |
| Sleep and resume with the power key | works |
| Session save and restore | works |

Not done or not tested: audio output, CF and Memory Stick media, the NX70V camera, jog dial and swivel; browser runtime
(implemented, not rebuilt on this branch); no comparison with a physical NX70V.
