/*
 * p6502clock.h
 *
 *  Created on: 19 de jul. de 2021
 *      Author: alanl
 */

#ifndef M_CLOCK_H_
#define M_CLOCK_H_

#include <chrono>

class emu_clock {
public:
	emu_clock() { }
	virtual ~emu_clock() { }

	virtual void beginCycle() = 0;
	virtual void waitCycles(int cycles) = 0;

	virtual void reset() = 0;
	virtual uint64_t cycles() const = 0;
};

class default_clock : public emu_clock {
protected:
	uint64_t m_current_cycles;
public:
	default_clock();
	virtual ~default_clock();

	virtual void beginCycle();
	virtual void waitCycles(int cycles);

	virtual void reset();
	virtual uint64_t cycles() const;
};

class chrono_clock : public default_clock {
private:
	double m_cycle_time;
	std::chrono::high_resolution_clock::time_point m_begin_time;
public:
	/**
	 * Initialize the clock with the speed in MHz
	 */
	chrono_clock(double speed);
	virtual ~chrono_clock();

	virtual void beginCycle();
	virtual void waitCycles(int cycles);
};

class precision_clock : public default_clock {
private:
	double m_cycle_time;
	unsigned long long m_begin_time;
public:
	/**
	 * Initialize the clock with the speed in MHz
	 */
	precision_clock(double speed);
	virtual ~precision_clock();

	virtual void beginCycle();
	virtual void waitCycles(int cycles);
};

#endif /* M_CLOCK_H_ */
