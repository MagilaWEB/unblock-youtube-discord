#include "timer.h"

using namespace std::chrono;

void Timer::Start()
{
	_start_time = Now();
}

Timer::Duration Timer::getElapsedTime() const
{
	return Now() - _start_time;
}

u64 Timer::GetElapsed_ms() const
{
	return duration_cast<milliseconds>(getElapsedTime()).count();
}

u64 Timer::GetElapsed_mi() const
{
	return duration_cast<microseconds>(getElapsedTime()).count();
}

float Timer::GetElapsed_sec() const
{
	return duration_cast<duration<float>>(getElapsedTime()).count();
}
