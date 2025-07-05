#pragma once

#include <iostream>

class CORE_API Debug
{
	enum MessageTypes
	{
		eOk,
		eInfo,
		eWarning,
		ePlease,
		eError,
		eFatal
	};

public:
	using exception = std::runtime_error;

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
		static CriticalSection lock;
		CriticalSection::raii  mt{ lock };

		auto str	 = utils::format(message, std::forward<Args>(args)...);
		auto log_str = utils::format("%s%s", get_prefix(type), str.c_str());
		((type >= MessageTypes::eError) ? std::cerr : std::cout) << log_str.c_str() << std::endl;
		if (type == MessageTypes::eFatal || (type == MessageTypes::eError && s_error_fatal))
			throw(exception(str.c_str()));
	}

public:
	static void initialize(const std::string& command_line);
	static void fatalErrorMessage(pcstr message);

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
				fatalErrorMessage(utils::format("Exception caught!\n%s", E.what()).c_str());
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

#define VERIFY(expr)                                                                                                                           \
	Debug::verify(!!(expr), "VERIFICATION FAILED!\nExpression: %s\nFile: %s\nLine: %d\nFunction: %s", #expr, __FILE__, __LINE__, __FUNCTION__)

#define ASSERT(expr)                                                                                                                         \
	Debug::_assert(!!(expr), "ASSERTION FAILED!\nExpression: %s\nFile: %s\nLine: %d\nFunction: %s", #expr, __FILE__, __LINE__, __FUNCTION__)
