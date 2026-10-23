// Ported from uARM-Palm/uARM mmiodev_TG50uc.c; not yet wired into any device.
//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "mmiodev_TG50uc.h"
#include "sony_tg50_hardware.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>


//0x100 seems to need to contain 'GA' (it contains 'IO' when an io interrupt has ben triggered it seems)
//0x0c (gpioD lower 4 bits) seems to be device revision id in lower 4 bits. nonzero is required to consider reading battery voltage at all

//164 is also likely an interrupt status word, with mask at 0x140 (or reversed)

//if 100 has 'IO' then irqs are masked by writing 1 to 0x170. unmasked by setting bit 0x0002 in 0x170
// in all cases, also 0x120 has mask which can be read and set (more than bit)

//184, 188, 18c is IRC stuff

//state on a normal clie:
/*	GPIO PORT/REG	00		20		40		60		80		a0		c0		120	
	0				e13d	1e40	0000	0000	a010	a110	0000	a110
	1				aaec	9fef	0000	0000	0000	0000	0000	0000
	2				ff00	00ff	0000	0000	ff00	ff00	0000	ff00
	3				0004	0000	0000	0000	0000	0000	0000	0000

	it is thus clear that each gpio port has 8 registers controlling it
	@0x000 is readable input state reg
	@0x020 is direction (1 = out)
	@0x040 is w1 "set hi" reg
	@0x060 is w1 "set lo" reg
	@0x080 is rising edge detect (original uARM: falling)
	@0x0a0 is falling edge detect (original uARM: rising)
	@0x0c0 is accumulates triggered edges, w1c
	@0x120 is whether triggered edges interrupt the main cpu!
*/


#define TG50_UC_BASE sony_tg50::kUcBase
#define TG50_UC_SIZE sony_tg50::kUcSize


struct TG50ucGpioPort {
	//sure
	uint16_t latches;
	uint16_t inputs;
	uint16_t direction;
	uint16_t detectRise;
	uint16_t detectFall;
	uint16_t edgesDetected;
	uint16_t edgesCausingIrq;
	
	//calced
	uint16_t curState, prevState;
};


struct TG50uc {
	
	struct PxaGpio *gpio;
	int8_t gpioChangeIrqNo;
    bool irqAsserted; // diagnostic edge tracking; also avoids redundant GPIO writes
	
	struct TG50ucGpioPort port[4];			//gpio[3] has revision straps on low 4 bits

	bool keys[8][8];
	uint32_t keyVals[8][8];
	bool ignoreNextReset;
};



static void tg50ucPrvRecalcGpioVals(struct TG50uc *uc, struct TG50ucGpioPort *port);

static void tg50ucPrvRecalcKeypadMatrix(struct TG50uc *uc)
{
    for (unsigned col = 0; col < 8; ++col) {
        bool anyDriven = false;
        bool hasLow = false;
        bool hasHigh = false;

        for (unsigned row = 0; row < 8; ++row) {
            if (!(uc->port[2].direction & (1u << row)) || !uc->keys[row][col])
                continue;
            anyDriven = true;
            if (uc->port[2].latches & (1u << row))
                hasHigh = true;
            else
                hasLow = true;
        }

        // Preserve upstream's high-wins convention for conflicting driven rows.
        const bool high = !anyDriven || hasHigh || !hasLow;
        if (high)
            uc->port[2].inputs |= (uint16_t)(1u << (col + 8));
        else
            uc->port[2].inputs &= (uint16_t)~(1u << (col + 8));
    }
    tg50ucPrvRecalcGpioVals(uc, &uc->port[2]);
}

