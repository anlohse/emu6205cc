#ifndef INSTRUCTION_IMPL_H
#define INSTRUCTION_IMPL_H

#include "../include/6502cc/emu_base.h"
#include "../include/6502cc/emu_memory.h"


void push8(Registers *regs, Bus *bus, uint8 val);
void push16(Registers *regs, Bus *bus, uint16 val);
uint8 pop8(Registers *regs, Bus *bus);
uint16 pop16(Registers *regs, Bus *bus);

template<class IParams, int cycles>
class ADC_Impl: public BaseInstruction {
private:
	IParams params;
public:
	ADC_Impl() {
	}
	virtual ~ADC_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = params.get8bit(regs, bus);
		uint16 res = regs->a + val + (regs->getStatus(FLAG_C) ? 1 : 0);
		regs->setStatus(FLAG_Z, (res & 0xff) == 0);
		if (regs->getStatus(FLAG_D)) {
			if (((regs->a & 0xF) + (val & 0xF) + (regs->getStatus(FLAG_C) ? 1 : 0)) > 9)
				res += 6;
			regs->setStatus(FLAG_N, res & 0x80);
			regs->setStatus(FLAG_V, !((regs->a ^ val) & 0x80) && ((regs->a ^ res) & 0x80));
			if (res > 0x99) {
				res += 96;
			}
			regs->setStatus(FLAG_C, res > 0x99);
		} else {
			regs->setStatus(FLAG_V, (~((uint16) regs->a ^ (uint16) val) & ((uint16) regs->a ^ res)) & 0x80);
			regs->setStatus(FLAG_C, res > 0x00ff);
			regs->setStatus(FLAG_N, (res & 0x80) != 0);
		}
		regs->a = res & 0xff;
		return cycles;
	}
};

template<class IParams, int cycles>
class AND_Impl: public BaseInstruction {
private:
	IParams params;
public:
	AND_Impl() {
	}
	virtual ~AND_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = params.get8bit(regs, bus);
		regs->a &= val;
		val = regs->a;
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		regs->setStatus(FLAG_Z, val == 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class ASL_Impl: public BaseInstruction {
private:
	IParams params;
public:
	ASL_Impl() {
	}
	virtual ~ASL_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = params.get8bit(regs, bus);
		regs->setStatus(FLAG_C, (val & 0x80) != 0);
		val <<= 1;
		val &= 0xfe;
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		regs->setStatus(FLAG_Z, val == 0);
		params.set8bit(regs,bus,val);
		return cycles;
	}
};

template <class ParamType>
int branch(Registers *regs, Bus *bus, ParamType params, uint8 flag, bool set) {
	params.init(regs,bus);
	uint16 val = params.get8bit(regs, bus);
	int xc = 0;
	if (regs->getStatus(flag) == set) {
		if (val & 0x80) val |= 0xff00;
		val = val + regs->pc;
		if ((regs->pc & 0xff00) != (val & 0xff00)) xc += 2;
		regs->pc = val;
	}
	return xc;
}

template<class IParams, int cycles>
class BCC_Impl: public BaseInstruction {
private:
	IParams params;
public:
	BCC_Impl() {
	}
	virtual ~BCC_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		return branch(regs, bus, params, FLAG_C, false);
	}
};

template<class IParams, int cycles>
class BCS_Impl: public BaseInstruction {
private:
	IParams params;
public:
	BCS_Impl() {
	}
	virtual ~BCS_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		return branch(regs, bus, params, FLAG_C, true);
	}
};

template<class IParams, int cycles>
class BEQ_Impl: public BaseInstruction {
private:
	IParams params;
public:
	BEQ_Impl() {
	}
	virtual ~BEQ_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		return branch(regs, bus, params, FLAG_Z, true);
	}
};

template<class IParams, int cycles>
class BIT_Impl: public BaseInstruction {
private:
	IParams params;
public:
	BIT_Impl() {
	}
	virtual ~BIT_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = params.get8bit(regs, bus);
		regs->setStatus(FLAG_V, (val & FLAG_V) != 0);
		regs->setStatus(FLAG_N, (val & FLAG_N) != 0);
		val &= regs->a;
		regs->setStatus(FLAG_Z, val == 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class BMI_Impl: public BaseInstruction {
private:
	IParams params;
public:
	BMI_Impl() {
	}
	virtual ~BMI_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		return branch(regs, bus, params, FLAG_N, true);
	}
};

template<class IParams, int cycles>
class BNE_Impl: public BaseInstruction {
private:
	IParams params;
public:
	BNE_Impl() {
	}
	virtual ~BNE_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		return branch(regs, bus, params, FLAG_Z, false);
	}
};

template<class IParams, int cycles>
class BPL_Impl: public BaseInstruction {
private:
	IParams params;
public:
	BPL_Impl() {
	}
	virtual ~BPL_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		return branch(regs, bus, params, FLAG_N, false);
	}
};

