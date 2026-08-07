/*
 * Memory.cpp
 *
 *  Created on: 17 de jul. de 2021
 *      Author: alanl
 */

#include "../include/6502cc/emul_exceptions.h"
#include <cstring>

#include "../include/6502cc/emu_memory.h"
#include "../include/6502cc/emu_bus.h"

int Memory::checkAddress(uint16 ad) const {
	// The common case is a full 64 KB block, where every uint16 is in range and
	// this is a predictable, never-taken branch.
	return ad < _size ? ad : ad % _size;
}

Memory::Memory(uint16 pages): _pages(pages ? pages : 1) {
	_size = _pages * MAX_BYTES_PER_PAGE;
	_bytes = new uint8[_size]();  // value-initialised: defined power-on state
}
Memory::Memory(uint16 pages, uint8* data, int length, uint16 offsetDest) : Memory(pages){
	write(data, length, offsetDest);
}
Memory::~Memory() {
	delete[] _bytes;
}

void Memory::write(uint8* src, int length, uint16 offsetDest) {
	if (!src || length <= 0 || offsetDest >= _size)
		return;
	if (length > _size - offsetDest)
		length = _size - offsetDest;
	memcpy(_bytes + offsetDest, src, length);
}
void Memory::read(uint8* dst, int length, uint16 offsetSrc) {
	if (!dst || length <= 0 || offsetSrc >= _size)
		return;
	if (length > _size - offsetSrc)
		length = _size - offsetSrc;
	memcpy(dst, _bytes + offsetSrc, length);
}

void Memory::write(uint16 address, uint8 value) {
	_bytes[checkAddress(address)] = value;
}
uint8 Memory::read(uint16 address) {
	return _bytes[checkAddress(address)];
}

int Memory::size() const {
	return _size;
}

const uint8* Memory::ptr() const {
	return _bytes;
}

void push8(Registers *regs, Bus *bus, uint8 val) {
	bus->write(0x0100 + regs->sp, val);
	regs->sp--;
}
void push16(Registers *regs, Bus *bus, uint16 val) {
	bus->write(0x0100 + regs->sp, (val >> 8) & 0xff);
	regs->sp--;
	bus->write(0x0100 + regs->sp, val & 0xff);
	regs->sp--;
}

uint8 pop8(Registers *regs, Bus *bus) {
	regs->sp++;
	return bus->read(0x0100 + regs->sp);
}
uint16 pop16(Registers *regs, Bus *bus) {
	regs->sp++;
	uint8 low = bus->read(0x0100 + regs->sp);
	regs->sp++;
	return low | (bus->read(0x0100 + regs->sp) << 8);
}

