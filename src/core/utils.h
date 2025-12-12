#pragma once
namespace utils
{
	struct DefaultInit
	{
		DefaultInit()							   = default;
		DefaultInit(const DefaultInit&)			   = default;
		DefaultInit(DefaultInit&&)				   = default;
		DefaultInit& operator=(const DefaultInit&) = default;
		DefaultInit& operator=(DefaultInit&&)	   = default;
		virtual ~DefaultInit()					   = default;
	};

	template<typename... Args>
	__forceinline std::string format(pcstr fmt, Args&&... args)
	{
		int size_s = snprintf(nullptr, 0, fmt, std::forward<Args>(args)...) + 1;
		if (size_s <= 0)
			throw std::runtime_error("Error during formatting");

		auto size = static_cast<size_t>(size_s);
		auto buf  = std::make_unique<char[]>(size);
		snprintf(buf.get(), size, fmt, args...);
		return { buf.get(), buf.get() + size - 1 };
	}

	CORE_API bool IsUTF8(pcstr string);
	CORE_API std::string UTF8_to_CP1251(pcstr str);

	CORE_API void ltrim(std::string& str);
	CORE_API void rtrim(std::string& str);
	CORE_API void trim(std::string& str);
}
