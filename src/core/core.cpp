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
					while (!_task_buffer_parallel.empty() && !_quit_task)
					{
						_task_run.emplace_back(_task_buffer_parallel.front());
						_task_buffer_parallel.pop_front();
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

				while (!_task_buffer.empty() && !_quit_task)
				{
					_task_buffer.front()();
					_task_buffer.pop_front();
				}

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

std::vector<std::string> Core::exec(std::string cmd)
{
	if (std::unique_ptr<FILE, decltype(&_pclose)> pipe{ _popen(cmd.c_str(), "r"), _pclose })
	{
		std::array<char, 1'024>	 buffer{};
		std::vector<std::string> result{};

		while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr)
			result.push_back(buffer.data());
		return result;
	}

	throw std::runtime_error("popen() failed!");
}

void Core::exec_parallel(std::string cmd, std::function<bool(std::string)>&& callback)
{
	std::jthread(
		[cmd, callback]
		{
			if (std::unique_ptr<FILE, decltype(&_pclose)> pipe{ _popen(cmd.c_str(), "r"), _pclose })
			{
				std::array<char, 1'024> buffer{};

				while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr)
				{
					auto data = buffer.data();
					if (callback(data))
						break;

					Debug::info("%s", data);
				}
			}
		}
	).detach();
}

std::tuple<u32, u32, u32> Core::_parseSimpleVersion(const std::string& version)
{
	std::stringstream ss(version);
	std::string		  major, minor, patch;

	std::getline(ss, major, '.');
	std::getline(ss, minor, '.');
	std::getline(ss, patch, '.');

	return std::make_tuple(std::stoi(major), std::stoi(minor), patch.empty() ? 0 : std::stoi(patch));
}

bool Core::isVersionNewer(std::string version1, std::string version2)
{
	auto [major1, minor1, patch1] = _parseSimpleVersion(version1);
	auto [major2, minor2, patch2] = _parseSimpleVersion(version2);

	if (major1 > major2)
		return true;
	if (major1 < major2)
		return false;

	if (minor1 > minor2)
		return true;
	if (minor1 < minor2)
		return false;

	return patch1 > patch2;
}

void Core::addTask(std::function<void()>&& callback)
{
	FAST_LOCK(_task_lock);
	_task_buffer.emplace_back(callback);
}

void Core::addTaskParallel(std::function<void()>&& callback)
{
	FAST_LOCK(_task_lock);
	_task_buffer_parallel.emplace_back(callback);
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
