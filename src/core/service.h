#pragma once
#include "timer.h"
#include "winsvc.h"

class CORE_API Service final
{
public:
	struct Config
	{
		u32 type{ 0 };
		u32 start_type{ 0 };
		u32 tag_id{ 0 };

		std::string binary_path;
		std::string start_name;
		std::string display_name;
		std::string load_order_group;

		SERVICE_STATUS_PROCESS sc_status{};
	};

	// RAII deleter for SC_HANDLE
	struct ScHandleDeleter
	{
		void operator()(SC_HANDLE h) const noexcept
		{
			if (h)
				CloseServiceHandle(h);
		}
	};
	using UniqueScHandle = std::unique_ptr<std::remove_pointer_t<SC_HANDLE>, ScHandleDeleter>;

	Service(const std::string_view name) : _name(name), _file_name(std::filesystem::path("")) {}
	Service(const std::string_view name, std::string_view _file_name) : _name(name), _file_name(std::filesystem::path(_file_name)) {}
	Service(Service&&) = default;
	~Service();

	void setName(std::string new_name);
	void setDescription(std::string_view description);
	void setArgs(std::vector<std::string> args);

	[[nodiscard]] std::string	getName() const;
	[[nodiscard]] const Config& getConfig();
	[[nodiscard]] bool			isRun();

	void create();
	void start();
	void stop();
	void remove();

	void update();
	void open();
	void close();

	static void						allService(std::function<void(std::string)>&& callback);
	static std::vector<std::string> allService();

private:
	void _initScManager();
	void _waitStatusService(DWORD check_state, DWORD check_stat_end, std::function<void()>&& on_timeout = [] {});

	CriticalSection _lock{};

	std::string				 _name;
	std::string				 _description;
	std::vector<std::string> _args;
	std::filesystem::path	 _file_name;

	UniqueScHandle _sc_manager;
	UniqueScHandle _sc;
	Config		   _config;

	Timer	  _time_limit;
	ULONGLONG _dw_start_time{ 0 };
	ULONGLONG _dw_wait_time{ 0 };

	static constexpr DWORD	_dw_timeout_ms	  = 30'000;
	static constexpr DWORD	_create_retry_ms  = 300;
	static constexpr DWORD	_start_retry_ms	  = 300;
	static constexpr DWORD	_open_retry_ms	  = 5;
	static constexpr size_t _max_open_retries = 3;
};
