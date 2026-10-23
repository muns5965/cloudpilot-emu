// Sony TG50 AN32502A I2C power controller, adapted from uARM-Palm/uARM.
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
