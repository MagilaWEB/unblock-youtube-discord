#include "ui_zapret_helper.h"

#include "ui.h"
#include "../unblock/unblock.h"

UiZapretHelper::UiZapretHelper(std::shared_ptr<Ui> ui, std::string_view root) : _ui(std::move(ui)), _root(root)
{
}

void UiZapretHelper::initialize()
{
	_initHelperSettings();
	_initHelperSummary();
	_initHelperChecking();
	_initHelperSeen();
	_initHelperValid();
	_initHelperError();
	_initHelperExhausted();
	_initHelperUnjudged();
}

void UiZapretHelper::setVisible(bool visible)
{
	if (visible == _visible)
		return;

	_visible = visible;

	auto toggle = [visible](auto& widget)
	{
		if (!widget->isCreate())
			return;

		if (visible)
			widget->show();
		else
			widget->hide();
	};

	toggle(_list_helper_checking);
	toggle(_list_helper_seen);
	toggle(_list_helper_valid);
	toggle(_list_helper_error);
	toggle(_list_helper_exhausted);
	toggle(_list_helper_unjudged);
	toggle(_helper_summary);
	toggle(_helper_pool);
	toggle(_helper_recheck_min);
	toggle(_helper_errors_progress_min);
	toggle(_helper_errors_recheck_sec);
}

void UiZapretHelper::_initHelperSummary()
{
	_helper_summary->create(_sel(" .common"));
}

void UiZapretHelper::_initHelperChecking()
{
	_list_helper_checking->create(_sel(" section"), utils::format(Localization::Str{ "str_zapret_helper_checking_title" }(), 0, 0));
}

std::span<const UiZapretHelper::HelperSettingDef> UiZapretHelper::helperSettingDefs()
{
	// Defaults mirror HelperConfig::defaults() (src/helper/helper_config.h).
	// Kept as literals: ui target does not link the helper parser.
	static const HelperSettingDef defs[]{
		{			 "pool_size",Input::Types::count,10, 1,64,	  "","str_helper_pool_title",				   "str_helper_pool_description",&UiZapretHelper::_helper_pool						 },
		{ "recheck_interval_min",
		 Input::Types::duration_min,
		 10, 5,
		 180, "min",
		 "str_helper_recheck_min_title",		 "str_helper_recheck_min_description",
		 &UiZapretHelper::_helper_recheck_min		 },
		{  "errors_progress_min",
		 Input::Types::duration_min,
		 2, 1,
		 30, "min",
		 "str_helper_errors_progress_min_title", "str_helper_errors_progress_min_description",
		 &UiZapretHelper::_helper_errors_progress_min },
		{	"errors_recheck_sec",
		 Input::Types::duration_sec,
		 15, 5,
		 300, "sec",
		 "str_helper_errors_recheck_sec_title",  "str_helper_errors_recheck_sec_description",
		 &UiZapretHelper::_helper_errors_recheck_sec },
	};

	return defs;
}

void UiZapretHelper::_initHelperSettings()
{
	auto submit = [this](JSArgs)
	{
		_applyHelperSettings();
		return false;
	};

	for (const auto& def : helperSettingDefs())
	{
		auto& widget = this->*def.widget;
		widget->create(
			_sel(" .common"),
			def.type,
			JSValue{ static_cast<int>(_helperSettingU32(def.key, def.fallback)) },
			Localization::Str{ def.title },
			Localization::Str{ def.description },
			Input::Options{ def.min_value, def.max_value, std::string{ def.unit } }
		);
		widget->addEventSubmit(submit);
	}

	_pushHelperSettings();
}

