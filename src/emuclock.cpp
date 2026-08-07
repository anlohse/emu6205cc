/*
 * p6502clock.cpp
 *
 *  Created on: 19 de jul. de 2021
 *      Author: alanl
 */

#include <iostream>
#include <thread>
#include <chrono>
#include "../include/6502cc/emu_clock.h"

default_clock::default_clock(): m_current_cycles(0) {
}
default_clock::~default_clock() {
}

void default_clock::beginCycle() {
}
void default_clock::waitCycles(int cycles) {
	m_current_cycles += cycles;
}
uint64_t default_clock::cycles() const {
	return m_current_cycles;
}

void default_clock::reset() {
	m_current_cycles = 0;
}


#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#include <windows.h>

/*
 * QueryPerformanceCounter returns ticks in QueryPerformanceFrequency units --
 * typically 10 MHz, i.e. 100 ns per tick -- not nanoseconds. Converting is what
 * makes this clock agree with the nanosecond budgets in waitCycles().
 *
 * The counter is rebased on first use so that (ticks * 1e9) cannot overflow, and
 * the conversion is split into whole seconds plus remainder to keep full
 * precision without a wider integer type.
 */
static uint64_t qpcFrequency() {
	LARGE_INTEGER f;
	QueryPerformanceFrequency(&f);
	return (uint64_t) f.QuadPart;
}

static uint64_t qpcTicks() {
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return (uint64_t) t.QuadPart;
}

uint64_t nanoTime() {
	static const uint64_t freq = qpcFrequency();
	static const uint64_t base = qpcTicks();
	uint64_t ticks = qpcTicks() - base;
	return (ticks / freq) * 1000000000ULL + ((ticks % freq) * 1000000000ULL) / freq;
}

/*
 * Sleep for ns nanoseconds.
 *
 * sleep_for on Windows has a granularity of roughly 1-15 ms, which is far
 * coarser than a single 6502 cycle. Sleeping for all but the last millisecond
 * and spinning out the remainder keeps the wait accurate without burning a core
 * on long waits.
 */
void _nanosleep(uint64_t ns) {
	const uint64_t SPIN_THRESHOLD = 1000000ULL; // 1 ms
	uint64_t deadline = nanoTime() + ns;
	if (ns > SPIN_THRESHOLD)
		std::this_thread::sleep_for(std::chrono::nanoseconds(ns - SPIN_THRESHOLD));
	while (nanoTime() < deadline)
		YieldProcessor();
}

#elif defined __linux__
#include <time.h>

uint64_t nanoTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void _nanosleep(uint64_t ns) {
    struct timespec ts;
    ts.tv_sec = ns / 1000000000ULL;
    ts.tv_nsec = ns % 1000000000ULL;
    nanosleep(&ts, NULL);
}
#endif

/*
 * Both pacing clocks track an absolute deadline rather than measuring each
 * interval independently.
 *
 * Measuring per-interval and resetting the origin on every call double-counts:
 * the time spent asleep in one call is charged against the next call's budget,
 * so the CPU ends up running at roughly twice the requested speed. Accumulating
 * a deadline also makes the pacing self-correcting -- an oversleep caused by
 * coarse timer granularity is absorbed by not sleeping on subsequent calls
 * until the deadline catches up.
 *
 * If the host falls further behind than RESYNC_NS -- a debugger breakpoint, a
 * descheduled thread -- the deadline is snapped to the present instead of
 * running flat out trying to make up time that no longer matters.
 */
static const int64_t RESYNC_NS = 50000000; // 50 ms

precision_clock::precision_clock(double speed) :
		m_cycle_time(0.000001 / speed), m_deadline(0) {
}

precision_clock::~precision_clock() {
}

void precision_clock::beginCycle() {
	m_deadline = nanoTime();
}

void precision_clock::waitCycles(int cycles) {
	default_clock::waitCycles(cycles);
	m_deadline += (uint64_t) (cycles * m_cycle_time * 1e9);
	uint64_t now = nanoTime();
	if (m_deadline > now)
		_nanosleep(m_deadline - now);
	else if ((int64_t) (now - m_deadline) > RESYNC_NS)
		m_deadline = now;
}

chrono_clock::chrono_clock(double speed) : m_cycle_time(0.000001/speed), m_deadline() {
}
chrono_clock::~chrono_clock() {
}
void chrono_clock::beginCycle() {
	m_deadline = std::chrono::steady_clock::now();
}

void chrono_clock::waitCycles(int cycles) {
	default_clock::waitCycles(cycles);
	m_deadline += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
			std::chrono::duration<double>(cycles * m_cycle_time));
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	if (m_deadline > now)
		std::this_thread::sleep_for(m_deadline - now);
	else if (std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_deadline).count() > RESYNC_NS)
		m_deadline = now;
}
