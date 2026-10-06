#include "debug.h"

namespace
{
	// URL percent-encoding (UTF-8): every byte except unreserved characters is encoded as %XX.
	std::string urlEncode(std::string_view str)
	{
		static constexpr pcstr hex_digits = "0123456789ABCDEF";

		std::string encoded;
		encoded.reserve(str.size() * 3);

		const auto isUnreserved = [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~'; };

		for (const unsigned char c : str)
		{
			if (isUnreserved(c))
				encoded.push_back(static_cast<char>(c));
			else
			{
				encoded.push_back('%');
				encoded.push_back(hex_digits[(c >> 4) & 0x0F]);
				encoded.push_back(hex_digits[c & 0x0F]);
			}
		}

		return encoded;
	}

	std::string readLogTailLines(size_t tail_lines)
	{
		File log_file;
		log_file.open(Core::get().currentPath() / "logs" / "log", ".txt", true);
		if (!log_file.isOpen())
			return "(log file not found)";

		std::vector<std::string> lines;
		lines.reserve(tail_lines);

		for (auto& line : log_file)
		{
			lines.push_back(std::move(line));
			if (lines.size() > tail_lines)
				lines.erase(lines.begin());
		}

		std::string tail;
		for (const auto& line : lines)
			tail.append(line).append("\n");

		return tail;
	}
}	 // namespace

std::string_view Debug::_getPrefix(MessageTypes type)
{
#ifdef WINDOWS
	switch (type)
	{
	case MessageTypes::Print:
		return "";
	case MessageTypes::Ok:
		return "\x1B[32mOK: \033[0m";
	case MessageTypes::Info:
		return "\x1B[34mINFO: \033[0m";
	case MessageTypes::Warning:
		return "\x1B[33m~WARNING: \033[0m";
	case MessageTypes::Please:
		return "\x1B[35m~PLEASE: \033[0m";
	case MessageTypes::Error:
		return "\x1B[31m!ERROR: \033[0m";
	case MessageTypes::Fatal:
		return "\x1B[31m!FATAL: \033[0m";
	default:
		error("unexpected debug message type {}", static_cast<u32>(type));
	}
#else
	switch (type)
	{
	case MessageTypes::Print:
		return "";
	case MessageTypes::Ok:
		return "OK: ";
	case MessageTypes::Info:
		return "INFO: ";
	case MessageTypes::Warning:
		return "~WARNING: ";
	case MessageTypes::Error:
		return "!ERROR: ";
	case MessageTypes::Fatal:
		return "!FATAL: ";
	default:
		error("unexpected debug message type {}", static_cast<u32>(type));
	}
#endif
	return "";
}

namespace
{
	// Prevent duplicate handling when multiple handlers catch the same exception.
	bool crash_handled{ false };

	std::string langStr(std::string_view key)
	{
		return Localization::Str{ key }();
	}

	// Builds the common user template section: "what were you doing" + reproduce steps + expected/actual.
	std::string userTemplate(bool crash)
	{
		return utils::format(
			"{}\n\n"
			"{}\n"
			"...\n\n"
			"{}\n"
			"1. ...\n"
			"2. ...\n"
			"3. ...\n\n"
			"{}\n"
			"...\n\n"
			"{}\n"
			"...\n",
			langStr("str_issue_describe_header"),
			crash ? langStr("str_issue_before_crash") : langStr("str_issue_report_description"),
			langStr("str_issue_steps"),
			langStr("str_issue_expected"),
			langStr("str_issue_actual")
		);
	}

