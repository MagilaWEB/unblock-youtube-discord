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
	/**
	 * Check host availability. HEAD first, falls back to GET on 403/405 or error.
	 * @return HTTP response code, or curl error code on failure.
	 */
	static std::expected<long, int> checkHost(const std::string& host);

private:
	/** Perform a single request (head or get). */
	static std::expected<long, int> _fetch(const std::string& url, bool head);
	/** Response body sink (discarded, not stored). */
	static size_t					_writeCallback(char*, size_t size, size_t count, void*);
	/** Request headers close to a browser's. */
	static curl_slist*				_buildHeaders();
};
