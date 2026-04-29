#include "service.h"

using namespace std::chrono_literals;

static std::string win_error_message(DWORD err)
{
	return std::system_category().message(static_cast<int>(err));
}

Service::~Service()
{
	close();
}

// -----------------------------------------------------------------------------
// Parameter setters
// -----------------------------------------------------------------------------
void Service::setName(std::string new_name)
{
	CRITICAL_SECTION_RAII(_lock);
	_name = std::move(new_name);
}

void Service::setDescription(std::string_view description)
{
	CRITICAL_SECTION_RAII(_lock);
	_description = description;
}

void Service::setArgs(std::vector<std::string> args)
{
	CRITICAL_SECTION_RAII(_lock);
	_args = std::move(args);
}

// -----------------------------------------------------------------------------
// Information retrieval
// -----------------------------------------------------------------------------
std::string Service::getName() const
{
	return _name;
}

const Service::Config& Service::getConfig()
{
	CRITICAL_SECTION_RAII(_lock);
	update();
	return _config;
}

bool Service::isRun()
{
	CRITICAL_SECTION_RAII(_lock);
	update();
	return _config.sc_status.dwCurrentState == SERVICE_START_PENDING || _config.sc_status.dwCurrentState == SERVICE_RUNNING;
}

// -----------------------------------------------------------------------------
// Internal methods
// -----------------------------------------------------------------------------
void Service::_initScManager()
{
	if (_sc_manager)
		return;

	SC_HANDLE h = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);

	ASSERT_ARGS(h, "Couldn't open SCManager to interact with system services!");

	_sc_manager.reset(h);
}

void Service::open()
{
	CRITICAL_SECTION_RAII(_lock);
	if (_sc)
		return;

	_initScManager();

	for (size_t i = 0; i < _max_open_retries; ++i)
	{
		SC_HANDLE h = OpenService(_sc_manager.get(), _name.c_str(), SC_MANAGER_ALL_ACCESS);
		if (h)
		{
			_sc.reset(h);
			update();
			return;
		}

		if (i < _max_open_retries - 1)
			std::this_thread::sleep_for(std::chrono::milliseconds(_open_retry_ms));
	}
}

