module unblock.update_version;

import std;

namespace
{
	std::optional<std::string> _versionAfterMask(std::string_view text, std::string_view mask, std::string_view stops)
	{
		const auto pos = text.find(mask);
		if (pos == std::string_view::npos)
			return {};

		auto version = text.substr(pos + mask.size());

		if (const auto end = version.find_first_of(stops); end != std::string_view::npos)
			version = version.substr(0, end);

		if (version.empty())
			return {};

		return std::string{ version };
	}
}

std::optional<std::string> parseUpdateVersionFromUrl(std::string_view url)
{
	return _versionAfterMask(url, "/releases/tag/v", "?#/");
}

std::optional<std::string> parseUpdateVersionFromHtml(std::string_view line)
{
	return _versionAfterMask(line, "/MagilaWEB/unblock-youtube-discord/tree/v", "\"");
}
