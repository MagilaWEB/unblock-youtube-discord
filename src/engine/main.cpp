#include "pch.h"

#include "engine.h"

using namespace std;

static void run(const std::string& command_line)
{
	Core::get().initialize(command_line);

	Engine::get().initialize();
	Engine::get().run();
}

#ifdef __clang__
	#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif

int main(int argc, char** argv)
{
	// Set UTF-8
	SetConsoleCP(65'001);
	SetConsoleOutputCP(65'001);

	size_t size = 1;
	for (int i = 1; i < argc; ++i)
		size += strlen(argv[i]);
	std::string lpCmdLine{};
	lpCmdLine.reserve(size);
	for (int i = 1; i < argc; ++i)
		lpCmdLine.append(argv[i]);

	Debug::initialize(lpCmdLine);
	auto res = Debug::try_wrap(run, lpCmdLine);

	return res;
}
