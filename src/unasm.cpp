#include <cstdio>
#include <sstream>
#include <iomanip>
#include "../include/6502cc/unasm.h"
#include "parameters.h"

using namespace std;

#define INVAL nullptr

UnAsm::UnAsm() {
}

UnAsm::~UnAsm() {
}

/*
 * Mnemonics for all 256 opcodes, documented and undocumented alike. The
 * processor executes every one of them, so the disassembler has to name every
 * one of them -- otherwise a listing that walks through undocumented code
 * cannot know the instruction length and loses sync with the instruction
 * stream.
 */
static const char* _instruction_names[256] = {
"BRK", "ORA", "KIL", "SLO", "NOP", "ORA", "ASL", "SLO", "PHP", "ORA", "ASL", "ANC", "NOP", "ORA", "ASL", "SLO",
"BPL", "ORA", "KIL", "SLO", "NOP", "ORA", "ASL", "SLO", "CLC", "ORA", "NOP", "SLO", "NOP", "ORA", "ASL", "SLO",
"JSR", "AND", "KIL", "RLA", "BIT", "AND", "ROL", "RLA", "PLP", "AND", "ROL", "ANC", "BIT", "AND", "ROL", "RLA",
"BMI", "AND", "KIL", "RLA", "NOP", "AND", "ROL", "RLA", "SEC", "AND", "NOP", "RLA", "NOP", "AND", "ROL", "RLA",
"RTI", "EOR", "KIL", "SRE", "NOP", "EOR", "LSR", "SRE", "PHA", "EOR", "LSR", "ALR", "JMP", "EOR", "LSR", "SRE",
"BVC", "EOR", "KIL", "SRE", "NOP", "EOR", "LSR", "SRE", "CLI", "EOR", "NOP", "SRE", "NOP", "EOR", "LSR", "SRE",
"RTS", "ADC", "KIL", "RRA", "NOP", "ADC", "ROR", "RRA", "PLA", "ADC", "ROR", "ARR", "JMP", "ADC", "ROR", "RRA",
"BVS", "ADC", "KIL", "RRA", "NOP", "ADC", "ROR", "RRA", "SEI", "ADC", "NOP", "RRA", "NOP", "ADC", "ROR", "RRA",
"NOP", "STA", "NOP", "SAX", "STY", "STA", "STX", "SAX", "DEY", "NOP", "TXA", "XAA", "STY", "STA", "STX", "SAX",
"BCC", "STA", "KIL", "SHA", "STY", "STA", "STX", "SAX", "TYA", "STA", "TXS", "TAS", "SHY", "STA", "SHX", "SHA",
"LDY", "LDA", "LDX", "LAX", "LDY", "LDA", "LDX", "LAX", "TAY", "LDA", "TAX", "LXA", "LDY", "LDA", "LDX", "LAX",
"BCS", "LDA", "KIL", "LAX", "LDY", "LDA", "LDX", "LAX", "CLV", "LDA", "TSX", "LAS", "LDY", "LDA", "LDX", "LAX",
"CPY", "CMP", "NOP", "DCP", "CPY", "CMP", "DEC", "DCP", "INY", "CMP", "DEX", "SBX", "CPY", "CMP", "DEC", "DCP",
"BNE", "CMP", "KIL", "DCP", "NOP", "CMP", "DEC", "DCP", "CLD", "CMP", "NOP", "DCP", "NOP", "CMP", "DEC", "DCP",
"CPX", "SBC", "NOP", "ISC", "CPX", "SBC", "INC", "ISC", "INX", "SBC", "NOP", "SBC", "CPX", "SBC", "INC", "ISC",
"BEQ", "SBC", "KIL", "ISC", "NOP", "SBC", "INC", "ISC", "SED", "SBC", "NOP", "ISC", "NOP", "SBC", "INC", "ISC"
};

/** Indexed indirect, ($nn,X). */
string getIndXParams(Bus* _bus, uint16& at) {
	uint8 data = _bus->read(at);
	at++;
	stringstream res;
	res << "($" << setfill('0') << setw(2) << right << hex << (int)data << ",X)";
	return res.str();
}

/** Indirect indexed, ($nn),Y. */
string getIndYParams(Bus* _bus, uint16& at) {
	uint8 data = _bus->read(at);
	at++;
	stringstream res;
	res << "($" << setfill('0') << setw(2) << right << hex << (int)data << "),Y";
	return res.str();
}

string getInd2Params(Bus* _bus, uint16& at) {
	uint16 data = _bus->read(at) | (_bus->read(at + 1) << 8);
	at++;
	at++;
	stringstream res;
	res << "($" << setfill('0') << setw(4) << right << hex << (int)data << ")";
	return res.str();
}

string getZPgParams(Bus* _bus, uint16& at) {
	uint8 data = _bus->read(at);
	at++;
	stringstream res;
	res << "$" << setfill('0') << setw(2) << right << hex << (int)data;
	return res.str();
}

