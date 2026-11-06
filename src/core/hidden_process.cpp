module core.hidden_process;

import ext.windows;
import core.utils;

namespace
{
	std::string quoteArgument(const std::string& value)
	{
		std::string out{ "\"" };
		for (char ch : value)
		{
			if (ch == '"')
				out += '\\';
			out += ch;
		}
		out += '"';
		return out;
	}

	std::string buildCommandLine(const std::span<const std::string> args)
	{
		std::string cmdline;
		for (const auto& arg : args)
		{
			if (!cmdline.empty())
				cmdline += ' ';
			cmdline += quoteArgument(arg);
		}
		return cmdline;
	}

	// Reads whatever is already in the pipe without blocking. Returns false once
	// the pipe is broken (the writer is gone and nothing is left).
	bool drainPipe(HANDLE pipe, std::string& out)
	{
		DWORD available = 0;
		if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr))
			return false;

		char buffer[4'096];
		while (available > 0)
		{
			const DWORD want = available < sizeof(buffer) ? available : static_cast<DWORD>(sizeof(buffer));

			DWORD read = 0;
			if (!ReadFile(pipe, buffer, want, &read, nullptr) || read == 0)
				break;

			out.append(buffer, read);
			available -= read;
		}

		return true;
	}
}	 // namespace

bool runHiddenProcess(const std::vector<std::string>& args, std::uint32_t timeout_ms)
{
	if (args.empty())
		return false;

	auto wide_cmd = utils::utf8ToUtf16(buildCommandLine(args));
	if (wide_cmd.empty())
		return false;

	STARTUPINFOW		si{};
	PROCESS_INFORMATION pi{};
	si.cb = sizeof(si);

	if (!CreateProcessW(nullptr, wide_cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
		return false;

	if (WaitForSingleObject(pi.hProcess, timeout_ms) == WAIT_TIMEOUT)
		TerminateProcess(pi.hProcess, 1);

	DWORD code = 1;
	GetExitCodeProcess(pi.hProcess, &code);

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return code == 0;
}

ProcessCaptureResult runHiddenProcessCapture(const std::span<const std::string> args, u32 timeout_ms)
{
	ProcessCaptureResult result;
	if (args.empty())
		return result;

	auto wide_cmd = utils::utf8ToUtf16(buildCommandLine(args));
	if (wide_cmd.empty())
		return result;

	SECURITY_ATTRIBUTES sa{};
	sa.nLength		  = sizeof(sa);
	sa.bInheritHandle = TRUE;

	HANDLE out_read = nullptr, out_write = nullptr;
	HANDLE err_read = nullptr, err_write = nullptr;
	if (!CreatePipe(&out_read, &out_write, &sa, 0) || !CreatePipe(&err_read, &err_write, &sa, 0))
	{
		if (out_read)
			CloseHandle(out_read);
		if (out_write)
			CloseHandle(out_write);
		if (err_read)
			CloseHandle(err_read);
		if (err_write)
			CloseHandle(err_write);
		return result;
	}

	// The read ends stay in the parent and must not be inherited: the child
	// has to hold only the write ends, otherwise the pipe never breaks.
	SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0);

	// A GUI process has no usable stdin handle; with STARTF_USESTDHANDLES the
	// child would otherwise inherit a null handle. Hand it the NUL device.
	const HANDLE null_input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
	if (null_input == INVALID_HANDLE_VALUE)
	{
		CloseHandle(out_read);
		CloseHandle(out_write);
		CloseHandle(err_read);
		CloseHandle(err_write);
		return result;
	}

	STARTUPINFOW si{};
	si.cb		  = sizeof(si);
	si.dwFlags	  = STARTF_USESTDHANDLES;
	si.hStdInput  = null_input;
	si.hStdOutput = out_write;
	si.hStdError  = err_write;

	PROCESS_INFORMATION pi{};
	if (!CreateProcessW(nullptr, wide_cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
	{
		CloseHandle(null_input);
		CloseHandle(out_read);
		CloseHandle(out_write);
		CloseHandle(err_read);
		CloseHandle(err_write);
		return result;
	}

	CloseHandle(null_input);
	result.started = true;

	// The child owns the write ends from now on.
	CloseHandle(out_write);
	CloseHandle(err_write);

	const u64 deadline = GetTickCount64() + static_cast<u64>(timeout_ms);
	bool	  exited   = false;
	while (!exited)
	{
		exited = WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0;
		drainPipe(out_read, result.out);
		drainPipe(err_read, result.err);

		if (!exited && GetTickCount64() >= deadline)
		{
			result.timed_out = true;
			TerminateProcess(pi.hProcess, 1);
			WaitForSingleObject(pi.hProcess, INFINITE);
			exited = true;
		}
	}

	// Data written before the exit is still buffered in the pipe, take it now.
	drainPipe(out_read, result.out);
	drainPipe(err_read, result.err);

	DWORD code = 0;
	GetExitCodeProcess(pi.hProcess, &code);
	result.exit_code = static_cast<u32>(code);

	CloseHandle(out_read);
	CloseHandle(err_read);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return result;
}
