/*
 * Asm.cpp
 *
 *  Created on: 24 de jul. de 2021
 *      Author: alanl
 */

#include <sstream>
#include <iostream>
#include <regex>
#include <vector>
#include <functional>
#include <unordered_map>

#include "../include/6502cc/asm.h"
#include "Opcode.h"
#include <cstring>

Asm::Asm(uint16 base_code) :
		m_base_code(base_code) {
}

Asm::~Asm() {
}

void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

typedef std::function<void(int param, uint8* ptr, uint16& pc)> AsmInstrGen;

struct AsmInstrKey {
	enum AddressingType {
		Absolute = 1,
		Absolute_X = 2,
		Absolute_Y = 3,
		Accumulator = 4,
		Immediate = 5,
		Implied = 6,
		Indirect = 7,
		Indirect_X = 8,
		Indirect_Y = 9,
		Relative = 10,
		Zero_Page = 11,
		Zero_Page_X = 12,
		Zero_Page_Y = 13,
		Unkown = 0x0f
	};
	std::string name;
	int type;
	AsmInstrKey() :
			name(), type(0) {
	}
	AsmInstrKey(const std::string &_name, int _type) :
			name(_name), type(_type) {
	}
	AsmInstrKey(const AsmInstrKey& other) :
			name(other.name), type(other.type) {
	}
	bool operator==(const AsmInstrKey& o) const {
		return name == o.name && type == o.type;
	}
};

template <>
struct std::hash<AsmInstrKey> {
	size_t operator()(const AsmInstrKey &key) const noexcept {
		std::hash<std::string> strhash;
		return  (key.type) |
				(strhash(key.name) << 4);
	}
};

template<>
struct std::equal_to<AsmInstrKey> {
	bool operator()(const AsmInstrKey& __x, const AsmInstrKey& __y) const
	      { return __x == __y; }
};

struct addr_2Param {
	void operator()(int val, uint8* ptr, uint16& pc) {
		*(ptr + pc) = val & 0xff;
		pc++;
		*(ptr + pc) = (val >> 8) & 0xff;
		pc++;
	}
};

struct addr_1Param {
	void operator()(int val, uint8* ptr, uint16& pc) {
		*(ptr + pc) = val & 0xff;
		pc++;
	}
};

struct addr_NoParams {
	void operator()(int val, uint8* ptr, uint16& pc) {
	}
};

template<typename AddrT, int opcode> struct Asm2Code {
	void operator()(int val, uint8* base_ptr, uint16& pc){
		AddrT addr;
		*(base_ptr + pc) = opcode & 0xff;
		pc++;
		addr(val,base_ptr,pc);
	}
};

