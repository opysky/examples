#include "stdafx.h"
#include "FPSCounter.h"

FPSCounter::FPSCounter(double interval) : _interval(interval) {
	assert(_interval > 0);
	reset();
}

void FPSCounter::reset() {
    _elapsed = 0;
	_frameCount = 0;
	_fps = 0;
}

void FPSCounter::update(double deltaTime) {
	++_frameCount;
	_elapsed += deltaTime;
	if (_elapsed > _interval) {
		_fps = _frameCount / _elapsed;
        _elapsed = 0;
		_frameCount = 0;
	}
}
