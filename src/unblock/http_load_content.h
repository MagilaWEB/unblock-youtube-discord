#pragma once

#include <atomic>

typedef void CURL;

class HttpsLoad
{
	CURL*			   _curl{ nullptr };
	std::string		   _url{};
	std::string		   _effective_url{};
	std::string		   _string_buffer;
	u32				   _code_result{ 0 };
	std::atomic<float> _progress{ 0.F };

public:
	HttpsLoad() = delete;
	HttpsLoad(std::string_view);
	~HttpsLoad();
	u32 codeResult() const;

	/** URL the request ended on, after all redirects. Empty until run(). */
	std::string				 effectiveUrl() const;
	std::vector<std::string> run();
	bool					 runToFile(std::filesystem::path);
	float					 progress() const;
};
