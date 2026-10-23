// Partial PEG-TG50 peripheral initialization adapted from uARM's deviceSonyTG50.c.
// Not yet connected to CloudpilotEmu's E2-only device factory or ROM loading.
#include "deviceSonyTG50.h"
#include "i2cdev_AN32502A.h"
#include "i2sdev_AK4534.h"
#include "mmiodev_TG50uc.h"
#include "mmiodev_MemoryStickController.h"
#include "sspdev_AD7873.h"
#include <cstdlib>

struct SonyTG50 {
    TG50uc* controller;
    Ad7873* touch;
    An32502A* power;
    AK4534* codec;
    MemoryStickController* stick;
    PxaGpio* gpio;
    SoC* soc;
};

SonyTG50* sonyTG50Initialize(SocPeriphs* sp) {
    if (!sp || !sp->mem || !sp->gpio || !sp->ssp || !sp->i2c || !sp->i2s)
        return nullptr;

    // Semantic IDs intentionally avoid SDL keycodes; ID 0 is reserved for unmapped keys.
    uint32_t keyMap[64] = {};
    for (unsigned row = 0; row < sony_tg50::kKeyboardRows; ++row)
        for (unsigned col = 0; col < sony_tg50::kKeyboardColumns; ++col)
            keyMap[row * 8 + col] =
                static_cast<uint32_t>(sony_tg50::kMatrix[row][col]);

    auto* dev = static_cast<SonyTG50*>(std::calloc(1, sizeof(SonyTG50)));
    if (!dev) return nullptr;
    dev->gpio = sp->gpio;
    dev->soc = sp->soc;
    // Registration of emulated peripherals is not currently reversible.
    // Call only once per initialized SoC, after TG50 is selected by a future factory.
    dev->touch = ad7873Init(sp->ssp, sp->gpio, sony_tg50::kTouchIrqGpio);
    dev->codec = ak4534Init(sp->i2c, sp->i2s, sp->gpio);
    dev->power = an32502aInit(sp->i2c);
    // Preserve separate idle-high GPIO13 and controller-driven GPIO14.
    pxaGpioSetState(sp->gpio, sony_tg50::kKeyRelatedIdleGpio, true);
    pxaGpioSetState(sp->gpio, sony_tg50::kUcIrqGpio, true);
    dev->controller = tg50ucInit(sp->mem, sp->gpio, sony_tg50::kUcIrqGpio, keyMap);
    dev->stick = msCtrlrInit(sp->mem, sony_tg50::kMemoryStickBase);
    if (!dev->touch || !dev->codec || !dev->power || !dev->controller || !dev->stick) {
        // MMIO/SSP/I2C callbacks may already reference allocated clients.
        // Do not free the clients or the device here until rollback support exists.
        return nullptr;
    }
    // Power key is wired to GPIO0, active low (uARM: keypadAddGpioKey(kp, SDLK_ESCAPE, 0, false),
    // which leaves the line high). Left low, the boot code at 0x20036DBC spins waiting for release.
    pxaGpioSetState(sp->gpio, 0, true);   // power key released (active low)
    pxaGpioSetState(sp->gpio, 1, true);   // reset switch (active low)
    pxaGpioSetState(sp->gpio, 3, true);   // cradle switch (not pressed)
    // GPIO4-6 bit-bang a 64-word serial EEPROM read on sleep resume. Data out (GPIO5) idles high,
    // so the ROM reads an erased EEPROM (0xFFFF) instead of zeros that it walks past its records.
    pxaGpioSetState(sp->gpio, 5, true);
    pxaGpioSetState(sp->gpio, 7, true);   // Memory Stick absent
    pxaGpioSetState(sp->gpio, 10, true);  // Memory Stick interrupt inactive
    pxaGpioSetState(sp->gpio, sony_tg50::kKeyRelatedIdleGpio, true); // GPIO13 idle high; the power key pulls it low
    // GPIO14 is owned by TG50uc after initialization; do not override it here.
    ad7873SetVbatt(dev->touch, 4200);
    ad7873Periodic(dev->touch);
    sp->dbgUart = sp->uarts[2]; // Mirrors original uARM TG50 setup.
    return dev;
}

