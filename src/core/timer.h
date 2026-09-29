#pragma once
class Timer final
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

#define LIMIT_UPDATE(name_time, sec, code)   \
	{                                        \
		static Timer name_time{};            \
		if (name_time.getElapsedSec() > sec) \
		{                                    \
			name_time.start();               \
			code                             \
		}                                    \
	}

#define LIMIT_UPDATE_FPS(name_time, fps, code)           \
	{                                                    \
		static Timer name_time{};                        \
		if ((name_time.getElapsedMs()) >= (1'000 / fps)) \
		{                                                \
			name_time.start();                           \
			code                                         \
		}                                                \
	}
