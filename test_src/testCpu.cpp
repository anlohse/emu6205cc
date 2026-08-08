/*
 * testCpu.cpp
 *
 * Targeted regression tests for addressing modes, cycle counts, interrupts,
 * memory safety and the undocumented opcodes. The functional test in
 * testFunctional.cpp covers documented behaviour broadly; these pin down the
 * specific cases it does not reach, and the ones that were previously wrong.
 */

#include "../include/6502cc/I6502Emulator.h"
#include <doctest/doctest.h>

#include "../include/6502cc/unasm.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>

namespace {

/** A CPU wired to 64 KB of zeroed RAM, with the reset vector at $0400. */
struct Rig {
	Registers regs;
	Memory mem;
	Bus bus;
	default_clock clock;
	Processor cpu;

	Rig() : mem(256), cpu(&bus, &regs, &clock) {
		bus.connect(&mem);
		mem.write((uint16) 0xfffc, (uint8) 0x00);
		mem.write((uint16) 0xfffd, (uint8) 0x04);
		std::memset(&regs, 0, sizeof(regs));
		regs.sp = 0xfd;
		regs.sr = FLAG__;
		regs.pc = 0x0400;
	}

	/** Write consecutive bytes starting at @p at. */
	void poke(uint16 at, std::initializer_list<int> bytes) {
		uint16 p = at;
		for (int v : bytes)
			mem.write(p++, (uint8) v);
	}

	void put(uint16 at, uint8 v) { mem.write(at, v); }
	uint8 get(uint16 at) { return mem.read(at); }

	/** Execute one instruction and report how many cycles it charged. */
	int stepCycles() {
		uint64_t before = clock.cycles();
		cpu.step();
		return (int) (clock.cycles() - before);
	}
};

} // namespace

/* ----------------------------------------------------------------------- */
/* Addressing modes                                                          */
/* ----------------------------------------------------------------------- */

TEST_CASE("indexed_indirect_uses_zero_page_pointer") {
	Rig r;
	r.regs.x = 0x04;
	r.poke(0x0400, { 0xA1, 0x20 });       // LDA ($20,X)
	r.poke(0x0024, { 0x00, 0x30 });       // pointer at $20+X -> $3000
	r.poke(0x0020, { 0x00, 0x40 });       // decoy at $20     -> $4000
	r.put(0x3000, 0xAA);
	r.put(0x4004, 0xBB);
	r.cpu.step();
	CHECK_EQ(r.regs.a, 0xAA);
}

TEST_CASE("indexed_indirect_wraps_in_zero_page") {
	Rig r;
	r.regs.x = 0x01;
	r.poke(0x0400, { 0xA1, 0xFF });       // LDA ($FF,X) -> pointer at $00/$01
	r.poke(0x0000, { 0x34, 0x12 });
	r.put(0x1234, 0x5A);
	r.cpu.step();
	CHECK_EQ(r.regs.a, 0x5A);
}

TEST_CASE("indirect_indexed_wraps_pointer_in_zero_page") {
	Rig r;
	r.regs.y = 0x01;
	r.poke(0x0400, { 0xB1, 0xFF });       // LDA ($FF),Y
	r.put(0x00FF, 0x00);                  // low byte
	r.put(0x0000, 0x30);                  // high byte wraps to $00, not $0100
	r.put(0x0100, 0x99);                  // would be read without wrapping
	r.put(0x3001, 0xAA);
	r.cpu.step();
	CHECK_EQ(r.regs.a, 0xAA);
}

TEST_CASE("jmp_indirect_reproduces_page_boundary_bug") {
	Rig r;
	r.poke(0x0400, { 0x6C, 0xFF, 0x30 }); // JMP ($30FF)
	r.put(0x30FF, 0x34);
	r.put(0x3000, 0x12);                  // hardware reads the high byte here
	r.put(0x3100, 0x56);                  // a naive implementation reads here
	r.cpu.step();
	CHECK_EQ(r.regs.pc, 0x1234);
}

