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

void Core::parallel_run()
{
	std::jthread thread(
		[this]
		{
			while (!_quit_task)
			{
				using namespace std::chrono;
				std::this_thread::sleep_for(20ms);

				{
					FAST_LOCK(_task_lock);

					if (_task_run.empty())
					{
						while (!_task_buffer.empty() && !_quit_task)
						{
							_task_run.emplace_back(_task_buffer.front());
							_task_buffer.pop_front();
						}
					}
				}

				std::for_each(
					std::execution::par,
					_task_run.begin(),
					_task_run.end(),
					[this](std::function<void()> callback)
					{
						if (!_quit_task)
							callback();
					}
				);

				_task_run.clear();

				_task_lock.EnterShared();
				if (_task_buffer.empty())
				{
					_task_lock.LeaveShared();
					FAST_LOCK(_task_complete_lock);
					while (!_task_complete.empty() && !_quit_task)
					{
						_task_complete.front()();
						_task_complete.pop_front();
					}
				}
				else
					_task_lock.LeaveShared();

				_task_run_state.store(false);

				FAST_LOCK_SHARED(_task_lock_js);
			}
		}
	);

	thread.detach();
}

void Core::finish()
{
	_quit_task = true;
	FAST_LOCK(_task_lock, 1);
	FAST_LOCK(_task_complete_lock);
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
	_task_buffer.emplace_back(callback);
}

void Core::taskComplete(std::function<void()>&& callback)
{
	FAST_LOCK_SHARED(_task_lock, _get);
	FAST_LOCK(_task_complete_lock);
	_task_complete.emplace_back(callback);
}

void Core::addTaskJS(std::function<void()> callback)
{
	FAST_LOCK(_task_lock_js);
	_task_js.emplace_back(callback);
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