static void tg50ucPrvRecalcGpioIrqs(struct TG50uc *uc)
{
    bool irq = false;
    for (unsigned i = 0; i < sizeof(uc->port) / sizeof(uc->port[0]); ++i)
        irq = irq || (uc->port[i].edgesDetected & uc->port[i].edgesCausingIrq) != 0;

    if (uc->gpioChangeIrqNo < 0 || irq == uc->irqAsserted)
        return;

#ifdef TG50_TRACE_IRQ
    fprintf(stderr, "[TG50 IRQ] GPIO %d: %s\n", (int)uc->gpioChangeIrqNo,
            irq ? "assert" : "release");
    for (unsigned i = 0; i < 4; ++i)
        fprintf(stderr, "[TG50 IRQ] port%u pending=%04x mask=%04x\n", i,
                uc->port[i].edgesDetected, uc->port[i].edgesCausingIrq);
#endif
    uc->irqAsserted = irq;
    pxaGpioSetState(uc->gpio, (uint_fast8_t)uc->gpioChangeIrqNo, !irq);
}

static void tg50ucPrvRecalcGpioVals(struct TG50uc *uc, struct TG50ucGpioPort *port)
{
	uint16_t diff, wentHi, wentLo;
	
	port->prevState = port->curState;
	port->curState = (port->latches & port->direction) | (port->inputs & ~port->direction);
	
	diff = port->prevState ^ port->curState;
	wentHi = diff & port->curState;
	wentLo = diff & port->prevState;
	
	port->edgesDetected |= wentLo & port->detectFall;
	port->edgesDetected |= wentHi & port->detectRise;
	tg50ucPrvRecalcGpioIrqs(uc);
}

static void tg50ucPrvReset(struct TG50uc *uc)
{
	uint32_t i;
	
	for (i = 0; i < sizeof(uc->port) / sizeof(*uc->port); i++) {
		uc->port[i].direction = 0;
		uc->port[i].latches = 0;
		uc->port[i].detectRise = 0;
		uc->port[i].detectFall = 0;
		uc->port[i].edgesDetected = 0;
		uc->port[i].edgesCausingIrq = 0;
		uc->port[i].curState = 0;
		uc->port[i].prevState = 0;
		tg50ucPrvRecalcGpioVals(uc, &uc->port[i]);
	}
    tg50ucPrvRecalcKeypadMatrix(uc); // restore idle-high keyboard columns
}