/* ----------------------------------------------------------------------- */
/* Cycle counts                                                              */
/* ----------------------------------------------------------------------- */

TEST_CASE("branch_cycles") {
	SUBCASE("not taken costs 2") {
		Rig r;
		r.regs.setStatus(FLAG_Z, false);
		r.poke(0x0400, { 0xF0, 0x10 });   // BEQ +16
		CHECK_EQ(r.stepCycles(), 2);
		CHECK_EQ(r.regs.pc, 0x0402);
	}
	SUBCASE("taken within the page costs 3") {
		Rig r;
		r.regs.setStatus(FLAG_Z, true);
		r.poke(0x0400, { 0xF0, 0x10 });
		CHECK_EQ(r.stepCycles(), 3);
		CHECK_EQ(r.regs.pc, 0x0412);
	}
	SUBCASE("taken across a page costs 4") {
		Rig r;
		r.regs.pc = 0x04F0;
		r.regs.setStatus(FLAG_Z, true);
		r.poke(0x04F0, { 0xF0, 0x40 });   // target $0532
		CHECK_EQ(r.stepCycles(), 4);
		CHECK_EQ(r.regs.pc, 0x0532);
	}
	SUBCASE("backward branch sign-extends") {
		Rig r;
		r.regs.setStatus(FLAG_Z, true);
		r.poke(0x0400, { 0xF0, 0xFC });   // BEQ -4 -> $03FE, crosses a page
		CHECK_EQ(r.stepCycles(), 4);
		CHECK_EQ(r.regs.pc, 0x03FE);
	}
}

TEST_CASE("page_cross_penalty_on_reads") {
	SUBCASE("LDA abs,X without crossing costs 4") {
		Rig r;
		r.regs.x = 0x01;
		r.poke(0x0400, { 0xBD, 0x00, 0x30 });   // LDA $3000,X
		CHECK_EQ(r.stepCycles(), 4);
	}
	SUBCASE("LDA abs,X crossing costs 5") {
		Rig r;
		r.regs.x = 0x01;
		r.poke(0x0400, { 0xBD, 0xFF, 0x30 });   // LDA $30FF,X -> $3100
		CHECK_EQ(r.stepCycles(), 5);
	}
	SUBCASE("LDA abs,Y crossing costs 5") {
		Rig r;
		r.regs.y = 0x01;
		r.poke(0x0400, { 0xB9, 0xFF, 0x30 });
		CHECK_EQ(r.stepCycles(), 5);
	}
	SUBCASE("LDA (zp),Y crossing costs 6") {
		Rig r;
		r.regs.y = 0x01;
		r.poke(0x0400, { 0xB1, 0x10 });
		r.poke(0x0010, { 0xFF, 0x30 });          // pointer $30FF, +1 -> $3100
		CHECK_EQ(r.stepCycles(), 6);
	}
}

TEST_CASE("no_page_cross_penalty_on_stores_and_rmw") {
	SUBCASE("STA abs,X is always 5") {
		Rig r;
		r.regs.x = 0x01;
		r.poke(0x0400, { 0x9D, 0xFF, 0x30 });   // crosses, but STA never varies
		CHECK_EQ(r.stepCycles(), 5);
	}
	SUBCASE("ASL abs,X is always 7") {
		Rig r;
		r.regs.x = 0x01;
		r.poke(0x0400, { 0x1E, 0xFF, 0x30 });
		CHECK_EQ(r.stepCycles(), 7);
	}
	SUBCASE("STA (zp),Y is always 6") {
		Rig r;
		r.regs.y = 0x01;
		r.poke(0x0400, { 0x91, 0x10 });
		r.poke(0x0010, { 0xFF, 0x30 });
		CHECK_EQ(r.stepCycles(), 6);
	}
}

/* ----------------------------------------------------------------------- */
/* Interrupts                                                                */
/* ----------------------------------------------------------------------- */

