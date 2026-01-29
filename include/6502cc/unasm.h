#ifndef UNASM_H
#define UNASM_H

#include "emu_base.h"
#include "emu_memory.h"
#include "emu_bus.h"
#include <string>

class UnAsm {
public:
	UnAsm();
	~UnAsm();
	std::string unasm_line(Bus* _bus, Registers* _regs);
	std::string unasm_line(Bus* _bus, uint16 at);
};

#endif // UNASM_H
