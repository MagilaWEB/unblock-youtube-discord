export module core.service;

import ext.windows;
import core.lock_thread;
import core.timer;
import core.types;

/** A snapshot of a failed service operation: what was attempted, what the SCM
 *  and win32 reported and how long the process lived. Filled on every failure
 *  in create()/start() and read by the diagnostics module. */
export struct ServiceFailure
{
	enum class Stage : u8
	{
		Create,
		Open,
		Start,
		WaitRunning,
		UnexpectedStop,
	};

	std::string service_name;
	std::string binary_path;	// exe the Service was configured with
	Stage		stage{ Stage::Start };
	u32			win32_error{ 0 };
	u32			win32_exit_code{ 0 };
	u32			specific_exit_code{ 0 };
	u32			state{ 0 };	   // dwCurrentState when the failure was recorded
	u32			checkpoint{ 0 };
	u64			start_tick_ms{ 0 };
	u64			lived_ms{ 0 };
	bool		created{ false };	 // the service was created during this attempt

	[[nodiscard]] bool valid() const;
};

export class Service final
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
		_dw_wait_time(other._dw_wait_time),
		_last_failure(std::move(other._last_failure)),
		_was_created(other._was_created),
		_start_tick_ms(other._start_tick_ms)
	{
	}
	~Service();

	void setName(std::string new_name);
	void setDescription(std::string_view description);
	void setArgs(std::vector<std::string> args);

	[[nodiscard]] std::string	getName() const;
	[[nodiscard]] const Config& getConfig();
	[[nodiscard]] bool			isRun();

	/** Snapshot of the last failure, empty (see valid()) when the last
	 *  operation succeeded. */
	[[nodiscard]] const ServiceFailure& getLastFailure() const;
	void								clearLastFailure();

	[[nodiscard]] std::vector<std::string> getArgs();
	[[nodiscard]] std::filesystem::path	   getFilePath() const;

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
	// Outcome of waiting for a starting service.
	enum class StartOutcome : u8
	{
		Running,
		Stopped,	// the process died or was stopped before reporting running
		TimedOut,
		Unknown,
	};

	void		 _initScManager();
	void		 _waitStatusService(DWORD check_state, DWORD check_stat_end, std::function<void()>&& on_timeout = [] {});
	StartOutcome _waitStarted();
	void		 _recordFailure(ServiceFailure::Stage stage, u32 win32_error);
	void		 _refreshCachedRunning();

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

	ServiceFailure _last_failure{};
	bool		   _was_created{ false };
	u64			   _start_tick_ms{ 0 };

	static constexpr DWORD	c_dw_timeout_ms	   = 30'000;
	static constexpr DWORD	c_create_retry_ms  = 300;
	static constexpr DWORD	c_start_retry_ms   = 300;
	static constexpr DWORD	c_open_retry_ms	   = 5;
	static constexpr size_t c_max_open_retries = 3;
};
