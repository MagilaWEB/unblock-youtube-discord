#include "http_load_content.h"
#include "curl/curl.h"

HttpsLoad::HttpsLoad(std::string url)
{
	_curl = curl_easy_init();

	_url = url;

	curl_easy_setopt(_curl, CURLOPT_URL, _url.c_str());
	curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &_code_result);
}

HttpsLoad::~HttpsLoad()
{
	if (_curl)
		curl_easy_cleanup(_curl);
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
	userp->append(static_cast<char*>(contents), size * nmemb);
	return size * nmemb;
}

std::vector<std::string> HttpsLoad::run()
{
	if (!_curl)
		return {};

	curl_easy_setopt(_curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(_curl, CURLOPT_TIMEOUT, 40L);
	curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &_stringBuffer);

	if (curl_easy_perform(_curl) != CURLcode::CURLE_OK)
	{
		Debug::warning("Couldn't get url[%s].", _url.c_str());
		return {};
	}

	std::vector<std::string> _line_content{};

	std::stringstream stream{ _stringBuffer };
	std::string		  line;
	while (std::getline(stream, line, '\n'))
		_line_content.push_back(line);

	return _line_content;
}

static size_t write_file(void* ptr, size_t size, size_t nmemb, void* stream)
{
	std::fstream* file	  = static_cast<std::fstream*>(stream);
	size_t		  written = size * nmemb;
	file->write(static_cast<char*>(ptr), written);
	return written;
}

void HttpsLoad::run_to_file(std::filesystem::path path)
{
	if (!_curl)
		return;

	auto test = path.parent_path();

	if (!std::filesystem::is_directory(test))
		std::filesystem::create_directory(test);

	std::fstream file;
	file.open(path, std::ios::out | std::ios::binary);

	curl_easy_setopt(_curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, write_file);
	curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &file);

	if (curl_easy_perform(_curl) != CURLcode::CURLE_OK)
	{
		Debug::warning("Couldn't get url[%s].", _url.c_str());
		return;
	}

	file.close();
}

u32 HttpsLoad::codeResult() const
{
	return _code_result;
}
