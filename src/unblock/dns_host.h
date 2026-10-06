#pragma once
#include <optional>
#include <string>

// Parse a "domain->IP" pin line (surrounding whitespace tolerated).
// Returns the {domain, ip} pair, or nullopt for anything else
// (plain domains, comments, blanks, garbage).
std::optional<std::pair<std::string, std::string>> parseDnsPin(const std::string& line);

const std::regex&						 reg_ipv4_pattern();
const std::regex&						 reg_domain_regex();
extern const std::vector<unsigned char>& data_vec();

// Owns the system hosts file: writes Unblock's own domain->IP pins
// (configs/dns_hosts/*.list) into it and restores the backup on disable.
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

	std::atomic_bool _user_host_complete{ false };
	bool			 _enable{ false };

public:
	DNSHost();

	void enable();
	void disable();

	// Rebuilds hosts_user from the pin lists in configs/dns_hosts.
	void update();

	bool isHostsUser() const;

private:
	std::string _pathHostDir();
};
