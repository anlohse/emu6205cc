#ifndef INSTRUCTION_IMPL_H
#define INSTRUCTION_IMPL_H

#include "../include/6502cc/emu_base.h"
#include "../include/6502cc/emu_memory.h"

/*
 * Instruction implementations.
 *
 * Every operation is a class template parameterised on an addressing-mode policy
 * (see parameters.h) and its base cycle count. Processor.cpp instantiates one per
 * opcode.
 *
 * The params object is a local inside execute(), not a member, so instruction
 * objects are stateless and the shared opcode table is safe to use from more than
 * one Processor.
 *
 * Read instructions add params.extraCycles() to their result to charge the
 * page-crossing penalty. Stores and read-modify-write instructions do not: they
 * always pay for the extra bus cycle, which is already in their base count.
 */

void push8(Registers *regs, Bus *bus, uint8 val);
void push16(Registers *regs, Bus *bus, uint16 val);
uint8 pop8(Registers *regs, Bus *bus);
uint16 pop16(Registers *regs, Bus *bus);

/** Set N and Z from a result byte -- the most common flag update. */
inline void setNZ(Registers *regs, uint8 val) {
	regs->setStatus(FLAG_Z, val == 0);
	regs->setStatus(FLAG_N, (val & 0x80) != 0);
}

/** Add with carry, honouring decimal mode. Shared by ADC, RRA. */
inline void adcCore(Registers *regs, uint8 val) {
	uint8 a = regs->a;
	uint16 res = a + val + (regs->getStatus(FLAG_C) ? 1 : 0);
	regs->setStatus(FLAG_Z, (res & 0xff) == 0);
	if (regs->getStatus(FLAG_D)) {
		if (((a & 0xF) + (val & 0xF) + (regs->getStatus(FLAG_C) ? 1 : 0)) > 9)
			res += 6;
		regs->setStatus(FLAG_N, res & 0x80);
		regs->setStatus(FLAG_V, !((a ^ val) & 0x80) && ((a ^ res) & 0x80));
		if (res > 0x99) {
			res += 96;
		}
		regs->setStatus(FLAG_C, res > 0x99);
	} else {
		regs->setStatus(FLAG_V, (~((uint16) a ^ (uint16) val) & ((uint16) a ^ res)) & 0x80);
		regs->setStatus(FLAG_C, res > 0x00ff);
		regs->setStatus(FLAG_N, (res & 0x80) != 0);
	}
	regs->a = res & 0xff;
}

/** Subtract with borrow, honouring decimal mode. Shared by SBC, ISC. */
inline void sbcCore(Registers *regs, uint8 val) {
	uint8 a = regs->a;
	uint16 res = a - val - (regs->getStatus(FLAG_C) ? 0 : 1);
	regs->setStatus(FLAG_V, (((uint16) a ^ (uint16) val) & ((uint16) a ^ res)) & 0x80);
	regs->setStatus(FLAG_N, (res & 0x80) != 0);
	regs->setStatus(FLAG_Z, (res & 0xff) == 0);
	if (regs->getStatus(FLAG_D)) {
		if (((a & 0x0F) - (regs->getStatus(FLAG_C) ? 0 : 1)) < (val & 0x0F))
			res -= 6;
		if (res > 0x99) {
			res -= 0x60;
		}
	}
	regs->a = res & 0xff;
	regs->setStatus(FLAG_C, res < 0x100);
}

/** Compare a register against a value, setting N/Z/C. Shared by CMP/CPX/CPY, DCP. */
inline void cmpCore(Registers *regs, uint8 reg, uint8 val) {
	uint16 r = (uint16) reg - (uint16) val;
	regs->setStatus(FLAG_C, r < 0x100);
	regs->setStatus(FLAG_Z, r == 0);
	regs->setStatus(FLAG_N, (r & 0x80) != 0);
}

/**
 * Conditional branch.
 *
 * Costs the base count, plus one cycle if the branch is taken, plus one more if
 * the target is in a different page than the instruction that follows.
 */
template<class ParamType>
inline int branch(Registers *regs, Bus *bus, uint8 flag, bool set, int cycles) {
	ParamType params;
	params.init(regs, bus);
	uint16 val = params.get8bit(regs, bus);
	if (regs->getStatus(flag) != set)
		return cycles;
	if (val & 0x80)
		val |= 0xff00; // sign-extend the displacement
	uint16 target = val + regs->pc;
	int extra = 1;
	if ((regs->pc & 0xff00) != (target & 0xff00))
		extra++;
	regs->pc = target;
	return cycles + extra;
}

