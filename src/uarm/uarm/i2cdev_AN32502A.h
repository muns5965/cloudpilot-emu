//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr
// Sony PEG-TG50 AN32502A power management I2C model, ported from uARM.

#ifndef _I2CDEV_AN32502A_H_
#define _I2CDEV_AN32502A_H_
#include "pxa_I2C.h"
struct An32502A;
An32502A* an32502aInit(PxaI2c* i2c);
#include <cstddef>
// Savestate support: fixed-size blob of the model's mutable state.
size_t an32502aStateSize();
void an32502aGetState(An32502A* an, void* out);
void an32502aSetState(An32502A* an, const void* in);

#endif
