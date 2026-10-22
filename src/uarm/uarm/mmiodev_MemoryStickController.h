// Sony TG50 Memory Stick controller register model, adapted from uARM-Palm/uARM.
#ifndef _MMIO_MEMORY_STICK_CONTROLLER_H_
#define _MMIO_MEMORY_STICK_CONTROLLER_H_
#include <cstdint>
#include "mem.h"
struct MemoryStickController;
MemoryStickController* msCtrlrInit(ArmMem* memory, uint32_t baseAddress);
#endif
