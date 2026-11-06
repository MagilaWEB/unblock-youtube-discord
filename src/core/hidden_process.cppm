export module core.hidden_process;

import std;
import core.types;

/** Runs a process without a console window and waits up to timeout_ms.
 *  args[0] is the executable, resolved via PATH when it has no directory.
 *
 * Returns true when the process exits with code 0. */
export bool runHiddenProcess(const std::vector<std::string>& args, std::uint32_t timeout_ms);

export struct ProcessCaptureResult
{
	bool		started{ false };
	bool		timed_out{ false };
	u32			exit_code{ 0 };
	std::string out;
	std::string err;
};

/** Runs a process without a console window, capturing its stdout and stderr.
 *  args[0] is the executable. After timeout_ms the process is terminated and
 *  timed_out is set, so a still-running process is reported as timed_out.
 *  started is false when the process could not be created at all. */
export ProcessCaptureResult runHiddenProcessCapture(std::span<const std::string> args, u32 timeout_ms);
