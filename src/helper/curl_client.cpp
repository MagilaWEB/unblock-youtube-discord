#include "curl_client.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <memory>
#include <string_view>
#include <thread>

namespace
{
	inline constexpr const char* c_user_agent{
		"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36"
	};
	inline constexpr u32 c_check_timeout_default{ 6 };
	inline constexpr u32 c_connect_timeout_default{ 5 };
	inline constexpr u32 c_max_redirects_default{ 5 };

	// Bulk-flow proof (throttling-after-handshake precaution): headers may
	// fly while the body is starved to ~100 B/s. A ranged GET of the first
	// kilobyte must arrive faster than the floor below, otherwise the host
	// counts as broken. Small fast pages are unaffected.
	inline constexpr long c_bulk_probe_range_end{ 1'023 };
	inline constexpr long c_low_speed_limit_bps{ 500 };
	inline constexpr long c_low_speed_time_sec{ 6 };

	// Voice-gateway probe: overall budget and the ping/pong exchange shape.
	inline constexpr u32 c_voice_timeout_sec{ 14 };
	inline constexpr u32 c_voice_ping_count{ 4 };
	inline constexpr u32 c_voice_ping_gap_ms{ 1'500 };
	inline constexpr u32 c_voice_recv_step_ms{ 100 };

	// RFC 6455 sample key: the gateway never validates it, the value only
	// has to be a valid base64 of 16 bytes.
	inline constexpr std::string_view c_ws_key{ "dGhlIHNhbXBsZSBub25jZQ==" };
}

void CurlCleanup::operator()(CURL* curl) const
{
	if (curl)
		curl_easy_cleanup(curl);
}

namespace
{
	// Runtime-tunable via CurlClient::configure() (HelperConfig / UDP CONFIG:).
	inline u32 g_check_timeout_sec{ c_check_timeout_default };
	inline u32 g_connect_timeout_sec{ c_connect_timeout_default };
	inline u32 g_max_redirects{ c_max_redirects_default };
}	 // namespace

void CurlClient::configure(u32 check_timeout_sec, u32 connect_timeout_sec, u32 max_redirects)
{
	g_check_timeout_sec	  = std::clamp(check_timeout_sec, 1u, 60u);
	g_connect_timeout_sec = std::clamp(connect_timeout_sec, 1u, 30u);
	g_max_redirects		  = std::clamp(max_redirects, 0u, 10u);
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
	curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, static_cast<long>(g_max_redirects));
	curl_easy_setopt(curl.get(), CURLOPT_NOBODY, head ? 1L : 0L);
	curl_easy_setopt(curl.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_MAX_DEFAULT);
	curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, c_user_agent);
	curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
	curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, static_cast<long>(g_check_timeout_sec));
	curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, static_cast<long>(g_connect_timeout_sec));
	curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, c_low_speed_limit_bps);
	curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, c_low_speed_time_sec);
	curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, &CurlClient::_writeCallback);

	if (!head)
	{
		// Body-flow proof: first kilobyte only, the write callback drops
		// everything. Servers ignoring Range send 200 + full body (roots
		// are small); 206/200/416 all prove the path, the speed guard
		// judges starvation.
		const std::string range = std::format("0-{}", c_bulk_probe_range_end);
		curl_easy_setopt(curl.get(), CURLOPT_RANGE, range.c_str());
	}

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

	// Headers fly but the body may be starved (throttling-after-handshake):
	// a HEAD-only OK locked "direct works" while file downloads crawled.
	// The ranged GET proof decides.
	return _fetch(url, false);
}

std::expected<long, int> CurlClient::checkVoiceHost(const std::string& host)
{
	std::unique_ptr<CURL, CurlCleanup> curl{ curl_easy_init() };
	if (!curl)
		return std::unexpected(static_cast<int>(CURLE_FAILED_INIT));

	// CONNECT_ONLY=2: TLS to the host, then raw send/recv over the socket.
	curl_easy_setopt(curl.get(), CURLOPT_URL, std::format("https://{}", host).c_str());
	curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl.get(), CURLOPT_FRESH_CONNECT, 1L);
	curl_easy_setopt(curl.get(), CURLOPT_CONNECT_ONLY, 2L);
	curl_easy_setopt(curl.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_MAX_DEFAULT);
	curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, c_user_agent);
	curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, static_cast<long>(g_connect_timeout_sec));

	CURLcode res = curl_easy_perform(curl.get());
	if (res != CURLE_OK)
		return std::unexpected(static_cast<int>(res));

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(c_voice_timeout_sec);

	// WebSocket upgrade (RFC 6455). The gateway answers 101 and pushes its
	// hello frame; a rejection with 4xx is fine too - any status proves the
	// server appdata path, which is exactly what the killer starves.
	const std::string request = std::format(
		"GET /?v=10 HTTP/1.1\r\n"
		"Host: {}\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: {}\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"User-Agent: {}\r\n"
		"\r\n",
		host,
		c_ws_key,
		c_user_agent
	);

	size_t sent = 0;
	res			= curl_easy_send(curl.get(), request.data(), request.size(), &sent);
	if (res != CURLE_OK || sent != request.size())
		return std::unexpected(static_cast<int>(res == CURLE_OK ? CURLE_SEND_ERROR : res));

	// Read until the end of the response headers.
	std::string response;
	long		code = 0;
	while (std::chrono::steady_clock::now() < deadline)
	{
		char   buffer[512];
		size_t received = 0;
		res				= curl_easy_recv(curl.get(), buffer, sizeof(buffer), &received);
		if (res == CURLE_OK && received > 0)
		{
			response.append(buffer, received);

			if (const auto body = response.find("\r\n\r\n"); body != std::string::npos)
			{
				if (const auto space = response.find(' '); space != std::string::npos)
					code = std::atol(response.c_str() + space + 1);
				break;
			}
			continue;
		}

		if (res == CURLE_AGAIN)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(c_voice_recv_step_ms));
			continue;
		}

		// CURLE_OK with zero bytes = clean close: the appdata path is dead.
		return std::unexpected(static_cast<int>(res == CURLE_OK ? CURLE_RECV_ERROR : res));
	}

	if (code == 0)
		return std::unexpected(static_cast<int>(CURLE_OPERATION_TIMEDOUT));

	// Sustained exchange: masked ping frames, expecting pong or any inbound
	// bytes. An empty-payload client frame still requires the MASK bit per
	// RFC 6455.
	const std::string ping{ "\x89\x80\x00\x00\x00\x00", 6 };
	for (u32 i = 0; i < c_voice_ping_count && std::chrono::steady_clock::now() < deadline; ++i)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(c_voice_ping_gap_ms));

		res = curl_easy_send(curl.get(), ping.data(), ping.size(), &sent);
		if (res != CURLE_OK)
			return std::unexpected(static_cast<int>(res));

		const auto pong_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(c_voice_ping_gap_ms);
		while (std::chrono::steady_clock::now() < pong_deadline)
		{
			char   buffer[256];
			size_t received = 0;
			res				= curl_easy_recv(curl.get(), buffer, sizeof(buffer), &received);
			if (res == CURLE_OK && received > 0)
				break;	  // any inbound byte = the path is alive

			if (res != CURLE_AGAIN)
				return std::unexpected(static_cast<int>(res == CURLE_OK ? CURLE_RECV_ERROR : res));

			std::this_thread::sleep_for(std::chrono::milliseconds(c_voice_recv_step_ms));
		}
	}

	return code;
}
