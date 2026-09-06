#pragma once

// dom — part of ui, a separate STATIC lib. Depends on core (types/concepts/Debug
// via utils_saucer.hpp) and saucer, but deliberately does NOT pull ui
// to avoid a dependency cycle (see dom_view.hpp).
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// utils_saucer.hpp needs core context (types/concepts/Debug); debug.h is not
// self-contained (CriticalSection, `debug` flag, windows.h for __forceinline,
// std::regex) and relies on core pch — same as src/ui/pch.h does.
#include "../core/pch.h"
