#ifndef _DEVICE_TYPE5_H_
#define _DEVICE_TYPE5_H_

enum DeviceType5 {
    deviceTypeE2 = 0,
    deviceTypeFrankenE2 = 1,
    deviceTypePV = 2,
    deviceTypeSonyTG50 = 3,  // staged: native --tg50 only
    deviceTypeSonyNX70V = 4,  // experimental: native --nx70v only, shares the TG50 peripherals
    deviceTypeInvalid = -1
};

#endif  // _DEVICE_TYPE5_H_
