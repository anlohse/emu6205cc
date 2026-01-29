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

static const char* _instruction_names[256] = {
"BRK", "ORA", INVAL, INVAL, INVAL, "ORA", "ASL", INVAL, "PHP", "ORA", "ASL", INVAL, INVAL, "ORA", "ASL", INVAL, "BPL", "ORA", INVAL, INVAL, INVAL, "ORA", "ASL", INVAL, "CLC", "ORA", INVAL, INVAL, INVAL, "ORA", "ASL", INVAL,
"JSR", "AND", INVAL, INVAL, "BIT", "AND", "ROL", INVAL, "PLP", "AND", "ROL", INVAL, "BIT", "AND", "ROL", INVAL, "BMI", "AND", INVAL, INVAL, INVAL, "AND", "ROL", INVAL, "SEC", "AND", INVAL, INVAL, INVAL, "AND", "ROL", INVAL,
"RTI", "EOR", INVAL, INVAL, INVAL, "EOR", "LSR", INVAL, "PHA", "EOR", "LSR", INVAL, "JMP", "EOR", "LSR", INVAL, "BVC", "EOR", INVAL, INVAL, INVAL, "EOR", "LSR", INVAL, "CLI", "EOR", INVAL, INVAL, INVAL, "EOR", "LSR", INVAL,
"RTS", "ADC", INVAL, INVAL, INVAL, "ADC", "ROR", INVAL, "PLA", "ADC", "ROR", INVAL, "JMP", "ADC", "ROR", INVAL, "BVS", "ADC", INVAL, INVAL, INVAL, "ADC", "ROR", INVAL, "SEI", "ADC", INVAL, INVAL, INVAL, "ADC", "ROR", INVAL,
INVAL, "STA", INVAL, INVAL, "STY", "STA", "STX", INVAL, "DEY", INVAL, "TXA", INVAL, "STY", "STA", "STX", INVAL, "BCC", "STA", INVAL, INVAL, "STY", "STA", "STX", INVAL, "TYA", "STA", "TXS", INVAL, INVAL, "STA", INVAL, INVAL,
"LDY", "LDA", "LDX", INVAL, "LDY", "LDA", "LDX", INVAL, "TAY", "LDA", "TAX", INVAL, "LDY", "LDA", "LDX", INVAL, "BCS", "LDA", INVAL, INVAL, "LDY", "LDA", "LDX", INVAL, "CLV", "LDA", "TSX", INVAL, "LDY", "LDA", "LDX", INVAL,
"CPY", "CMP", INVAL, INVAL, "CPY", "CMP", "DEC", INVAL, "INY", "CMP", "DEX", INVAL, "CPY", "CMP", "DEC", INVAL, "BNE", "CMP", INVAL, INVAL, INVAL, "CMP", "DEC", INVAL, "CLD", "CMP", INVAL, INVAL, INVAL, "CMP", "DEC", INVAL,
"CPX", "SBC", INVAL, INVAL, "CPX", "SBC", "INC", INVAL, "INX", "SBC", "NOP", INVAL, "CPX", "SBC", "INC", INVAL, "BEQ", "SBC", INVAL, INVAL, INVAL, "SBC", "INC", INVAL, "SED", "SBC", INVAL, INVAL, INVAL, "SBC", "INC", INVAL
};

string getIndParams(Bus* _bus, uint16& at) {
	uint8 data = _bus->read(at);
	at++;
	stringstream res;
	res << "($" << setfill('0') << setw(2) << right << hex << (int)data << ")";
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

string getImmParams(Bus* _bus, uint16& at) {
	uint8 data = _bus->read(at);
	at++;
	stringstream res;
	res << "$" << setfill('0') << setw(2) << right << hex << (int)data;
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
			return res + " " + getIndParams(_bus, _regs->pc) + ", X";
		case 1:
			return res + " " + getZPgParams(_bus, _regs->pc);
		case 2:
			return res + " " + getImmParams(_bus, _regs->pc);
		case 3:
			return res + " " + getAbsParams(_bus, _regs->pc);
		case 4:
			return res + " " + getIndParams(_bus, _regs->pc) + ", Y";
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
		case 5:
			return res + " " + getZPgParams(_bus, _regs->pc) + ", X";
		case 6:
			return res;
		case 7:
			return res + " " + getAbsParams(_bus, _regs->pc) + ", X";
		}
		break;
	}
	return "";
}

string UnAsm::unasm_line(Bus* _bus, uint16 at) {
	Registers r;
	r.pc = at;
	return unasm_line(_bus, &r);
}

