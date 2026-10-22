// Sony TG50 Memory Stick register-only stub.
// Source: https://github.com/uARM-Palm/uARM/blob/master/mmiodev_MemoryStickController.c
// No card/media emulation is implemented.
#include "mmiodev_MemoryStickController.h"
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cstdio>
#if defined(TG50_MS_EXT_ALIAS) && defined(TG50_MS_EXT_ZERO)
#error "Select exactly one TG50 Memory Stick experiment"
#endif
struct MemoryStickController {
    uint16_t systemReg;
    uint16_t reg0, reg40, reg50, reg2c0, reg2d0, reg2e0, reg2f0;
    uint16_t status180, status188;
    uint32_t replyLo2f0, replyHi2f8;
};
static bool access(void* ctx, uint32_t address, uint_fast8_t size, bool write, void* buf) {
    auto* m = static_cast<MemoryStickController*>(ctx);
    // The original 0x400-byte block remains unchanged. Only five offsets
    // observed in TG50/KOSPI testing are admitted in the additional window.
    constexpr uint32_t base = 0x14000000u;
    if (address < base) return false;
    uint32_t offset = address - base;
    const bool extended = offset == 0x10c0 || offset == 0x10d0 ||
                          offset == 0x10e0 || offset == 0x10f0 || offset == 0x10f8;
    if (offset >= 0x400 && !extended) return false;
    if (extended) {
        // Limit each extra register to the access type seen in the diagnostic trace.
        const bool read16 = !write && size == 2 &&
                            (offset == 0x10c0 || offset == 0x10d0);
        const bool write16 = write && size == 2 &&
                             (offset == 0x10d0 || offset == 0x10e0);
        const bool read32 = !write && size == 4 &&
                            (offset == 0x10f0 || offset == 0x10f8);
        if (!(read16 || write16 || read32)) return false;
#if defined(TG50_MS_EXT_ALIAS)
        offset -= 0xe00;  // diagnostic hypothesis: mirrored command registers
#elif defined(TG50_MS_EXT_ZERO)
        // Diagnostic compatibility experiment, not an emulation of the actual registers.
        if (!write) std::memset(buf, 0, size);
#ifdef TG50_MS_TRACE
        std::fprintf(stderr, "[TG50 MS zero] %c%u @ %08x\n",
                     write ? 'W' : 'R', static_cast<unsigned>(size * 8), address);
#endif
        return true;
#else
        return false;  // standard build preserves strict legacy semantics
#endif
    }
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
    if (!memRegionAdd(mem, base, 0x1400, access, m)) {
        std::free(m);
        return nullptr;
    }
    return m;
}
