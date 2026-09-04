#include "scheduler.h"
#include "debug.h"

Scheduler& Scheduler::get()
{
	static Scheduler instance;
	return instance;
}

Scheduler::Scheduler() : _worker(&Scheduler::loop, this)
{
}

Scheduler::~Scheduler()
{
	_stop.store(true, std::memory_order_release);
	_cv.notify_all();
	if (_worker.joinable())
		_worker.join();
}

Scheduler::Token Scheduler::enqueue(std::chrono::steady_clock::time_point at, std::function<void()> fn)
{
	if (!fn)
		return 0;

	const Token token = _next.fetch_add(1, std::memory_order_relaxed);
	{
		std::lock_guard lk{ _mutex };
		_queue.emplace(at, token, std::move(fn));
	}
	_cv.notify_one();
	return token;
}

void Scheduler::cancel(Token token)
{
	if (token == 0)
		return;
	std::lock_guard lk{ _mutex };
	_cancelled.insert(token);
}

void Scheduler::loop()
{
	std::unique_lock lk{ _mutex };
	while (!_stop.load(std::memory_order_acquire))
	{
		if (_queue.empty())
		{
			_cv.wait(lk, [this] { return _stop.load(std::memory_order_acquire) || !_queue.empty(); });
			continue;
		}

		const auto now = std::chrono::steady_clock::now();
		if (_queue.top().at > now)
		{
			_cv.wait_until(lk, _queue.top().at);
			continue;
		}

		Task task = std::move(_queue.top());
		_queue.pop();
		const bool dropped = _cancelled.erase(task.token) > 0;
		lk.unlock();

		// A running callback cannot be stopped by cancel — that is what the
		// stale flag is for (see Tutorial::_gen). The worker must never die.
		if (!dropped)
		{
			try
			{
				task.fn();
			}
			catch (const std::exception& e)
			{
				Debug::error("Scheduler task threw: {}", e.what());
			}
			catch (...)
			{
				Debug::error("Scheduler task threw unknown exception");
			}
		}

		lk.lock();
	}
}
