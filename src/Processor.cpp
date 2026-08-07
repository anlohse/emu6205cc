#include "../include/6502cc/emu_processor.h"
#include "parameters.h"
#include "InstructionImpl.h"

Processor::Processor(Bus* _bus, Registers* _regs, emu_clock* _clock): p_bus(_bus), p_regs(_regs), p_clock(_clock),
		m_running(false), m_stopping(false), m_nmi_pending(false), m_irq_line(false), p_instr_callback(nullptr) {
}

Processor::~Processor() {
}

/*
 * The opcode matrix.
 *
 * All 256 opcodes are covered. Entries marked "undocumented" are not part of the
 * official instruction set but are stable on NMOS parts and used by real
 * software; see InstructionImpl.h for the ones whose behaviour is only
 * approximated.
 *
 * The cycle count is the base cost. Read instructions add one more when an
 * indexed address crosses a page, and branches add one when taken and one more
 * when the target is in another page -- both handled at execution time.
 */
BaseInstruction* Processor::_instruction_table[256] = {
	/* 0x00 */ new BRK_Impl <      NullParams, 7>(),
	/* 0x01 */ new ORA_Impl < IndirectXParams, 6>(),
	/* 0x02 */ new KIL_Impl <      NullParams, 3>(),  // undocumented
	/* 0x03 */ new SLO_Impl < IndirectXParams, 8>(),  // undocumented
	/* 0x04 */ new NOPR_Impl<  ZeroPageParams, 3>(),  // undocumented
	/* 0x05 */ new ORA_Impl <  ZeroPageParams, 3>(),
	/* 0x06 */ new ASL_Impl <  ZeroPageParams, 5>(),
	/* 0x07 */ new SLO_Impl <  ZeroPageParams, 5>(),  // undocumented
	/* 0x08 */ new PHP_Impl <      NullParams, 3>(),
	/* 0x09 */ new ORA_Impl < ImmediateParams, 2>(),
	/* 0x0A */ new ASL_Impl <AccumulatorParams,2>(),
	/* 0x0B */ new ANC_Impl < ImmediateParams, 2>(),  // undocumented
	/* 0x0C */ new NOPR_Impl<  AbsoluteParams, 4>(),  // undocumented
	/* 0x0D */ new ORA_Impl <  AbsoluteParams, 4>(),
	/* 0x0E */ new ASL_Impl <  AbsoluteParams, 6>(),
	/* 0x0F */ new SLO_Impl <  AbsoluteParams, 6>(),  // undocumented

	/* 0x10 */ new BPL_Impl <  RelativeParams, 2>(),
	/* 0x11 */ new ORA_Impl < IndirectYParams, 5>(),
	/* 0x12 */ new KIL_Impl <      NullParams, 3>(),  // undocumented
	/* 0x13 */ new SLO_Impl < IndirectYParams, 8>(),  // undocumented
	/* 0x14 */ new NOPR_Impl< ZeroPageXParams, 4>(),  // undocumented
	/* 0x15 */ new ORA_Impl < ZeroPageXParams, 4>(),
	/* 0x16 */ new ASL_Impl < ZeroPageXParams, 6>(),
	/* 0x17 */ new SLO_Impl < ZeroPageXParams, 6>(),  // undocumented
	/* 0x18 */ new CLC_Impl <      NullParams, 2>(),
	/* 0x19 */ new ORA_Impl < AbsoluteYParams, 4>(),
	/* 0x1A */ new NOP_Impl <      NullParams, 2>(),  // undocumented
	/* 0x1B */ new SLO_Impl < AbsoluteYParams, 7>(),  // undocumented
	/* 0x1C */ new NOPR_Impl< AbsoluteXParams, 4>(),  // undocumented
	/* 0x1D */ new ORA_Impl < AbsoluteXParams, 4>(),
	/* 0x1E */ new ASL_Impl < AbsoluteXParams, 7>(),
	/* 0x1F */ new SLO_Impl < AbsoluteXParams, 7>(),  // undocumented

	/* 0x20 */ new JSR_Impl <  AbsConstParams, 6>(),
	/* 0x21 */ new AND_Impl < IndirectXParams, 6>(),
	/* 0x22 */ new KIL_Impl <      NullParams, 3>(),  // undocumented
	/* 0x23 */ new RLA_Impl < IndirectXParams, 8>(),  // undocumented
	/* 0x24 */ new BIT_Impl <  ZeroPageParams, 3>(),
	/* 0x25 */ new AND_Impl <  ZeroPageParams, 3>(),
	/* 0x26 */ new ROL_Impl <  ZeroPageParams, 5>(),
	/* 0x27 */ new RLA_Impl <  ZeroPageParams, 5>(),  // undocumented
	/* 0x28 */ new PLP_Impl <      NullParams, 4>(),
	/* 0x29 */ new AND_Impl < ImmediateParams, 2>(),
	/* 0x2A */ new ROL_Impl <AccumulatorParams,2>(),
	/* 0x2B */ new ANC_Impl < ImmediateParams, 2>(),  // undocumented
	/* 0x2C */ new BIT_Impl <  AbsoluteParams, 4>(),
	/* 0x2D */ new AND_Impl <  AbsoluteParams, 4>(),
	/* 0x2E */ new ROL_Impl <  AbsoluteParams, 6>(),
	/* 0x2F */ new RLA_Impl <  AbsoluteParams, 6>(),  // undocumented

	/* 0x30 */ new BMI_Impl <  RelativeParams, 2>(),
	/* 0x31 */ new AND_Impl < IndirectYParams, 5>(),
	/* 0x32 */ new KIL_Impl <      NullParams, 3>(),  // undocumented
	/* 0x33 */ new RLA_Impl < IndirectYParams, 8>(),  // undocumented
	/* 0x34 */ new NOPR_Impl< ZeroPageXParams, 4>(),  // undocumented
	/* 0x35 */ new AND_Impl < ZeroPageXParams, 4>(),
	/* 0x36 */ new ROL_Impl < ZeroPageXParams, 6>(),
	/* 0x37 */ new RLA_Impl < ZeroPageXParams, 6>(),  // undocumented
	/* 0x38 */ new SEC_Impl <      NullParams, 2>(),
	/* 0x39 */ new AND_Impl < AbsoluteYParams, 4>(),
	/* 0x3A */ new NOP_Impl <      NullParams, 2>(),  // undocumented
	/* 0x3B */ new RLA_Impl < AbsoluteYParams, 7>(),  // undocumented
	/* 0x3C */ new NOPR_Impl< AbsoluteXParams, 4>(),  // undocumented
	/* 0x3D */ new AND_Impl < AbsoluteXParams, 4>(),
	/* 0x3E */ new ROL_Impl < AbsoluteXParams, 7>(),
	/* 0x3F */ new RLA_Impl < AbsoluteXParams, 7>(),  // undocumented

	/* 0x40 */ new RTI_Impl <      NullParams, 6>(),
	/* 0x41 */ new EOR_Impl < IndirectXParams, 6>(),
	/* 0x42 */ new KIL_Impl <      NullParams, 3>(),  // undocumented
	/* 0x43 */ new SRE_Impl < IndirectXParams, 8>(),  // undocumented
	/* 0x44 */ new NOPR_Impl<  ZeroPageParams, 3>(),  // undocumented
	/* 0x45 */ new EOR_Impl <  ZeroPageParams, 3>(),
	/* 0x46 */ new LSR_Impl <  ZeroPageParams, 5>(),
	/* 0x47 */ new SRE_Impl <  ZeroPageParams, 5>(),  // undocumented
	/* 0x48 */ new PHA_Impl <      NullParams, 3>(),
	/* 0x49 */ new EOR_Impl < ImmediateParams, 2>(),
	/* 0x4A */ new LSR_Impl <AccumulatorParams,2>(),
	/* 0x4B */ new ALR_Impl < ImmediateParams, 2>(),  // undocumented
	/* 0x4C */ new JMP_Impl <  AbsConstParams, 3>(),
	/* 0x4D */ new EOR_Impl <  AbsoluteParams, 4>(),
	/* 0x4E */ new LSR_Impl <  AbsoluteParams, 6>(),
	/* 0x4F */ new SRE_Impl <  AbsoluteParams, 6>(),  // undocumented

	/* 0x50 */ new BVC_Impl <  RelativeParams, 2>(),
	/* 0x51 */ new EOR_Impl < IndirectYParams, 5>(),
	/* 0x52 */ new KIL_Impl <      NullParams, 3>(),  // undocumented
	/* 0x53 */ new SRE_Impl < IndirectYParams, 8>(),  // undocumented
	/* 0x54 */ new NOPR_Impl< ZeroPageXParams, 4>(),  // undocumented
	/* 0x55 */ new EOR_Impl < ZeroPageXParams, 4>(),
	/* 0x56 */ new LSR_Impl < ZeroPageXParams, 6>(),
	/* 0x57 */ new SRE_Impl < ZeroPageXParams, 6>(),  // undocumented
	/* 0x58 */ new CLI_Impl <      NullParams, 2>(),
	/* 0x59 */ new EOR_Impl < AbsoluteYParams, 4>(),
	/* 0x5A */ new NOP_Impl <      NullParams, 2>(),  // undocumented
	/* 0x5B */ new SRE_Impl < AbsoluteYParams, 7>(),  // undocumented
	/* 0x5C */ new NOPR_Impl< AbsoluteXParams, 4>(),  // undocumented
	/* 0x5D */ new EOR_Impl < AbsoluteXParams, 4>(),
	/* 0x5E */ new LSR_Impl < AbsoluteXParams, 7>(),
	/* 0x5F */ new SRE_Impl < AbsoluteXParams, 7>(),  // undocumented

	/* 0x60 */ new RTS_Impl <      NullParams, 6>(),
	/* 0x61 */ new ADC_Impl < IndirectXParams, 6>(),
	/* 0x62 */ new KIL_Impl <      NullParams, 3>(),  // undocumented
	/* 0x63 */ new RRA_Impl < IndirectXParams, 8>(),  // undocumented
	/* 0x64 */ new NOPR_Impl<  ZeroPageParams, 3>(),  // undocumented
	/* 0x65 */ new ADC_Impl <  ZeroPageParams, 3>(),
	/* 0x66 */ new ROR_Impl <  ZeroPageParams, 5>(),
	/* 0x67 */ new RRA_Impl <  ZeroPageParams, 5>(),  // undocumented
	/* 0x68 */ new PLA_Impl <      NullParams, 4>(),
	/* 0x69 */ new ADC_Impl < ImmediateParams, 2>(),
	/* 0x6A */ new ROR_Impl <AccumulatorParams,2>(),
	/* 0x6B */ new ARR_Impl < ImmediateParams, 2>(),  // undocumented
	/* 0x6C */ new JMP_Impl <  IndirectParams, 5>(),
	/* 0x6D */ new ADC_Impl <  AbsoluteParams, 4>(),
	/* 0x6E */ new ROR_Impl <  AbsoluteParams, 6>(),
	/* 0x6F */ new RRA_Impl <  AbsoluteParams, 6>(),  // undocumented

	/* 0x70 */ new BVS_Impl <  RelativeParams, 2>(),
	/* 0x71 */ new ADC_Impl < IndirectYParams, 5>(),
	/* 0x72 */ new KIL_Impl <      NullParams, 3>(),  // undocumented
	/* 0x73 */ new RRA_Impl < IndirectYParams, 8>(),  // undocumented
	/* 0x74 */ new NOPR_Impl< ZeroPageXParams, 4>(),  // undocumented
	/* 0x75 */ new ADC_Impl < ZeroPageXParams, 4>(),
	/* 0x76 */ new ROR_Impl < ZeroPageXParams, 6>(),
	/* 0x77 */ new RRA_Impl < ZeroPageXParams, 6>(),  // undocumented
	/* 0x78 */ new SEI_Impl <      NullParams, 2>(),
	/* 0x79 */ new ADC_Impl < AbsoluteYParams, 4>(),
	/* 0x7A */ new NOP_Impl <      NullParams, 2>(),  // undocumented
	/* 0x7B */ new RRA_Impl < AbsoluteYParams, 7>(),  // undocumented
	/* 0x7C */ new NOPR_Impl< AbsoluteXParams, 4>(),  // undocumented
	/* 0x7D */ new ADC_Impl < AbsoluteXParams, 4>(),
	/* 0x7E */ new ROR_Impl < AbsoluteXParams, 7>(),
	/* 0x7F */ new RRA_Impl < AbsoluteXParams, 7>(),  // undocumented

	/* 0x80 */ new NOP_Impl < ImmediateParams, 2>(),  // undocumented
	/* 0x81 */ new STA_Impl < IndirectXParams, 6>(),
	/* 0x82 */ new NOP_Impl < ImmediateParams, 2>(),  // undocumented
	/* 0x83 */ new SAX_Impl < IndirectXParams, 6>(),  // undocumented
	/* 0x84 */ new STY_Impl <  ZeroPageParams, 3>(),
	/* 0x85 */ new STA_Impl <  ZeroPageParams, 3>(),
	/* 0x86 */ new STX_Impl <  ZeroPageParams, 3>(),
	/* 0x87 */ new SAX_Impl <  ZeroPageParams, 3>(),  // undocumented
	/* 0x88 */ new DEY_Impl <      NullParams, 2>(),
	/* 0x89 */ new NOP_Impl < ImmediateParams, 2>(),  // undocumented
	/* 0x8A */ new TXA_Impl <      NullParams, 2>(),
	/* 0x8B */ new XAA_Impl < ImmediateParams, 2>(),  // undocumented, unstable
	/* 0x8C */ new STY_Impl <  AbsoluteParams, 4>(),
	/* 0x8D */ new STA_Impl <  AbsoluteParams, 4>(),
	/* 0x8E */ new STX_Impl <  AbsoluteParams, 4>(),
	/* 0x8F */ new SAX_Impl <  AbsoluteParams, 4>(),  // undocumented

	/* 0x90 */ new BCC_Impl <  RelativeParams, 2>(),
	/* 0x91 */ new STA_Impl < IndirectYParams, 6>(),
	/* 0x92 */ new KIL_Impl <      NullParams, 3>(),  // undocumented
	/* 0x93 */ new SHx_Impl < IndirectYParams, 6, 0>(),  // SHA, undocumented, unstable
	/* 0x94 */ new STY_Impl < ZeroPageXParams, 4>(),
	/* 0x95 */ new STA_Impl < ZeroPageXParams, 4>(),
	/* 0x96 */ new STX_Impl < ZeroPageYParams, 4>(),
	/* 0x97 */ new SAX_Impl < ZeroPageYParams, 4>(),  // undocumented
	/* 0x98 */ new TYA_Impl <      NullParams, 2>(),
	/* 0x99 */ new STA_Impl < AbsoluteYParams, 5>(),
	/* 0x9A */ new TXS_Impl <      NullParams, 2>(),
	/* 0x9B */ new SHx_Impl < AbsoluteYParams, 5, 3>(),  // TAS, undocumented, unstable
	/* 0x9C */ new SHx_Impl < AbsoluteXParams, 5, 2>(),  // SHY, undocumented, unstable
	/* 0x9D */ new STA_Impl < AbsoluteXParams, 5>(),
	/* 0x9E */ new SHx_Impl < AbsoluteYParams, 5, 1>(),  // SHX, undocumented, unstable
	/* 0x9F */ new SHx_Impl < AbsoluteYParams, 5, 0>(),  // SHA, undocumented, unstable

	/* 0xA0 */ new LDY_Impl < ImmediateParams, 2>(),
	/* 0xA1 */ new LDA_Impl < IndirectXParams, 6>(),
	/* 0xA2 */ new LDX_Impl < ImmediateParams, 2>(),
	/* 0xA3 */ new LAX_Impl < IndirectXParams, 6>(),  // undocumented
	/* 0xA4 */ new LDY_Impl <  ZeroPageParams, 3>(),
	/* 0xA5 */ new LDA_Impl <  ZeroPageParams, 3>(),
	/* 0xA6 */ new LDX_Impl <  ZeroPageParams, 3>(),
	/* 0xA7 */ new LAX_Impl <  ZeroPageParams, 3>(),  // undocumented
	/* 0xA8 */ new TAY_Impl <      NullParams, 2>(),
	/* 0xA9 */ new LDA_Impl < ImmediateParams, 2>(),
	/* 0xAA */ new TAX_Impl <      NullParams, 2>(),
	/* 0xAB */ new LAX_Impl < ImmediateParams, 2>(),  // LXA, undocumented, unstable
	/* 0xAC */ new LDY_Impl <  AbsoluteParams, 4>(),
	/* 0xAD */ new LDA_Impl <  AbsoluteParams, 4>(),
	/* 0xAE */ new LDX_Impl <  AbsoluteParams, 4>(),
	/* 0xAF */ new LAX_Impl <  AbsoluteParams, 4>(),  // undocumented

	/* 0xB0 */ new BCS_Impl <  RelativeParams, 2>(),
	/* 0xB1 */ new LDA_Impl < IndirectYParams, 5>(),
	/* 0xB2 */ new KIL_Impl <      NullParams, 3>(),  // undocumented
	/* 0xB3 */ new LAX_Impl < IndirectYParams, 5>(),  // undocumented
	/* 0xB4 */ new LDY_Impl < ZeroPageXParams, 4>(),
	/* 0xB5 */ new LDA_Impl < ZeroPageXParams, 4>(),
	/* 0xB6 */ new LDX_Impl < ZeroPageYParams, 4>(),
	/* 0xB7 */ new LAX_Impl < ZeroPageYParams, 4>(),  // undocumented
	/* 0xB8 */ new CLV_Impl <      NullParams, 2>(),
	/* 0xB9 */ new LDA_Impl < AbsoluteYParams, 4>(),
	/* 0xBA */ new TSX_Impl <      NullParams, 2>(),
	/* 0xBB */ new LAS_Impl < AbsoluteYParams, 4>(),  // undocumented
	/* 0xBC */ new LDY_Impl < AbsoluteXParams, 4>(),
	/* 0xBD */ new LDA_Impl < AbsoluteXParams, 4>(),
	/* 0xBE */ new LDX_Impl < AbsoluteYParams, 4>(),
	/* 0xBF */ new LAX_Impl < AbsoluteYParams, 4>(),  // undocumented

	/* 0xC0 */ new CPY_Impl < ImmediateParams, 2>(),
	/* 0xC1 */ new CMP_Impl < IndirectXParams, 6>(),
	/* 0xC2 */ new NOP_Impl < ImmediateParams, 2>(),  // undocumented
	/* 0xC3 */ new DCP_Impl < IndirectXParams, 8>(),  // undocumented
	/* 0xC4 */ new CPY_Impl <  ZeroPageParams, 3>(),
	/* 0xC5 */ new CMP_Impl <  ZeroPageParams, 3>(),
	/* 0xC6 */ new DEC_Impl <  ZeroPageParams, 5>(),
	/* 0xC7 */ new DCP_Impl <  ZeroPageParams, 5>(),  // undocumented
	/* 0xC8 */ new INY_Impl <      NullParams, 2>(),
	/* 0xC9 */ new CMP_Impl < ImmediateParams, 2>(),
	/* 0xCA */ new DEX_Impl <      NullParams, 2>(),
	/* 0xCB */ new SBX_Impl < ImmediateParams, 2>(),  // undocumented
	/* 0xCC */ new CPY_Impl <  AbsoluteParams, 4>(),
	/* 0xCD */ new CMP_Impl <  AbsoluteParams, 4>(),
	/* 0xCE */ new DEC_Impl <  AbsoluteParams, 6>(),
	/* 0xCF */ new DCP_Impl <  AbsoluteParams, 6>(),  // undocumented

	/* 0xD0 */ new BNE_Impl <  RelativeParams, 2>(),
	/* 0xD1 */ new CMP_Impl < IndirectYParams, 5>(),
	/* 0xD2 */ new KIL_Impl <      NullParams, 3>(),  // undocumented
	/* 0xD3 */ new DCP_Impl < IndirectYParams, 8>(),  // undocumented
	/* 0xD4 */ new NOPR_Impl< ZeroPageXParams, 4>(),  // undocumented
	/* 0xD5 */ new CMP_Impl < ZeroPageXParams, 4>(),
	/* 0xD6 */ new DEC_Impl < ZeroPageXParams, 6>(),
	/* 0xD7 */ new DCP_Impl < ZeroPageXParams, 6>(),  // undocumented
	/* 0xD8 */ new CLD_Impl <      NullParams, 2>(),
	/* 0xD9 */ new CMP_Impl < AbsoluteYParams, 4>(),
	/* 0xDA */ new NOP_Impl <      NullParams, 2>(),  // undocumented
	/* 0xDB */ new DCP_Impl < AbsoluteYParams, 7>(),  // undocumented
	/* 0xDC */ new NOPR_Impl< AbsoluteXParams, 4>(),  // undocumented
	/* 0xDD */ new CMP_Impl < AbsoluteXParams, 4>(),
	/* 0xDE */ new DEC_Impl < AbsoluteXParams, 7>(),
	/* 0xDF */ new DCP_Impl < AbsoluteXParams, 7>(),  // undocumented

	/* 0xE0 */ new CPX_Impl < ImmediateParams, 2>(),
	/* 0xE1 */ new SBC_Impl < IndirectXParams, 6>(),
	/* 0xE2 */ new NOP_Impl < ImmediateParams, 2>(),  // undocumented
	/* 0xE3 */ new ISC_Impl < IndirectXParams, 8>(),  // undocumented
	/* 0xE4 */ new CPX_Impl <  ZeroPageParams, 3>(),
	/* 0xE5 */ new SBC_Impl <  ZeroPageParams, 3>(),
	/* 0xE6 */ new INC_Impl <  ZeroPageParams, 5>(),
	/* 0xE7 */ new ISC_Impl <  ZeroPageParams, 5>(),  // undocumented
	/* 0xE8 */ new INX_Impl <      NullParams, 2>(),
	/* 0xE9 */ new SBC_Impl < ImmediateParams, 2>(),
	/* 0xEA */ new NOP_Impl <      NullParams, 2>(),
	/* 0xEB */ new SBC_Impl < ImmediateParams, 2>(),  // undocumented alias of $E9
	/* 0xEC */ new CPX_Impl <  AbsoluteParams, 4>(),
	/* 0xED */ new SBC_Impl <  AbsoluteParams, 4>(),
	/* 0xEE */ new INC_Impl <  AbsoluteParams, 6>(),
	/* 0xEF */ new ISC_Impl <  AbsoluteParams, 6>(),  // undocumented

	/* 0xF0 */ new BEQ_Impl <  RelativeParams, 2>(),
	/* 0xF1 */ new SBC_Impl < IndirectYParams, 5>(),
	/* 0xF2 */ new KIL_Impl <      NullParams, 3>(),  // undocumented
	/* 0xF3 */ new ISC_Impl < IndirectYParams, 8>(),  // undocumented
	/* 0xF4 */ new NOPR_Impl< ZeroPageXParams, 4>(),  // undocumented
	/* 0xF5 */ new SBC_Impl < ZeroPageXParams, 4>(),
	/* 0xF6 */ new INC_Impl < ZeroPageXParams, 6>(),
	/* 0xF7 */ new ISC_Impl < ZeroPageXParams, 6>(),  // undocumented
	/* 0xF8 */ new SED_Impl <      NullParams, 2>(),
	/* 0xF9 */ new SBC_Impl < AbsoluteYParams, 4>(),
	/* 0xFA */ new NOP_Impl <      NullParams, 2>(),  // undocumented
	/* 0xFB */ new ISC_Impl < AbsoluteYParams, 7>(),  // undocumented
	/* 0xFC */ new NOPR_Impl< AbsoluteXParams, 4>(),  // undocumented
	/* 0xFD */ new SBC_Impl < AbsoluteXParams, 4>(),
	/* 0xFE */ new INC_Impl < AbsoluteXParams, 7>(),
	/* 0xFF */ new ISC_Impl < AbsoluteXParams, 7>()   // undocumented
};

