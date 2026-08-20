#include "curl_client.h"

#include <format>
#include <memory>

namespace
{
	inline const std::string c_user_agent{
		"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36"
	};
	inline constexpr u32 c_check_timeout_sec{ 6 };
	inline constexpr u32 c_connect_timeout_sec{ 5 };
	inline constexpr u32 c_max_redirects{ 5 };
}

void CurlCleanup::operator()(CURL* curl) const
{
	if (curl)
		curl_easy_cleanup(curl);
}

void SlistCleanup::operator()(curl_slist* list) const
{
	if (list)
		curl_slist_free_all(list);
}

CurlGlobal::CurlGlobal()
{
	curl_global_init(CURL_GLOBAL_ALL);
}

CurlGlobal::~CurlGlobal()
{
	curl_global_cleanup();
}

size_t CurlClient::_writeCallback(char*, size_t size, size_t count, void*)
{
	return size * count;
}

curl_slist* CurlClient::_buildHeaders()
{
	curl_slist* list = nullptr;
	list			 = curl_slist_append(list, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
	list			 = curl_slist_append(list, "Accept-Language: ru-RU,ru;q=0.9,en-US;q=0.8,en;q=0.7");
	list			 = curl_slist_append(list, "Cache-Control: no-cache");
	list			 = curl_slist_append(list, "Sec-Fetch-Dest: document");
	list			 = curl_slist_append(list, "Sec-Fetch-Mode: navigate");
	list			 = curl_slist_append(list, "Sec-Fetch-Site: none");
	list			 = curl_slist_append(list, "Upgrade-Insecure-Requests: 1");
	return list;
}

std::expected<long, int> CurlClient::_fetch(const std::string& url, bool head)
{
	std::unique_ptr<CURL, CurlCleanup> curl{ curl_easy_init() };
	if (!curl)
		return std::unexpected(static_cast<int>(CURLE_FAILED_INIT));

	std::unique_ptr<curl_slist, SlistCleanup> headers{ _buildHeaders() };

	curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl.get(), CURLOPT_FRESH_CONNECT, 1L);
	curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, static_cast<long>(c_max_redirects));
	curl_easy_setopt(curl.get(), CURLOPT_NOBODY, head ? 1L : 0L);
	curl_easy_setopt(curl.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_MAX_DEFAULT);
	curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, c_user_agent.c_str());
	curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
	curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, static_cast<long>(c_check_timeout_sec));
	curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, static_cast<long>(c_connect_timeout_sec));
	curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, &CurlClient::_writeCallback);

	CURLcode res = curl_easy_perform(curl.get());
	if (res != CURLE_OK)
		return std::unexpected(static_cast<int>(res));

	long code{ 0 };
	curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &code);
	return code;
}

std::expected<long, int> CurlClient::checkHost(const std::string& host)
{
	const std::string_view suffix{ ".googlevideo.com" };
	const bool			   is_googlevideo = host.size() > suffix.size() && host.ends_with(suffix);

	const std::string url = is_googlevideo ? std::format("https://{}/videoplayback?expire=1", host) : std::format("https://{}", host);

	auto result = _fetch(url, true);
	if (!result)
		return _fetch(url, false);

	return result;
}
