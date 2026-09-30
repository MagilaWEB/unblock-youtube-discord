#pragma once

#include <filesystem>
#include <string>

namespace dns
{
	// Reads a whole file as binary. On failure returns {} and sets error.
	std::string readFile(const std::filesystem::path& path, std::string& error);

	// Writes via a .tmp sibling and renames over the target.
	bool writeFileAtomic(const std::filesystem::path& path, const std::string& content);

	// Directory of the running executable (where AdguardDns64.dll lives).
	std::filesystem::path exeDir();
}	 // namespace dns
