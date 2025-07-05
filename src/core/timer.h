#pragma once

#include <chrono>

template<typename Period, typename Rep = float>
class Timer
{
	using Time	   = std::chrono::time_point<std::chrono::steady_clock>;
	using Duration = std::chrono::duration<Rep, Period>;
	using Self	   = Timer<Period, Rep>;

public:
	Timer() : _start_time(get_time()) {}

private:
	static Time get_time() { return std::chrono::high_resolution_clock::now(); }

private:
	const Time _start_time;

public:
	[[nodiscard]] Rep get() const
	{
		Duration duration{ get_time() - _start_time };
		return duration.count();
	}

	static Rep measure(auto&& func)
	{
		Self timer;
		func();
		return timer.get();
	}
};

typedef Timer<std::milli>		TimerMilli;
typedef Timer<std::ratio<1, 1>> TimerSec;