void Processor::run() {
	m_running = true;
	m_stopping = false;
	p_clock->beginCycle();
	while (m_running && !m_stopping) {
		step();
	}
	m_running = false;
	m_stopping = false;
}

int Processor::serviceInterrupt(uint16 vector) {
	push16(p_regs, p_bus, p_regs->pc);
	// B is clear for a hardware interrupt -- that is how a handler tells it
	// apart from BRK, which pushes the same byte with B set.
	push8(p_regs, p_bus, (p_regs->sr & ~FLAG_B) | FLAG__);
	p_regs->setStatus(FLAG_I, true);
	p_regs->pc = p_bus->read(vector) | (p_bus->read(vector + 1) << 8);
	return 7;
}

void Processor::step() {
	// NMI is edge-triggered and ignores the I flag; IRQ is level-triggered and
	// gated by it. Both are serviced in place of the next instruction.
	if (m_nmi_pending.exchange(false)) {
		p_clock->waitCycles(serviceInterrupt(0xFFFA));
	} else if (m_irq_line.load() && !p_regs->getStatus(FLAG_I)) {
		p_clock->waitCycles(serviceInterrupt(0xFFFE));
	} else {
		int code = p_bus->read(p_regs->pc++);
		BaseInstruction* p_instr = _instruction_table[code];
		int cycles = p_instr->execute(p_regs, p_bus);
		p_clock->waitCycles(cycles);
	}
	if (p_instr_callback) p_instr_callback();
}

