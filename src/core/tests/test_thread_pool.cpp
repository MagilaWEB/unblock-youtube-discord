#include <catch2/catch_test_macros.hpp>

#include "../pch.h"
#include "../thread_pool.h"

#include <atomic>
#include <chrono>

using namespace std::chrono_literals;

namespace
{
	bool wait_until(std::function<bool()> pred, std::chrono::milliseconds timeout = 5s)
	{
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		while (std::chrono::steady_clock::now() < deadline)
		{
			if (pred())
				return true;
			std::this_thread::sleep_for(5ms);
		}
		return pred();
	}
}	 // namespace

TEST_CASE("ThreadPool::defaultThreadCount is clamped to [2, 32]", "[thread_pool]")
{
	const auto n = ThreadPool::defaultThreadCount();
	CHECK(n >= ThreadPool::kMinWorkers);
	CHECK(n <= ThreadPool::kMaxWorkers);
}

TEST_CASE("ThreadPool runs all enqueued tasks", "[thread_pool]")
{
	ThreadPool pool;
	pool.start(4);

	std::atomic_int counter{ 0 };
	for (int i = 0; i < 100; ++i)
		pool.enqueue([&] { counter.fetch_add(1, std::memory_order_relaxed); });

	CHECK(wait_until([&] { return counter.load() == 100; }));
	CHECK(pool.isIdle());
	pool.stop();
}

TEST_CASE("ThreadPool executes tasks concurrently", "[thread_pool]")
{
	ThreadPool pool;
	pool.start(4);

	std::atomic_int current{ 0 };
	std::atomic_int observedMax{ 0 };
	std::atomic_int done{ 0 };

	for (int i = 0; i < 8; ++i)
	{
		pool.enqueue(
			[&]
			{
				const int c	   = current.fetch_add(1) + 1;
				int		  prev = observedMax.load();
				while (prev < c && !observedMax.compare_exchange_weak(prev, c))
				{
				}
				std::this_thread::sleep_for(100ms);
				current.fetch_sub(1);
				done.fetch_add(1);
			}
		);
	}

	CHECK(wait_until([&] { return done.load() == 8; }));
	CHECK(observedMax.load() > 1);
	pool.stop();
}

TEST_CASE("ThreadPool::onComplete fires after the specific task", "[thread_pool]")
{
	ThreadPool pool;
	pool.start(2);

	std::atomic_bool taskDone{ false };
	std::atomic_bool cbDone{ false };

	const auto id = pool.enqueue(
		[&]
		{
			std::this_thread::sleep_for(100ms);
			taskDone.store(true, std::memory_order_release);
		}
	);
	REQUIRE(id != 0);

	pool.onComplete(id, [&] { cbDone.store(true, std::memory_order_release); });

	// Callback must not run before its task.
	std::this_thread::sleep_for(20ms);
	CHECK(!cbDone.load(std::memory_order_acquire));

	CHECK(wait_until([&] { return cbDone.load(std::memory_order_acquire); }));
	CHECK(taskDone.load(std::memory_order_acquire));
	pool.stop();
}

TEST_CASE("ThreadPool::onComplete on already finished task still runs", "[thread_pool]")
{
	ThreadPool pool;
	pool.start(2);

	std::atomic_bool ran{ false };
	const auto		 id = pool.enqueue([] {});
	REQUIRE(id != 0);
	CHECK(wait_until([&] { return pool.isIdle(); }));

	pool.onComplete(id, [&] { ran.store(true, std::memory_order_release); });
	CHECK(wait_until([&] { return ran.load(); }));
	pool.stop();
}

TEST_CASE("ThreadPool::onDrain fires after all tasks", "[thread_pool]")
{
	ThreadPool pool;
	pool.start(4);

	std::atomic_int	 done{ 0 };
	std::atomic_bool drained{ false };

	for (int i = 0; i < 10; ++i)
		pool.enqueue(
			[&]
			{
				std::this_thread::sleep_for(20ms);
				done.fetch_add(1);
			}
		);

	pool.onDrain([&] { drained.store(true); });

	std::this_thread::sleep_for(20ms);
	CHECK(!drained.load());
	CHECK(wait_until([&] { return drained.load(); }));
	CHECK(done.load() == 10);
	pool.stop();
}

TEST_CASE("ThreadPool::onDrain on idle pool runs", "[thread_pool]")
{
	ThreadPool pool;
	pool.start(2);
	REQUIRE(pool.isIdle());

	std::atomic_bool drained{ false };
	pool.onDrain([&] { drained.store(true); });
	CHECK(wait_until([&] { return drained.load(); }));
	pool.stop();
}

TEST_CASE("ThreadPool::onComplete with unknown id is a no-op", "[thread_pool]")
{
	ThreadPool pool;
	pool.start(2);

	CHECK_NOTHROW(pool.onComplete(0, [] {}));
	CHECK_NOTHROW(pool.onComplete(0xDE'AD'BE'EF, [] {}));
	CHECK(pool.isIdle());
	pool.stop();
}

TEST_CASE("ThreadPool survives a throwing task and fires its completions", "[thread_pool]")
{
	ThreadPool pool;
	pool.start(2);

	std::atomic_bool afterOk{ false };
	std::atomic_bool cbOk{ false };

	const auto bad = pool.enqueue([] { throw std::runtime_error("boom"); });
	REQUIRE(bad != 0);
	pool.onComplete(bad, [&] { cbOk.store(true); });
	pool.enqueue([&] { afterOk.store(true); });

	CHECK(wait_until([&] { return afterOk.load() && cbOk.load(); }));
	CHECK(pool.isIdle());

	// Pool still usable after the throw.
	std::atomic_bool again{ false };
	pool.enqueue([&] { again.store(true); });
	CHECK(wait_until([&] { return again.load(); }));
	pool.stop();
}

TEST_CASE("ThreadPool::stop drains pending tasks", "[thread_pool]")
{
	ThreadPool pool;
	pool.start(2);

	std::atomic_int counter{ 0 };
	for (int i = 0; i < 20; ++i)
		pool.enqueue([&] { counter.fetch_add(1); });
	pool.stop();

	CHECK(counter.load() == 20);
}
