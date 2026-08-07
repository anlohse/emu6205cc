/*
 * testFunctional.cpp
 *
 * Runs Klaus Dormann's 6502 functional test suite against the core.
 *
 *   https://github.com/Klaus2m5/6502_65C02_functional_tests
 *   Copyright (C) 2012-2020 Klaus Dormann, GPL-3.0-or-later
 *
 * The image is not vendored here -- CMake downloads it at configure time and
 * passes its location in EMU6502_TEST_DATA_DIR. See the comment in
 * CMakeLists.txt for why.
 *
 * The image is built with decimal mode enabled and report=0, so both success
 * and failure are self-trapping loops: execution stops when the program counter
 * stops advancing. Success is the `jmp *` at $3469, immediately followed by
 * `jmp start` ($4C $00 $04). Any other trap address is a failing sub-test, and
 * bin_files/6502_functional_test.lst maps it to the opcode responsible.
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

namespace {

const uint16 FUNCTIONAL_TEST_START   = 0x0400;
const uint16 FUNCTIONAL_TEST_SUCCESS = 0x3469;

// A passing run is fully deterministic, so both totals are exact signatures of
// the pinned image. The cycle count in particular is a sensitive regression
// guard: it covers branch and page-crossing penalties across 30M instructions,
// which no targeted test can do as thoroughly. If you change the pinned
// revision in CMakeLists.txt, or deliberately change cycle behaviour, re-derive
// these from the failure message rather than assuming they still hold.
const long long EXPECTED_INSTRUCTIONS = 30646177LL;
const uint64_t  EXPECTED_CYCLES       = 96241374ULL;

// Generous ceiling so a hang reports rather than running forever.
const long long INSTRUCTION_LIMIT = 200000000LL;

#ifdef EMU6502_TEST_DATA_DIR
bool loadImage(const char* name, std::vector<uint8>& out) {
	std::string path = std::string(EMU6502_TEST_DATA_DIR) + "/" + name;
	std::ifstream is(path.c_str(), std::ifstream::binary);
	if (!is)
		return false;
	out.assign(0x10000, 0);
	is.read((char*) out.data(), 0x10000);
	return is.gcount() > 0;
}
#endif

} // namespace

TEST_CASE("functional_test_suite") {
#ifndef EMU6502_TEST_DATA_DIR
	MESSAGE("functional test image not configured -- skipping "
			"(enable EMU6502_FETCH_FUNCTIONAL_TESTS or set EMU6502_TEST_DATA_DIR)");
#else
	std::vector<uint8> image;
	if (!loadImage("6502_functional_test.bin", image)) {
		MESSAGE("6502_functional_test.bin not found in " EMU6502_TEST_DATA_DIR " -- skipping");
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
	// Klaus's documented procedure is to load the image flat at $0000 and jump
	// straight to the code segment. The reset vector deliberately points at a
	// trap handler instead, so that an accidental reset is caught rather than
	// silently restarting the suite.
	regs.pc = FUNCTIONAL_TEST_START;

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

	// A trap anywhere other than the success address means a sub-test failed;
	// look the address up in bin_files/6502_functional_test.lst to find which.
	char msg[160];
	std::snprintf(msg, sizeof(msg),
			"trapped at $%04X (success is $%04X) after %lld instructions, %llu cycles",
			regs.pc, FUNCTIONAL_TEST_SUCCESS, steps, (unsigned long long) clock.cycles());
	REQUIRE_MESSAGE(regs.pc == FUNCTIONAL_TEST_SUCCESS, msg);

	CHECK_MESSAGE(steps == EXPECTED_INSTRUCTIONS, msg);
	CHECK_MESSAGE(clock.cycles() == EXPECTED_CYCLES, msg);
#endif
}
