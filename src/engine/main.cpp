import ext.windows;
import core;
import core.debug;
import engine;
import unblock;

using namespace std;

static void run(const std::string& /*command_line*/)
{
	auto& core	 = Core::get();
	auto& engine = Engine::get();

	engine.initialize();
	core.parallelRun();
	engine.run();
	core.finish();
}

static bool isRunningAsAdmin()
{
	BOOL					 isAdmin	 = FALSE;
	PSID					 adminSid	 = nullptr;
	SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

	if (!AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminSid))
		return false;

	if (!CheckTokenMembership(nullptr, adminSid, &isAdmin))
		isAdmin = FALSE;

	FreeSid(adminSid);
	return isAdmin == TRUE;
}

static bool requestElevation()
{
	char exePath[MAX_PATH];
	if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0)
		return false;

	SHELLEXECUTEINFOA sei{};
	sei.cbSize		 = sizeof(sei);
	sei.fMask		 = SEE_MASK_NOCLOSEPROCESS;
	sei.lpVerb		 = "runas";
	sei.lpFile		 = exePath;
	sei.lpParameters = Debug::commandLine().c_str();
	sei.nShow		 = SW_NORMAL;

	if (!ShellExecuteExA(&sei))
		return false;

	if (sei.hProcess)
	{
		WaitForSingleObject(sei.hProcess, INFINITE);
		CloseHandle(sei.hProcess);
	}

	return true;
}

// Полная уборка без интерфейса: её зовёт деинсталлятор установщика, прежде чем
// снести файлы. Приложение знает про службы, драйвер WinDivert, hosts-пины и
// возврат адаптерам их DNS — дублировать это в Inno не нужно.
static int uninstall()
{
	Unblock unblock;

	unblock.setDnsEnabled(false);
	unblock.tgProxy().run(false);
	unblock.removeService();

	return 0;
}

// NOLINTNEXTLINE(bugprone-exception-escape) - unhandled exceptions are caught by Debug::tryWrap/SEH handlers
int main(int argc, char** argv)
{
	std::string lp_cmd_line{};
	for (int i = 1; i < argc; ++i)
		lp_cmd_line.append(argv[i]);

	Debug::initialize(lp_cmd_line);

	if (!isRunningAsAdmin())
	{
		if (requestElevation())
			return 0;
		return 1;
	}

	if (lp_cmd_line.find("--uninstall") != std::string::npos)
		return uninstall();

	auto res = Debug::tryWrap(run, lp_cmd_line);
	return res;
}
