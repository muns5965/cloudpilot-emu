#ifndef _TIMEUTIL_H_
#define _TIMEUTIL_H_

#include <cstdint>

// RTC time: seconds since 1904, host local time, plus the test offset below.
uint64_t palmEpochSeconds();

// Test-only offset added to palmEpochSeconds() by the native "time-skip" command (forward only,
// while the device is in deep sleep). 0 unless that command was used.
uint64_t palmEpochTestOffset();
void palmEpochSetTestOffset(uint64_t seconds);

#endif  // _TIMEUTIL_H_