template<class IParams, int cycles>
class BRK_Impl: public BaseInstruction {
private:
	IParams params;
public:
	BRK_Impl() {
	}
	virtual ~BRK_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		push16(regs, bus, regs->pc+1);
		push8(regs, bus, regs->sr | FLAG_B);
		regs->setStatus(FLAG_I, true);
		regs->pc = bus->read(0xFFFE) | (bus->read(0xFFFF) << 8);
		return cycles;
	}
};

template<class IParams, int cycles>
class BVC_Impl: public BaseInstruction {
private:
	IParams params;
public:
	BVC_Impl() {
	}
	virtual ~BVC_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		return branch(regs, bus, params, FLAG_V, false);
	}
};

template<class IParams, int cycles>
class BVS_Impl: public BaseInstruction {
private:
	IParams params;
public:
	BVS_Impl() {
	}
	virtual ~BVS_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		return branch(regs, bus, params, FLAG_V, true);
	}
};

template<class IParams, int cycles>
class CLC_Impl: public BaseInstruction {
private:
	IParams params;
public:
	CLC_Impl() {
	}
	virtual ~CLC_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_C, false);
		return cycles;
	}
};

template<class IParams, int cycles>
class CLD_Impl: public BaseInstruction {
private:
	IParams params;
public:
	CLD_Impl() {
	}
	virtual ~CLD_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_D, false);
		return cycles;
	}
};

template<class IParams, int cycles>
class CLI_Impl: public BaseInstruction {
private:
	IParams params;
public:
	CLI_Impl() {
	}
	virtual ~CLI_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_I, false);
		return cycles;
	}
};

template<class IParams, int cycles>
class CLV_Impl: public BaseInstruction {
private:
	IParams params;
public:
	CLV_Impl() {
	}
	virtual ~CLV_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_V, false);
		return cycles;
	}
};

