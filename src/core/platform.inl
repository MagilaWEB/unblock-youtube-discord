#if defined(PLATFORM_POSIX) || defined(__linux__)
	#define LINUX
#elif defined(_WIN32) || defined(__CYGWIN__)
	#define WINDOWS
	#include "windows.h"
#else
static_assert(false, "unrecognized platform");
#endif

#define PLATFORM_FATAL static_assert(false, "unimplemented platform specific code!")
