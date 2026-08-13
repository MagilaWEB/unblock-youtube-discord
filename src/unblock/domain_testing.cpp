#include "domain_testing.h"
#include "curl/curl.h"
#include "ipc_signals.h"

static size_t progress_callback(void* clientp, curl_off_t /*dltotal*/, curl_off_t /*dlnow*/, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
	if (clientp)
	{
		const auto domain = static_cast<DomainTesting*>(clientp);
		if (domain->isCancelTesting())
			return CURLE_COULDNT_CONNECT;

		// const u32 error_rate = domain->errorRate();
		// if (error_rate >= MAX_ERROR_CONECTION)
		//	return CURLE_COULDNT_CONNECT;
	}

	return CURLE_OK;
}

static size_t write_data(void*, size_t size, size_t nmemb, void*)
{
	return size * nmemb;
}

DomainTesting::DomainTesting()
{
	static bool init_timer_wait_testing{ false };

	if (init_timer_wait_testing)
		return;

	if (CURL* curl = curl_easy_init())
	{
		curl_easy_setopt(curl, CURLOPT_URL, "https://yandex.ru/");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullptr);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		double	 total_time = 0;
		CURLcode res		= curl_easy_perform(curl);
		if (res == CURLE_OK)
		{
			curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
			const u32 time_sec = static_cast<u32>(total_time * 10) + 3;
			_max_wait_testing.store(time_sec > 10 ? 10 : time_sec);

			curl_easy_cleanup(curl);
			init_timer_wait_testing = true;
			return;
		}

		_max_wait_testing.store(5);
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

void DomainTesting::test(bool base_test, std::function<void(std::string url, bool state)>&& callback, bool reset_test)
{
	Debug::info("Start test domain.");

	_is_testing		= true;
	_cancel_testing = false;
	_reset_test		= reset_test;
	_domain_error = _domain_ok = 0;

	if (base_test)
	{
		// BASE TESTING!!!

		_clearURLS();

		_genericURLS("base");

		bool state{ false };
		std::for_each(
			std::execution::par,
			_list_host.begin(),
			_list_host.end(),
			[this, &state](CurlDomain& domain)
			{
				if (isConnectionUrl(this, domain))
				{
					_domain_ok++;
					InputConsole::textOk(Localization::Str{ "str_success_url" }(), domain.url);
				}
				else
				{
					InputConsole::textWarning(Localization::Str{ "str_warning_url" }(), domain.url);
					_domain_error++;
				}

				if (_domain_ok.load() == _list_host.size())
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
		_list_host.begin(),
		_list_host.end(),
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
				callback(domain.url, state);

				return domain.url;
			};

			if (isConnectionUrl(this, domain))
			{
				state = true;
				_domain_ok++;
				InputConsole::textOk(Localization::Str{ "str_success_url" }(), result());
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
	return static_cast<u32>((static_cast<float>(_domain_ok.load()) / static_cast<float>(_list_host.size())) * 100.f);
}

u32 DomainTesting::errorRate() const
{
	return static_cast<u32>((static_cast<float>(_domain_error.load()) / static_cast<float>(_list_host.size())) * 100.f);
}

void DomainTesting::printTestInfo() const
{
	InputConsole::textInfo(Localization::Str{ "str_result_url_testing" }(), _domain_ok.load(), _list_host.size(), successRate());
}

bool DomainTesting::isConnectionUrl(DomainTesting* obj, CurlDomain& domain)
{
	if (!domain.curl)
		return false;

	curl_easy_setopt(domain.curl, CURLOPT_URL, domain.url.c_str());
	curl_easy_setopt(domain.curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(domain.curl, CURLOPT_FRESH_CONNECT, 1L);
	curl_easy_setopt(domain.curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(domain.curl, CURLOPT_MAXREDIRS, 10L);
	curl_easy_setopt(domain.curl, CURLOPT_NOBODY, 0L);
	curl_easy_setopt(domain.curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_MAX_DEFAULT);
	curl_easy_setopt(domain.curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(
		domain.curl,
		CURLOPT_USERAGENT,
		"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36"
	);
	curl_easy_setopt(domain.curl, CURLOPT_ACCEPT_ENCODING, "");

	struct curl_slist* headers = nullptr;
	headers					   = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
	headers					   = curl_slist_append(headers, "Accept-Language: ru-RU,ru;q=0.9,en-US;q=0.8,en;q=0.7");
	headers					   = curl_slist_append(headers, "Cache-Control: no-cache");
	headers					   = curl_slist_append(headers, "Sec-Fetch-Dest: document");
	headers					   = curl_slist_append(headers, "Sec-Fetch-Mode: navigate");
	headers					   = curl_slist_append(headers, "Sec-Fetch-Site: none");
	headers					   = curl_slist_append(headers, "Upgrade-Insecure-Requests: 1");
	curl_easy_setopt(domain.curl, CURLOPT_HTTPHEADER, headers);

	u32 timeout = _max_wait_testing.load();
	curl_easy_setopt(domain.curl, CURLOPT_CONNECTTIMEOUT, timeout);
	curl_easy_setopt(domain.curl, CURLOPT_TIMEOUT, timeout);

	curl_easy_setopt(domain.curl, CURLOPT_NOPROGRESS, 0L);

	curl_easy_setopt(domain.curl, CURLOPT_XFERINFODATA, obj);
	curl_easy_setopt(domain.curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

	curl_easy_setopt(domain.curl, CURLOPT_WRITEFUNCTION, write_data);

	auto& ipc = IPCSignals::get();

	while (true)
	{
		if (obj && obj->isCancelTesting())
			break;

		auto get_host = [&]() -> std::string
		{
			std::smatch m;
			return std::regex_search(domain.url, m, std::regex{ R"(://([^/?#]+))" }) && m.size() > 1 ? m[1].str() : "";
		};

		auto zapret_ckeck = [&]
		{
			if (obj->isResetConect())
			{
				auto host = get_host();

				if (!host.empty() && ipc.has("exhausted", host))
				{
#ifdef DEBUG
					Debug::info("ZAPRET2: exhausted for url[{}]", domain.url);
#endif
					return true;
				}
			}

			return false;
		};

		if (zapret_ckeck())
			break;

		CURLcode res = curl_easy_perform(domain.curl);

		if (res == CURLE_OK)
		{
			u32 http_code = 0;
			curl_easy_getinfo(domain.curl, CURLINFO_RESPONSE_CODE, &http_code);
			if (http_code > 0)
			{
				double total_time = 0.0;
				curl_easy_getinfo(domain.curl, CURLINFO_TOTAL_TIME, &total_time);
				domain.result_time_sec = total_time;
				return true;
			}
		}
		else if (!obj->isResetConect())
			break;

		Debug::info("curl result code[{}], url[{}]", static_cast<u32>(res), domain.url);
	}

	curl_slist_free_all(headers);
	double total_time = 0.0;
	curl_easy_getinfo(domain.curl, CURLINFO_TOTAL_TIME, &total_time);
	domain.result_time_sec = total_time;
	return false;
}

std::vector<std::string> DomainTesting::listHost(bool all)
{
	std::vector<std::string> list_host;
	if (all)
	{
		if (_loadFile("all"))
		{
			if (_file_test_host.isOpen())
			{
				for (auto& str : _file_test_host)
				{
					if (str.empty())
						continue;

					list_host.emplace_back(str);
				}
			}

			_file_test_host.close();
		}

		return list_host;
	}

	for (auto& name : _section_opt_service_names)
	{
		if (_loadFile(name))
		{
			if (_file_test_host.isOpen())
			{
				for (auto& str : _file_test_host)
				{
					if (str.empty())
						continue;

					list_host.emplace_back(str);
				}
			}

			_file_test_host.close();
		}
	}

	return list_host;
}

bool DomainTesting::_loadFile(std::filesystem::path file)
{
	_file_test_host.open(Core::get().configsPath() / "domain_test" / file.string(), ".list", true);
	return _file_test_host.isOpen() && !_file_test_host.empty();
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
	if (_file_test_host.isOpen())
	{
		for (auto& str : _file_test_host)
		{
			if (str.empty())
				continue;

			_list_host.emplace_back(CurlDomain{ curl_easy_init(), str });
		}
	}

	_file_test_host.close();
}

void DomainTesting::_clearURLS()
{
	for (auto& curl_domain : _list_host)
		if (curl_domain.curl)
			curl_easy_cleanup(curl_domain.curl);

	_list_host.clear();
}
