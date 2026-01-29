/*
 * testAsm.cpp
 *
 *  Created on: 25 de jul. de 2021
 *      Author: alanl
 */


#include "../include/6502cc/I6502Emulator.h"
#include <doctest/doctest.h>

#include <6502cc/asm.h>

#define ASSERT_OPCODE_2PARAM(a,opcode,byte1,byte2) CHECK_EQ(data[a],opcode);\
	CHECK_EQ(data[a+1],byte1);\
	CHECK_EQ(data[a+2],byte2)

#define ASSERT_OPCODE_1PARAM(a,opcode,byte1) CHECK_EQ(data[a],opcode);\
	CHECK_EQ(data[a+1],byte1)

#define ASSERT_OPCODE(a,opcode) CHECK_EQ(data[a],opcode)

struct Test_Asm {

	Asm _m_asm;

	Test_Asm(): _m_asm(0x0400) {
	}
};

TEST_CASE_FIXTURE(Test_Asm, "test_asm_adc") {
    const char* code = R"***( ; ADC test
    adc #$12
    adc $34
    adc $35,x
    adc $3678
    adc $3940,x
    adc $4142,y
    adc ($51),x
    adc ($52),y
)***";
    auto vdata = _m_asm.compile(code);
    uint8* data = vdata.data();
    ASSERT_OPCODE_1PARAM(0x400,0x69,0x12);
    ASSERT_OPCODE_1PARAM(0x402,0x65,0x34);
    ASSERT_OPCODE_1PARAM(0x404,0x75,0x35);
    ASSERT_OPCODE_2PARAM(0x406,0x6d,0x78,0x36);
    ASSERT_OPCODE_2PARAM(0x409,0x7d,0x40,0x39);
    ASSERT_OPCODE_2PARAM(0x40c,0x79,0x42,0x41);
    ASSERT_OPCODE_1PARAM(0x40f,0x61,0x51);
    ASSERT_OPCODE_1PARAM(0x411,0x71,0x52);
}

TEST_CASE_FIXTURE(Test_Asm, "test_asm_invalid_syntax1") {
    const char* code = R"***( ; Syntax error test
    and #$2322
)***";
    CHECK_THROWS_WITH_AS(_m_asm.compile(code), "Syntax error at line: 2", asm_syntax_exception);
}

TEST_CASE_FIXTURE(Test_Asm, "test_asm_invalid_syntax2") {
    const char* code = R"***( ; Syntax error test
    and ($2322),x
)***";
    CHECK_THROWS_WITH_AS(_m_asm.compile(code), "Syntax error at line: 2", asm_syntax_exception);
}

TEST_CASE_FIXTURE(Test_Asm, "test_asm_invalid_syntax3") {
    const char* code = R"***( ; Syntax error test
    adc
)***";
    CHECK_THROWS_WITH_AS(_m_asm.compile(code), "Syntax error at line: 2", asm_syntax_exception);
}

TEST_CASE_FIXTURE(Test_Asm, "test_asm_and") {
    const char* code = R"***( ; AND test
    and #$23
    and $34
    and $35,x
    and $3678
    and $3940,x
    and $4142,y
    and ($51),x
    and ($52),y
)***";
    auto vdata = _m_asm.compile(code);
    uint8* data = vdata.data();
    ASSERT_OPCODE_1PARAM(0x400,0x29,0x23);
    ASSERT_OPCODE_1PARAM(0x402,0x25,0x34);
    ASSERT_OPCODE_1PARAM(0x404,0x35,0x35);
    ASSERT_OPCODE_2PARAM(0x406,0x2d,0x78,0x36);
    ASSERT_OPCODE_2PARAM(0x409,0x3d,0x40,0x39);
    ASSERT_OPCODE_2PARAM(0x40c,0x39,0x42,0x41);
    ASSERT_OPCODE_1PARAM(0x40f,0x21,0x51);
    ASSERT_OPCODE_1PARAM(0x411,0x31,0x52);
}