#include "dns_host.h"

#include <algorithm>
#include <map>
#include <regex>
#include <set>
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

	const size_t end = s.find_last_not_of(" \t\r\n");
	return std::string{ s.substr(begin, end - begin + 1) };
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

	if ((!_file_hosts_backup.isOpen()) || _file_hosts_backup.empty())
		if (!_file_hosts.empty())
			for (auto& line : _file_hosts)
				_file_hosts_backup.writeText(line);

	_dir_dns_hosts = Core::get().configsPath() / "dns_hosts";
	_geohide_cache = Core::get().configsPath() / "hosts_geohide.cache";
	_manual_hosts  = Core::get().configsPath() / "hosts";

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
	_last_progress.store(0.F);

	// Layer 1: explicit domain->IP pins from configs/dns_hosts/*.list.
	// One domain may carry several IPs (first occurrence order kept).
	std::map<std::string, std::vector<std::string>> domain_to_ips;
	std::error_code									ec;
	for (auto& entry : std::filesystem::directory_iterator(_dir_dns_hosts, ec))
	{
		if (_cancel_update.load(std::memory_order_relaxed))
			return;

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
				if (std::ranges::find(ips, pin->second) == ips.end())
					ips.push_back(pin->second);
			}
			else
				Debug::warning("Skipping invalid dns_hosts line [{}] in [{}]", trimmed, entry.path().string());
		}

		file.close();
	}

	if (_cancel_update.load(std::memory_order_relaxed))
		return;

	// Layer 2+3 source: fresh GeoHide bulk, cache fallback when offline.
	auto load = std::make_shared<HttpsLoad>(_regionUrl());
	{
		std::lock_guard lock{ _load_mutex };
		_active_load = load;
	}

	auto	  lines = load->run();
	const u32 code	= load->codeResult();

	_last_progress.store(load->progress());
	{
		std::lock_guard lock{ _load_mutex };
		_active_load.reset();
	}

	if (_cancel_update.load(std::memory_order_relaxed))
		return;

	std::vector<std::string> geohide;
	if (code == 200 && !lines.empty())
	{
		geohide = std::move(lines);

		File cache{};
		cache.open(_geohide_cache, "", true);
		cache.clear();
		for (auto& line : geohide)
			cache.writeText(line);
		cache.close();
	}
	else
	{
		Debug::warning("GeoHide download failed (code {}), using cache.", code);

		File cache{};
		cache.open(_geohide_cache, "", true);
		for (auto& line : cache)
			geohide.push_back(line);
		cache.close();
	}

	// Domains covered by upper layers are dropped from lower ones.
	std::set<std::string> covered;
	for (auto& [domain, ips] : domain_to_ips)
		covered.insert(domain);

	_file_hosts_user.open();

	if (isHostsUser())
		_file_hosts_user.clear();

	// Layer 1: pins, one line per pin.
	for (auto& [domain, ips] : domain_to_ips)
		for (auto& ip : ips)
			_file_hosts_user.writeText(ip + " " + domain);

	// Layer 2: manual hosts lines. Comments, blanks and non-IPv4 lines
	// (e.g. IPv6) pass through untouched.
	for (auto& line :
		 [&]() -> std::vector<std::string>
		 {
			 File manual{ false };
			 manual.open(_manual_hosts, "");

			 std::vector<std::string> out;
			 for (auto& l : manual)
				 out.push_back(l);
			 return out;
		 }())
	{
		const std::string trimmed = trimSpaces(line);
		if (trimmed.empty() || trimmed.starts_with('#'))
		{
			_file_hosts_user.writeText(line);
			continue;
		}

		std::stringstream ss{ trimmed };
		std::string		  first;
		ss >> first;

		if (!std::regex_match(first, reg_ipv4_pattern()))
		{
			_file_hosts_user.writeText(line);
			continue;
		}

		std::string kept, token;
		while (ss >> token)
			if (!covered.contains(token))
			{
				kept += (kept.empty() ? "" : " ");
				kept += token;
				covered.insert(token);
			}

		if (!kept.empty())
			_file_hosts_user.writeText(first + " " + kept);
	}

	// Layer 3: GeoHide bulk, same filtering as the manual layer.
	for (auto& line : geohide)
	{
		const std::string trimmed = trimSpaces(line);
		if (trimmed.empty() || trimmed.starts_with('#'))
		{
			_file_hosts_user.writeText(line);
			continue;
		}

		std::stringstream ss{ trimmed };
		std::string		  first;
		ss >> first;

		if (!std::regex_match(first, reg_ipv4_pattern()))
		{
			_file_hosts_user.writeText(line);
			continue;
		}

		std::string kept, token;
		while (ss >> token)
			if (!covered.contains(token))
			{
				kept += (kept.empty() ? "" : " ");
				kept += token;
				covered.insert(token);
			}

		if (!kept.empty())
			_file_hosts_user.writeText(first + " " + kept);
	}

	_user_host_complete.store(true);
	_file_hosts_user.close();
}

