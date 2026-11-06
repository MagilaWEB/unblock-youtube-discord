module core.service_diagnostics;

import ext.windows;
import core;
import core.debug;
import core.event_log;
import core.hidden_process;
import core.localization;
import core.service;
import core.types;
import core.utils;

namespace
{
	constexpr u32	 c_probe_timeout_ms = 3'000;
	constexpr size_t c_raw_clip			= 600;

	// ERROR_FAILED_SERVICE_CONTROLLER_CONNECT: a service image run outside the
	// SCM exits with exactly this code, so the probe verdict would be a lie.
	constexpr u32 c_error_failed_service_controller_connect = 1'063;

	std::string localized(pcstr key)
	{
		return Localization::Str{ key }();
	}

	std::string hex(u32 value)
	{
		return std::format("0x{:08X}", value);
	}

	// One line, no control characters: the value ends up in the UI and in a log.
	std::string clip(std::string_view text, size_t limit = c_raw_clip)
	{
		std::string line;
		line.reserve(text.size());

		for (const char ch : text)
			line += (ch == '\r' || ch == '\n' || ch == '\t') ? ' ' : ch;

		while (line.contains("  "))
			line.replace(line.find("  "), 2, " ");

		std::string trimmed = line;
		while (!trimmed.empty() && trimmed.front() == ' ')
			trimmed.erase(trimmed.begin());

		return trimmed.size() <= limit ? trimmed : trimmed.substr(0, limit) + "...";
	}

	std::string join(const std::vector<std::string>& items, std::string_view separator)
	{
		std::string out;
		for (const auto& item : items)
		{
			if (!out.empty())
				out += separator;
			out += item;
		}
		return out;
	}

