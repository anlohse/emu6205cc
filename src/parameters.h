/*
 * parameters.h
 *
 *  Created on: 18 de jul. de 2021
 *      Author: alanl
 *
 * Addressing modes.
 *
 * Each struct is a compile-time policy plugged into the instruction templates in
 * InstructionImpl.h. The contract is:
 *
 *   init(regs, bus)        consume the operand bytes and advance regs->pc
 *   get8bit / get16bit     resolve to a value
 *   set8bit / set16bit     resolve to an address and store
 *   extraCycles()          page-crossing penalty, valid only after a get/set
 *
 * A params object is constructed fresh inside every execute() call, so it holds
 * no state between instructions and execution is reentrant.
 */

#ifndef PARAMETERS_H_
#define PARAMETERS_H_

#include "../include/6502cc/emu_base.h"
#include "../include/6502cc/emu_bus.h"

#define READ16(ad) (bus->read(ad) | (bus->read(ad+1) << 8))
#define WRITE16(ad,val) bus->write(ad, val & 0xff); bus->write(ad+1, (val >> 8) & 0xff)

/** Read a 16-bit value from the zero page, wrapping the high byte within it. */
#define ZP_READ16(bus,zp) ((bus)->read((uint8)(zp)) | ((bus)->read((uint8)((zp) + 1)) << 8))

/**
 * Common state for addressing modes.
 *
 * m_crossed is set by the indexed modes when adding the index register carries
 * into a new page. Read instructions add it to their cycle count; stores and
 * read-modify-write instructions do not, because they always pay for the extra
 * bus cycle regardless.
 */
struct ParamsBase {
	bool m_crossed;
	bool m_dummyDone;
	ParamsBase() : m_crossed(false), m_dummyDone(false) { }
	int extraCycles() const { return m_crossed ? 1 : 0; }

	/**
	 * Perform the read the chip makes before its address is finished.
	 *
	 * A 6502 has no idle cycles: it drives the bus on every one of them, so
	 * the cycle an indexed mode spends adding the index is a real read, of a
	 * real address, that is often the wrong one. Nothing notices in RAM. It is
	 * extremely visible on $2007 or $4015, where reading has consequences, and
	 * that is what blargg's dummy-read ROMs are about.
	 *
	 * A no-op for the modes that have no such cycle; the indexed ones shadow
	 * it. Callers ask for it because whether it happens is a property of the
	 * instruction, not of the address: a read only makes it when the index
	 * carries into the high byte, while a write or a read-modify-write makes
	 * it every time.
	 */
	void dummyRead(Registers* regs, Bus* bus) { (void)regs; (void)bus; }
};

struct NullParams : ParamsBase {
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

struct AccumulatorParams : ParamsBase {
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

struct ImmediateParams : ParamsBase {
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

struct ZeroPageParams : ParamsBase {
	uint8 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc++);
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		return bus->read(PG2ABS(0, data));
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		return ZP_READ16(bus, data);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		bus->write(PG2ABS(0, data), val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		bus->write((uint8)data, val & 0xff);
		bus->write((uint8)(data + 1), (val >> 8) & 0xff);
	}
};

struct ZeroPageXParams : ParamsBase {
	uint8 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc++);
	}
	/** Reading the un-indexed address is what the adding cycle really does. */
	void dummyRead(Registers* regs, Bus* bus) {
		(void)regs;
		if (m_dummyDone)
			return;
		m_dummyDone = true;
		bus->read(PG2ABS(0, data));
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		dummyRead(regs, bus);
		uint8 ad =data + regs->x;
		return bus->read(PG2ABS(0, ad));
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		uint8 ad = data + regs->x;
		return ZP_READ16(bus, ad);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		uint8 ad = data + regs->x;
		bus->write(PG2ABS(0, ad), val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		uint8 ad = data + regs->x;
		bus->write((uint8)ad, val & 0xff);
		bus->write((uint8)(ad + 1), (val >> 8) & 0xff);
	}
};

struct ZeroPageYParams : ParamsBase {
	uint8 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc++);
	}
	/** Reading the un-indexed address is what the adding cycle really does. */
	void dummyRead(Registers* regs, Bus* bus) {
		(void)regs;
		if (m_dummyDone)
			return;
		m_dummyDone = true;
		bus->read(PG2ABS(0, data));
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		dummyRead(regs, bus);
		uint8 ad = data + regs->y;
		return bus->read(PG2ABS(0, ad));
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		uint8 ad = data + regs->y;
		return ZP_READ16(bus, ad);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		uint8 ad = data + regs->y;
		bus->write(PG2ABS(0, ad), val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		uint8 ad = data + regs->y;
		bus->write((uint8)ad, val & 0xff);
		bus->write((uint8)(ad + 1), (val >> 8) & 0xff);
	}
};

