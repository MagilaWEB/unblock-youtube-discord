#include "ui_dns_hosts.h"

#include "ui.h"
#include "../unblock/unblock.h"

#include <algorithm>
#include <regex>
#include <sstream>

UiDnsHosts::UiDnsHosts(std::shared_ptr<Ui> ui, std::shared_ptr<Unblock> unblock)
    : _ui(std::move(ui)), _unblock(std::move(unblock))
{
}

void UiDnsHosts::initialize()
{
	_window_wait_response_from_server->create(Localization::Str{"str_please_wait"},
											  "str_window_wait_response_from_server_description");
	_window_wait_response_from_server->setType(SecondaryWindow::Type::Info);

	_window_check_region->create(Localization::Str{ "str_please_wait" }, "str_window_check_region_description");
	_window_check_region->setType(SecondaryWindow::Type::Wait);

	_enableDnsHosts();
}

void UiDnsHosts::_enableDnsHosts()
{
	_window_to_warn_enable_dns_hosts->create(Localization::Str{"str_warning"}, "");

	_window_to_warn_enable_dns_hosts->setType(SecondaryWindow::Type::YesNo);
	_window_to_warn_enable_dns_hosts->addEventYesNo(
		[this](JSArgs args)
		{
			_ui->userConfig()->writeSectionParameter("SYSTEM", "enable_dns_hosts", JSToCPP(args[0]));
			_enableDnsHostsUpdate();
			_window_to_warn_enable_dns_hosts->hide();
			return false;
		}
	);

	_window_wait_update_dns->create(Localization::Str{"str_please_wait"}, "");

	_window_wait_update_dns->setType(SecondaryWindow::Type::Wait);
	_window_wait_update_dns->addEventCancel(
		[this](JSArgs)
		{
			_unblock->dnsHostsCancelUpdate();
			return false;
		}
	);

	_enable_dns_hosts
		->create("#local_dns section .common", "str_checkbox_enable_dns_hosts_title",
				 Localization::Str{"str_checkbox_enable_dns_hosts_description"});
	_enable_dns_hosts->addTutorialStep("str_tour_dns_hosts_title", "str_tour_dns_hosts_description", 10);
	_enable_dns_hosts->addEventClick(
		[this](JSArgs args)
		{
			if (JSToCPP<bool>(args[0]))
			{
				_enableDnsHostsWarningUser();
				return false;
			}

			_ui->userConfig()->writeSectionParameter("SYSTEM", "enable_dns_hosts", "false");
			_enableDnsHostsUpdate();
			return false;
		}
	);

	_start_update_dns_hosts->create("#local_dns section .common", "str_button_start_dns_hosts_update_title");
	_start_update_dns_hosts->addTutorialStep("str_tour_update_dns_title", "str_tour_update_dns_description", 11);

	_start_update_dns_hosts->addEventClick(
		[this](JSArgs)
		{
			Core::get().addTask([this] { _updateDnsHosts(); });
			return false;
		}
	);

	// Активный регион (select) — из списка ниже.
	_select_region
		->create("#local_dns section .common", "str_select_dns_hosts_region_title",
				 Localization::Str{ "str_select_dns_hosts_region_description" });
	_select_region->addEventChange(
		[this](JSArgs args)
		{
			const auto region	= JSToCPP<std::string>(args[0]);
			const auto previous = _unblock->dnsHostsRegion();

			Core::get().addTask(
				// NOLINTNEXTLINE(bugprone-exception-escape) - worker callback; the availability check may allocate.
				[this, region, previous]
				{
					_window_check_region->show();
					const bool available = _unblock->dnsHostsRegionAvailable(region);
					_window_check_region->hide();

					if (available)
					{
						_applyActiveRegion(region);
						return;
					}

					Debug::warning("Region [{}] is not available, reverted to [{}]", region, previous);
					_select_region->setSelectedOptionValue(previous);
				}
			);
			return false;
		}
	);

	// Редактируемый список регионов: по умолчанию ru / eu / us, пользователь
	// может добавить свой (поддомен geohide.ru) или удалить лишний.
	_region_list
		->create("#local_dns section .common", Localization::Str{ "str_dns_hosts_regions_title" },
				 Localization::Str{ "str_dns_hosts_regions_description" }(),
				 Localization::Str{ "str_input_dns_hosts_region_placeholder" }());
	_region_list->setValidator(
		[this](const std::string& value)
		{
			if (value.empty())
				return false;

			// Регион не должен дублироваться.
			if (std::ranges::find(_region_list->items(), value) != _region_list->items().end())
				return false;

			// Регион — поддомен: латиница, цифры, дефис, точка.
			return std::ranges::all_of(value, [](unsigned char ch)
			{
				return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '.';
			});
		}
	);

	_region_list->addEventChange([this](JSArgs args) { _onRegionsChanged(args); return false; });

	// Базовый URL geohide — редактируемый, на случай если автор перенесёт сервис.
	_dns_hosts_url
		->create("#local_dns section .common", Input::Types::text, JSValue{ "geohide.ru" },
				 Localization::Str{ "str_input_dns_hosts_url_title" },
				 Localization::Str{ "str_input_dns_hosts_url_description" });
	_dns_hosts_url->addEventSubmit(
		[this](JSArgs args)
		{
			const auto host = JSToCPP<std::string>(args[0]);
			if (!_isValidHost(host))
				return false;
			_applyBaseUrl(host);
			return false;
		}
	);

	// Инициализация из конфига: список регионов через ';', иначе дефолт.
	std::vector<std::string> regions{ "ru", "eu", "us" };
	if (auto cfg = _ui->userConfig()->parameterSection<std::string>("SYSTEM", "dns_hosts_regions"))
	{
		regions.clear();
		std::stringstream stream{ cfg.value() };
		std::string		item;
		while (std::getline(stream, item, ';'))
			if (!item.empty())
				regions.push_back(item);
	}

	_region_list->setItems(regions);
	_rebuildRegionSelect();

	// Активный регион из конфига, иначе первый в списке.
	std::string active = JSToCPP<std::string>(_select_region->getSelectedOptionValue());
	if (auto cfg = _ui->userConfig()->parameterSection<std::string>("SYSTEM", "dns_hosts_region"))
		if (std::ranges::find(regions, cfg.value()) != regions.end())
			active = cfg.value();

	_select_region->setSelectedOptionValue(active);
	_unblock->setDnsHostsRegion(active);

	// Базовый хост из конфига, иначе дефолт.
	std::string base_url = "geohide.ru";
	if (auto cfg = _ui->userConfig()->parameterSection<std::string>("SYSTEM", "dns_hosts_url"))
		base_url = cfg.value();

	_unblock->setDnsHostsBaseUrl(base_url);

	_enableDnsHostsUpdate();
}

