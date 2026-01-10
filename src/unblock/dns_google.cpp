#include "dns_host.h"
#include "curl/curl.h"

inline static const std::regex reg_symbols_del{ "\\{|\\}|\\[|\\]|\"|" };
inline static const std::regex reg_period_comma_del{ "\\.\\," };
inline static const std::regex reg_equally{ "\\:" };
inline static const std::regex reg_comma_whitespace{ "\\.\\ " };

DNSHost::Google::Google(std::string test_domain)
{
	_url.append(test_domain);
	_url.append("&edns_client_subnet=0.0.0.0/0");
	http  = std::make_unique<HttpsLoad>(_url);
}

void DNSHost::Google::run()
{
	auto lines = http->run();
	for (auto& line : lines)
	{
		auto stringBuffer = std::regex_replace(line, reg_symbols_del, "");
		stringBuffer	  = std::regex_replace(stringBuffer, reg_period_comma_del, ",");
		stringBuffer	  = std::regex_replace(stringBuffer, reg_comma_whitespace, " ");

		std::stringstream stream{ stringBuffer };
		std::string		  new_line;

		std::string name{};
		while (std::getline(stream, new_line, ','))
			if (new_line.contains("Answer:name"))
			{
				static std::string answer_del{ "Answer:" };
				_formatToMap(name, new_line.substr(answer_del.length(), new_line.length()));
			}
			else if (new_line.contains("data"))
				_formatToMap(name, new_line);
	}
}

const DNSHost::Google::MapDomainIP& DNSHost::Google::content() const
{
	return _map_domains_ip;
}

void DNSHost::Google::_formatToMap(std::string& domain, std::string str)
{
	std::smatch para;
	if (std::regex_search(str, para, reg_equally))
	{
		auto&		prefix = para.prefix();
		std::string value{ para.suffix().str() };
		if (prefix == "name")
		{
			domain = value;
			return;
		}

		if (domain.empty())
			return;

		if (prefix != "data")
			return;

		if (std::regex_match(value, reg_ipv4_pattern) || std::regex_match(value, reg_domain_regex))
			_map_domains_ip[domain].push_back(value);
	}
}
