#pragma once

#include "Stopwatch.h"

class FPSCounter {
public:
	FPSCounter(double interval = 1);
    void reset();
	void update(double deltaTime);
	double currentFps() const { return _fps; }
	double interval() const { return _interval; }
	void setInterval(double interval) { this->_interval = interval; }

private:
	double _elapsed;
	double _interval;
	int _frameCount;
	double _fps;
};