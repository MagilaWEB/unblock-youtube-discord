#include <catch2/catch_test_macros.hpp>

import std;
import core.event_log;
import core.localization;
import core.service;
import core.service_diagnostics;
import core.types;

namespace fs = std::filesystem;

// The verdict logic reads localized strings, so the fixture replaces them with
// markers: the tests assert on the decision, not on the wording of US.list.
// Keys that are not listed resolve to their own name, which is harmless.
struct DiagnosticsFixture
{
	fs::path root;

	DiagnosticsFixture()
	{
		root = fs::temp_directory_path() / "unblock_service_diag_test";
		fs::remove_all(root);
		fs::create_directories(root / "ui" / "text");
		// Core::get() (reached through File::open/Localization) insists on these.
		fs::create_directories(root / "bin");
		fs::create_directories(root / "binaries");
		fs::create_directories(root / "configs");
		fs::current_path(root);

		{
			std::ofstream ofs(root / "ui" / "text" / "US.list");
			ofs << "str_diag_verdict_unknown=VERDICT_UNKNOWN\n";
			ofs << "str_diag_verdict_antivirus=VERDICT_ANTIVIRUS\n";
			ofs << "str_diag_verdict_registration=VERDICT_REGISTRATION\n";
			ofs << "str_diag_verdict_disabled=VERDICT_DISABLED\n";
			ofs << "str_diag_verdict_scm=VERDICT_SCM\n";
			ofs << "str_diag_verdict_probe_exit=VERDICT_PROBE_EXIT {}\n";
			ofs << "str_diag_verdict_exit_code=VERDICT_EXIT_CODE {} {}\n";
			ofs << "str_diag_summary_failure=SUMMARY {} {} {} {} {}\n";
			ofs << "str_diag_summary_unknown=SUMMARY_UNKNOWN\n";
			ofs << "str_diag_summary_registration=SUM_REG {}\n";
			ofs << "str_diag_check_scm_found=CHECK_SCM_FOUND {}\n";
			ofs << "str_diag_check_event=CHECK_EVENT {} {}\n";
			ofs << "str_diag_check_registration_mismatch=REG_MISMATCH {}\n";
			ofs << "str_diag_check_probe_output=CHECK_PROBE_OUTPUT {}\n";
			ofs << "str_diag_check_probe_not_indicative=CHECK_PROBE_NOT_INDICATIVE\n";
			ofs << "str_diag_code_dll_not_found=CODE_DLL_NOT_FOUND\n";
			ofs << "str_diag_rec_antivirus_2=REC_AV_FOLDER {}\n";
			ofs << "str_diag_section_recommendations=RECOMMENDATIONS\n";
			ofs << "str_diag_section_raw=RAW\n";
		}

		Localization::get().set("US");
	}

	~DiagnosticsFixture()
	{
		fs::current_path(fs::temp_directory_path());
		fs::remove_all(root);
	}
};

namespace
{
	// The picture of a service that started, lived for a moment and died.
	ServiceFailure makeFailure()
	{
		ServiceFailure failure;
		failure.service_name = "zapret2";
		failure.binary_path	 = "C:\\app\\binaries\\SvcHost.exe";
		failure.stage		 = ServiceFailure::Stage::UnexpectedStop;
		failure.state		 = 1;	 // SERVICE_STOPPED
		failure.lived_ms	 = 250;
		failure.created		 = true;
		return failure;
	}

	// Everything healthy: nothing to blame, so the verdict must stay honest.
	CollectedEvidence cleanEvidence()
	{
		CollectedEvidence evidence;
		evidence.binary_exists			  = true;
		evidence.log_dir_writable		  = true;
		evidence.log_dir				  = "C:\\app\\logs";
		evidence.registration_known		  = true;
		evidence.registered_command		  = "\"C:\\app\\binaries\\SvcHost.exe\" C:\\app\\bin\\winws2.exe";
		evidence.expected_binary		  = "C:\\app\\binaries\\SvcHost.exe";
		evidence.start_type				  = 2;	  // SERVICE_AUTO_START
		evidence.windivert_checked		  = true;
		evidence.windivert_dll_exists	  = true;
		evidence.windivert_sys_exists	  = true;
		evidence.windivert_driver_known	  = true;
		evidence.windivert_driver_running = true;
		evidence.elevated				  = true;
		evidence.install_path			  = "C:\\app";
		return evidence;
	}

	EventLogRecord scmRecord(u32 id, std::string message)
	{
		EventLogRecord record;
		record.id		   = id;
		record.provider	   = "Service Control Manager";
		record.channel	   = "System";
		record.message	   = std::move(message);
		record.time_utc_ms = 1;
		return record;
	}
}	 // namespace

TEST_CASE("Diagnostics: empty report is empty", "[service_diagnostics]")
{
	DiagnosticReport report;
	CHECK(report.empty());
	CHECK(report.fullText().empty());
}

