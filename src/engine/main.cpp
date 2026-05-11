#include "engine.h"

using namespace std;

static void run(const std::string& command_line)
{
	auto& core	 = Core::get();
	auto& engine = Engine::get();

	core.initialize(command_line);
	engine.initialize();
	core.parallel_run();
	engine.run();
	core.finish();
}

#ifdef __clang__
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
	if (GetModuleFileName(nullptr, exePath, MAX_PATH) == 0)
		return false;

	SHELLEXECUTEINFO sei{};
	sei.cbSize		 = sizeof(sei);
	sei.fMask		 = SEE_MASK_NOCLOSEPROCESS;
	sei.lpVerb		 = "runas";
	sei.lpFile		 = exePath;
	sei.lpParameters = Debug::commandLine().c_str();
	sei.nShow		 = SW_NORMAL;

	if (!ShellExecuteEx(&sei))
		return false;

	if (sei.hProcess)
	{
		WaitForSingleObject(sei.hProcess, INFINITE);
		CloseHandle(sei.hProcess);
	}

	return true;
}
#endif

int main(int argc, char** argv)
{
	std::string lp_cmd_line{};
	for (int i = 1; i < argc; ++i)
		lp_cmd_line.append(argv[i]);

	Debug::initialize(lp_cmd_line);

#ifdef __clang__
	if (!isRunningAsAdmin())
	{
		if (requestElevation())
			return 0;
		else
			return 1;
	}
#endif

	auto res = Debug::try_wrap(run, lp_cmd_line);
	return res;
}