void UiDnsHosts::updateInfoWindow()
{
	static std::string disc_text{Localization::Str{"str_window_wait_update_dns_description"}()};
	LIMIT_UPDATE(Description, .5f, {
		if (_window_wait_update_dns->isShow())
		{
			float			   progress = _unblock->dnsHostsUpdateProgress();
			_window_wait_update_dns->setDescription(utils::format(disc_text, progress));
		}
	})
}

void UiDnsHosts::_updateDnsHosts()
{
	_window_wait_update_dns->show();
	_unblock->dnsHostsUpdate();
	_unblock->dnsHosts(false);
	_unblock->dnsHosts(true);
	_window_wait_update_dns->hide();
}

void UiDnsHosts::_saveRegions()
{
	const auto& items = _region_list->items();

	std::string joined;
	for (std::size_t i = 0; i < items.size(); i++)
	{
		if (i)
			joined.push_back(';');
		joined += items[i];
	}
	_ui->userConfig()->writeSectionParameter("SYSTEM", "dns_hosts_regions", joined);
}

void UiDnsHosts::_rebuildRegionSelect()
{
	const auto& items = _region_list->items();

	// Сохраняем текущий выбор, если он ещё есть в списке.
	std::string active = JSToCPP<std::string>(_select_region->getSelectedOptionValue());
	if (active.empty() || std::ranges::find(items, active) == items.end())
		active = items.empty() ? "" : items.front();

	_select_region->clear();
	for (const auto& region : items)
		_select_region->createOption(region, region);

	if (!active.empty())
		_select_region->setSelectedOptionValue(active);
}

