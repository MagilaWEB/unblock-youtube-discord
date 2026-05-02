#include "input_console.h"

using namespace std::chrono;

void InputConsole::pause(std::string_view info)
{
	textPlease(Localization::Str("str_console_input_pause")(), true, info);

	KEY(VK::ENTER, true);

	clear();
}

std::string InputConsole::getString()
{
	std::string text_input{};

	textPlease(Localization::Str("str_console_input_pause")(), true);
	std::cin.clear();

	std::cin >> text_input;

	return text_input;
}

u32 InputConsole::getU32()
{
	std::string text_input{};
	u32			count{ 0 };

	textPlease(Localization::Str("str_console_input_get_u32")(), true);
	std::cin.clear();
	std::cin >> text_input;

	if (!_forbiddenCharacters(text_input))
		return getU32();

	try
	{
		count = stoul(text_input);
	}
	catch (std::invalid_argument)
	{
		textError(Localization::Str("str_console_input_get_u32_invalid_argument")(), true);
		std::cin.clear();
		return getU32();
	}
	catch (std::out_of_range)
	{
		textError(Localization::Str("str_console_input_get_u32_out_of_range")(), true);
		std::cin.clear();
		return getU32();
	}
	catch (...)
	{
		textError(Localization::Str("str_console_input_get_u32_unknown")(), true);
		std::cin.clear();
		return getU32();
	}

	clear();

	return count;
}

u32 InputConsole::sendNum(std::list<u8> nums)
{
	constexpr std::pair<VK, u32> key_nums[]{
		{	  VK::NUM0, 0 },
		{	  VK::NUM1, 1 },
		{	  VK::NUM2, 2 },
		{	  VK::NUM3, 3 },
		{	  VK::NUM4, 4 },
		{	  VK::NUM5, 5 },
		{	  VK::NUM6, 6 },
		{	  VK::NUM7, 7 },
		{	  VK::NUM8, 8 },
		{	  VK::NUM9, 9 },
		{ VK::NUMPAD0, 0 },
		{ VK::NUMPAD1, 1 },
		{ VK::NUMPAD2, 2 },
		{ VK::NUMPAD3, 3 },
		{ VK::NUMPAD4, 4 },
		{ VK::NUMPAD5, 5 },
		{ VK::NUMPAD6, 6 },
		{ VK::NUMPAD7, 7 },
		{ VK::NUMPAD8, 8 },
		{ VK::NUMPAD9, 9 },
	};

	std::string str_nums{};

	u32 it{ 0 };
	for (auto& send_num : nums)
	{
		it++;

		ASSERT(send_num <= 9);

		if (it == nums.size())
			str_nums.append(std::to_string(send_num));
		else
			str_nums.append(std::to_string(send_num).append(", "));
	}

	textPlease(Localization::Str("str_console_input_send_num")(), true, str_nums.c_str());

	while (true)
	{
		for (auto& [key, num] : key_nums)
		{
			for (auto& send_num : nums)
			{
				if (send_num == num)
				{
					if (KEY<bool>(key, false))
					{
						clear();
						return num;
					}
				}
			}
		}

		std::this_thread::yield();
	}
}

bool InputConsole::getBool()
{
	textPlease(Localization::Str("str_console_input_get_bool")(), true);

	while (true)
	{
		if (KEY<bool>(VK::ENTER, false))
		{
			clear();
			return true;
		}

		if (KEY<bool>(VK::ESCAPE, false))
			break;

		std::this_thread::yield();
	}

	clear();
	return false;
}

size_t InputConsole::selectFromList(const std::list<std::string>& list, std::function<void(size_t)>&& callback)
{
	const auto size = list.size();
	size_t	   select{ 0 };
	bool	   print{ true };

	callback(select);

	while (true)
	{
		if (KEY<bool>(VK::UP, false) || KEY<bool>(VK::NUMPAD8, false))
		{
			if (select == 0)
				select = size - 1;
			else
				select--;

			clear();

			callback(select);
			print = true;
		}

		if (KEY<bool>(VK::DOWN, false) || KEY<bool>(VK::NUMPAD2, false))
		{
			select++;

			if (select == size)
				select = 0;

			clear();

			callback(select);
			print = true;
		}

		if (print)
		{
			print = false;

			textInfo(Localization::Str("str_console_input_select_from_list")());

			std::string str_all{};

			u32 it{ 0 };
			for (const std::string& element : list)
			{
				std::string str{};
				if (select == it)
					str = ">> ";
				else
					str = "   ";

				str_all.append(std::format("{}{}: {}\n", str, it, element));

				it++;
			}

			textPlease(Localization::Str("str_console_input_please_select")(), true);

			text(str_all);
		}

		if (KEY<bool>(VK::ENTER, false))
			break;

		std::this_thread::yield();
	}

	clear();

	return select;
}

std::string InputConsole::textColor(std::string_view text, ColorType type, bool reset_color)
{
	auto color = [&type]
	{
		switch (type)
		{
		case ColorType::BLACK:
			return "30";
		case ColorType::DARK_BLUE:
			return "34";
		case ColorType::DARK_GREEN:
			return "32";
		case ColorType::LIGHT_BLUE:
			return "36";
		case ColorType::DARK_RED:
			return "31";
		case ColorType::MAGENTA:
			return "35";	// color_magenta    5
		case ColorType::ORANGE:
			return "33";	// color_orange     6
		case ColorType::LIGHT_GRAY:
			return "37";	// color_light_gray 7
		case ColorType::GRAY:
			return "90";	// color_gray       8
		case ColorType::BLUE:
			return "94";	// color_blue       9
		case ColorType::GREEN:
			return "92";	// color_green     10
		case ColorType::CYAN:
			return "96";	// color_cyan      11
		case ColorType::RED:
			return "91";	// color_red       12
		case ColorType::PINK:
			return "95";	// color_pink      13
		case ColorType::YELLOW:
			return "93";	// color_yellow    14
		case ColorType::WHITE:
			return "97";	// color_white     15
		default:
			return "37";
		}
	};

	std::string mod_text = std::format("{}{}m{}", "\033[", color(), text);

	if (reset_color)
		mod_text.append("\033[0m");

	return mod_text;
}

void InputConsole::clear()
{
	CriticalSection::raii mt{ _lock };
#if defined _WIN32
	system("cls");
#elif defined(__LINUX__) || defined(__gnu_linux__) || defined(__linux__)
	system("clear");
#endif
}

bool InputConsole::_forbiddenCharacters(const std::string& text)
{
	constexpr static pcstr forbidden_characters[]{ "-", ".", "," };

	for (auto& _char : forbidden_characters)
	{
		if (text.contains(_char))
		{
			textError(Localization::Str("str_console_input_forbidden_characters")(), true);
			std::cin.clear();
			return false;
		}
	}

	return true;
}
