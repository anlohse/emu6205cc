/*
 * t6502.h
 *
 *  Created on: 16 de jul. de 2021
 *      Author: alanl
 */

#ifndef T6502_H_
#define T6502_H_

typedef   signed char int8;
typedef unsigned char uint8;
typedef unsigned short uint16;

/**
 * Bit masks for the processor status register (SR/P).
 *
 * FLAG__ is bit 5, which does not exist as a real flip-flop on the 6502 — it
 * reads as 1 in any pushed copy of the status register. FLAG_B (bit 4) is
 * likewise not a real register bit: it only distinguishes a status byte pushed
 * by BRK/PHP from one pushed by a hardware interrupt.
 */
enum Flags6502 {
	FLAG_C = 0x01,  //!< Carry
	FLAG_Z = 0x02,  //!< Zero
	FLAG_I = 0x04,  //!< Interrupt disable
	FLAG_D = 0x08,  //!< Decimal mode (BCD arithmetic in ADC/SBC)
	FLAG_B = 0x10,  //!< Break — set only in pushed copies
	FLAG__ = 0x20,  //!< Unused — always 1 in pushed copies
	FLAG_V = 0x40,  //!< Overflow
	FLAG_N = 0x80   //!< Negative
};

/**
 * The complete architectural state of the CPU.
 *
 * This is a plain struct with no invariants; the emulator writes it directly.
 * I6502Emulator::start() is what initialises it to post-reset values
 * (SP = $FD, PC loaded from the reset vector at $FFFC).
 */
struct Registers {
	uint8 a,   //!< Accumulator
	      x,   //!< Index X
	      y,   //!< Index Y
	      sr,  //!< Status register — see Flags6502
	      sp;  //!< Stack pointer; the stack lives at $0100 + sp and wraps in page 1
	uint16 pc; //!< Program counter

	/** Set or clear one or more Flags6502 bits. */
	void setStatus(int flag, bool v) {
		if (v)
			sr |= flag;
		else
			sr &= ~flag;
	}
	/** Test a Flags6502 bit. Non-const for historical reasons. */
	bool getStatus(int flag) {
		return (sr & flag) != 0;
	}
};

#endif /* T6502_H_ */
