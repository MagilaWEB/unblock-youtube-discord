#include "dns_host.h"

DNSHost::DNSHost()
{
	_host		 = _etc / "hosts";
	_host_backup = _etc / "hosts_backup";

	_file_hosts->open(_host, "", true);
	_file_hosts_backup->open(_host_backup, "", true);

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

	for (auto& entry : std::filesystem::directory_iterator(_dir_dns_hosts))
	{
		auto& file = _list_dns_hosts_file.emplace_back();
		file.open(entry.path(), "", true);

		_list_dns_hosts_file_name.push_back(entry.path().stem().string());
	}
}

const std::list<std::string>& DNSHost::listDnsFileName()
{
	return _list_dns_hosts_file_name;
}

void DNSHost::enable()
{
	if (_enable)
		return;

	_enable = true;

	_file_hosts->clear();

	_file_hosts_backup->forLine(
		[this](std::string line)
		{
			_file_hosts->writeText(line);
			return false;
		}
	);

	for (auto& file : _list_dns_hosts_file)
	{
		file.forLine(
			[this](std::string line)
			{
				_file_hosts->writeText(line);
				return false;
			}
		);
	}

	_file_hosts->close();
}

void DNSHost::disable()
{
	_enable = false;

	_file_hosts->clear();

	_file_hosts_backup->forLine(
		[this](std::string line)
		{
			_file_hosts->writeText(line);
			return false;
		}
	);

	_file_hosts->close();
}