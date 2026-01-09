#include "http_load_content.h"
#include "curl/curl.h"

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
	userp->append(static_cast<char*>(contents), size * nmemb);
	return size * nmemb;
}

HttpsLoad::HttpsLoad(std::string url)
{
	_curl = curl_easy_init();

	_url = url;

	curl_easy_setopt(_curl, CURLOPT_URL, _url.c_str());
	curl_easy_setopt(_curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(_curl, CURLOPT_TIMEOUT, 40L);
	curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &_stringBuffer);
}

HttpsLoad::~HttpsLoad()
{
	if (_curl)
		curl_easy_cleanup(_curl);
}

void HttpsLoad::run()
{
	if (!_curl)
		return;

	_code_result = static_cast<u32>(curl_easy_perform(_curl));
	if (_code_result != CURLcode::CURLE_OK)
	{
		Debug::warning("Couldn't get url[%s].", _url.c_str());
		return;
	}

	std::stringstream stream{ _stringBuffer };
	std::string		  line;
	while (std::getline(stream, line, '\n'))
		_line_content.push_back(line);
}

u32 HttpsLoad::codeResult() const
{
	return _code_result;
}

const std::list<std::string>& HttpsLoad::content() const
{
	return _line_content;
}
