#include "dns_host.h"
#include <concurrent_vector.h>
#include <curl/curl.h>

const std::regex& reg_ipv4_pattern()
{
	static const std::regex re{ R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)" };
	return re;
}
const std::regex& reg_domain_regex()
{
	static const std::regex re{ R"(^([a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}$)" };
	return re;
}

const std::vector<unsigned char>& data_vec()
{
	static const std::vector<unsigned char> d{ 0x0d, 0x33, 0x34, 0x3e, 0x35, 0x2d, 0x29, 0x75, 0x09, 0x23, 0x29, 0x2e, 0x3f, 0x37,
											   0x69, 0x68, 0x75, 0x3e, 0x28, 0x33, 0x2c, 0x3f, 0x28, 0x29, 0x75, 0x3f, 0x2e, 0x39 };
	return d;
}

DNSHost::DNSHost()
{
	_etc = std::filesystem::temp_directory_path().root_path();

	std::string		  component;
	std::stringstream ss{ _pathHostDir() };

	while (std::getline(ss, component, '/'))
		if (!component.empty())
			_etc = _etc / component;

	_host		 = _etc / "hosts";
	_host_backup = _etc / "hosts_backup";
	_host_user	 = _etc / "hosts_user";

	_file_hosts.open(_host, "", true);
	_file_hosts_backup.open(_host_backup, "", true);
	_file_hosts_user.open(_host_user, ".list", true);

	_user_host_complete.store(!_file_hosts_user.empty());

	if ((!_file_hosts_backup.isOpen()) || _file_hosts_backup.empty())
		if (!_file_hosts.empty())
			for (auto& line : _file_hosts)
				_file_hosts_backup.writeText(line);

	_dir_dns_hosts = Core::get().configsPath() / "dns_hosts";

	_file_hosts.close();
	_file_hosts_backup.close();
	_file_hosts_user.close();
}

const std::list<std::string>& DNSHost::listDnsFileName()
{
	_loadInfo();
	return _list_dns_hosts_file_name;
}

void DNSHost::enable()
{
	if (_enable || !isHostsUser())
		return;

	_enable = true;

	_file_hosts.open();
	_file_hosts.clear();

	_file_hosts_backup.open();
	for (auto& line : _file_hosts_backup)
		_file_hosts.writeText(line);

	_file_hosts_user.open();
	for (auto& line : _file_hosts_user)
		_file_hosts.writeText(line);

	_file_hosts.close();
	_file_hosts_backup.close();
	_file_hosts_user.close();
}

void DNSHost::disable()
{
	if (!isHostsUser())
		return;

	_enable = false;
	_file_hosts.open();
	_file_hosts.clear();

	_file_hosts_backup.open();
	for (auto& line : _file_hosts_backup)
		_file_hosts.writeText(line);

	_file_hosts.close();
	_file_hosts_backup.close();
}

void DNSHost::update()
{
	_cancel_update.store(false);
	_map_list.clear();

	_list_hosts.clear();

	for (auto& entry : std::filesystem::directory_iterator(_dir_dns_hosts))
	{
		File file{};
		file.open(entry.path(), "", true);

		for (auto& line : file)
			if (!line.empty())
				if (std::ranges::find(_list_hosts, line) == _list_hosts.end())
					_list_hosts.push_back(line);
	}

	if (_cancel_update.load(std::memory_order_relaxed))
		return;

	for (auto& domain : _list_hosts)
		_map_list[domain].reserve(0);

	std::for_each(
		std::execution::par,
		_list_hosts.begin(),
		_list_hosts.end(),
		[this](const std::string& domain)
		{
			if (_cancel_update.load(std::memory_order_relaxed))
				return;

			static const std::regex reg_equally{ R"(->)" };
			std::smatch				para;
			if (std::regex_search(domain, para, reg_equally))
			{
				const std::string value = para.suffix().str();
				if (!value.empty())
				{
					const std::string key = para.prefix().str();
					{
						FAST_LOCK(_map_list_lock);
						_map_list[key] = value;
					}

					_size_iter++;
				}

				return;
			}

			_map_list_lock.EnterShared();
			if (!_map_list[domain].empty())
			{
				_map_list_lock.LeaveShared();
				return;
			}
			_map_list_lock.LeaveShared();

			_writeDomain(domain);

			_size_iter++;
		}
	);

	if (!_cancel_update.load(std::memory_order_relaxed))
	{
		_file_hosts_user.open();

		if (isHostsUser())
			_file_hosts_user.clear();

		_loadInfo();

		File local_hosts{ false };
		local_hosts.open(Core::get().configsPath() / "hosts", "");
		for (auto& line : local_hosts)
		{
			static const std::regex ip_domain_regex{ R"(^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})\s+([^\s]+))" };
			std::smatch				para;
			if (std::regex_search(line, para, ip_domain_regex))
			{
				const std::string domain = para[2].str();
				if ((!domain.empty()) && (!_map_list[domain].empty()))
					continue;

				_file_hosts_user.writeText(line);
			}
		}

		std::unordered_map<std::string, std::vector<std::string>> ip_to_domains;
		ip_to_domains.reserve(_map_list.size());

		for (const auto& [domain, ip] : _map_list)
			if (!ip.empty())
				ip_to_domains[ip].push_back(domain);

		for (const auto& [ip, domains] : ip_to_domains)
		{
			std::string line;
			line.reserve(ip.size() + 1 + domains.size() * 20);
			line += ip;

			for (const auto& d : domains)
			{
				line += ' ';
				line += d;
			}

			_file_hosts_user.writeText(line);
		}

		_user_host_complete.store(true);
		_file_hosts_user.close();
	}

	_size_iter.store(0);
}

