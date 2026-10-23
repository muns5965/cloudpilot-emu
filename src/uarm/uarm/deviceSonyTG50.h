// Sony PEG-TG50 staged device peripheral assembly.
// This does not register a DeviceType5 or replace the E2 device factory.
#ifndef _DEVICE_SONY_TG50_H_
#define _DEVICE_SONY_TG50_H_
#include <cstdint>
#include "device.h"
#include "sony_tg50_hardware.h"

struct SonyTG50;
SonyTG50* sonyTG50Initialize(SocPeriphs* peripherals);
void sonyTG50Periodic(SonyTG50* device);
void sonyTG50Touch(SonyTG50* device, int x, int y);
void sonyTG50SleepWakeup(SonyTG50* device);
void sonyTG50Key(SonyTG50* device, sony_tg50::Key key, bool pressed);
// Maps the host's existing generic hardware key identifiers into TG50 matrix keys.
void sonyTG50HostKey(SonyTG50* device, KeyId key, bool pressed);
#endif
