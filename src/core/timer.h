#pragma once
class CORE_API Timer final
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

	virtual Duration getElapsedTime() const;

	u64 getElapsed_ms() const;

	u64 getElapsed_mi() const;

	float getElapsed_sec() const;

	Time now() const { return Clock::now(); }
};

#if __clang__
	#define LIMIT_UPDATE(name_time, sec, code)              \
		{                                                   \
			[[clang::no_destroy]] static Timer name_time{}; \
			if (name_time.GetElapsed_sec() > sec)           \
			{                                               \
				name_time.Start();                          \
				code                                        \
			}                                               \
		}

	#define LIMIT_UPDATE_FPS(name_time, fps, code)            \
		{                                                     \
			[[clang::no_destroy]] static Timer name_time{};   \
			if ((name_time.GetElapsed_ms()) >= (1'000 / fps)) \
			{                                                 \
				name_time.Start();                            \
				code                                          \
			}                                                 \
		}
#else
	#define LIMIT_UPDATE(name_time, sec, code)    \
		{                                         \
			static Timer name_time{};             \
			if (name_time.GetElapsed_sec() > sec) \
			{                                     \
				name_time.Start();                \
				code                              \
			}                                     \
		}

	#define LIMIT_UPDATE_FPS(name_time, fps, code)            \
		{                                                     \
			static Timer name_time{};                         \
			if ((name_time.GetElapsed_ms()) >= (1'000 / fps)) \
			{                                                 \
				name_time.Start();                            \
				code                                          \
			}                                                 \
		}

#endif
