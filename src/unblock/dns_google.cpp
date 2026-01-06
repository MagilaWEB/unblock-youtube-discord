#include "dns_host.h"
#include "curl/curl.h"

inline static const std::regex reg_symbols_del{ "\\{|\\}|\\[|\\]|\"|" };
inline static const std::regex reg_period_comma_del{ "\\.\\," };
inline static const std::regex reg_equally{ "\\:" };
inline static const std::regex reg_comma_whitespace{ "\\.\\ " };

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
	userp->append(static_cast<char*>(contents), size * nmemb);
	return size * nmemb;
}

DNSHost::Google::Google(std::string test_domain)
{
	_curl = curl_easy_init();

	_url.append(test_domain);

	curl_easy_setopt(_curl, CURLOPT_URL, _url.c_str());
	curl_easy_setopt(_curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &_stringBuffer);
}

DNSHost::Google::~Google()
{
	if (_curl)
		curl_easy_cleanup(_curl);
}

void DNSHost::Google::run()
{
	if (!_curl)
		return;

	_code_result = static_cast<u32>(curl_easy_perform(_curl));
	if (_code_result != CURLcode::CURLE_OK)
		return;

	auto stringBuffer = std::regex_replace(_stringBuffer, reg_symbols_del, "");
	stringBuffer	  = std::regex_replace(stringBuffer, reg_period_comma_del, ",");
	stringBuffer	  = std::regex_replace(stringBuffer, reg_comma_whitespace, " ");

	std::stringstream stream{ stringBuffer };
	std::string		  line;

	std::string name{};
	while (std::getline(stream, line, ','))
		if (line.contains("Answer:name"))
		{
			static std::string answer_del{ "Answer:" };
			_formatToMap(name, line.substr(answer_del.length(), line.length()));
		}
		else if (line.contains("data"))
			_formatToMap(name, line);
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
