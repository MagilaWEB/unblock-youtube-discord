#pragma once

#include <atomic>

typedef void CURL;

class HttpsLoad
{
	CURL*			   _curl{ nullptr };
	std::string		   _url{};
	std::string		   _stringBuffer;
	u32				   _code_result{ 0 };
	std::atomic<float> _progress{ 0.F };

public:
	HttpsLoad() = delete;
	HttpsLoad(std::string_view);
	~HttpsLoad();
	u32						 codeResult() const;
	std::vector<std::string> run();
	bool					 run_to_file(std::filesystem::path);
	float					 progress() const;
};
