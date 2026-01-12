#include "dns_host.h"
#include <concurrent_vector.h>
#include <curl/curl.h>

DNSHost::DNSHost()
{
	_host		 = _etc / "hosts";
	_host_backup = _etc / "hosts_backup";
	_host_user	 = _etc / "hosts_user";

	_file_hosts->open(_host, "", true);
	_file_hosts_backup->open(_host_backup, "", true);
	_file_hosts_user->open(_host_user, ".list", true);

	if (!_file_hosts_backup->isOpen())
	{
		_file_hosts->forLine(
			[this](std::string str)
			{
				_file_hosts_backup->writeText(str);
				return false;
			}
		);

		_file_hosts_backup->close();
	}

	_dir_dns_hosts = Core::get().configsPath() / "dns_hosts";
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

	_loadInfo();

	_enable = true;

	_file_hosts->clear();

	_file_hosts_backup->forLine(
		[this](std::string line)
		{
			_file_hosts->writeText(line);
			return false;
		}
	);

	_file_hosts_user->forLine(
		[this](std::string line)
		{
			_file_hosts->writeText(line);
			return false;
		}
	);

	_file_hosts->save();
}

void DNSHost::disable()
{
	if (!isHostsUser())
		return;

	_enable = false;

	_file_hosts->clear();

	_file_hosts_backup->forLine(
		[this](std::string line)
		{
			_file_hosts->writeText(line);
			return false;
		}
	);

	_file_hosts->save();
}

void DNSHost::update()
{
	_cancel_update.store(false);
	_map_list.clear();

	_list_domains.clear();

	for (auto& entry : std::filesystem::directory_iterator(_dir_dns_hosts))
	{
		File file{};
		file.open(entry.path(), "", true);

		file.forLine(
			[this](std::string domain)
			{
				if (!domain.empty())
					if (std::find(_list_domains.begin(), _list_domains.end(), domain) == _list_domains.end())
						_list_domains.push_back(domain);

				return false;
			}
		);
	}

	if (_cancel_update.load())
		return;

	std::for_each(
		std::execution::par,
		_list_domains.begin(),
		_list_domains.end(),
		[this](std::string domain)
		{
			if (_cancel_update.load())
				return;

			if (!_map_list[domain].empty())
				return;

			_writeDomain(domain);

			_size_iter++;
		}
	);

	if (!_cancel_update.load())
	{
		if (isHostsUser())
			_file_hosts_user->clear();

		auto lines = HttpsLoad{ "https://raw.githubusercontent.com/Internet-Helper/GeoHideDNS/refs/heads/main/hosts/hosts" }.run();
		for (auto& line : lines)
			_file_hosts_user->writeText(line);

		for (auto& [domain, ip_list] : _map_list)
			for (auto& ip : ip_list)
				_file_hosts_user->writeText(ip + " " + domain);

		_file_hosts_user->save();
	}

	_size_iter.store(0);
}

bool DNSHost::isHostsUser() const
{
	return _file_hosts_user->isOpen();
}

void DNSHost::cancel()
{
	_cancel_update.store(true);
}

float DNSHost::percentageCompletion() const
{
	return (static_cast<float>(_size_iter.load()) / static_cast<float>(_list_domains.size())) * 100.f;
}

void DNSHost::_loadInfo()
{
	static bool load{ false };
	if (!load)
	{
		load = true;

		for (auto& entry : std::filesystem::directory_iterator(_dir_dns_hosts))
			_list_dns_hosts_file_name.push_back(entry.path().stem().string());

		static const std::string start_line{ "# AMD" };
		static const std::string end_line{ "# Xerox" };
		bool					 run_service{ false };

		auto lines = HttpsLoad{ "https://raw.githubusercontent.com/Internet-Helper/GeoHideDNS/refs/heads/main/hosts/hosts" }.run();
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
				if (run_service && line == end_line)
					run_service = false;
			}

			_file_hosts_user->writeText(line);
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
	for (auto& [key, ip_list] : map_domain)
	{
		for (auto& ip : ip_list)
			if (std::regex_match(ip, reg_ipv4_pattern))
			{
				auto& list = _map_list[key];
				if (std::find(list.begin(), list.end(), ip) == list.end())
				{
					CRITICAL_SECTION_RAII(_lock);
					_map_list[key].emplace_back(ip);
				}
			}
			else if (_map_list[ip].empty())
				if (_map_list[ip].empty())
					_writeDomain(ip);
	}
}
