#include "../include/6502cc/emu_processor.h"
#include "parameters.h"
#include "InstructionImpl.h"

Processor::Processor(Bus* _bus, Registers* _regs, emu_clock* _clock): p_bus(_bus), p_regs(_regs), p_clock(_clock), m_running(false), m_stopping(false), p_instr_callback(nullptr) {
}

Processor::~Processor() {
}

static BaseInstruction* nop_instruction = new NOP_Impl<NullParams,2>();

BaseInstruction* Processor::_instruction_table[256] = {
/*                           0x00,                              0x01,                              0x02,                              0x03,                              0x04,                              0x05,                              0x06,                              0x07,                              0x08,                              0x09,                                0x0A,                              0x0B,                              0x0C,                              0x0D,                              0x0E,                              0x0F, */
new BRK_Impl<     NullParams,7>(), new ORA_Impl<IndirectXParams,6>(),                   nop_instruction,                   nop_instruction,                   nop_instruction, new ORA_Impl< ZeroPageParams,3>(), new ASL_Impl< ZeroPageParams,5>(),                   nop_instruction, new PHP_Impl<     NullParams,3>(), new ORA_Impl<ImmediateParams,2>(), new ASL_Impl<AccumulatorParams,2>(),                   nop_instruction,                   nop_instruction, new ORA_Impl< AbsoluteParams,4>(), new ASL_Impl< AbsoluteParams,6>(),                   nop_instruction, // 0x0X
new BPL_Impl< RelativeParams,2>(), new ORA_Impl<IndirectYParams,5>(),                   nop_instruction,                   nop_instruction,                   nop_instruction, new ORA_Impl<ZeroPageXParams,4>(), new ASL_Impl<ZeroPageXParams,6>(),                   nop_instruction, new CLC_Impl<     NullParams,2>(), new ORA_Impl<AbsoluteYParams,4>(),                     nop_instruction,                   nop_instruction,                   nop_instruction, new ORA_Impl<AbsoluteXParams,4>(), new ASL_Impl<AbsoluteXParams,7>(),                   nop_instruction, // 0x1X
new JSR_Impl< AbsConstParams,6>(), new AND_Impl<IndirectXParams,6>(),                   nop_instruction,                   nop_instruction, new BIT_Impl< ZeroPageParams,3>(), new AND_Impl< ZeroPageParams,3>(), new ROL_Impl< ZeroPageParams,5>(),                   nop_instruction, new PLP_Impl<     NullParams,4>(), new AND_Impl<ImmediateParams,2>(), new ROL_Impl<AccumulatorParams,2>(),                   nop_instruction, new BIT_Impl< AbsoluteParams,4>(), new AND_Impl< AbsoluteParams,4>(), new ROL_Impl< AbsoluteParams,6>(),                   nop_instruction, // 0x2X
new BMI_Impl< RelativeParams,2>(), new AND_Impl<IndirectYParams,5>(),                   nop_instruction,                   nop_instruction,                   nop_instruction, new AND_Impl<ZeroPageXParams,4>(), new ROL_Impl<ZeroPageXParams,6>(),                   nop_instruction, new SEC_Impl<     NullParams,2>(), new AND_Impl<AbsoluteYParams,4>(),                     nop_instruction,                   nop_instruction,                   nop_instruction, new AND_Impl<AbsoluteXParams,4>(), new ROL_Impl<AbsoluteXParams,7>(),                   nop_instruction, // 0x3X
new RTI_Impl<     NullParams,6>(), new EOR_Impl<IndirectXParams,6>(),                   nop_instruction,                   nop_instruction,                   nop_instruction, new EOR_Impl< ZeroPageParams,3>(), new LSR_Impl< ZeroPageParams,5>(),                   nop_instruction, new PHA_Impl<     NullParams,3>(), new EOR_Impl<ImmediateParams,2>(), new LSR_Impl<AccumulatorParams,2>(),                   nop_instruction, new JMP_Impl< AbsConstParams,3>(), new EOR_Impl< AbsoluteParams,4>(), new LSR_Impl< AbsoluteParams,6>(),                   nop_instruction, // 0x4X
new BVC_Impl< RelativeParams,2>(), new EOR_Impl<IndirectYParams,5>(),                   nop_instruction,                   nop_instruction,                   nop_instruction, new EOR_Impl<ZeroPageXParams,4>(), new LSR_Impl<ZeroPageXParams,6>(),                   nop_instruction, new CLI_Impl<     NullParams,2>(), new EOR_Impl<AbsoluteYParams,4>(),                     nop_instruction,                   nop_instruction,                   nop_instruction, new EOR_Impl<AbsoluteXParams,4>(), new LSR_Impl<AbsoluteXParams,7>(),                   nop_instruction, // 0x5X
new RTS_Impl<     NullParams,6>(), new ADC_Impl<IndirectXParams,6>(),                   nop_instruction,                   nop_instruction,                   nop_instruction, new ADC_Impl< ZeroPageParams,3>(), new ROR_Impl< ZeroPageParams,5>(),                   nop_instruction, new PLA_Impl<     NullParams,4>(), new ADC_Impl<ImmediateParams,2>(), new ROR_Impl<AccumulatorParams,2>(),                   nop_instruction, new JMP_Impl< IndirectParams,5>(), new ADC_Impl< AbsoluteParams,4>(), new ROR_Impl< AbsoluteParams,6>(),                   nop_instruction, // 0x6X
new BVS_Impl< RelativeParams,2>(), new ADC_Impl<IndirectYParams,5>(),                   nop_instruction,                   nop_instruction,                   nop_instruction, new ADC_Impl<ZeroPageXParams,4>(), new ROR_Impl<ZeroPageXParams,6>(),                   nop_instruction, new SEI_Impl<     NullParams,2>(), new ADC_Impl<AbsoluteYParams,4>(),                     nop_instruction,                   nop_instruction,                   nop_instruction, new ADC_Impl<AbsoluteXParams,4>(), new ROR_Impl<AbsoluteXParams,7>(),                   nop_instruction, // 0x7X
                  nop_instruction, new STA_Impl<IndirectXParams,6>(),                   nop_instruction,                   nop_instruction, new STY_Impl< ZeroPageParams,3>(), new STA_Impl< ZeroPageParams,3>(), new STX_Impl< ZeroPageParams,3>(),                   nop_instruction, new DEY_Impl<     NullParams,2>(),                   nop_instruction, new TXA_Impl<       NullParams,2>(),                   nop_instruction, new STY_Impl< AbsoluteParams,4>(), new STA_Impl< AbsoluteParams,4>(), new STX_Impl< AbsoluteParams,4>(),                   nop_instruction, // 0x8X
new BCC_Impl< RelativeParams,2>(), new STA_Impl<IndirectYParams,6>(),                   nop_instruction,                   nop_instruction, new STY_Impl<ZeroPageXParams,4>(), new STA_Impl<ZeroPageXParams,4>(), new STX_Impl<ZeroPageYParams,4>(),                   nop_instruction, new TYA_Impl<     NullParams,2>(), new STA_Impl<AbsoluteYParams,5>(), new TXS_Impl<       NullParams,2>(),                   nop_instruction,                   nop_instruction, new STA_Impl<AbsoluteXParams,5>(),                   nop_instruction,                   nop_instruction, // 0x9X
new LDY_Impl<ImmediateParams,2>(), new LDA_Impl<IndirectXParams,6>(), new LDX_Impl<ImmediateParams,2>(),                   nop_instruction, new LDY_Impl< ZeroPageParams,3>(), new LDA_Impl< ZeroPageParams,3>(), new LDX_Impl< ZeroPageParams,3>(),                   nop_instruction, new TAY_Impl<     NullParams,2>(), new LDA_Impl<ImmediateParams,2>(), new TAX_Impl<       NullParams,2>(),                   nop_instruction, new LDY_Impl< AbsoluteParams,4>(), new LDA_Impl< AbsoluteParams,4>(), new LDX_Impl< AbsoluteParams,4>(),                   nop_instruction, // 0xaX
new BCS_Impl< RelativeParams,2>(), new LDA_Impl<IndirectYParams,5>(),                   nop_instruction,                   nop_instruction, new LDY_Impl<ZeroPageXParams,4>(), new LDA_Impl<ZeroPageXParams,4>(), new LDX_Impl<ZeroPageYParams,4>(),                   nop_instruction, new CLV_Impl<     NullParams,2>(), new LDA_Impl<AbsoluteYParams,4>(), new TSX_Impl<       NullParams,2>(),                   nop_instruction, new LDY_Impl<AbsoluteXParams,4>(), new LDA_Impl<AbsoluteXParams,4>(), new LDX_Impl<AbsoluteYParams,4>(),                   nop_instruction, // 0xbX
new CPY_Impl<ImmediateParams,2>(), new CMP_Impl<IndirectXParams,6>(),                   nop_instruction,                   nop_instruction, new CPY_Impl< ZeroPageParams,3>(), new CMP_Impl< ZeroPageParams,3>(), new DEC_Impl< ZeroPageParams,5>(),                   nop_instruction, new INY_Impl<     NullParams,2>(), new CMP_Impl<ImmediateParams,2>(), new DEX_Impl<       NullParams,2>(),                   nop_instruction, new CPY_Impl< AbsoluteParams,4>(), new CMP_Impl< AbsoluteParams,4>(), new DEC_Impl< AbsoluteParams,6>(),                   nop_instruction, // 0xcX
new BNE_Impl< RelativeParams,2>(), new CMP_Impl<IndirectYParams,5>(),                   nop_instruction,                   nop_instruction,                   nop_instruction, new CMP_Impl<ZeroPageXParams,4>(), new DEC_Impl<ZeroPageXParams,6>(),                   nop_instruction, new CLD_Impl<     NullParams,2>(), new CMP_Impl<AbsoluteYParams,4>(),                     nop_instruction,                   nop_instruction,                   nop_instruction, new CMP_Impl<AbsoluteXParams,4>(), new DEC_Impl<AbsoluteXParams,7>(),                   nop_instruction, // 0xdX
new CPX_Impl<ImmediateParams,2>(), new SBC_Impl<IndirectXParams,6>(),                   nop_instruction,                   nop_instruction, new CPX_Impl< ZeroPageParams,3>(), new SBC_Impl< ZeroPageParams,3>(), new INC_Impl< ZeroPageParams,5>(),                   nop_instruction, new INX_Impl<     NullParams,2>(), new SBC_Impl<ImmediateParams,2>(), new NOP_Impl<       NullParams,2>(),                   nop_instruction, new CPX_Impl< AbsoluteParams,4>(), new SBC_Impl< AbsoluteParams,4>(), new INC_Impl< AbsoluteParams,6>(),                   nop_instruction, // 0xeX
new BEQ_Impl< RelativeParams,3>(), new SBC_Impl<IndirectYParams,5>(),                   nop_instruction,                   nop_instruction,                   nop_instruction, new SBC_Impl<ZeroPageXParams,4>(), new INC_Impl<ZeroPageXParams,6>(),                   nop_instruction, new SED_Impl<     NullParams,2>(), new SBC_Impl<AbsoluteYParams,4>(),                     nop_instruction,                   nop_instruction,                   nop_instruction, new SBC_Impl<AbsoluteXParams,4>(), new INC_Impl<AbsoluteXParams,7>(),                   nop_instruction  // 0xfX
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

void Processor::step() {
	int code = p_bus->read(p_regs->pc++);
	BaseInstruction* p_instr = _instruction_table[code];
	int cycles = p_instr->execute(p_regs, p_bus);
	p_clock->waitCycles(cycles);
	if (p_instr_callback) p_instr_callback();
}

void Processor::pause() {
	if (m_running)
		m_stopping = true;
}

void Processor::resume() {
	run();
}

bool Processor::isRunning() const {
	return m_running;
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