	std::string toLower(std::string_view text)
	{
		std::string out{ text };
		std::ranges::transform(out, out.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
		return out;
	}

	bool iContains(std::string_view haystack, std::string_view needle)
	{
		return toLower(haystack).contains(toLower(needle));
	}

	std::string stageText(ServiceFailure::Stage stage)
	{
		switch (stage)
		{
		case ServiceFailure::Stage::Create:
			return localized("str_diag_stage_create");
		case ServiceFailure::Stage::Open:
			return localized("str_diag_stage_open");
		case ServiceFailure::Stage::Start:
			return localized("str_diag_stage_start");
		case ServiceFailure::Stage::WaitRunning:
			return localized("str_diag_stage_wait_running");
		case ServiceFailure::Stage::UnexpectedStop:
			return localized("str_diag_stage_unexpected_stop");
		}

		return localized("str_diag_code_unknown");
	}

	// Known failure codes from the SCM, win32 and the NT exception set. The NT
	// values are the documented exception codes, the SCM ones are the ones the
	// service manager reports in the event log and in dwWin32ExitCode.
	std::string codeText(u32 code)
	{
		switch (code)
		{
		case 0xC0'00'01'35:
			return localized("str_diag_code_dll_not_found");
		case 0xC0'00'01'42:
			return localized("str_diag_code_dll_init_failed");
		case 0xC0'00'00'05:
			return localized("str_diag_code_access_violation");
		case 0xC0'00'00'FD:
			return localized("str_diag_code_stack_overflow");
		case 1'066:
			return localized("str_diag_code_service_specific");
		case 1'053:
			return localized("str_diag_code_service_timeout");
		case 1'058:
			return localized("str_diag_code_service_disabled");
		case 1'067:
			return localized("str_diag_code_process_exited");
		case 1'069:
			return localized("str_diag_code_logon_failed");
		case 1'072:
			return localized("str_diag_code_stop_pending");
		case 1'075:
			return localized("str_diag_code_dependency_missing");
		case 1'083:
			return localized("str_diag_code_bad_account");
		case 1'084:
			return localized("str_diag_code_critical_startup");
		case 5:
			return localized("str_diag_code_access_denied");
		case 2:
			return localized("str_diag_code_file_not_found");
		default:
			return localized("str_diag_code_unknown");
		}
	}
}	 // namespace

bool DiagnosticReport::empty() const
{
	return verdict.empty() && summary.empty() && checks.empty();
}

std::string DiagnosticReport::fullText() const
{
	std::string text = verdict;

	if (!summary.empty())
		text += "\n\n" + summary;

	if (!checks.empty())
	{
		text += "\n";
		for (const auto& check : checks)
		{
			const char* tag = "SKIPPED";
			if (check.level == DiagnosticCheck::Level::Ok)
				tag = "OK";
			else if (check.level == DiagnosticCheck::Level::Warning)
				tag = "WARNING";
			else if (check.level == DiagnosticCheck::Level::Failed)
				tag = "FAILED";

			text += "\n[" + std::string{ tag } + "] " + check.text;
		}
	}

	if (!recommendations.empty())
	{
		text += "\n\n" + localized("str_diag_section_recommendations");
		for (const auto& recommendation : recommendations)
			text += "\n" + recommendation;
	}

	if (!raw.empty())
	{
		text += "\n\n" + localized("str_diag_section_raw");
		for (const auto& line : raw)
			text += "\n" + line;
	}

	return text;
}

std::string DiagnosticReport::issueText() const
{
	// Minimal cause description for a GitHub issue: just the verdict and the
	// summary (failure stage, Win32 code, process exit code, lifetime). The
	// complete report — checklist, recommendations and raw evidence — is
	// written to logs/service-report.txt, which the user attaches.
	std::string text = verdict;

	if (!summary.empty())
		text += "\n\n" + summary;

	return text;
}

DiagnosticReport buildReport(const ServiceFailure& failure, const CollectedEvidence& evidence)
{
	DiagnosticReport report;

	// The verdict is chosen by the strongest piece of evidence; everything that
	// is merely suspicious only fills the checklist.
	enum class Cause
	{
		Probe,
		Scm,
		ExitCode,
		Antivirus,
		Disabled,
		Registration,
		Driver,
		Conflict,
		Rights,
		Path,
		None,
	};

	auto rank = [](Cause cause)
	{
		switch (cause)
		{
		case Cause::Probe:
			return 0;
		case Cause::Scm:
			return 1;
		case Cause::ExitCode:
			return 2;
		case Cause::Antivirus:
			return 3;
		case Cause::Disabled:
			return 4;
		case Cause::Registration:
			return 5;
		case Cause::Driver:
			return 6;
		case Cause::Conflict:
			return 7;
		case Cause::Rights:
			return 8;
		case Cause::Path:
			return 9;
		case Cause::None:
			return 10;
		}
		return 10;
	};

	Cause cause	  = Cause::None;
	auto  promote = [&cause, &rank](Cause candidate)
	{
		if (rank(candidate) < rank(cause))
			cause = candidate;
	};

	auto add_check = [&report](DiagnosticCheck::Level level, std::string text) { report.checks.emplace_back(level, std::move(text)); };

	auto add_recommendation = [&report](std::string text)
	{ report.recommendations.push_back(std::format("{}. {}", report.recommendations.size() + 1, text)); };

	// --- Raw data ------------------------------------------------------------
	report.raw.push_back(std::format("service={}", failure.service_name));
	report.raw.push_back(std::format("binary={}", failure.binary_path));
	report.raw.push_back(std::format("stage={}", stageText(failure.stage)));
	report.raw.push_back(std::format("win32_error={} ({})", failure.win32_error, hex(failure.win32_error)));
	report.raw.push_back(std::format("win32_exit_code={} ({})", failure.win32_exit_code, hex(failure.win32_exit_code)));
	report.raw.push_back(std::format("specific_exit_code={} ({})", failure.specific_exit_code, hex(failure.specific_exit_code)));
	report.raw.push_back(std::format("service_state={}", failure.state));
	report.raw.push_back(std::format("checkpoint={}", failure.checkpoint));
	report.raw.push_back(std::format("lived_ms={}", failure.lived_ms));
	report.raw.push_back(std::format("created_during_this_run={}", failure.created));

	if (!evidence.install_path.empty())
		report.raw.push_back(std::format("install_path={}", evidence.install_path));
	if (evidence.registration_known)
		report.raw.push_back(std::format("registered_command={}", evidence.registered_command));
	for (const auto& file : evidence.missing_files)
		report.raw.push_back(std::format("missing_file={}", file));
	for (const auto& file : evidence.blocked_files)
		report.raw.push_back(std::format("blocked_file={}", file));
	for (const auto& process : evidence.conflicting_processes)
		report.raw.push_back(std::format("conflicting_process={}", process));
	for (const auto& issue : evidence.path_issues)
		report.raw.push_back(std::format("path_issue={}", issue));
	for (const auto& event : evidence.scm_events)
		report.raw.push_back(
			std::format("scm_event id={} time={} msg={}", event.id, event.time_utc_ms, clip(event.message.empty() ? event.xml : event.message))
		);
	for (const auto& event : evidence.defender_events)
		report.raw.push_back(
			std::format("defender_event id={} time={} msg={}", event.id, event.time_utc_ms, clip(event.message.empty() ? event.xml : event.message))
		);
	if (evidence.probe_attempted)
	{
		report.raw.push_back(
			std::format("probe_started={} timed_out={} exit_code={}", evidence.probe_started, evidence.probe_timed_out, evidence.probe_exit_code)
		);
		if (!evidence.probe_out.empty())
			report.raw.push_back("probe_stdout=" + clip(evidence.probe_out));
		if (!evidence.probe_err.empty())
			report.raw.push_back("probe_stderr=" + clip(evidence.probe_err));
	}

	// --- Codes ---------------------------------------------------------------
	if (failure.win32_exit_code != 0)
	{
		add_check(
			DiagnosticCheck::Level::Failed,
			utils::format(localized("str_diag_check_exit_code"), hex(failure.win32_exit_code), codeText(failure.win32_exit_code))
		);
	}
	else
	{
		add_check(DiagnosticCheck::Level::Ok, localized("str_diag_check_exit_code_ok"));
	}

	if (failure.specific_exit_code != 0)
		add_check(
			DiagnosticCheck::Level::Failed,
			utils::format(localized("str_diag_check_specific_code"), failure.specific_exit_code, codeText(failure.specific_exit_code))
		);

	if (failure.win32_error != 0)
		add_check(
			DiagnosticCheck::Level::Failed,
			utils::format(localized("str_diag_check_win32_error"), hex(failure.win32_error), codeText(failure.win32_error))
		);

	// An image-level crash code names the fault on its own (missing DLL, bad
	// DLL init, access violation, stack overflow), so it outranks the softer
	// signals below. The generic SCM codes (1067, and alike) do not: they only
	// say "the process is gone", so they stay a check.
	const bool crash_code = failure.win32_exit_code == 0xC0'00'01'35 || failure.win32_exit_code == 0xC0'00'01'42
						 || failure.win32_exit_code == 0xC0'00'00'05 || failure.win32_exit_code == 0xC0'00'00'FD;
	if (crash_code)
		promote(Cause::ExitCode);

	add_check(DiagnosticCheck::Level::Warning, utils::format(localized("str_diag_check_lifetime"), failure.lived_ms));

	// A file that is not on disk cannot be blamed by the probe: the probe ran a
	// command whose target is missing, so its exit code is noise. The
	// missing-file evidence below owns the verdict then.
	const bool file_gone = !evidence.binary_exists || !evidence.missing_files.empty();

	// The probe output is the only evidence that shows the actual crash text.
	const std::string probe_output = clip(evidence.probe_err.empty() ? evidence.probe_out : evidence.probe_err, 400);
	if (evidence.probe_attempted)
	{
		if (!evidence.probe_started)
		{
			add_check(DiagnosticCheck::Level::Warning, localized("str_diag_check_probe_failed"));
		}
		else if (evidence.probe_timed_out)
		{
			add_check(DiagnosticCheck::Level::Ok, localized("str_diag_check_probe_ok"));
			if (!file_gone)
				promote(Cause::Probe);
		}
		else if (evidence.probe_exit_code == c_error_failed_service_controller_connect)
		{
			// Nothing to conclude: the image is a service and refuses to run
			// outside the Service Control Manager.
			add_check(DiagnosticCheck::Level::Warning, localized("str_diag_check_probe_not_indicative"));
		}
		else if (evidence.probe_exit_code == 0)
		{
			add_check(DiagnosticCheck::Level::Warning, localized("str_diag_check_probe_zero"));
			if (!file_gone)
				promote(Cause::Probe);
		}
		else
		{
			add_check(DiagnosticCheck::Level::Failed, utils::format(localized("str_diag_check_probe_exit"), hex(evidence.probe_exit_code)));
			if (!file_gone)
				promote(Cause::Probe);
		}

		if (!probe_output.empty())
			add_check(DiagnosticCheck::Level::Warning, utils::format(localized("str_diag_check_probe_output"), probe_output));
	}
	else
	{
		add_check(DiagnosticCheck::Level::Skipped, localized("str_diag_check_probe_skipped"));
	}

	// --- Service Control Manager log ----------------------------------------
	if (evidence.scm_events.empty())
	{
		add_check(DiagnosticCheck::Level::Ok, localized("str_diag_check_scm_empty"));
	}
	else
	{
		add_check(DiagnosticCheck::Level::Failed, utils::format(localized("str_diag_check_scm_found"), evidence.scm_events.size()));
		for (const auto& event : evidence.scm_events)
			add_check(
				DiagnosticCheck::Level::Warning,
				utils::format(localized("str_diag_check_event"), event.id, clip(event.message.empty() ? event.xml : event.message, 400))
			);
		promote(Cause::Scm);
	}

	// --- Defender log -------------------------------------------------------
	bool defender_blocked = false;
	for (const auto& event : evidence.defender_events)
		if (!evidence.install_path.empty() && (event.message.contains(evidence.install_path) || event.xml.contains(evidence.install_path)))
			defender_blocked = true;

	if (evidence.defender_events.empty())
	{
		add_check(DiagnosticCheck::Level::Ok, localized("str_diag_check_defender_empty"));
	}
	else
	{
		add_check(
			defender_blocked ? DiagnosticCheck::Level::Failed : DiagnosticCheck::Level::Warning,
			utils::format(localized("str_diag_check_defender_found"), evidence.defender_events.size())
		);
		for (const auto& event : evidence.defender_events)
			add_check(
				DiagnosticCheck::Level::Warning,
				utils::format(localized("str_diag_check_event"), event.id, clip(event.message.empty() ? event.xml : event.message, 400))
			);

		if (defender_blocked)
		{
			add_check(DiagnosticCheck::Level::Failed, localized("str_diag_check_defender_blocked"));
			promote(Cause::Antivirus);
		}
	}

	// --- Files --------------------------------------------------------------
	if (evidence.binary_exists)
		add_check(DiagnosticCheck::Level::Ok, utils::format(localized("str_diag_check_binary_exists"), failure.binary_path));
	else
		add_check(DiagnosticCheck::Level::Failed, utils::format(localized("str_diag_check_binary_missing"), failure.binary_path));

	if (evidence.missing_files.empty())
		add_check(DiagnosticCheck::Level::Ok, localized("str_diag_check_files_ok"));
	else
		add_check(DiagnosticCheck::Level::Failed, utils::format(localized("str_diag_check_files_missing"), join(evidence.missing_files, ", ")));

	if (evidence.binary_blocked || !evidence.blocked_files.empty())
	{
		std::vector<std::string> blocked = evidence.blocked_files;
		if (evidence.binary_blocked)
			blocked.insert(blocked.begin(), failure.binary_path);
		add_check(DiagnosticCheck::Level::Failed, utils::format(localized("str_diag_check_blocked"), join(blocked, ", ")));
	}

	if (evidence.windivert_checked)
	{
		if (evidence.windivert_dll_exists && evidence.windivert_sys_exists)
			add_check(DiagnosticCheck::Level::Ok, localized("str_diag_check_windivert_ok"));
		else if (!evidence.windivert_dll_exists)
			add_check(DiagnosticCheck::Level::Failed, utils::format(localized("str_diag_check_windivert_missing"), "WinDivert.dll"));
		else
			add_check(DiagnosticCheck::Level::Failed, utils::format(localized("str_diag_check_windivert_missing"), "WinDivert64.sys"));
	}

	add_check(
		evidence.log_dir_writable ? DiagnosticCheck::Level::Ok : DiagnosticCheck::Level::Failed,
		evidence.log_dir_writable ? localized("str_diag_check_log_dir_ok") : utils::format(localized("str_diag_check_log_dir_fail"), evidence.log_dir)
	);

	// Losing the service binary, its WinDivert files or a quarantine marker is
	// the classic antivirus signature: the file was there, now it is not.
	const bool windivert_lost = evidence.windivert_checked && (!evidence.windivert_dll_exists || !evidence.windivert_sys_exists);
	if (file_gone || evidence.binary_blocked || !evidence.blocked_files.empty() || windivert_lost)
		promote(Cause::Antivirus);

	// --- Registration and driver -------------------------------------------
	if (evidence.registration_known)
	{
		if (evidence.start_type == SERVICE_DISABLED)
		{
			add_check(DiagnosticCheck::Level::Failed, localized("str_diag_check_service_disabled"));
			promote(Cause::Disabled);
		}

		if (!evidence.expected_binary.empty() && iContains(evidence.registered_command, evidence.expected_binary))
		{
			add_check(DiagnosticCheck::Level::Ok, localized("str_diag_check_registration_ok"));
		}
		else
		{
			add_check(
				DiagnosticCheck::Level::Failed,
				utils::format(localized("str_diag_check_registration_mismatch"), clip(evidence.registered_command, 300))
			);
			promote(Cause::Registration);
		}
	}

	if (evidence.windivert_checked)
	{
		if (evidence.windivert_driver_running)
			add_check(DiagnosticCheck::Level::Ok, localized("str_diag_check_driver_ok"));
		else if (evidence.windivert_driver_known)
		{
			add_check(DiagnosticCheck::Level::Failed, localized("str_diag_check_driver_stopped"));
			promote(Cause::Driver);
		}
		else
		{
			add_check(DiagnosticCheck::Level::Failed, localized("str_diag_check_driver_absent"));
			promote(Cause::Driver);
		}
	}

	// --- Environment --------------------------------------------------------
	if (evidence.conflicting_processes.empty())
		add_check(DiagnosticCheck::Level::Ok, localized("str_diag_check_no_conflicts"));
	else
	{
		add_check(DiagnosticCheck::Level::Failed, utils::format(localized("str_diag_check_conflicts"), join(evidence.conflicting_processes, ", ")));
		promote(Cause::Conflict);
	}

	add_check(
		evidence.elevated ? DiagnosticCheck::Level::Ok : DiagnosticCheck::Level::Failed,
		evidence.elevated ? localized("str_diag_check_elevated_ok") : localized("str_diag_check_elevated_fail")
	);
	if (!evidence.elevated)
		promote(Cause::Rights);

	if (evidence.path_issues.empty())
		add_check(DiagnosticCheck::Level::Ok, localized("str_diag_check_path_ok"));
	else
	{
		add_check(DiagnosticCheck::Level::Failed, utils::format(localized("str_diag_check_path_issue"), join(evidence.path_issues, ", ")));
		promote(Cause::Path);
	}

	// --- Verdict, summary and recommendations ------------------------------
	const std::string conflict_names = join(evidence.conflicting_processes, ", ");
	const std::string path_issues	 = join(evidence.path_issues, ", ");

	report.summary = utils::format(
		localized("str_diag_summary_failure"),
		failure.service_name,
		stageText(failure.stage),
		hex(failure.win32_error),
		hex(failure.win32_exit_code),
		failure.lived_ms
	);

	switch (cause)
	{
	case Cause::Probe:
		if (evidence.probe_timed_out || evidence.probe_exit_code == 0)
		{
			report.verdict	= localized("str_diag_verdict_probe_ok");
			report.summary += "\n\n" + localized("str_diag_summary_probe_ok");
			add_recommendation(localized("str_diag_rec_probe_ok_1"));
			add_recommendation(localized("str_diag_rec_probe_ok_2"));
		}
		else
		{
			report.verdict	= utils::format(localized("str_diag_verdict_probe_exit"), hex(evidence.probe_exit_code));
			report.summary += "\n\n" + utils::format(localized("str_diag_summary_probe_exit"), hex(evidence.probe_exit_code));
			report.summary += "\n"
							+ (probe_output.empty() ? localized("str_diag_summary_probe_no_output")
													: utils::format(localized("str_diag_summary_probe_output"), probe_output));
			add_recommendation(localized("str_diag_rec_probe_1"));
			add_recommendation(localized("str_diag_rec_probe_2"));
		}
		break;

	case Cause::Scm:
		report.verdict	= localized("str_diag_verdict_scm");
		report.summary += "\n\n" + localized("str_diag_summary_scm");
		add_recommendation(localized("str_diag_rec_scm_1"));
		add_recommendation(localized("str_diag_rec_scm_2"));
		break;

	case Cause::ExitCode:
		report.verdict = utils::format(localized("str_diag_verdict_exit_code"), hex(failure.win32_exit_code), codeText(failure.win32_exit_code));
		report.summary +=
			"\n\n" + utils::format(localized("str_diag_summary_exit_code"), hex(failure.win32_exit_code), codeText(failure.win32_exit_code));
		add_recommendation(localized("str_diag_rec_exit_1"));
		add_recommendation(localized("str_diag_rec_exit_2"));
		break;

	case Cause::Antivirus:
		report.verdict	= localized("str_diag_verdict_antivirus");
		report.summary += "\n\n" + localized("str_diag_summary_antivirus");
		add_recommendation(localized("str_diag_rec_antivirus_1"));
		add_recommendation(utils::format(localized("str_diag_rec_antivirus_2"), evidence.install_path));
		add_recommendation(localized("str_diag_rec_antivirus_3"));
		add_recommendation(localized("str_diag_rec_antivirus_4"));
		add_recommendation(localized("str_diag_rec_antivirus_5"));
		break;

	case Cause::Disabled:
		report.verdict	= localized("str_diag_verdict_disabled");
		report.summary += "\n\n" + localized("str_diag_summary_disabled");
		add_recommendation(localized("str_diag_rec_disabled_1"));
		break;

	case Cause::Registration:
		report.verdict	= localized("str_diag_verdict_registration");
		report.summary += "\n\n" + utils::format(localized("str_diag_summary_registration"), clip(evidence.registered_command, 300));
		add_recommendation(localized("str_diag_rec_registration_1"));
		add_recommendation(localized("str_diag_rec_registration_2"));
		break;

	case Cause::Driver:
		report.verdict	= localized("str_diag_verdict_driver");
		report.summary += "\n\n" + localized("str_diag_summary_driver");
		add_recommendation(localized("str_diag_rec_driver_1"));
		add_recommendation(localized("str_diag_rec_driver_2"));
		break;

	case Cause::Conflict:
		report.verdict	= utils::format(localized("str_diag_verdict_conflict"), conflict_names);
		report.summary += "\n\n" + utils::format(localized("str_diag_summary_conflict"), conflict_names);
		add_recommendation(utils::format(localized("str_diag_rec_conflict_1"), conflict_names));
		add_recommendation(localized("str_diag_rec_conflict_2"));
		break;

	case Cause::Rights:
		report.verdict	= localized("str_diag_verdict_rights");
		report.summary += "\n\n" + localized("str_diag_summary_rights");
		add_recommendation(localized("str_diag_rec_rights_1"));
		break;

	case Cause::Path:
		report.verdict	= localized("str_diag_verdict_path");
		report.summary += "\n\n" + utils::format(localized("str_diag_summary_path"), path_issues);
		add_recommendation(localized("str_diag_rec_path_1"));
		add_recommendation(localized("str_diag_rec_path_2"));
		break;

	case Cause::None:
		report.verdict	= localized("str_diag_verdict_unknown");
		report.summary += "\n\n" + localized("str_diag_summary_unknown");
		add_recommendation(localized("str_diag_rec_unknown_1"));
		add_recommendation(localized("str_diag_rec_unknown_2"));
		break;
	}

	return report;
}

namespace
{
	// --- Win32 helpers: they only fill CollectedEvidence, they decide nothing --
	std::string toUtf8(std::wstring_view wide)
	{
		if (wide.empty())
			return {};

		const int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
		if (length <= 0)
			return {};

		std::string out(static_cast<size_t>(length), '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), length, nullptr, nullptr);
		return out;
	}

	bool isElevated()
	{
		BOOL					 admin	   = FALSE;
		PSID					 admin_sid = nullptr;
		SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;

		if (!AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admin_sid))
			return false;

		const bool result = CheckTokenMembership(nullptr, admin_sid, &admin) && admin == TRUE;
		FreeSid(admin_sid);
		return result;
	}

