#ifdef __clang__
	#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
	#pragma clang diagnostic ignored "-Wc++20-compat-pedantic"
	#pragma clang diagnostic ignored "-Wswitch-enum"
	#pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif

#ifndef NDEBUG
	#define DEBUG
constexpr bool debug = true;
#else
constexpr bool debug = false;
#endif

#define FORWARD_CALL(expr) [&] { expr; }
