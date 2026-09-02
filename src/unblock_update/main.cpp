// unblock_update — a standalone helper that performs application update/uninstall.
//
// The main process (engine.exe) launches it after the update has been downloaded
// and extracted; this helper outlives engine.exe and finishes the job:
//
//	unblock_update.exe <appRoot> <enginePid> update <updateRoot>
//	unblock_update.exe <appRoot> <enginePid> remove
//
//	appRoot    — installation root of the application (Core::currentPath());
//	enginePid  — PID of engine.exe whose termination must be awaited;
//	updateRoot — temporary directory holding the extracted update (contains an
//	            "unblock" folder with the new payload), typically %TEMP%\unblock.
//
// update: waits for engine to exit, copies updateRoot\unblock\* into appRoot,
//         removes updateRoot, and starts the fresh appRoot\bin\engine.exe.
// remove: waits for engine to exit and removes appRoot entirely.
//
// The module deliberately avoids core.dll dependencies — WinAPI and
// std::filesystem only — to stay self-contained and reliable. Its progress is
// recorded in %TEMP%\unblock_update.log.

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace
{
	fs::path selfPath()
	{
		wchar_t buffer[MAX_PATH]{};
		GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
		return fs::path{ buffer };
	}

	bool waitForProcess(DWORD pid)
	{
		HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
		if (!process)
			return false;

		WaitForSingleObject(process, INFINITE);
		CloseHandle(process);
		return true;
	}

	bool runProcess(const fs::path& exe, const fs::path& work_dir)
	{
		std::wstring cmd_line{ L"\"" + exe.wstring() + L"\"" };

		STARTUPINFOW	startup{};
		PROCESS_INFORMATION process{};
		startup.cb = sizeof(startup);

		if (!CreateProcessW(
				nullptr,
				cmd_line.data(),
				nullptr,
				nullptr,
				FALSE,
				0,
				nullptr,
				work_dir.c_str(),
				&startup,
				&process
			))
			return false;

		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		return true;
	}

	void log(const fs::path& log_path, const std::wstring& message)
	{
		std::wofstream stream{ log_path, std::ios::app };
		stream << message << L'\n';
	}

	int updateMode(const fs::path& app_root, const fs::path& update_root, const fs::path& log_path)
	{
		const fs::path update_src = update_root / L"unblock";
		const fs::path self		   = selfPath();

		std::error_code ec;

		log(log_path, L"[update] copy " + update_src.wstring() + L" -> " + app_root.wstring());

		if (fs::exists(update_src, ec))
		{
			for (const auto& entry : fs::directory_iterator(update_src, ec))
			{
				if (ec)
				{
					log(log_path, L"[update] iterate error: " + std::to_wstring(ec.value()));
					break;
				}

				// Skip our own executable: the engine refreshes it before launching us,
				// and a running image cannot be overwritten.
				const fs::path target = app_root / entry.path().filename();
				if (target == self)
					continue;

				fs::copy(entry.path(), target, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
				if (ec)
				{
					log(log_path, L"[update] copy error " + entry.path().wstring() + L": " + std::to_wstring(ec.value()));
					ec.clear();
				}
			}
		}

		fs::remove_all(update_root, ec);

		const fs::path engine = app_root / L"bin" / L"engine.exe";
		if (!runProcess(engine, app_root / L"bin"))
		{
			log(log_path, L"[update] failed to start engine");
			return 1;
		}

		return 0;
	}

	int removeMode(const fs::path& app_root, const fs::path& log_path)
	{
		std::error_code ec;
		fs::remove_all(app_root, ec);
		if (ec)
		{
			log(log_path, L"[remove] error: " + std::to_wstring(ec.value()));
			return 1;
		}
		return 0;
	}
} // namespace

int wmain(int argc, wchar_t* argv[])
{
	// unblock_update.exe <appRoot> <enginePid> <mode> [<updateRoot>]
	if (argc < 4)
		return 1;

	const fs::path   app_root	 = argv[1];
	const DWORD		 engine_pid	 = static_cast<DWORD>(std::wcstoul(argv[2], nullptr, 10));
	const std::wstring mode		 = argv[3];

	const fs::path log_path = fs::temp_directory_path() / L"unblock_update.log";
	log(log_path, L"[start] mode=" + mode + L" appRoot=" + app_root.wstring());

	waitForProcess(engine_pid);

	if (mode == L"update")
	{
		if (argc < 5)
			return 1;
		return updateMode(app_root, argv[4], log_path);
	}

	if (mode == L"remove")
		return removeMode(app_root, log_path);

	log(log_path, L"[start] unknown mode: " + mode);
	return 1;
}