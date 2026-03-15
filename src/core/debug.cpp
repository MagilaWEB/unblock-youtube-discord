#include "debug.h"

pcstr Debug::get_prefix(MessageTypes type)
{
#ifdef WINDOWS
	switch (type)
	{
	case MessageTypes::ePrint:
		return "";
	case MessageTypes::eOk:
		return "\x1B[32mOK: \033[0m";
	case MessageTypes::eInfo:
		return "\x1B[34mINFO: \033[0m";
	case MessageTypes::eWarning:
		return "\x1B[33m~WARNING: \033[0m";
	case MessageTypes::ePlease:
		return "\x1B[35m~PLAESE: \033[0m";
	case MessageTypes::eError:
		return "\x1B[31m!ERROR: \033[0m";
	case MessageTypes::eFatal:
		return "\x1B[31m!FATAL: \033[0m";
	default:
		error("unexpected debug message type {}", static_cast<u32>(type));
	}
#else
	switch (type)
	{
	case MessageTypes::ePrint:
		return "";
	case MessageTypes::eOk:
		return "OK: ";
	case MessageTypes::eInfo:
		return "INFO: ";
	case MessageTypes::eWarning:
		return "~WARNING: ";
	case MessageTypes::eError:
		return "!ERROR: ";
	case MessageTypes::eFatal:
		return "!FATAL: ";
	default:
		error("unexpected debug message type {}", static_cast<u32>(type));
	}
#endif
	return "";
}

void Debug::initialize(const std::string& /*command_line*/)
{
	s_catch_exceptions = true;
	s_error_fatal	   = true;
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

	log.open(dir_logs / "log", ".txt", true);
	if (log.isOpen())
	{
		log_backup.open(dir_logs / "log_backup", ".txt", true);
		if (log_backup.isOpen())
			log_backup.clear();

		log.forLine(
			[](std::string str)
			{
				log_backup.writeText(str);
				return false;
			}
		);

		log.clear();
	}
}

void Debug::fatalErrorMessage(std::string message)
{
	log.writeText(std::to_string(++_console_line) + ". " + message);
	log.close();
	std::cerr << message << std::endl;
}

std::string Debug::pretty_stacktrace()
{
	auto		trace  = std::stacktrace::current(1);	 // пропускаем текущую функцию
	std::string result = "🚨 Stacktrace (depth: " + std::to_string(trace.size()) + "):\n";

	int frame_num = 0;
	for (const auto& frame : trace)
	{
		std::string func = frame.description();
		if (func.empty())
			func = "???";

		std::string file = frame.source_file();
		int			line = frame.source_line();

		std::string location;
		if (!file.empty())
		{
			std::filesystem::path p(file);
			location = std::format("{}:{}", p.filename().string(), line);
		}
		else
			location = std::format("{:016x}", reinterpret_cast<uintptr_t>(frame.native_handle()));

		const int func_width = 86;
		if (func.length() > func_width)
			func = func.substr(0, func_width - 3) + "...";

		result += std::format("  #{:2} -> {:<{}} ({})\n", frame_num++, func, func_width, location);
	}
	return result;
}
