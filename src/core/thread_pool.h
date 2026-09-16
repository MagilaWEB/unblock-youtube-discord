#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// -----------------------------------------------------------------------
// ThreadPool — fixed-size pool of workers that drain queued tasks.
// Workers sleep on a condition variable when the queue is empty and wake
// only when new work arrives (no polling).
//
// Completion hooks:
//   onComplete(id, cb) — run cb after the specific task id finishes. If the
//     task already finished, cb is enqueued immediately. Unknown/future ids
//     are ignored with a warning.
//   onDrain(cb) — run cb once the pool becomes idle (queue empty and no
//     active tasks); if already idle, cb is enqueued immediately.
//
// Every callback is itself enqueued as a new task (gets its own id), so
// callbacks may safely call enqueue/onComplete/onDrain without deadlock.
// A task that throws does not kill its worker; the error is logged and the
// task still counts as finished so its completions fire.
// -----------------------------------------------------------------------

class ThreadPool final
{
public:
	using TaskId = std::uint64_t;
	using Count	 = std::uint32_t;

	static constexpr Count kMinWorkers = 2;
	static constexpr Count kMaxWorkers = 32;

	/// Resolve worker count: hardware_concurrency clamped to [kMinWorkers, kMaxWorkers].
	/// Falls back to 4 when hardware_concurrency() reports 0.
	static Count defaultThreadCount();

	ThreadPool() = default;
	~ThreadPool() { stop(); }

	ThreadPool(const ThreadPool&)			 = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	/// Start workers. threadCount == 0 means auto (defaultThreadCount()).
	/// Idempotent: does nothing if already started.
	void start(Count threadCount = 0);

	/// Drain pending tasks, join workers. Idempotent, safe to call twice.
	/// After stop() the pool may be started again.
	void stop();

	/// Enqueue fn for execution by the pool. Returns task id (0 on empty fn).
	TaskId enqueue(std::function<void()>&& fn);

	/// Run cb after task id finishes (see class comment).
	void onComplete(TaskId id, std::function<void()>&& cb);

	/// Run cb once the pool is idle (see class comment).
	void onDrain(std::function<void()>&& cb);

	[[nodiscard]] bool	 isIdle() const;
	[[nodiscard]] size_t threadCount() const;

private:
	struct Task
	{
		TaskId				  id{ 0 };
		std::function<void()> fn;
	};

	void _workerRoutine();
	void _runTask(Task task);

	mutable std::mutex											   _mutex;
	std::condition_variable										   _cv;
	std::deque<Task>											   _queue;
	std::unordered_set<TaskId>									   _pendingIds;
	std::unordered_set<TaskId>									   _active;
	std::unordered_map<TaskId, std::vector<std::function<void()>>> _waitersForId;
	std::vector<std::function<void()>>							   _waitersForAll;
	std::vector<std::thread>									   _workers;
	std::atomic<TaskId>											   _nextId{ 1 };
	std::atomic_bool											   _stop{ false };
};
