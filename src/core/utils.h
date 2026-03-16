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
	__forceinline std::string format(std::string_view fmt, Args&&... args)
	{
		return std::vformat(fmt, std::make_format_args(args...));
	}

	CORE_API bool IsUTF8(std::string_view string);
	CORE_API std::string UTF8_to_CP1251(std::string_view str);

	CORE_API void ltrim(std::string& str);
	CORE_API void rtrim(std::string& str);
	CORE_API void trim(std::string& str);
}