/* ------------------------------------------------------------------------- */
/* Documented instructions                                                    */
/* ------------------------------------------------------------------------- */

template<class IParams, int cycles>
class ADC_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		adcCore(regs, params.get8bit(regs, bus));
		return cycles + params.extraCycles();
	}
};

template<class IParams, int cycles>
class AND_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		regs->a &= params.get8bit(regs, bus);
		setNZ(regs, regs->a);
		return cycles + params.extraCycles();
	}
};

template<class IParams, int cycles>
class ASL_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 val = params.get8bit(regs, bus);
		regs->setStatus(FLAG_C, (val & 0x80) != 0);
		val <<= 1;
		setNZ(regs, val);
		params.set8bit(regs, bus, val);
		return cycles;
	}
};

template<class IParams, int cycles>
class BCC_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		return branch<IParams>(regs, bus, FLAG_C, false, cycles);
	}
};

template<class IParams, int cycles>
class BCS_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		return branch<IParams>(regs, bus, FLAG_C, true, cycles);
	}
};

template<class IParams, int cycles>
class BEQ_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		return branch<IParams>(regs, bus, FLAG_Z, true, cycles);
	}
};

template<class IParams, int cycles>
class BIT_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 val = params.get8bit(regs, bus);
		regs->setStatus(FLAG_V, (val & FLAG_V) != 0);
		regs->setStatus(FLAG_N, (val & FLAG_N) != 0);
		regs->setStatus(FLAG_Z, (val & regs->a) == 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class BMI_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		return branch<IParams>(regs, bus, FLAG_N, true, cycles);
	}
};

template<class IParams, int cycles>
class BNE_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		return branch<IParams>(regs, bus, FLAG_Z, false, cycles);
	}
};

template<class IParams, int cycles>
class BPL_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		return branch<IParams>(regs, bus, FLAG_N, false, cycles);
	}
};

template<class IParams, int cycles>
class BRK_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		// PC already points past the opcode; BRK skips one more byte.
		push16(regs, bus, regs->pc + 1);
		push8(regs, bus, regs->sr | FLAG_B | FLAG__);
		regs->setStatus(FLAG_I, true);
		regs->pc = bus->read(0xFFFE) | (bus->read(0xFFFF) << 8);
		return cycles;
	}
};

template<class IParams, int cycles>
class BVC_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		return branch<IParams>(regs, bus, FLAG_V, false, cycles);
	}
};

template<class IParams, int cycles>
class BVS_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		return branch<IParams>(regs, bus, FLAG_V, true, cycles);
	}
};

template<class IParams, int cycles>
class CLC_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_C, false);
		return cycles;
	}
};

template<class IParams, int cycles>
class CLD_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_D, false);
		return cycles;
	}
};

template<class IParams, int cycles>
class CLI_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_I, false);
		return cycles;
	}
};

template<class IParams, int cycles>
class CLV_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_V, false);
		return cycles;
	}
};

template<class IParams, int cycles>
class CMP_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		cmpCore(regs, regs->a, params.get8bit(regs, bus));
		return cycles + params.extraCycles();
	}
};

template<class IParams, int cycles>
class CPX_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		cmpCore(regs, regs->x, params.get8bit(regs, bus));
		return cycles;
	}
};

template<class IParams, int cycles>
class CPY_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		cmpCore(regs, regs->y, params.get8bit(regs, bus));
		return cycles;
	}
};

template<class IParams, int cycles>
class DEC_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 val = params.get8bit(regs, bus) - 1;
		params.set8bit(regs, bus, val);
		setNZ(regs, val);
		return cycles;
	}
};

template<class IParams, int cycles>
class DEX_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->x--;
		setNZ(regs, regs->x);
		return cycles;
	}
};

template<class IParams, int cycles>
class DEY_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->y--;
		setNZ(regs, regs->y);
		return cycles;
	}
};

template<class IParams, int cycles>
class EOR_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		regs->a ^= params.get8bit(regs, bus);
		setNZ(regs, regs->a);
		return cycles + params.extraCycles();
	}
};