struct RelativeParams : ParamsBase {
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


struct AbsConstParams : ParamsBase {
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

struct AbsoluteParams : ParamsBase {
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

struct AbsoluteXParams : ParamsBase {
	uint16 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc) | (bus->read(regs->pc+1) << 8);
		regs->pc += 2;
	}
	/** Effective address before indexing -- needed by the SHx family. */
	uint16 base(Registers* regs, Bus* bus) {
		return data;
	}
	uint16 addr(Registers* regs, Bus* bus) {
		uint16 ad = data + regs->x;
		m_crossed = (ad & 0xff00) != (data & 0xff00);
		return ad;
	}
	/** The address driven before the carry into the high byte is applied. */
	void dummyRead(Registers* regs, Bus* bus) {
		if (m_dummyDone)
			return;
		m_dummyDone = true;
		bus->read((uint16)((data & 0xff00) | ((data + regs->x) & 0x00ff)));
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		const uint16 ad = addr(regs, bus);
		// A read only spends that cycle when the index carried; without a
		// carry the address was right the first time.
		if (m_crossed)
			dummyRead(regs, bus);
		return bus->read(ad);
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		uint16 ad = addr(regs, bus);
		return READ16(ad);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		bus->write(addr(regs, bus), val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		uint16 ad = addr(regs, bus);
		WRITE16(ad, val);
	}
};

struct AbsoluteYParams : ParamsBase {
	uint16 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc) | (bus->read(regs->pc+1) << 8);
		regs->pc += 2;
	}
	/** Effective address before indexing -- needed by the SHx family. */
	uint16 base(Registers* regs, Bus* bus) {
		return data;
	}
	uint16 addr(Registers* regs, Bus* bus) {
		uint16 ad = data + regs->y;
		m_crossed = (ad & 0xff00) != (data & 0xff00);
		return ad;
	}
	/** The address driven before the carry into the high byte is applied. */
	void dummyRead(Registers* regs, Bus* bus) {
		if (m_dummyDone)
			return;
		m_dummyDone = true;
		bus->read((uint16)((data & 0xff00) | ((data + regs->y) & 0x00ff)));
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		const uint16 ad = addr(regs, bus);
		// A read only spends that cycle when the index carried; without a
		// carry the address was right the first time.
		if (m_crossed)
			dummyRead(regs, bus);
		return bus->read(ad);
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		uint16 ad = addr(regs, bus);
		return READ16(ad);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		bus->write(addr(regs, bus), val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		uint16 ad = addr(regs, bus);
		WRITE16(ad, val);
	}
};

/**
 * Indirect, used only by JMP ($nnnn).
 *
 * Reproduces the hardware bug: the vector's high byte is fetched from the start
 * of the same page rather than from the next one, so JMP ($30FF) reads $30FF and
 * $3000. Some real programs depend on this.
 */
struct IndirectParams : ParamsBase {
	uint16 data;
	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc) | (bus->read(regs->pc+1) << 8);
		regs->pc += 2;
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		return bus->read(data);
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		uint16 hi = (data & 0xff00) | ((data + 1) & 0x00ff);
		return bus->read(data) | (bus->read(hi) << 8);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
	}
};

/**
 * Indexed indirect, (zp,X).
 *
 * X is added to the zero-page operand to select the pointer; both pointer bytes
 * are fetched from the zero page with wraparound. The index is NOT applied to
 * the resulting target address.
 */
struct IndirectXParams : ParamsBase {
	uint8 data;
	// Resolving the pointer costs two bus reads, and hardware pays for them
	// once. Instructions that both read and write -- the RMW family -- ask for
	// the address more than once, so without this the same fetch would be
	// issued again and the instruction would make more accesses than it has
	// cycles to make them in.
	bool m_resolved;
	uint16 m_addr;
	IndirectXParams() : m_resolved(false), m_addr(0) { }

	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc++);
	}
	uint16 addr(Registers* regs, Bus* bus) {
		if (!m_resolved) {
			uint8 zp = data + regs->x;
			m_addr = ZP_READ16(bus, zp);
			m_resolved = true;
		}
		return m_addr;
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		return bus->read(addr(regs, bus));
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		uint16 ad = addr(regs, bus);
		return READ16(ad);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		bus->write(addr(regs, bus), val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		uint16 ad = addr(regs, bus);
		WRITE16(ad, val);
	}
};

/**
 * Indirect indexed, (zp),Y.
 *
 * The pointer is fetched from the zero page (with wraparound) and Y is added to
 * the 16-bit result, which may cross a page.
 */
struct IndirectYParams : ParamsBase {
	uint8 data;
	// As above: the pointer is fetched once per instruction, not once per
	// request for it. The SHx family asks for the base and the indexed address
	// separately, so both are memoised rather than just the second.
	bool m_baseResolved;
	uint16 m_base;
	IndirectYParams() : m_baseResolved(false), m_base(0) { }

	void init(Registers* regs, Bus* bus) {
		data = bus->read(regs->pc++);
	}
	/** Pointer value before Y is added -- needed by the SHx family. */
	uint16 base(Registers* regs, Bus* bus) {
		if (!m_baseResolved) {
			m_base = ZP_READ16(bus, data);
			m_baseResolved = true;
		}
		return m_base;
	}
	uint16 addr(Registers* regs, Bus* bus) {
		uint16 ptr = base(regs, bus);
		uint16 ad = ptr + regs->y;
		m_crossed = (ad & 0xff00) != (ptr & 0xff00);
		return ad;
	}
	void dummyRead(Registers* regs, Bus* bus) {
		if (m_dummyDone)
			return;
		m_dummyDone = true;
		const uint16 ptr = base(regs, bus);
		bus->read((uint16)((ptr & 0xff00) | ((ptr + regs->y) & 0x00ff)));
	}
	uint8 get8bit(Registers* regs, Bus* bus) {
		const uint16 ad = addr(regs, bus);
		if (m_crossed)
			dummyRead(regs, bus);
		return bus->read(ad);
	}
	uint16 get16bit(Registers* regs, Bus* bus) {
		uint16 ad = addr(regs, bus);
		return READ16(ad);
	}
	void set8bit(Registers* regs, Bus* bus, uint8 val) {
		bus->write(addr(regs, bus), val);
	}
	void set16bit(Registers* regs, Bus* bus, uint16 val) {
		uint16 ad = addr(regs, bus);
		WRITE16(ad, val);
	}
};


#endif /* PARAMETERS_H_ */
