#pragma once
#include "http_load_content.h"

const std::regex& reg_ipv4_pattern();
const std::regex& reg_domain_regex();
extern const std::vector<unsigned char>& data_vec();

class DNSHost final : public utils::DefaultInit
{
	std::filesystem::path _etc{};
	std::filesystem::path _host{};
	std::filesystem::path _host_backup{};
	std::filesystem::path _host_user{};
	std::filesystem::path _dir_dns_hosts{};

	File _file_hosts;
	File _file_hosts_backup;
	File _file_hosts_user;

	std::list<std::string>			   _list_domains{};
	std::list<std::string>			   _list_dns_hosts_file_name{};
	std::map<std::string, std::string> _map_list{};

	CriticalSection	 _lock;
	std::atomic_uint _size_iter{ 0 };
	std::atomic_bool _user_host_complete{ false };
	std::atomic_bool _cancel_update{ false };
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
		std::unique_ptr<HttpsLoad> _http;
		std::string				   _url{ "https://dns.google/resolve?name=" };

		MapDomainIP _map_domains_ip{};

		void _formatToMap(std::string&, std::string_view);
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
	std::string									_pathHostDir();
	void										_loadInfo();
	std::optional<DNSHost::Google::MapDomainIP> _getIPGoogle(std::string domain);
	void										_writeDomain(std::string domain);
};
