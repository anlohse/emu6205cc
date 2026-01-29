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

uint64_t nanoTime() {
	uint64_t time = 0;
	QueryPerformanceCounter((LARGE_INTEGER *) &time);
	return time;
}

uint64_t nanocallMeasure() {

	uint64_t time1 = 0, time2 = 0;
	time1 = nanoTime();
	for (int i = 0; i < 1000; i++) {
		time2 = nanoTime();
	}
	return (time2 - time1) / 1000LL;
}

uint64_t _1nanotimeMeasure = nanocallMeasure();

void _nanosleep(uint64_t ns){
#ifdef USE_PRECISION_CLOCK
	uint64_t time1 = 0, time2 = 0;
	ns -=_1nanotimeMeasure*2;
	QueryPerformanceCounter((LARGE_INTEGER *) &time1);
	do {
		Sleep(0);
		QueryPerformanceCounter((LARGE_INTEGER *) &time2);
	} while((time2-time1) < ns);
#else
    std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
#endif
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

precision_clock::precision_clock(double speed) :
		m_cycle_time(0.000001 / speed), m_begin_time(0) {
}

precision_clock::~precision_clock() {
}

void precision_clock::beginCycle() {
	m_begin_time = nanoTime();
}

void precision_clock::waitCycles(int cycles) {
	default_clock::waitCycles(cycles);
	uint64_t _end_time = nanoTime();
	int64_t dur = _end_time - m_begin_time;
	int64_t tm = (int64_t)(cycles * m_cycle_time * 1e9) - dur;
	m_begin_time = _end_time;
	if (tm > 0)
		_nanosleep(tm);
}

chrono_clock::chrono_clock(double speed) : m_cycle_time(0.000001/speed), m_begin_time() {
}
chrono_clock::~chrono_clock() {
}
void chrono_clock::beginCycle() {
	m_begin_time = std::chrono::high_resolution_clock::now();
}

void chrono_clock::waitCycles(int cycles) {
	default_clock::waitCycles(cycles);
	std::chrono::high_resolution_clock::time_point _end_time = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> dur = _end_time - m_begin_time;
	int64_t tm = (int64_t)((cycles * m_cycle_time  - dur.count()) * 1e9);
	m_begin_time = _end_time;
	if (tm > 0)
		std::this_thread::sleep_for(std::chrono::nanoseconds(tm));
}