void Processor::pause() {
	m_stopping = true;
}

void Processor::resume() {
	run();
}

bool Processor::isRunning() const {
	return m_running;
}

void Processor::nmi() {
	m_nmi_pending = true;
}

void Processor::irq(bool asserted) {
	m_irq_line = asserted;
}

bool Processor::nmiPending() const {
	return m_nmi_pending;
}

bool Processor::irqAsserted() const {
	return m_irq_line;
}

void Processor::clearInterrupts() {
	m_nmi_pending = false;
	m_irq_line = false;
}

void Processor::setInstructionCallback(const InstructionCallBackType& _instr_callback) {
	p_instr_callback = _instr_callback;
}

DebugProcessor::DebugProcessor(Bus *_bus, Registers *_regs,
		emu_clock* _clock, std::unordered_set<uint16> _breakpoints) :
		Processor(_bus, _regs, _clock), m_breakpoints(_breakpoints), p_breakp_callback(nullptr) {
}

DebugProcessor::DebugProcessor(Bus *_bus, Registers *_regs,
		emu_clock* _clock, std::initializer_list<uint16> _breakpoints) :
		Processor(_bus, _regs, _clock), m_breakpoints(_breakpoints), p_breakp_callback(nullptr) {
}

DebugProcessor::~DebugProcessor() {
}

void DebugProcessor::run() {
	m_running = true;
	m_stopping = false;
	p_clock->beginCycle();
	// Step once before testing, so resuming while stopped *at* a breakpoint
	// makes progress instead of re-triggering immediately.
	step();
	while (m_breakpoints.find(p_regs->pc) == m_breakpoints.end() && m_running && !m_stopping) {
		step();
	}
	if (m_breakpoints.find(p_regs->pc) != m_breakpoints.end() && p_breakp_callback) {
		p_breakp_callback();
	}
	m_running = false;
	m_stopping = false;
}

void DebugProcessor::setBreakpointCallback(const InstructionCallBackType& _instr_callback) {
	p_breakp_callback = _instr_callback;
}

bool DebugProcessor::addBreakpoint(uint16 address) {
	return m_breakpoints.insert(address).second;
}

bool DebugProcessor::removeBreakpoint(uint16 address) {
	return m_breakpoints.erase(address) != 0;
}

void DebugProcessor::clearBreakpoints() {
	m_breakpoints.clear();
}

const std::unordered_set<uint16>& DebugProcessor::breakpoints() const {
	return m_breakpoints;
}
