#include "hidden_process.h"

#include "utils.h"

bool runHiddenProcess(const std::vector<std::string>& args, uint32_t timeout_ms)
{
	if (args.empty())
		return false;

	auto quote = [](const std::string& value)
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
	};

	std::string cmdline;
	for (auto& arg : args)
	{
		if (!cmdline.empty())
			cmdline += ' ';
		cmdline += quote(arg);
	}

	auto wide_cmd = utils::utf8ToUtf16(cmdline);
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
