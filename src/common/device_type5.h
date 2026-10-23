#ifndef _DEVICE_TYPE5_H_
#define _DEVICE_TYPE5_H_

enum DeviceType5 {
    deviceTypeE2 = 0,
    deviceTypeFrankenE2 = 1,
    deviceTypePV = 2,
    deviceTypeSonyTG50 = 3,  // native --tg50; web/native auto-detected (sony/vrna)
    deviceTypeSonyNX70V = 4,  // experimental, shares the TG50 peripherals; needs a TG50 boot ROM
    deviceTypeInvalid = -1
};

#endif  // _DEVICE_TYPE5_H_
