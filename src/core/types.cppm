export module core.types;

import std;

export using s8 = std::int8_t;
export using u8 = std::uint8_t;

export using s16 = std::int16_t;
export using u16 = std::uint16_t;

export using s32 = std::int32_t;
export using u32 = std::uint32_t;

export using s64 = std::int64_t;
export using u64 = std::uint64_t;

export using pstr	= char*;
export using pcstr	= const char*;
export using cpcstr = const char* const;

export template<typename T>
constexpr auto type_max = std::numeric_limits<T>::max();

export template<typename T>
constexpr auto type_min = std::numeric_limits<T>::lowest();

export template<typename T>
constexpr auto type_zero = std::numeric_limits<T>::min();

export template<typename T>
constexpr auto type_epsilon = std::numeric_limits<T>::epsilon();
