/*
 * emul_exceptions.h
 *
 *  Created on: 17 de jul. de 2021
 *      Author: alanl
 */

#include <exception>
#include <string>

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


