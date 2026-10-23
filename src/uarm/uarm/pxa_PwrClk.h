//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _PXA_PWR_CLK_H_
#define _PXA_PWR_CLK_H_

#include "CPU.h"
#include "mem.h"

class SoC;
struct PxaPwrClk;

struct PxaPwrClk* pxaPwrClkInit(struct ArmCpu* cpu, struct ArmMem* physMem, class SoC* soc,
                                bool isPXA270);

void pxaPwrClkReset(struct PxaPwrClk* pc);

// Deep sleep (PWRMODE 3). Disabled by default: the CPU is reset immediately ("pretend we woke up").
// When enabled, the CPU stays powered down until pxaPwrClkCheckWake() sees a PWER wake edge.
void pxaPwrClkSetDeepSleepEnabled(struct PxaPwrClk* pc, bool enabled);
bool pxaPwrClkInDeepSleep(struct PxaPwrClk* pc);
// Feed the current GPIO0-31 pin levels. Returns true if an enabled edge on GPIO0-15 woke the CPU.
bool pxaPwrClkCheckWake(struct PxaPwrClk* pc, uint32_t gpioLevels);

// Deep sleep savestate (separate chunk, written only when deep sleep is enabled).
template <typename T>
void pxaPwrClkSaveDeepSleep(struct PxaPwrClk* pc, T& savestate);
template <typename T>
void pxaPwrClkLoadDeepSleep(struct PxaPwrClk* pc, T& loader);

template <typename T>
void pxaPwrClkSave(struct PxaPwrClk* pc, T& savestate);

template <typename T>
void pxaPwrClkLoad(struct PxaPwrClk* pc, T& loader);

#endif
