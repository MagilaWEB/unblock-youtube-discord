export module core.timer;

import std;
import core.types;

export class Timer final
{
public:
	using Clock	   = std::chrono::high_resolution_clock;
	using Time	   = std::chrono::time_point<Clock>;
	using Duration = Time::duration;

protected:
	Time _start_time;

public:
	constexpr Timer() noexcept : _start_time() {}

	void start();

	Duration getElapsedTime() const;

	u64 getElapsedMs() const;

	u64 getElapsedMi() const;

	float getElapsedSec() const;

	Time now() const { return Clock::now(); }
};