/** Immediate operands carry a '#', which is what distinguishes LDA #$10 from LDA $10. */
string getImmParams(Bus* _bus, uint16& at) {
	uint8 data = _bus->read(at);
	at++;
	stringstream res;
	res << "#$" << setfill('0') << setw(2) << right << hex << (int)data;
	return res.str();
}

string getRelParams(Bus* _bus, uint16& at) {
	uint8 data = _bus->read(at);
	at++;
	stringstream res;
	int8 v = (int8)data;
	res << (int)v;
	return res.str();
}

string getAbsParams(Bus* _bus, uint16& at) {
	uint16 data = _bus->read(at) | (_bus->read(at + 1) << 8);
	at++;
	at++;
	stringstream res;
	res << "$" << setfill('0') << setw(4) << right << hex << (int)data;
	return res.str();
}

string UnAsm::unasm_line(Bus* _bus, Registers* _regs) {
	int instr = _bus->read(_regs->pc);
	_regs->pc++;
	const char* name = _instruction_names[instr];
	if (!name) {
		stringstream res;
		res << "$" << setfill('0') << setw(2) << right << hex << instr;
		return res.str();
	}
	string res(name);
	int c = instr & 0x03;
	int b = (instr & 0x1c) >> 2;
	int a = (instr & 0xe0) >> 5;
	switch(c) {
	case 0:
		switch(b) {
		case 0:
			if (a == 1) return res + " " + getAbsParams(_bus, _regs->pc);
			if (a < 4) return res;
			return res + " " + getImmParams(_bus, _regs->pc);
		case 1:
			return res + " " + getZPgParams(_bus, _regs->pc);
		case 2:
			return res;
		case 3:
			if (a == 3) return res + " " + getInd2Params(_bus, _regs->pc);
			return res + " " + getAbsParams(_bus, _regs->pc);
		case 4:
			return res + " " + getRelParams(_bus, _regs->pc);
		case 5:
			return res + " " + getZPgParams(_bus, _regs->pc) + ", X";
		case 6:
			return res;
		case 7:
			return res + " " + getAbsParams(_bus, _regs->pc) + ", X";
		}
		break;
	case 1:
		switch(b) {
		case 0:
			return res + " " + getIndXParams(_bus, _regs->pc);
		case 1:
			return res + " " + getZPgParams(_bus, _regs->pc);
		case 2:
			return res + " " + getImmParams(_bus, _regs->pc);
		case 3:
			return res + " " + getAbsParams(_bus, _regs->pc);
		case 4:
			return res + " " + getIndYParams(_bus, _regs->pc);
		case 5:
			return res + " " + getZPgParams(_bus, _regs->pc) + ", X";
		case 6:
			return res + " " + getAbsParams(_bus, _regs->pc) + ", Y";
		case 7:
			return res + " " + getAbsParams(_bus, _regs->pc) + ", X";
		}
		break;
	case 2:
		switch(b) {
		case 0:
			// $02/$22/$42/$62 are KIL and take no operand; $82..$E2 are
			// immediate (LDX #, and the undocumented NOP #).
			if (a < 4) return res;
			return res + " " + getImmParams(_bus, _regs->pc);
		case 1:
			return res + " " + getZPgParams(_bus, _regs->pc);
		case 2:
			if (a < 4)
				return res + " A";
			else
				return res;
		case 3:
			return res + " " + getAbsParams(_bus, _regs->pc);
		case 4:
			return res;  // KIL
		case 5:
			// STX/LDX and their undocumented neighbours index by Y here.
			return res + " " + getZPgParams(_bus, _regs->pc) + ((a == 4 || a == 5) ? ",Y" : ",X");
		case 6:
			return res;
		case 7:
			return res + " " + getAbsParams(_bus, _regs->pc) + ((a == 4 || a == 5) ? ",Y" : ",X");
		}
		break;
	case 3:
		// The undocumented column. Addressing mirrors c == 1, except that the
		// $9x and $Bx rows index by Y rather than X.
		switch(b) {
		case 0:
			return res + " " + getIndXParams(_bus, _regs->pc);
		case 1:
			return res + " " + getZPgParams(_bus, _regs->pc);
		case 2:
			return res + " " + getImmParams(_bus, _regs->pc);
		case 3:
			return res + " " + getAbsParams(_bus, _regs->pc);
		case 4:
			return res + " " + getIndYParams(_bus, _regs->pc);
		case 5:
			return res + " " + getZPgParams(_bus, _regs->pc) + ((a == 4 || a == 5) ? ",Y" : ",X");
		case 6:
			return res + " " + getAbsParams(_bus, _regs->pc) + ",Y";
		case 7:
			return res + " " + getAbsParams(_bus, _regs->pc) + ((a == 4 || a == 5) ? ",Y" : ",X");
		}
		break;
	}
	return "";
}

string UnAsm::unasm_line(Bus* _bus, uint16 at) {
	Registers r = { };  // only pc is consulted, but leave nothing indeterminate
	r.pc = at;
	return unasm_line(_bus, &r);
}

