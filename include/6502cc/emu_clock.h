/*
 * p6502clock.h
 *
 *  Created on: 19 de jul. de 2021
 *      Author: alanl
 */

#ifndef M_CLOCK_H_
#define M_CLOCK_H_

#include <chrono>

/**
 * Cycle accounting and (optionally) wall-clock pacing.
 *
 * Processor::step() calls waitCycles() with the cost of each instruction. An
 * implementation may simply accumulate the count, or additionally sleep so the
 * emulated CPU tracks real time.
 */
class emu_clock {
public:
	emu_clock() { }
	virtual ~emu_clock() { }

	/** Mark the timing origin. Call once before entering a run loop. */
	virtual void beginCycle() = 0;
	/** Charge @p cycles to the counter and pace if this clock paces. */
	virtual void waitCycles(int cycles) = 0;

	/** Zero the cycle counter. */
	virtual void reset() = 0;
	/** Total cycles executed since the last reset(). */
	virtual uint64_t cycles() const = 0;
};

/**
 * Free-running clock: counts cycles, never sleeps.
 *
 * The right choice for tests, headless runs, and any host that does its own
 * frame pacing.
 */
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

/**
 * Paces with std::chrono and sleep_for. Unit-correct.
 *
 * @note Sleeping once per instruction cannot actually pace a 1 MHz CPU: a
 * 2-cycle instruction is 2 us, while sleep_for granularity is roughly 1-15 ms
 * on a desktop OS. Expect this to run slower than the requested speed. For
 * accurate throttling, use default_clock and sleep once per frame instead.
 */
class chrono_clock : public default_clock {
private:
	double m_cycle_time;
	/** Absolute time the emulated CPU should have reached; advanced per call. */
	std::chrono::steady_clock::time_point m_deadline;
public:
	/**
	 * Initialize the clock with the speed in MHz
	 */
	chrono_clock(double speed);
	virtual ~chrono_clock();

	virtual void beginCycle();
	virtual void waitCycles(int cycles);
};

/**
 * Tighter pacing using the platform's high-resolution timer.
 *
 * Sleeps for all but the last millisecond of a wait and spins out the
 * remainder, which is accurate to well under a microsecond without burning a
 * core on long waits. Uses QueryPerformanceCounter on Windows (converted from
 * its own frequency units to nanoseconds) and clock_gettime(CLOCK_MONOTONIC)
 * on Linux.
 *
 * @note Still called once per instruction, so it cannot make up for the fact
 * that a 2-cycle instruction is 2 us. For steady throttling, prefer
 * default_clock plus your own per-frame pacing.
 */
class precision_clock : public default_clock {
private:
	double m_cycle_time;
	/** Absolute nanosecond deadline the emulated CPU should have reached. */
	unsigned long long m_deadline;
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
