#include "stdafx.h"
#include "Stopwatch.h"

Stopwatch::Stopwatch() {
	QueryPerformanceFrequency(&_frequency);
	reset();
}

void Stopwatch::start() {
	if (!_isRunning) {
		QueryPerformanceCounter(&_startTime);
		_isRunning = true;
	}
}

void Stopwatch::stop() {
	if (_isRunning) {
		LARGE_INTEGER currentTime;
		QueryPerformanceCounter(&currentTime);
		_elapsedTime.QuadPart += currentTime.QuadPart - _startTime.QuadPart;
		_isRunning = false;
	}
}

void Stopwatch::reset() {
	_startTime.QuadPart = 0;
	_elapsedTime.QuadPart = 0;
	_isRunning = false;
}

void Stopwatch::restart() {
	reset();
	start();
}

std::int64_t Stopwatch::elapsedTime() const {
	std::int64_t elapsed = _elapsedTime.QuadPart;
	if (_isRunning) {
		LARGE_INTEGER currentTime;
		QueryPerformanceCounter(&currentTime);
		elapsed += currentTime.QuadPart - _startTime.QuadPart;
	}
	return elapsed;
}

double Stopwatch::elapsedSec() const {
	return elapsedTime() / static_cast<double>(_frequency.QuadPart);
}

std::int64_t Stopwatch::elapsedMSec() const {
	return static_cast<std::uint64_t>(elapsedSec() * 1000);
}
