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

/**
 * A one-pass, regex-driven assembler for building small test programs in-line.
 *
 * @warning Its operand syntax is **not** conventional 6502 assembly. Read
 * docs/assembler.md before writing source for it. In particular:
 *  - indexed indirect is written `($nn),x`, not `($nn,X)`;
 *  - hex literals must be exactly 2 or 4 digits — the count selects zero page
 *    vs absolute, and anything else is a syntax error;
 *  - branch operands are parsed but always emitted as $00;
 *  - there are no labels, directives or expressions.
 *
 * For real programs use AS65 (bundled in test/) or ca65.
 */
class Asm {
private:
	uint16 m_base_code;
public:
	/** @param base_code address the first emitted byte lands at. */
	Asm(uint16 base_code);
	~Asm();

	/**
	 * Assemble one instruction per element.
	 *
	 * @return always a 65536-byte image, pre-filled with $FF, with code at the
	 *         base address and the reset vector at $FFFC/$FFFD pointing to it.
	 *         Ready to hand to Memory::write(image.data(), 0x10000, 0).
	 * @throws asm_syntax_exception on the first line that does not parse.
	 */
	std::vector<uint8> compile(const std::vector<std::string>& lines);
	/** As above, splitting @p source on CR, LF or CRLF. */
	std::vector<uint8> compile(const std::string& source);
};

/** Thrown by Asm::compile(). what() is "Syntax error at line: N" (1-based). */
class asm_syntax_exception : public emu_exception {
public:
	asm_syntax_exception(int line) noexcept;
	virtual ~asm_syntax_exception() noexcept;
};

#endif /* ASM_H_ */
