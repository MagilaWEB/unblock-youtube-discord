#include "domain_testing.h"
#include "curl/curl.h"

DomainTesting::DomainTesting(bool enable_proxy) : _proxy(enable_proxy)
{
	_files_test_domain->open((Core::get().configsPath() / "service"), ".config", true);
}

DomainTesting::~DomainTesting()
{
	_is_testing = false;

	for (auto& curl_domain : _list_domain)
		if (curl_domain.curl)
			curl_easy_cleanup(curl_domain.curl);

	_list_domain.clear();
}

void DomainTesting::loadDomain(bool video)
{
	for (auto& curl_domain : _list_domain)
		if (curl_domain.curl)
			curl_easy_cleanup(curl_domain.curl);

	_list_domain.clear();

	if (video)
	{
		_loadFile("domain_video");

		_file_test_domain->forLine(
			[this](std::string str)
			{
				if (str.empty())
					return false;

				_list_domain.emplace_back(CurlDomain{ curl_easy_init(), str });
				return false;
			}
		);
	}
	else
	{
		_files_test_domain->forLine(
			[this](std::string str_line)
			{
				if (str_line.empty())
					return false;

				_loadFile(str_line);

				_file_test_domain->forLine(
					[this](std::string str)
					{
						if (str.empty())
							return false;

						_list_domain.emplace_back(CurlDomain{ curl_easy_init(), str });
						return false;
					}
				);

				return false;
			}
		);
	}
}

void DomainTesting::changeProxy(std::string ip, u32 port)
{
	_proxyIP   = ip;
	_proxyPORT = port;
}

void DomainTesting::test(bool test_video, std::function<void(pcstr url, bool state)>&& callback)
{
	Debug::info("Start test domain.");

	_is_testing		= true;
	_cancel_testing = false;
	_domain_error = _domain_ok = 0;

	loadDomain(test_video);

	std::for_each(
		std::execution::par,
		_list_domain.begin(),
		_list_domain.end(),
		[this, test_video, callback](const CurlDomain& domain)
		{
			if ((!_accurate_test) && errorRate() >= MAX_ERROR_CONECTION)
			{
				_domain_error++;
				return;
			}

			bool state{ false };

			if (test_video ? isConnectionUrlVideo(domain) : isConnectionUrl(domain))
			{
				state = true;
				_domain_ok++;
			}
			else
			{
				InputConsole::textWarning("проблема доступа: %s", domain.url.c_str());
				_domain_error++;
			}

			callback(domain.url.c_str(), state);
		}
	);

	_is_testing = false;
	Debug::info("Finish test domain.");
}

void DomainTesting::changeAccurateTest(bool state)
{
	_accurate_test = state;
}

void DomainTesting::cancelTesting()
{
	_cancel_testing = true;
}

u32 DomainTesting::successRate() const
{
	return static_cast<u32>((static_cast<float>(_domain_ok.load()) / static_cast<float>(_list_domain.size())) * 100.f);
}

u32 DomainTesting::errorRate() const
{
	return static_cast<u32>((static_cast<float>(_domain_error.load()) / static_cast<float>(_list_domain.size())) * 100.f);
}

void DomainTesting::printTestInfo() const
{
	InputConsole::textInfo(
		"Тестирование завершилось с результатом [%d] из [%d] общий успех [%d%%]",
		_domain_ok.load(),
		_list_domain.size(),
		successRate()
	);
}

static size_t progress_callback(void* clientp, curl_off_t /*dltotal*/, curl_off_t /*dlnow*/, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
	if (clientp)
	{
		const auto domain = static_cast<DomainTesting*>(clientp);
		if (domain->isCancelTesting())
			return CURLE_COULDNT_CONNECT;

		if (!domain->isAccurateTest())
		{
			const u32 error_rate = domain->errorRate();
			if (error_rate >= MAX_ERROR_CONECTION)
				return CURLE_COULDNT_CONNECT;
		}
	}

	return CURLE_OK;
}

static size_t write_data(void* /*buffer*/, size_t size, size_t nmemb, void* /*userdata*/)
{
	return size * nmemb;
}

