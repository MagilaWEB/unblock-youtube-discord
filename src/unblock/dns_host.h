#pragma once
#include "http_load_content.h"

inline static const std::regex reg_ipv4_pattern{ R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)" };
inline static const std::regex reg_domain_regex{ R"(^([a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}$)" };

class DNSHost final : public utils::DefaultInit
{
	std::filesystem::path _etc{ std::filesystem::temp_directory_path().root_path() / "Windows" / "System32" / "drivers" / "etc" };
	std::filesystem::path _host{};
	std::filesystem::path _host_backup{};
	std::filesystem::path _host_user{};
	std::filesystem::path _dir_dns_hosts{};

	Ptr<File> _file_hosts;
	Ptr<File> _file_hosts_backup;
	Ptr<File> _file_hosts_user;

	std::list<std::string>							_list_domains{};
	std::list<std::string>							_list_dns_hosts_file_name{};
	std::map<std::string, std::vector<std::string>> _map_list{};

	CriticalSection	 _lock;
	std::atomic_bool _cancel_update{ false };
	std::atomic_uint _size_iter{ 0 };
	bool			 _enable{ false };

public:
	struct Google
	{
		using MapDomainIP = std::map<std::string, std::list<std::string>>;

		Google() = delete;
		Google(std::string);
		~Google() = default;

		void			   run();
		const MapDomainIP& content() const;

	private:
		std::unique_ptr<HttpsLoad> http;
		std::string				   _url{ "https://dns.google/resolve?name=" };
		// std::string _domain{};
		// u32			_code_result{ 0 };
		// std::string _stringBuffer;
		MapDomainIP				   _map_domains_ip{};

		void _formatToMap(std::string&, std::string);
	};

public:
	DNSHost();

	const std::list<std::string>& listDnsFileName();
	void						  enable();
	void						  disable();

	void update();

	bool isHostsUser() const;
	void cancel();

	float percentageCompletion() const;

private:
	std::optional<DNSHost::Google::MapDomainIP> _getIPGoogle(std::string domain);
	void										_writeDomain(std::string domain);
};