template<class IParams, int cycles>
class CMP_Impl: public BaseInstruction {
private:
	IParams params;
public:
	CMP_Impl() {
	}
	virtual ~CMP_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = params.get8bit(regs, bus);
		uint16 r = (uint16)regs->a - (uint16)val;
		regs->setStatus(FLAG_C, r < 0x100);
		regs->setStatus(FLAG_Z, r == 0);
		regs->setStatus(FLAG_N, (r & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class CPX_Impl: public BaseInstruction {
private:
	IParams params;
public:
	CPX_Impl() {
	}
	virtual ~CPX_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = params.get8bit(regs, bus);
		uint16 r = (uint16)regs->x - (uint16)val;
		regs->setStatus(FLAG_C, r < 0x100);
		regs->setStatus(FLAG_Z, r == 0);
		regs->setStatus(FLAG_N, (r & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class CPY_Impl: public BaseInstruction {
private:
	IParams params;
public:
	CPY_Impl() {
	}
	virtual ~CPY_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = params.get8bit(regs, bus);
		uint16 r = (uint16)regs->y - (uint16)val;
		regs->setStatus(FLAG_C, r < 0x100);
		regs->setStatus(FLAG_Z, r == 0);
		regs->setStatus(FLAG_N, (r & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class DEC_Impl: public BaseInstruction {
private:
	IParams params;
public:
	DEC_Impl() {
	}
	virtual ~DEC_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint16 val = params.get8bit(regs, bus);
		val--;
		params.set8bit(regs, bus, val & 0xff);
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class DEX_Impl: public BaseInstruction {
private:
	IParams params;
public:
	DEX_Impl() {
	}
	virtual ~DEX_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		uint16 val = regs->x;
		val--;
		regs->x = val & 0xff;
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class DEY_Impl: public BaseInstruction {
private:
	IParams params;
public:
	DEY_Impl() {
	}
	virtual ~DEY_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		uint16 val = regs->y;
		val--;
		regs->y = val & 0xff;
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class EOR_Impl: public BaseInstruction {
private:
	IParams params;
public:
	EOR_Impl() {
	}
	virtual ~EOR_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = regs->a ^ params.get8bit(regs, bus);
		regs->a = val;
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class INC_Impl: public BaseInstruction {
private:
	IParams params;
public:
	INC_Impl() {
	}
	virtual ~INC_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = params.get8bit(regs, bus);
		val++;
		params.set8bit(regs, bus, val);
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class INX_Impl: public BaseInstruction {
private:
	IParams params;
public:
	INX_Impl() {
	}
	virtual ~INX_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		uint8 val = regs->x;
		val++;
		regs->x = val;
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class INY_Impl: public BaseInstruction {
private:
	IParams params;
public:
	INY_Impl() {
	}
	virtual ~INY_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		uint8 val = regs->y;
		val++;
		regs->y = val;
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class JMP_Impl: public BaseInstruction {
private:
	IParams params;
public:
	JMP_Impl() {
	}
	virtual ~JMP_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint16 ad = params.get16bit(regs, bus);
		regs->pc = ad;
		return cycles;
	}
};

template<class IParams, int cycles>
class JSR_Impl: public BaseInstruction {
private:
	IParams params;
public:
	JSR_Impl() {
	}
	virtual ~JSR_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint16 ad = params.get16bit(regs, bus);
		push16(regs,bus,regs->pc-1);
		regs->pc = ad;
		return cycles;
	}
};

template<class IParams, int cycles>
class LDA_Impl: public BaseInstruction {
private:
	IParams params;
public:
	LDA_Impl() {
	}
	virtual ~LDA_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = params.get8bit(regs, bus);
		regs->a = val;
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class LDX_Impl: public BaseInstruction {
private:
	IParams params;
public:
	LDX_Impl() {
	}
	virtual ~LDX_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = params.get8bit(regs, bus);
		regs->x = val;
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class LDY_Impl: public BaseInstruction {
private:
	IParams params;
public:
	LDY_Impl() {
	}
	virtual ~LDY_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = params.get8bit(regs, bus);
		regs->y = val;
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class LSR_Impl: public BaseInstruction {
private:
	IParams params;
public:
	LSR_Impl() {
	}
	virtual ~LSR_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = params.get8bit(regs, bus);
		regs->setStatus(FLAG_C, (val & 0x01) != 0);
		val >>= 1;
		val &= 0x7f;
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, false);
		params.set8bit(regs,bus,val);
		return cycles;
	}
};

template<class IParams, int cycles>
class NOP_Impl: public BaseInstruction {
private:
	IParams params;
public:
	NOP_Impl() {
	}
	virtual ~NOP_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		return cycles;
	}
};

template<class IParams, int cycles>
class ORA_Impl: public BaseInstruction {
private:
	IParams params;
public:
	ORA_Impl() {
	}
	virtual ~ORA_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = regs->a | params.get8bit(regs, bus);
		regs->a = val;
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class PHA_Impl: public BaseInstruction {
private:
	IParams params;
public:
	PHA_Impl() {
	}
	virtual ~PHA_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		push8(regs, bus, regs->a);
		return cycles;
	}
};

template<class IParams, int cycles>
class PHP_Impl: public BaseInstruction {
private:
	IParams params;
public:
	PHP_Impl() {
	}
	virtual ~PHP_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		push8(regs, bus, regs->sr | FLAG_B);
		return cycles;
	}
};

template<class IParams, int cycles>
class PLA_Impl: public BaseInstruction {
private:
	IParams params;
public:
	PLA_Impl() {
	}
	virtual ~PLA_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		regs->a = pop8(regs, bus);
		regs->setStatus(FLAG_Z, regs->a == 0);
		regs->setStatus(FLAG_N, (regs->a & 0x80) != 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class PLP_Impl: public BaseInstruction {
private:
	IParams params;
public:
	PLP_Impl() {
	}
	virtual ~PLP_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		regs->sr = pop8(regs, bus);
		regs->setStatus(FLAG__, true);
		return cycles;
	}
};

template<class IParams, int cycles>
class ROL_Impl: public BaseInstruction {
private:
	IParams params;
public:
	ROL_Impl() {
	}
	virtual ~ROL_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 old, val;
		old = val = params.get8bit(regs, bus);
		val = (val << 1) | (regs->getStatus(FLAG_C) ? 1 : 0);
		regs->setStatus(FLAG_C, (old & 0x80) != 0);
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		params.set8bit(regs,bus,val);
		return cycles;
	}
};

template<class IParams, int cycles>
class ROR_Impl: public BaseInstruction {
private:
	IParams params;
public:
	ROR_Impl() {
	}
	virtual ~ROR_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 old,val;
		old = val = params.get8bit(regs, bus);
		val = (val >> 1) | (regs->getStatus(FLAG_C) ? 0x80 : 0);
		regs->setStatus(FLAG_C, (old & 0x01) != 0);
		regs->setStatus(FLAG_Z, val == 0);
		regs->setStatus(FLAG_N, (val & 0x80) != 0);
		params.set8bit(regs,bus,val);
		return cycles;
	}
};

template<class IParams, int cycles>
class RTI_Impl: public BaseInstruction {
private:
	IParams params;
public:
	RTI_Impl() {
	}
	virtual ~RTI_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		regs->sr = pop8(regs, bus);
		regs->pc = pop16(regs, bus);
		regs->setStatus(FLAG__, true);
		return cycles;
	}
};

template<class IParams, int cycles>
class RTS_Impl: public BaseInstruction {
private:
	IParams params;
public:
	RTS_Impl() {
	}
	virtual ~RTS_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		regs->pc = pop16(regs, bus)+1;
		return cycles;
	}
};

template<class IParams, int cycles>
class SBC_Impl: public BaseInstruction {
private:
	IParams params;
public:
	SBC_Impl() {
	}
	virtual ~SBC_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs,bus);
		uint8 val = params.get8bit(regs, bus);
		uint16 res = regs->a - val - (regs->getStatus(FLAG_C) ? 0 : 1);
		regs->setStatus(FLAG_V, (((uint16)regs->a ^ (uint16)val) & ((uint16)regs->a ^ res)) & 0x80);
		regs->setStatus(FLAG_N, (res & 0x80) != 0);
		regs->setStatus(FLAG_Z, (res & 0xff) == 0);
		if (regs->getStatus(FLAG_D)) {
			if (((regs->a & 0x0F) - (regs->getStatus(FLAG_C) ? 0 : 1)) < (val & 0x0F))
				res -= 6;
			if (res > 0x99) {
				res -= 0x60;
			}
		}
		regs->a = res & 0xff;
		regs->setStatus(FLAG_C, res < 0x100);
		return cycles;
	}
};

template<class IParams, int cycles>
class SEC_Impl: public BaseInstruction {
private:
	IParams params;
public:
	SEC_Impl() {
	}
	virtual ~SEC_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_C, true);
		return cycles;
	}
};

template<class IParams, int cycles>
class SED_Impl: public BaseInstruction {
private:
	IParams params;
public:
	SED_Impl() {
	}
	virtual ~SED_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_D, true);
		return cycles;
	}
};

template<class IParams, int cycles>
class SEI_Impl: public BaseInstruction {
private:
	IParams params;
public:
	SEI_Impl() {
	}
	virtual ~SEI_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		regs->setStatus(FLAG_I, true);
		return cycles;
	}
};

template<class IParams, int cycles>
class STA_Impl: public BaseInstruction {
private:
	IParams params;
public:
	STA_Impl() {
	}
	virtual ~STA_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs, bus);
		params.set8bit(regs, bus, regs->a);
		return cycles;
	}
};

template<class IParams, int cycles>
class STX_Impl: public BaseInstruction {
private:
	IParams params;
public:
	STX_Impl() {
	}
	virtual ~STX_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs, bus);
		params.set8bit(regs, bus, regs->x);
		return cycles;
	}
};

