/*
 * testAdc.cpp
 *
 *  Created on: 24 de jul. de 2021
 *      Author: alanl
 */

#include "../include/6502cc/I6502Emulator.h"
#include <doctest/doctest.h>
#include "../src/Opcode.h"
#include <cstring> // for memset

uint8 program_test_adc_imm[] = {CLD_IMPLIED, LDA_IMMEDIATE, 0x01, ADC_IMMEDIATE, 0x21, BNE_RELATIVE, 0xfe};
uint8 program_test_adc_zp [] = {CLD_IMPLIED, LDA_IMMEDIATE, 0x01, STA_ZERO_PAGE, 0x00, LDA_IMMEDIATE, 0x01, ADC_ZERO_PAGE, 0x00, BNE_RELATIVE, 0xfe};
uint8 program_test_adc_zpx[] = {CLD_IMPLIED, LDA_IMMEDIATE, 0x03, STA_ZERO_PAGE, 0x03, LDA_IMMEDIATE, 0x04, LDX_IMMEDIATE, 0x02, ADC_ZERO_PAGE_X, 0x01, BNE_RELATIVE, 0xfe};
uint8 program_test_adc_abs[] = {CLD_IMPLIED, LDA_IMMEDIATE, 0x05, STA_ABSOLUTE, 0x00, 0x02, LDA_IMMEDIATE, 0x06, ADC_ABSOLUTE, 0x00, 0x02, BNE_RELATIVE, 0xfe};
uint8 program_test_adc_abx[] = {CLD_IMPLIED, LDA_IMMEDIATE, 0x06, STA_ABSOLUTE, 0x03, 0x02, LDA_IMMEDIATE, 0x07, LDX_IMMEDIATE, 0x03, ADC_ABSOLUTE_X, 0x00, 0x02, BNE_RELATIVE, 0xfe};


struct Test_ADC {
	Registers* regs;
	Memory* mem;
	Bus* bus;
	Processor* processor;
	I6502Emulator* emu;
	default_clock clock;
	uint16 oldpc;

	Test_ADC(): regs(nullptr), mem(nullptr), bus(nullptr), processor(nullptr), emu(nullptr), clock(), oldpc(0) {
		regs = new Registers();
		mem = new Memory(256);
		bus = new Bus();
		bus->connect(mem);
		processor = new Processor(bus, regs, &clock);
		processor->setInstructionCallback([this]() {
			if (oldpc == regs->pc) { // stop the execution if it enters in a trap
				processor->pause();
			} else {
				oldpc = regs->pc;
			}
		});
		emu = new I6502Emulator(regs, mem, bus, processor);
        prepare_test();
	}

	~Test_ADC() {
		delete emu;
		delete processor;
		delete bus;
		delete mem;
		delete regs;
	}

	void loadProgram(uint8* _d, int len) {
		mem->write(_d, len, 0x400);
	}

	void prepare_test() {
        // Using static to avoid stack overflow or heap allocation for speed, 
        // though 64KB on stack is risky on some platforms, typical desktop stack is 1MB+.
        // Let's dynamically allocate to be safe and clean.
        uint8* _data = new uint8[0x10000];
        memset(_data, 0, 0x10000);
		_data[0xfffc] = 0x00;
		_data[0xfffd] = 0x04;
		mem->write(_data, 0x10000, 0);
		oldpc = 0;
        delete[] _data;
	}
};

TEST_CASE_FIXTURE(Test_ADC, "test_adc_imm") {
	loadProgram(program_test_adc_imm, sizeof(program_test_adc_imm));
	emu->start(true);
	CHECK_MESSAGE(!regs->getStatus(FLAG_C), "Flag Carry should be 0");
	CHECK_MESSAGE(!regs->getStatus(FLAG_Z), "Flag Zero should be 0");
	CHECK_MESSAGE(!regs->getStatus(FLAG_V), "Flag Overflow should be 0");
	CHECK_EQ(regs->a, 0x22);
}

TEST_CASE_FIXTURE(Test_ADC, "test_adc_zp") {
	loadProgram(program_test_adc_zp, sizeof(program_test_adc_zp));
	emu->start(true);
	CHECK_MESSAGE(!regs->getStatus(FLAG_C), "Flag Carry should be 0");
	CHECK_MESSAGE(!regs->getStatus(FLAG_Z), "Flag Zero should be 0");
	CHECK_MESSAGE(!regs->getStatus(FLAG_V), "Flag Overflow should be 0");
	CHECK_EQ(regs->a, 0x02);
}

TEST_CASE_FIXTURE(Test_ADC, "test_adc_zpx") {
	loadProgram(program_test_adc_zpx, sizeof(program_test_adc_zpx));
	emu->start(true);
	CHECK_MESSAGE(!regs->getStatus(FLAG_C), "Flag Carry should be 0");
	CHECK_MESSAGE(!regs->getStatus(FLAG_Z), "Flag Zero should be 0");
	CHECK_MESSAGE(!regs->getStatus(FLAG_V), "Flag Overflow should be 0");
	CHECK_EQ(regs->a, 0x07);
}

TEST_CASE_FIXTURE(Test_ADC, "test_adc_abs") {
	loadProgram(program_test_adc_abs, sizeof(program_test_adc_abs));
	emu->start(true);
	CHECK_MESSAGE(!regs->getStatus(FLAG_C), "Flag Carry should be 0");
	CHECK_MESSAGE(!regs->getStatus(FLAG_Z), "Flag Zero should be 0");
	CHECK_MESSAGE(!regs->getStatus(FLAG_V), "Flag Overflow should be 0");
	CHECK_EQ(regs->a, 0x0b);
}

TEST_CASE_FIXTURE(Test_ADC, "test_adc_abx") {
	loadProgram(program_test_adc_abx, sizeof(program_test_adc_abx));
	emu->start(true);
	CHECK_MESSAGE(!regs->getStatus(FLAG_C), "Flag Carry should be 0");
	CHECK_MESSAGE(!regs->getStatus(FLAG_Z), "Flag Zero should be 0");
	CHECK_MESSAGE(!regs->getStatus(FLAG_V), "Flag Overflow should be 0");
	CHECK_EQ(regs->a, 0x0d);
}