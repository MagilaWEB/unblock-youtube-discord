#include "service.h"
#include "timer.h"

Service::~Service()
{
	close();
}

void Service::setName(std::string new_name)
{
	CRITICAL_SECTION_RAII(lock);

	_name = new_name;
}

void Service::setDescription(pcstr description)
{
	CRITICAL_SECTION_RAII(lock);

	_description = description;
}

std::string Service::getName() const
{
	return _name;
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

	ASSERT_ARGS(!sc, "The service [%s] has already been found or created, it cannot be recreated!", _name.c_str());

	_initScManager();

	std::string sc_path{ "\"\"" };
	sc_path.insert(1, (Core::get().binariesPath() / file_name).string());

	std::string args{ " " };

	for (auto& arg : _args)
		args = args.append(arg).append(" ");

	sc_path.append(args);

	sc = CreateService(
		_sc_manager,
		utils::UTF8_to_CP1251(_name.c_str()).c_str(),
		utils::UTF8_to_CP1251(_description.c_str()).c_str(),
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

	open();

	ASSERT_ARGS(sc, "Service could not be created [%s]!", _name.c_str());

	update();
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
		InputConsole::textError("служба [%s] не остановлена или в прогрессе остановки, запустить не возможно!", _name.c_str());
		return;
	}

	//_waitStatusService(SERVICE_STOP_PENDING, SERVICE_STOPPED, [this] { remove(); });

	std::vector<pcstr> args;
	for (auto& arg : _args)
		args.push_back(arg.c_str());

	InputConsole::textPlease("подождите окончания запуска службы [%s]", true, false, _name.c_str());

	bool send_start = false;

	Timer timer;
	timer.start();

	do
	{
		send_start = StartService(sc, static_cast<u32>(args.size()), args.data());

		// We try for 5 seconds, otherwise we interrupt.
		if (timer.getElapsed_sec() > 5.f)
		{
			InputConsole::textError("истекло время ожидания запуска службы [%s], процесс запуска прерван, причина не известна!", _name.c_str());
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
			InputConsole::textError("истекло время ожидания запуска службы [%s], процесс будет остановлен!", _name.c_str());
			stop();
		}
	);

	_waitStatusService(
		SERVICE_CONTINUE_PENDING,
		SERVICE_RUNNING,
		[this]
		{
			InputConsole::textError("истекло время ожидания запуска службы [%s], процесс будет остановлен!", _name.c_str());
			stop();
		}
	);

	if (config.sc_status.dwCurrentState == SERVICE_RUNNING)
		InputConsole::textOk("служба [%s] запущена.", _name.c_str());
	else
		InputConsole::textWarning("служба [%s] была запущена но неожиданно сменила свой статус.", _name.c_str());
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
			sc = OpenService(_sc_manager, _name.c_str(), SC_MANAGER_ALL_ACCESS);
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
		InputConsole::textPlease("подождите окончания остановки службы [%s]", true, false, _name.c_str());

		auto service_stop = [this, &stopped]
		{
			stopped					= true;
			const bool send_control = ControlService(sc, SERVICE_CONTROL_STOP, reinterpret_cast<LPSERVICE_STATUS>(&config.sc_status));
			ASSERT_ARGS(send_control, "Failed to send a request to stop the service [%s]!", _name.c_str());
		};

		service_stop();

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

	if (stopped && config.sc_status.dwCurrentState == SERVICE_STOPPED)
		InputConsole::textOk("служба [%s] остановлена.", _name.c_str());
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

	while (config.sc_status.dwCurrentState == check_state)
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
