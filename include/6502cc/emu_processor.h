#ifndef ASM_processor_H
#define ASM_processor_H

#include "emu_base.h"
#include "emu_memory.h"
#include "emu_bus.h"
#include <atomic>
#include <unordered_set>
#include <functional>

#include "emu_clock.h"

/**
 * One entry in the opcode table.
 *
 * Concrete instructions are class templates parameterised on an addressing-mode
 * struct and a cycle count, instantiated once per opcode in Processor.cpp.
 *
 * @warning Implementations hold their decoded operand in a *member*, so a
 * single instruction object is not reentrant. Because the table is static and
 * shared by every Processor, two processors stepping on different threads will
 * corrupt each other's operands.
 *
 * @see docs/architecture.md
 */
class BaseInstruction {
public:
	BaseInstruction() {
	}
	virtual ~BaseInstruction() {
	}
	/**
	 * Consume operand bytes (advancing regs->pc), apply the effects, and
	 * return the number of cycles consumed.
	 */
	virtual int execute(Registers *regs, Bus *bus) = 0;
};

/**
 * The interpreter: fetch, decode, execute.
 *
 * Holds no state of its own beyond the run and interrupt flags — registers,
 * memory and pacing are all borrowed. The caller owns all three and must keep
 * them alive.
 */
class Processor {
public:
	typedef std::function<void()> InstructionCallBackType;
protected:
	friend class I6502Emulator;
	Bus* p_bus;
	Registers* p_regs;
	emu_clock* p_clock;
	/** Shared 256-entry opcode table, covering all 256 opcodes. */
	static BaseInstruction* _instruction_table[256];
	std::atomic<bool> m_running;
	std::atomic<bool> m_stopping;
	std::atomic<bool> m_nmi_pending;  //!< Edge-triggered: latched until serviced.
	std::atomic<bool> m_irq_line;     //!< Level-triggered: held while a device asserts it.
	/**
	 * Whether the last instruction changed I too late to be polled.
	 *
	 * CLI, SEI and PLP write I in their own final cycle, which is after the
	 * point where the CPU has already decided whether to service an interrupt
	 * at that boundary -- so their change is felt one instruction later. Every
	 * other instruction, RTI included, is in time.
	 *
	 * Only the delay is tracked, not a shadow copy of the flag, so that a
	 * debugger or a test writing I directly is obeyed at once rather than an
	 * instruction later.
	 */
	bool m_i_flag_delayed;
	bool m_i_flag_before;   //!< What I was before that instruction.
	InstructionCallBackType p_instr_callback;

	/**
	 * Push PC and SR, disable interrupts and jump through @p vector.
	 * @return the cycle cost of the sequence.
	 */
	int serviceInterrupt(uint16 vector);
public:
	Processor(Bus* _bus, Registers* _regs, emu_clock* _clock);
	virtual ~Processor();

	/**
	 * Run step() until pause() is called. **Blocks the calling thread** — run
	 * it on a std::thread if you need to keep a UI responsive.
	 */
	virtual void run();

	/**
	 * Execute exactly one instruction, or service a pending interrupt.
	 *
	 * Interrupts are checked before the opcode fetch: a latched NMI wins, then
	 * IRQ if the I flag is clear. Either way the clock is charged and the
	 * instruction callback fires.
	 *
	 * This is the integration point for co-processors. The cycle cost is
	 * charged to the clock, so read emu_clock::cycles() before and after to
	 * learn how long the step took and advance a PPU or APU by the same amount.
	 */
	virtual void step();

	/** Request that run() return. Non-blocking; returns before the loop exits. */
	virtual void pause();
	/** Resume execution by re-entering run(). Blocks like run() does. */
	virtual void resume();
	bool isRunning() const;

	/**
	 * Assert the NMI line.
	 *
	 * Edge-triggered: the request latches and is serviced before the next
	 * instruction regardless of the I flag. Safe to call from another thread.
	 */
	void nmi();

	/**
	 * Hold or release the IRQ line.
	 *
	 * Level-triggered: while asserted, an interrupt is serviced before every
	 * instruction for as long as the I flag is clear. The device is responsible
	 * for releasing the line once its handler acknowledges it. Safe to call
	 * from another thread.
	 */
	void irq(bool asserted);

	bool nmiPending() const;
	bool irqAsserted() const;
	/** Drop any latched NMI and release the IRQ line. Called on reset. */
	void clearInterrupts();

	/** Install a hook fired after every instruction. Keep it cheap. */
	void setInstructionCallback(const InstructionCallBackType& _instr_callback);
};

/**
 * A Processor that stops when pc reaches a breakpoint address.
 *
 * run() deliberately steps once before testing the breakpoint set, so that
 * resuming while stopped *at* a breakpoint makes progress instead of
 * immediately re-triggering.
 */
class DebugProcessor : public Processor {
protected:
	std::unordered_set<uint16> m_breakpoints;
	InstructionCallBackType p_breakp_callback;
public:
	DebugProcessor(Bus* _bus, Registers* _regs, emu_clock* _clock, std::unordered_set<uint16> _breakpoints);
	DebugProcessor(Bus* _bus, Registers* _regs, emu_clock* _clock, std::initializer_list<uint16> _breakpoints);
	virtual ~DebugProcessor();

	/** Step until pc hits a breakpoint or pause() is called. Blocks. */
	virtual void run();

	/** @return false if the address was already present. */
	bool addBreakpoint(uint16 address);
	/** @return false if the address was not present. */
	bool removeBreakpoint(uint16 address);
	void clearBreakpoints();
	/** Install a hook fired when run() stops on a breakpoint. */
	void setBreakpointCallback(const InstructionCallBackType& _instr_callback);
	const std::unordered_set<uint16>& breakpoints() const;
};

#endif // ASM_processor_H
