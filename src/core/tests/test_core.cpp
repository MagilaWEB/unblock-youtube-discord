#include <catch2/catch_test_macros.hpp>
#include "../pch.h"
#include "../core.h"

#include <fstream>

namespace fs = std::filesystem;

struct CoreFixture
{
	fs::path root;

	CoreFixture()
	{
		root = fs::temp_directory_path() / "unblock_core_test";
		fs::remove_all(root);
		fs::create_directories(root / "bin");
		fs::create_directories(root / "binaries");
		fs::create_directories(root / "configs");
		fs::current_path(root);

		static bool initialized = false;
		if (!initialized)
		{
			Core::get();
			initialized = true;
		}
	}

	~CoreFixture()
	{
		fs::current_path(fs::temp_directory_path());
		fs::remove_all(root);
	}
};

TEST_CASE("Core::isVersionNewer newer major", "[core][version]")
{
	CoreFixture fx;
	CHECK(Core::get().isVersionNewer("2.0.0", "1.0.0"));
}

TEST_CASE("Core::isVersionNewer older major", "[core][version]")
{
	CoreFixture fx;
	CHECK_FALSE(Core::get().isVersionNewer("1.0.0", "2.0.0"));
}

TEST_CASE("Core::isVersionNewer equal versions", "[core][version]")
{
	CoreFixture fx;
	CHECK_FALSE(Core::get().isVersionNewer("1.0.0", "1.0.0"));
}

TEST_CASE("Core::isVersionNewer newer minor", "[core][version]")
{
	CoreFixture fx;
	CHECK(Core::get().isVersionNewer("1.1.0", "1.0.0"));
}

TEST_CASE("Core::isVersionNewer older minor", "[core][version]")
{
	CoreFixture fx;
	CHECK_FALSE(Core::get().isVersionNewer("1.0.0", "1.1.0"));
}

TEST_CASE("Core::isVersionNewer newer patch", "[core][version]")
{
	CoreFixture fx;
	CHECK(Core::get().isVersionNewer("1.0.1", "1.0.0"));
}

TEST_CASE("Core::isVersionNewer older patch", "[core][version]")
{
	CoreFixture fx;
	CHECK_FALSE(Core::get().isVersionNewer("1.0.0", "1.0.1"));
}

TEST_CASE("Core::isVersionNewer no patch defaults to 0", "[core][version]")
{
	CoreFixture fx;
	CHECK(Core::get().isVersionNewer("2.0", "1.0"));
	CHECK_FALSE(Core::get().isVersionNewer("1.0", "2.0"));
}

TEST_CASE("Core::isVersionNewer same major minor no patch", "[core][version]")
{
	CoreFixture fx;
	CHECK_FALSE(Core::get().isVersionNewer("1.0", "1.0"));
}
