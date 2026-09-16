#pragma once

#include "thread_pool.h"

class Core final
{
	ThreadPool _pool;

	Core();
	~Core() = default;

	std::filesystem::path _current_path{};
	std::filesystem::path _bin_path{};
	std::filesystem::path _binaries_path{};
	std::filesystem::path _configs_path{};
	std::filesystem::path _user_path{};
	std::filesystem::path _temp_path{};

public:
	Core(Core&&) = delete;

	using TaskId = ThreadPool::TaskId;

public:
	static Core& get();

	void initialize(const std::string& command_line);
	void parallel_run();
	void finish();

	std::filesystem::path currentPath() const;
	std::filesystem::path binPath() const;
	std::filesystem::path binariesPath() const;
	std::filesystem::path configsPath() const;
	std::filesystem::path userPath() const;
	std::filesystem::path tempPath() const;

	std::vector<std::string> exec(std::string cmd);
	void					 exec_parallel(std::string cmd, std::function<bool(std::string)>&& callback);

	bool isVersionNewer(std::string version1, std::string version2);

	/// Enqueue a task for execution by the pool. Returns the task id.
	TaskId addTask(std::function<void()>&& callback);

	/// Run callback after the specific task finishes. If the task already
	/// finished, the callback is enqueued immediately.
	void taskComplete(TaskId id, std::function<void()>&& callback);

	/// Run callback once the pool is idle (all tasks finished). If already
	/// idle, the callback is enqueued immediately.
	void taskComplete(std::function<void()>&& callback);

private:
	std::tuple<u32, u32, u32> _parseSimpleVersion(const std::string& version);
};