TEST_CASE("nmi_is_serviced_regardless_of_interrupt_flag") {
	Rig r;
	r.regs.setStatus(FLAG_I, true);        // must not block NMI
	r.poke(0xFFFA, { 0x00, 0x80 });        // NMI vector -> $8000
	r.poke(0x0400, { 0xEA });              // NOP, should not run yet

	r.cpu.nmi();
	CHECK(r.cpu.nmiPending());
	int cycles = r.stepCycles();

	CHECK_EQ(cycles, 7);
	CHECK_EQ(r.regs.pc, 0x8000);
	CHECK(r.regs.getStatus(FLAG_I));
	CHECK_FALSE(r.cpu.nmiPending());       // edge-triggered: consumed

	// Return address and status were pushed; B is clear for a hardware interrupt.
	CHECK_EQ(r.get(0x01FD), 0x04);         // PC high
	CHECK_EQ(r.get(0x01FC), 0x00);         // PC low
	CHECK_EQ(r.get(0x01FB) & FLAG_B, 0);
	CHECK_EQ(r.get(0x01FB) & FLAG__, FLAG__);

	// The next step resumes normally.
	r.poke(0x8000, { 0xEA });
	CHECK_EQ(r.stepCycles(), 2);
}

TEST_CASE("irq_is_gated_by_the_interrupt_flag") {
	Rig r;
	r.poke(0xFFFE, { 0x00, 0x90 });        // IRQ vector -> $9000
	r.poke(0x0400, { 0xEA, 0xEA });

	SUBCASE("blocked while I is set") {
		r.regs.setStatus(FLAG_I, true);
		r.cpu.irq(true);
		CHECK_EQ(r.stepCycles(), 2);       // the NOP ran
		CHECK_EQ(r.regs.pc, 0x0401);
	}
	SUBCASE("serviced once I is clear") {
		r.regs.setStatus(FLAG_I, false);
		r.cpu.irq(true);
		CHECK_EQ(r.stepCycles(), 7);
		CHECK_EQ(r.regs.pc, 0x9000);
		CHECK(r.regs.getStatus(FLAG_I));
	}
	SUBCASE("level-triggered: stays asserted until released") {
		r.regs.setStatus(FLAG_I, false);
		r.cpu.irq(true);
		r.cpu.step();                      // serviced, I now set
		CHECK(r.cpu.irqAsserted());
		r.cpu.irq(false);
		CHECK_FALSE(r.cpu.irqAsserted());
	}
}

TEST_CASE("rti_returns_from_an_interrupt") {
	Rig r;
	r.poke(0xFFFE, { 0x00, 0x90 });
	r.poke(0x0400, { 0xEA });
	r.poke(0x9000, { 0x40 });              // RTI
	r.regs.setStatus(FLAG_I, false);
	r.regs.setStatus(FLAG_C, true);

	r.cpu.irq(true);
	r.cpu.step();                          // service
	r.cpu.irq(false);
	CHECK_EQ(r.regs.pc, 0x9000);

	r.cpu.step();                          // RTI
	CHECK_EQ(r.regs.pc, 0x0400);           // resumes at the interrupted instruction
	CHECK(r.regs.getStatus(FLAG_C));       // flags restored
	CHECK_FALSE(r.regs.getStatus(FLAG_I));
}

TEST_CASE("brk_pushes_status_with_break_set") {
	Rig r;
	r.poke(0xFFFE, { 0x00, 0x90 });
	r.poke(0x0400, { 0x00 });              // BRK
	r.cpu.step();
	CHECK_EQ(r.regs.pc, 0x9000);
	CHECK_EQ(r.get(0x01FB) & FLAG_B, FLAG_B);  // BRK sets B, an IRQ does not
	// BRK pushes the address two bytes past the opcode.
	CHECK_EQ(r.get(0x01FD), 0x04);
	CHECK_EQ(r.get(0x01FC), 0x02);
}

