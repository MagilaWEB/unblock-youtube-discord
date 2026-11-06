module;

#include "macros.hpp"

module core.service;

import ext.windows;
import core;
import core.debug;
import core.input_console;
import core.localization;
import core.utils;

using namespace std::chrono_literals;

static std::string winErrorMessage(DWORD err)
{
	return std::system_category().message(static_cast<int>(err));
}

bool ServiceFailure::valid() const
{
	return !service_name.empty();
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

const ServiceFailure& Service::getLastFailure() const
{
	return _last_failure;
}

void Service::clearLastFailure()
{
	_last_failure = {};
}

std::vector<std::string> Service::getArgs()
{
	CRITICAL_SECTION_RAII(_lock);
	return _args;
}

std::filesystem::path Service::getFilePath() const
{
	return Core::get().binariesPath() / _file_name;
}

void Service::_recordFailure(ServiceFailure::Stage stage, u32 win32_error)
{
	_last_failure					 = {};
	_last_failure.service_name		 = _name;
	_last_failure.binary_path		 = getFilePath().string();
	_last_failure.stage				 = stage;
	_last_failure.win32_error		 = win32_error;
	_last_failure.created			 = _was_created;
	_last_failure.start_tick_ms		 = _start_tick_ms;
	_last_failure.state				 = static_cast<u32>(_config.sc_status.dwCurrentState);
	_last_failure.checkpoint		 = static_cast<u32>(_config.sc_status.dwCheckPoint);
	_last_failure.win32_exit_code	 = static_cast<u32>(_config.sc_status.dwWin32ExitCode);
	_last_failure.specific_exit_code = static_cast<u32>(_config.sc_status.dwServiceSpecificExitCode);

	if (_start_tick_ms != 0)
		_last_failure.lived_ms = GetTickCount64() - _start_tick_ms;
}

bool Service::isRun()
{
	// Never block: a stop/start holds _lock for seconds (up to c_dw_timeout_ms
	// per wait) while the UI tick polls this every frame. Refresh and cache
	// only when the lock is free, otherwise answer from the last snapshot.
	if (_lock.tryEnter())
	{
		update();
		const bool running = _config.sc_status.dwCurrentState == SERVICE_START_PENDING || _config.sc_status.dwCurrentState == SERVICE_RUNNING;
		_cached_running.store(running, std::memory_order_relaxed);
		_lock.leave();
		return running;
	}

	return _cached_running.load(std::memory_order_relaxed);
}

void Service::_refreshCachedRunning()
{
	const DWORD state = _config.sc_status.dwCurrentState;
	_cached_running.store(state == SERVICE_START_PENDING || state == SERVICE_RUNNING, std::memory_order_relaxed);
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

	for (size_t i = 0; i < c_max_open_retries; ++i)
	{
		SC_HANDLE h = OpenService(_sc_manager.get(), _name.c_str(), SC_MANAGER_ALL_ACCESS);
		if (h)
		{
			_sc.reset(h);
			update();
			return;
		}

		if (i < c_max_open_retries - 1)
			std::this_thread::sleep_for(std::chrono::milliseconds(c_open_retry_ms));
	}

	_recordFailure(ServiceFailure::Stage::Open, GetLastError());
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
			"For the [{}] service, the number of launch parameter characters exceeds 30,000. The current number of characters is [{}]. Please note "
			"that the maximum length can be 32,767! ",
			_name,
			fullCmdLine.length()
		);

	auto wname	  = utils::utf8ToUtf16(_name);
	auto wdesc	  = utils::utf8ToUtf16(_description);
	auto wtempBin = utils::utf8ToUtf16(tempBinPath);
	auto wfullCmd = utils::utf8ToUtf16(fullCmdLine);

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
			_was_created = true;

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
				std::string message = winErrorMessage(err);
				_recordFailure(ServiceFailure::Stage::Create, err);
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

		if (_time_limit.getElapsedSec() > 5.0F)
		{
			std::string message = winErrorMessage(err);
			_recordFailure(ServiceFailure::Stage::Create, err);
			InputConsole::textError(Localization::Str{ "str_error_create_service" }(), _name, message.c_str());
			return;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(c_create_retry_ms));
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
			_recordFailure(ServiceFailure::Stage::Open, GetLastError());
			InputConsole::textError(Localization::Str{ "str_error_stopping_service" }(), _name);
			return;
		}
	}

	update();

	DWORD state = _config.sc_status.dwCurrentState;
	if (state != SERVICE_STOPPED && state != SERVICE_STOP_PENDING)
	{
		InputConsole::textError(Localization::Str{ "str_error_stopping_service" }(), _name);
		return;
	}

	InputConsole::textPlease(Localization::Str{ "str_wait_starting_service" }(), true, _name);

	std::vector<pcstr> argv;
	for (const auto& arg : _args)
		argv.push_back(arg.c_str());

	_time_limit.start();
	DWORD start_error = 0;
	while (true)
	{
		if (StartService(_sc.get(), static_cast<DWORD>(argv.size()), argv.data()))
			break;

		start_error = GetLastError();

		if (_time_limit.getElapsedSec() > 5.0F)
		{
			_recordFailure(ServiceFailure::Stage::Start, start_error);
			InputConsole::textError(Localization::Str{ "str_error_wait_time_start_service" }(), _name);
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(c_start_retry_ms));
	}

	// The process is alive from here on: this tick anchors lived_ms and the
	// event log window the diagnostics look at.
	_start_tick_ms = GetTickCount64();
	update();

	bool unexpected_stop = false;
	bool timed_out		 = false;
	switch (_waitStarted())
	{
	case StartOutcome::Stopped:
		// The process died or was stopped before it reported running. That is
		// not a timeout and must not be reported as a status change: the SCM
		// exit codes recorded in the snapshot are the actual evidence.
		unexpected_stop = true;
		_recordFailure(ServiceFailure::Stage::UnexpectedStop, 0);
		InputConsole::textError(Localization::Str{ "str_error_service_died_after_start" }(), _name);
		break;
	case StartOutcome::TimedOut:
		timed_out = true;
		_recordFailure(ServiceFailure::Stage::WaitRunning, 0);
		InputConsole::textError(Localization::Str{ "str_expired_wait_time_start_service" }(), _name);
		stop();
		break;
	default:
		break;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	update();

	if (unexpected_stop)
		return;

	if (_config.sc_status.dwCurrentState == SERVICE_RUNNING)
	{
		clearLastFailure();
		InputConsole::textOk(Localization::Str{ "str_success_start_service" }(), _name);
	}
	else
	{
		// The timeout snapshot (pending state + checkpoint) is the useful one,
		// stop() has just overwritten the status.
		if (!timed_out)
			_recordFailure(ServiceFailure::Stage::WaitRunning, 0);
		InputConsole::textWarning(Localization::Str{ "str_warning_no_start_service" }(), _name);
	}
}

