#pragma once 

class Stopwatch {
public:
	Stopwatch();
	void start();
	void stop();
	void reset();
	void restart();
	bool isRunning() const { return _isRunning; }
	std::int64_t elapsedTime() const;
	double elapsedSec() const;
	std::int64_t elapsedMSec() const;
	std::int64_t frequency() const { return _frequency.QuadPart; }

private:
	LARGE_INTEGER _frequency;
	LARGE_INTEGER _startTime;
	LARGE_INTEGER _elapsedTime;
	bool _isRunning;
};