void UiZapretHelper::updateChecking()
{
	if (!_list_helper_checking->isCreate())
		return;

	// Unique among the helper lists: this one is fed by instant CHECKING/
	// DONE edges, while seen/valid/error/exhausted ride the 500ms snapshots.
	// Ui::update() ticks at ~33Hz and the in-check set rotates as workers
	// start/finish, so rebuilding the scrollable <ul> every tick made the
	// list strobe (and reset its scroll). Match the snapshot cadence: the
	// list still reads edges, but only repaints twice a second.
	LIMIT_UPDATE(HelperChecking, .5f, {
		auto hosts = _ui->_unblock->helperCheckingHosts();
		std::ranges::sort(hosts);

		// Pool load snapshot from the helper (authoritative counts): per-host
		// edges below stay truthful now that verdict snapshots no longer spam
		// O(N) datagrams per lua packet and drown them on loopback.
		const auto stats	= _ui->_unblock->helperStats();
		const bool snapshot = stats.in_check != 0 || stats.queued != 0 || stats.known != 0 || hosts.empty();
		const auto title	= utils::format(
			Localization::Str{ "str_zapret_helper_checking_title" }(),
			snapshot ? stats.in_check : hosts.size(),
			snapshot ? stats.queued : 0
		);

		if (title != _last_helper_checking_title)
		{
			_last_helper_checking_title = title;
			_list_helper_checking->setTitle(title);
		}

		if (hosts == _last_helper_checking)
			return;

		_last_helper_checking = hosts;

		_list_helper_checking->clear();
		for (auto& host : _last_helper_checking)
			_list_helper_checking->createLi(Localization::Str{ host });
	})
}

void UiZapretHelper::_initHelperSeen()
{
	_list_helper_seen->create(_sel(" section"), utils::format(Localization::Str{ "str_zapret_helper_seen_title" }(), 0));
}

void UiZapretHelper::updateSeen()
{
	if (!_list_helper_seen->isCreate())
		return;

	auto hosts = _ui->_unblock->helperSeenHosts();
	std::ranges::sort(hosts);

	if (hosts == _last_helper_seen)
		return;

	_last_helper_seen = hosts;

	_list_helper_seen->setTitle(utils::format(Localization::Str{ "str_zapret_helper_seen_title" }(), hosts.size()));
	_list_helper_seen->clear();
	for (auto& host : hosts)
		_list_helper_seen->createLi(Localization::Str{ host });
}

void UiZapretHelper::_initHelperValid()
{
	_list_helper_valid->create(_sel(" section"), utils::format(Localization::Str{ "str_zapret_helper_valid_title" }(), 0));
}

void UiZapretHelper::updateValid()
{
	if (!_list_helper_valid->isCreate())
		return;

	auto entries = _ui->_unblock->helperValidHosts();
	std::ranges::sort(entries);

	if (entries == _last_helper_valid)
		return;

	_last_helper_valid = entries;

	_list_helper_valid->setTitle(utils::format(Localization::Str{ "str_zapret_helper_valid_title" }(), entries.size()));
	_list_helper_valid->clear();
	for (auto& [host, strategy] : entries)
		_list_helper_valid->createLiSuccess(utils::format(Localization::Str{ "str_zapret_helper_valid_item" }(), host, strategy), true);
}

void UiZapretHelper::_initHelperError()
{
	_list_helper_error->create(_sel(" section"), utils::format(Localization::Str{ "str_zapret_helper_error_title" }(), 0));
}

void UiZapretHelper::updateError()
{
	if (!_list_helper_error->isCreate())
		return;

	auto entries = _ui->_unblock->helperErrorHosts();
	std::ranges::sort(entries);

	if (entries == _last_helper_error)
		return;

	_last_helper_error = entries;

	_list_helper_error->setTitle(utils::format(Localization::Str{ "str_zapret_helper_error_title" }(), entries.size()));
	_list_helper_error->clear();
	for (auto& [host, strategy] : entries)
		_list_helper_error->createLiSuccess(utils::format(Localization::Str{ "str_zapret_helper_error_item" }(), host, strategy));
}

void UiZapretHelper::_initHelperExhausted()
{
	_list_helper_exhausted->create(_sel(" section"), utils::format(Localization::Str{ "str_zapret_helper_exhausted_title" }(), 0));
}

