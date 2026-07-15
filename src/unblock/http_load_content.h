#pragma once
typedef void CURL;

class HttpsLoad
{
	CURL*		_curl{ nullptr };
	std::string _url{};
	std::string _stringBuffer;
	u32			_code_result{ 0 };
	float		_progress{ 0.f };

public:
	HttpsLoad() = delete;
	HttpsLoad(std::string_view);
	~HttpsLoad();
	u32						 codeResult() const;
	std::vector<std::string> run();
	void					 run_to_file(std::filesystem::path);
	float					 progress() const;
};
