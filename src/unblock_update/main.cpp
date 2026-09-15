// unblock_update — a standalone helper that performs application update/uninstall.
//
// The main process (engine.exe) stages this helper into %TEMP%\unblock and
// launches it from there AFTER the update has been downloaded and extracted:
//
//	unblock_update.exe <appRoot> <enginePid> update <updateRoot>
//	unblock_update.exe <appRoot> <enginePid> remove
//
//	appRoot    — installation root of the application (Core::currentPath());
//	enginePid  — PID of engine.exe whose termination must be awaited;
//	updateRoot — temporary directory holding the extracted update (contains an
//	            "unblock" folder with the new payload), typically %TEMP%\unblock.
//
// update: waits for engine to exit, syncs updateRoot\unblock\* into appRoot,
//         removes updateRoot, and starts the fresh appRoot\bin\engine.exe.
//         Fully managed dirs (bin, binaries, ui) are wiped before copy so
//         files removed from the new version do not linger as stale junk.
//         User data (user, logs) is never touched.
// remove: waits for engine to exit and removes appRoot entirely.
//
// The module deliberately avoids core.dll dependencies — WinAPI and
// std::filesystem only — to stay self-contained and reliable. Its progress is
// recorded in %TEMP%\unblock_update.log.

#include <windows.h>

