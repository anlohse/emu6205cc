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
 * The buffer is zero-filled on construction, so reads before any write return a
 * defined value.
 *
 * Addresses beyond the allocated size wrap around (`address % size`) rather than
 * running off the end, which mirrors what undecoded address lines do on real
 * hardware and keeps a short Memory memory-safe under any program. Allocate the
 * full 256 pages if you do not want mirroring.
 *
 * The bulk read()/write() overloads clamp their length to the buffer.
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
	int _size;
	/** Wrap an address into the allocated range. */
	int checkAddress(uint16 ad) const;
public:
	/** Allocate @p pages * 256 zeroed bytes. A page count of 0 is treated as 1. */
	Memory(uint16 pages);
	/** Allocate, then copy @p length bytes from @p data to @p offsetDest. */
	Memory(uint16 pages, uint8* data, int length, uint16 offsetDest = 0);
	virtual ~Memory();

	/** Bulk load; length is clamped to what fits from @p offsetDest. */
	virtual void write(uint8* src, int length, uint16 offsetDest = 0);
	/** Bulk read; length is clamped to what fits from @p offsetSrc. */
	virtual void read(uint8* dst, int length, uint16 offsetSrc = 0);

	/** Store one byte. */
	virtual void write(uint16 address, uint8 value);
	/** Load one byte. */
	virtual uint8 read(uint16 address);

	/** Allocated size in bytes. */
	int size() const;

	/** Direct view of the backing buffer, for memory dumps. */
	virtual const uint8* ptr() const;
};

#endif /* MEMORY_H_ */
