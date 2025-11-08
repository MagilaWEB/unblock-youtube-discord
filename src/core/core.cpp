Core::Core()
{
	auto current_path = std::filesystem::current_path();

	if (current_path.filename().string() == "bin")
	{
		_bin_path	  = current_path;
		_current_path = current_path.parent_path();
	}
	else
	{
		_current_path = current_path;
		current_path  = current_path / "bin";
		if (std::filesystem::exists(current_path))
			_bin_path = current_path;
		else
			Debug::fatal("bin directory not found!");
	}

	_binaries_path = (_current_path / "binaries");

	if (!std::filesystem::exists(_binaries_path))
		Debug::fatal("binaries directory not found!");

	_configs_path = (_current_path / "configs");

	if (!std::filesystem::exists(_configs_path))
		Debug::fatal("configs directory not found!");

	_user_path = (_current_path / "user");

	if (!std::filesystem::exists(_user_path))
		std::filesystem::create_directories(_user_path);
}

Core& Core::get()
{
	#if __clang__
	[[clang::no_destroy]]
	#endif
	static Core instance;
	return instance;
}

void Core::initialize(const std::string& /*command_line*/)
{
}

std::filesystem::path Core::currentPath() const
{
	return _current_path;
}

std::filesystem::path Core::binPath() const
{
	return _bin_path;
}

std::filesystem::path Core::binariesPath() const
{
	return _binaries_path;
}

std::filesystem::path Core::configsPath() const
{
	return _configs_path;
}

std::filesystem::path Core::userPath() const
{
	return _user_path;
}

void Core::addTask(std::function<void()>&& callback)
{
	FAST_LOCK(_task_lock);
	_task.emplace_back(callback);
}

void Core::addTaskJS(std::function<void()> callback)
{
	FAST_LOCK(_task_lock_js);
	_task_js.emplace_back(callback);
}

std::deque<std::function<void()>>& Core::getTask()
{
	return _task;
}

std::deque<std::function<void()>>& Core::getTaskJS()
{
	return _task_js;
}

FastLock& Core::getTaskLock()
{
	return _task_lock;
}

FastLock& Core::getTaskLockJS()
{
	return _task_lock_js;
}

void Core::setThreadJsID(DWORD id)
{
	_thread_js_id = id;
}

DWORD Core::getThreadJsID()
{
	return _thread_js_id;
}
