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

/**
 * Intended as a tighter-pacing clock using the platform's high-resolution timer.
 *
 * @warning Miscalibrated on Windows. nanoTime() returns a raw
 * QueryPerformanceCounter value, which is in QueryPerformanceFrequency units
 * (10 MHz, i.e. 100 ns per tick, on typical hardware) rather than nanoseconds.
 * waitCycles() subtracts those ticks from a nanosecond budget, so elapsed time
 * is under-counted by ~100x and the clock over-sleeps. The Linux path uses
 * clock_gettime(CLOCK_MONOTONIC) and is genuinely nanoseconds.
 *
 * @warning The tighter busy-wait loop is behind `#ifdef USE_PRECISION_CLOCK`,
 * which the build never defines — so it currently falls back to sleep_for and
 * behaves no better than chrono_clock.
 *
 * Prefer default_clock until this is fixed.
 */
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
