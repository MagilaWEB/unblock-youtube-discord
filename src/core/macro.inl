#ifdef __clang__
	#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
	#pragma clang diagnostic ignored "-Wc++20-compat-pedantic"
	#pragma clang diagnostic ignored "-Wswitch-enum"
	#pragma clang diagnostic ignored "-Wcovered-switch-default"
	#pragma clang diagnostic ignored "-Winvalid-token-paste"
	#pragma clang diagnostic ignored "-Wheader-hygiene"
#else
	#pragma warning(disable : 4'251)
	#pragma warning(disable : 4'275)
#endif

#ifndef NDEBUG
	#define DEBUG
constexpr bool debug = true;
#else
constexpr bool debug = false;
#endif

#define FORWARD_CALL(expr) [&] { expr; }
