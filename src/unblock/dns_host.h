#pragma once
#include "http_load_content.h"

inline static const std::regex reg_ipv4_pattern{ R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)" };
inline static const std::regex reg_domain_regex{ R"(^([a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}$)" };
inline static const std::vector<unsigned char> data{ 0x0d, 0x33, 0x34, 0x3e, 0x35, 0x2d, 0x29, 0x75, 0x09, 0x23, 0x29, 0x2e, 0x3f, 0x37,
													 0x69, 0x68, 0x75, 0x3e, 0x28, 0x33, 0x2c, 0x3f, 0x28, 0x29, 0x75, 0x3f, 0x2e, 0x39 };

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

	FastLock	 _lock;
	std::atomic_bool _user_host_complete{ false };
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
