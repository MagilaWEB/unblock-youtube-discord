#include "domain_testing.h"
#include "curl/curl.h"

static size_t progress_callback(void* clientp, curl_off_t /*dltotal*/, curl_off_t /*dlnow*/, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
	if (clientp)
	{
		const auto domain = static_cast<DomainTesting*>(clientp);
		if (domain->isCancelTesting())
			return CURLE_COULDNT_CONNECT;

		const u32 error_rate = domain->errorRate();
		if (error_rate >= MAX_ERROR_CONECTION)
			return CURLE_COULDNT_CONNECT;
	}

	return CURLE_OK;
}

static size_t write_data(void* /*buffer*/, size_t size, size_t nmemb, void* /*userdata*/)
{
	return size * nmemb;
}

DomainTesting::DomainTesting(bool enable_proxy) : _proxy(enable_proxy)
{
	CURL* curl = curl_easy_init();

	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, "https://yandex.ru/");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

		double	 total_time = 0;
		CURLcode res		= curl_easy_perform(curl);
		if (res == CURLE_OK)
			curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);

		_max_wait_testing.store(static_cast<u32>(total_time * 3) + 4);

		curl_easy_cleanup(curl);
	}
}

DomainTesting::~DomainTesting()
{
	_clearURLS();
}

void DomainTesting::loadDomain(bool video)
{
	_clearURLS();

	if (video || _proxy)
	{
		if (!_loadFile(video ? "domain_video" : "all"))
			return;

		_file_test_domain.forLine(
			[this](std::string str)
			{
				if (str.empty())
					return false;

				_list_domain.emplace_back(CurlDomain{ curl_easy_init(), str });
				return false;
			}
		);

		return;
	}

	_genericURLS();
}

void DomainTesting::changeProxy(std::string_view ip, u32 port)
{
	_proxyIP   = ip;
	_proxyPORT = port;
}

void DomainTesting::test(bool test_video, bool base_test, std::function<void(std::string url, bool state)>&& callback)
{
	Debug::info("Start test domain.");

	_is_testing		= true;
	_cancel_testing = false;
	_domain_error = _domain_ok = 0;

	if (base_test && !test_video)
	{
		// BASE TESTING!!!

		_clearURLS();

		_genericURLS("base");

		bool state{ false };
		std::for_each(
			std::execution::par,
			_list_domain.begin(),
			_list_domain.end(),
			[this, &state](CurlDomain& domain)
			{
				if (isConnectionUrl(domain))
					_domain_ok++;
				else
				{
					InputConsole::textWarning(Localization::Str{ "str_warning_url" }(), domain.url);
					_domain_error++;
				}

				if (_domain_ok.load() == _list_domain.size())
					state = true;
			}
		);

		if (!state)
		{
			Debug::warning("Invalid test domain.");
			_is_testing = false;
			return;
		}

		_domain_error = _domain_ok = 0;

		InputConsole::textOk(Localization::Str{ "str_base_testing_url_success" }());
	}

	loadDomain(test_video);

	std::for_each(
		std::execution::par,
		_list_domain.begin(),
		_list_domain.end(),
		[this, test_video, callback](CurlDomain& domain)
		{
			if (errorRate() >= MAX_ERROR_CONECTION)
			{
				_domain_error++;
				return;
			}

			bool state{ false };

			auto result = [&]
			{
				constexpr const char* TLS_OK  = "\xE2\x9C\x93";
				constexpr const char* TLS_ERR = "\xE2\x9C\x97";
				std::string			  text	  = utils::format(
					 "{:<30} | TLS1.0[{}] TLS1.1[{}] TLS1.2[{}] TLS1.3[{}] | time: {:>5.2f}s",
					 domain.url,
					 domain.tls_1_0 ? TLS_OK : TLS_ERR,
					 domain.tls_1_1 ? TLS_OK : TLS_ERR,
					 domain.tls_1_2 ? TLS_OK : TLS_ERR,
					 domain.tls_1_3 ? TLS_OK : TLS_ERR,
					 domain.result_time_sec
				 );

				callback(text, state);

				return text;
			};

			if (test_video ? isConnectionUrlVideo(domain) : isConnectionUrl(domain))
			{
				state = true;
				_domain_ok++;
				result();
				return;
			}

			InputConsole::textWarning(Localization::Str{ "str_warning_url" }(), result());
			_domain_error++;
		}
	);

	_is_testing = false;
	Debug::info("Finish test domain.");
}

void DomainTesting::changeMaxWaitTesting(u32 second)
{
	ASSERT_ARGS(second > 0, "The connection timeout cannot be equal to 0! This will provoke endless waiting.");
	//_max_wait_testing = second;
}

void DomainTesting::changeMaxConnectionAttempts(u32 count)
{
	ASSERT_ARGS(count > 0, "The connection timeout cannot be equal to 0! This will provoke endless waiting.");
	_max_connection_attempts = count;
}

void DomainTesting::changeOptionalServices(std::list<std::string> list_services)
{
	_section_opt_service_names = list_services;
}

void DomainTesting::cancelTesting()
{
	_cancel_testing = true;
	_is_testing		= false;
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
	InputConsole::textInfo(Localization::Str{ "str_result_url_testing" }(), _domain_ok.load(), _list_domain.size(), successRate());
}

