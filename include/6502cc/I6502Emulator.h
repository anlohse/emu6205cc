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

class I6502Emulator {
private:
	Registers* m_regs;
	Memory* m_memory;
	Bus* m_bus;
	Processor* m_processor;
public:
	I6502Emulator(Registers* _regs, Memory* _memory, Bus* _bus, Processor* _processor);
	virtual ~I6502Emulator();

	void reset(bool run);

	void start(bool run);
	void stop();
};

#endif /* I6502EMULATOR_H_ */
