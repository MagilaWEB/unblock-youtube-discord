export module core.service_diagnostics;

import std;
import core.event_log;
import core.service;
import core.types;

/** One named check with its outcome. */
export struct DiagnosticCheck
{
	enum class Level : u8
	{
		Ok,
		Warning,
		Failed,
		Skipped,
	};

	Level		level{ Level::Skipped };
	std::string text;
};

/** The user facing result of a service failure diagnosis. */
export struct DiagnosticReport
{
	std::string					 service;			 // the failing service name (issue title)
	std::string					 verdict;			 // one short line for the UI status bar
	std::string					 summary;			 // what happened, 1-3 paragraphs
	std::vector<DiagnosticCheck> checks;			 // the checklist shown in the window
	std::vector<std::string>	 recommendations;	 // numbered steps for the user
	std::vector<std::string>	 raw;				 // codes, paths, log excerpts

	[[nodiscard]] bool		  empty() const;
	[[nodiscard]] std::string fullText() const;

	/** Minimal cause description for a GitHub issue: just the verdict and the
	 *  summary (failure stage, Win32 and exit codes, lifetime). The complete
	 *  report — checklist, recommendations and raw evidence — is attached as
	 *  logs/service-report.txt. */
	[[nodiscard]] std::string issueText() const;
};

/** Everything the verdict needs, gathered from the system. A plain struct, so
 *  buildReport() stays callable (and unit testable) without a single
 * Win32 call:
 *  the collector fills it, the verdict logic only reads it. */
export struct CollectedEvidence
{
	// Files
	bool					 binary_exists{ false };
	bool					 binary_blocked{ false };
	std::vector<std::string> missing_files;
	std::vector<std::string> blocked_files;
	bool					 windivert_checked{ false };
	bool					 windivert_dll_exists{ false };
	bool					 windivert_sys_exists{ false };
	bool					 log_dir_writable{ false };
	std::string				 log_dir;

	// Registration and driver (registration_known is false when the SCM could
	// not be asked at all, kept apart from "the service is not registered")
	bool		registration_known{ false };
	u32			start_type{ 0 };
	std::string registered_command;
	std::string expected_binary;
	bool		windivert_driver_known{ false };
	bool		windivert_driver_running{ false };

	// Environment
	bool					 elevated{ false };
	std::string				 install_path;
	std::vector<std::string> conflicting_processes;
	std::vector<std::string> path_issues;

	// Event log
	std::vector<EventLogRecord> scm_events;
	std::vector<EventLogRecord> defender_events;

	// Active probe
	bool					 probe_attempted{ false };
	bool					 probe_started{ false };
	bool					 probe_timed_out{ false };
	u32						 probe_exit_code{ 0 };
	std::string				 probe_out;
	std::string				 probe_err;
	std::vector<std::string> probe_args;	// the registered command line, split for the probe
};

/** Turns a failure snapshot plus the collected evidence into a report. Pure:
 *  no Win32 call, no environment access. */
export DiagnosticReport buildReport(const ServiceFailure& failure, const CollectedEvidence& evidence);

export class ServiceDiagnostics
{
public:
	/** Diagnoses a failed service run. allow_probe also runs the service binary
	 *  as a plain process with captured stdout/stderr, the strongest
	 * evidence
	 *  for a crash. Returns an empty report when there is nothing to diagnose. */
	static DiagnosticReport diagnose(std::span<const ServiceFailure> failures, bool allow_probe);
};
