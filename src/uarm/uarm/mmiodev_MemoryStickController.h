//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr
// Sony PEG-TG50 Memory Stick controller register model, ported from uARM.

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
