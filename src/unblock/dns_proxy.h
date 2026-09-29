#pragma once

#include "../core/service.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

/** Unblock DNS proxy (unblock_dns.exe): config file, service lifecycle,
 *  status counters over IPC and adapter repair on an unclean stop. */
class DnsProxy final
{
public:
	static std::vector<std::string> defaultUpstreams();
	static std::vector<std::string> defaultBootstrap();
	static uint32_t					defaultTimeout();

	DnsProxy();

	void run(bool state);
	bool isRun();

	void							setUpstreams(std::vector<std::string> upstreams);
	const std::vector<std::string>& upstreams() const;

	void							setBootstrap(std::vector<std::string> bootstrap);
	const std::vector<std::string>& bootstrap() const;

	void	 setTimeout(uint32_t timeout_ms);
	uint32_t timeout() const;

	std::string status() const;

	bool testUpstream(const std::string& value, std::string& output);

	void repairBoot();

private:
	std::filesystem::path _configPath() const;
	std::filesystem::path _backupPath() const;
	std::filesystem::path _logPath() const;
	void				  _writeConfig(const std::vector<std::string>& upstreams, const std::vector<std::string>& bootstrap, uint32_t timeout_ms);

	Service _service{ "unblock_dns", "SvcHost.exe" };

	std::vector<std::string> _upstreams;
	std::vector<std::string> _bootstrap;
	uint32_t				 _timeout_ms{ 0 };
};
