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

	bool IsUTF8(std::string_view string);
	std::string UTF8_to_CP1251(std::string_view utf8_str);
	std::wstring UTF8_to_UTF16(std::string_view utf8_str);

	void ltrim(std::string& str);
	void rtrim(std::string& str);
	void trim(std::string& str);

	/** Проверяет, что строка — валидное имя хоста (домен), опционально с портом. */
	bool isValidHost(std::string_view host);

	/** Проверяет, что строка — валидный IP-адрес или подсеть (IPv4/IPv6, опционально с префиксом /N). */
	bool isValidNetwork(std::string_view network);
}
