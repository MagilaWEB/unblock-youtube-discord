#pragma once

#include <iostream>
#include <stacktrace>
#include "file_system.h"

class CORE_API Debug
{
	enum MessageTypes
	{
		ePrint,
		eOk,
		eInfo,
		eWarning,
		ePlease,
		eError,
		eFatal
	};

	inline static size_t		  _console_line{ 0 };
	inline static CriticalSection _lock;

public:
	using exception = std::runtime_error;

	inline static File log;
	inline static File log_backup;

public:
	Debug()		   = delete;
	~Debug()	   = delete;
	Debug(Debug&&) = delete;

private:
	inline static bool s_error_fatal{ debug };
	inline static bool s_catch_exceptions{ !debug };

	[[nodiscard]] static std::string_view get_prefix(MessageTypes type);

	static std::string strip_ansi(const std::string & s)
	{
		static const std::regex ansi_re("\\x1B\\[[0-9;]*m");
		return std::regex_replace(s, ansi_re, "");
	}

	template<typename... Args>
	static void msg(MessageTypes type, std::string_view message, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };

		std::string str = utils::format(message, args...);

		const bool error_state = type >= MessageTypes::eError;
		if (error_state)
			str.append("\n" + pretty_stacktrace());

		log.writeText(std::to_string(_console_line) + ". " + strip_ansi(str));

		auto log_str = std::format("{}. {}{}", ++_console_line, get_prefix(type), str);
		(error_state ? std::cerr : std::cout) << log_str.c_str() << std::endl;

		if (type == MessageTypes::eFatal || (type == MessageTypes::eError && s_error_fatal))
			throw(exception(str.c_str()));
	}

	[[noreturn]] static void cpp_terminate_handler()
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
		msg += Debug::pretty_stacktrace();

		Debug::winApiWindowShow("str_error", msg.c_str());
		Debug::fatalErrorMessage(msg.c_str());

		Debug::log.close();
		std::abort();
	}

public:
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

		MessageBox(nullptr, TEXT(utils::UTF8_to_CP1251(format.c_str()).c_str()), TEXT(utils::UTF8_to_CP1251(text_lang_title()).c_str()), MB_OK);
	}

	template<typename Fn, typename... Args>
	static int try_wrap(Fn&& fn, Args&&... args)
	{
		if (s_catch_exceptions)
		{
			try
			{
				fn(std::forward<Args>(args)...);
			}
			catch (...)
			{
				cpp_terminate_handler();
			}
		}
		else
			fn(std::forward<Args>(args)...);

		return 0;
	}

	template<typename... Args>
	__forceinline static std::unexpected<std::string> str_unexpected(std::string_view fmt, Args&&... args)
	{
		return std::unexpected(utils::format(fmt, args...));
	}

	/** Send ok */
	template<typename... Args>
	static void print(std::string_view message, Args&&... args)
	{
		msg(MessageTypes::ePrint, message, args...);
	}

	/** Send ok */
	template<typename... Args>
	static void ok(std::string_view message, Args&&... args)
	{
		msg(MessageTypes::eOk, message, args...);
	}

	/** Send info */
	template<typename... Args>
	static void info(std::string_view message, Args&&... args)
	{
		msg(MessageTypes::eInfo, message, args...);
	}

	/** Send warning */
	template<typename... Args>
	static void warning(std::string_view message, Args&&... args)
	{
		msg(MessageTypes::eWarning, message, args...);
	}

	/** Send please */
	template<typename... Args>
	static void please(std::string_view message, Args&&... args)
	{
		msg(MessageTypes::ePlease, message, args...);
	}

	/** Display error message and exit in certain conditions */
	template<typename... Args>
	static void error(std::string_view message, Args&&... args)
	{
		msg(MessageTypes::eError, message, args...);
	}

	/** Display error message and exit anyway */
	template<typename... Args>
	[[noreturn]] static void fatal(std::string_view message, Args&&... args)
	{
		msg(MessageTypes::eFatal, message, args...);
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
	static void _assert(bool condition, std::string_view message, Args&&... args)
	{
		if (!condition)
			fatal(message, args...);
	}

	static std::string pretty_stacktrace();
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
	Debug::_assert(                                                                            \
		!!(expr),                                                                              \
		"ASSERTION FAILED!\n\tExpression: \t{}\n\tFile: \t{}\n\tLine: \t{}\n\tFunction: \t{}", \
		#expr,                                                                                 \
		__FILE__,                                                                              \
		__LINE__,                                                                              \
		__FUNCTION__                                                                           \
	)

#define ASSERT_ARGS(expr, msg, ...)                                                                                             \
	Debug::_assert(                                                                                                             \
		!!(expr),                                                                                                               \
		std::string{ "ASSERTION FAILED!\n\tExpression: \t{}\n\tFile: \t{}\n\tLine: \t{}\n\tFunction: \t{}\n\n\t" }.append(msg), \
		#expr,                                                                                                                  \
		__FILE__,                                                                                                               \
		__LINE__,                                                                                                               \
		__FUNCTION__,                                                                                                           \
		__VA_ARGS__                                                                                                             \
	)
