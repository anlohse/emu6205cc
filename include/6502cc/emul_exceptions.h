/*
 * emul_exceptions.h
 *
 *  Created on: 17 de jul. de 2021
 *      Author: alanl
 */

#include <exception>
#include <string>

/**
 * Base class for emulator exceptions: a std::exception carrying a message.
 *
 * @note This header has no include guard. It is currently included exactly
 * once (by asm.h), so nothing breaks, but adding a second include path will.
 */
class emu_exception : public std::exception {
private:
	std::string _msg;
public:
	emu_exception(const std::string& msg) noexcept: _msg(msg) {
	}
	virtual ~emu_exception() noexcept {}
	virtual const char* what() const noexcept {
		return _msg.c_str();
	}
};


