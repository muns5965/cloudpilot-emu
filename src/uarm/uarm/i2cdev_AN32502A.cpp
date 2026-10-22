// Sony CLIÉ TG50 AN32502A power management I2C model.
// Adapted from https://github.com/uARM-Palm/uARM/blob/master/i2cdev_AN32502A.c
// Initial port only: not yet instantiated by a TG50 device.
#include "i2cdev_AN32502A.h"

#include <cstdlib>
#include <cstdint>

struct An32502A {
    bool inTransaction;
    bool addrSeen;
    bool ourAddr;
    bool isRead;
    bool regAddrSeen;
    uint8_t reg;
};

static bool an32502aWriteRegister(uint8_t reg, uint8_t /*value*/) {
    // Original uARM model accepts writes to registers 0..2 without storing them.
    return reg <= 2;
}

static uint8_t an32502aReadRegister(uint8_t /*reg*/) {
    // Original uARM model returns zero; real read behavior remains undocumented.
    return 0;
}

static uint_fast8_t an32502aI2cAction(void* context, ActionI2C action, uint_fast8_t value) {
    auto* an = static_cast<An32502A*>(context);
    switch (action) {
        case i2cStart:
            an->inTransaction = true;
            an->regAddrSeen = false;
            [[fallthrough]];
        case i2cRestart:
            an->addrSeen = false;
            an->ourAddr = false;
            return 0;
        case i2cStop:
            an->inTransaction = false;
            return 0;
        case i2cTx:
            if (!an->inTransaction) return 0;
            if (!an->addrSeen) {
                an->addrSeen = true;
                if ((value >> 1) != 0x73) return 0;
                an->ourAddr = true;
                an->isRead = (value & 1) != 0;
                return 1;
            }
            if (!an->ourAddr || an->isRead) return 0;
            if (!an->regAddrSeen) {
                an->regAddrSeen = true;
                an->reg = static_cast<uint8_t>(value);
                return 1;
            }
            return an32502aWriteRegister(an->reg++, static_cast<uint8_t>(value)) ? 1 : 0;
        case i2cRx:
            if (!an->inTransaction || !an->addrSeen || !an->ourAddr ||
                !an->isRead || !an->regAddrSeen)
                return 0;
            return an32502aReadRegister(an->reg++);
        default:
            return 0;
    }
}

An32502A* an32502aInit(PxaI2c* i2c) {
    if (!i2c) return nullptr;
    auto* an = static_cast<An32502A*>(std::calloc(1, sizeof(An32502A)));
    if (!an) return nullptr;
    if (!pxaI2cDeviceAdd(i2c, an32502aI2cAction, an)) {
        std::free(an);
        return nullptr;
    }
    return an;
}
