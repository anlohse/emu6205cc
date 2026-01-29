/*
 * I6502Emulator.cpp
 *
 *  Created on: 20 de jul. de 2021
 *      Author: alanl
 */

#include "../include/6502cc/I6502Emulator.h"
#include <cstring>

#include "../include/6502cc/emu_base.h"
#include "../include/6502cc/emu_memory.h"

#include <thread>

void wait() {
	std::this_thread::yield();
}

I6502Emulator::I6502Emulator(Registers *_regs, Memory *_memory, Bus* _bus,
		Processor *_processor): m_regs(_regs), m_memory(_memory), m_bus(_bus), m_processor(_processor) {
}
I6502Emulator::~I6502Emulator() {
}

void I6502Emulator::reset(bool run) {
	stop();
	start(run);
}

void I6502Emulator::start(bool run) {
	memset((void*)m_regs,0,sizeof(Registers));
	m_regs->sp = 0xfd;
	m_regs->sr = FLAG__;
	m_regs->pc = m_bus->read(0xfffc) | (m_bus->read(0xfffd) << 8);
	m_processor->p_clock->reset();
	m_processor->p_clock->beginCycle();
	m_processor->p_clock->waitCycles(8);
	if (run)
		m_processor->run();
}

void I6502Emulator::stop() {
	m_processor->pause();
	while (m_processor->isRunning())
		wait();
}

