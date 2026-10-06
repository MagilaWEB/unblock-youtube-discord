#include "dns_host.h"

#include <algorithm>
#include <map>
#include <regex>
#include <sstream>

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

static std::string trimSpaces(std::string_view s)
{
	const size_t begin = s.find_first_not_of(" \t\r\n");
	if (begin == std::string_view::npos)
		return {};

	return std::string{ s.substr(begin, s.find_last_not_of(" \t\r\n") - begin + 1) };
}

std::optional<std::pair<std::string, std::string>> parseDnsPin(const std::string& line)
{
	static const std::string arrow{ "->" };

	const size_t pos = line.find(arrow);
	if (pos == std::string::npos)
		return std::nullopt;

	const std::string left	= trimSpaces(std::string_view{ line }.substr(0, pos));
	const std::string right = trimSpaces(std::string_view{ line }.substr(pos + arrow.size()));

	if (!std::regex_match(left, reg_domain_regex()))
		return std::nullopt;

	if (!std::regex_match(right, reg_ipv4_pattern()))
		return std::nullopt;

	return std::make_pair(left, right);
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

	if ((!_file_hosts_backup.isOpen() || _file_hosts_backup.empty()) && !_file_hosts.empty())
		for (auto& line : _file_hosts)
			_file_hosts_backup.writeText(line);

	_dir_dns_hosts = Core::get().configsPath() / "dns_hosts";

	_file_hosts.close();
	_file_hosts_backup.close();
	_file_hosts_user.close();
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
	// Explicit domain->IP pins from configs/dns_hosts/*.list. One domain may
	// carry several IPs (first occurrence order kept).
	std::map<std::string, std::vector<std::string>> domain_to_ips;
	std::error_code									ec;
	for (auto& entry : std::filesystem::directory_iterator(_dir_dns_hosts, ec))
	{
		if (!entry.is_regular_file())
			continue;

		File file{};
		file.open(entry.path(), "", true);

		for (auto& line : file)
		{
			const std::string trimmed = trimSpaces(line);
			if (trimmed.empty() || trimmed.starts_with('#'))
				continue;

			if (auto pin = parseDnsPin(trimmed))
			{
				auto& ips = domain_to_ips[pin->first];
				if (!std::ranges::contains(ips, pin->second))
					ips.push_back(pin->second);
			}
			else
				Debug::warning("Skipping invalid dns_hosts line [{}] in [{}]", trimmed, entry.path().string());
		}

		file.close();
	}

	_file_hosts_user.open();

	if (isHostsUser())
		_file_hosts_user.clear();

	for (auto& [domain, ips] : domain_to_ips)
		for (auto& ip : ips)
			_file_hosts_user.writeText(ip + " " + domain);

	_user_host_complete.store(true);
	_file_hosts_user.close();
}

bool DNSHost::isHostsUser() const
{
	return _user_host_complete.load();
}

std::string DNSHost::_pathHostDir()
{
	static constexpr char XOR_KEY{ 0x5A };

	return data_vec() | std::views::transform([](unsigned char code) { return static_cast<char>(code ^ XOR_KEY); }) | std::ranges::to<std::string>();
}