bool DNSHost::isHostsUser() const
{
	return _user_host_complete.load();
}

void DNSHost::cancel()
{
	_cancel_update.store(true);
}

float DNSHost::percentageCompletion() const
{
	return (static_cast<float>(_size_iter.load()) / static_cast<float>(_list_hosts.size())) * 100.F;
}

std::string DNSHost::_pathHostDir()
{
	static constexpr char XOR_KEY{ 0x5A };

	std::string result;

	std::transform(
		data_vec().begin(),
		data_vec().end(),
		std::back_inserter(result),
		[](unsigned char code) { return static_cast<char>(code ^ XOR_KEY); }
	);

	return result;
}

void DNSHost::_loadInfo()
{
	static bool load{ false };
	if (!load)
	{
		load = true;

		for (auto& entry : std::filesystem::directory_iterator(_dir_dns_hosts))
			_list_dns_hosts_file_name.push_back(entry.path().stem().string());

		File local_hosts{ false };
		local_hosts.open(Core::get().configsPath() / "hosts", "");

		HttpsLoad hosts{ "https://raw.githubusercontent.com/Internet-Helper/GeoHideDNS/refs/heads/main/hosts/hosts" };
		auto	  lines = hosts.run();
		if (hosts.codeResult() != 200 || lines.empty())
		{
			for (auto& line : local_hosts)
				lines.push_back(line);
		}
		else
		{
			local_hosts.clear();
			for (auto& line : lines)
				local_hosts.writeText(line);
		}

		const std::string start_line{ "# AMD" };
		const std::string end_line{ "# Xerox" };
		bool			  run_service{ false };
		for (auto& line : lines)
		{
			if ((!run_service) && line == start_line)
			{
				run_service = true;
				_list_dns_hosts_file_name.push_back(line.substr(2, line.length()));
			}
			else if (run_service && line.contains("# "))
			{
				_list_dns_hosts_file_name.push_back(line.substr(2, line.length()));
				if (line == end_line)
					run_service = false;
			}
		}
	}
}

std::optional<DNSHost::Google::MapDomainIP> DNSHost::_getIPGoogle(std::string domain)
{
	if (_cancel_update.load())
		return {};

	Google dns_google{ domain };
	dns_google.run();

	auto& content = dns_google.content();
	if (content.empty())
		return {};

	return content;
}

void DNSHost::_writeDomain(std::string domain)
{
	auto map_result = _getIPGoogle(domain);
	if (!map_result)
		return;

	auto& map_domain = map_result.value();
	for (auto& [original_domain, ip_list] : map_domain)
	{
		for (auto& ip_or_domain : ip_list)
		{
			_map_list_lock.EnterShared();
			if (_map_list.contains(original_domain))
			{
				_map_list_lock.LeaveShared();

				// IP
				if (std::regex_match(ip_or_domain, reg_ipv4_pattern()))
				{
					{
						FAST_LOCK(_map_list_lock);
						_map_list[original_domain] = ip_or_domain;
					}

					continue;
				}
			}

			_map_list_lock.EnterShared();

			if (_map_list.contains(ip_or_domain) && !_map_list[ip_or_domain].empty())
			{
				const std::string resolved_ip = _map_list[ip_or_domain];
				_map_list_lock.LeaveShared();
				{
					FAST_LOCK(_map_list_lock);
					_map_list[original_domain] = resolved_ip;
				}
				continue;
			}

			_map_list_lock.LeaveShared();

			// DOMAIN
			{
				FAST_LOCK(_map_list_lock);
				_map_list[ip_or_domain].reserve(0);
			}

			_writeDomain(ip_or_domain);

			{
				FAST_LOCK(_map_list_lock);
				_map_list[original_domain] = _map_list[ip_or_domain];
			}
		}
	}
}
