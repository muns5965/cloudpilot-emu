#include "diag.h"

#ifdef UARM_DIAG

#include <cstdio>
#include <cstring>

#include "pace.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/heap.h>
#endif

// Output format (one line each, parsed by the web test runner):
//   [diag] abort <data|fetch> <r|w> addr=<hex> region=<name> pc=<hex> at=<name> mode=<hex> first
//   [diag] undef instr=<hex> pc=<hex> at=<name> mode=<hex> first
//   [diag] summary t=<s> ips=<n> ipsmax=<n> heap=<MiB> aborts=<n> unique=<n> undef=<n> new=<n>
//   [diag] repeat addr=<hex> pc=<hex> count=<n>          (sites whose count grew since the last summary, max 8)
//   [diag] mmio first <r|w> addr=<hex> size=<n> dev=<name> pc=<hex> at=<name> ok=<0|1>
//   [diag] mmio regs=<n> reads=<n> writes=<n> refused=<n>    (with each summary)
// "first" lines are printed once per site / register; repeats are only counted and reported in summaries.

namespace {
    constexpr uint64_t SUMMARY_INTERVAL_USEC = 10000000;
    constexpr unsigned SITES = 512;

    struct Site {
        uint32_t addr, pc;
        uint64_t count, reported;
        bool used;
    };

    Site sites[SITES];
    unsigned uniqueSites = 0;
    uint64_t aborts = 0, undefs = 0, newSinceSummary = 0, lastSummaryUsec = 0, firstUsec = 0;
    bool overflowReported = false;

    const char* region(uint32_t a) {
        if (a >= 0x10000000u && a < 0x11500000u) return "rom";
        if (a >= 0x20000000u && a < 0x30000000u) return "ram";
        if (a >= 0x30000000u && a < 0x30001000u) {
            switch (a & 0xf00) {
                case 0x000: return "pv-ic";
                case 0x100: return (a & 0xf0) == 0x10 ? "pv-uart" : "pv-uart-debug";
                case 0x200: return "pv-display";
                case 0x300: return "pv-touch";
                case 0x400: return "pv-rtc";
                case 0x500: return "pv-keys";
                case 0x600: return "pv-timer";
                case 0x700: return "pv-audio";
                case 0x800: return "pv-storage";
                case 0x900: return "pv-sysctl";
                default: return "pv-other";
            }
        }
        if (a >= 0x3f843000u && a < 0x3f844000u) return "sony-shared";
        if (a >= 0xffff0000u && a < 0xffff0100u) return "tinyram";
        return "unmapped";
    }

    // where the faulting code runs: the DAL is the first ROM chunk (0x10000230), the OS and modules follow
    const char* codeAt(uint32_t pc) {
        if (pc >= 0x10000230u && pc < 0x10040000u) return "dal";
        if (pc >= 0x10000000u && pc < 0x11500000u) return "rom";
        if (pc >= 0x20000000u && pc < 0x30000000u) return "ram";
        return region(pc);
    }

    Site* site(uint32_t addr, uint32_t pc, bool& isNew) {
        uint32_t h = ((addr >> 2) * 2654435761u ^ pc * 40503u) % SITES;
        for (unsigned i = 0; i < SITES; i++, h = (h + 1) % SITES) {
            if (sites[h].used && sites[h].addr == addr && sites[h].pc == pc) {
                isNew = false;
                return &sites[h];
            }
            if (!sites[h].used) {
                sites[h] = {addr, pc, 0, 0, true};
                uniqueSites++;
                isNew = true;
                return &sites[h];
            }
        }
        return nullptr;
    }
}  // namespace

namespace {
    constexpr unsigned REGS = 1024;
    struct Reg {
        uint32_t key;  // address << 1 | write
        uint64_t count;
        bool used;
    };
    Reg regs[REGS];
    unsigned regCount = 0;
    uint64_t mmioReads = 0, mmioWrites = 0, mmioRefused = 0;
}  // namespace

namespace {
    constexpr unsigned SHARED_VALUES = 512;
    uint64_t sharedSeen[SHARED_VALUES];
    unsigned sharedCount = 0;