template<class IParams, int cycles>
class INC_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 val = params.get8bit(regs, bus) + 1;
		params.set8bit(regs, bus, val);
		setNZ(regs, val);
		return cycles;
	}
};

template<class IParams, int cycles>
class INX_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->x++;
		setNZ(regs, regs->x);
		return cycles;
	}
};

template<class IParams, int cycles>
class INY_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->y++;
		setNZ(regs, regs->y);
		return cycles;
	}
};

template<class IParams, int cycles>
class JMP_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		regs->pc = params.get16bit(regs, bus);
		return cycles;
	}
};

template<class IParams, int cycles>
class JSR_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint16 ad = params.get16bit(regs, bus);
		push16(regs, bus, regs->pc - 1);
		regs->pc = ad;
		return cycles;
	}
};

template<class IParams, int cycles>
class LDA_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		regs->a = params.get8bit(regs, bus);
		setNZ(regs, regs->a);
		return cycles + params.extraCycles();
	}
};

template<class IParams, int cycles>
class LDX_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		regs->x = params.get8bit(regs, bus);
		setNZ(regs, regs->x);
		return cycles + params.extraCycles();
	}
};

template<class IParams, int cycles>
class LDY_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		regs->y = params.get8bit(regs, bus);
		setNZ(regs, regs->y);
		return cycles + params.extraCycles();
	}
};

template<class IParams, int cycles>
class LSR_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 val = params.get8bit(regs, bus);
		regs->setStatus(FLAG_C, (val & 0x01) != 0);
		val >>= 1;
		setNZ(regs, val);
		params.set8bit(regs, bus, val);
		return cycles;
	}
};

template<class IParams, int cycles>
class NOP_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		// Consumes its operand bytes, which matters for the undocumented
		// multi-byte NOPs -- otherwise PC would land inside them.
		IParams params;
		params.init(regs, bus);
		return cycles;
	}
};

/**
 * Undocumented NOP that still performs its memory read.
 *
 * The read is observable on a bus with memory-mapped I/O, and the addressed
 * forms pay the page-crossing penalty.
 */
template<class IParams, int cycles>
class NOPR_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		params.get8bit(regs, bus);
		return cycles + params.extraCycles();
	}
};

template<class IParams, int cycles>
class ORA_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		regs->a |= params.get8bit(regs, bus);
		setNZ(regs, regs->a);
		return cycles + params.extraCycles();
	}
};

template<class IParams, int cycles>
class PHA_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		push8(regs, bus, regs->a);
		return cycles;
	}
};

template<class IParams, int cycles>
class PHP_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		push8(regs, bus, regs->sr | FLAG_B | FLAG__);
		return cycles;
	}
};

template<class IParams, int cycles>
class PLA_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->a = pop8(regs, bus);
		setNZ(regs, regs->a);
		return cycles;
	}
};

template<class IParams, int cycles>
class PLP_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		// B is not a real register bit; bit 5 always reads as set.
		regs->sr = (pop8(regs, bus) & ~FLAG_B) | FLAG__;
		return cycles;
	}
};

template<class IParams, int cycles>
class ROL_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 old = params.get8bit(regs, bus);
		uint8 val = (old << 1) | (regs->getStatus(FLAG_C) ? 1 : 0);
		regs->setStatus(FLAG_C, (old & 0x80) != 0);
		setNZ(regs, val);
		params.set8bit(regs, bus, val);
		return cycles;
	}
};

template<class IParams, int cycles>
class ROR_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 old = params.get8bit(regs, bus);
		uint8 val = (old >> 1) | (regs->getStatus(FLAG_C) ? 0x80 : 0);
		regs->setStatus(FLAG_C, (old & 0x01) != 0);
		setNZ(regs, val);
		params.set8bit(regs, bus, val);
		return cycles;
	}
};

template<class IParams, int cycles>
class RTI_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->sr = (pop8(regs, bus) & ~FLAG_B) | FLAG__;
		regs->pc = pop16(regs, bus);
		return cycles;
	}
};

template<class IParams, int cycles>
class RTS_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->pc = pop16(regs, bus) + 1;
		return cycles;
	}
};

template<class IParams, int cycles>
class SBC_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		sbcCore(regs, params.get8bit(regs, bus));
		return cycles + params.extraCycles();
	}
};

