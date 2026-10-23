// Ported interface for Sony PEG-TG50 embedded controller from uARM-Palm/uARM.
#ifndef _MMIO_TG50_UC_H_
#define _MMIO_TG50_UC_H_
#include <cstdint>
#include "pxa_GPIO.h"
#include "mem.h"
struct TG50uc;
TG50uc* tg50ucInit(ArmMem* physMem, PxaGpio* gpio, int8_t gpioChangeIrqNo, const uint32_t* keyMap);
void tg50ucGpioSetInState(TG50uc* uc, unsigned port, unsigned pin, bool hi);
void tg50ucSetKeyPressed(TG50uc* uc, uint32_t key, bool pressed);
// The boot code writes the reset command again on sleep resume; the controller keeps
// running through sleep, so the next reset command after a wake is ignored.
void tg50ucIgnoreNextReset(TG50uc* uc);
#endif
