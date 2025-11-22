#include "engine.h"

using namespace std;

static void run(const std::string& command_line)
{
	auto& core = Core::get();
	auto& engine = Engine::get();

	core.initialize(command_line);
	engine.initialize();
	core.parallel_run();
	engine.run();
	core.finish();
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
