#include "dns_util.h"

#include <windows.h>

#include <fstream>
#include <sstream>

namespace dns
{
	std::string readFile(const std::filesystem::path& path, std::string& error)
	{
		std::ifstream in{ path, std::ios::binary };
		if (!in)
		{
			error = "Couldn't open file: " + path.string();
			return {};
		}

		std::ostringstream ss;
		ss << in.rdbuf();
		return ss.str();
	}

	bool writeFileAtomic(const std::filesystem::path& path, const std::string& content)
	{
		const auto tmp = path.string() + ".tmp";
		{
			std::ofstream out{ tmp, std::ios::binary | std::ios::trunc };
			if (!out)
				return false;
			out << content;
		}

		std::error_code ec;
		std::filesystem::rename(tmp, path, ec);
		return !ec;
	}

	std::filesystem::path exeDir()
	{
		wchar_t buf[MAX_PATH]{};
		GetModuleFileNameW(nullptr, buf, MAX_PATH);
		return std::filesystem::path{ buf }.parent_path();
	}
}	 // namespace dns