	std::string buildCrashMessage(PEXCEPTION_RECORD record)
	{
		std::string msg = "SEH Exception (Crash) caught!\n";

		switch (record->ExceptionCode)
		{
		case EXCEPTION_ACCESS_VIOLATION:
			msg += "Cause: Access Violation (Invalid pointer)\n";
			break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			msg += "Cause: Integer Division by Zero\n";
			break;
		case EXCEPTION_STACK_OVERFLOW:
			msg += "Cause: Stack Overflow\n";
			break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			msg += "Cause: Illegal Instruction\n";
			break;
		default:
			msg += "Cause: Unknown SEH Exception\n";
			break;
		}

		if (record->ExceptionCode != EXCEPTION_STACK_OVERFLOW)
		{
			try
			{
				msg += "\n\n";
				msg += Debug::prettyStacktrace();
			}
			catch (...)
			{
				msg += "\n\n(Stack trace collection failed due to corrupted state)";
			}
		}
		else
			msg += "\n\n(Stack trace unavailable: stack overflow)";

		return msg;
	}

	[[noreturn]] void handleCrash(const std::string& msg, u32 error_code)
	{
		if (crash_handled)
			ExitProcess(1);

		crash_handled = true;

		Debug::winApiWindowShow("str_error", msg.c_str());

		// Write the current crash into the log BEFORE reading its tail,
		// so the crash message + stack trace appear once as the last log entry.
		Debug::fatalErrorMessage(msg);

		Debug::openGitHubIssue(
			utils::format(langStr("str_issue_crash_title"), utils::format("0x{:08X}", error_code)),
			Debug::buildCrashIssueBody(Debug::readLogTail(150))
		);

		Debug::s_log.close();

		// Terminate immediately: the process state is corrupted.
		ExitProcess(static_cast<UINT>(0xC0'00'00'05));
	}
}	 // namespace

static LONG WINAPI seh_unhandled_filter(_EXCEPTION_POINTERS* pExceptionInfo)
{
	// If the crash handler is disabled (tests), let the process crash normally.
	if (!Debug::crashHandlerEnabled())
		return EXCEPTION_CONTINUE_SEARCH;

	// Ignore non-fatal exceptions (e.g. debug break exceptions raised by OutputDebugString).
	if ((pExceptionInfo->ExceptionRecord->ExceptionCode & 0x80'00'00'00U) == 0)
		return EXCEPTION_CONTINUE_SEARCH;

	if (pExceptionInfo->ExceptionRecord->ExceptionCode == 0xE0'6D'73'63)
		return EXCEPTION_CONTINUE_SEARCH;

	handleCrash(buildCrashMessage(pExceptionInfo->ExceptionRecord), pExceptionInfo->ExceptionRecord->ExceptionCode);

	// Unreachable (handleCrash terminates the process).
	return EXCEPTION_EXECUTE_HANDLER;
}

// Vectored handler: catches every exception (including access violation) at the
// OS level before the CRT/SEH machinery, regardless of compiler flags.
static LONG CALLBACK vectored_exception_handler(PEXCEPTION_POINTERS pExceptionInfo)
{
	// If the crash handler is disabled (tests), let the process crash normally.
	if (!Debug::crashHandlerEnabled())
		return EXCEPTION_CONTINUE_SEARCH;

	// Ignore non-fatal exceptions: those without the severity bit set
	// (e.g. DBG_PRINTEXCEPTION_C 0x4001000A used by OutputDebugString,
	// which Ultralight raises while logging). They must go through normally.
	if ((pExceptionInfo->ExceptionRecord->ExceptionCode & 0x80'00'00'00U) == 0)
		return EXCEPTION_CONTINUE_SEARCH;

	// Let C++ exceptions (0xE06D7363) go through the normal try/catch path.
	if (pExceptionInfo->ExceptionRecord->ExceptionCode == 0xE0'6D'73'63)
		return EXCEPTION_CONTINUE_SEARCH;

	handleCrash(buildCrashMessage(pExceptionInfo->ExceptionRecord), pExceptionInfo->ExceptionRecord->ExceptionCode);

	// Unreachable (handleCrash terminates the process).
	return EXCEPTION_CONTINUE_SEARCH;
}

namespace
{
	struct SEHFilterGuard
	{
		LPTOP_LEVEL_EXCEPTION_FILTER old_filter;
		PVOID						 old_vectored;

		SEHFilterGuard() :
			old_filter(SetUnhandledExceptionFilter(seh_unhandled_filter)),
			old_vectored(AddVectoredExceptionHandler(1, vectored_exception_handler))
		{
		}

		~SEHFilterGuard()
		{
			if (old_vectored)
				RemoveVectoredExceptionHandler(old_vectored);

			SetUnhandledExceptionFilter(old_filter);
		}
	} guard;	// NOLINT(bugprone-throwing-static-initialization) - WinAPI setup, never throws
}

void Debug::initialize(const std::string& command_line)
{
	s_catch_exceptions = true;
	s_error_fatal	   = true;

	_command_line = command_line;

	std::set_terminate(_cppTerminateHandler);
}

void Debug::initLogFile()
{
	static bool init{ false };
	if (init)
		return;

	init		  = true;
	auto dir_logs = Core::get().currentPath() / "logs";

	if (!std::filesystem::exists(dir_logs))
		std::filesystem::create_directories(dir_logs);

	s_log.open(dir_logs / "log", ".txt", true);
	if (s_log.isOpen())
	{
		s_log_backup.open(dir_logs / "log_backup", ".txt", true);
		if (s_log_backup.isOpen())
			s_log_backup.clear();

		for (auto& line : s_log)
			s_log_backup.writeText(line);

		s_log.clear();
	}
}

void Debug::fatalErrorMessage(std::string message)
{
	s_log.writeText(std::to_string(++_console_line) + ". " + message);
	s_log.close();
	std::cerr << message << '\n';
}

void Debug::setCrashHandlerEnabled(bool enabled)
{
	_crash_handler_enabled = enabled;
}

void Debug::openGitHubIssue(const std::string& title, const std::string& body)
{
	constexpr pcstr c_issue_url_base{ "https://github.com/MagilaWEB/unblock-youtube-discord/issues/new" };

	const std::string url = std::string{ c_issue_url_base } + "?title=" + urlEncode(title) + "&body=" + urlEncode(body);

	// ShellExecuteA is safe for '?' and '&' in the URL (unlike system("start ...")).
	ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::string Debug::buildCrashIssueBody(const std::string& log_tail)
{
	return utils::format(
		"{}\n\n"
		"{}\n\n"
		"<details>\n<summary>{}</summary>\n\n"
		"```\n{}\n```\n\n</details>\n\n"
		"{}\n\n"
		"{}",
		langStr("str_issue_crash_header"),
		utils::format(langStr("str_issue_version"), version()),
		langStr("str_issue_log_summary"),
		log_tail,
		userTemplate(true),
		langStr("str_issue_full_log_hint")
	);
}

std::string Debug::buildReportIssueBody()
{
	return utils::format(
		"{}\n\n"
		"{}\n\n"
		"{}",
		langStr("str_issue_report_header"),
		utils::format(langStr("str_issue_version"), version()),
		userTemplate(false)
	);
}

std::string Debug::readLogTail(size_t tail_lines)
{
	return readLogTailLines(tail_lines);
}

std::string Debug::prettyStacktrace()
{
	try
	{
		auto		trace  = std::stacktrace::current(1);
		std::string result = "Stacktrace (depth: " + std::to_string(trace.size()) + "):\n";

		int frame_num = 0;
		for (const auto& frame : trace)
		{
			std::string func = frame.description();
			if (func.empty())
				func = "???";

			std::string file = frame.source_file();
			u32			line = frame.source_line();

			std::string location;
			if (!file.empty())
				location = std::format("{}:{}", std::filesystem::path(file).filename().string(), line);
			else
				location = std::format("{:016x}", reinterpret_cast<uintptr_t>(frame.native_handle()));

			const int func_width = 86;
			if (func.length() > func_width)
				func = func.substr(0, func_width - 3) + "...";

			result += std::format("  #{:2} -> {:<{}} ({})\n", frame_num++, func, func_width, location);
		}

		return result;
	}
	catch (const std::exception& e)
	{
		return std::format("Stacktrace unavailable: {}", e.what());
	}
}
