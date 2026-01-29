#ifndef ASM_processor_H
#define ASM_processor_H

#include "emu_base.h"
#include "emu_memory.h"
#include "emu_bus.h"
#include <unordered_set>
#include <functional>

#include "emu_clock.h"

class BaseInstruction {
public:
	BaseInstruction() {
	}
	virtual ~BaseInstruction() {
	}
	virtual int execute(Registers *regs, Bus *bus) = 0;
};

class Processor {
public:
	typedef std::function<void()> InstructionCallBackType;
protected:
	friend class I6502Emulator;
	Bus* p_bus;
	Registers* p_regs;
	emu_clock* p_clock;
	static BaseInstruction* _instruction_table[256];
	volatile bool m_running;
	volatile bool m_stopping;
	InstructionCallBackType p_instr_callback;
public:
	Processor(Bus* _bus, Registers* _regs, emu_clock* _clock);
	virtual ~Processor();
	virtual void run();
	virtual void step();
	virtual void pause();
	virtual void resume();
	bool isRunning() const;
	void setInstructionCallback(const InstructionCallBackType& _instr_callback);
};

class DebugProcessor : public Processor {
protected:
	std::unordered_set<uint16> m_breakpoints;
	InstructionCallBackType p_breakp_callback;
public:
	DebugProcessor(Bus* _bus, Registers* _regs, emu_clock* _clock, std::unordered_set<uint16> _breakpoints);
	DebugProcessor(Bus* _bus, Registers* _regs, emu_clock* _clock, std::initializer_list<uint16> _breakpoints);
	virtual ~DebugProcessor();
	virtual void run();
	bool addBreakpoint(uint16 address);
	bool removeBreakpoint(uint16 address);
	void clearBreakpoints();
	void setBreakpointCallback(const InstructionCallBackType& _instr_callback);
	const std::unordered_set<uint16>& breakpoints() const;
};

#endif // ASM_processor_H
