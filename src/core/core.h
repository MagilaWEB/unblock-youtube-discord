#pragma once

class CORE_API Core final
{
	inline static DWORD _thread_js_id{};

	FastLock						  _task_lock;
	FastLock						  _task_complete_lock;
	FastLock						  _task_lock_js;
	std::deque<std::function<void()>> _task_buffer;
	std::deque<std::function<void()>> _task_complete;
	std::list<std::function<void()>>  _task_run;
	std::deque<std::function<void()>> _task_js;

	std::atomic_bool _quit_task{ false };

	Core();
	~Core() = default;

	std::filesystem::path _current_path{};
	std::filesystem::path _bin_path{};
	std::filesystem::path _binaries_path{};
	std::filesystem::path _configs_path{};
	std::filesystem::path _user_path{};

public:
	Core(Core&&) = delete;

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

	std::vector<std::string> exec(std::string cmd);
	void					 exec_parallel(std::string cmd, std::function<bool(std::string)>&& callback);

	bool isVersionNewer(std::string version1, std::string version2);

	void addTask(std::function<void()>&& callback);

	void taskComplete(std::function<void()>&& callback);

	void addTaskJS(std::function<void()> callback);

	std::deque<std::function<void()>>& getTaskJS();
	FastLock&						   getTaskLock();
	FastLock&						   getTaskLockJS();

	static void	 setThreadJsID(DWORD id);
	static DWORD getThreadJsID();

private:
	std::tuple<u32, u32, u32> _parseSimpleVersion(const std::string& version);
};
