/*
 * Asm.h
 *
 *  Created on: 24 de jul. de 2021
 *      Author: alanl
 */

#ifndef ASM_H_
#define ASM_H_

#include "emu_base.h"
#include <vector>
#include <string>
#include "emul_exceptions.h"

class Asm {
private:
	uint16 m_base_code;
public:
	Asm(uint16 base_code);
	~Asm();

	std::vector<uint8> compile(const std::vector<std::string>& lines);
	std::vector<uint8> compile(const std::string& source);
};

class asm_syntax_exception : public emu_exception {
public:
	asm_syntax_exception(int line) noexcept;
	virtual ~asm_syntax_exception() noexcept;
};

#endif /* ASM_H_ */
