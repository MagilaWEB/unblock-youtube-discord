#include "service.h"

Service::~Service()
{
	close();
}

void Service::setName(std::string new_name)
{
	CRITICAL_SECTION_RAII(lock);

	_name = new_name;
}

void Service::setDescription(std::string_view description)
{
	CRITICAL_SECTION_RAII(lock);

	_description = description;
}

std::string Service::getName() const
{
	return _name.data();
}

const Service::Config& Service::getConfig()
{
	CRITICAL_SECTION_RAII(lock);

	update();

	return config;
}

bool Service::isRun()
{
	CRITICAL_SECTION_RAII(lock);

	update();

	return config.sc_status.dwCurrentState == SERVICE_START_PENDING || config.sc_status.dwCurrentState == SERVICE_RUNNING;
}

void Service::create()
{
	CRITICAL_SECTION_RAII(lock);

	ASSERT_ARGS(!sc, "The service [{}] has already been found or created, it cannot be recreated!", _name);

	_initScManager();

	std::string sc_path{ "\"\"" };
	sc_path.insert(1, (Core::get().binariesPath() / file_name).string());

	std::string args{ " " };

	for (auto& arg : _args)
		args = args.append(arg + " ");

	sc_path.append(args);

	ASSERT_ARGS(sc_path.length() <= 4'128, "The maximum line size for the service path is 4127!");

	_time_limit.start();
	do
	{
		sc = CreateService(
			_sc_manager,
			utils::UTF8_to_CP1251(_name).c_str(),
			utils::UTF8_to_CP1251(_description).c_str(),
			SC_MANAGER_ALL_ACCESS,
			SERVICE_WIN32_OWN_PROCESS,
			SERVICE_AUTO_START,
			SERVICE_ERROR_NORMAL,
			utils::UTF8_to_CP1251(sc_path.c_str()).c_str(),
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr
		);

		u32 err = GetLastError();

		update();

		if (sc)
			break;

		open();

		if (sc)
			break;

		if (_time_limit.getElapsed_sec() > 5.f)
		{
			std::string message = std::system_category().message(static_cast<int>(err));
			InputConsole::textError(Localization::Str{ "str_error_create_service" }(), _name, message.c_str());
			return;
		}

		using namespace std::chrono;
		std::this_thread::sleep_for(300ms);

	} while (true);

	ASSERT_ARGS(sc, "Service could not be created [{}]!", _name);
}

void Service::setArgs(std::vector<std::string> args)
{
	CRITICAL_SECTION_RAII(lock);

	_args = args;
}

void Service::start()
{
	CRITICAL_SECTION_RAII(lock);

	update();

	if (config.sc_status.dwCurrentState != SERVICE_STOPPED && config.sc_status.dwCurrentState != SERVICE_STOP_PENDING)
	{
		InputConsole::textError(Localization::Str{ "str_error_stoping_service" }(), _name);
		return;
	}

	InputConsole::textPlease(Localization::Str{ "str_wait_startig_service" }(), true, _name);

	std::vector<pcstr> args;
	for (auto& arg : _args)
		args.push_back(arg.c_str());

	bool send_start = false;
	_time_limit.start();
	do
	{
		send_start = StartService(sc, static_cast<u32>(args.size()), args.data());

		// We try for 5 seconds, otherwise we interrupt.
		if (_time_limit.getElapsed_sec() > 5.f)
		{
			InputConsole::textError(Localization::Str{ "str_error_wait_time_start_service" }(), _name);
			return;
		}

		if (!send_start)
		{
			// wait
			using namespace std::chrono;
			std::this_thread::sleep_for(300ms);
		}
	} while (!send_start);

	update();

	_waitStatusService(
		SERVICE_START_PENDING,
		SERVICE_RUNNING,
		[this]
		{
			InputConsole::textError(Localization::Str{ "str_expired_wait_time_start_service" }(), _name);
			stop();
		}
	);

	_waitStatusService(
		SERVICE_CONTINUE_PENDING,
		SERVICE_RUNNING,
		[this]
		{
			InputConsole::textError(Localization::Str{ "str_expired_wait_time_start_service" }(), _name);
			stop();
		}
	);

	using namespace std::chrono;
	std::this_thread::sleep_for(30ms);

	update();

	if (config.sc_status.dwCurrentState == SERVICE_RUNNING)
		InputConsole::textOk(Localization::Str{ "str_success_start_service" }(), _name);
	else
		InputConsole::textWarning(Localization::Str{ "str_warning_no_start_service" }(), _name);
}

void Service::update()
{
	if (!sc)
		return;

	DWORD dw_bytes_needed{ 0 };

	bool q_service = QueryServiceStatusEx(
		sc,
		SC_STATUS_PROCESS_INFO,
		reinterpret_cast<LPBYTE>(&config.sc_status),
		sizeof(SERVICE_STATUS_PROCESS),
		&dw_bytes_needed
	);

	if (!q_service)
	{
		CloseServiceHandle(sc);
		sc				 = nullptr;
		config.sc_status = {};
		return;
	}

	dw_bytes_needed = 0;

	LPQUERY_SERVICE_CONFIGA service_config = nullptr;
	u32						size_buffer{ 0 };
	while (true)
	{
		if (QueryServiceConfig(sc, service_config, size_buffer, &dw_bytes_needed))
		{
			if (service_config)
			{
				config.type				= static_cast<u32>(service_config->dwServiceType);
				config.start_type		= static_cast<u32>(service_config->dwStartType);
				config.tag_id			= static_cast<u32>(service_config->dwTagId);
				config.start_name		= service_config->lpServiceStartName;
				config.display_name		= service_config->lpDisplayName;
				config.load_order_group = service_config->lpLoadOrderGroup;
				config.binary_path		= service_config->lpBinaryPathName;

				free(service_config);
			}

			break;
		}

		u32 err = GetLastError();
		if (service_config && ERROR_MORE_DATA != err)
		{
			free(service_config);
			break;
		}

		size_buffer += dw_bytes_needed;
		free(service_config);
		service_config = reinterpret_cast<LPQUERY_SERVICE_CONFIGA>(malloc(size_buffer));
	}
}

void Service::open()
{
	CRITICAL_SECTION_RAII(lock);

	_initScManager();

	if (_sc_manager)
	{
		if (!sc)
		{
			u32 it{ 0 };
			do
			{
				sc = OpenService(_sc_manager, _name.data(), SC_MANAGER_ALL_ACCESS);
				if (sc)
					break;

				using namespace std::chrono;
				std::this_thread::sleep_for(5ms);

			} while (it++ < 2);

			update();
		}
	}
}

void Service::stop()
{
	CRITICAL_SECTION_RAII(lock);

	if (!sc)
		return;

	update();

	bool stopped = false;

	if (config.sc_status.dwCurrentState != SERVICE_STOPPED)
	{
		InputConsole::textPlease(Localization::Str{ "str_wait_stoping_service" }(), true, _name);

		auto service_stop = [this, &stopped]
		{
			stopped					= true;
			const bool send_control = ControlService(sc, SERVICE_CONTROL_STOP, reinterpret_cast<LPSERVICE_STATUS>(&config.sc_status));
			ASSERT_ARGS(send_control, "Failed to send a request to stop the service [{}]!", _name);
		};

		service_stop();

		_waitStatusService(
			SERVICE_RUNNING,
			SERVICE_STOPPED,
			[this, service_stop]
			{
				service_stop();
				_waitStatusService(SERVICE_RUNNING, SERVICE_STOPPED, service_stop);
			}
		);

		_waitStatusService(
			SERVICE_STOP_PENDING,
			SERVICE_STOPPED,
			[this, service_stop]
			{
				service_stop();
				_waitStatusService(SERVICE_STOP_PENDING, SERVICE_STOPPED, service_stop);
			}
		);
	}

	update();

	if ((stopped && config.sc_status.dwCurrentState == SERVICE_STOPPED) || !sc)
		InputConsole::textOk(Localization::Str{ "str_success_stop_service" }(), _name);
}

void Service::remove()
{
	CRITICAL_SECTION_RAII(lock);

	if (sc)
	{
		stop();
		DeleteService(sc);
		CloseServiceHandle(sc);
		sc = nullptr;
	}
}

void Service::close()
{
	CRITICAL_SECTION_RAII(lock);

	if (_sc_manager)
	{
		CloseServiceHandle(_sc_manager);
		_sc_manager = nullptr;
	}

	CloseServiceHandle(sc);
	sc				 = nullptr;
	config.sc_status = {};
}

void Service::allService(std::function<void(std::string)>&& callback)
{
	static CriticalSection lock;

	CRITICAL_SECTION_RAII(lock);

	if (!_sc_manager)
		_sc_manager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);

	if (_sc_manager)
	{
		LPENUM_SERVICE_STATUSA service_all	  = nullptr;
		DWORD				   buffer_size	  = 0;
		DWORD				   service_needed = 0;

		while (true)
		{
			DWORD service_count = 0;

			if (EnumServicesStatus(_sc_manager, SERVICE_WIN32, SERVICE_STATE_ALL, service_all, buffer_size, &service_needed, &service_count, nullptr))
			{
				if (service_all)
				{
					for (u32 i = 0; i < service_count; i++)
					{
						std::string name{ service_all[i].lpServiceName };
						callback(name.size() > 0 ? name : service_all[i].lpDisplayName);
					}

					free(service_all);
				}
				break;
			}

			u32 err = GetLastError();
			if (ERROR_MORE_DATA != err)
			{
				free(service_all);
				break;
			}

			buffer_size += service_needed;
			free(service_all);
			service_all = reinterpret_cast<LPENUM_SERVICE_STATUSA>(malloc(buffer_size));
		}
	}
}

std::list<std::string> Service::allService()
{
	std::list<std::string> list_service{};
	allService([&list_service](std::string service_name) { list_service.push_back(service_name); });
	return list_service;
}

void Service ::_waitStatusService(DWORD check_state, DWORD check_stat_end, std::function<void()>&& send_timeout_check)
{
	_dw_start_time = GetTickCount64();

	while (sc && config.sc_status.dwCurrentState == check_state)
	{
		_dw_wait_time = std::clamp<DWORD>(config.sc_status.dwWaitHint / 10, 1'000, 10'000);

		std::this_thread::sleep_for(std::chrono::milliseconds(_dw_wait_time));

		update();

		if (config.sc_status.dwCurrentState == check_stat_end)
			break;

		if ((GetTickCount64() - _dw_start_time) > _dw_timeout)
		{
			send_timeout_check();
			break;
		}
	}
}

void Service::_initScManager()
{
	CRITICAL_SECTION_RAII(lock);

	if (!_sc_manager)
		_sc_manager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);

	ASSERT_ARGS(_sc_manager, "Couldn't open SCManager to interact with system services!");
}
