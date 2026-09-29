#pragma once
#include "unblock.h"

/** Outcome of an automatic strategy/version search. */
struct AutoStrategyResult
{
	bool		won{ false };
	bool		cancelled{ false };
	std::string strategy;
	std::string version;
	std::string fake_key;
};

/** Walks version/config/fake-profile combinations and picks the winner.
 *  UI-agnostic: the page passes callbacks for progress text, per-iteration

 * *  selector sync and the final outcome. Runs synchronously on a worker. */
class AutoStrategyRunner
{
public:
	struct Callbacks
	{
		std::function<void(std::string_view text)>					progress;
		std::function<void(std::string_view version, bool changed)> iteration;
		std::function<void(const AutoStrategyResult&)>				finished;
		std::function<bool()>										cancelled;
	};

	explicit AutoStrategyRunner(Unblock& unblock);

	void run(Technology technology, std::string_view start_version, const Callbacks& callbacks);

private:
	bool _tryNext(Technology technology, const Callbacks& callbacks);
	bool _runPassiveRound(Technology technology, std::string_view header, const Callbacks& callbacks);
	bool _runTestingRound(const Callbacks& callbacks);

	static std::string_view _waitDescription(Technology technology);

	Unblock&	_unblock;
	std::string _current_version;
};
