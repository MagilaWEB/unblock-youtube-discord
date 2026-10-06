#include "auto_strategy_runner.h"
#include "../core/input_console.h"

AutoStrategyRunner::AutoStrategyRunner(Unblock& unblock) : _unblock(unblock)
{
}

std::string_view AutoStrategyRunner::_waitDescription(Technology technology)
{
	return technology == Technology::Zapret1 ? "str_window_auto_start_wait_description_zapret1" : "str_window_auto_start_wait_description";
}

void AutoStrategyRunner::run(Technology technology, std::string_view start_version, const Callbacks& callbacks)
{
	_current_version = std::string{ start_version };

	InputConsole::textOk(Localization::Str{ "str_beginning_auto_selection" }());

	while (_tryNext(technology, callbacks))
	{
		if (callbacks.cancelled && callbacks.cancelled())
			break;

		_unblock.startService(technology);

		const std::string strategy_name = _unblock.getNameStrategies(technology);
		std::string		  text_desc;

		// On Zapret1 the same config is retried with every fake profile, so the
		// description names the active one — otherwise iterations look like a
		// stuck loop.
		if (technology == Technology::Zapret1)
			text_desc = utils::format(
				Localization::Str{ "str_window_auto_start_wait_name_strategy_description_zapret1" }(),
				strategy_name,
				_current_version,
				_unblock.fakeBinKey(technology)
			);
		else
			text_desc = utils::format(Localization::Str{ "str_window_auto_start_wait_name_strategy_description" }(), strategy_name, _current_version);

		text_desc.insert(0, "\n");
		text_desc.insert(0, Localization::Str{ _waitDescription(technology) }());

		if (callbacks.progress)
			callbacks.progress(text_desc);

		if (technology == Technology::Zapret2 ? _runPassiveRound(technology, text_desc, callbacks) : _runTestingRound(callbacks))
		{
			AutoStrategyResult result{};
			result.won		= true;
			result.strategy = strategy_name;
			result.version	= _current_version;
			result.fake_key = technology == Technology::Zapret1 ? _unblock.fakeBinKey(technology) : std::string{};

			if (callbacks.finished)
				callbacks.finished(result);

			return;
		}
	}

	AutoStrategyResult result{};
	result.cancelled = callbacks.cancelled && callbacks.cancelled();

	if (result.cancelled)
		_unblock.stopService();

	if (callbacks.finished)
		callbacks.finished(result);
}

bool AutoStrategyRunner::_tryNext(Technology technology, const Callbacks& callbacks)
{
	if (_unblock.automaticallyStrategy(technology))
	{
		if (callbacks.iteration)
			callbacks.iteration(_current_version, false);

		return true;
	}

	auto strategy_dirs = _unblock.listVersionStrategy(technology);
	if (strategy_dirs.empty())
		return false;

	const auto it	   = std::ranges::find(strategy_dirs, _current_version);
	const auto it_next = it + 1;

	if (it != strategy_dirs.end() && it_next != strategy_dirs.end())
	{
		_current_version = *it_next;
		_unblock.changeDirVersionStrategy(technology, _current_version);

		if (callbacks.iteration)
			callbacks.iteration(_current_version, true);

		return true;
	}

	// Wrapped around: reset to the first version and stop the search.
	_current_version = strategy_dirs.front();
	_unblock.changeDirVersionStrategy(technology, _current_version);

	if (callbacks.iteration)
		callbacks.iteration(_current_version, true);

	return false;
}

bool AutoStrategyRunner::_runPassiveRound(Technology technology, std::string_view header, const Callbacks& callbacks)
{
	auto								  expected_hosts = _unblock.testHostNames();
	const std::unordered_set<std::string> expected(expected_hosts.begin(), expected_hosts.end());

	// Drop stale datagrams from previous rounds.
	_unblock.helperCheckingHosts();
	_unblock.helperSeenHosts();
	_unblock.helperUnjudgedHosts();
	_unblock.helperValidHosts();
	_unblock.helperErrorHosts();
	_unblock.helperExhaustedHosts();

	// Passive round: no curl testing here at all. The helper probes every LIST
	// host through the running desync, lua marks fully-tried hosts exhausted.
	// Verdict rule over valid/dead/errors: fail fast once dead exceeds 10%,
	// confirm once valid reaches 90%, otherwise wait until every expected host
	// is terminal (valid/exhausted/unjudged/error).
	bool		settled = expected.empty();
	size_t		valid_count{ 0 };
	size_t		dead_count{ 0 };
	size_t		error_count{ 0 };
	std::string last_live;
	auto		last_live_at = std::chrono::steady_clock::now() - std::chrono::seconds(10);

	while (!settled)
	{
		if (callbacks.cancelled && callbacks.cancelled())
			return false;

		if (!_unblock.isRun(technology))
			return false;

		std::unordered_set<std::string> valid_set;
		for (auto& [host, _] : _unblock.helperValidHosts())
			if (expected.contains(host))
				valid_set.insert(host);

		std::unordered_set<std::string> exhausted_set;
		for (auto& [host, _] : _unblock.helperExhaustedHosts())
			if (expected.contains(host))
				exhausted_set.insert(host);

		std::unordered_set<std::string> unjudged_set;
		for (auto& host : _unblock.helperUnjudgedHosts())
			if (expected.contains(host))
				unjudged_set.insert(host);

		std::unordered_set<std::string> error_set;
		for (auto& [host, _] : _unblock.helperErrorHosts())
			if (expected.contains(host))
				error_set.insert(host);

		auto checking = _unblock.helperCheckingHosts();

		valid_count = valid_set.size();
		dead_count	= exhausted_set.size() + unjudged_set.size();
		error_count = error_set.size();

		// DOM bridge from the worker: at most every 2s and only on change,
		// never a blind 2Hz hammer.
		const auto now	= std::chrono::steady_clock::now();
		auto	   live = std::string{ header } + "\n"
						+ utils::format(
							  Localization::Str{ "str_window_auto_start_wait_live" }(),
							  valid_count,
							  checking.size(),
							  error_count,
							  exhausted_set.size()
						);

		if (live != last_live && now - last_live_at >= std::chrono::seconds(2))
		{
			last_live	 = std::move(live);
			last_live_at = now;

			if (callbacks.progress)
				callbacks.progress(last_live);
		}

		// Fail-fast: too many dead hosts among the judged ones — switch the
		// config now instead of waiting for the whole list.
		if (autoRoundDeadExceeded(valid_count, dead_count, error_count))
			return false;

		if (!(settled = autoRoundSettled(expected, valid_set, exhausted_set, unjudged_set, error_set)))
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	if (callbacks.cancelled && callbacks.cancelled())
		return false;

	if (!_unblock.isRun(technology))
		return false;

	return judgeAutoRound(valid_count, dead_count, error_count);
}

bool AutoStrategyRunner::_runTestingRound(const Callbacks& callbacks)
{
	_unblock.testingDomain();

	if (callbacks.cancelled && callbacks.cancelled())
		return false;

	return _unblock.validDomain();
}
