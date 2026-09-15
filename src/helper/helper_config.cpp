#include "helper_config.h"

#include <algorithm>
#include <cctype>
#include <fstream>

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#endif

namespace
{
	void trim(std::string& s)
	{
		const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
		s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
	}

	u32 parseClamped(std::string_view text, u32 fallback, u32 lo, u32 hi)
	{
		if (text.empty())
			return fallback;
		try
		{
			unsigned long v = std::stoul(std::string{ text });
			return std::clamp(static_cast<u32>(v), lo, hi);
		}
		catch (...)
		{
			return fallback;
		}
	}

	void applyPair(HelperConfig& cfg, std::string_view key, std::string_view value)
	{
		if (key == "pool_size")
			cfg.pool_size = parseClamped(value, cfg.pool_size, 1, 64);
		else if (key == "check_timeout_sec" || key == "check_timeout")
			cfg.check_timeout_sec = parseClamped(value, cfg.check_timeout_sec, 1, 60);
		else if (key == "connect_timeout_sec" || key == "connect_timeout")
			cfg.connect_timeout_sec = parseClamped(value, cfg.connect_timeout_sec, 1, 30);
		else if (key == "max_redirects")
			cfg.max_redirects = parseClamped(value, cfg.max_redirects, 0, 10);
		else if (key == "recheck_interval_min" || key == "recheck_min")
			cfg.recheck_interval_min = parseClamped(value, cfg.recheck_interval_min, 5, 180);
		else if (key == "errors_progress_min" || key == "errors_progress_recheck_min")
			cfg.errors_progress_min = parseClamped(value, cfg.errors_progress_min, 1, 30);
		else if (key == "errors_recheck_sec")
			cfg.errors_recheck_sec = parseClamped(value, cfg.errors_recheck_sec, 5, 300);
		// Unknown keys are ignored: forward compatibility.
	}

	void parseLines(std::istream& in, HelperConfig& cfg)
	{
		bool		in_helper = false;
		std::string line;
		while (std::getline(in, line))
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#')
				continue;
			if (line.front() == '[')
			{
				const auto	close = line.find(']');
				std::string name  = (close == std::string::npos) ? line : line.substr(1, close - 1);
				trim(name);
				in_helper = (name == "HELPER");
				continue;
			}
			if (!in_helper)
				continue;
			const auto eq = line.find('=');
			if (eq == std::string::npos)
				continue;
			std::string key = line.substr(0, eq);
			std::string val = line.substr(eq + 1);
			trim(key);
			trim(val);
			applyPair(cfg, key, val);
		}
	}
}	 // namespace

void HelperConfig::normalize()
{
	pool_size			 = std::clamp(pool_size, 1u, 64u);
	check_timeout_sec	 = std::clamp(check_timeout_sec, 1u, 60u);
	connect_timeout_sec	 = std::clamp(connect_timeout_sec, 1u, 30u);
	max_redirects		 = std::clamp(max_redirects, 0u, 10u);
	recheck_interval_min = std::clamp(recheck_interval_min, 5u, 180u);
	errors_progress_min	 = std::clamp(errors_progress_min, 1u, 30u);
	errors_recheck_sec	 = std::clamp(errors_recheck_sec, 5u, 300u);
}

std::filesystem::path HelperConfig::resolvePath()
{
#ifdef _WIN32
	wchar_t		buf[MAX_PATH]{};
	const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
	if (n > 0)
	{
		std::filesystem::path exe{
			std::wstring{ buf, n }
		};
		// exe lives in bin/ -> config lives in <root>/user/setting.config
		return (exe.parent_path().parent_path() / "user" / "setting.config");
	}
#endif
	return std::filesystem::current_path() / "user" / "setting.config";
}

HelperConfig HelperConfig::load()
{
	return loadFrom(resolvePath());
}

HelperConfig HelperConfig::loadFrom(const std::filesystem::path& path)
{
	HelperConfig	cfg = defaults();
	std::error_code ec;
	if (!std::filesystem::exists(path, ec))
		return cfg;

	std::ifstream in(path);
	if (!in.is_open())
		return cfg;

	parseLines(in, cfg);
	cfg.normalize();
	return cfg;
}

HelperConfig HelperConfig::parsePayload(std::string_view payload, const HelperConfig& base)
{
	HelperConfig cfg = base;
	std::string	 key, val;
	std::string	 cur{ payload };
	size_t		 pos = 0;
	while (pos <= cur.size())
	{
		const auto	semi = cur.find(';', pos);
		std::string pair = (semi == std::string::npos) ? cur.substr(pos) : cur.substr(pos, semi - pos);
		trim(pair);
		if (!pair.empty())
		{
			const auto eq = pair.find('=');
			if (eq != std::string::npos)
			{
				key = pair.substr(0, eq);
				val = pair.substr(eq + 1);
				trim(key);
				trim(val);
				applyPair(cfg, key, val);
			}
		}
		if (semi == std::string::npos)
			break;
		pos = semi + 1;
	}
	cfg.normalize();
	return cfg;
}

std::string HelperConfig::makeMessage() const
{
	char buf[256]{};
	snprintf(
		buf,
		sizeof(buf),
		"CONFIG:pool_size=%u;check_timeout_sec=%u;connect_timeout_sec=%u;max_redirects=%u;recheck_interval_min=%u;errors_progress_min=%u;errors_"
		"recheck_sec=%u",
		pool_size,
		check_timeout_sec,
		connect_timeout_sec,
		max_redirects,
		recheck_interval_min,
		errors_progress_min,
		errors_recheck_sec
	);
	return std::string{ buf };
}
