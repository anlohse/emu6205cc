/*
 * parameters.h
 *
 *  Created on: 18 de jul. de 2021
 *      Author: alanl
 */

#ifndef PARAMETERS_H_
#define PARAMETERS_H_

#include "../include/6502cc/emu_base.h"
#include "../include/6502cc/emu_Bus.h"
#include "../include/6502cc/emu_bus.h"

#define READ16(ad) (bus->read(ad) | (bus->read(ad+1) << 8))
#define WRITE16(ad,val) bus->write(ad, val & 0xff); bus->write(ad+1, (val >> 8) & 0xff)

struct NullParams {
	void init(Registers* regs, Bus* bus) {
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		return 0;
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		return 0;
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
	}
};

struct AccumulatorParams {
	void init(Registers* regs, Bus* bus) {
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		return regs->a;
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		return regs->a;
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		regs->a = val;
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		regs->a = val;
	}
};

struct ImmediateParams {
	uint8 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc++);
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		return data;
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		return data;
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
	}
};

struct ZeroPageParams {
	uint8 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc++);
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		return bus->read(PG2ABS(0, data));
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		return READ16(PG2ABS(0, data));
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		bus->write(PG2ABS(0, data), val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		WRITE16(PG2ABS(0, data), val);
	}
};

struct ZeroPageXParams {
	uint8 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc++);
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		uint8 ad =data + regs->x;
		return bus->read(PG2ABS(0, ad));
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		uint8 ad = data + regs->x;
		return READ16(PG2ABS(0, ad));
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		uint8 ad = data + regs->x;
		bus->write(PG2ABS(0, ad), val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		uint8 ad = data + regs->x;
		WRITE16(PG2ABS(0, ad), val);
	}
};

struct ZeroPageYParams {
	uint8 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc++);
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		uint8 ad = data + regs->y;
		return bus->read(PG2ABS(0, ad));
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		uint8 ad = data + regs->y;
		return READ16(PG2ABS(0, ad));
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		uint8 ad = data + regs->y;
		bus->write(PG2ABS(0, ad), val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		uint8 ad = data + regs->y;
		WRITE16(PG2ABS(0, ad), val);
	}
};

struct RelativeParams {
	uint8 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc++);
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		return data;
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		return data;
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
	}
};


struct AbsConstParams {
	uint16 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc) | (bus->read(regs->pc+1) << 8);
		regs->pc += 2;
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		return data;
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		return data;
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
	}
};

struct AbsoluteParams {
	uint16 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc) | (bus->read(regs->pc+1) << 8);
		regs->pc += 2;
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		return bus->read(data);
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		return READ16(data);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		bus->write(data, val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		WRITE16(data, val);
	}
};

struct AbsoluteXParams {
	uint16 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc) | (bus->read(regs->pc+1) << 8);
		regs->pc += 2;
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		uint16 ad = data + regs->x;
		return bus->read(ad);
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		uint16 ad = data + regs->x;
		return READ16(ad);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		uint16 ad = data + regs->x;
		bus->write(ad, val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		uint16 ad = data + regs->x;
		WRITE16(ad, val);
	}
};

struct AbsoluteYParams {
	uint16 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc) | (bus->read(regs->pc+1) << 8);
		regs->pc += 2;
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		uint16 ad = data + regs->y;
		return bus->read(ad);
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		uint16 ad = data + regs->y;
		return READ16(ad);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		uint16 ad = data + regs->y;
		bus->write(ad, val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		uint16 ad = data + regs->y;
		WRITE16(ad, val);
	}
};

struct IndirectParams {
	uint16 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc) | (bus->read(regs->pc+1) << 8);
		regs->pc += 2;
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		return bus->read(data);
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		return READ16(data);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
	}
};

struct IndirectXParams {
	uint8 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc++);
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		uint16 address = bus->read(data) | (bus->read(data+1) << 8);
		return bus->read(address + regs->x);
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		uint16 address = bus->read(data) | (bus->read(data+1) << 8);
		return READ16(address + regs->x);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		uint16 address = bus->read(data) | (bus->read(data+1) << 8);
		bus->write(address + regs->x, val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		uint16 address = bus->read(data) | (bus->read(data+1) << 8);
		WRITE16(address + regs->x, val);
	}
};

struct IndirectYParams {
	uint8 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc++);
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		uint16 address = bus->read(data) | (bus->read(data+1) << 8);
		return bus->read(address + regs->y);
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		uint16 address = bus->read(data) | (bus->read(data+1) << 8);
		return READ16(address + regs->y);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		uint16 address = bus->read(data) | (bus->read(data+1) << 8);
		bus->write(address + regs->y, val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		uint16 address = bus->read(data) | (bus->read(data+1) << 8);
		WRITE16(address + regs->y, val);
	}
};


#endif /* PARAMETERS_H_ */
