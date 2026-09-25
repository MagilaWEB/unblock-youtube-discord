#pragma once
#include "http_load_content.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>

// Parse a "domain->IP" pin line (surrounding whitespace tolerated).
// Returns the {domain, ip} pair, or nullopt for anything else
// (plain domains, comments, blanks, garbage).
std::optional<std::pair<std::string, std::string>> parseDnsPin(const std::string& line);

const std::regex&						 reg_ipv4_pattern();
const std::regex&						 reg_domain_regex();
extern const std::vector<unsigned char>& data_vec();

class DNSHost final : public utils::DefaultInit
{
	std::filesystem::path _etc{};
	std::filesystem::path _host{};
	std::filesystem::path _host_backup{};
	std::filesystem::path _host_user{};
	std::filesystem::path _dir_dns_hosts{};
	std::filesystem::path _geohide_cache{};
	std::filesystem::path _manual_hosts{};

	File _file_hosts;
	File _file_hosts_backup;
	File _file_hosts_user;

	std::list<std::string> _list_dns_hosts_file_name{};

	mutable std::mutex		   _load_mutex;
	std::shared_ptr<HttpsLoad> _active_load{};
	std::atomic<float>		   _last_progress{ 0.F };

	std::string _region{ "ru" };
	std::string _base_url{ "geohide.ru" };

	std::atomic_bool _user_host_complete{ false };
	std::atomic_bool _cancel_update{ false };
	bool			 _enable{ false };

public:
	DNSHost();

	const std::list<std::string>& listDnsFileName();
	void						  enable();
	void						  disable();

	void update();

	bool isHostsUser() const;
	void cancel();

	// Live GeoHide download progress (0-100) for the wait window.
	float downloadProgress() const;

	void			   setRegion(std::string_view region);
	const std::string& region() const { return _region; }

	void			   setBaseUrl(std::string_view url);
	const std::string& baseUrl() const { return _base_url; }

	bool regionAvailable(std::string_view region) const;

private:
	std::string _pathHostDir();
	void		_loadInfo();
	std::string _regionUrl() const;
};
