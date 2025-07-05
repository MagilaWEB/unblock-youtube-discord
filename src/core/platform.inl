#if defined(PLATFORM_POSIX) || defined(__linux__)
	#define LINUX
#elif defined(_WIN32) || defined(__CYGWIN__)
	#define WINDOWS
	#include "windows.h"
#else
static_assert(false, "unrecognized platform");
#endif

#if defined(WINDOWS)
	#define EXPORT __declspec(dllexport)
	#define IMPORT __declspec(dllimport)
#elif defined(LINUX)
	#define EXPORT __attribute__((visibility("default")))
	#define IMPORT
#endif

#define PLATFORM_FATAL static_assert(false, "unimplemented platform specific code!")