std::unordered_map<AsmInstrKey, AsmInstrGen> _build_instr_mapper() {
	std::unordered_map<AsmInstrKey, AsmInstrGen> map;

	map[AsmInstrKey("ADC", AsmInstrKey::Immediate)] = AsmInstrGen(Asm2Code<addr_1Param, 0x69>());
	map[AsmInstrKey("ADC", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0x65>());
	map[AsmInstrKey("ADC", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x75>());
	map[AsmInstrKey("ADC", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0x6d>());
	map[AsmInstrKey("ADC", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0x7d>());
	map[AsmInstrKey("ADC", AsmInstrKey::Absolute_Y)] = AsmInstrGen(Asm2Code<addr_2Param, 0x79>());
	map[AsmInstrKey("ADC", AsmInstrKey::Indirect_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x61>());
	map[AsmInstrKey("ADC", AsmInstrKey::Indirect_Y)] = AsmInstrGen(Asm2Code<addr_1Param, 0x71>());
	map[AsmInstrKey("AND", AsmInstrKey::Immediate)] = AsmInstrGen(Asm2Code<addr_1Param, 0x29>());
	map[AsmInstrKey("AND", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0x25>());
	map[AsmInstrKey("AND", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x35>());
	map[AsmInstrKey("AND", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0x2d>());
	map[AsmInstrKey("AND", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0x3d>());
	map[AsmInstrKey("AND", AsmInstrKey::Absolute_Y)] = AsmInstrGen(Asm2Code<addr_2Param, 0x39>());
	map[AsmInstrKey("AND", AsmInstrKey::Indirect_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x21>());
	map[AsmInstrKey("AND", AsmInstrKey::Indirect_Y)] = AsmInstrGen(Asm2Code<addr_1Param, 0x31>());
	map[AsmInstrKey("ASL", AsmInstrKey::Accumulator)] = AsmInstrGen(Asm2Code<addr_NoParams, 0xa>());
	map[AsmInstrKey("ASL", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0x6>());
	map[AsmInstrKey("ASL", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x16>());
	map[AsmInstrKey("ASL", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0xe>());
	map[AsmInstrKey("ASL", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0x1e>());
	map[AsmInstrKey("BCC", AsmInstrKey::Relative)] = AsmInstrGen(Asm2Code<addr_1Param, 0x90>());
	map[AsmInstrKey("BCS", AsmInstrKey::Relative)] = AsmInstrGen(Asm2Code<addr_1Param, 0xb0>());
	map[AsmInstrKey("BEQ", AsmInstrKey::Relative)] = AsmInstrGen(Asm2Code<addr_1Param, 0xf0>());
	map[AsmInstrKey("BIT", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0x24>());
	map[AsmInstrKey("BIT", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0x2c>());
	map[AsmInstrKey("BMI", AsmInstrKey::Relative)] = AsmInstrGen(Asm2Code<addr_1Param, 0x30>());
	map[AsmInstrKey("BNE", AsmInstrKey::Relative)] = AsmInstrGen(Asm2Code<addr_1Param, 0xd0>());
	map[AsmInstrKey("BPL", AsmInstrKey::Relative)] = AsmInstrGen(Asm2Code<addr_1Param, 0x10>());
	map[AsmInstrKey("BRK", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x0>());
	map[AsmInstrKey("BVC", AsmInstrKey::Relative)] = AsmInstrGen(Asm2Code<addr_1Param, 0x50>());
	map[AsmInstrKey("BVS", AsmInstrKey::Relative)] = AsmInstrGen(Asm2Code<addr_1Param, 0x70>());
	map[AsmInstrKey("CLC", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x18>());
	map[AsmInstrKey("CLD", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0xd8>());
	map[AsmInstrKey("CLI", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x58>());
	map[AsmInstrKey("CLV", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0xb8>());
	map[AsmInstrKey("CMP", AsmInstrKey::Immediate)] = AsmInstrGen(Asm2Code<addr_1Param, 0xc9>());
	map[AsmInstrKey("CMP", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0xc5>());
	map[AsmInstrKey("CMP", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0xd5>());
	map[AsmInstrKey("CMP", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0xcd>());
	map[AsmInstrKey("CMP", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0xdd>());
	map[AsmInstrKey("CMP", AsmInstrKey::Absolute_Y)] = AsmInstrGen(Asm2Code<addr_2Param, 0xd9>());
	map[AsmInstrKey("CMP", AsmInstrKey::Indirect_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0xc1>());
	map[AsmInstrKey("CMP", AsmInstrKey::Indirect_Y)] = AsmInstrGen(Asm2Code<addr_1Param, 0xd1>());
	map[AsmInstrKey("CPX", AsmInstrKey::Immediate)] = AsmInstrGen(Asm2Code<addr_1Param, 0xe0>());
	map[AsmInstrKey("CPX", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0xe4>());
	map[AsmInstrKey("CPX", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0xec>());
	map[AsmInstrKey("CPY", AsmInstrKey::Immediate)] = AsmInstrGen(Asm2Code<addr_1Param, 0xc0>());
	map[AsmInstrKey("CPY", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0xc4>());
	map[AsmInstrKey("CPY", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0xcc>());
	map[AsmInstrKey("DEC", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0xc6>());
	map[AsmInstrKey("DEC", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0xd6>());
	map[AsmInstrKey("DEC", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0xce>());
	map[AsmInstrKey("DEC", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0xde>());
	map[AsmInstrKey("DEX", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0xca>());
	map[AsmInstrKey("DEY", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x88>());
	map[AsmInstrKey("EOR", AsmInstrKey::Immediate)] = AsmInstrGen(Asm2Code<addr_1Param, 0x49>());
	map[AsmInstrKey("EOR", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0x45>());
	map[AsmInstrKey("EOR", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x55>());
	map[AsmInstrKey("EOR", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0x4d>());
	map[AsmInstrKey("EOR", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0x5d>());
	map[AsmInstrKey("EOR", AsmInstrKey::Absolute_Y)] = AsmInstrGen(Asm2Code<addr_2Param, 0x59>());
	map[AsmInstrKey("EOR", AsmInstrKey::Indirect_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x41>());
	map[AsmInstrKey("EOR", AsmInstrKey::Indirect_Y)] = AsmInstrGen(Asm2Code<addr_1Param, 0x51>());
	map[AsmInstrKey("INC", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0xe6>());
	map[AsmInstrKey("INC", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0xf6>());
	map[AsmInstrKey("INC", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0xee>());
	map[AsmInstrKey("INC", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0xfe>());
	map[AsmInstrKey("INX", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0xe8>());
	map[AsmInstrKey("INY", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0xc8>());
	map[AsmInstrKey("JMP", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0x4c>());
	map[AsmInstrKey("JMP", AsmInstrKey::Indirect)] = AsmInstrGen(Asm2Code<addr_2Param, 0x6c>());
	map[AsmInstrKey("JSR", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0x20>());
	map[AsmInstrKey("LDA", AsmInstrKey::Immediate)] = AsmInstrGen(Asm2Code<addr_1Param, 0xa9>());
	map[AsmInstrKey("LDA", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0xa5>());
	map[AsmInstrKey("LDA", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0xb5>());
	map[AsmInstrKey("LDA", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0xad>());
	map[AsmInstrKey("LDA", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0xbd>());
	map[AsmInstrKey("LDA", AsmInstrKey::Absolute_Y)] = AsmInstrGen(Asm2Code<addr_2Param, 0xb9>());
	map[AsmInstrKey("LDA", AsmInstrKey::Indirect_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0xa1>());
	map[AsmInstrKey("LDA", AsmInstrKey::Indirect_Y)] = AsmInstrGen(Asm2Code<addr_1Param, 0xb1>());
	map[AsmInstrKey("LDX", AsmInstrKey::Immediate)] = AsmInstrGen(Asm2Code<addr_1Param, 0xa2>());
	map[AsmInstrKey("LDX", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0xa6>());
	map[AsmInstrKey("LDX", AsmInstrKey::Zero_Page_Y)] = AsmInstrGen(Asm2Code<addr_1Param, 0xb6>());
	map[AsmInstrKey("LDX", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0xae>());
	map[AsmInstrKey("LDX", AsmInstrKey::Absolute_Y)] = AsmInstrGen(Asm2Code<addr_2Param, 0xbe>());
	map[AsmInstrKey("LDY", AsmInstrKey::Immediate)] = AsmInstrGen(Asm2Code<addr_1Param, 0xa0>());
	map[AsmInstrKey("LDY", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0xa4>());
	map[AsmInstrKey("LDY", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0xb4>());
	map[AsmInstrKey("LDY", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0xac>());
	map[AsmInstrKey("LDY", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0xbc>());
	map[AsmInstrKey("LSR", AsmInstrKey::Accumulator)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x4a>());
	map[AsmInstrKey("LSR", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0x46>());
	map[AsmInstrKey("LSR", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x56>());
	map[AsmInstrKey("LSR", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0x4e>());
	map[AsmInstrKey("LSR", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0x5e>());
	map[AsmInstrKey("NOP", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0xea>());
	map[AsmInstrKey("ORA", AsmInstrKey::Immediate)] = AsmInstrGen(Asm2Code<addr_1Param, 0x9>());
	map[AsmInstrKey("ORA", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0x5>());
	map[AsmInstrKey("ORA", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x15>());
	map[AsmInstrKey("ORA", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0xd>());
	map[AsmInstrKey("ORA", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0x1d>());
	map[AsmInstrKey("ORA", AsmInstrKey::Absolute_Y)] = AsmInstrGen(Asm2Code<addr_2Param, 0x19>());
	map[AsmInstrKey("ORA", AsmInstrKey::Indirect_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x1>());
	map[AsmInstrKey("ORA", AsmInstrKey::Indirect_Y)] = AsmInstrGen(Asm2Code<addr_1Param, 0x11>());
	map[AsmInstrKey("PHA", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x48>());
	map[AsmInstrKey("PHP", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x8>());
	map[AsmInstrKey("PLA", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x68>());
	map[AsmInstrKey("PLP", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x28>());
	map[AsmInstrKey("ROL", AsmInstrKey::Accumulator)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x2a>());
	map[AsmInstrKey("ROL", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0x26>());
	map[AsmInstrKey("ROL", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x36>());
	map[AsmInstrKey("ROL", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0x2e>());
	map[AsmInstrKey("ROL", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0x3e>());
	map[AsmInstrKey("ROR", AsmInstrKey::Accumulator)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x6a>());
	map[AsmInstrKey("ROR", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0x66>());
	map[AsmInstrKey("ROR", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x76>());
	map[AsmInstrKey("ROR", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0x6e>());
	map[AsmInstrKey("ROR", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0x7e>());
	map[AsmInstrKey("RTI", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x40>());
	map[AsmInstrKey("RTS", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x60>());
	map[AsmInstrKey("SBC", AsmInstrKey::Immediate)] = AsmInstrGen(Asm2Code<addr_1Param, 0xe9>());
	map[AsmInstrKey("SBC", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0xe5>());
	map[AsmInstrKey("SBC", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0xf5>());
	map[AsmInstrKey("SBC", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0xed>());
	map[AsmInstrKey("SBC", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0xfd>());
	map[AsmInstrKey("SBC", AsmInstrKey::Absolute_Y)] = AsmInstrGen(Asm2Code<addr_2Param, 0xf9>());
	map[AsmInstrKey("SBC", AsmInstrKey::Indirect_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0xe1>());
	map[AsmInstrKey("SBC", AsmInstrKey::Indirect_Y)] = AsmInstrGen(Asm2Code<addr_1Param, 0xf1>());
	map[AsmInstrKey("SEC", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x38>());
	map[AsmInstrKey("SED", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0xf8>());
	map[AsmInstrKey("SEI", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x78>());
	map[AsmInstrKey("STA", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0x85>());
	map[AsmInstrKey("STA", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x95>());
	map[AsmInstrKey("STA", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0x8d>());
	map[AsmInstrKey("STA", AsmInstrKey::Absolute_X)] = AsmInstrGen(Asm2Code<addr_2Param, 0x9d>());
	map[AsmInstrKey("STA", AsmInstrKey::Absolute_Y)] = AsmInstrGen(Asm2Code<addr_2Param, 0x99>());
	map[AsmInstrKey("STA", AsmInstrKey::Indirect_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x81>());
	map[AsmInstrKey("STA", AsmInstrKey::Indirect_Y)] = AsmInstrGen(Asm2Code<addr_1Param, 0x91>());
	map[AsmInstrKey("STX", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0x86>());
	map[AsmInstrKey("STX", AsmInstrKey::Zero_Page_Y)] = AsmInstrGen(Asm2Code<addr_1Param, 0x96>());
	map[AsmInstrKey("STX", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0x8e>());
	map[AsmInstrKey("STY", AsmInstrKey::Zero_Page)] = AsmInstrGen(Asm2Code<addr_1Param, 0x84>());
	map[AsmInstrKey("STY", AsmInstrKey::Zero_Page_X)] = AsmInstrGen(Asm2Code<addr_1Param, 0x94>());
	map[AsmInstrKey("STY", AsmInstrKey::Absolute)] = AsmInstrGen(Asm2Code<addr_2Param, 0x8c>());
	map[AsmInstrKey("TAX", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0xaa>());
	map[AsmInstrKey("TAY", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0xa8>());
	map[AsmInstrKey("TSX", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0xba>());
	map[AsmInstrKey("TXA", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x8a>());
	map[AsmInstrKey("TXS", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x9a>());
	map[AsmInstrKey("TYA", AsmInstrKey::Implied)] = AsmInstrGen(Asm2Code<addr_NoParams, 0x98>());

	return map;
}

static std::unordered_map<AsmInstrKey, AsmInstrGen> instruction_mapper = _build_instr_mapper();

std::string uppercopy(const std::string& _str) {
	std::string str = _str;
	std::transform(str.begin(), str.end(),str.begin(), ::toupper);
	return str;
}

std::vector<uint8> Asm::compile(const std::vector<std::string> &lines) {
	std::string rgx_str = "^\\s*([a-z]+)(\\s+|$)(\\()?(\\#)?(\\$[0-9a-f]+)?(\\))?((\\+|-)?\\d+)?(a)?(\\s*,\\s*(x|y))?\\s*(;.*?)?$";
	std::regex rgx(rgx_str, std::regex::ECMAScript | std::regex::icase);
	auto line = lines.begin();
	auto lend = lines.end();
	uint16 pc = m_base_code;
	uint8 r[0x10000];
	memset(r,0xff,0x10000);
	r[0xfffc] = pc & 0xff;
	r[0xfffd] = (pc >> 8) & 0xff;
	uint8* base_ptr = r;
	for (; line != lend; line++) {
		std::string s = *line;
		ltrim(s);
		if (s.empty() || s[0] == ';') continue;
		std::smatch m;
		std::regex_match (s, m, rgx);
		if (m.size() > 0) {
			auto instr = m[1];
			int type = AsmInstrKey::Unkown;
			auto opar = m[3];
			auto imm = m[4];
			auto val = m[5];
			auto cpar = m[6];
			auto rel = m[7];
			auto rega = m[9];
			auto regxy = m[11];
			if (imm.length() == 0 && val.length() == 0 && opar.length() == 0 && cpar.length() == 0 && rel.length() == 0 && rega.length() == 0 && regxy.length() == 0)
				type = AsmInstrKey::Implied;
			else if (imm.length() > 0 && val.length() == 3 && opar.length() == 0 && cpar.length() == 0 && rel.length() == 0 && rega.length() == 0 && regxy.length() == 0)
				type = AsmInstrKey::Immediate;
			else if (imm.length() == 0 && val.length() == 5 && opar.length() == 0 && cpar.length() == 0 && rel.length() == 0 && rega.length() == 0 && regxy.length() == 0)
				type = AsmInstrKey::Absolute;
			else if (imm.length() == 0 && val.length() == 5 && opar.length() == 0 && cpar.length() == 0 && rel.length() == 0 && rega.length() == 0 && regxy.length() == 1)
				type = (regxy.str() == "x" || regxy.str() == "X") ? AsmInstrKey::Absolute_X : AsmInstrKey::Absolute_Y;
			else if (imm.length() == 0 && val.length() == 0 && opar.length() == 0 && cpar.length() == 0 && rel.length() == 0 && rega.length() == 1 && regxy.length() == 0)
				type = AsmInstrKey::Accumulator;
			else if (imm.length() == 0 && val.length() == 5 && opar.length() == 1 && cpar.length() == 1 && rel.length() == 0 && rega.length() == 0 && regxy.length() == 0)
				type = AsmInstrKey::Indirect;
			else if (imm.length() == 0 && val.length() == 3 && opar.length() == 1 && cpar.length() == 1 && rel.length() == 0 && rega.length() == 0 && regxy.length() == 1)
				type = (regxy.str() == "x" || regxy.str() == "X") ? AsmInstrKey::Indirect_X : AsmInstrKey::Indirect_Y;
			else if (imm.length() == 0 && val.length() == 3 && opar.length() == 0 && cpar.length() == 0 && rel.length() == 0 && rega.length() == 0 && regxy.length() == 0)
				type = AsmInstrKey::Zero_Page;
			else if (imm.length() == 0 && val.length() == 3 && opar.length() == 0 && cpar.length() == 0 && rel.length() == 0 && rega.length() == 0 && regxy.length() == 1)
				type = (regxy.str() == "x" || regxy.str() == "X") ? AsmInstrKey::Zero_Page_X : AsmInstrKey::Zero_Page_Y;
			else if (imm.length() == 0 && val.length() == 0 && opar.length() == 0 && cpar.length() == 0 && rel.length() > 0 && rega.length() == 0 && regxy.length() == 0)
				type = AsmInstrKey::Relative;
			AsmInstrKey key(uppercopy(instr.str()), type);
			int parsedVal = val.length() == 0 ? 0 : (rel.length() == 0 ? strtoul(val.str().c_str() + 1, nullptr, 16) : atoi(val.str().c_str()));
			auto found = instruction_mapper.find(key);
			auto end = instruction_mapper.end();
			if (found == end)
				throw asm_syntax_exception(line - lines.begin() + 1);
			else
				(*found).second(parsedVal, base_ptr, pc);
		} else {
			throw asm_syntax_exception(line - lines.begin() + 1);
		}
	}
	return std::vector<uint8>(r,r+0x10000);
}

std::istream& getline(std::istream &is, std::string &t) {
	t.clear();
	std::istream::sentry se(is, true);
	std::streambuf *sb = is.rdbuf();
	for (;;) {
		int c = sb->sbumpc();
		switch (c) {
		case '\n': // linux or mac
			if (sb->sgetc() == '\r') // mac eol
				sb->sbumpc();
			return is;
		case '\r': // windows eol
			if (sb->sgetc() == '\n')
				sb->sbumpc();
			return is;
		case std::streambuf::traits_type::eof():
			if (t.empty())
				is.setstate(std::ios::eofbit);
			return is;
		default:
			t += (char) c;
		}
	}
	return is;
}

std::vector<uint8> Asm::compile(const std::string &source) {
	std::vector<std::string> lines;
	std::stringstream ss(source);
	std::string str;
	while (getline(ss, str)) {
		lines.push_back(str);
	}
	return compile(lines);
}

static std::string EXC_FORMAT = std::string("Syntax error at line: ");
std::string format_error(int line) {
	return EXC_FORMAT + std::to_string(line);
}

asm_syntax_exception::asm_syntax_exception(int line) noexcept : emu_exception(format_error(line)) {
}
asm_syntax_exception::~asm_syntax_exception() noexcept {
}