static bool tg50ucPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct TG50uc *uc = (struct TG50uc*)userData;
	bool handled = true;
	uint32_t val = 0;
	
	pa -= TG50_UC_BASE;
	
	
	if (size == 4) {
		
		if (write)
			val = *(uint32_t*)buf;
		
		switch (pa) {
			case 0x00:	//GPIOA state (RO)
			case 0x04:	//GPIOB state (RO)
			case 0x08:	//GPIOC state (RO)
			case 0x0c:	//GPIOD state (RO)
				if (write)
					handled = false;
				else
					val = uc->port[(pa >> 2) & 3].curState;
				break;
			
			case 0x20: // GPIOA direction
            case 0x24: // GPIOB direction
            case 0x28: // GPIOC direction (keyboard rows)
            case 0x2c: // GPIOD direction
                if (write) {
                    auto& port = uc->port[(pa >> 2) & 3];
                    port.direction = static_cast<uint16_t>(val);
                    if (pa == 0x28)
                        tg50ucPrvRecalcKeypadMatrix(uc);
                    else
                        tg50ucPrvRecalcGpioVals(uc, &port);
                } else {
                    val = uc->port[(pa >> 2) & 3].direction;
                }
                break;

            case 0x40:	//GPIOA set (WO)
			case 0x44:	//GPIOB set (WO)
			case 0x48:	//GPIOC set (WO)
			case 0x4c:	//GPIOD set (WO)
				if (!write)
					handled = false;
				else {
					uc->port[(pa >> 2) & 3].latches |= val;
					tg50ucPrvRecalcKeypadMatrix(uc);
					tg50ucPrvRecalcGpioVals(uc, &uc->port[(pa >> 2) & 3]);
				}
				break;
			
			case 0x60:	//GPIOA clear (WO)
			case 0x64:	//GPIOB clear (WO)
			case 0x68:	//GPIOC clear (WO)
			case 0x6c:	//GPIOD clear (WO)
				if (!write)
					handled = false;
				else {
					uc->port[(pa >> 2) & 3].latches &=~ val;
					tg50ucPrvRecalcKeypadMatrix(uc);
					tg50ucPrvRecalcGpioVals(uc, &uc->port[(pa >> 2) & 3]);
				}
				break;
			
			// Swapped versus original uARM (0x80 = falling there). Idle TG50 firmware sets port C
			// 0x88 = 0, 0xa8 = 0xff00 to catch key presses (columns go low); see docs/sony-tg50-port.md
			case 0x80:	//GPIOA risingEdgeDetect (RW)
			case 0x84:	//GPIOB risingEdgeDetect (RW)
			case 0x88:	//GPIOC risingEdgeDetect (RW)
			case 0x8c:	//GPIOD risingEdgeDetect (RW)
				if (write)
					uc->port[(pa >> 2) & 3].detectRise = val;
				else
					val = uc->port[(pa >> 2) & 3].detectRise;
				break;
			
			case 0xa0:	//GPIOA fallingEdgeDetect (RW)
			case 0xa4:	//GPIOB fallingEdgeDetect (RW)
			case 0xa8:	//GPIOC fallingEdgeDetect (RW)
			case 0xac:	//GPIOD fallingEdgeDetect (RW)
				if (write)
					uc->port[(pa >> 2) & 3].detectFall = val;
				else
					val = uc->port[(pa >> 2) & 3].detectFall;
				break;
			
			case 0xc0:	//GPIOA edgesDetected (R, w1c)
			case 0xc4:	//GPIOB edgesDetected (R, w1c)
			case 0xc8:	//GPIOC edgesDetected (R, w1c)
			case 0xcc:	//GPIOD edgesDetected (R, w1c)
				if (write) {
					uc->port[(pa >> 2) & 3].edgesDetected &=~ val;
					tg50ucPrvRecalcGpioIrqs(uc);
				}
				else
					val = uc->port[(pa >> 2) & 3].edgesDetected;
				break;

			case 0x100:	//version query?
				if (write)
					handled = false;
				else
					val = 0x4741;	//'GA'
				break;
			
			case 0x120:	//GPIOA edgeDetectCausesIrq (RW)
			case 0x124:	//GPIOB edgeDetectCausesIrq (RW)
			case 0x128:	//GPIOC edgeDetectCausesIrq (RW)
			case 0x12c:	//GPIOD edgeDetectCausesIrq (RW)
				if (write) {
					uc->port[(pa >> 2) & 3].edgesCausingIrq = val;
					tg50ucPrvRecalcGpioIrqs(uc);
				}
				else
					val = uc->port[(pa >> 2) & 3].edgesCausingIrq;
				break;
			
			//high-power ir things
			case 0x180:
			case 0x184:
			case 0x188:
			case 0x18c:
			case 0x194:
				//fallthrough
			
			case 0x140:
			case 0x164:
			case 0x200:
			case 0x204:
			case 0x210:		//same as reset ???
				break;
			
			default:
				handled = false;
				break;
		}
		
		if (!write)
			*(uint32_t*)buf = val;
		
		if (handled) {
			
			if (write)
				fprintf(stderr, " * UC 0x%08lx -> [0x%08lx]\n", (unsigned long)val, (unsigned long)pa);
			else
				fprintf(stderr, " * UC [0x%08lx] -> 0x%08lx\n", (unsigned long)pa, (unsigned long)val);
		}
		else {
			
			if (write)
				fprintf(stderr, " * UC 0x%08lx -> [0x%08lx] FAILED\n", (unsigned long)val, (unsigned long)pa);
			else
				fprintf(stderr, " * UC [0x%08lx] -> (4 bytes) FAILED\n", (unsigned long)pa);
		}
		
		return handled;
	}
	else if (size == 1) {
		
		if (pa == 0x210 && write && *(uint8_t*)buf == 0xff) {	//reset
			if (uc->ignoreNextReset) {	// sleep resume: keep keyboard/IRQ configuration
				uc->ignoreNextReset = false;
				return true;
			}
			tg50ucPrvReset(uc);
			fprintf(stderr, " * UC reset\n");
			return true;
		}
		
		if (write)
			fprintf(stderr, " * UC 0x%02x -> [0x%08x] FAILED\n", *(uint8_t*)buf, pa);
		else
			fprintf(stderr, " * UC [0x%08x] -> (1 byte) FAILED\n", pa);
		
		return false;
	}
	else {
		
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	return true;
}

