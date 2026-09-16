#include "pch.h"
#include "thread_pool.h"
#include "debug.h"

#include <algorithm>

ThreadPool::Count ThreadPool::defaultThreadCount()
{
	unsigned hw = std::thread::hardware_concurrency();
	if (hw == 0)
		hw = 4;
	return std::clamp<Count>(static_cast<Count>(hw), kMinWorkers, kMaxWorkers);
}

void ThreadPool::start(Count threadCount)
{
	std::lock_guard lk{ _mutex };
	if (!_workers.empty())
		return;

	if (threadCount == 0)
		threadCount = defaultThreadCount();
	threadCount = std::clamp(threadCount, kMinWorkers, kMaxWorkers);

	_stop.store(false, std::memory_order_release);
	_workers.reserve(threadCount);
	for (Count i = 0; i < threadCount; ++i)
		_workers.emplace_back([this] { _workerRoutine(); });
}

void ThreadPool::stop()
{
	std::vector<std::thread> workers;
	{
		std::lock_guard lk{ _mutex };
		if (_workers.empty())
			return;
		_stop.store(true, std::memory_order_release);
		workers = std::move(_workers);
	}
	_cv.notify_all();
	for (auto& w : workers)
		if (w.joinable())
			w.join();
}

ThreadPool::TaskId ThreadPool::enqueue(std::function<void()>&& fn)
{
	if (!fn)
		return 0;

	const TaskId id = _nextId.fetch_add(1, std::memory_order_relaxed);
	{
		std::lock_guard lk{ _mutex };
		_queue.emplace_back(id, std::move(fn));
		_pendingIds.insert(id);
	}
	_cv.notify_one();
	return id;
}

void ThreadPool::onComplete(TaskId id, std::function<void()>&& cb)
{
	if (id == 0 || !cb)
		return;

	bool runNow = false;
	{
		std::lock_guard lk{ _mutex };
		if (_pendingIds.contains(id) || _active.contains(id))
		{
			_waitersForId[id].emplace_back(std::move(cb));
			return;
		}
		if (id < _nextId.load(std::memory_order_acquire))
		{
			// Task already finished: enqueue the callback after unlock.
			runNow = true;
		}
		else
		{
			Debug::warning("ThreadPool::onComplete: unknown task id [{}], ignored", id);
			return;
		}
	}

	if (runNow)
		enqueue(std::move(cb));
}

void ThreadPool::onDrain(std::function<void()>&& cb)
{
	if (!cb)
		return;

	bool runNow = false;
	{
		std::lock_guard lk{ _mutex };
		if (_queue.empty() && _active.empty())
			runNow = true;
		else
			_waitersForAll.emplace_back(std::move(cb));
	}

	if (runNow)
		enqueue(std::move(cb));
}

bool ThreadPool::isIdle() const
{
	std::lock_guard lk{ _mutex };
	return _queue.empty() && _active.empty();
}

size_t ThreadPool::threadCount() const
{
	std::lock_guard lk{ _mutex };
	return _workers.size();
}

void ThreadPool::_workerRoutine()
{
	while (true)
	{
		Task task;
		{
			std::unique_lock lk{ _mutex };
			_cv.wait(lk, [this] { return _stop.load(std::memory_order_acquire) || !_queue.empty(); });
			if (_stop.load(std::memory_order_acquire) && _queue.empty())
				return;
			if (_queue.empty())
				continue;
			task = std::move(_queue.front());
			_queue.pop_front();
			_pendingIds.erase(task.id);
			_active.insert(task.id);
		}

		_runTask(std::move(task));
	}
}

void ThreadPool::_runTask(Task task)
{
	// Never let a task kill its worker. Note: Debug::error throws in debug
	// builds, so report failures via warning (non-throwing) here.
	try
	{
		task.fn();
	}
	catch (const std::exception& e)
	{
		Debug::warning("ThreadPool task threw: {}", e.what());
	}
	catch (...)
	{
		Debug::warning("ThreadPool task threw unknown exception");
	}

	std::vector<std::function<void()>> ready;
	{
		std::lock_guard lk{ _mutex };
		_active.erase(task.id);
		if (auto it = _waitersForId.find(task.id); it != _waitersForId.end())
		{
			ready = std::move(it->second);
			_waitersForId.erase(it);
		}
		if (_queue.empty() && _active.empty() && !_waitersForAll.empty())
		{
			ready.insert(ready.end(), std::make_move_iterator(_waitersForAll.begin()), std::make_move_iterator(_waitersForAll.end()));
			_waitersForAll.clear();
		}
	}

	// Callbacks become new tasks: no lock held, reentrancy is safe.
	for (auto& cb : ready)
		enqueue(std::move(cb));
}