void UiDnsHosts::_onRegionsChanged(JSArgs args)
{
	_saveRegions();
	_rebuildRegionSelect();

	// Только что добавленный пользователем регион проверяем на доступность.
	if (args.size() >= 2 && JSToCPP<std::string>(args[0]) == "add")
	{
		const auto region = JSToCPP<std::string>(args[1]);
		Core::get().addTask(
			// NOLINTNEXTLINE(bugprone-exception-escape) - worker callback; the availability check may allocate.
			[this, region]
			{
				_window_check_region->show();
				_checkRegionAvailability(region);
				_window_check_region->hide();
			}
		);
	}

	// DNS не пересобираем: активный регион меняется только выбором в селекте.
}

void UiDnsHosts::_checkRegionAvailability(std::string region)
{
	if (!_unblock->dnsHostsRegionAvailable(region))
	{
		Debug::warning("Region [{}] is not available, removed", region);
		_region_list->removeItem(region);
	}
}

void UiDnsHosts::_applyActiveRegion(const std::string& region)
{
	_ui->userConfig()->writeSectionParameter("SYSTEM", "dns_hosts_region", region);
	_unblock->setDnsHostsRegion(region);

	if (_ui->userConfig()->parameterSection<bool>("SYSTEM", "enable_dns_hosts").value_or(false))
		Core::get().addTask([this] { _updateDnsHosts(); });
}

void UiDnsHosts::_applyBaseUrl(const std::string& url)
{
	_ui->userConfig()->writeSectionParameter("SYSTEM", "dns_hosts_url", url);
	_unblock->setDnsHostsBaseUrl(url);

	if (_ui->userConfig()->parameterSection<bool>("SYSTEM", "enable_dns_hosts").value_or(false))
		Core::get().addTask([this] { _updateDnsHosts(); });
}

bool UiDnsHosts::_isValidHost(const std::string& host) const
{
	// Хост без схемы: домен или IP, опционально с портом.
	static const std::regex host_regex{ R"(^[a-zA-Z0-9]([a-zA-Z0-9.-]*[a-zA-Z0-9])?(:[0-9]{1,5})?$)" };
	return std::regex_match(host, host_regex);
}

void UiDnsHosts::_enableDnsHostsUpdate()
{
	auto result = _ui->userConfig()->parameterSection<bool>("SYSTEM", "enable_dns_hosts");
	if (result)
	{
		const bool state = result.value();
		_enable_dns_hosts->setState(state);

		if (state)
			_start_update_dns_hosts->show();
		else
			_start_update_dns_hosts->hide();

		_window_wait_response_from_server->show();

		Core::get().addTask(
			[this, state]
			{
				if (state && (!_unblock->dnsHostsCheck()))
				{
					_window_wait_response_from_server->hide();
					_window_wait_update_dns->show();
					_unblock->dnsHostsUpdate();
					_window_wait_update_dns->hide();
				}
				else
					_window_wait_response_from_server->hide();

				_unblock->dnsHosts(state);
			}
		);
	}
	else
	{
		_start_update_dns_hosts->hide();
		_enableDnsHostsWarningUser();
	}
}

void UiDnsHosts::_enableDnsHostsWarningUser()
{
	static std::string description{Localization::Str{"str_window_to_warn_enable_dns_hosts_description"}()};

	Core::get().addTask(
		[this]
		{
			_window_wait_response_from_server->show();
			std::string str_list_name{};
			auto&	   list_name = _unblock->dnsHostsListName();
			for (auto& name : list_name)
				str_list_name.append(name).append(", ");

			_window_wait_response_from_server->hide();

			str_list_name.pop_back();
			str_list_name.pop_back();

			_window_to_warn_enable_dns_hosts->setDescription(utils::format(description, str_list_name));
			_window_to_warn_enable_dns_hosts->show();
		}
	);
}
