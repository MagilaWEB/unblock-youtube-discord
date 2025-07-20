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
	_file_test_domain->open((Core::get().configsPath() / file), ".list", true);
}

void DomainTesting::test(bool test_video)
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
		[&test_video, this](std::string domain)
		{
			if (test_video ? isConnectionUrlVideo(domain.c_str()) : isConnectionUrl(domain.c_str()))
				_domain_ok++;
			else
			{
				InputConsole::textWarning("проблема доступа: %s", domain.c_str());
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

bool DomainTesting::isConnectionUrlVideo(pcstr url) const
{
	if (CURL* curl = curl_easy_init())
	{
		u32 count_connection{ 0U };
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, 0L);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
		curl_easy_setopt(
			curl,
			CURLOPT_USERAGENT,
			"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36 Edg/138.0.0.0"
		);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);

		while (count_connection++ < 8)
		{
			u32 http_code{ 0U };

			curl_easy_perform(curl);
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

			if (http_code != 0)
				return http_code == 403;
		}

		curl_easy_cleanup(curl);
	}

	return false;
}

bool DomainTesting::isConnectionUrl(pcstr url) const
{
	if (CURL* curl = curl_easy_init())
	{
		u32 count_connection{ 0U };
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(
			curl,
			CURLOPT_USERAGENT,
			"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36 Edg/138.0.0.0"
		);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);

		while (count_connection++ < 2)
		{
			if (count_connection == 2)
			{
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, 0L);
				curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
			}

			u32 http_code{ 0U };

			curl_easy_perform(curl);
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

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

		curl_easy_cleanup(curl);
	}

	return false;
}
