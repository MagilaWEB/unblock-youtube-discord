#include "dns_host.h"
#include "curl/curl.h"

DNSHost::Google::Google(std::string test_domain)
{
	_url.reserve(test_domain.size() + 32);
	_url.append(test_domain);
	_url.append("&edns_client_subnet=0.0.0.0/0");
	_http = std::make_unique<HttpsLoad>(_url);
}

static void removeChars(std::string& s, std::string_view chars)
{
	s.erase(std::remove_if(s.begin(), s.end(), [chars](char c) { return chars.find(c) != std::string_view::npos; }), s.end());
}

static void replaceAll(std::string& s, std::string_view from, std::string_view to)
{
	if (from.empty())
		return;
	size_t pos = 0;
	while ((pos = s.find(from, pos)) != std::string::npos)
	{
		s.replace(pos, from.length(), to);
		pos += to.length();
	}
}

void DNSHost::Google::run()
{
	auto lines = _http->run();
	for (auto& line : lines)
	{
		removeChars(line, "{}[]\"");

		replaceAll(line, ".,", ",");

		replaceAll(line, ". ", " ");

		std::string name;
		size_t		start = 0;
		size_t		end;
		while ((end = line.find(',', start)) != std::string::npos)
		{
			std::string_view token(&line[start], end - start);
			start = end + 1;

			if (token.find("Answer:name") == 0)
			{
				static constexpr std::string_view answerPrefix = "Answer:";
				if (token.size() > answerPrefix.size())
				{
					std::string_view value = token.substr(answerPrefix.size());
					_formatToMap(name, value);
				}
			}
			else if (token.find("data") == 0)
			{
				_formatToMap(name, token);
			}
		}

		if (start < line.size())
		{
			std::string_view token(&line[start], line.size() - start);
			if (token.find("Answer:name") == 0)
			{
				static constexpr std::string_view answerPrefix = "Answer:";
				if (token.size() > answerPrefix.size())
					_formatToMap(name, token.substr(answerPrefix.size()));
			}
			else if (token.find("data") == 0)
			{
				_formatToMap(name, token);
			}
		}
	}
}

void DNSHost::Google::_formatToMap(std::string& domain, std::string_view str)
{
	size_t colonPos = str.find(':');
	if (colonPos == std::string_view::npos)
		return;

	std::string_view key   = str.substr(0, colonPos);
	std::string_view value = str.substr(colonPos + 1);

	while (!value.empty() && std::isspace(value.front()))
		value.remove_prefix(1);

	while (!value.empty() && std::isspace(value.back()))
		value.remove_suffix(1);

	if (key == "name")
	{
		domain = value;
		return;
	}

	if (domain.empty())
		return;

	if (key != "data")
		return;

	std::string valStr(value);
	if (std::regex_match(valStr, reg_ipv4_pattern) || std::regex_match(valStr, reg_domain_regex))
		_map_domains_ip[domain].push_back(std::move(valStr));
}

const DNSHost::Google::MapDomainIP& DNSHost::Google::content() const
{
	return _map_domains_ip;
}