void Service::create()
{
	CRITICAL_SECTION_RAII(_lock);

	ASSERT_ARGS(!_sc, "The service [{}] has already been found or created, it cannot be recreated!", _name);

	_initScManager();

	std::string tempBinPath = (Core::get().binariesPath() / _file_name).string();
	std::string fullCmdLine = "\"" + tempBinPath + "\"";
	for (const auto& arg : _args)
		fullCmdLine += " " + arg;

	ASSERT_ARGS(fullCmdLine.length() <= 32'767, "The maximum line size for the service path is 32'767!");

	if (fullCmdLine.length() > 30'000)
		Debug::warning(
			"For the [{}] service, the number of launch parameter characters exceeds 30,000. The current number of characters is [{}]. Please note that the maximum length can be 32,767! ",
			_name,
			fullCmdLine.length()
		);

	auto wname	  = utils::UTF8_to_UTF16(_name);
	auto wdesc	  = utils::UTF8_to_UTF16(_description);
	auto wtempBin = utils::UTF8_to_UTF16(tempBinPath);
	auto wfullCmd = utils::UTF8_to_UTF16(fullCmdLine);

	_time_limit.start();
	while (true)
	{
		SC_HANDLE h = CreateServiceW(
			_sc_manager.get(),
			wname.c_str(),
			wname.c_str(),
			SC_MANAGER_ALL_ACCESS,
			SERVICE_WIN32_OWN_PROCESS,
			SERVICE_AUTO_START,
			SERVICE_ERROR_NORMAL,
			wtempBin.c_str(),
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr
		);

		if (h)
		{
			_sc.reset(h);

			SERVICE_DESCRIPTIONW sd{};
			sd.lpDescription = wdesc.empty() ? nullptr : const_cast<LPWSTR>(wdesc.c_str());
			ChangeServiceConfig2W(_sc.get(), SERVICE_CONFIG_DESCRIPTION, &sd);

			if (!ChangeServiceConfigW(
					_sc.get(),
					SERVICE_NO_CHANGE,
					SERVICE_NO_CHANGE,
					SERVICE_NO_CHANGE,
					wfullCmd.c_str(),
					nullptr,
					nullptr,
					nullptr,
					nullptr,
					nullptr,
					nullptr
				))
			{
				DWORD		err		= GetLastError();
				std::string message = win_error_message(err);
				InputConsole::textError(Localization::Str{ "str_error_create_service" }(), _name, message.c_str());
				_sc.reset();
				return;
			}

			update();
			return;
		}

		DWORD err = GetLastError();

		open();

		if (_sc)
			return;

		if (_time_limit.getElapsed_sec() > 5.0f)
		{
			std::string message = win_error_message(err);
			InputConsole::textError(Localization::Str{ "str_error_create_service" }(), _name, message.c_str());
			return;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(_create_retry_ms));
	}
}

void Service::start()
{
	CRITICAL_SECTION_RAII(_lock);

	if (!_sc)
	{
		open();

		if (!_sc)
		{
			InputConsole::textError(Localization::Str{ "str_error_stoping_service" }(), _name);
			return;
		}
	}

	update();

	DWORD state = _config.sc_status.dwCurrentState;
	if (state != SERVICE_STOPPED && state != SERVICE_STOP_PENDING)
	{
		InputConsole::textError(Localization::Str{ "str_error_stoping_service" }(), _name);
		return;
	}

	InputConsole::textPlease(Localization::Str{ "str_wait_startig_service" }(), true, _name);

	std::vector<pcstr> argv;
	for (const auto& arg : _args)
		argv.push_back(arg.c_str());

	_time_limit.start();
	bool started = false;
	while (true)
	{
		started = StartService(_sc.get(), static_cast<DWORD>(argv.size()), argv.data());
		if (started)
			break;

		if (_time_limit.getElapsed_sec() > 5.0f)
		{
			InputConsole::textError(Localization::Str{ "str_error_wait_time_start_service" }(), _name);
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(_start_retry_ms));
	}

	update();

	auto on_timeout = [this]
	{
		InputConsole::textError(Localization::Str{ "str_expired_wait_time_start_service" }(), _name);
		stop();
	};

	_waitStatusService(SERVICE_START_PENDING, SERVICE_RUNNING, on_timeout);
	_waitStatusService(SERVICE_CONTINUE_PENDING, SERVICE_RUNNING, on_timeout);

	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	update();

	if (_config.sc_status.dwCurrentState == SERVICE_RUNNING)
		InputConsole::textOk(Localization::Str{ "str_success_start_service" }(), _name);
	else
		InputConsole::textWarning(Localization::Str{ "str_warning_no_start_service" }(), _name);
}

void Service::update()
{
	if (!_sc)
		return;

	DWORD				   needed = 0;
	SERVICE_STATUS_PROCESS temp{};
	if (!QueryServiceStatusEx(_sc.get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&temp), sizeof(temp), &needed))
	{
		_sc.reset();
		_config.sc_status = {};
		return;
	}
	_config.sc_status = temp;

	needed = 0;
	std::unique_ptr<QUERY_SERVICE_CONFIGA, decltype(&free)> configBuf{ nullptr, &free };
	DWORD													bufSize = 0;
	while (true)
	{
		QUERY_SERVICE_CONFIGA* ptr = reinterpret_cast<QUERY_SERVICE_CONFIGA*>(configBuf.get());
		if (QueryServiceConfigA(_sc.get(), ptr, bufSize, &needed))
		{
			if (ptr)
			{
				_config.type			 = static_cast<u32>(ptr->dwServiceType);
				_config.start_type		 = static_cast<u32>(ptr->dwStartType);
				_config.tag_id			 = static_cast<u32>(ptr->dwTagId);
				_config.start_name		 = ptr->lpServiceStartName ? ptr->lpServiceStartName : "";
				_config.display_name	 = ptr->lpDisplayName ? ptr->lpDisplayName : "";
				_config.load_order_group = ptr->lpLoadOrderGroup ? ptr->lpLoadOrderGroup : "";
				_config.binary_path		 = ptr->lpBinaryPathName ? ptr->lpBinaryPathName : "";
			}
			break;
		}

		DWORD err = GetLastError();
		if (err != ERROR_INSUFFICIENT_BUFFER && err != ERROR_MORE_DATA)
			break;

		bufSize += needed;
		configBuf.reset(reinterpret_cast<QUERY_SERVICE_CONFIGA*>(malloc(bufSize)));

		if (!configBuf)
			break;
	}
}

void Service::stop()
{
	CRITICAL_SECTION_RAII(_lock);
	if (!_sc)
		return;

	update();

	if (_config.sc_status.dwCurrentState == SERVICE_STOPPED)
		return;

	InputConsole::textPlease(Localization::Str{ "str_wait_stoping_service" }(), true, _name);

	bool stopped   = false;
	auto send_stop = [&]
	{
		SERVICE_STATUS dummy;
		const bool	   ok = ControlService(_sc.get(), SERVICE_CONTROL_STOP, &dummy);
		ASSERT_ARGS(ok, "Failed to send a request to stop the service [{}]!", _name);
		stopped = true;
	};

	send_stop();

	_waitStatusService(SERVICE_RUNNING, SERVICE_STOPPED, send_stop);
	_waitStatusService(SERVICE_STOP_PENDING, SERVICE_STOPPED, send_stop);

	update();
	if (stopped && _config.sc_status.dwCurrentState == SERVICE_STOPPED)
		InputConsole::textOk(Localization::Str{ "str_success_stop_service" }(), _name);
}

void Service::remove()
{
	CRITICAL_SECTION_RAII(_lock);
	if (!_sc)
	{
		open();
		if (!_sc)
			return;
	}

	stop();

	DeleteService(_sc.get());	 // ignore error, service might already be deleted
	_sc.reset();
}

void Service::close()
{
	CRITICAL_SECTION_RAII(_lock);
	_sc.reset();
	_sc_manager.reset();
	_config = {};
}

// -----------------------------------------------------------------------------
// Static methods
// -----------------------------------------------------------------------------
void Service::allService(std::function<void(std::string)>&& callback)
{
	UniqueScHandle scManager(OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
	if (!scManager)
		return;

	DWORD												   needed = 0, returned = 0;
	std::unique_ptr<ENUM_SERVICE_STATUSA, decltype(&free)> buf{ nullptr, &free };
	DWORD												   bufSize = 0;
	while (true)
	{
		if (EnumServicesStatusA(scManager.get(), SERVICE_WIN32, SERVICE_STATE_ALL, buf.get(), bufSize, &needed, &returned, nullptr))
		{
			if (buf)
			{
				for (DWORD i = 0; i < returned; ++i)
				{
					std::string name = buf.get()[i].lpServiceName;
					callback(name.empty() ? buf.get()[i].lpDisplayName : name);
				}
			}

			break;
		}

		if (GetLastError() != ERROR_MORE_DATA)
			break;

		bufSize += needed;
		buf.reset(reinterpret_cast<ENUM_SERVICE_STATUSA*>(malloc(bufSize)));

		if (!buf)
			break;
	}
}

std::vector<std::string> Service::allService()
{
	std::vector<std::string> list;
	allService([&list](std::string name) { list.push_back(name); });
	return list;
}

// -----------------------------------------------------------------------------
// Status waiting
// -----------------------------------------------------------------------------
void Service::_waitStatusService(DWORD check_state, DWORD check_stat_end, std::function<void()>&& on_timeout)
{
	_dw_start_time = GetTickCount64();
	while (_sc && _config.sc_status.dwCurrentState == check_state)
	{
		_dw_wait_time = std::clamp<DWORD>(_config.sc_status.dwWaitHint / 10, 1'000, 10'000);
		std::this_thread::sleep_for(std::chrono::milliseconds(_dw_wait_time));

		update();

		if (_config.sc_status.dwCurrentState == check_stat_end)
			break;

		if ((GetTickCount64() - _dw_start_time) > _dw_timeout_ms)
		{
			on_timeout();
			break;
		}
	}
}
