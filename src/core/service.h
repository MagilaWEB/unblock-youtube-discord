#pragma once
#include "winsvc.h"

class CORE_API Service final
{
	constexpr static ULONGLONG _dw_timeout{ 30'000 };
	static inline SC_HANDLE	   _sc_manager{ nullptr };

	CriticalSection lock;

	std::string				 _name{ "" };
	std::string				 _description{ "" };
	std::vector<std::string> _args;
	ULONGLONG				 _dw_start_time{ GetTickCount64() };
	ULONGLONG				 _dw_wait_time{ 0 };

protected:
	struct Config
	{
		u32 type{ 0 };
		u32 start_type{ 0 };
		u32 tag_id{ 0 };

		std::string			  binary_path{ "" };
		std::string			  start_name{ "" };
		std::string			  display_name{ "" };
		std::string			  load_order_group{ "" };

		SERVICE_STATUS_PROCESS sc_status{};
	};

	Config config{};

public:
	const std::filesystem::path file_name;
	Service(const pcstr name) : _name(name), file_name(std::filesystem::path("")) {}
	Service(const pcstr name, pcstr _file_name) : _name(name), file_name(std::filesystem::path(_file_name)) {}
	~Service();

	SC_HANDLE sc{ nullptr };

	void		  setName(std::string new_name);
	void		  setDescription(pcstr description);
	std::string	  getName() const;
	const Config& getConfig();

	bool isRun();

	void create();
	void setArgs(std::vector<std::string> args);
	void start();
	void update();

	void open();

	void stop();

	void remove();

	void close();

	static void					  allService(std::function<void(std::string)>&& callback);
	static std::list<std::string> allService();

private:
	void _initScManager();
	void _waitStatusService(DWORD check_state, DWORD check_stat_end, std::function<void()>&& fn);
};
