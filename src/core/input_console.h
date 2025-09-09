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

	static void pause(pcstr info = "");
	static u32	getU32();
	static u8	sendNum(std::list<u8> nums);
	static bool getBool();

	static u32 selectFromList(const std::list<std::string>& list, std::function<void(u32 select)>&& callback = [](u32) {});

	template<typename... Args>
	static void text(pcstr text, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		msg(text, "%s", ColorType::CYAN, true, std::forward<Args>(args)...);
		std::cout << std::endl;
	}

	template<typename... Args>
	static void textOk(pcstr text, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		msg(text, "Успех: %s", ColorType::GREEN, true, std::forward<Args>(args)...);
		std::cout << std::endl;
	}

	template<typename... Args>
	static void textInfo(pcstr text, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		msg(text, "Информация: %s", ColorType::YELLOW, true, std::forward<Args>(args)...);
		std::cout << std::endl;
	}

	template<typename... Args>
	static void textAsk(pcstr text, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		std::cout << std::endl;
		msg(text, "%s???", ColorType::DARK_GREEN, true, std::forward<Args>(args)...);
		std::cout << std::endl;
	}

	template<typename... Args>
	static void textWarning(pcstr text, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		msg(text, "Предупреждение: %s", ColorType::ORANGE, true, std::forward<Args>(args)...);
		std::cout << std::endl;
	}

	template<typename... Args>
	static void textError(pcstr text, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		msg(text, "Ошибка: %s", ColorType::RED, true, std::forward<Args>(args)...);
		std::cout << std::endl;
	}

	template<typename... Args>
	static void textPlease(pcstr text, bool reset_color, bool end, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		msg(text, "Пожалуйста, %s:", ColorType::CYAN, reset_color, std::forward<Args>(args)...);
		if (end)
			std::cout << std::endl;
	}

	static std::string textColor(pcstr text, ColorType type, bool reset_color = true);

	static void clear();

private:
	template<typename... Args>
	static void msg(pcstr text, pcstr prefix, ColorType type, bool reset_color, Args&&... args)
	{
		CriticalSection::raii mt{ _lock };
		std::string			  mod_text = utils::format(prefix, utils::format(text, std::forward<Args>(args)...).c_str());
		mod_text					   = textColor(mod_text.c_str(), type, reset_color);
		std::cout << mod_text.c_str();
	}

	static bool _forbiddenCharacters(const std::string& text, pcstr info);
};
