using s8 = std::int8_t;
using u8 = std::uint8_t;

using s16 = std::int16_t;
using u16 = std::uint16_t;

using s32 = std::int32_t;
using u32 = std::uint32_t;

using s64 = std::int64_t;
using u64 = std::uint64_t;

using pstr	 = char*;
using pcstr	 = const char*;
using cpcstr = const char* const;

template<typename T>
constexpr auto type_max = std::numeric_limits<T>::max();

template<typename T>
constexpr auto type_min = -std::numeric_limits<T>::max();

template<typename T>
constexpr auto type_zero = std::numeric_limits<T>::min();

template<typename T>
constexpr auto type_epsilon = std::numeric_limits<T>::epsilon();
