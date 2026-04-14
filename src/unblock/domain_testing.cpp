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

DomainTesting::DomainTesting()
{
	CURL* curl = curl_easy_init();

	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, "https://yandex.ru/");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullptr);
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
				callback(domain.url, state);

				return domain.url;
			};

			if (isConnectionUrl(domain))
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
	if (!domain.curl)
		return false;

	curl_easy_setopt(domain.curl, CURLOPT_URL, domain.url.c_str());
	curl_easy_setopt(domain.curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(domain.curl, CURLOPT_FRESH_CONNECT, 1L);
	curl_easy_setopt(domain.curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(domain.curl, CURLOPT_CONNECT_ONLY, 1L);
	curl_easy_setopt(domain.curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_MAX_DEFAULT);

	long timeout = _max_wait_testing.load();
	curl_easy_setopt(domain.curl, CURLOPT_CONNECTTIMEOUT, timeout);
	curl_easy_setopt(domain.curl, CURLOPT_TIMEOUT, timeout);

	curl_easy_setopt(domain.curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(domain.curl, CURLOPT_XFERINFODATA, this);
	curl_easy_setopt(domain.curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

	curl_easy_setopt(domain.curl, CURLOPT_WRITEFUNCTION, nullptr);
	curl_easy_setopt(domain.curl, CURLOPT_READFUNCTION, nullptr);
	curl_easy_setopt(domain.curl, CURLOPT_HEADERFUNCTION, nullptr);

	const u32 max_retries = 100;
	u32		  retry		  = 0;

	while (retry < max_retries)
	{
		CURLcode res = curl_easy_perform(domain.curl);

		if (res == CURLE_OK)
		{
			double connect_time = 0.0;
			curl_easy_getinfo(domain.curl, CURLINFO_CONNECT_TIME, &connect_time);
			domain.result_time_sec = connect_time;
			return true;
		}

		if (res != CURLE_SSL_CONNECT_ERROR)
			break;

		++retry;
	}

	double total_time = 0.0;
	curl_easy_getinfo(domain.curl, CURLINFO_TOTAL_TIME, &total_time);
	domain.result_time_sec = total_time;
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
