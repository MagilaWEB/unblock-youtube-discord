#pragma once

class CORE_API Core final
{
	inline static DWORD								_thread_js_id{};
	inline static FastLock							_task_lock;
	inline static FastLock							_task_lock_js;
	inline static std::deque<std::function<void()>> _task;
	inline static std::deque<std::function<void()>> _task_js;

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

	static void addTask(std::function<void()>&& callback);
	static void addTaskJS(std::function<void()> callback);

	static std::deque<std::function<void()>>& getTask();
	static std::deque<std::function<void()>>& getTaskJS();
	static FastLock&						  getTaskLock();
	static FastLock&						  getTaskLockJS();

	static void	 setThreadJsID(DWORD id);
	static DWORD getThreadJsID();
};
