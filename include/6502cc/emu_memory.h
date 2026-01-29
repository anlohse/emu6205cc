/*
 * Memory.h
 *
 *  Created on: 17 de jul. de 2021
 *      Author: alanl
 */

#ifndef MEMORY_H_
#define MEMORY_H_

#include "emu_base.h"

#define MAX_PAGES 256
#define MAX_BYTES_PER_PAGE 256
#define MAX_BYTES MAX_PAGES*MAX_BYTES_PER_PAGE

#define PG2ABS(page,offset) (offset | (page << 8))

class Memory {
private:
	uint8* _bytes;
	uint16 _pages;
	void checkAddress(uint16 ad);
public:
	Memory(uint16 pages);
	Memory(uint16 pages, uint8* data, int length, uint16 offsetDest = 0);
	virtual ~Memory();

	virtual void write(uint8* src, int length, uint16 offsetDest = 0);
	virtual void read(uint8* dst, int length, uint16 offsetSrc = 0);

	virtual void write(uint16 address, uint8 value);
	virtual uint8 read(uint16 address);

	virtual const uint8* ptr() const;
};

#endif /* MEMORY_H_ */
