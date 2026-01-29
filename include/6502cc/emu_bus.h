/*
 * emu_bus.h
 *
 *  Created on: 26 de jul. de 2021
 *      Author: alanl
 */

#ifndef EMU_BUS_H_
#define EMU_BUS_H_

#include "emu_base.h"
#include "emu_memory.h"

class Bus {
protected:
	Memory* p_mem;
public:
	Bus();
	virtual ~Bus();

	virtual void connect(Memory* mem);

	virtual uint8 read(uint16 address);
	virtual void write(uint16 address, uint8 val);
};

#endif /* EMU_BUS_H_ */
