/*
 * I6502Emulator.h
 *
 *  Created on: 20 de jul. de 2021
 *      Author: alanl
 */

#ifndef I6502EMULATOR_H_
#define I6502EMULATOR_H_

#include "emu_base.h"
#include "emu_memory.h"
#include "emu_bus.h"
#include "emu_processor.h"

/**
 * Convenience façade over the four objects that make up a system.
 *
 * It implements reset/start/stop and nothing else. It does **not** own any of
 * the objects passed to it — you construct them, you keep them alive, you
 * destroy them.
 *
 * @code
 * Registers regs;  Memory mem(256);  Bus bus;  default_clock clk;
 * bus.connect(&mem);
 * Processor cpu(&bus, &regs, &clk);
 * I6502Emulator emu(&regs, &mem, &bus, &cpu);
 * emu.start(false);            // reset only; drive cpu.step() yourself
 * @endcode
 */
class I6502Emulator {
private:
	Registers* m_regs;
	Memory* m_memory;   //!< Held but unused; the Bus owns the access path.
	Bus* m_bus;
	Processor* m_processor;
public:
	I6502Emulator(Registers* _regs, Memory* _memory, Bus* _bus, Processor* _processor);
	virtual ~I6502Emulator();

	/** stop() followed by start(). */
	void reset(bool run);

	/**
	 * Perform the reset sequence: zero the registers, set SP to $FD, load PC
	 * from the reset vector at $FFFC/$FFFD, and charge 8 cycles.
	 *
	 * @param run when true, calls Processor::run() and therefore **blocks the
	 *            calling thread** until stop() or pause() is called from
	 *            elsewhere. Pass false to reset and return immediately.
	 *
	 * @note Unlike real hardware, this does not set the I flag.
	 */
	void start(bool run);

	/** Ask the processor to pause and spin until it has actually stopped. */
	void stop();
};

#endif /* I6502EMULATOR_H_ */
