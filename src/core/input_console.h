#pragma once
#include "virtual_key_codes.hpp"

class CORE_API InputConsole final
{
	inline static CriticalSection _lock;
	enum class ColorType : u16
	{
		BLACK,
		DARK_BLUE,
		DARK_GREEN,
		LIGHT_BLUE,
		DARK_RED,
		MAGENTA,
		ORANGE,
		LIGHT_GRAY,
		GRAY,
		BLUE,
		GREEN,
		CYAN,
		RED,
		PINK,
		YELLOW,
		WHITE
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
		CriticalSection::raii mt{ _lock };
		msg(text, "{}", ColorType::CYAN, true, args...);
		std::cout << std::endl;
	}

	template<typename... Args>
	static void textOk(std::string_view text, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		msg(text, "Успех: {}", ColorType::GREEN, true, args...);
		std::cout << std::endl;
	}

	template<typename... Args>
	static void textInfo(std::string_view text, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		msg(text, "Информация: {}", ColorType::YELLOW, true, args...);
	}

	template<typename... Args>
	static void textAsk(std::string_view text, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		std::cout << std::endl;
		msg(text, "{}???", ColorType::DARK_GREEN, true, args...);
	}

	template<typename... Args>
	static void textWarning(std::string_view text, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		msg(text, "Предупреждение: {}", ColorType::ORANGE, true, args...);
	}

	template<typename... Args>
	static void textError(std::string_view text, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		msg(text, "Ошибка: {}", ColorType::RED, true, args...);
	}

	template<typename... Args>
	static void textPlease(std::string_view text, bool reset_color, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		msg(text, "Пожалуйста, {}:", ColorType::CYAN, reset_color, args...);
	}

	static std::string textColor(std::string_view text, ColorType type, bool reset_color = true);

	static void clear();

private:
	template<typename... Args>
	static void msg(std::string_view text, std::string_view prefix, ColorType type, bool reset_color, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		std::string			  mod_text = utils::format(prefix, utils::format(text, args...));
		mod_text = textColor(mod_text.c_str(), type, reset_color);
		Debug::print("{}", mod_text);
	}

	static bool _forbiddenCharacters(const std::string& text);
};