TEST_CASE("Diagnostics: an image crash code names the missing DLL", "[service_diagnostics]")
{
	DiagnosticsFixture fx;

	auto failure			= makeFailure();
	failure.win32_exit_code = 0xC0'00'01'35;

	auto report = buildReport(failure, cleanEvidence());

	CHECK(report.verdict.contains("VERDICT_EXIT_CODE"));
	CHECK(report.verdict.contains("CODE_DLL_NOT_FOUND"));
	CHECK(report.fullText().contains("CODE_DLL_NOT_FOUND"));
	CHECK_FALSE(report.recommendations.empty());
}

TEST_CASE("Diagnostics: a missing WinDivert file gives the antivirus block", "[service_diagnostics]")
{
	DiagnosticsFixture fx;

	auto evidence				  = cleanEvidence();
	evidence.windivert_dll_exists = false;

	auto report = buildReport(makeFailure(), evidence);

	CHECK(report.verdict == "VERDICT_ANTIVIRUS");
	CHECK(report.recommendations.size() >= 4);
	CHECK(report.fullText().contains("REC_AV_FOLDER"));
	// The folder to exclude must be the real install path.
	CHECK(std::ranges::any_of(report.recommendations, [](const std::string& line) { return line.contains("C:\\app"); }));
}

TEST_CASE("Diagnostics: a service registered at an old path is reported", "[service_diagnostics]")
{
	DiagnosticsFixture fx;

	auto evidence				= cleanEvidence();
	evidence.registered_command = "\"D:\\old\\binaries\\SvcHost.exe\" D:\\old\\bin\\winws2.exe";

	auto report = buildReport(makeFailure(), evidence);

	CHECK(report.verdict == "VERDICT_REGISTRATION");
	CHECK(report.fullText().contains("REG_MISMATCH"));
	CHECK(report.fullText().contains("D:\\old"));
}

TEST_CASE("Diagnostics: a disabled service is reported", "[service_diagnostics]")
{
	DiagnosticsFixture fx;

	auto evidence		= cleanEvidence();
	evidence.start_type = 4;	// SERVICE_DISABLED

	auto report = buildReport(makeFailure(), evidence);

	CHECK(report.verdict == "VERDICT_DISABLED");
}

TEST_CASE("Diagnostics: a disabled service outranks a healthy probe", "[service_diagnostics]")
{
	DiagnosticsFixture fx;

	auto evidence			 = cleanEvidence();
	evidence.start_type		 = 4;	 // SERVICE_DISABLED
	evidence.probe_attempted = true;
	evidence.probe_started	 = true;
	evidence.probe_timed_out = true;

	auto report = buildReport(makeFailure(), evidence);

	// The probe only proves the binary runs outside the SCM: it must not bury
	// the real blocker and tell the user to check rights and antivirus.
	CHECK(report.verdict == "VERDICT_DISABLED");
}

TEST_CASE("Diagnostics: no evidence stays an honest unknown with raw data", "[service_diagnostics]")
{
	DiagnosticsFixture fx;

	auto report = buildReport(makeFailure(), cleanEvidence());

	CHECK(report.verdict == "VERDICT_UNKNOWN");
	CHECK_FALSE(report.empty());
	CHECK_FALSE(report.raw.empty());
	CHECK_FALSE(report.recommendations.empty());
	// The raw section is what makes the honest answer useful.
	CHECK(report.fullText().contains("RAW"));
	CHECK(report.fullText().contains("win32_exit_code"));
	CHECK(report.fullText().contains("SUMMARY_UNKNOWN"));
}