template<class IParams, int cycles>
class SEC_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_C, true);
		return cycles;
	}
};

template<class IParams, int cycles>
class SED_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_D, true);
		return cycles;
	}
};

template<class IParams, int cycles>
class SEI_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_I, true);
		return cycles;
	}
};

template<class IParams, int cycles>
class STA_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		params.set8bit(regs, bus, regs->a);
		return cycles;
	}
};

template<class IParams, int cycles>
class STX_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		params.set8bit(regs, bus, regs->x);
		return cycles;
	}
};

template<class IParams, int cycles>
class STY_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		params.set8bit(regs, bus, regs->y);
		return cycles;
	}
};

template<class IParams, int cycles>
class TAX_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->x = regs->a;
		setNZ(regs, regs->x);
		return cycles;
	}
};

template<class IParams, int cycles>
class TAY_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->y = regs->a;
		setNZ(regs, regs->y);
		return cycles;
	}
};

template<class IParams, int cycles>
class TSX_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->x = regs->sp;
		setNZ(regs, regs->x);
		return cycles;
	}
};

template<class IParams, int cycles>
class TXA_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->a = regs->x;
		setNZ(regs, regs->a);
		return cycles;
	}
};

template<class IParams, int cycles>
class TXS_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->sp = regs->x; // does not affect flags
		return cycles;
	}
};

template<class IParams, int cycles>
class TYA_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->a = regs->y;
		setNZ(regs, regs->a);
		return cycles;
	}
};

/* ------------------------------------------------------------------------- */
/* Undocumented instructions                                                  */
/*                                                                            */
/* Not part of the official instruction set, but stable across NMOS 6502 parts */
/* and used by real software. Names follow the common convention.             */
/* ------------------------------------------------------------------------- */

/** LAX: load A and X from memory. */
template<class IParams, int cycles>
class LAX_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		regs->a = regs->x = params.get8bit(regs, bus);
		setNZ(regs, regs->a);
		return cycles + params.extraCycles();
	}
};

/** SAX: store A AND X. Affects no flags. */
template<class IParams, int cycles>
class SAX_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		params.set8bit(regs, bus, regs->a & regs->x);
		return cycles;
	}
};

/** DCP: decrement memory, then compare with A. */
template<class IParams, int cycles>
class DCP_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 val = params.get8bit(regs, bus) - 1;
		params.set8bit(regs, bus, val);
		cmpCore(regs, regs->a, val);
		return cycles;
	}
};

/** ISC (also ISB/INS): increment memory, then subtract it from A. */
template<class IParams, int cycles>
class ISC_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 val = params.get8bit(regs, bus) + 1;
		params.set8bit(regs, bus, val);
		sbcCore(regs, val);
		return cycles;
	}
};

/** SLO (also ASO): shift memory left, then OR into A. */
template<class IParams, int cycles>
class SLO_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 val = params.get8bit(regs, bus);
		regs->setStatus(FLAG_C, (val & 0x80) != 0);
		val <<= 1;
		params.set8bit(regs, bus, val);
		regs->a |= val;
		setNZ(regs, regs->a);
		return cycles;
	}
};

/** RLA: rotate memory left, then AND into A. */
template<class IParams, int cycles>
class RLA_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 old = params.get8bit(regs, bus);
		uint8 val = (old << 1) | (regs->getStatus(FLAG_C) ? 1 : 0);
		regs->setStatus(FLAG_C, (old & 0x80) != 0);
		params.set8bit(regs, bus, val);
		regs->a &= val;
		setNZ(regs, regs->a);
		return cycles;
	}
};

/** SRE (also LSE): shift memory right, then EOR into A. */
template<class IParams, int cycles>
class SRE_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 val = params.get8bit(regs, bus);
		regs->setStatus(FLAG_C, (val & 0x01) != 0);
		val >>= 1;
		params.set8bit(regs, bus, val);
		regs->a ^= val;
		setNZ(regs, regs->a);
		return cycles;
	}
};

/** RRA: rotate memory right, then add it to A with carry. */
template<class IParams, int cycles>
class RRA_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 old = params.get8bit(regs, bus);
		uint8 val = (old >> 1) | (regs->getStatus(FLAG_C) ? 0x80 : 0);
		regs->setStatus(FLAG_C, (old & 0x01) != 0);
		params.set8bit(regs, bus, val);
		adcCore(regs, val);
		return cycles;
	}
};

