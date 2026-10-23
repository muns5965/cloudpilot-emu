// Sony PEG-TG50 AD7873 touchscreen model, ported from uARM-Palm/uARM.
// Not yet instantiated: TG50 device registration is pending.
#ifndef _SSPDEV_AD7873_H_
#define _SSPDEV_AD7873_H_
#include <cstddef>
#include <cstdint>
#include "pxa_SSP.h"
#include "pxa_GPIO.h"
struct Ad7873;
Ad7873* ad7873Init(PxaSsp* ssp, PxaGpio* gpio, int8_t penIntGpio);
void ad7873Periodic(Ad7873* ad);
void ad7873PenInput(Ad7873* ad, int16_t x, int16_t y);
void ad7873SetVbatt(Ad7873* ad, uint16_t mV);
void ad7873SetVaux(Ad7873* ad, uint16_t mV);
// Savestate support: fixed-size blob of the model's mutable state.
size_t ad7873StateSize();
void ad7873GetState(Ad7873* ad, void* out);
void ad7873SetState(Ad7873* ad, const void* in);
#endif
