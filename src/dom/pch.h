#pragma once

// dom — part of ui, a separate STATIC lib. Deliberately does NOT pull
// core/ui to avoid a dependency cycle (see dom_view.hpp).
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
