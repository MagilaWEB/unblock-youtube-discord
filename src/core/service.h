#pragma once
#include "timer.h"
#include "winsvc.h"

#include <atomic>

class Service final
{
public:
	struct Config
	{
		std::string binary_path;
		std::string start_name;
		std::string display_name;
		std::string load_order_group;

		u32 type{ 0 };
		u32 start_type{ 0 };
		u32 tag_id{ 0 };

		SERVICE_STATUS_PROCESS sc_status{};
	};

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
	Service(const std::string_view name, std::string_view name_file) : _name(name), _file_name(std::filesystem::path(name_file)) {}
	Service(Service&& other) noexcept :
		_cached_running(other._cached_running.load(std::memory_order_relaxed)),
		_name(std::move(other._name)),
		_description(std::move(other._description)),
		_args(std::move(other._args)),
		_file_name(std::move(other._file_name)),
		_sc_manager(std::move(other._sc_manager)),
		_sc(std::move(other._sc)),
		_config(std::move(other._config)),
		_time_limit(other._time_limit),
		_dw_start_time(other._dw_start_time),
		_dw_wait_time(other._dw_wait_time)
	{
	}
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
	void _refreshCachedRunning();

	CriticalSection _lock{};

	// Last known running state, refreshed under _lock by update(). isRun()
	// returns it without blocking when a stop/start holds the lock for
	// seconds, so the UI tick never stalls the window on a service call.
	std::atomic<bool> _cached_running{ false };

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
