#include <catch2/catch_test_macros.hpp>

#include "../pch.h"

using namespace std::chrono_literals;

TEST_CASE("Scheduler::after fires in milliseconds", "[scheduler]")
{
	std::atomic_bool fired{ false };
	Scheduler::get().after(20ms, [&] { fired.store(true, std::memory_order_release); });

	const auto deadline = std::chrono::steady_clock::now() + 2s;
	while (!fired.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(5ms);

	CHECK(fired.load());
}

TEST_CASE("Scheduler::after speaks seconds too", "[scheduler]")
{
	std::atomic_bool fired{ false };
	Scheduler::get().after(0s, [&] { fired.store(true, std::memory_order_release); });

	const auto deadline = std::chrono::steady_clock::now() + 2s;
	while (!fired.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(5ms);

	CHECK(fired.load());
}

TEST_CASE("Scheduler::cancel drops the task", "[scheduler]")
{
	std::atomic_bool fired{ false };
	const auto		 token = Scheduler::get().after(150ms, [&] { fired.store(true, std::memory_order_release); });
	Scheduler::get().cancel(token);

	std::this_thread::sleep_for(400ms);
	CHECK(!fired.load());
}

TEST_CASE("Scheduler::cancel of garbage token is a no-op", "[scheduler]")
{
	CHECK_NOTHROW(Scheduler::get().cancel(0));
	CHECK_NOTHROW(Scheduler::get().cancel(0xDE'AD'BE'EF));
}
