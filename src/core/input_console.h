#pragma once
#include "virtual_key_codes.hpp"

class InputConsole final
{
	inline static CriticalSection _lock;
	enum class ColorType : u16
	{
		Black,
		DarkBlue,
		DarkGreen,
		LightBlue,
		DarkRed,
		Magenta,
		Orange,
		LightGray,
		Gray,
		Blue,
		Green,
		Cyan,
		Red,
		Pink,
		Yellow,
		White
	};

public:
	InputConsole()	= default;
	~InputConsole() = default;

	static void		   pause(std::string_view info = "");
	static std::string getString();
	static u32		   getU32();
	static u32		   sendNum(std::list<u8> nums);
	static bool		   getBool();

	static size_t selectFromList(const std::list<std::string>& list, std::function<void(size_t select)>&& callback = [](u32) {});

	template<typename... Args>
	static void text(std::string_view text, Args&&... args)
	{
		CriticalSection::Raii mt{ _lock };
		msg(text, "{}", ColorType::Cyan, true, args...);
		std::cout << std::endl;
	}

	template<typename... Args>
	static void textOk(std::string_view text, Args&&... args)
	{
		CriticalSection::Raii mt{ _lock };
		msg(text, Localization::Str("str_console_input_success")(), ColorType::Green, true, args...);
		std::cout << std::endl;
	}

	template<typename... Args>
	static void textInfo(std::string_view text, Args&&... args)
	{
		CriticalSection::Raii mt{ _lock };
		msg(text, Localization::Str("str_console_input_info")(), ColorType::Yellow, true, args...);
	}

	template<typename... Args>
	static void textAsk(std::string_view text, Args&&... args)
	{
		CriticalSection::Raii mt{ _lock };
		std::cout << std::endl;
		msg(text, "{}???", ColorType::DarkGreen, true, args...);
	}

	template<typename... Args>
	static void textWarning(std::string_view text, Args&&... args)
	{
		CriticalSection::Raii mt{ _lock };
		msg(text, Localization::Str("str_console_input_warning")(), ColorType::Orange, true, args...);
	}

	template<typename... Args>
	static void textError(std::string_view text, Args&&... args)
	{
		CriticalSection::Raii mt{ _lock };
		msg(text, Localization::Str("str_console_input_error")(), ColorType::Red, true, args...);
	}

	template<typename... Args>
	static void textPlease(std::string_view text, bool reset_color, Args&&... args)
	{
		CriticalSection::Raii mt{ _lock };
		msg(text, Localization::Str("str_console_input_please")(), ColorType::Cyan, reset_color, args...);
	}

	static std::string textColor(std::string_view text, ColorType type, bool reset_color = true);

	static void clear();

private:
	template<typename... Args>
	static void msg(std::string_view text, std::string_view prefix, ColorType type, bool reset_color, Args&&... args)
	{
		CriticalSection::Raii mt{ _lock };
		std::string			  mod_text = utils::format(prefix, utils::format(text, args...));
		mod_text					   = textColor(mod_text.c_str(), type, reset_color);
		Debug::print("{}", mod_text);
	}

	static bool _forbiddenCharacters(const std::string& text);
};
