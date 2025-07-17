#include "pch.h"
#include "unblock_api.hpp"
#include "domain_testing.h"
#include "curl/curl.h"

DomainTesting::~DomainTesting()
{
	_is_testing = false;
}

std::string DomainTesting::fileName() const
{
	return _file_test_domain->name();
}

void DomainTesting::loadFile(std::filesystem::path file)
{
	auto file_dir = Core::get().configsPath() / file;
	_file_test_domain->open(file_dir, ".list", true);
}

void DomainTesting::test()
{
	_is_testing	  = false;
	_domain_error = _domain_ok = 0;
	_list_domain.clear();

	_file_test_domain->forLine(
		[this](std::string str)
		{
			_list_domain.push_back(str);
			return false;
		}
	);

	std::for_each(
		std::execution::par,
		_list_domain.begin(),
		_list_domain.end(),
		[this](std::string domain)
		{
			if (isConnectionUrl(domain.c_str()))
				_domain_ok++;
			else
			{
#ifdef DEBUG
				InputConsole::textError("нет доступа: %s", domain.c_str());
#endif
				_domain_error++;
			}

			InputConsole::textInfo(
				"Успех [%d], ошибки [%d], всего на тестировании [%d]",
				_domain_ok.load(),
				_domain_error.load(),
				_list_domain.size()
			);
		}
	);

#ifdef DEBUG
	if (_domain_error)
		InputConsole::pause();
#endif

	_is_testing = true;

	InputConsole::clear();
}

u32 DomainTesting::successRate() const
{
	if (!_is_testing)
	{
		InputConsole::textWarning("Показатель успеха неизвестен, тестирование не проводилось!");
		return 0;
	}

	return static_cast<u32>((static_cast<float>(_domain_ok.load()) / static_cast<float>(_list_domain.size())) * 100.f);
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

static size_t write_data(void* /*buffer*/, size_t size, size_t nmemb, void* /*userp*/)
{
	return size * nmemb;
}

bool DomainTesting::isConnectionUrl(pcstr url) const
{
	bool ok = false;

	httplib::Client cli{ std::string{ "http://" }.append(url).c_str() };
	cli.set_url_encode(true);
	cli.set_tcp_nodelay(true);
	cli.set_write_timeout(15);
	cli.set_connection_timeout(10);

	auto check_http_code = [&ok, &url](u32 code, auto&& curl_start)
	{
		switch (code)
		{
		case httplib::StatusCode::OK_200:
		{
			ok = true;
			break;
		}
		case httplib::StatusCode::MovedPermanently_301:
		{
			ok = true;
			break;
		}
		case httplib::StatusCode::Found_302:
		{
			ok = true;
			break;
		}
		case httplib::StatusCode::SeeOther_303:
		{
			ok = true;
			break;
		}
		case httplib::StatusCode::TemporaryRedirect_307:
		{
			curl_start();
			break;
		}
		case httplib::StatusCode::PermanentRedirect_308:
		{
			curl_start();
			break;
		}
		case httplib::StatusCode::BadRequest_400:
		{
			ok = true;
			break;
		}
		case httplib::StatusCode::Unauthorized_401:
		{
			ok = true;
			break;
		}
		case httplib::StatusCode::NotFound_404:
		{
			ok = true;
			break;
		}
		case httplib::StatusCode::Forbidden_403:
		{
			constexpr pcstr list_sort[]{ "cdn.discord", "videoplayback?", "youtubei." };
			for (auto l_sort : list_sort)
			{
				if (std::string{ url }.find(l_sort) != std::string::npos)
				{
					ok = true;
					break;
				}
			}
			break;
		}
		/*case httplib::StatusCode::MethodNotAllowed_405:
		{
			ok = true;
			break;
		}*/
		default:
			ok = false;
			curl_start();
			break;
		}
	};

	auto curl_test = [&url, &check_http_code]
	{
		CURL* curl = curl_easy_init();
		if (curl)
		{
			u32 http_code = 0;

			curl_easy_setopt(curl, CURLOPT_URL, std::string{ "https://" }.append(url).c_str());
			curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15);

			curl_easy_perform(curl);

			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

			if (http_code != 0)
				check_http_code(http_code, [] {});

			curl_easy_cleanup(curl);
		}
	};

	if (auto res = cli.Options("/"))
		check_http_code(static_cast<u32>(res->status), curl_test);

	if (!ok)
		curl_test();

	return ok;
}
