#ifndef UNASM_H
#define UNASM_H

#include "emu_base.h"
#include "emu_memory.h"
#include "emu_bus.h"
#include <string>

/**
 * Single-instruction disassembler.
 *
 * Decodes opcodes from their `aaabbbcc` bit fields rather than from a table.
 * Unassigned opcodes render as a bare `$XX`. Output uses the same
 * non-standard `($nn), X` operand form as Asm — see docs/assembler.md.
 */
class UnAsm {
public:
	UnAsm();
	~UnAsm();

	/**
	 * Disassemble at _regs->pc and **advance _regs->pc** past the instruction.
	 *
	 * Use this to walk a listing forward. Pass a scratch copy of the CPU's
	 * registers so the running program counter is not disturbed.
	 */
	std::string unasm_line(Bus* _bus, Registers* _regs);

	/**
	 * Disassemble at @p at without modifying anything.
	 *
	 * Convenient for random access, but it does not report the instruction's
	 * length, so it cannot be used to step forward — use the Registers* form
	 * for that.
	 */
	std::string unasm_line(Bus* _bus, uint16 at);
};

#endif // UNASM_H
