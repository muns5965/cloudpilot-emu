// Sony TG50 AK4534 codec register control, adapted from
// https://github.com/uARM-Palm/uARM/blob/master/i2sdev_AK4534.c
// Only I2C register emulation is implemented; no PCM output path yet.
#include "i2sdev_AK4534.h"
#include <cstdlib>
#include <cstdint>

struct AK4534 {
    uint8_t regs[16];
    bool inTransaction, addrSeen, ourAddr, isRead, regAddrSeen;
    uint8_t reg;
};

static uint_fast8_t ak4534I2cAction(void* ctx, ActionI2C action, uint_fast8_t value) {
    auto* ak = static_cast<AK4534*>(ctx);
    switch (action) {
        case i2cStart:
            ak->inTransaction = true;
            ak->regAddrSeen = false;
            [[fallthrough]];
        case i2cRestart:
            ak->addrSeen = false;
            ak->ourAddr = false;
            return 0;
        case i2cStop:
            ak->inTransaction = false;
            return 0;
        case i2cTx:
            if (!ak->inTransaction) return 0;
            if (!ak->addrSeen) {
                ak->addrSeen = true;
                if ((value >> 1) != 0x10) return 0;
                ak->ourAddr = true;
                ak->isRead = (value & 1) != 0;
                return 1;
            }
            if (!ak->ourAddr || ak->isRead) return 0;
            if (!ak->regAddrSeen) {
                ak->regAddrSeen = true;
                ak->reg = static_cast<uint8_t>(value);
                return 1;
            }
            if (ak->reg >= sizeof(ak->regs)) return 0;
            ak->regs[ak->reg++] = static_cast<uint8_t>(value);
            return 1;
        case i2cRx:
            if (!ak->inTransaction || !ak->addrSeen || !ak->ourAddr || !ak->isRead)
                return 0;
            if (ak->reg >= sizeof(ak->regs)) return 0;
            return ak->regs[ak->reg++];
        default:
            return 0;
    }
}

AK4534* ak4534Init(PxaI2c* i2c, PxaI2s* /*i2s*/, PxaGpio* /*gpio*/) {
    if (!i2c) return nullptr;
    auto* ak = static_cast<AK4534*>(std::calloc(1, sizeof(AK4534)));
    if (!ak) return nullptr;
    ak->regs[1] = 0x80;
    ak->regs[3] = 0x03;
    ak->regs[4] = 0x02;
    ak->regs[6] = 0x11;
    ak->regs[7] = 0x01;
    ak->regs[9] = 0x40;
    ak->regs[10] = 0x36;
    ak->regs[11] = 0x10;
    if (!pxaI2cDeviceAdd(i2c, ak4534I2cAction, ak)) {
        std::free(ak);
        return nullptr;
    }
    return ak;
}
