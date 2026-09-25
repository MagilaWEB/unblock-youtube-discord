#pragma once

#include <curl/curl.h>

#include <expected>
#include <string>

#include "types.h"

/** RAII deleter for CURL. */
class CurlCleanup
{
public:
	void operator()(CURL* curl) const;
};

/** RAII deleter for curl_slist. */
class SlistCleanup
{
public:
	void operator()(curl_slist* list) const;
};

/** Global libcurl init/cleanup RAII. */
class CurlGlobal
{
public:
	CurlGlobal();
	~CurlGlobal();
	CurlGlobal(const CurlGlobal&)			 = delete;
	CurlGlobal& operator=(const CurlGlobal&) = delete;
};

/** HTTP client for host availability checks. */
class CurlClient
{
public:
	/** Apply runtime timeouts (from HelperConfig / UDP CONFIG:). Values are clamped. */
	static void configure(u32 check_timeout_sec, u32 connect_timeout_sec, u32 max_redirects);

	/**
	 * Check host availability. HEAD first (cheap alive test), then a ranged
	 * GET of the first kilobyte with a low-speed guard: OK requires the
	 * body to actually flow, so throttling-after-handshake counts as FAIL.
	 * Falls back to plain GET when HEAD itself fails.
	 * @return HTTP response code, or curl error code on failure.
	 */
	static std::expected<long, int> checkHost(const std::string& host);

	/**
	 * Terminal (strategy-independent) failure: the check died before the
	 * first packet (DNS resolution). Desync operates on packets, so no
	 * strategy can fix it — the host counts as fully-tried immediately.
	 * Anything later (refused/timeout/TLS) saw packets and stays with the
	 * normal hunt.
	 */
	static bool isTerminalError(int curl_code);

	/**
	 * Voice-gateway check: TLS connect, then a raw WebSocket upgrade and a
	 * sustained ping/pong exchange. Plain curl only proves the TLS
	 * handshake, which survives on almost every strategy; the upgrade
	 * response and the following frames are the server appdata the killer
	 * starves, so this check reflects the actual voice path.
	 * @return HTTP response code of the upgrade, or curl error code.
	 */
	static std::expected<long, int> checkVoiceHost(const std::string& host);

private:
	/** Perform a single request (head or get). */
	static std::expected<long, int> _fetch(const std::string& url, bool head);
	/** Response body sink (discarded, not stored). */
	static size_t					_writeCallback(char*, size_t size, size_t count, void*);
	/** Request headers close to a browser's. */
	static curl_slist*				_buildHeaders();
};