template<class IParams, int cycles>
class STY_Impl: public BaseInstruction {
private:
	IParams params;
public:
	STY_Impl() {
	}
	virtual ~STY_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		params.init(regs, bus);
		params.set8bit(regs, bus, regs->y);
		return cycles;
	}
};

template<class IParams, int cycles>
class TAX_Impl: public BaseInstruction {
private:
	IParams params;
public:
	TAX_Impl() {
	}
	virtual ~TAX_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		uint8 res = regs->a;
		regs->x = res;
		regs->setStatus(FLAG_N, (res & 0x80) != 0);
		regs->setStatus(FLAG_Z, res == 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class TAY_Impl: public BaseInstruction {
private:
	IParams params;
public:
	TAY_Impl() {
	}
	virtual ~TAY_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		uint8 res = regs->a;
		regs->y = res;
		regs->setStatus(FLAG_N, (res & 0x80) != 0);
		regs->setStatus(FLAG_Z, res == 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class TSX_Impl: public BaseInstruction {
private:
	IParams params;
public:
	TSX_Impl() {
	}
	virtual ~TSX_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		uint8 res = regs->sp;
		regs->x = res;
		regs->setStatus(FLAG_N, (res & 0x80) != 0);
		regs->setStatus(FLAG_Z, res == 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class TXA_Impl: public BaseInstruction {
private:
	IParams params;
public:
	TXA_Impl() {
	}
	virtual ~TXA_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		uint8 res = regs->x;
		regs->a = res;
		regs->setStatus(FLAG_N, (res & 0x80) != 0);
		regs->setStatus(FLAG_Z, res == 0);
		return cycles;
	}
};

template<class IParams, int cycles>
class TXS_Impl: public BaseInstruction {
private:
	IParams params;
public:
	TXS_Impl() {
	}
	virtual ~TXS_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		regs->sp = regs->x;
		return cycles;
	}
};

template<class IParams, int cycles>
class TYA_Impl: public BaseInstruction {
private:
	IParams params;
public:
	TYA_Impl() {
	}
	virtual ~TYA_Impl() {
	}
	virtual int execute(Registers *regs, Bus *bus) {
		uint8 res = regs->y;
		regs->a = res;
		regs->setStatus(FLAG_N, (res & 0x80) != 0);
		regs->setStatus(FLAG_Z, res == 0);
		return cycles;
	}
};

#endif // INSTRUCTION_IMPL_H
