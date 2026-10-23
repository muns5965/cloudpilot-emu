// Sony CLIÉ PEG-TG50 and PEG-NX70V hardware definitions: shared peripheral register addresses,
// GPIO assignments and the keyboard matrix used by the uARM device models.
// Source: https://github.com/uARM-Palm/uARM/blob/master/deviceSonyTG50.c
//         https://github.com/uARM-Palm/uARM/blob/master/mmiodev_TG50uc.c
#ifndef _SONY_TG50_HARDWARE_H_
#define _SONY_TG50_HARDWARE_H_

#include <cstdint>

namespace sony_tg50 {
constexpr uint32_t kUcBase = 0x08000000ul;
constexpr uint32_t kUcSize = 0x0220ul;
constexpr uint32_t kMemoryStickBase = 0x14000000ul;
constexpr uint32_t kUnknownSramBase = 0xac000000ul;
constexpr uint32_t kUnknownSramSize = 1024ul;

// PEG-NX70V differences found by tracing its ROM on the TG50 peripheral set.
// CF slot controller (Sony CfSlot Library), 8-bit registers with a 4-byte stride.
constexpr uint32_t kNx70vCfSlotBase = 0x10000000ul;
constexpr uint32_t kNx70vCfSlotSize = 0x10000ul;
constexpr uint32_t kNx70vCfSlotStatus = 0x10;      // card status register
constexpr uint8_t kNx70vCfSlotNoCard = 0xC0;       // card-detect bits: no card inserted
// Sony controller port A pin 4: TG50 reads high as "hold off", NX70V reads high as "hold on".
constexpr unsigned kHoldSwitchPort = 0;
constexpr unsigned kHoldSwitchPin = 4;
// uARM deviceSonyTG50.c keeps GPIO13 high (inactive) and drives GPIO14
// through the Sony microcontroller. GPIO13 is a sleep wake source (PWER/PFER);
// this port pulls it low while the power key is pressed so the ROM wakes up.
constexpr unsigned kKeyRelatedIdleGpio = 13;
constexpr unsigned kUcIrqGpio = 14;
constexpr unsigned kTouchIrqGpio = 9;
constexpr unsigned kKeyboardRows = 8;
constexpr unsigned kKeyboardColumns = 8;
// Original uARM: 17 MiB, mirrored after RAM (firmware probes beyond 32 MiB).
constexpr uint32_t kRamSize = 17ul << 20;
// PXA25x SoC revision used in original uARM's TG50 device implementation.
constexpr unsigned kSocRevision = 0;

// Physical keyboard matrix, following the original uARM deviceSonyTG50.c.
// Values are semantic keys, deliberately independent of SDL/browser key codes.
enum class Key : uint8_t {
    none, power, hard1, hard2, hard3, hard4, voice, pageUp, pageDown,
    home, jogBack, jogSelect, graffiti, caps, tab, shift, control,
    blue, red, comma, period, up, down, space, underscore,
    left, right, enter, backspace,
    q, w, e, r, t, h, y, u, i, o, p, j,
    a, s, d, f, g, k, z, x, c, v, b, l, n, m
};

constexpr Key kMatrix[kKeyboardRows][kKeyboardColumns] = {
    {Key::power, Key::hard1, Key::hard2, Key::none, Key::hard3, Key::none, Key::hard4, Key::voice},
    {Key::pageUp, Key::pageDown, Key::home, Key::jogBack, Key::none, Key::jogSelect, Key::graffiti, Key::none},
    {Key::q, Key::caps, Key::w, Key::e, Key::r, Key::t, Key::h, Key::none},
    {Key::y, Key::tab, Key::u, Key::i, Key::o, Key::p, Key::j, Key::none},
    {Key::a, Key::shift, Key::s, Key::d, Key::f, Key::g, Key::k, Key::none},
    {Key::z, Key::control, Key::x, Key::c, Key::v, Key::b, Key::l, Key::none},
    {Key::n, Key::blue, Key::m, Key::comma, Key::period, Key::up, Key::down, Key::none},
    {Key::space, Key::red, Key::underscore, Key::left, Key::right, Key::enter, Key::backspace, Key::none},
};
}  // namespace sony_tg50

#endif  // _SONY_TG50_HARDWARE_H_
