#pragma once

class DNSHost final : public utils::DefaultInit
{
	std::filesystem::path _etc{ std::filesystem::temp_directory_path().root_path() / "Windows" / "System32" / "drivers" / "etc" };
	std::filesystem::path _host{};
	std::filesystem::path _host_backup{};
	std::filesystem::path _dir_dns_hosts{};

	Ptr<File> _file_hosts;
	Ptr<File> _file_hosts_backup;

	std::list<File> _list_dns_hosts_file{};
	std::list<std::string> _list_dns_hosts_file_name{};

	bool _enable{ false };

public:
	DNSHost();

	const std::list<std::string> & listDnsFileName();

	void enable();
	void disable();
};

