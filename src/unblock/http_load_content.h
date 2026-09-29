#pragma once

#include <atomic>

typedef void CURL;

class HttpsLoad
{
	CURL*			   _curl{ nullptr };
	std::string		   _url{};
	std::string		   _string_buffer;
	u32				   _code_result{ 0 };
	std::atomic<float> _progress{ 0.F };

public:
	HttpsLoad() = delete;
	HttpsLoad(std::string_view);
	~HttpsLoad();
	u32						 codeResult() const;
	std::vector<std::string> run();
	bool					 runToFile(std::filesystem::path);
	float					 progress() const;
};
