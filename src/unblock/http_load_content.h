#pragma once
typedef void	  CURL;

class HttpsLoad
{
	CURL*				   _curl{ nullptr };
	std::string			   _url{};
	u32					   _code_result{ 0 };
	std::string			   _stringBuffer;
	std::list<std::string> _line_content{};

public:
	HttpsLoad() = delete;
	HttpsLoad(std::string);
	~HttpsLoad();
	u32							  codeResult() const;
	void						  run();
	const std::list<std::string>& content() const;
};