    // every distinct (address, value) written to the Sony shared page, once (the page is a software interface
    // between Sony code and the DAL, so its values are worth seeing; capped)
    void sharedWrite(uint32_t addr, unsigned size, const void* buf) {
        uint32_t v = 0;
        memcpy(&v, buf, size > 4 ? 4 : size);
        const uint64_t key = (static_cast<uint64_t>(addr) << 32) | v;
        for (unsigned i = 0; i < sharedCount; i++)
            if (sharedSeen[i] == key) return;
        if (sharedCount == SHARED_VALUES) return;
        sharedSeen[sharedCount++] = key;
        const uint32_t pc = diagCpuPc();
        printf("[diag] shared w addr=%08x size=%u value=%0*x pc=%08x at=%s\n", addr, size, static_cast<int>(size * 2), v, pc,
               codeAt(pc));
    }
}  // namespace

// the ARM pc only shows where the 68k emulation was entered; the 68k state shows the instruction that faulted
namespace { void diagWatchCheck(); }

void diagPaceFault(uint32_t addr, bool write) {
    printf("[diag] pace-fault %s addr=%08x pc=%08x at=%s pc68k=%08x op=%04x\n", write ? "w" : "r", addr, diagCpuPc(),
           codeAt(diagCpuPc()), paceGetPC(), paceGetLastOpcode());
    diagVa0();
    diagWatchCheck();
    printf("[diag] pace-regs d=%08x %08x %08x %08x %08x %08x %08x %08x a=%08x %08x %08x %08x %08x %08x %08x %08x\n",
           paceGetDreg(0), paceGetDreg(1), paceGetDreg(2), paceGetDreg(3), paceGetDreg(4), paceGetDreg(5), paceGetDreg(6),
           paceGetDreg(7), paceGetAreg(0), paceGetAreg(1), paceGetAreg(2), paceGetAreg(3), paceGetAreg(4), paceGetAreg(5),
           paceGetAreg(6), paceGetAreg(7));
}

void diagMmio(uint32_t addr, unsigned size, bool write, bool ok, const void* buf) {
    (write ? mmioWrites : mmioReads)++;
    if (!ok) mmioRefused++;
    if (write && ok && buf && addr >= 0x3f843000u && addr < 0x3f844000u) sharedWrite(addr, size, buf);

    const uint32_t key = (addr << 1) | (write ? 1 : 0);
    uint32_t h = (key * 2654435761u) % REGS;
    for (unsigned i = 0; i < REGS; i++, h = (h + 1) % REGS) {
        if (regs[h].used && regs[h].key == key) {
            regs[h].count++;
            return;
        }
        if (!regs[h].used) {
            regs[h] = {key, 1, true};
            regCount++;
            const uint32_t pc = diagCpuPc();
            printf("[diag] mmio first %s addr=%08x size=%u dev=%s pc=%08x at=%s ok=%d\n", write ? "w" : "r", addr, size,
                   region(addr), pc, codeAt(pc), ok ? 1 : 0);
            return;
        }
    }
}

void diagMemAbort(uint32_t addr, bool write, bool instrFetch, uint32_t pc, uint32_t cpsr) {
    aborts++;

    bool isNew = false;
    Site* s = site(addr, pc, isNew);
    if (!s) {
        if (!overflowReported) printf("[diag] sites full (%u); further sites only counted\n", SITES);
        overflowReported = true;
        return;
    }

    s->count++;
    if (!isNew) return;

    newSinceSummary++;
    printf("[diag] abort %s %s addr=%08x region=%s pc=%08x at=%s mode=%02x first\n", instrFetch ? "fetch" : "data",
           write ? "w" : "r", addr, region(addr), pc, codeAt(pc), cpsr & 0x1f);
}

void diagUndefined(uint32_t pc, uint32_t instr, uint32_t cpsr) {
    undefs++;

    bool isNew = false;
    Site* s = site(0xffffffffu, pc, isNew);
    if (s) s->count++;
    if (!isNew) return;

    newSinceSummary++;
    printf("[diag] undef instr=%08x pc=%08x at=%s mode=%02x first\n", instr, pc, codeAt(pc), cpsr & 0x1f);
}

