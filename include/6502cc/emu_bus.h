/*
 * emu_bus.h
 *
 *  Created on: 26 de jul. de 2021
 *      Author: alanl
 */

#ifndef EMU_BUS_H_
#define EMU_BUS_H_

#include "emu_base.h"
#include "emu_memory.h"

/**
 * The CPU's window onto the outside world.
 *
 * Every memory access made by every instruction goes through read()/write(),
 * which makes this the extension point for any system with memory-mapped I/O.
 * The base implementation just forwards to a single connected Memory with no
 * address decoding at all.
 *
 * To model a real machine, subclass and decode the address:
 *
 * @code
 * class NesBus : public Bus {
 *     uint8 read(uint16 a) override {
 *         if (a < 0x2000) return ram[a & 0x07FF];   // mirrored RAM
 *         if (a < 0x4000) return ppu->reg(a & 7);
 *         return cart->cpuRead(a);
 *     }
 * };
 * @endcode
 *
 * If your device registers have side effects on read (as PPU registers do),
 * add a separate side-effect-free accessor for debuggers to use — read() is
 * called by the running CPU and must not be used for inspection.
 *
 * @see docs/nes-roadmap.md
 */
class Bus {
protected:
	Memory* p_mem;  //!< Unused by subclasses that decode addresses themselves.
public:
	Bus();
	virtual ~Bus();

	/** Attach the backing store used by the default read()/write(). */
	virtual void connect(Memory* mem);

	/** Read one byte. Returns 0 when no Memory is connected. */
	virtual uint8 read(uint16 address);
	/** Write one byte. Silently discarded when no Memory is connected. */
	virtual void write(uint16 address, uint8 val);
};

#endif /* EMU_BUS_H_ */