struct TG50uc* tg50ucInit(struct ArmMem *physMem, struct PxaGpio *gpio, int8_t gpioChangeIrqNo, const uint32_t *keyMap)
{
	struct TG50uc *uc = (struct TG50uc*)malloc(sizeof(*uc));
	unsigned r, c;
	
	if (!uc) return nullptr;
	
	memset(uc, 0, sizeof (*uc));
	
	uc->gpio = gpio;
	uc->gpioChangeIrqNo = gpioChangeIrqNo;
	
	tg50ucPrvReset(uc);

	tg50ucGpioSetInState(uc, 0, 4, true);	//hold switch off
	tg50ucGpioSetInState(uc, 0, 13, true);	//in cradle
	tg50ucGpioSetInState(uc, 0, 14, true);	//charging
	tg50ucGpioSetInState(uc, 0, 15, true);	//headphones removed
	tg50ucGpioSetInState(uc, 3, 2, true);	//revision 4 (0100)
	
	for (r = 0; r < 8; r++) {
		for (c = 0; c < 8; c++)
			uc->keyVals[r][c] = *keyMap++;
	}
	
	tg50ucPrvRecalcKeypadMatrix(uc); // establish column pullups before firmware reads
	if (!memRegionAdd(physMem, TG50_UC_BASE, TG50_UC_SIZE, tg50ucPrvMemAccessF, uc)) { free(uc); return nullptr; }
	
	return uc;
}

void tg50ucIgnoreNextReset(struct TG50uc *uc)
{
	uc->ignoreNextReset = true;
}

void tg50ucGpioSetInState(struct TG50uc *uc, unsigned port, unsigned pin, bool hi)
{
	if (hi)
		uc->port[port].inputs |= 1 << pin;
	else
		uc->port[port].inputs &=~ (1 << pin);
	
	tg50ucPrvRecalcGpioVals(uc, &uc->port[port]);
}

void tg50ucSetKeyPressed(struct TG50uc *uc, uint32_t key, bool pressed)
{
	bool changed = false;
	unsigned r, c;
	
	for (r = 0; r < 8; r++) {
		for (c = 0; c < 8; c++) {
			if (uc->keyVals[r][c] == key && key != 0) {
				if (uc->keys[r][c] != pressed) {
					
					changed = true;
					uc->keys[r][c] = pressed;
				}
			}
		}
	}
	
	if (changed)
		tg50ucPrvRecalcKeypadMatrix(uc);
}

namespace {
struct TG50ucState {
	struct TG50ucGpioPort port[4];
	bool keys[8][8];
	bool irqAsserted;
	bool ignoreNextReset;
};
}

size_t tg50ucStateSize() { return sizeof(TG50ucState); }

void tg50ucGetState(struct TG50uc *uc, void *out)
{
	TG50ucState s;
	memset(&s, 0, sizeof(s));
	memcpy(s.port, uc->port, sizeof(s.port));
	memcpy(s.keys, uc->keys, sizeof(s.keys));
	s.irqAsserted = uc->irqAsserted;
	s.ignoreNextReset = uc->ignoreNextReset;
	memcpy(out, &s, sizeof(s));
}

void tg50ucSetState(struct TG50uc *uc, const void *in)
{
	TG50ucState s;
	memcpy(&s, in, sizeof(s));
	memcpy(uc->port, s.port, sizeof(s.port));
	memcpy(uc->keys, s.keys, sizeof(s.keys));
	uc->irqAsserted = s.irqAsserted;
	uc->ignoreNextReset = s.ignoreNextReset;
}
