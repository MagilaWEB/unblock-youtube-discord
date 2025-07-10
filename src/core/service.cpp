#include "pch.h"
#include "service.h"

Service::~Service()
{
	close();
}

void Service::create(pcstr description)
{
	if (sc)
	{
		Debug::error("Служба [%s] уже найдена или создана, повторно создать невозможно!", name);
		return;
	}

	_initScManager();

	auto sc_path = utils::format("\"%s\"", (Core::get().binariesPath() / file_name).string().c_str());

	std::string args{ " " };

	for (auto& arg : _args)
		args = args.append(arg).append(" ");

	sc_path.append(args);

	sc = CreateService(
		_sc_manager,
		name,
		description,
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

	if (!sc)
	{
		Debug::error("Не удалось создать службу [%s]!", name);
		return;
	}

	update();
}

void Service::setArgs(std::vector<std::string> args)
{
	_args = args;
}

void Service::start()
{
	update();

	if (sc_status.dwCurrentState != SERVICE_STOPPED && sc_status.dwCurrentState != SERVICE_STOP_PENDING)
		return;

	_waitStatusService(SERVICE_STOP_PENDING, SERVICE_STOPPED, [this] { remove(); });

	std::vector<pcstr> args;
	for (auto& arg : _args)
		args.push_back(arg.c_str());

	InputConsole::textPlease("подождите окончания запуска службы [%s]", true, false, name);

	const bool send_start = StartService(sc, args.size(), args.data());
	if (!send_start)
	{
		Debug::error("Не удалось отправить запрос на запуск службы [%s]!", name);
		return;
	}

	update();

	_waitStatusService(
		SERVICE_START_PENDING,
		SERVICE_RUNNING,
		[this]
		{
			InputConsole::textError("истекло время ожидания запуска службы [%s], процес будет остановден!", name);
			stop();
		}
	);

	_waitStatusService(
		SERVICE_CONTINUE_PENDING,
		SERVICE_RUNNING,
		[this]
		{
			InputConsole::textError("истекло время ожидания запуска службы [%s], процес будет остановден!", name);
			stop();
		}
	);

	if (sc_status.dwCurrentState == SERVICE_RUNNING)
		InputConsole::textOk("служба [%s] запущена.", name);
}

void Service::update()
{
	if (sc)
	{
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
}

void Service::waitRanning()
{
	if (sc_status.dwCurrentState != SERVICE_RUNNING)
	{
		_waitStatusService(
			SERVICE_START_PENDING,
			SERVICE_RUNNING,
			[]
			{
				
			}
		);
	}
}

void Service::open()
{
	_initScManager();

	if (_sc_manager)
	{
		if (!sc)
		{
			sc = OpenService(_sc_manager, name, SC_MANAGER_ALL_ACCESS);
			update();
		}
	}
}

void Service::stop()
{
	if (sc)
	{
		update();

		bool stoped = false;

		if (sc_status.dwCurrentState != SERVICE_STOPPED)
		{
			InputConsole::textPlease("подождите окончания остановки службы [%s]", true, false, name);

			auto service_stop = [this, &stoped]
			{
				stoped					= true;
				const bool send_control = ControlService(sc, SERVICE_CONTROL_STOP, reinterpret_cast<LPSERVICE_STATUS>(&sc_status));
				if (!send_control)
					Debug::error("Не удалость отправить запрост на остановку службы [%s]!", name);
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

		if (stoped && sc_status.dwCurrentState == SERVICE_STOPPED)
			InputConsole::textOk("служба [%s] остановлена.", name);
	}
}

void Service::remove()
{
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
	if (!_sc_manager)
		_sc_manager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);

	if (!_sc_manager)
		InputConsole::textError("Не удалось открыть SCManager для взаимодействия с службами системы!");
}