bool DomainTesting::isConnectionUrlVideo(const CurlDomain& domain) const
{
	if (domain.curl)
	{
		u32 count_connection{ 0U };

		curl_easy_setopt(domain.curl, CURLOPT_URL, domain.url.c_str());
		if (_proxy)
		{
			curl_easy_setopt(domain.curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
			curl_easy_setopt(domain.curl, CURLOPT_PROXY, _proxyIP.c_str());
			curl_easy_setopt(domain.curl, CURLOPT_PORT, std::to_string(_proxyPORT).c_str());
		}

		curl_easy_setopt(domain.curl, CURLOPT_HTTPHEADER, 0L);
		curl_easy_setopt(domain.curl, CURLOPT_HTTPGET, 1L);
		curl_easy_setopt(
			domain.curl,
			CURLOPT_USERAGENT,
			"Mozilla/5.0 ( Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/137.0.0.0 Safari/537.36 "
			"YaBrowser/138.0.9197.153"
		);

		curl_easy_setopt(domain.curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(domain.curl, CURLOPT_XFERINFODATA, this);
		curl_easy_setopt(domain.curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

		curl_easy_setopt(domain.curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(domain.curl, CURLOPT_CONNECTTIMEOUT, _accurate_test ? 10L : 5L);
		curl_easy_setopt(domain.curl, CURLOPT_TIMEOUT, _accurate_test ? 10L : 5L);

		while (count_connection++ < 8)
		{
			u32 http_code{ 0U };

			curl_easy_perform(domain.curl);
			curl_easy_getinfo(domain.curl, CURLINFO_RESPONSE_CODE, &http_code);

			if (http_code != 0)
				return http_code == 403;
		}
	}

	return false;
}

bool DomainTesting::isConnectionUrl(const CurlDomain& domain) const
{
	if (domain.curl)
	{
		u32 count_connection{ 0U };

		curl_easy_setopt(domain.curl, CURLOPT_URL, domain.url.c_str());

		if (_proxy)
		{
			curl_easy_setopt(domain.curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
			curl_easy_setopt(domain.curl, CURLOPT_PROXY, _proxyIP.c_str());
			curl_easy_setopt(domain.curl, CURLOPT_PORT, std::to_string(_proxyPORT).c_str());
		}

		curl_easy_setopt(
			domain.curl,
			CURLOPT_USERAGENT,
			"Mozilla/5.0 ( Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/137.0.0.0 Safari/537.36 "
			"YaBrowser/138.0.9197.153"
		);

		curl_easy_setopt(domain.curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(domain.curl, CURLOPT_XFERINFODATA, this);
		curl_easy_setopt(domain.curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

		curl_easy_setopt(domain.curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(domain.curl, CURLOPT_CONNECTTIMEOUT, _accurate_test ? 10L : 5L);
		curl_easy_setopt(domain.curl, CURLOPT_TIMEOUT, _accurate_test ? 10L : 5L);

		while (count_connection++ < 2)
		{
			if (count_connection == 2)
			{
				curl_easy_setopt(domain.curl, CURLOPT_HTTPHEADER, 0L);
				curl_easy_setopt(domain.curl, CURLOPT_HTTPGET, 1L);
			}

			u32 http_code{ 0U };

			curl_easy_perform(domain.curl);
			curl_easy_getinfo(domain.curl, CURLINFO_RESPONSE_CODE, &http_code);

			switch (http_code)
			{
			case 200U:
			{
				return true;
			}
			case 201U:
			{
				return true;
			}
			case 202U:
			{
				return true;
			}
			case 203U:
			{
				return true;
			}
			case 204U:
			{
				return true;
			}
			case 301U:
			{
				return true;
			}
			case 302U:
			{
				return true;
			}
			case 303U:
			{
				return true;
			}
			case 304U:
			{
				return true;
			}
			case 400U:
			{
				return true;
			}
			case 401U:
			{
				return true;
			}
			case 403U:
			{
				return true;
			}
			case 404U:
			{
				return true;
			}
			case 405U:
			{
				return true;
			}
			case 520U:
			{
				return true;
			}
			case 530U:
			{
				return true;
			}
			default:
				break;
			}
		}
	}

	return false;
}

void DomainTesting::_loadFile(std::filesystem::path file)
{
	_file_test_domain->open((Core::get().configsPath() / file), ".list", true);
}
