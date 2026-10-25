//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _PXA_RTC_H_
#define _PXA_RTC_H_

#include "CPU.h"
#include "mem.h"
#include "pxa_IC.h"

struct PxaRtc;

struct PxaRtc* pxaRtcInit(struct ArmMem* physMem, struct PxaIc* ic);

// checkAlarm false: skip the alarm compare (and its IRQ update) for this tick.
void pxaRtcTick(struct PxaRtc* rtc, bool checkAlarm);

// The armed RTC alarm (RTSR.ALE) has not fired yet and fires within leadSeconds.
bool pxaRtcAlarmDue(struct PxaRtc* rtc, uint32_t leadSeconds);

template <typename T>
void pxaRtcSave(struct PxaRtc* rtc, T& savestate);

template <typename T>
void pxaRtcLoad(struct PxaRtc* rtc, T& loader);

#endif
