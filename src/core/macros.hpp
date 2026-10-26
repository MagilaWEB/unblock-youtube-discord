#pragma once

// Preprocessor-only definitions. Macros cannot be exported from a module, so
// the units that need them include this header textually.

#if defined(PLATFORM_POSIX) || defined(__linux__)
	#define LINUX
#elif defined(_WIN32) || defined(__CYGWIN__)
	#define WINDOWS
#else
static_assert(false, "unrecognized platform");
#endif

#define PLATFORM_FATAL static_assert(false, "unimplemented platform specific code!")

#define CRITICAL_SECTION_RAII(_lock, ...)  \
	CriticalSection::Raii mt_##__VA_ARGS__ \
	{                                      \
		_lock                              \
	}

#define FAST_LOCK(_lock, ...)       \
	FastLock::Raii mt_##__VA_ARGS__ \
	{                               \
		_lock                       \
	}
#define FAST_LOCK_SHARED(_lock, ...) \
	FastLock::Raii mt_##__VA_ARGS__  \
	{                                \
		_lock, true                  \
	}

#define VERIFY(expr)                                                                            \
	Debug::verify(                                                                              \
		!!(expr),                                                                               \
		"VERIFICATION FAILED!\n\tExpression: \t{}\n\tFile: \t{}\n\tLine: {}\n\tFunction: \t{}", \
		#expr,                                                                                  \
		__FILE__,                                                                               \
		__LINE__,                                                                               \
		__FUNCTION__                                                                            \
	)

#define ASSERT(expr)                                                                           \
	Debug::assertion(                                                                          \
		!!(expr),                                                                              \
		"ASSERTION FAILED!\n\tExpression: \t{}\n\tFile: \t{}\n\tLine: \t{}\n\tFunction: \t{}", \
		#expr,                                                                                 \
		__FILE__,                                                                              \
		__LINE__,                                                                              \
		__FUNCTION__                                                                           \
	)

#define ASSERT_ARGS(expr, msg, ...)                                                                                             \
	Debug::assertion(                                                                                                           \
		!!(expr),                                                                                                               \
		std::string{ "ASSERTION FAILED!\n\tExpression: \t{}\n\tFile: \t{}\n\tLine: \t{}\n\tFunction: \t{}\n\n\t" }.append(msg), \
		#expr,                                                                                                                  \
		__FILE__,                                                                                                               \
		__LINE__,                                                                                                               \
		__FUNCTION__,                                                                                                           \
		__VA_ARGS__                                                                                                             \
	)

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
