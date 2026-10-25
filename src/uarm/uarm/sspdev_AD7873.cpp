//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr
// Sony PEG-TG50 AD7873 touchscreen, ported from uARM sspdev_AD7873.c.
// SSP conversion is pipelined: each transfer returns the previous channel value.
// TG50 X/Y ADC wiring is reversed in the original hardware model.

#include "sspdev_AD7873.h"
#include <cstdlib>
#include <cstring>

struct Ad7873 {
    PxaGpio* gpio;
    int8_t penIntGpio;
    uint16_t prevVal;
    bool penDown;
    int16_t penX, penY;
    uint16_t vBattAdcVal, vAuxAdcVal;
};

static uint_fast16_t ad7873SspClient(void* userData, uint_fast8_t nBits, uint_fast16_t command) {
    auto* ad = static_cast<Ad7873*>(userData);
    if (nBits != 12) return 0;

    const uint16_t result = ad->prevVal;
    switch (command) {
        case 0x93: ad->prevVal = static_cast<uint16_t>(ad->penX); break;
        case 0xA6: ad->prevVal = ad->vBattAdcVal; break;
        case 0xB8: ad->prevVal = ad->penDown ? 0x07ff : 0; break;
        case 0xE6:
        case 0xE7: ad->prevVal = ad->vAuxAdcVal; break;
        case 0xD3:
        case 0xD0: ad->prevVal = static_cast<uint16_t>(ad->penY); break;
        default: ad->prevVal = 0; break;
    }
    return result;
}

Ad7873* ad7873Init(PxaSsp* ssp, PxaGpio* gpio, int8_t penIntGpio) {
    if (!ssp || !gpio) return nullptr;
    auto* ad = static_cast<Ad7873*>(std::calloc(1, sizeof(Ad7873)));
    if (!ad) return nullptr;
    ad->gpio = gpio;
    ad->penIntGpio = penIntGpio;
    if (!pxaSspAddClient(ssp, ad7873SspClient, ad)) {
        std::free(ad);
        return nullptr;
    }
    return ad;
}

void ad7873Periodic(Ad7873* ad) {
    if (ad && ad->penIntGpio >= 0)
        pxaGpioSetState(ad->gpio, static_cast<uint_fast8_t>(ad->penIntGpio), !ad->penDown);
}

void ad7873PenInput(Ad7873* ad, int16_t x, int16_t y) {
    if (!ad) return;
    ad->penDown = x >= 0 && y >= 0;
    ad->penX = ad->penDown ? y : -1;
    ad->penY = ad->penDown ? x : -1;
}

void ad7873SetVbatt(Ad7873* ad, uint16_t mV) {
    if (ad) ad->vBattAdcVal = static_cast<uint16_t>((static_cast<uint32_t>(mV) / 4) * 4096 / 2500);
}

void ad7873SetVaux(Ad7873* ad, uint16_t mV) {
    if (ad) ad->vAuxAdcVal = static_cast<uint16_t>((static_cast<uint32_t>(mV) * 4096) / 2500);
}

namespace {
struct Ad7873State {
    uint16_t prevVal, vBattAdcVal, vAuxAdcVal;
};
}  // namespace

size_t ad7873StateSize() { return sizeof(Ad7873State); }

void ad7873GetState(Ad7873* ad, void* out) {
    Ad7873State s{ad->prevVal, ad->vBattAdcVal, ad->vAuxAdcVal};
    std::memcpy(out, &s, sizeof(s));
}

void ad7873SetState(Ad7873* ad, const void* in) {
    Ad7873State s;
    std::memcpy(&s, in, sizeof(s));
    ad->prevVal = s.prevVal;
    ad->vBattAdcVal = s.vBattAdcVal;
    ad->vAuxAdcVal = s.vAuxAdcVal;
    ad7873PenInput(ad, -1, -1);  // the host pen is up after a restore
    ad7873Periodic(ad);
}