TEST_CASE("reset_disables_interrupts") {
	Registers regs;
	Memory mem(256);
	mem.write((uint16) 0xfffc, (uint8) 0x34);
	mem.write((uint16) 0xfffd, (uint8) 0x12);
	Bus bus;
	bus.connect(&mem);
	default_clock clock;
	Processor cpu(&bus, &regs, &clock);
	I6502Emulator emu(&regs, &mem, &bus, &cpu);

	cpu.nmi();
	emu.start(false);

	CHECK_EQ(regs.pc, 0x1234);
	CHECK_EQ(regs.sp, 0xfd);
	CHECK(regs.getStatus(FLAG_I));
	CHECK(regs.getStatus(FLAG__));
	CHECK_FALSE(cpu.nmiPending());     // reset drops a latched NMI
	CHECK_EQ(clock.cycles(), 7);
}

/* ----------------------------------------------------------------------- */
/* Memory                                                                    */
/* ----------------------------------------------------------------------- */

TEST_CASE("memory_is_zero_filled") {
	Memory mem(256);
	CHECK_EQ(mem.read(0x0000), 0);
	CHECK_EQ(mem.read(0x8000), 0);
	CHECK_EQ(mem.read(0xFFFF), 0);
}

TEST_CASE("short_memory_wraps_instead_of_overrunning") {
	Memory mem(2);                     // 512 bytes
	CHECK_EQ(mem.size(), 512);
	mem.write((uint16) 0x0100, (uint8) 0x5A);
	CHECK_EQ(mem.read(0x0100), 0x5A);
	CHECK_EQ(mem.read(0x0300), 0x5A);  // mirrored
	mem.write((uint16) 0xFFFF, (uint8) 0x77);
	CHECK_EQ(mem.read(0x01FF), 0x77);
}

TEST_CASE("bulk_transfers_are_clamped") {
	Memory mem(1);                     // 256 bytes
	uint8 src[512];
	std::memset(src, 0xAB, sizeof(src));
	mem.write(src, sizeof(src), 0);    // must not overrun
	CHECK_EQ(mem.read(0x00FF), 0xAB);

	uint8 dst[512];
	std::memset(dst, 0, sizeof(dst));
	mem.read(dst, sizeof(dst), 0);
	CHECK_EQ(dst[0xFF], 0xAB);
}

/* ----------------------------------------------------------------------- */
/* Clocks                                                                    */
/* ----------------------------------------------------------------------- */

TEST_CASE("pacing_clocks_track_wall_clock") {
	// 100000 cycles at 1 MHz is 100 ms of emulated time. The bounds are wide
	// because timer granularity and host load both move the result around, but
	// they still catch the two ways this has been wrong: double-counting each
	// sleep (runs ~2x too fast) and per-call sleep granularity with no
	// correction (ran ~1500x too slow).
	auto measure = [](emu_clock& clk) {
		auto t0 = std::chrono::steady_clock::now();
		clk.reset();
		clk.beginCycle();
		for (int i = 0; i < 20000; i++)
			clk.waitCycles(5);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - t0).count();
		CHECK_EQ(clk.cycles(), 100000);
		return (long long) ms;
	};

	SUBCASE("chrono_clock") {
		chrono_clock clk(1.0);
		long long ms = measure(clk);
		CHECK_MESSAGE(ms >= 60, "ran too fast: ", ms, " ms for a 100 ms target");
		CHECK_MESSAGE(ms <= 500, "ran too slow: ", ms, " ms for a 100 ms target");
	}
	SUBCASE("precision_clock") {
		precision_clock clk(1.0);
		long long ms = measure(clk);
		CHECK_MESSAGE(ms >= 60, "ran too fast: ", ms, " ms for a 100 ms target");
		CHECK_MESSAGE(ms <= 500, "ran too slow: ", ms, " ms for a 100 ms target");
	}
}

TEST_CASE("default_clock_counts_without_pacing") {
	default_clock clk;
	clk.beginCycle();
	clk.waitCycles(3);
	clk.waitCycles(4);
	CHECK_EQ(clk.cycles(), 7);
	clk.reset();
	CHECK_EQ(clk.cycles(), 0);
}

