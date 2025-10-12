#pragma once

class CORE_API Core final
{
	inline static DWORD								_thread_js_id{};
	inline static FastLock							_task_lock;
	inline static CriticalSection					_task_lock_js;
	inline static std::queue<std::function<void()>> _task;
	inline static std::queue<std::function<void()>> _task_js;

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
	void		 initialize(const std::string& command_line);

	std::filesystem::path currentPath() const;
	std::filesystem::path binPath() const;
	std::filesystem::path binariesPath() const;
	std::filesystem::path configsPath() const;
	std::filesystem::path userPath() const;

	static void addTask(std::function<void()>&& fn);
	static void addTaskJS(const std::function<void()>& fn);

	static std::queue<std::function<void()>>& getTask();
	static std::queue<std::function<void()>>& getTaskJS();
	static FastLock&						  getTaskLock();
	static CriticalSection&					  getTaskLockJS();

	static void	 setThreadJsID(DWORD id);
	static DWORD getThreadJsID();
};
