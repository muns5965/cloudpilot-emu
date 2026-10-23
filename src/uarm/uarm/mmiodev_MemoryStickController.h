// Sony TG50 Memory Stick controller register model, adapted from uARM-Palm/uARM.
#ifndef _MMIO_MEMORY_STICK_CONTROLLER_H_
#define _MMIO_MEMORY_STICK_CONTROLLER_H_
#include <cstdint>
#include "mem.h"
struct MemoryStickController;
MemoryStickController* msCtrlrInit(ArmMem* memory, uint32_t baseAddress);
#include <cstddef>
// Savestate support: fixed-size blob of the model's mutable state.
size_t msCtrlrStateSize();
void msCtrlrGetState(MemoryStickController* m, void* out);
void msCtrlrSetState(MemoryStickController* m, const void* in);

#endif
