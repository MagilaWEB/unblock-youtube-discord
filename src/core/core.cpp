#include <charconv>

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

	_temp_path = std::filesystem::temp_directory_path();
}

Core& Core::get()
{
	static Core instance;
	return instance;
}

void Core::parallelRun()
{
	_pool.start();
}

void Core::finish()
{
	_pool.stop();
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

std::filesystem::path Core::tempPath() const
{
	return _temp_path;
}

std::vector<std::string> Core::exec(std::string cmd)
{
	if (std::unique_ptr<FILE, decltype(&_pclose)> pipe{ _popen(cmd.c_str(), "r"), _pclose })
	{
		std::array<char, 1'024>	 buffer{};
		std::vector<std::string> result{};

		while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr)
			result.emplace_back(buffer.data());
		return result;
	}

	throw std::runtime_error("popen() failed!");
}

void Core::execParallel(std::string cmd, std::function<bool(std::string)>&& callback)
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

					Debug::info("{}", data);
				}
			}

			callback("EXIT");
		}
	).detach();
}

std::tuple<u32, u32, u32> Core::_parseSimpleVersion(const std::string& version)
{
	std::array<u32, 3> parts{};
	size_t			   idx = 0;

	for (auto part : version | std::views::split('.'))
	{
		if (idx >= parts.size())
			break;

		const std::string_view token{ std::ranges::data(part), std::ranges::size(part) };

		u32 value = 0;
		if (auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value); ec == std::errc{})
			parts[idx] = value;

		++idx;
	}

	return { parts[0], parts[1], parts[2] };
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

Core::TaskId Core::addTask(std::function<void()>&& callback)
{
	return _pool.enqueue(std::move(callback));
}

void Core::taskComplete(TaskId id, std::function<void()>&& callback)
{
	_pool.onComplete(id, std::move(callback));
}

void Core::taskComplete(std::function<void()>&& callback)
{
	_pool.onDrain(std::move(callback));
}