bool DomainTesting::isConnectionUrlVideo(CurlDomain& domain)
{
	if (domain.curl)
	{
		u32 count_connection{ 0U };

		curl_easy_setopt(domain.curl, CURLOPT_URL, domain.url.c_str());
		curl_easy_setopt(domain.curl, CURLOPT_HTTPGET, 0L);
		curl_easy_setopt(domain.curl, CURLOPT_FOLLOWLOCATION, 1L);

		if (_proxy)
		{
			curl_easy_setopt(domain.curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
			curl_easy_setopt(domain.curl, CURLOPT_PROXY, _proxyIP.c_str());
			curl_easy_setopt(domain.curl, CURLOPT_PORT, std::to_string(_proxyPORT).c_str());
		}
		else
			curl_easy_setopt(domain.curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTPS2);

		curl_easy_setopt(domain.curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(domain.curl, CURLOPT_XFERINFODATA, this);
		curl_easy_setopt(domain.curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

		curl_easy_setopt(domain.curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(domain.curl, CURLOPT_TIMEOUT, _max_wait_testing.load());

		while (count_connection++ < _max_connection_attempts.load())
		{
			u32 http_code{ 0U };

			_tlsTesting(domain, http_code);

			if (http_code != 0)
				return http_code == 403;

			if (count_connection < _max_connection_attempts.load())
				curl_easy_setopt(domain.curl, CURLOPT_TIMEOUT, _max_wait_testing.load() / 2);
		}
	}

	return false;
}

bool DomainTesting::isConnectionUrl(CurlDomain& domain)
{
	if (domain.curl)
	{
		curl_easy_setopt(domain.curl, CURLOPT_URL, domain.url.c_str());
		curl_easy_setopt(domain.curl, CURLOPT_HTTPGET, 0L);
		curl_easy_setopt(domain.curl, CURLOPT_FOLLOWLOCATION, 1L);

		if (_proxy)
		{
			curl_easy_setopt(domain.curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
			curl_easy_setopt(domain.curl, CURLOPT_PROXY, _proxyIP.c_str());
			curl_easy_setopt(domain.curl, CURLOPT_PORT, std::to_string(_proxyPORT).c_str());
		}
		else
			curl_easy_setopt(domain.curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTPS2);

		curl_easy_setopt(domain.curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(domain.curl, CURLOPT_XFERINFODATA, this);
		curl_easy_setopt(domain.curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

		curl_easy_setopt(domain.curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(domain.curl, CURLOPT_TIMEOUT, _max_wait_testing.load());

		u32 count_connection{ 0 };
		while (count_connection++ < _max_connection_attempts.load())
		{
			u32 http_code{ 0U };

			_tlsTesting(domain, http_code);

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

		if (count_connection < _max_connection_attempts.load())
			curl_easy_setopt(domain.curl, CURLOPT_TIMEOUT, _max_wait_testing.load() / 2);
	}

	return false;
}

bool DomainTesting::_tlsTesting(CurlDomain& domain, u32& http_code)
{
	constexpr static std::pair<int, int> ssl_version[]{
		{ CURL_SSLVERSION_TLSv1_0, CURL_SSLVERSION_MAX_TLSv1_0 },
		{ CURL_SSLVERSION_TLSv1_1, CURL_SSLVERSION_MAX_TLSv1_1 },
		{ CURL_SSLVERSION_TLSv1_2, CURL_SSLVERSION_MAX_TLSv1_2 },
		{ CURL_SSLVERSION_TLSv1_3, CURL_SSLVERSION_MAX_TLSv1_3 }
	};

	domain.result_time_sec = 0;

	double time{ 0 };
	for (auto& [version, version_max] : ssl_version)
	{
		curl_easy_setopt(domain.curl, CURLOPT_SSLVERSION, version | version_max);

		auto				 res	  = curl_easy_perform(domain.curl);
		curl_tlssessioninfo* tls_info = nullptr;
		CURLcode			 info_res = curl_easy_getinfo(domain.curl, CURLINFO_TLS_SSL_PTR, &tls_info);
		domain.setTls(
			version - CURL_SSLVERSION_TLSv1_0,
			res == CURLE_OK && info_res == CURLE_OK && tls_info && tls_info->backend != CURLSSLBACKEND_NONE
		);

		curl_easy_getinfo(domain.curl, CURLINFO_TOTAL_TIME, &time);
		domain.result_time_sec += time;
	}

	if (!domain.tls_1_0 && !domain.tls_1_1 && !domain.tls_1_2 && !domain.tls_1_3)
		return false;

	curl_easy_getinfo(domain.curl, CURLINFO_RESPONSE_CODE, &http_code);

	return true;
}

bool DomainTesting::_loadFile(std::filesystem::path file)
{
	_file_test_domain.open(Core::get().configsPath() / "domain_test" / file.string(), ".list", true);
	return _file_test_domain.isOpen() && !_file_test_domain.empty();
}

void DomainTesting::_genericURLS(std::string base_name)
{
	if (_section_opt_service_names.empty())
	{
		if (base_name.empty())
			base_name = "all";

		if (_loadFile(base_name))
			_appendURLS();
	}
	else
	{
		if (!base_name.empty())
			base_name += "_";

		for (auto& name : _section_opt_service_names)
			if (_loadFile(base_name + name))
				_appendURLS();
	}
}

void DomainTesting::_appendURLS()
{
	if (_file_test_domain.isOpen())
		_file_test_domain.forLine(
			[this](std::string str)
			{
				if (str.empty())
					return false;

				_list_domain.emplace_back(CurlDomain{ curl_easy_init(), str });
				return false;
			}
		);
}

void DomainTesting::_clearURLS()
{
	for (auto& curl_domain : _list_domain)
		if (curl_domain.curl)
			curl_easy_cleanup(curl_domain.curl);

	_list_domain.clear();
}
