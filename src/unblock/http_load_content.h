#pragma once
typedef void CURL;

class HttpsLoad
{
	CURL*		_curl{ nullptr };
	std::string _url{};
	u32			_code_result{ 0 };
	std::string _stringBuffer;

public:
	HttpsLoad() = delete;
	HttpsLoad(std::string);
	~HttpsLoad();
	u32						 codeResult() const;
	std::vector<std::string> run();
	void					 run_to_file(std::filesystem::path);
};