bool DNSHost::isHostsUser() const
{
	return _user_host_complete.load();
}

void DNSHost::cancel()
{
	_cancel_update.store(true);
}

float DNSHost::downloadProgress() const
{
	std::lock_guard lock{ _load_mutex };
	if (_active_load)
		return _active_load->progress();
	return _last_progress.load();
}

void DNSHost::setRegion(std::string_view region)
{
	_region = region.empty() ? "ru" : std::string{ region };
}

void DNSHost::setBaseUrl(std::string_view url)
{
	// Store only the host (no scheme), like in the telegram proxy. If a full
	// URL is passed in, strip the scheme and the trailing slash.
	std::string host{ url };

	if (host.starts_with("https://"))
		host.erase(0, std::string_view{ "https://" }.size());
	else if (host.starts_with("http://"))
		host.erase(0, std::string_view{ "http://" }.size());

	while (!host.empty() && host.back() == '/')
		host.pop_back();

	_base_url = host.empty() ? "geohide.ru" : host;
}

std::string DNSHost::_regionUrl() const
{
	if (_region.empty() || _region == "ru")
		return "https://" + _base_url + "/hosts";
	return "https://" + _base_url + "/" + _region + "/hosts";
}

bool DNSHost::regionAvailable(std::string_view region) const
{
	const std::string reg = region.empty() ? "ru" : std::string{ region };
	const std::string url = (reg == "ru") ? "https://" + _base_url + "/hosts" : "https://" + _base_url + "/" + reg + "/hosts";

	HttpsLoad load{ url };
	load.run();
	return load.codeResult() == 200;
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
	// Rebuilt on every call: the cache is written by update() and the
	// service-name list must reflect it without an app restart.
	_list_dns_hosts_file_name.clear();

	// No downloads here: service names come from the last cached GeoHide
	// bulk (empty on the very first run) and the dns_hosts file stems.
	std::error_code ec;
	for (auto& entry : std::filesystem::directory_iterator(_dir_dns_hosts, ec))
		if (entry.is_regular_file())
			_list_dns_hosts_file_name.push_back(entry.path().stem().string());

	// Service sections are "# <name>" comments. The file header and the
	// fallback blocks (the Cyrillic "Сервисные IP", "Резервные", ...) are
	// rejected by the ASCII check, so only real service names qualify.
	static const std::regex ipv4_regex{ R"(^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$)" };

	File cache{};
	cache.open(_geohide_cache, "", true);
	for (auto& line : cache)
	{
		if (!line.starts_with("# "))
			continue;

		const std::string name = line.substr(2);
		if (name.empty() || std::regex_match(name, ipv4_regex))
			continue;

		const bool has_non_ascii = std::ranges::any_of(name, [](unsigned char ch) { return ch >= 0x80; });
		if (has_non_ascii)
			continue;

		_list_dns_hosts_file_name.push_back(name);
	}
	cache.close();
}
