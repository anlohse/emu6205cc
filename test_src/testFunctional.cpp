/*
 * testFunctional.cpp
 *
 * Runs Klaus Dormann's 6502 functional test suite against the core.
 *
 * The image (test/6502_functional_test.bin) is built with decimal mode enabled
 * and report=0, so both success and failure are self-trapping loops: execution
 * stops when the program counter stops advancing. Success is the `jmp *` at
 * $3469, which is immediately followed by `jmp start` ($4C $00 $04).
 *
 * This is the strongest regression gate in the suite -- it exercises every
 * documented opcode, every addressing mode, every flag, and BCD arithmetic.
 */

#include "../include/6502cc/I6502Emulator.h"
#include <doctest/doctest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#ifndef EMU6502_TEST_DATA_DIR
#define EMU6502_TEST_DATA_DIR "test"
#endif

namespace {

const uint16 FUNCTIONAL_TEST_START   = 0x0400;
const uint16 FUNCTIONAL_TEST_SUCCESS = 0x3469;

// Generous ceiling: a passing run takes ~30.6M instructions.
const long long INSTRUCTION_LIMIT = 200000000LL;

bool loadImage(const char* name, std::vector<uint8>& out) {
	std::string path = std::string(EMU6502_TEST_DATA_DIR) + "/" + name;
	std::ifstream is(path.c_str(), std::ifstream::binary);
	if (!is)
		return false;
	out.assign(0x10000, 0);
	is.read((char*) out.data(), 0x10000);
	return is.gcount() > 0;
}

} // namespace

TEST_CASE("functional_test_suite") {
	std::vector<uint8> image;
	if (!loadImage("6502_functional_test.bin", image)) {
		MESSAGE("6502_functional_test.bin not found -- skipping");
		return;
	}

	Registers regs;
	Memory mem(256);
	mem.write(image.data(), 0x10000, 0);
	Bus bus;
	bus.connect(&mem);
	default_clock clock;
	Processor cpu(&bus, &regs, &clock);
	I6502Emulator emu(&regs, &mem, &bus, &cpu);

	emu.start(false);
	REQUIRE_EQ(regs.pc, FUNCTIONAL_TEST_START);

	long long steps = 0;
	uint16 pc = regs.pc;
	while (steps < INSTRUCTION_LIMIT) {
		pc = regs.pc;
		cpu.step();
		steps++;
		if (regs.pc == pc)
			break; // self-trap: either success or a failing test
	}

	REQUIRE_MESSAGE(steps < INSTRUCTION_LIMIT, "instruction limit reached without trapping");

	// A trap anywhere other than the success address means a test failed. The
	// address identifies which one -- cross-reference test/6502_functional_test.lst.
	char msg[128];
	std::snprintf(msg, sizeof(msg),
			"functional test trapped at $%04X (success is $%04X) after %lld instructions",
			regs.pc, FUNCTIONAL_TEST_SUCCESS, steps);
	CHECK_MESSAGE(regs.pc == FUNCTIONAL_TEST_SUCCESS, msg);
}
