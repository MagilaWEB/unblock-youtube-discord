module;

#include "version.hpp"

export module engine.version;

import std;

// version.hpp is generated during the build (target unblock_version) and stays a
// header for two reasons: engine.rc needs its macros textually, because
// VERSION_NUMBER is a comma-separated list for the resource compiler, and module
// scanning runs before the generator does. The C++ side takes the values from
// here instead of pulling the macros into every translation unit.
//
// Macros do not cross a module boundary, so each value is captured before being
// dropped and re-declared as a constant with the same name and type: the call
// sites read exactly as they did with the macro.

#define UB_VERSION_VALUES(X) \
	X(VERSION_STR)           \
	X(VERSION_FULL)          \
	X(VERSION_BUMP)          \
	X(VERSION_HASH)          \
	X(VERSION_BASE_TAG)      \
	X(VERSION_DISTANCE)      \
	X(VERSION_RELEASE)       \
	X(VERSION_DIRTY)

#define UB_VERSION_CAPTURE(name) inline constexpr auto ub_##name = name;
UB_VERSION_VALUES(UB_VERSION_CAPTURE)
#undef UB_VERSION_CAPTURE

#undef VERSION_STR
#undef VERSION_FULL
#undef VERSION_BUMP
#undef VERSION_HASH
#undef VERSION_BASE_TAG
#undef VERSION_DISTANCE
#undef VERSION_RELEASE
#undef VERSION_DIRTY

#define UB_VERSION_EXPORT(name) export inline constexpr auto name = ub_##name;
UB_VERSION_VALUES(UB_VERSION_EXPORT)
#undef UB_VERSION_EXPORT
