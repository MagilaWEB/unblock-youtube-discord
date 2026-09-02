#pragma once

namespace concepts
{
	template<typename T>
	concept VallidIntegerUsignet = std::same_as<T, u32> || std::same_as<T, u64> || std::same_as<T, u16> || std::same_as<T, u8>;

	template<typename T>
	concept VallidIntegerLong = std::same_as<T, s64>;
	template<typename T>
	concept VallidInteger = VallidIntegerLong<T> || std::same_as<T, s32> || std::same_as<T, s16> || std::same_as<T, s8>;

	template<typename T>
	concept VallidNumber = std::same_as<T, float> || std::same_as<T, double>;

	template<typename T>
	concept VallidStringPctr = std::same_as<T, pcstr> || std::same_as<T, cpcstr>;
	template<typename T>
	concept VallidString = VallidStringPctr<T> || std::same_as<T, std::string>;

	template<typename T>
	concept VallidALL = VallidIntegerUsignet<T> || VallidInteger<T> || VallidString<T> || VallidNumber<T> || std::same_as<T, bool>;
}
