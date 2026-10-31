#ifndef _UARM_DIAG_H_
#define _UARM_DIAG_H_

// Test diagnostics (build with -DUARM_DIAG only; the regular builds compile none of this). Records CPU memory
// aborts (unmapped addresses and device registers a model refuses), undefined instructions and a periodic summary
// as one-line "[diag] ..." messages on stdout, for an automated test runner that reads the console. Only error
// paths and one timestamp check per main-loop slice are touched, so emulation timing is not changed.

#include <cstdint>

#ifdef UARM_DIAG

void diagMemAbort(uint32_t addr, bool write, bool instrFetch, uint32_t pc, uint32_t cpsr);
void diagUndefined(uint32_t pc, uint32_t instr, uint32_t cpsr);
void diagSummary(uint64_t nowUsec, uint32_t currentIps, uint64_t currentIpsMax);

// register census: every access that reaches a device region (not RAM/ROM), counted per (address, direction) in
// memory; the first access of each register is printed once, totals with the summary
// buf holds the value of a write (used to record the distinct values written to the Sony shared page)
void diagMmio(uint32_t addr, unsigned size, bool write, bool ok, const void* buf);

// a memory fault inside the 68k emulation (PACE): the regular build reports it on stderr only natively
void diagPaceFault(uint32_t addr, bool write);

uint32_t diagCpuPc(void);  // pc of the instruction being executed (CPU.cpp)
void diagVa0(void);
// a RAM area watched from the summary: the first change and each later change are reported (offsets, bytes), so a
// region that exists only for compatibility (VZ90 low RAM at 0) shows when and how it is used
// a 68k write below 0x1000 (low memory; RAM only on VZ90 images): the first 16 are printed with value, 68k pc and
// opcode, whatever the old contents were (a CLR.B of a byte that is already 0 changes nothing a watch could see)
void diagLowWrite(uint32_t addr, unsigned size, uint32_t value, uint32_t pc68k, uint16_t opcode);
void diagWatchRam(const char* name, uint32_t base, const void* buffer, uint32_t size);         // mapping of virtual address 0 under the guest's page tables (CPU.cpp)

#endif

#endif  // _UARM_DIAG_H_
