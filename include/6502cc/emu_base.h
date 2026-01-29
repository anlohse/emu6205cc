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

enum Flags6502 {
	FLAG_C = 0x01,
	FLAG_Z = 0x02,
	FLAG_I = 0x04,
	FLAG_D = 0x08,
	FLAG_B = 0x10,
	FLAG__ = 0x20,
	FLAG_V = 0x40,
	FLAG_N = 0x80
};

struct Registers {
	uint8 a, x, y, sr, sp;
	uint16 pc;
	void setStatus(int flag, bool v) {
		if (v)
			sr |= flag;
		else
			sr &= ~flag;
	}
	bool getStatus(int flag) {
		return (sr & flag) != 0;
	}
};

#endif /* T6502_H_ */