/* ----------------------------------------------------------------------- */
/* Undocumented opcodes                                                      */
/* ----------------------------------------------------------------------- */

TEST_CASE("lax_loads_a_and_x") {
	Rig r;
	r.poke(0x0400, { 0xA7, 0x10 });    // LAX $10
	r.put(0x0010, 0x84);
	CHECK_EQ(r.stepCycles(), 3);
	CHECK_EQ(r.regs.a, 0x84);
	CHECK_EQ(r.regs.x, 0x84);
	CHECK(r.regs.getStatus(FLAG_N));
}

TEST_CASE("sax_stores_a_and_x") {
	Rig r;
	r.regs.a = 0xCC;
	r.regs.x = 0x0F;
	r.poke(0x0400, { 0x87, 0x10 });    // SAX $10
	CHECK_EQ(r.stepCycles(), 3);
	CHECK_EQ(r.get(0x0010), 0x0C);
	CHECK_EQ(r.regs.a, 0xCC);          // affects no flags or registers
}

TEST_CASE("dcp_decrements_then_compares") {
	Rig r;
	r.regs.a = 0x10;
	r.poke(0x0400, { 0xC7, 0x10 });    // DCP $10
	r.put(0x0010, 0x11);
	CHECK_EQ(r.stepCycles(), 5);
	CHECK_EQ(r.get(0x0010), 0x10);
	CHECK(r.regs.getStatus(FLAG_Z));   // A == decremented value
	CHECK(r.regs.getStatus(FLAG_C));
}

TEST_CASE("isc_increments_then_subtracts") {
	Rig r;
	r.regs.a = 0x10;
	r.regs.setStatus(FLAG_C, true);    // no borrow
	r.poke(0x0400, { 0xE7, 0x10 });    // ISC $10
	r.put(0x0010, 0x04);
	CHECK_EQ(r.stepCycles(), 5);
	CHECK_EQ(r.get(0x0010), 0x05);
	CHECK_EQ(r.regs.a, 0x0B);          // $10 - $05
}

TEST_CASE("slo_shifts_then_ors") {
	Rig r;
	r.regs.a = 0x01;
	r.poke(0x0400, { 0x07, 0x10 });    // SLO $10
	r.put(0x0010, 0x40);
	CHECK_EQ(r.stepCycles(), 5);
	CHECK_EQ(r.get(0x0010), 0x80);
	CHECK_EQ(r.regs.a, 0x81);
	CHECK_FALSE(r.regs.getStatus(FLAG_C));
}

TEST_CASE("rla_rotates_then_ands") {
	Rig r;
	r.regs.a = 0xFF;
	r.regs.setStatus(FLAG_C, true);
	r.poke(0x0400, { 0x27, 0x10 });    // RLA $10
	r.put(0x0010, 0x40);
	CHECK_EQ(r.stepCycles(), 5);
	CHECK_EQ(r.get(0x0010), 0x81);     // (0x40 << 1) | carry
	CHECK_EQ(r.regs.a, 0x81);
}

TEST_CASE("sre_shifts_then_eors") {
	Rig r;
	r.regs.a = 0xFF;
	r.poke(0x0400, { 0x47, 0x10 });    // SRE $10
	r.put(0x0010, 0x03);
	CHECK_EQ(r.stepCycles(), 5);
	CHECK_EQ(r.get(0x0010), 0x01);
	CHECK_EQ(r.regs.a, 0xFE);
	CHECK(r.regs.getStatus(FLAG_C));   // bit 0 of the original
}

TEST_CASE("rra_rotates_then_adds") {
	Rig r;
	r.regs.a = 0x10;
	r.regs.setStatus(FLAG_C, false);
	r.poke(0x0400, { 0x67, 0x10 });    // RRA $10
	r.put(0x0010, 0x02);
	CHECK_EQ(r.stepCycles(), 5);
	CHECK_EQ(r.get(0x0010), 0x01);
	CHECK_EQ(r.regs.a, 0x11);          // $10 + $01, no carry in
}

