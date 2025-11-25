#pragma once

#include <iostream>
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

	[[nodiscard]] static pcstr get_prefix(MessageTypes type);

	template<typename... Args>
	static void msg(MessageTypes type, pcstr message, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };

		auto str	 = utils::format(message, std::forward<Args>(args)...);
		auto log_str = utils::format("%d. %s%s", ++_console_line, get_prefix(type), str.c_str());

		log.writeText(std::to_string(_console_line) + ". " + str);

		((type >= MessageTypes::eError) ? std::cerr : std::cout) << log_str.c_str() << std::endl;

		if (type == MessageTypes::eFatal || (type == MessageTypes::eError && s_error_fatal))
		{
			log.close();
			throw(exception(str.c_str()));
		}
	}

public:
	static void initialize(const std::string& command_line);
	static void initLogFile();
	static void fatalErrorMessage(std::string message);

	template<typename Fn, typename... Args>
	static int try_wrap(Fn&& fn, Args&&... args)
	{
		if (s_catch_exceptions)
		{
			try
			{
				fn(std::forward<Args>(args)...);
			}
			catch (std::exception& E)
			{
				fatalErrorMessage(utils::format("Exception caught!\n%s", E.what()));
				return -1;
			}
			catch (...)
			{
				fatalErrorMessage("Exception caught!\nUnknown exception...");
				return -1;
			}
		}
		else
			fn(std::forward<Args>(args)...);

		return 0;
	}

	template<typename... Args>
	__forceinline static std::unexpected<std::string> str_unexpected(pcstr fmt, Args&&... args)
	{
		return std::unexpected(utils::format(fmt, std::forward<Args>(args)...));
	}

	/** Send ok */
	template<typename... Args>
	static void print(pcstr message, Args&&... args)
	{
		msg(MessageTypes::ePrint, message, std::forward<Args>(args)...);
	}

	/** Send ok */
	template<typename... Args>
	static void ok(pcstr message, Args&&... args)
	{
		msg(MessageTypes::eOk, message, std::forward<Args>(args)...);
	}

	/** Send info */
	template<typename... Args>
	static void info(pcstr message, Args&&... args)
	{
		msg(MessageTypes::eInfo, message, std::forward<Args>(args)...);
	}

	/** Send warning */
	template<typename... Args>
	static void warning(pcstr message, Args&&... args)
	{
		msg(MessageTypes::eWarning, message, std::forward<Args>(args)...);
	}

	/** Send please */
	template<typename... Args>
	static void please(pcstr message, Args&&... args)
	{
		msg(MessageTypes::ePlease, message, std::forward<Args>(args)...);
	}

	/** Display error message and exit in certain conditions */
	template<typename... Args>
	static void error(pcstr message, Args&&... args)
	{
		msg(MessageTypes::eError, message, std::forward<Args>(args)...);
	}

	/** Display error message and exit anyway */
	template<typename... Args>
	[[noreturn]] static void fatal(pcstr message, Args&&... args)
	{
		msg(MessageTypes::eFatal, message, std::forward<Args>(args)...);
	}

	/** Check condition and throw warning if it fails */
	template<typename... Args>
	static void check(bool condition, pcstr message, Args&&... args)
	{
		if (!condition)
			warning(message, std::forward<Args>(args)...);
	}

	/** Check condition and throw error if it fails */
	template<typename... Args>
	static void verify(bool condition, pcstr message, Args&&... args)
	{
		if (!condition)
			error(message, std::forward<Args>(args)...);
	}

	/** Check condition and fatal if it fails */
	template<typename... Args>
	static void _assert(bool condition, pcstr message, Args&&... args)
	{
		if (!condition)
			fatal(message, std::forward<Args>(args)...);
	}
};

#define VERIFY(expr)                                                                            \
	Debug::verify(                                                                              \
		!!(expr),                                                                               \
		"VERIFICATION FAILED!\n\tExpression: \t%s\n\tFile: \t%s\n\tLine: %d\n\tFunction: \t%s", \
		#expr,                                                                                  \
		__FILE__,                                                                               \
		__LINE__,                                                                               \
		__FUNCTION__                                                                            \
	)

#define ASSERT(expr)                                                                           \
	Debug::_assert(                                                                            \
		!!(expr),                                                                              \
		"ASSERTION FAILED!\n\tExpression: \t%s\n\tFile: \t%s\n\tLine: \t%d\n\tFunction: \t%s", \
		#expr,                                                                                 \
		__FILE__,                                                                              \
		__LINE__,                                                                              \
		__FUNCTION__                                                                           \
	)

#define ASSERT_ARGS(expr, msg, ...)                                                                                                     \
	Debug::_assert(                                                                                                                     \
		!!(expr),                                                                                                                       \
		std::string{ "ASSERTION FAILED!\n\tExpression: \t%s\n\tFile: \t%s\n\tLine: \t%d\n\tFunction: \t%s\n\n\t" }.append(msg).c_str(), \
		#expr,                                                                                                                          \
		__FILE__,                                                                                                                       \
		__LINE__,                                                                                                                       \
		__FUNCTION__,                                                                                                                   \
		__VA_ARGS__                                                                                                                     \
	)
