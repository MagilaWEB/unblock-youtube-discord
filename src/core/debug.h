#pragma once

#include <iostream>
#include <stacktrace>
#include <atomic>
#include "file_system.h"

class Debug
{
	enum class MessageTypes
	{
		Print,
		Ok,
		Info,
		Warning,
		Please,
		Error,
		Fatal
	};

	inline static std::string	   _command_line{};
	inline static std::string	   _version_str{};
	inline static size_t		   _console_line{ 0 };
	inline static CriticalSection  _lock;
	inline static std::atomic_bool _crash_handler_enabled{ true };

public:
	using exception = std::runtime_error;

	inline static File s_log;
	inline static File s_log_backup;

public:
	Debug()		   = delete;
	~Debug()	   = delete;
	Debug(Debug&&) = delete;

private:
	inline static bool s_error_fatal{ debug };
	inline static bool s_catch_exceptions{ !debug };

	[[nodiscard]] static std::string_view _getPrefix(MessageTypes type);

	static std::string _stripAnsi(const std::string& s)
	{
		static const std::regex ansi_re("\\x1B\\[[0-9;]*m");
		return std::regex_replace(s, ansi_re, "");
	}

	template<typename... Args>
	static void _msg(MessageTypes type, std::string_view message, Args&&... args)
	{
		CriticalSection::Raii mt{ _lock };

		std::string str = utils::format(message, args...);

		const bool error_state = std::to_underlying(type) >= std::to_underlying(MessageTypes::Error);
		if (error_state)
			str.append("\n" + prettyStacktrace());

		s_log.writeText(std::to_string(_console_line) + ". " + _stripAnsi(str));

		auto log_str = std::format("{}. {}{}", ++_console_line, _getPrefix(type), str);
		(error_state ? std::cerr : std::cout) << log_str.c_str() << std::endl;

		if (type == MessageTypes::Fatal || (type == MessageTypes::Error && s_error_fatal))
			throw(exception(str.c_str()));
	}

	[[noreturn]] static void _cppTerminateHandler()
	{
		std::string msg;

		if (auto eptr = std::current_exception())
		{
			try
			{
				std::rethrow_exception(eptr);
			}
			catch (const std::exception& e)
			{
				msg	 = "C++ Exception: ";
				msg += e.what();
			}
			catch (...)
			{
				msg = "Unknown C++ exception type.";
			}
		}
		else
		{
			msg = "std::terminate called without active exception.";
		}

		msg += "\n\n";
		msg += Debug::prettyStacktrace();

		Debug::winApiWindowShow("str_error", msg.c_str());

		// Write the current crash into the log BEFORE reading its tail,
		// so the crash message + stack trace appear once as the last log entry.
		Debug::fatalErrorMessage(msg.c_str());

		const std::string log_tail = readLogTail(150);

		// Automatically open a GitHub issue with a crash report.
		const std::string title = utils::format(Localization::Str{ "str_issue_crash_title" }(), utils::format("0x{:08X}", 0u));
		openGitHubIssue(title, buildCrashIssueBody(log_tail));

		Debug::s_log.close();
		std::abort();
	}

public:
	static const std::string& commandLine() { return _command_line; }

	/** Report a problem on GitHub: opens the issue creation form in the browser
	 *  with pre-filled title and body (template + information). */
	static void openGitHubIssue(const std::string& title, const std::string& body);

	/** Enable/disable the crash handler (vectored exception handler + SEH filter).
	 *  Disabled in tests so a crash reports to the test runner instead of showing a dialog. */
	static void setCrashHandlerEnabled(bool enabled);
	static bool crashHandlerEnabled() { return _crash_handler_enabled; }

	/** Builds the crash report body (version + log tail + user template). */
	static std::string buildCrashIssueBody(const std::string& log_tail);

	/** Builds the manual bug report body (version + user template). */
	static std::string buildReportIssueBody();

	/** Reads the last tail_lines lines from logs/log.txt. */
	static std::string readLogTail(size_t tail_lines);

	static void				  setVersion(std::string_view version) { _version_str = version; }
	static const std::string& version() { return _version_str; }

	static void initialize(const std::string& command_line);
	static void initLogFile();
	static void fatalErrorMessage(std::string message);