TEST_CASE("anc_copies_bit7_into_carry") {
	Rig r;
	r.regs.a = 0xFF;
	r.poke(0x0400, { 0x0B, 0x80 });    // ANC #$80
	CHECK_EQ(r.stepCycles(), 2);
	CHECK_EQ(r.regs.a, 0x80);
	CHECK(r.regs.getStatus(FLAG_C));
	CHECK(r.regs.getStatus(FLAG_N));
}

TEST_CASE("alr_ands_then_shifts_right") {
	Rig r;
	r.regs.a = 0xFF;
	r.poke(0x0400, { 0x4B, 0x03 });    // ALR #$03
	CHECK_EQ(r.stepCycles(), 2);
	CHECK_EQ(r.regs.a, 0x01);          // (0xFF & 0x03) >> 1
	CHECK(r.regs.getStatus(FLAG_C));   // bit 0 before the shift
}

TEST_CASE("sbx_subtracts_from_a_and_x") {
	Rig r;
	r.regs.a = 0xF0;
	r.regs.x = 0x0F;
	r.poke(0x0400, { 0xCB, 0x01 });    // SBX #$01
	CHECK_EQ(r.stepCycles(), 2);
	CHECK_EQ(r.regs.x, 0xFF);          // (0xF0 & 0x0F) - 1 = -1
	CHECK_FALSE(r.regs.getStatus(FLAG_C));
}

TEST_CASE("sbc_eb_is_an_alias_of_e9") {
	Rig r;
	r.regs.a = 0x10;
	r.regs.setStatus(FLAG_C, true);
	r.poke(0x0400, { 0xEB, 0x05 });    // SBC #$05 (undocumented encoding)
	CHECK_EQ(r.stepCycles(), 2);
	CHECK_EQ(r.regs.a, 0x0B);
}

TEST_CASE("undocumented_nops_consume_their_operands") {
	SUBCASE("2-byte immediate NOP") {
		Rig r;
		r.poke(0x0400, { 0x80, 0xFF });
		CHECK_EQ(r.stepCycles(), 2);
		CHECK_EQ(r.regs.pc, 0x0402);
	}
	SUBCASE("2-byte zero page NOP") {
		Rig r;
		r.poke(0x0400, { 0x04, 0xFF });
		CHECK_EQ(r.stepCycles(), 3);
		CHECK_EQ(r.regs.pc, 0x0402);
	}
	SUBCASE("3-byte absolute NOP") {
		Rig r;
		r.poke(0x0400, { 0x0C, 0x34, 0x12 });
		CHECK_EQ(r.stepCycles(), 4);
		CHECK_EQ(r.regs.pc, 0x0403);
	}
	SUBCASE("3-byte absolute,X NOP pays the page-cross penalty") {
		Rig r;
		r.regs.x = 0x01;
		r.poke(0x0400, { 0x1C, 0xFF, 0x30 });
		CHECK_EQ(r.stepCycles(), 5);
		CHECK_EQ(r.regs.pc, 0x0403);
	}
	SUBCASE("1-byte implied NOP") {
		Rig r;
		r.poke(0x0400, { 0x1A });
		CHECK_EQ(r.stepCycles(), 2);
		CHECK_EQ(r.regs.pc, 0x0401);
	}
}

TEST_CASE("kil_jams_the_processor") {
	Rig r;
	r.poke(0x0400, { 0x02 });
	r.cpu.step();
	CHECK_EQ(r.regs.pc, 0x0400);       // never advances
	r.cpu.step();
	CHECK_EQ(r.regs.pc, 0x0400);
}

/* ----------------------------------------------------------------------- */
/* Disassembler / processor agreement                                        */
/* ----------------------------------------------------------------------- */

