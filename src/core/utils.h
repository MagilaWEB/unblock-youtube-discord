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
	inline std::string format(std::string_view fmt, Args&&... args)
	{
		return std::vformat(fmt, std::make_format_args(args...));
	}

	bool		 isUtf8(std::string_view string);
	std::string	 utf8ToCp1251(std::string_view utf8_str);
	std::wstring utf8ToUtf16(std::string_view utf8_str);

	void ltrim(std::string& str);
	void rtrim(std::string& str);
	void trim(std::string& str);

	/** Returns true when the string is a valid host name (domain). */
	bool isValidHostName(std::string_view str);

	/** Returns true when the string is a valid host name (domain), optionally with a port. */
	bool isValidHostNamePort(std::string_view host);

	/** Returns true when the string is a valid IP address or subnet (IPv4/IPv6, optional /N prefix). */
	bool isValidNetwork(std::string_view network);

	/** Copies UTF-8 text to the Windows clipboard. Returns false on failure. */
	bool copyToClipboard(std::string_view utf8_str);
}