/** ANC: AND immediate, then copy bit 7 of the result into carry. */
template<class IParams, int cycles>
class ANC_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		regs->a &= params.get8bit(regs, bus);
		setNZ(regs, regs->a);
		regs->setStatus(FLAG_C, (regs->a & 0x80) != 0);
		return cycles;
	}
};

/** ALR (also ASR): AND immediate, then shift A right. */
template<class IParams, int cycles>
class ALR_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 val = regs->a & params.get8bit(regs, bus);
		regs->setStatus(FLAG_C, (val & 0x01) != 0);
		regs->a = val >> 1;
		setNZ(regs, regs->a);
		return cycles;
	}
};

/**
 * ARR: AND immediate, then rotate A right -- with its own flag rules.
 *
 * Carry comes from bit 6 of the result and overflow from bit 6 XOR bit 5. The
 * decimal-mode variant is genuinely strange and is not modelled here; the 2A03
 * has decimal disabled, so this is the form that matters in practice.
 */
template<class IParams, int cycles>
class ARR_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 val = regs->a & params.get8bit(regs, bus);
		regs->a = (val >> 1) | (regs->getStatus(FLAG_C) ? 0x80 : 0);
		setNZ(regs, regs->a);
		regs->setStatus(FLAG_C, (regs->a & 0x40) != 0);
		regs->setStatus(FLAG_V, (((regs->a >> 6) ^ (regs->a >> 5)) & 0x01) != 0);
		return cycles;
	}
};

/** SBX (also AXS): X = (A AND X) - immediate, without borrow. */
template<class IParams, int cycles>
class SBX_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 val = params.get8bit(regs, bus);
		uint8 lhs = regs->a & regs->x;
		regs->setStatus(FLAG_C, lhs >= val);
		regs->x = lhs - val;
		setNZ(regs, regs->x);
		return cycles;
	}
};

/** LAS (also LAR): A = X = SP = memory AND SP. */
template<class IParams, int cycles>
class LAS_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 val = params.get8bit(regs, bus) & regs->sp;
		regs->a = regs->x = regs->sp = val;
		setNZ(regs, val);
		return cycles + params.extraCycles();
	}
};

/**
 * SHA/SHX/SHY/TAS family: store a register ANDed with (address high byte + 1).
 *
 * Genuinely unstable on hardware -- the AND with H+1 is dropped when the index
 * carries into a new page, and behaviour varies between chips. This implements
 * the widely used deterministic approximation. Software that relies on these is
 * vanishingly rare.
 *
 * @param Reg 0 = A AND X (SHA), 1 = X (SHX), 2 = Y (SHY), 3 = A AND X into SP too (TAS)
 */
template<class IParams, int cycles, int Reg>
class SHx_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		uint8 src;
		switch (Reg) {
		case 1:  src = regs->x; break;
		case 2:  src = regs->y; break;
		default: src = regs->a & regs->x; break;
		}
		if (Reg == 3)
			regs->sp = regs->a & regs->x;
		uint8 hi = (uint8) ((params.base(regs, bus) >> 8) + 1);
		params.set8bit(regs, bus, src & hi);
		return cycles;
	}
};

/**
 * XAA (also ANE): A = (A OR magic) AND X AND immediate.
 *
 * The "magic" value depends on analog conditions on the die and differs between
 * chips and even between runs. $EE is the value most emulators settle on. Do not
 * rely on this instruction.
 */
template<class IParams, int cycles>
class XAA_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		IParams params;
		params.init(regs, bus);
		regs->a = (regs->a | 0xEE) & regs->x & params.get8bit(regs, bus);
		setNZ(regs, regs->a);
		return cycles;
	}
};

/**
 * KIL (also JAM/HLT): locks the processor up until reset.
 *
 * Modelled by rewinding PC onto the opcode, so the CPU spins on it forever --
 * which is what the hardware does, and what any "PC stopped advancing" trap
 * detector will report.
 */
template<class IParams, int cycles>
class KIL_Impl: public BaseInstruction {
public:
	virtual int execute(Registers *regs, Bus *bus) {
		regs->pc--;
		return cycles;
	}
};

#endif // INSTRUCTION_IMPL_H
