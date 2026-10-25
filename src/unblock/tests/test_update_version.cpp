#include <catch2/catch_test_macros.hpp>

#include "../update_version.h"

TEST_CASE("update version comes from the release redirect", "[update][version]")
{
	CHECK(parseUpdateVersionFromUrl("https://github.com/MagilaWEB/unblock-youtube-discord/releases/tag/v1.8.0") == "1.8.0");
}

TEST_CASE("update version ignores the redirect without a release", "[update][version]")
{
	CHECK_FALSE(parseUpdateVersionFromUrl("https://github.com/MagilaWEB/unblock-youtube-discord/releases").has_value());
	CHECK_FALSE(parseUpdateVersionFromUrl("").has_value());
}

TEST_CASE("update version survives the current release page markup", "[update][version]")
{
	// The release page links its tag like this, and the attribute the old parser
	// required (data-tab-item="i0code-tab") is not in the markup anymore.
	constexpr std::string_view line{
		R"(<a href="/MagilaWEB/unblock-youtube-discord/tree/v1.8.0" data-view-component="true" class="Link Link--muted">)"
	};

	CHECK(parseUpdateVersionFromHtml(line) == "1.8.0");
}

TEST_CASE("update version ignores html without a release tag", "[update][version]")
{
	CHECK_FALSE(parseUpdateVersionFromHtml(R"(<a href="/MagilaWEB/unblock-youtube-discord/tree/main">)").has_value());
	CHECK_FALSE(parseUpdateVersionFromHtml("no link here").has_value());
	CHECK_FALSE(parseUpdateVersionFromHtml("").has_value());
}
