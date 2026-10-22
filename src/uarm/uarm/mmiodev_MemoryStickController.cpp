// Sony TG50 Memory Stick register-only stub.
// Source: https://github.com/uARM-Palm/uARM/blob/master/mmiodev_MemoryStickController.c
// No card/media emulation is implemented.
#include "mmiodev_MemoryStickController.h"
#include <cstdlib>
#include <cstdint>
#include <cstring>
struct MemoryStickController {
    uint16_t systemReg;
    uint16_t reg0, reg40, reg50, reg2c0, reg2d0, reg2e0, reg2f0;
    uint16_t status180, status188;
    uint32_t replyLo2f0, replyHi2f8;
};
static bool access(void* ctx, uint32_t address, uint_fast8_t size, bool write, void* buf) {
    auto* m = static_cast<MemoryStickController*>(ctx);
    uint32_t offset = address & 0x3ffu;
    if (!write && size == 4 && (offset == 0x2f0 || offset == 0x2f8)) {
        const uint32_t val = offset == 0x2f0 ? m->replyLo2f0 : m->replyHi2f8;
        std::memcpy(buf, &val, sizeof(val));
        return true;
    }
    if (size != 2) return false;
    uint16_t val = 0;
    if (write) std::memcpy(&val, buf, sizeof(val));
    switch (offset) {
        case 0x00: if (write) m->reg0 = val; else val = m->reg0; break;
        case 0x08: if (write) m->systemReg = (m->systemReg & 0x8a00) | (val & 0x75ff); else val = m->systemReg; break;
        case 0x40: if (write) m->reg40 = val; else val = m->reg40; break;
        case 0x50: if (write) m->reg50 = val; else val = m->reg50; break;
        case 0x180: if (write) m->status180 &= static_cast<uint16_t>(~val); else val = m->status180; break;
        case 0x188: if (write) m->status188 &= static_cast<uint16_t>(~val); else val = m->status188; break;
        case 0x2c0: if (write) m->reg2c0 = val; else val = m->reg2c0; break;
        case 0x2d0: if (write) m->reg2d0 = val & static_cast<uint16_t>(~1u); else val = m->reg2d0; break;
        case 0x2e0: if (write) m->reg2e0 = val; else val = m->reg2e0; break;
        case 0x2f0: if (write) m->reg2f0 = val; else val = m->reg2f0; break;
        default: return false;
    }
    if (!write) std::memcpy(buf, &val, sizeof(val));
    return true;
}
MemoryStickController* msCtrlrInit(ArmMem* mem, uint32_t base) {
    if (!mem) return nullptr;
    auto* m = static_cast<MemoryStickController*>(std::calloc(1, sizeof(MemoryStickController)));
    if (!m) return nullptr;
    m->systemReg = 0x4455;
    if (!memRegionAdd(mem, base, 0x400, access, m)) {
        std::free(m);
        return nullptr;
    }
    return m;
}