#include <tlhelp32.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
	fs::path selfPath()
	{
		wchar_t buffer[MAX_PATH]{};
		GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
		return fs::path{ buffer };
	}

	void log(const fs::path& log_path, const std::wstring& message)
	{
		std::wofstream stream{ log_path, std::ios::app };
		stream << message << L'\n';
	}

	std::wstring lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(), ::towlower);
		return value;
	}

	// Full path of a running process image; empty when it cannot be queried
	// (process gone, access denied). Never throws.
	fs::path processImagePath(DWORD pid)
	{
		HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if (!process)
			return {};

		wchar_t	 buffer[MAX_PATH]{};
		DWORD	 size = static_cast<DWORD>(std::size(buffer));
		fs::path result;
		if (QueryFullProcessImageNameW(process, 0, buffer, &size))
			result = buffer;
		CloseHandle(process);
		return result;
	}

	// True when no engine.exe belonging to this install is running anymore.
	// A same-named foreign process is ignored (path check); a process whose
	// path cannot be queried is treated as ours — better to wait a little
	// longer than to wipe bin/ under a live engine.
	bool noOurEngineRunning(const fs::path& app_root, const fs::path& log_path)
	{
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snapshot == INVALID_HANDLE_VALUE)
		{
			log(log_path, L"[wait] process snapshot failed, assuming engine alive");
			return false;
		}

		PROCESSENTRY32W entry{};
		entry.dwSize = sizeof(entry);

		bool		 found = false;
		std::wstring details;
		if (Process32FirstW(snapshot, &entry))
		{
			do
			{
				if (lower(entry.szExeFile) != L"engine.exe")
					continue;

				const fs::path image = processImagePath(entry.th32ProcessID);
				if (!image.empty())
				{
					std::error_code ec;
					const auto		root = lower(fs::absolute(app_root, ec).lexically_normal().wstring());
					const auto		dir	 = lower(image.parent_path().parent_path().wstring());
					// engine.exe lives in <appRoot>\bin — compare its folder.
					if (dir != root)
						continue;
				}
				// Unknown path: conservatively assume it is ours.
				found	= true;
				details = L"pid=" + std::to_wstring(entry.th32ProcessID);
				break;
			} while (Process32NextW(snapshot, &entry));
		}
		CloseHandle(snapshot);

		if (found)
			log(log_path, L"[wait] engine.exe still running (" + details + L")");
		return !found;
	}

	// Triple gate before touching appRoot: the PID must be gone, no
	// engine.exe of this install may be running, and bin\engine.exe must be
	// openable exclusively (no lingering file locks, e.g. a driver or AV
	// holding the image right after process exit). Bounded — never hangs
	// the update forever — but every gate is logged.
	void waitForEngineExit(DWORD pid, const fs::path& app_root, const fs::path& log_path)
	{
		constexpr DWORD kProcessMs = 120'000;
		constexpr DWORD kProbeMs   = 30'000;
		constexpr DWORD kFileMs	   = 60'000;

		const ULONGLONG start	= GetTickCount64();
		HANDLE			process = OpenProcess(SYNCHRONIZE, FALSE, pid);
		if (!process)
		{
			log(log_path, L"[wait] engine PID already gone, verifying");
		}
		else
		{
			while (true)
			{
				const DWORD wait_left = static_cast<DWORD>(GetTickCount64() - start < kProcessMs ? kProcessMs - (GetTickCount64() - start) : 0);
				if (WaitForSingleObject(process, wait_left > 5'000 ? 5'000 : wait_left) == WAIT_OBJECT_0)
				{
					log(log_path, L"[wait] engine PID exited after " + std::to_wstring(GetTickCount64() - start) + L"ms");
					break;
				}
				if (GetTickCount64() - start >= kProcessMs)
				{
					log(log_path, L"[wait] TIMEOUT waiting for engine PID, verifying anyway");
					break;
				}
				log(log_path, L"[wait] still waiting for engine PID...");
			}
			CloseHandle(process);
		}

		while (!noOurEngineRunning(app_root, log_path))
		{
			if (GetTickCount64() - start >= kProcessMs + kProbeMs)
			{
				log(log_path, L"[wait] TIMEOUT waiting for engine.exe to disappear");
				break;
			}
			Sleep(500);
		}

		const fs::path	engine_exe = app_root / L"bin" / L"engine.exe";
		std::error_code ec;
		if (!fs::exists(engine_exe, ec))
			return;

		while (true)
		{
			HANDLE file = CreateFileW(engine_exe.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file != INVALID_HANDLE_VALUE)
			{
				CloseHandle(file);
				log(log_path, L"[wait] bin\\engine.exe is writable, proceeding");
				return;
			}
			const DWORD err = GetLastError();
			if (err != ERROR_SHARING_VIOLATION && err != ERROR_ACCESS_DENIED && err != ERROR_LOCK_VIOLATION)
			{
				log(log_path, L"[wait] engine.exe probe error=" + std::to_wstring(err) + L", proceeding");
				return;
			}
			if (GetTickCount64() - start >= kProcessMs + kProbeMs + kFileMs)
			{
				log(log_path, L"[wait] TIMEOUT waiting for engine.exe unlock, proceeding anyway");
				return;
			}
			Sleep(500);
		}
	}

	// remove_all with retries: right after process exit the OS/AV may still
	// hold a handle for a moment; one blind attempt leaves a half-wiped dir
	// that copyRecursive then merges over — the "bin not cleaned" symptom.
	bool retryRemoveAll(const fs::path& target, const fs::path& log_path)
	{
		for (int attempt = 0; attempt < 5; ++attempt)
		{
			std::error_code ec;
			fs::remove_all(target, ec);
			if (!ec && !fs::exists(target, ec))
				return true;
			log(log_path,
				L"[update] wipe retry " + std::to_wstring(attempt + 1) + L" for " + target.wstring() + L" ec=" + std::to_wstring(ec.value()));
			Sleep(500);
		}
		std::error_code ec;
		return !fs::exists(target, ec);
	}

	bool runProcess(const fs::path& exe, const fs::path& work_dir)
	{
		std::wstring cmd_line{ L"\"" + exe.wstring() + L"\"" };

		STARTUPINFOW		startup{};
		PROCESS_INFORMATION process{};
		startup.cb = sizeof(startup);

		if (!CreateProcessW(nullptr, cmd_line.data(), nullptr, nullptr, FALSE, 0, nullptr, work_dir.c_str(), &startup, &process))
			return false;

		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		return true;
	}

	// Case-insensitive comparison: Windows paths are case-insensitive but
	// fs::path::operator== is not, so a naive `target == self` guard never
	// fires and the copy slams into the running image.
	bool samePath(const fs::path& left, const fs::path& right)
	{
		std::error_code ec;
		const auto		a = lower(fs::absolute(left, ec).lexically_normal().wstring());
		const auto		b = lower(fs::absolute(right, ec).lexically_normal().wstring());
		return !a.empty() && a == b;
	}

	// File-by-file copy: a single locked file must not abort the whole
	// update (std::filesystem::copy with overwrite_existing stops at the
	// first sharing violation, leaving a half-updated install that looks
	// like "the update did nothing").
	void copyRecursive(const fs::path& src, const fs::path& dst, const fs::path& self, const fs::path& log_path, int& errors)
	{
		std::error_code ec;

		if (samePath(dst, self))
		{
			log(log_path, L"[update] skip running image: " + dst.wstring());
			return;
		}

		const auto status = fs::symlink_status(src, ec);
		if (ec)
		{
			log(log_path, L"[update] stat error " + src.wstring() + L": " + std::to_wstring(ec.value()));
			++errors;
			return;
		}

		if (fs::is_directory(status))
		{
			fs::create_directories(dst, ec);
			if (ec)
			{
				log(log_path, L"[update] mkdir error " + dst.wstring() + L": " + std::to_wstring(ec.value()));
				++errors;
				ec.clear();
			}

			fs::directory_iterator it{ src, ec };
			if (ec)
			{
				log(log_path, L"[update] iterate error " + src.wstring() + L": " + std::to_wstring(ec.value()));
				++errors;
				return;
			}

			for (const auto& entry : it)
				copyRecursive(entry.path(), dst / entry.path().filename(), self, log_path, errors);
			return;
		}

		fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
		if (ec)
		{
			log(log_path, L"[update] copy error " + src.wstring() + L" -> " + dst.wstring() + L": " + std::to_wstring(ec.value()));
			++errors;
		}
	}

	// Managed dirs are shipped verbatim by every release — wipe them first
	// so files deleted upstream do not survive as stale junk (e.g. in bin/).
	// Everything else merges over the install; user data is never touched.
	bool isManagedDir(const std::wstring& name)
	{
		const auto n = lower(name);
		return n == L"bin" || n == L"binaries" || n == L"ui";
	}

	bool isPreservedDir(const std::wstring& name)
	{
		const auto n = lower(name);
		return n == L"user" || n == L"logs";
	}

	// Best-effort temp cleanup that never deletes our own running image
	// (DeleteFile on a running exe fails and aborts remove_all midway,
	// leaving leftovers for the next update to trip over).
	void cleanupUpdateRoot(const fs::path& update_root, const fs::path& self, const fs::path& log_path)
	{
		std::error_code		   ec;
		fs::directory_iterator it{ update_root, ec };
		if (ec)
		{
			log(log_path, L"[update] cleanup iterate error: " + std::to_wstring(ec.value()));
			return;
		}

		for (const auto& entry : it)
		{
			if (samePath(entry.path(), self))
				continue;
			fs::remove_all(entry.path(), ec);
			if (ec)
			{
				log(log_path, L"[update] cleanup error " + entry.path().wstring() + L": " + std::to_wstring(ec.value()));
				ec.clear();
			}
		}
	}

	int updateMode(const fs::path& app_root, DWORD engine_pid, const fs::path& update_root, const fs::path& log_path)
	{
		const fs::path update_src = update_root / L"unblock";
		const fs::path self		  = selfPath();

		// Never touch appRoot while the old engine is alive: locked files
		// turn the managed-dir wipe into a half-wipe that copyRecursive
		// then merges over — the "bin not cleaned" symptom.
		waitForEngineExit(engine_pid, app_root, log_path);

		std::error_code ec;

		log(log_path, L"[update] copy " + update_src.wstring() + L" -> " + app_root.wstring());

		if (!fs::exists(update_src, ec) || ec)
		{
			log(log_path, L"[update] payload missing, aborting (nothing copied)");
			return 1;
		}

		int copied = 0;
		int errors = 0;

		fs::directory_iterator it{ update_src, ec };
		if (ec)
		{
			log(log_path, L"[update] iterate error: " + std::to_wstring(ec.value()));
			return 1;
		}

		for (const auto& entry : it)
		{
			const std::wstring name	  = entry.path().filename().wstring();
			const fs::path	   target = app_root / entry.path().filename();

			if (isPreservedDir(name))
			{
				log(log_path, L"[update] preserve: " + target.wstring());
				continue;
			}

			std::error_code exists_ec;
			if (isManagedDir(name) && fs::exists(target, exists_ec))
			{
				if (retryRemoveAll(target, log_path))
					log(log_path, L"[update] wiped: " + target.wstring());
				else
				{
					log(log_path, L"[update] WIPE FAILED, merging over leftovers: " + target.wstring());
					++errors;
				}
			}

			copyRecursive(entry.path(), target, self, log_path, errors);
			++copied;
		}

		log(log_path, L"[update] entries=" + std::to_wstring(copied) + L" errors=" + std::to_wstring(errors));

		cleanupUpdateRoot(update_root, self, log_path);

		const fs::path engine = app_root / L"bin" / L"engine.exe";
		if (!runProcess(engine, app_root / L"bin"))
		{
			log(log_path, L"[update] failed to start engine");
			return 1;
		}

		log(log_path, L"[update] done");
		return 0;
	}

	int removeMode(const fs::path& app_root, DWORD engine_pid, const fs::path& log_path)
	{
		waitForEngineExit(engine_pid, app_root, log_path);

		std::error_code ec;
		fs::remove_all(app_root, ec);
		if (ec)
		{
			log(log_path, L"[remove] error: " + std::to_wstring(ec.value()));
			return 1;
		}
		return 0;
	}
}	 // namespace

int wmain(int argc, wchar_t* argv[])
{
	// unblock_update.exe <appRoot> <enginePid> <mode> [<updateRoot>]
	if (argc < 4)
		return 1;

	const fs::path	   app_root	  = argv[1];
	const DWORD		   engine_pid = static_cast<DWORD>(std::wcstoul(argv[2], nullptr, 10));
	const std::wstring mode		  = argv[3];

	const fs::path log_path = fs::temp_directory_path() / L"unblock_update.log";
	log(log_path, L"[start] mode=" + mode + L" appRoot=" + app_root.wstring() + L" pid=" + std::to_wstring(engine_pid));

	if (mode == L"update")
	{
		if (argc < 5)
			return 1;
		return updateMode(app_root, engine_pid, argv[4], log_path);
	}

	if (mode == L"remove")
		return removeMode(app_root, engine_pid, log_path);

	log(log_path, L"[start] unknown mode: " + mode);
	return 1;
}
