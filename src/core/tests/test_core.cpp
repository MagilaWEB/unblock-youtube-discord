#include <catch2/catch_test_macros.hpp>
#include "../pch.h"
#include "../core.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>

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

TEST_CASE("Core::addTask runs in pool and taskComplete(id) fires", "[core][tasks]")
{
	CoreFixture fx;
	auto&		core = Core::get();
	core.parallel_run();

	std::atomic_bool taskDone{ false };
	std::atomic_bool cbDone{ false };
	std::atomic_bool drained{ false };

	const auto id = core.addTask(
		[&]
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			taskDone.store(true, std::memory_order_release);
		}
	);
	CHECK(id != 0);

	core.taskComplete(id, [&] { cbDone.store(true, std::memory_order_release); });
	core.taskComplete([&] { drained.store(true, std::memory_order_release); });

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while ((!cbDone.load(std::memory_order_acquire) || !drained.load(std::memory_order_acquire)) && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));

	CHECK(taskDone.load());
	CHECK(cbDone.load());
	CHECK(drained.load());
}