void Service::update()
{
	if (!_sc)
	{
		_config.sc_status = {};
		_refreshCachedRunning();
		return;
	}

	DWORD				   needed = 0;
	SERVICE_STATUS_PROCESS temp{};
	if (!QueryServiceStatusEx(_sc.get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&temp), sizeof(temp), &needed))
	{
		_sc.reset();
		_config.sc_status = {};
		_refreshCachedRunning();
		return;
	}

	_config.sc_status = temp;
	_refreshCachedRunning();

	needed = 0;
	std::unique_ptr<QUERY_SERVICE_CONFIGA, decltype(&std::free)> configBuf{ nullptr, &std::free };
	DWORD														 bufSize = 0;
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
		configBuf.reset(reinterpret_cast<QUERY_SERVICE_CONFIGA*>(std::malloc(bufSize)));

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

	InputConsole::textPlease(Localization::Str{ "str_wait_stopping_service" }(), true, _name);

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
	_was_created = false;
}

void Service::close()
{
	CRITICAL_SECTION_RAII(_lock);
	_sc.reset();
	_sc_manager.reset();
	_config		 = {};
	_was_created = false;
	_refreshCachedRunning();
}

// -----------------------------------------------------------------------------
// Static methods
// -----------------------------------------------------------------------------
void Service::allService(std::function<void(std::string)>&& callback)
{
	UniqueScHandle scManager(OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
	if (!scManager)
		return;

	DWORD														needed = 0, returned = 0;
	std::unique_ptr<ENUM_SERVICE_STATUSA, decltype(&std::free)> buf{ nullptr, &std::free };
	DWORD														bufSize = 0;
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
		buf.reset(reinterpret_cast<ENUM_SERVICE_STATUSA*>(std::malloc(bufSize)));

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

		if ((GetTickCount64() - _dw_start_time) > c_dw_timeout_ms)
		{
			on_timeout();
			break;
		}
	}
}

Service::StartOutcome Service::_waitStarted()
{
	_dw_start_time = GetTickCount64();

	while (_sc)
	{
		switch (_config.sc_status.dwCurrentState)
		{
		case SERVICE_RUNNING:
			return StartOutcome::Running;

		// The process left the pending state by dying (or being stopped). The
		// old code treated any exit from START_PENDING as success and left the
		// caller to report a vague "changed its status".
		case SERVICE_STOPPED:
		case SERVICE_STOP_PENDING:
			return StartOutcome::Stopped;

		case SERVICE_START_PENDING:
		case SERVICE_CONTINUE_PENDING:
			break;

		default:
			return StartOutcome::Unknown;
		}

		_dw_wait_time = std::clamp<DWORD>(_config.sc_status.dwWaitHint / 10, 1'000, 10'000);
		std::this_thread::sleep_for(std::chrono::milliseconds(_dw_wait_time));

		update();

		if ((GetTickCount64() - _dw_start_time) > c_dw_timeout_ms)
			return StartOutcome::TimedOut;
	}

	return StartOutcome::Stopped;
}