void sonyTG50Periodic(SonyTG50* dev) {
    if (dev) ad7873Periodic(dev->touch);
}

void sonyTG50Touch(SonyTG50* dev, int x, int y) {
    if (!dev) return;
    ad7873PenInput(dev->touch,
                   x >= 0 && y >= 0 ? static_cast<int16_t>(3200 - 10 * x) : -1,
                   x >= 0 && y >= 0 ? static_cast<int16_t>(3200 - 10 * y) : -1);
    ad7873Periodic(dev->touch);
}

void sonyTG50Key(SonyTG50* dev, sony_tg50::Key key, bool pressed) {
    if (dev && key != sony_tg50::Key::none)
        tg50ucSetKeyPressed(dev->controller, static_cast<uint32_t>(key), pressed);
}

void sonyTG50HostKey(SonyTG50* dev, KeyId key, bool pressed) {
    using K = sony_tg50::Key;
    K tg50 = K::none;
    switch (key) {
        case keyIdHard1: tg50 = K::hard1; break;
        case keyIdHard2: tg50 = K::hard2; break;
        case keyIdHard3: tg50 = K::hard3; break;
        case keyIdHard4: tg50 = K::hard4; break;
        case keyIdUp: tg50 = K::up; break;
        case keyIdDown: tg50 = K::down; break;
        case keyIdLeft: tg50 = K::left; break;
        case keyIdRight: tg50 = K::right; break;
        case keyIdSelect: tg50 = K::jogSelect; break;
        case keyIdPower: tg50 = K::power; break;
        case keyIdKbdSpace: tg50 = K::space; break;
        case keyIdKbdEnter: tg50 = K::enter; break;
        case keyIdKbdBackspace: tg50 = K::backspace; break;
        case keyIdKbdShift: tg50 = K::shift; break;
        case keyIdKbdCtrl: tg50 = K::control; break;
        case keyIdKbdTab: tg50 = K::tab; break;
        case keyIdKbdCaps: tg50 = K::caps; break;
        case keyIdKbdComma: tg50 = K::comma; break;
        case keyIdKbdPeriod: tg50 = K::period; break;
        case keyIdKbdUnderscore: tg50 = K::underscore; break;
        case keyIdKbdBlue: tg50 = K::blue; break;
        case keyIdKbdRed: tg50 = K::red; break;
        case keyIdHome: tg50 = K::home; break;
        case keyIdPageUp: tg50 = K::pageUp; break;
        case keyIdPageDown: tg50 = K::pageDown; break;
        case keyIdJogBack: tg50 = K::jogBack; break;
        case keyIdVoice: tg50 = K::voice; break;
        case keyIdGraffiti: tg50 = K::graffiti; break;
        default:
            if (key >= keyIdKbdA && key <= keyIdKbdZ) {
                static constexpr K kLetters[26] = {
                    K::a, K::b, K::c, K::d, K::e, K::f, K::g, K::h, K::i, K::j, K::k, K::l, K::m,
                    K::n, K::o, K::p, K::q, K::r, K::s, K::t, K::u, K::v, K::w, K::x, K::y, K::z};
                tg50 = kLetters[key - keyIdKbdA];
            }
            break;
    }
    if (key == keyIdPower) {
        // The power key drives GPIO0 (active low, as in original uARM) and pulls the controller's
        // key line GPIO13 low. The ROM sleeps on GPIO0; it only fully wakes on a GPIO13 edge.
        const bool asleep = dev->soc && dev->soc->IsInDeepSleep();
        pxaGpioSetState(dev->gpio, sony_tg50::kKeyRelatedIdleGpio, !pressed);

        // A press that wakes the device stays off GPIO0; otherwise the ROM treats it as a GPIO0
        // wake and goes back to sleep. It still reaches the matrix (the ROM turns the screen on).
        if (!(pressed && asleep)) pxaGpioSetState(dev->gpio, 0, !pressed);
    }
    sonyTG50Key(dev, tg50, pressed);
}

void sonyTG50SleepWakeup(SonyTG50* dev) {
    if (dev) tg50ucIgnoreNextReset(dev->controller);
}
