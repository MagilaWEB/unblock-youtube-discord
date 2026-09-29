#pragma once

namespace concepts
{
	template<typename T>
	concept ValidIntegerUnsigned = std::same_as<T, u32> || std::same_as<T, u64> || std::same_as<T, u16> || std::same_as<T, u8>;

	template<typename T>
	concept ValidIntegerLong = std::same_as<T, s64>;
	template<typename T>
	concept ValidInteger = ValidIntegerLong<T> || std::same_as<T, s32> || std::same_as<T, s16> || std::same_as<T, s8>;

	template<typename T>
	concept ValidNumber = std::same_as<T, float> || std::same_as<T, double>;

	template<typename T>
	concept ValidStringPtr = std::same_as<T, pcstr> || std::same_as<T, cpcstr>;
	template<typename T>
	concept ValidString = ValidStringPtr<T> || std::same_as<T, std::string>;

	template<typename T>
	concept ValidAll = ValidIntegerUnsigned<T> || ValidInteger<T> || ValidString<T> || ValidNumber<T> || std::same_as<T, bool>;
}