TEST_CASE("disassembler_renders_operands") {
	Rig r;
	UnAsm un;
	auto text = [&](std::initializer_list<int> bytes) {
		r.poke(0x0400, bytes);
		Registers probe = { };
		probe.pc = 0x0400;
		return un.unasm_line(&r.bus, &probe);
	};

	// Immediate must be distinguishable from zero page -- $A9 $10 is LDA #$10,
	// while $A5 $10 loads from address $10.
	CHECK_EQ(text({ 0xA9, 0x10 }), "LDA #$10");
	CHECK_EQ(text({ 0xA5, 0x10 }), "LDA $10");
	CHECK_EQ(text({ 0xA2, 0xFF }), "LDX #$ff");
	CHECK_EQ(text({ 0xAD, 0x02, 0x20 }), "LDA $2002");
	CHECK_EQ(text({ 0xA1, 0x20 }), "LDA ($20,X)");
	CHECK_EQ(text({ 0xB1, 0x20 }), "LDA ($20),Y");
	CHECK_EQ(text({ 0x6C, 0x34, 0x12 }), "JMP ($1234)");
	CHECK_EQ(text({ 0x0A }), "ASL A");
	CHECK_EQ(text({ 0xEA }), "NOP");
}

TEST_CASE("disassembler_lengths_match_execution") {
	// For every opcode, the number of bytes the disassembler consumes must
	// equal the number the processor consumes -- otherwise a debugger listing
	// loses sync with the instruction stream as soon as it meets one of the
	// undocumented opcodes.
	//
	// Operands are all $00, which makes every branch displacement zero, so a
	// taken branch advances PC by 2 exactly like an untaken one.
	//
	// Excluded: instructions that deliberately move PC somewhere unrelated.
	auto isControlFlow = [](int op) {
		switch (op) {
		case 0x00: // BRK
		case 0x20: // JSR
		case 0x40: // RTI
		case 0x60: // RTS
		case 0x4C: // JMP abs
		case 0x6C: // JMP (ind)
		case 0x02: case 0x12: case 0x22: case 0x32:  // KIL jams PC in place
		case 0x42: case 0x52: case 0x62: case 0x72:
		case 0x92: case 0xB2: case 0xD2: case 0xF2:
			return true;
		default:
			return false;
		}
	};

	UnAsm un;
	int checked = 0;
	for (int op = 0; op < 256; op++) {
		if (isControlFlow(op))
			continue;

		Rig r;
		r.poke(0x0400, { op, 0x00, 0x00 });

		Registers probe = { };
		probe.pc = 0x0400;
		std::string text = un.unasm_line(&r.bus, &probe);
		int disasmLen = probe.pc - 0x0400;

		r.cpu.step();
		int execLen = r.regs.pc - 0x0400;

		char label[8];
		std::snprintf(label, sizeof(label), "$%02X", op);
		INFO("opcode ", label, " disassembled as '", text, "'");
		CHECK_EQ(disasmLen, execLen);
		CHECK_FALSE(text.empty());
		checked++;
	}
	CHECK_EQ(checked, 256 - 18);
}

/* ----------------------------------------------------------------------- */
/* Flag handling                                                             */
/* ----------------------------------------------------------------------- */

TEST_CASE("plp_ignores_the_break_bit") {
	Rig r;
	r.poke(0x0400, { 0x28 });          // PLP
	r.regs.sp = 0xFC;
	r.put(0x01FD, 0xFF);               // every bit set, including B
	r.cpu.step();
	CHECK_EQ(r.regs.sr & FLAG_B, 0);   // B is not a real register bit
	CHECK_EQ(r.regs.sr & FLAG__, FLAG__);
}

TEST_CASE("php_always_pushes_break_and_unused_set") {
	Rig r;
	r.regs.sr = FLAG__;                // B clear in the register
	r.poke(0x0400, { 0x08 });          // PHP
	r.cpu.step();
	CHECK_EQ(r.get(0x01FD) & (FLAG_B | FLAG__), FLAG_B | FLAG__);
}
