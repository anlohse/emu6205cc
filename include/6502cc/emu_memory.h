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

/** Compose a 16-bit address from a page number and an offset within it. */
#define PG2ABS(page,offset) (offset | (page << 8))

/**
 * A flat block of RAM, sized in 256-byte pages.
 *
 * @warning There is no bounds checking. read()/write() accept the full 16-bit
 * address range no matter how many pages were allocated, so any Memory smaller
 * than 256 pages can be driven out of bounds by ordinary program execution.
 * Pass 256 unless you are certain the program stays in range.
 *
 * @warning The buffer is not zero-initialised; contents before the first write
 * are indeterminate.
 *
 * @note Overload hazard: `write(0, x)` binds the literal `0` to the
 * `write(uint8*, int, uint16)` overload as a null pointer constant, not to
 * `write(uint16, uint8)`. Write `write((uint16)0, x)` when the address is a
 * zero literal.
 */
class Memory {
private:
	uint8* _bytes;
	uint16 _pages;
	void checkAddress(uint16 ad);
public:
	/** Allocate @p pages * 256 bytes of uninitialised storage. */
	Memory(uint16 pages);
	/** Allocate, then copy @p length bytes from @p data to @p offsetDest. */
	Memory(uint16 pages, uint8* data, int length, uint16 offsetDest = 0);
	virtual ~Memory();

	/** Bulk load. No bounds check on offsetDest + length. */
	virtual void write(uint8* src, int length, uint16 offsetDest = 0);
	/** Bulk read. No bounds check on offsetSrc + length. */
	virtual void read(uint8* dst, int length, uint16 offsetSrc = 0);

	/** Store one byte. */
	virtual void write(uint16 address, uint8 value);
	/** Load one byte. */
	virtual uint8 read(uint16 address);

	/** Direct view of the backing buffer, for memory dumps. */
	virtual const uint8* ptr() const;
};

#endif /* MEMORY_H_ */
