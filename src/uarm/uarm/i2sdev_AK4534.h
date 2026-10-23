// Sony PEG-TG50 AK4534 audio codec (I2C register model), from uARM-Palm/uARM.
#ifndef _I2SDEV_AK4534_H_
#define _I2SDEV_AK4534_H_
#include "pxa_I2C.h"
#include "pxa_I2S.h"
#include "pxa_GPIO.h"
struct AK4534;
AK4534* ak4534Init(PxaI2c* i2c, PxaI2s* i2s, PxaGpio* gpio);
#include <cstddef>
// Savestate support: fixed-size blob of the model's mutable state.
size_t ak4534StateSize();
void ak4534GetState(AK4534* ak, void* out);
void ak4534SetState(AK4534* ak, const void* in);

#endif