namespace {
    struct Watch { const char* name; uint32_t base; const uint8_t* buf; uint32_t size; uint8_t* last; unsigned changes; };
    Watch watches[4];
    unsigned watchCount = 0;

    void diagWatchCheck() {
        for (unsigned w = 0; w < watchCount; w++) {
            Watch& x = watches[w];
            uint32_t first = x.size, last = 0, n = 0;
            for (uint32_t i = 0; i < x.size; i++)
                if (x.buf[i] != x.last[i]) { if (first == x.size) first = i; last = i; n++; }
            if (!n) continue;
            if (x.changes++ < 16) {
                printf("[diag] watch %s change %u: %u bytes in %08x..%08x pc=%08x at=%s now", x.name, x.changes, n,
                       x.base + first, x.base + last, diagCpuPc(), codeAt(diagCpuPc()));
                for (uint32_t i = first; i <= last && i < first + 16; i++) printf(" %02x", x.buf[i]);
                printf("\n");
            }
            memcpy(x.last, x.buf, x.size);
        }
    }
}  // namespace

void diagLowWrite(uint32_t addr, unsigned size, uint32_t value, uint32_t pc68k, uint16_t opcode) {
    // writes to the first word are counted on their own (16 more), so the boot's other low writes cannot use up the
    // budget before a later NULL write (the Memo button path) happens
    static unsigned n = 0, n0 = 0;
    if (addr < 4 ? n0++ < 16 : n++ < 16)
        printf("[diag] low-write addr=%08x size=%u value=%0*x pc68k=%08x op=%04x n=%u\n", addr, size,
               static_cast<int>(size * 2), value, pc68k, opcode, addr < 4 ? n0 : n);
}

void diagWatchRam(const char* name, uint32_t base, const void* buffer, uint32_t size) {
    if (watchCount >= 4) return;
    Watch& x = watches[watchCount++];
    x = {name, base, static_cast<const uint8_t*>(buffer), size, new uint8_t[size], 0};
    memcpy(x.last, x.buf, size);
    printf("[diag] watch %s %08x size %u\n", name, base, size);
}

void diagSummary(uint64_t nowUsec, uint32_t currentIps, uint64_t currentIpsMax) {
    if (firstUsec == 0) firstUsec = lastSummaryUsec = nowUsec;
    if (nowUsec - lastSummaryUsec < SUMMARY_INTERVAL_USEC) return;
    lastSummaryUsec = nowUsec;

    unsigned heapMiB = 0;
#ifdef __EMSCRIPTEN__
    heapMiB = static_cast<unsigned>(emscripten_get_heap_size() >> 20);
#endif

    printf("[diag] summary t=%.1f ips=%u ipsmax=%llu heap=%u aborts=%llu unique=%u undef=%llu new=%llu\n",
           (nowUsec - firstUsec) / 1e6, currentIps, static_cast<unsigned long long>(currentIpsMax), heapMiB,
           static_cast<unsigned long long>(aborts), uniqueSites, static_cast<unsigned long long>(undefs),
           static_cast<unsigned long long>(newSinceSummary));
    newSinceSummary = 0;
    diagVa0();
    diagWatchCheck();
    printf("[diag] mmio regs=%u reads=%llu writes=%llu refused=%llu\n", regCount,
           static_cast<unsigned long long>(mmioReads), static_cast<unsigned long long>(mmioWrites),
           static_cast<unsigned long long>(mmioRefused));

    unsigned shown = 0;
    for (auto& s : sites) {
        if (!s.used || s.count == s.reported) continue;
        if (s.count > 1 && shown < 8) {
            printf("[diag] repeat addr=%08x pc=%08x count=%llu\n", s.addr, s.pc,
                   static_cast<unsigned long long>(s.count));
            shown++;
        }
        s.reported = s.count;
    }
}

#endif  // UARM_DIAG