	template<typename... Args>
	static void winApiWindowShow(pcstr title, pcstr desc, Args&&... args)
	{
		Localization::Str text_lang_title{ title };
		Localization::Str text_lang_desc{ desc };
		auto			  desc_format = text_lang_desc();
		std::string		  format;

		if (desc != desc_format)
			format = utils::format(desc_format, args...);
		else
			format = desc_format;

		MessageBoxA(nullptr, utils::utf8ToCp1251(format.c_str()).c_str(), utils::utf8ToCp1251(text_lang_title()).c_str(), MB_OK);
	}

	template<typename Fn, typename... Args>
	static int tryWrap(Fn&& fn, Args&&... args)
	{
		if (s_catch_exceptions)
		{
			try
			{
				fn(std::forward<Args>(args)...);
			}
			catch (...)
			{
				_cppTerminateHandler();
			}
		}
		else
			fn(std::forward<Args>(args)...);

		return 0;
	}

	template<typename... Args>
	__forceinline static std::unexpected<std::string> strUnexpected(std::string_view fmt, Args&&... args)
	{
		return std::unexpected(utils::format(fmt, args...));
	}

	template<typename... Args>
	static void print(std::string_view message, Args&&... args)
	{
		_msg(MessageTypes::Print, message, args...);
	}

	template<typename... Args>
	static void ok(std::string_view message, Args&&... args)
	{
		_msg(MessageTypes::Ok, message, args...);
	}

	template<typename... Args>
	static void info(std::string_view message, Args&&... args)
	{
		_msg(MessageTypes::Info, message, args...);
	}

	template<typename... Args>
	static void warning(std::string_view message, Args&&... args)
	{
		_msg(MessageTypes::Warning, message, args...);
	}

	template<typename... Args>
	static void please(std::string_view message, Args&&... args)
	{
		_msg(MessageTypes::Please, message, args...);
	}

	/** Display error message and exit in certain conditions */
	template<typename... Args>
	static void error(std::string_view message, Args&&... args)
	{
		_msg(MessageTypes::Error, message, args...);
	}

	/** Display error message and exit anyway */
	template<typename... Args>
	[[noreturn]] static void fatal(std::string_view message, Args&&... args)
	{
		_msg(MessageTypes::Fatal, message, args...);
	}

	/** Check condition and throw warning if it fails */
	template<typename... Args>
	static void check(bool condition, std::string_view message, Args&&... args)
	{
		if (!condition)
			warning(message, args...);
	}

	/** Check condition and throw error if it fails */
	template<typename... Args>
	static void verify(bool condition, std::string_view message, Args&&... args)
	{
		if (!condition)
			error(message, args...);
	}

	/** Check condition and fatal if it fails */
	template<typename... Args>
	static void assertion(bool condition, std::string_view message, Args&&... args)
	{
		if (!condition)
			fatal(message, args...);
	}

	static std::string prettyStacktrace();
};

#define VERIFY(expr)                                                                            \
	Debug::verify(                                                                              \
		!!(expr),                                                                               \
		"VERIFICATION FAILED!\n\tExpression: \t{}\n\tFile: \t{}\n\tLine: {}\n\tFunction: \t{}", \
		#expr,                                                                                  \
		__FILE__,                                                                               \
		__LINE__,                                                                               \
		__FUNCTION__                                                                            \
	)

#define ASSERT(expr)                                                                           \
	Debug::assertion(                                                                          \
		!!(expr),                                                                              \
		"ASSERTION FAILED!\n\tExpression: \t{}\n\tFile: \t{}\n\tLine: \t{}\n\tFunction: \t{}", \
		#expr,                                                                                 \
		__FILE__,                                                                              \
		__LINE__,                                                                              \
		__FUNCTION__                                                                           \
	)

#define ASSERT_ARGS(expr, msg, ...)                                                                                             \
	Debug::assertion(                                                                                                           \
		!!(expr),                                                                                                               \
		std::string{ "ASSERTION FAILED!\n\tExpression: \t{}\n\tFile: \t{}\n\tLine: \t{}\n\tFunction: \t{}\n\n\t" }.append(msg), \
		#expr,                                                                                                                  \
		__FILE__,                                                                                                               \
		__LINE__,                                                                                                               \
		__FUNCTION__,                                                                                                           \
		__VA_ARGS__                                                                                                             \
	)