TEST_CASE("Diagnostics: a Service Control Manager record outranks the install path", "[service_diagnostics]")
{
	DiagnosticsFixture fx;

	auto evidence		 = cleanEvidence();
	evidence.path_issues = { "PATH_TOO_LONG" };
	evidence.scm_events.push_back(scmRecord(7'031, "The zapret2 service terminated unexpectedly."));

	auto report = buildReport(makeFailure(), evidence);

	CHECK(report.verdict == "VERDICT_SCM");
	CHECK(report.fullText().contains("CHECK_SCM_FOUND"));
	CHECK(report.fullText().contains("CHECK_EVENT"));
}

TEST_CASE("Diagnostics: the probe output outranks everything else", "[service_diagnostics]")
{
	DiagnosticsFixture fx;

	auto evidence			 = cleanEvidence();
	evidence.probe_attempted = true;
	evidence.probe_started	 = true;
	evidence.probe_exit_code = 0xC0'00'01'42;
	evidence.probe_err		 = "ERROR: dll init failed";
	evidence.scm_events.push_back(scmRecord(7'031, "The zapret2 service terminated unexpectedly."));

	auto failure			= makeFailure();
	failure.win32_exit_code = 0xC0'00'01'42;

	auto report = buildReport(failure, evidence);

	CHECK(report.verdict.contains("VERDICT_PROBE_EXIT"));
	CHECK(report.fullText().contains("CHECK_PROBE_OUTPUT"));
	CHECK(report.fullText().contains("dll init failed"));
	CHECK(report.fullText().contains("CHECK_SCM_FOUND"));
}

TEST_CASE("Diagnostics: fullText keeps the whole probe output, issueText clips it", "[service_diagnostics]")
{
	DiagnosticsFixture fx;

	auto evidence			 = cleanEvidence();
	evidence.probe_attempted = true;
	evidence.probe_started	 = true;
	evidence.probe_exit_code = 5;	 // neither 0 nor 1063, so the probe branch reports a failure
	evidence.probe_err		 = std::string(600, 'x') + "PROBE_TAIL";

	auto report = buildReport(makeFailure(), evidence);

	// The attached file carries the evidence whole...
	CHECK(report.fullText().contains("PROBE_TAIL"));
	// ...while the issue URL (issueText) keeps only the short summary.
	CHECK_FALSE(report.issueText().contains("PROBE_TAIL"));
}

TEST_CASE("Diagnostics: a missing file outranks the probe verdict", "[service_diagnostics]")
{
	DiagnosticsFixture fx;

	auto evidence			 = cleanEvidence();
	evidence.binary_exists	 = false;	 // the service image itself is gone
	evidence.missing_files	 = { "C:\\app\\bin\\zapret_helper.exe" };
	evidence.probe_attempted = true;
	evidence.probe_started	 = true;
	evidence.probe_exit_code = 0xC0'00'01'42;	 // would normally blame the binary
	evidence.probe_err		 = "ERROR: dll init failed";

	auto report = buildReport(makeFailure(), evidence);

	// A file that is not there cannot be blamed by the probe.
	CHECK(report.verdict == "VERDICT_ANTIVIRUS");
	CHECK_FALSE(report.verdict.contains("VERDICT_PROBE_EXIT"));
	CHECK(report.fullText().contains("C:\\app\\bin\\zapret_helper.exe"));
}

TEST_CASE("Diagnostics: probe 1063 is not indicative and does not raise the verdict", "[service_diagnostics]")
{
	DiagnosticsFixture fx;

	auto evidence			 = cleanEvidence();
	evidence.probe_attempted = true;
	evidence.probe_started	 = true;
	evidence.probe_exit_code = 1'063;	 // ERROR_FAILED_SERVICE_CONTROLLER_CONNECT

	auto report = buildReport(makeFailure(), evidence);

	CHECK_FALSE(report.verdict.contains("VERDICT_PROBE_EXIT"));
	CHECK(report.verdict == "VERDICT_UNKNOWN");
	CHECK(report.fullText().contains("CHECK_PROBE_NOT_INDICATIVE"));
}

TEST_CASE("Diagnostics: fullText carries verdict, checks, recommendations and raw", "[service_diagnostics]")
{
	DiagnosticsFixture fx;

	auto evidence				  = cleanEvidence();
	evidence.windivert_dll_exists = false;

	auto report = buildReport(makeFailure(), evidence);
	auto text	= report.fullText();

	CHECK(text.contains("VERDICT_ANTIVIRUS"));
	CHECK(text.contains("SUMMARY"));
	CHECK(text.contains("RECOMMENDATIONS"));
	CHECK(text.contains("RAW"));
	CHECK(text.contains("1. "));
}

TEST_CASE("Diagnostics: issueText carries only the cause description", "[service_diagnostics]")
{
	DiagnosticsFixture fx;

	auto evidence				  = cleanEvidence();
	evidence.windivert_dll_exists = false;

	auto report = buildReport(makeFailure(), evidence);
	auto text	= report.issueText();

	// Cause: verdict plus a non-empty summary; nothing else.
	REQUIRE_FALSE(report.summary.empty());
	REQUIRE_FALSE(report.raw.empty());
	REQUIRE_FALSE(report.checks.empty());
	REQUIRE_FALSE(report.recommendations.empty());

	CHECK(text.contains("VERDICT_ANTIVIRUS"));
	CHECK(text.contains(report.summary));

	// No raw section, no raw lines, no checklist tags, no recommendations.
	CHECK_FALSE(text.contains("RAW"));
	CHECK_FALSE(text.contains("win32_exit_code"));
	CHECK_FALSE(text.contains("RECOMMENDATIONS"));
	CHECK_FALSE(text.contains("REC_AV_FOLDER"));
	CHECK_FALSE(text.contains("[OK]"));
	CHECK_FALSE(text.contains("[FAILED]"));
	CHECK_FALSE(text.contains("[WARNING]"));
	CHECK_FALSE(text.contains("[SKIPPED]"));
}
