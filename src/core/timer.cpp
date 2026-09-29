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

u64 Timer::getElapsedMs() const
{
	return static_cast<u64>(duration_cast<milliseconds>(getElapsedTime()).count());
}

u64 Timer::getElapsedMi() const
{
	return static_cast<u64>(duration_cast<microseconds>(getElapsedTime()).count());
}

float Timer::getElapsedSec() const
{
	return duration_cast<duration<float>>(getElapsedTime()).count();
}
