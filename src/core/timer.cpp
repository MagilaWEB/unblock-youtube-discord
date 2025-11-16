#include "timer.h"

using namespace std::chrono;

void Timer::start()
{
	_start_time = now();
}

Timer::Duration Timer::getElapsedTime() const
{
	return now() - _start_time;
}

u64 Timer::getElapsed_ms() const
{
	return duration_cast<milliseconds>(getElapsedTime()).count();
}

u64 Timer::getElapsed_mi() const
{
	return duration_cast<microseconds>(getElapsedTime()).count();
}

float Timer::getElapsed_sec() const
{
	return duration_cast<duration<float>>(getElapsedTime()).count();
}
