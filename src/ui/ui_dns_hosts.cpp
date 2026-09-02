#include "ui_dns_hosts.h"

#include "ui.h"
#include "../unblock/unblock.h"

UiDnsHosts::UiDnsHosts(std::shared_ptr<Ui> ui, std::shared_ptr<Unblock> unblock)
    : _ui(std::move(ui)), _unblock(std::move(unblock))
{
}

void UiDnsHosts::initialize()
{
	_window_wait_response_from_server->create(Localization::Str{"str_please_wait"},
											  "str_window_wait_response_from_server_description");
	_window_wait_response_from_server->setType(SecondaryWindow::Type::Info);

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
			Core::get().addTask(
				[this]
				{
					_window_wait_update_dns->show();
					_unblock->dnsHostsUpdate();
					_unblock->dnsHosts(false);
					_unblock->dnsHosts(true);
					_window_wait_update_dns->hide();
				}
			);
			return false;
		}
	);

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
