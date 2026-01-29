/*
 * emu_bus.cpp
 *
 *  Created on: 26 de jul. de 2021
 *      Author: alanl
 */

#include "../include/6502cc/emu_bus.h"

Bus::Bus(): p_mem(nullptr) {
	// TODO Auto-generated constructor stub

}

Bus::~Bus() {
}

void Bus::connect(Memory *mem) {
	p_mem = mem;
}

uint8 Bus::read(uint16 address) {
	if (p_mem)
		return p_mem->read(address);
	return 0;
}
void Bus::write(uint16 address, uint8 val) {
	if (p_mem)
		p_mem->write(address, val);
}
