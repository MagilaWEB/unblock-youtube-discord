export module helper.curl_client;

import std;
import ext.curl;
import helper.types;

/** RAII deleter for CURL. */
export class CurlCleanup
{
public:
	void operator()(CURL* curl) const;
};

/** RAII deleter for curl_slist. */
export class SlistCleanup
{
public:
	void operator()(curl_slist* list) const;
};

/** Global libcurl init/cleanup RAII. */
export class CurlGlobal
{
public:
	CurlGlobal();
	~CurlGlobal();
	CurlGlobal(const CurlGlobal&)			 = delete;
	CurlGlobal& operator=(const CurlGlobal&) = delete;
};

/** Abort predicate polled by libcurl while a transfer is in flight; returning
 *  true kills it with CURLE_ABORTED_BY_CALLBACK. The helper uses it to drop a
 *  probe the moment lua rules the current strategy dead: the next probe must
 *  start on the new strategy instead of waiting out the old one's timeout. */
export using CurlAbort = std::function<bool()>;

/** HTTP client for host availability checks. */
export class CurlClient
{
public:
	/**
	 * Check host availability with a plain GET of https://<host>. The body
	 * is read through the write callback and discarded: the
	 * request must
	 * actually flow, so a throttled or dead path fails.
	 * @param abort polled while the transfer runs; true aborts it.
	 * @return HTTP response code, or curl error code on failure.
	 */
	static std::expected<long, int> checkHost(const std::string& host, const CurlAbort& abort = {});

	/**
	 * Terminal (strategy-independent) failure: the check died before the
	 * first packet (DNS resolution). Desync operates on packets, so
	 * no
	 * strategy can fix it — the host counts as fully-tried immediately.
	 * Anything later (refused/timeout/TLS) saw packets and stays with
	 * the
	 * normal hunt.
	 */
	static bool isTerminalError(int curl_code);

	/**
	 * Voice-gateway check: TLS connect, then a raw WebSocket upgrade and a
	 * sustained ping/pong exchange. Plain curl only proves the
	 * TLS
	 * handshake, which survives on almost every strategy; the upgrade
	 * response and the following frames are the server appdata the
	 * killer
	 * starves, so this check reflects the actual voice path.
	 * @param abort polled between ping/pong steps; true aborts the probe.
	 * @return HTTP response code of the upgrade, or curl error code.
	 */
	static std::expected<long, int> checkVoiceHost(const std::string& host, const CurlAbort& abort = {});

private:
	/** Perform a single GET request. */
	static std::expected<long, int> _fetch(const std::string& url, const CurlAbort& abort);
	/** libcurl progress trampoline: turns the abort predicate into a cancel. */
	static int						_progressAbort(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
	/** Response body sink (discarded, not stored). */
	static size_t					_writeCallback(char*, size_t size, size_t count, void*);
	/** Request headers close to a browser's. */
	static curl_slist*				_buildHeaders();
};
