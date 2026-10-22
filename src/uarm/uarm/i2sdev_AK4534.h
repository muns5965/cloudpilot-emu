// Sony PEG-TG50 AK4534 audio codec (I2C register model), from uARM-Palm/uARM.
#ifndef _I2SDEV_AK4534_H_
#define _I2SDEV_AK4534_H_
#include "pxa_I2C.h"
#include "pxa_I2S.h"
#include "pxa_GPIO.h"
struct AK4534;
AK4534* ak4534Init(PxaI2c* i2c, PxaI2s* i2s, PxaGpio* gpio);
#endif
