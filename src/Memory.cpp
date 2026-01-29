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

void Memory::checkAddress(uint16 ad) { }
Memory::Memory(uint16 pages): _pages(pages) {
	_bytes = new uint8[_pages * MAX_BYTES_PER_PAGE];
}
Memory::Memory(uint16 pages, uint8* data, int length, uint16 offsetDest) : Memory(pages){
	write(data, length, offsetDest);
}
Memory::~Memory() {
	delete _bytes;
}

void Memory::write(uint8* src, int length, uint16 offsetDest) {
	memcpy(_bytes + offsetDest, src, length);
}
void Memory::read(uint8* dst, int length, uint16 offsetSrc) {
	memcpy(dst, _bytes + offsetSrc, length);
}

void Memory::write(uint16 address, uint8 value) {
	checkAddress(address);
	_bytes[address] = value;
}
uint8 Memory::read(uint16 address) {
	checkAddress(address);
	return _bytes[address];
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