	// A Mark-of-the-Web stream means the file came from the internet and is
	// still blocked; quarantine tools leave the same picture behind.
	bool isZoneBlocked(const std::filesystem::path& path)
	{
		return GetFileAttributesW((path.wstring() + L":Zone.Identifier").c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	bool isDirectoryWritable(const std::filesystem::path& directory)
	{
		std::error_code error;
		std::filesystem::create_directories(directory, error);
		if (!std::filesystem::is_directory(directory, error))
			return false;

		const auto probe = directory / "unblock_diag_write.tmp";
		{
			std::ofstream out(probe, std::ios::binary);
			if (!out)
				return false;
			out << 'x';
		}

		std::filesystem::remove(probe, error);
		return true;
	}

	std::vector<std::string> splitArguments(std::string_view command_line)
	{
		std::vector<std::string> args;
		std::string				 current;
		bool					 quoted = false;

		for (const char ch : command_line)
		{
			if (ch == '"')
			{
				quoted = !quoted;
				continue;
			}

			if (!quoted && (ch == ' ' || ch == '\t'))
			{
				if (!current.empty())
					args.push_back(std::exchange(current, {}));
				continue;
			}

			current += ch;
		}

		if (!current.empty())
			args.push_back(std::move(current));

		return args;
	}

	// A path has a separator (or a drive prefix) plus an extension. Plain values
	// (1.2.3.4, 80,443, host names) stay out of the file checks this way.
	bool looksLikePath(const std::string& token)
	{
		const size_t name_start = token.find_last_of("\\/");
		const auto	 name		= token.substr(name_start == std::string::npos ? 0 : name_start + 1);
		const bool	 separator	= token.contains('\\') || token.contains('/');
		const bool	 drive		= token.size() > 1 && std::isalpha(static_cast<unsigned char>(token[0])) && token[1] == ':';

		return (separator || drive) && name.contains('.');
	}

	// File references in a service command line: the payload exe, --config,
	// --hostlist/--ipset/--lua-init/--blob values and any '@'-prefixed path.
	std::vector<std::string> referencedPaths(std::string_view command_line, const std::filesystem::path& base)
	{
		std::vector<std::string> paths;

		for (std::string token : splitArguments(command_line))
		{
			if (token.starts_with("--"))
			{
				const size_t equals = token.find('=');
				if (equals == std::string::npos)
					continue;
				token = token.substr(equals + 1);
			}

			if (const size_t at = token.rfind('@'); at != std::string::npos)
				token = token.substr(at + 1);
			else if (!looksLikePath(token))
				continue;

			if (token.empty())
				continue;

			std::filesystem::path path{ token };
			if (path.is_relative())
				path = base / path;

			const std::string normalized = path.lexically_normal().string();
			if (!normalized.empty() && !std::ranges::contains(paths, normalized))
				paths.push_back(normalized);
		}

		return paths;
	}

	std::string fullPathOf(u32 process_id)
	{
		HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
		if (!process)
			return {};

		std::wstring buffer(1'024, L'\0');
		DWORD		 size = static_cast<DWORD>(buffer.size());
		const bool	 ok	  = QueryFullProcessImageNameW(process, 0, buffer.data(), &size);
		CloseHandle(process);

		if (!ok)
			return {};

		buffer.resize(size);
		return toUtf8(buffer);
	}

	bool isConflictName(std::string_view lower_name)
	{
		return lower_name == "winws.exe" || lower_name == "winws2.exe" || lower_name == "goodbyedpi.exe"
			|| (lower_name.starts_with("zapret") && lower_name.ends_with(".exe"));
	}

	std::vector<EventLogRecord> querySince(std::string_view channel, std::string_view xpath, u64 since_utc_ms, u32 max_records)
	{
		std::vector<EventLogRecord> records;
		for (auto& record : queryEventLog(channel, xpath, max_records))
		{
			// A record whose timestamp could not be parsed would slip past the
			// time filter and pull in months-old events. Drop it: unusable
			// time means unverifiable age.
			if (record.time_utc_ms == 0)
				continue;

			// Newest first: everything past the first older entry is older too.
			if (record.time_utc_ms < since_utc_ms)
				break;

			records.push_back(std::move(record));
		}
		return records;
	}

	u64 wallClockMs()
	{
		return static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
	}

	void checkEventLog(const ServiceFailure& failure, CollectedEvidence& evidence)
	{
		if (!eventLogAvailable())
		{
			Debug::warning("service diagnostics: event log is not available");
			return;
		}

		// The wall time of the attempt: the start tick is anchored to the clock
		// now, the process has been alive for lived_ms at most.
		const u64 now		 = wallClockMs();
		const u64 elapsed	 = failure.start_tick_ms == 0 ? 0 : GetTickCount64() - failure.start_tick_ms;
		const u64 start_wall = elapsed < now ? now - elapsed : now;

		const u64 scm_since = start_wall > 5'000 ? start_wall - 5'000 : 0;
		evidence.scm_events = querySince(
			"System",
			"*[System[Provider[@Name='Service Control Manager'] and (EventID=7000 or EventID=7009 or EventID=7024 or EventID=7031 or "
			"EventID=7034)]]",
			scm_since,
			8
		);

		// The query window is time based, so it also catches events of every
		// other service that failed around the same moment. Keep only the
		// entries that actually name this service, otherwise an unrelated SCM
		// record would produce a false verdict.
		std::erase_if(
			evidence.scm_events,
			[&failure](const EventLogRecord& record)
			{ return !iContains(record.message, failure.service_name) && !iContains(record.xml, failure.service_name); }
		);

		const u64 defender_since = start_wall > 60'000 ? start_wall - 60'000 : 0;
		evidence.defender_events =
			querySince("Microsoft-Windows-Windows Defender/Operational", "*[System[(EventID=1116 or EventID=1117)]]", defender_since, 8);
	}

	void checkFiles(const ServiceFailure& failure, CollectedEvidence& evidence)
	{
		const auto binaries = Core::get().binariesPath();

		evidence.log_dir		  = (Core::get().currentPath() / "logs").string();
		evidence.log_dir_writable = isDirectoryWritable(evidence.log_dir);

		evidence.binary_exists	= std::filesystem::exists(failure.binary_path);
		evidence.binary_blocked = evidence.binary_exists && isZoneBlocked(failure.binary_path);

		for (const auto& path : referencedPaths(evidence.registered_command, binaries))
			if (!std::filesystem::exists(path))
				evidence.missing_files.push_back(path);
			else if (isZoneBlocked(path))
				evidence.blocked_files.push_back(path);

		if (evidence.windivert_checked)
		{
			const auto dll = binaries / "WinDivert.dll";
			const auto sys = binaries / "WinDivert64.sys";

			evidence.windivert_dll_exists = std::filesystem::exists(dll);
			evidence.windivert_sys_exists = std::filesystem::exists(sys);

			for (const auto& file : { dll, sys })
				if (std::filesystem::exists(file) && isZoneBlocked(file))
					evidence.blocked_files.push_back(file.string());
		}
	}

	void checkRegistration(const ServiceFailure& failure, CollectedEvidence& evidence)
	{
		Service service{ std::string_view{ failure.service_name } };
		service.open();

		const Service::Config& config = service.getConfig();
		if (config.binary_path.empty())
			return;

		evidence.registration_known = true;
		evidence.registered_command = config.binary_path;
		evidence.start_type			= config.start_type;
		evidence.probe_args			= splitArguments(config.binary_path);
	}

	void checkDriver(CollectedEvidence& evidence)
	{
		Service driver{ "WinDivert" };
		driver.open();

		const Service::Config& config = driver.getConfig();
		if (config.binary_path.empty())
			return;

		evidence.windivert_driver_known	  = true;
		evidence.windivert_driver_running = config.sc_status.dwCurrentState == SERVICE_RUNNING;
	}

	void checkConflicts(CollectedEvidence& evidence)
	{
		const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snapshot == INVALID_HANDLE_VALUE)
			return;

		const std::string binaries = toLower(Core::get().binariesPath().string());

		PROCESSENTRY32W entry{};
		entry.dwSize = sizeof(entry);

		if (Process32FirstW(snapshot, &entry))
		{
			do
			{
				const std::string display = toUtf8(std::wstring_view{ entry.szExeFile });
				if (!isConflictName(toLower(display)))
					continue;

				// Our own copies live under binaries/ and are not a conflict.
				const std::string path = toLower(fullPathOf(entry.th32ProcessID));
				if (!path.empty() && !binaries.empty() && path.starts_with(binaries))
					continue;

				if (!std::ranges::contains(evidence.conflicting_processes, display))
					evidence.conflicting_processes.push_back(display);
			} while (Process32NextW(snapshot, &entry));
		}

		CloseHandle(snapshot);
	}

	void checkInstallPath(CollectedEvidence& evidence)
	{
		const std::string path	= Core::get().currentPath().string();
		const std::string lower = toLower(path);

		if (path.size() > 240)
			evidence.path_issues.push_back(localized("str_diag_path_too_long"));
		if (std::ranges::any_of(path, [](char ch) { return static_cast<unsigned char>(ch) > 0x7F; }))
			evidence.path_issues.push_back(localized("str_diag_path_non_ascii"));
		if (lower.contains("onedrive"))
			evidence.path_issues.push_back(localized("str_diag_path_onedrive"));
		if (path.starts_with("\\\\"))
			evidence.path_issues.push_back(localized("str_diag_path_unc"));
	}

	void runProbe(const ServiceFailure& failure, CollectedEvidence& evidence)
	{
		std::vector<std::string> args = evidence.probe_args;
		if (args.empty())
			args.push_back(failure.binary_path);

		// SvcHost.exe is a service wrapper: as a plain process it dies with
		// ERROR_FAILED_SERVICE_CONTROLLER_CONNECT before loading its payload.
		// Probe the payload (second token onward) instead of the wrapper.
		if (args.size() > 1)
		{
			const auto		  name_start = args.front().find_last_of("\\/");
			const std::string name		 = toLower(args.front().substr(name_start == std::string::npos ? 0 : name_start + 1));
			if (name == "svchost.exe")
				args.erase(args.begin());
		}

		evidence.probe_attempted = true;

		const ProcessCaptureResult result = runHiddenProcessCapture(args, c_probe_timeout_ms);
		evidence.probe_started			  = result.started;
		evidence.probe_timed_out		  = result.timed_out;
		evidence.probe_exit_code		  = result.exit_code;
		evidence.probe_out				  = result.out;
		evidence.probe_err				  = result.err;
	}

	CollectedEvidence collectEvidence(const ServiceFailure& failure, bool allow_probe)
	{
		CollectedEvidence evidence;

		evidence.install_path	   = Core::get().currentPath().string();
		evidence.expected_binary   = failure.binary_path;
		evidence.elevated		   = isElevated();
		// WinDivert is only loaded by the zapret engines.
		evidence.windivert_checked = failure.service_name.starts_with("zapret");

		// Registration goes first: the command line it returns is what the file
		// checks parse their paths from.
		if (evidence.elevated)
		{
			checkRegistration(failure, evidence);
			checkDriver(evidence);
		}

		checkFiles(failure, evidence);
		checkConflicts(evidence);
		checkInstallPath(evidence);
		checkEventLog(failure, evidence);

		if (allow_probe)
			runProbe(failure, evidence);

		return evidence;
	}

	// Several services can die in one run. A service that actually started and
	// died carries exit codes and log entries, a Create failure carries only an
	// error code, so the most informative one is reported.
	int scoreOf(const ServiceFailure& failure)
	{
		int score = 0;
		switch (failure.stage)
		{
		case ServiceFailure::Stage::UnexpectedStop:
			score = 4;
			break;
		case ServiceFailure::Stage::WaitRunning:
			score = 3;
			break;
		case ServiceFailure::Stage::Start:
			score = 2;
			break;
		default:
			score = 1;
			break;
		}

		if (failure.win32_exit_code != 0)
			++score;

		return score;
	}
}	 // namespace

DiagnosticReport ServiceDiagnostics::diagnose(std::span<const ServiceFailure> failures, bool allow_probe)
{
	DiagnosticReport report;
	if (failures.empty())
		return report;

	const ServiceFailure* target = &failures.front();
	for (const auto& failure : failures)
		if (scoreOf(failure) > scoreOf(*target))
			target = &failure;

	if (!target->valid())
		return report;

	const CollectedEvidence evidence = collectEvidence(*target, allow_probe);
	report							 = buildReport(*target, evidence);

	// Several services dying in the same attempt is evidence in itself: a
	// shared cause (driver, rights, antivirus) is likelier than each binary
	// breaking on its own. The single most informative failure above still
	// drives the verdict, this check only names the whole set.
	std::vector<std::string> dead_services;
	for (const auto& failure : failures)
		if (failure.valid())
			dead_services.push_back(failure.service_name);

	if (dead_services.size() > 1)
		report.checks.insert(
			report.checks.begin(),
			DiagnosticCheck{ DiagnosticCheck::Level::Failed, utils::format(localized("str_diag_check_failed_services"), join(dead_services, ", ")) }
		);

	Debug::info("service diagnostics [{}]: {}", target->service_name, report.verdict);
	return report;
}
