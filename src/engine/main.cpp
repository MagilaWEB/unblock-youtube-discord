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
	std::string lp_cmd_line{};
	for (int i = 1; i < argc; ++i)
		lp_cmd_line.append(argv[i]);

	Debug::initialize(lp_cmd_line);
	auto res = Debug::try_wrap(run, lp_cmd_line);
	return res;
}
