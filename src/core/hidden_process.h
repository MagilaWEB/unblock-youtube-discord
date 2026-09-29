#pragma once

#include <cstdint>
#include <string>
#include <vector>

/** Runs a process without a console window and waits up to timeout_ms.
 *  args[0] is the executable, resolved via PATH when it has no directory.
 *
 * Returns true when the process exits with code 0. */
bool runHiddenProcess(const std::vector<std::string>& args, uint32_t timeout_ms);
