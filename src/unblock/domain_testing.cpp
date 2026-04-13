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

static size_t write_data(void*, size_t size, size_t nmemb, void*)
{
	size_t total = size * nmemb;
	return total;
}

DomainTesting::DomainTesting()
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

		const u32 time_sec = static_cast<u32>(total_time * 10) + 5;
		_max_wait_testing.store(time_sec > 10 ? 10 : time_sec);

		curl_easy_cleanup(curl);
	}
}

DomainTesting::~DomainTesting()
{
	_clearURLS();
}

void DomainTesting::loadDomain()
{
	_clearURLS();
	_genericURLS();
}

void DomainTesting::changeProxy(std::string_view ip, u32 port)
{
	_proxyIP   = ip;
	_proxyPORT = port;
}

void DomainTesting::test(bool base_test, std::function<void(std::string url, bool state)>&& callback)
{
	Debug::info("Start test domain.");

	_is_testing		= true;
	_cancel_testing = false;
	_domain_error = _domain_ok = 0;

	if (base_test)
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
				{
					_domain_ok++;
					InputConsole::textOk(Localization::Str{ "str_success_url" }(), domain.url);
				}
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

	loadDomain();

	std::for_each(
		std::execution::par,
		_list_domain.begin(),
		_list_domain.end(),
		[this, callback](CurlDomain& domain)
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

			if (isConnectionUrl(domain))
			{
				state = true;
				_domain_ok++;
				result();
				InputConsole::textOk(Localization::Str{ "str_success_url" }(), domain.url);
				return;
			}

			InputConsole::textWarning(Localization::Str{ "str_warning_url" }(), result());
			_domain_error++;
		}
	);

	_is_testing = false;
	Debug::info("Finish test domain.");
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

bool DomainTesting::isConnectionUrl(CurlDomain& domain)
{
	if (domain.curl)
	{
		curl_easy_setopt(domain.curl, CURLOPT_URL, domain.url.c_str());
		curl_easy_setopt(domain.curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(domain.curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(domain.curl, CURLOPT_NOBODY, 0L);

		curl_easy_setopt(domain.curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(domain.curl, CURLOPT_XFERINFODATA, this);
		curl_easy_setopt(domain.curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

		curl_easy_setopt(domain.curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(domain.curl, CURLOPT_BUFFERSIZE, 32'768L);

		long timeout = _max_wait_testing.load();
		curl_easy_setopt(domain.curl, CURLOPT_CONNECTTIMEOUT, timeout);
		curl_easy_setopt(domain.curl, CURLOPT_TIMEOUT, timeout);
		curl_easy_setopt(domain.curl, CURLOPT_LOW_SPEED_LIMIT, 1'024L);
		curl_easy_setopt(domain.curl, CURLOPT_LOW_SPEED_TIME, 10L);
		curl_easy_setopt(domain.curl, CURLOPT_FRESH_CONNECT, 1L);

		constexpr static std::pair<int, int> ssl_version[]{
			{ CURL_SSLVERSION_TLSv1_0, CURL_SSLVERSION_MAX_TLSv1_0 },
			{ CURL_SSLVERSION_TLSv1_1, CURL_SSLVERSION_MAX_TLSv1_1 },
			{ CURL_SSLVERSION_TLSv1_2, CURL_SSLVERSION_MAX_TLSv1_2 },
			{ CURL_SSLVERSION_TLSv1_3, CURL_SSLVERSION_MAX_TLSv1_3 }
		};
		constexpr static u32 size_ssl_version{ sizeof(ssl_version) / sizeof(std::pair<int, int>) };

		domain.result_time_sec = 0;

		u32 iter{ 0 };
		u32 reset{ 0 };
		u32 reset_time{ 0 };
		while (iter < size_ssl_version)
		{
			auto& [version, version_max] = ssl_version[iter];
			curl_easy_setopt(domain.curl, CURLOPT_SSLVERSION, version | version_max);

			auto skip = [&reset, &reset_time, &iter]
			{
				reset	   = 0;
				reset_time = 0;
				iter++;
			};

			auto res = curl_easy_perform(domain.curl);
			
			if (res == CURLE_OPERATION_TIMEDOUT || res == CURLE_SSL_CONNECT_ERROR)
			{
				if (res == CURLE_OPERATION_TIMEDOUT && ++reset_time > 1)
					skip();
				else if (res == CURLE_SSL_CONNECT_ERROR && ++reset > 100)
					skip();
				
#ifdef DEBUG
				Debug::info("Reset connect url[{}] zapret2", domain.url);
#endif
				continue;
			}

			curl_tlssessioninfo* tls_info = nullptr;
			CURLcode			 info_res = curl_easy_getinfo(domain.curl, CURLINFO_TLS_SSL_PTR, &tls_info);

			const bool tls_ok = res == CURLE_OK && info_res == CURLE_OK && tls_info && tls_info->backend != CURLSSLBACKEND_NONE;

			domain.setTls(version - CURL_SSLVERSION_TLSv1_0, tls_ok);

			double time{ 0 };
			curl_easy_getinfo(domain.curl, CURLINFO_TOTAL_TIME, &time);

			if (tls_ok)
			{
				if (domain.result_time_sec == 0.0)
					domain.result_time_sec = time;
				else
					domain.result_time_sec = domain.result_time_sec > time ? time : domain.result_time_sec;
			}

			skip();

			if (domain.tls_1_0 && domain.tls_1_1 && domain.tls_1_2 && domain.tls_1_3)
				return true;
		}

		if (domain.tls_1_0 || domain.tls_1_1 || domain.tls_1_2 || domain.tls_1_3)
			return true;

		curl_easy_getinfo(domain.curl, CURLINFO_TOTAL_TIME, &domain.result_time_sec);
	}

	return false;
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
	{
		for (auto& str : _file_test_domain)
		{
			if (str.empty())
				continue;

			_list_domain.emplace_back(CurlDomain{ curl_easy_init(), str });
		}
	}

	_file_test_domain.close();
}

void DomainTesting::_clearURLS()
{
	for (auto& curl_domain : _list_domain)
		if (curl_domain.curl)
			curl_easy_cleanup(curl_domain.curl);

	_list_domain.clear();
}
