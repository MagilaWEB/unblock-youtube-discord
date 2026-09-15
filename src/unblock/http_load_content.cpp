#include "http_load_content.h"
#include "curl/curl.h"

HttpsLoad::HttpsLoad(std::string_view url)
{
	_curl = curl_easy_init();

	_url = url;

	curl_easy_setopt(_curl, CURLOPT_URL, _url.c_str());
	curl_easy_setopt(_curl, CURLOPT_FOLLOWLOCATION, 1L);
}

HttpsLoad::~HttpsLoad()
{
	if (_curl)
		curl_easy_cleanup(_curl);
}

static int ProgressCallback(float* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
	if (dltotal > 0)
		*clientp = static_cast<float>(dlnow) / static_cast<float>(dltotal) * 100.F;

	return CURLE_OK;
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

	// Reset per-request state: a reused handle must not report stale data.
	_stringBuffer.clear();
	_code_result = 0;
	_progress	 = 0.f;

	curl_easy_setopt(_curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(_curl, CURLOPT_TIMEOUT, 20L);
	curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &_stringBuffer);
	curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(_curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
	curl_easy_setopt(_curl, CURLOPT_XFERINFODATA, &_progress);

	if (curl_easy_perform(_curl) != CURLcode::CURLE_OK)
	{
		Debug::warning("Couldn't get url[{}].", _url);
		return {};
	}

	curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &_code_result);

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
	file->write(static_cast<char*>(ptr), static_cast<std::streamsize>(written));
	return written;
}

bool HttpsLoad::run_to_file(std::filesystem::path path)
{
	if (!_curl)
		return false;

	// Reset per-request state so a reused handle (static update loader)
	// never reports the previous download's code/progress.
	_code_result = 0;
	_progress	 = 0.f;

	std::error_code dir_ec;
	std::filesystem::create_directories(path.parent_path(), dir_ec);

	std::fstream file;
	file.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!file.is_open())
	{
		Debug::warning("Couldn't open file[{}] for download.", path.string());
		return false;
	}

	curl_easy_setopt(_curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(_curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(_curl, CURLOPT_CONNECTTIMEOUT, 30L);
	curl_easy_setopt(_curl, CURLOPT_TIMEOUT, 300L);
	curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, write_file);
	curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &file);
	curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(_curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
	curl_easy_setopt(_curl, CURLOPT_XFERINFODATA, &_progress);

	if (curl_easy_perform(_curl) != CURLcode::CURLE_OK)
	{
		Debug::warning("Couldn't get url[{}].", _url);
		file.close();
		std::error_code remove_ec;
		std::filesystem::remove(path, remove_ec);
		return false;
	}

	curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &_code_result);

	file.close();

	if (_code_result != 200)
	{
		std::error_code remove_ec;
		std::filesystem::remove(path, remove_ec);
		return false;
	}

	std::error_code size_ec;
	if (!std::filesystem::exists(path, size_ec) || std::filesystem::file_size(path, size_ec) == 0)
	{
		Debug::warning("Downloaded file[{}] is empty.", path.string());
		return false;
	}

	return true;
}

u32 HttpsLoad::codeResult() const
{
	return _code_result;
}

float HttpsLoad::progress() const
{
	return _progress;
}
