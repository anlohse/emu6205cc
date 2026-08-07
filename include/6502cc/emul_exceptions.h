/*
 * emul_exceptions.h
 *
 *  Created on: 17 de jul. de 2021
 *      Author: alanl
 */

#ifndef EMUL_EXCEPTIONS_H_
#define EMUL_EXCEPTIONS_H_

#include <exception>
#include <string>

/**
 * Base class for emulator exceptions: a std::exception carrying a message.
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

#endif /* EMUL_EXCEPTIONS_H_ */

