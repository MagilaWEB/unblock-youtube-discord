#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>

// -----------------------------------------------------------------------
// Scheduler — manager of deferred tasks. Replaces the
// "Core::addTaskParallel + sleep_for" combo: one background worker,
// a task arrives on time instead of occupying a pool thread with a sleep.
//
// Units — any chrono::duration: milliseconds, seconds, minutes...
//   Scheduler::get().after(std::chrono::milliseconds(450), [] { ... });
//   Scheduler::get().after(std::chrono::seconds(2), [] { ... });
//   Scheduler::get().after(std::chrono::minutes(5), [] { ... });
//
// Cancellation — by token (cancel) or by generation from outside: the
// callback checks whether it is still current (see Tutorial::_gen). A
// running callback cannot be stopped by cancel — that is what the stale
// flag is for.
//
// Threads: after()/cancel() are callable from anywhere; callbacks run on
// the scheduler worker — touch the UI only via the thread-safe bridge
// (dom::execute / evaluate + await), same as from Core tasks.
// -----------------------------------------------------------------------

class Scheduler final
{
public:
	using Token = u64;

	static Scheduler& get();

	/// Schedule fn after delay. Returns a token for cancel().
	/// @example Scheduler::get().after(std::chrono::milliseconds(900), [] { ... });
	template<typename Rep, typename Period>
	Token after(std::chrono::duration<Rep, Period> delay, std::function<void()> fn)
	{
		return enqueue(std::chrono::steady_clock::now() + delay, std::move(fn));
	}

	/// Cancel a scheduled (not yet run) task. No-op for foreign/stale tokens
	/// and for an already running task.
	void cancel(Token token);

private:
	Scheduler();
	~Scheduler();
	Scheduler(const Scheduler&)			   = delete;
	Scheduler& operator=(const Scheduler&) = delete;

	struct Task
	{
		std::chrono::steady_clock::time_point at;
		Token								  token;
		std::function<void()>				  fn;
	};

	struct LaterFirst
	{
		bool operator()(const Task& a, const Task& b) const { return a.at > b.at; }
	};

	Token enqueue(std::chrono::steady_clock::time_point at, std::function<void()> fn);
	void  loop();

	std::mutex												 _mutex;
	std::condition_variable									 _cv;
	std::priority_queue<Task, std::vector<Task>, LaterFirst> _queue;
	std::unordered_set<Token>								 _cancelled;
	std::atomic<Token>										 _next{ 1 };
	std::atomic_bool										 _stop{ false };
	std::thread												 _worker;
};
