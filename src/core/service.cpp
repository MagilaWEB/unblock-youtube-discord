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

void Service::setDescription(pcstr description)
{
	CRITICAL_SECTION_RAII(lock);

	_description = description;
}

std::string Service::getName() const
{
	return _name;
}

bool Service::isRun()
{
	CRITICAL_SECTION_RAII(lock);

	update();

	return sc_status.dwCurrentState == SERVICE_START_PENDING || sc_status.dwCurrentState == SERVICE_RUNNING;
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
		sc_path.c_str(),
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

	if (sc_status.dwCurrentState != SERVICE_STOPPED && sc_status.dwCurrentState != SERVICE_STOP_PENDING)
		return;

	_waitStatusService(SERVICE_STOP_PENDING, SERVICE_STOPPED, [this] { remove(); });

	std::vector<pcstr> args;
	for (auto& arg : _args)
		args.push_back(arg.c_str());

	InputConsole::textPlease("подождите окончания запуска службы [%s]", true, false, _name.c_str());

	bool send_start = false;
	u32	 i_start	= 0;
	do
	{
		send_start = StartService(sc, static_cast<u32>(args.size()), args.data());
		ASSERT_ARGS(i_start++ <= 5, "Failed to send a request to start the service [%s]!", _name.c_str());
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

	if (sc_status.dwCurrentState == SERVICE_RUNNING)
		InputConsole::textOk("служба [%s] запущена.", _name.c_str());
}

void Service::update()
{
	if (!sc)
		return;

	bool q_service =
		QueryServiceStatusEx(sc, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&sc_status), sizeof(SERVICE_STATUS_PROCESS), &_dw_bytes_needed);

	if (!q_service)
	{
		CloseServiceHandle(sc);
		sc				 = nullptr;
		sc_status		 = {};
		_dw_bytes_needed = 0;
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

	if (sc_status.dwCurrentState != SERVICE_STOPPED)
	{
		InputConsole::textPlease("подождите окончания остановки службы [%s]", true, false, _name.c_str());

		auto service_stop = [this, &stopped]
		{
			stopped					= true;
			const bool send_control = ControlService(sc, SERVICE_CONTROL_STOP, reinterpret_cast<LPSERVICE_STATUS>(&sc_status));
			ASSERT_ARGS(send_control, "Failed to send a request to stop the service [%s]!", _name.c_str());
		};

		service_stop();

		_waitStatusService(
			SERVICE_STOP_PENDING,
			SERVICE_STOPPED,
			[&]
			{
				service_stop();
				_waitStatusService(SERVICE_STOP_PENDING, SERVICE_STOPPED, service_stop);
			}
		);
	}

	update();

	if (stopped && sc_status.dwCurrentState == SERVICE_STOPPED)
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
	sc_status		 = {};
	_dw_bytes_needed = 0;
}

void Service ::_waitStatusService(DWORD check_state, DWORD check_stat_end, std::function<void()>&& send_timeout_check)
{
	_dw_start_time = GetTickCount64();

	while (sc_status.dwCurrentState == check_state)
	{
		_dw_wait_time = std::clamp<DWORD>(sc_status.dwWaitHint / 10, 1'000, 10'000);

		std::this_thread::sleep_for(std::chrono::milliseconds(_dw_wait_time));

		update();

		if (sc_status.dwCurrentState == check_stat_end)
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

	ASSERT_ARGS(_sc_manager, "Couldn't open SCManager to interact with system services!", _name.c_str());
}
