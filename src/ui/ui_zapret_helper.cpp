#include "ui_zapret_helper.h"

#include "ui.h"
#include "../unblock/unblock.h"

UiZapretHelper::UiZapretHelper(std::shared_ptr<Ui> ui, std::string_view root) : _ui(std::move(ui)), _root(root)
{
}

void UiZapretHelper::initialize()
{
	_initHelperSettings();
	_initHelperChecking();
	_initHelperSeen();
	_initHelperValid();
	_initHelperError();
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
	toggle(_helper_pool);
	toggle(_helper_check_timeout);
	toggle(_helper_connect_timeout);
	toggle(_helper_max_redirects);
	toggle(_helper_recheck_min);
	toggle(_helper_errors_progress_min);
	toggle(_helper_errors_recheck_sec);
}

void UiZapretHelper::_initHelperChecking()
{
	_list_helper_checking->create(_sel(" section"), utils::format(Localization::Str{ "str_zapret_helper_checking_title" }(), 0));
}

std::span<const UiZapretHelper::HelperSettingDef> UiZapretHelper::helperSettingDefs()
{
	// Defaults mirror HelperConfig::defaults() (src/helper/helper_config.h).
	// Kept as literals: ui target does not link the helper parser.
	// Timeouts stay strict seconds: the helper applies whole seconds
	// (check_timeout_sec / connect_timeout_sec), sub-second input
	// would be false precision.
	static const HelperSettingDef defs[]{
		{			 "pool_size",Input::Types::count,20, 1,64,	  "","str_helper_pool_title",				   "str_helper_pool_description",&UiZapretHelper::_helper_pool						 },
		{	 "check_timeout_sec",
		 Input::Types::duration_sec,
		 6, 1,
		 60, "sec",
		 "str_helper_check_timeout_title",	   "str_helper_check_timeout_description",
		 &UiZapretHelper::_helper_check_timeout		 },
		{  "connect_timeout_sec",
		 Input::Types::duration_sec,
		 5, 1,
		 30, "sec",
		 "str_helper_connect_timeout_title",	 "str_helper_connect_timeout_description",
		 &UiZapretHelper::_helper_connect_timeout	 },
		{		 "max_redirects",
		 Input::Types::count,
		 5, 0,
		 10,	"",
		 "str_helper_max_redirects_title",	   "str_helper_max_redirects_description",
		 &UiZapretHelper::_helper_max_redirects		 },
		{ "recheck_interval_min",
		 Input::Types::duration_min,
		 30, 5,
		 180, "min",
		 "str_helper_recheck_min_title",		 "str_helper_recheck_min_description",
		 &UiZapretHelper::_helper_recheck_min		 },
		{  "errors_progress_min",
		 Input::Types::duration_min,
		 3, 1,
		 30, "min",
		 "str_helper_errors_progress_min_title", "str_helper_errors_progress_min_description",
		 &UiZapretHelper::_helper_errors_progress_min },
		{	"errors_recheck_sec",
		 Input::Types::duration_sec,
		 30, 5,
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

	auto hosts = _ui->_unblock->helperCheckingHosts();
	std::ranges::sort(hosts);

	if (hosts == _last_helper_checking)
		return;

	_last_helper_checking = hosts;

	_list_helper_checking->setTitle(utils::format(Localization::Str{ "str_zapret_helper_checking_title" }(), hosts.size()));
	_list_helper_checking->clear();
	for (auto& host : hosts)
		_list_helper_checking->createLi(Localization::Str{ host });
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