void UiZapretHelper::updateExhausted()
{
	if (!_list_helper_exhausted->isCreate())
		return;

	auto entries = _ui->_unblock->helperExhaustedHosts();
	std::ranges::sort(entries);

	if (entries == _last_helper_exhausted)
		return;

	_last_helper_exhausted = entries;

	_list_helper_exhausted->setTitle(utils::format(Localization::Str{ "str_zapret_helper_exhausted_title" }(), entries.size()));
	_list_helper_exhausted->clear();
	for (auto& [host, strategy] : entries)
		_list_helper_exhausted->createLiSuccess(utils::format(Localization::Str{ "str_zapret_helper_exhausted_item" }(), host, strategy));
}

void UiZapretHelper::_initHelperUnjudged()
{
	_list_helper_unjudged->create(_sel(" section"), utils::format(Localization::Str{ "str_zapret_helper_unjudged_title" }(), 0));
}

void UiZapretHelper::updateUnjudged()
{
	if (!_list_helper_unjudged->isCreate())
		return;

	auto hosts = _ui->_unblock->helperUnjudgedHosts();
	std::ranges::sort(hosts);

	if (hosts == _last_helper_unjudged)
		return;

	_last_helper_unjudged = hosts;

	_list_helper_unjudged->setTitle(utils::format(Localization::Str{ "str_zapret_helper_unjudged_title" }(), hosts.size()));
	_list_helper_unjudged->clear();
	for (auto& host : hosts)
		_list_helper_unjudged->createLi(Localization::Str{ host });
}

void UiZapretHelper::updateSummary()
{
	if (!_helper_summary->isCreate())
		return;

	// Effectiveness = share of judged hosts that work: valid / (valid + error
	// + exhausted). unjudged is pending, so it is shown but not in the ratio.
	LIMIT_UPDATE(HelperSummary, 1.F, {
		const auto valid	 = _ui->_unblock->helperValidHosts().size();
		const auto error	 = _ui->_unblock->helperErrorHosts().size();
		const auto exhausted = _ui->_unblock->helperExhaustedHosts().size();
		const auto unjudged	 = _ui->_unblock->helperUnjudgedHosts().size();
		const auto judged	 = valid + error + exhausted;
		const auto percent	 = judged != 0 ? static_cast<unsigned>((valid * 100 + judged / 2) / judged) : 0U;

		_helper_summary->setText(utils::format(Localization::Str{ "str_zapret_helper_summary" }(), percent, valid, error, exhausted, unjudged));
	})
}

u32 UiZapretHelper::_helperSettingU32(std::string_view key, u32 fallback) const
{
	if (auto v = _ui->userConfig()->parameterSection<std::string>("HELPER", std::string{ key }))
	{
		try
		{
			return static_cast<u32>(std::stoul(v.value()));
		}
		catch (...)
		{
			return fallback;
		}
	}

	return fallback;
}

void UiZapretHelper::_applyHelperSettings()
{
	// Blocking DOM getters: never on the JS thread, always via background task.
	Core::get().addTask(
		[this]
		{
			for (const auto& def : helperSettingDefs())
			{
				auto&	  widget = this->*def.widget;
				const u32 value	 = widget->getValueU32(def.type, def.fallback, def.min_value, def.max_value);
				_ui->userConfig()->writeSectionParameter("HELPER", std::string{ def.key }, std::to_string(value));
			}

			_pushHelperSettings();
		}
	);
}

void UiZapretHelper::_pushHelperSettings() const
{
	std::string message{ "CONFIG:" };
	bool		first{ true };
	for (const auto& def : helperSettingDefs())
	{
		if (!first)
			message += ";";
		first = false;

		message += std::string{ def.key } + "=" + std::to_string(_helperSettingU32(def.key, def.fallback));
	}

	_ui->_unblock->setHelperConfigMessage(std::move(message));

	// Live push when the helper is already running; startService() repeats
	// the same message right after launch (bind-race retries inside).
	if (_ui->_unblock->activeService())
		_ui->_unblock->pushHelperConfig();
}
