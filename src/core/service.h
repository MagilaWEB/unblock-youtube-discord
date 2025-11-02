#pragma once
#include "winsvc.h"

class CORE_API Service final
{
	const static inline ULONGLONG _dw_timeout{ 30'000 };
	static inline SC_HANDLE		  _sc_manager{ nullptr };

	CriticalSection lock;

	std::string				 _name{ "" };
	std::string				 _description{ "" };
	std::vector<std::string> _args;
	DWORD					 _dw_bytes_needed{ 0 };
	ULONGLONG				 _dw_start_time{ GetTickCount64() };
	ULONGLONG				 _dw_wait_time{ 0 };

public:
	
	const std::filesystem::path file_name;
	Service(const pcstr name) : _name(name), file_name(std::filesystem::path("")) {}
	Service(const pcstr name, pcstr _file_name) : _name(name), file_name(std::filesystem::path(_file_name)) {}
	~Service();

	SC_HANDLE			   sc{ nullptr };
	SERVICE_STATUS_PROCESS sc_status{};

	void setName(std::string new_name);
	void setDescription(pcstr description);
	std::string getName() const;

	bool isRun();

	void create();
	void setArgs(std::vector<std::string> args);
	void start();
	void update();

	void open();

	void stop();

	void remove();

	void close();

private:
	void _initScManager();
	void _waitStatusService(DWORD check_state, DWORD check_stat_end, std::function<void()>&& fn);
};
