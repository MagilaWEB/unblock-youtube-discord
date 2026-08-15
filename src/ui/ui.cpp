#include "ui.h"
#include "ui_base.h"

Ui::Ui(std::shared_ptr<UiBase> ui_base) : _ui_base(std::move(ui_base))
{
	_unblock = std::make_shared<Unblock>();
}

void Ui::postConstruct()
{
	self = shared_from_this();

	_ui_dns_hosts = std::make_unique<UiDnsHosts>(self, _unblock);
	_ui_proxy_tg = std::make_unique<UiProxyTg>(self, _unblock);
	_ui_zapret2 = std::make_unique<UiZapret2>(self);
	_ui_unblock = std::make_unique<UiUnblock>(self);
}

void Ui::_initializeAppState()
{
	_checkValidRootDirectory();

	//_checkWhitelist();

	_updateApp();
	_checkConflictService();
}

void Ui::_initComponents()
{
	_ui_unblock->initialize();
	_ui_dns_hosts->initialize();
	_ui_proxy_tg->initialize();

	if (_ui_zapret2)
		_ui_zapret2->initialize();
}

void Ui::_initializeFooter()
{
	_footerElements();
	_removeApp();
}

void Ui::_initializeWindowBase() const
{
	_window_wait_start_service->create(Localization::Str{ "str_please_wait" }, "str_window_service_start_wait_description");
	_window_wait_start_service->setType(SecondaryWindow::Type::Info);
}

void Ui::initialize()
{
	if (_init)
		return;

	_initializeWindowBase();

	_initializeAppState();
	_initComponents();
	_initializeFooter();

	_init = true;
}

void Ui::jsUpdate()
{
	_ui_dns_hosts->updateInfoWindow();
	_ui_zapret2->updateHelperChecking();
	_ui_zapret2->updateHelperSeen();
	_ui_zapret2->updateHelperValid();
	_ui_zapret2->updateHelperError();
	_updateAppProgressWindowInfo();
}

namespace
{
	std::string reportIssueBody()
	{
		return Debug::buildReportIssueBody();
	}
} // namespace

void Ui::_footerElements()
{
	_link_to_github->create("footer", "str_link_to_github");
	_link_to_github->addEventClick(
		[](JSArgs)
		{
			Core::get().addTask([] { system("start https://github.com/MagilaWEB/unblock-youtube-discord"); });
			return false;
		}
	);

	_link_to_telegram->create("footer", "str_link_to_telegram");
	_link_to_telegram->addEventClick(
		[](JSArgs)
		{
			Core::get().addTask([] { system("start https://t.me/+OqRXcWFw4kpmMTcy"); });
			return false;
		}
	);

	_link_report_issue->create("footer", "str_link_report_issue");
	_link_report_issue->addEventClick(
		[](JSArgs)
		{
			Core::get().addTask(
				[]
				{
					Localization::Str title{ "str_issue_report_title" };
					Debug::openGitHubIssue(title(), reportIssueBody());
				}
			);
			return false;
		}
	);

	_button_report_issue_tutorial->create("#tutorial_issue_container", "str_link_report_issue");
	_button_report_issue_tutorial->addEventClick(
		[](JSArgs)
		{
			Core::get().addTask(
				[]
				{
					Localization::Str title{ "str_issue_report_title" };
					Debug::openGitHubIssue(title(), reportIssueBody());
				}
			);
			return false;
		}
	);
}

void Ui::_checkConflictService()
{
	_window_warning_conflict_service->create(Localization::Str{ "str_warning" }, "");
	_window_warning_conflict_service->setType(SecondaryWindow::Type::YesNo);

	auto description = Localization::Str{ "str_window_warning_conflict_service" }();

	auto& conflict_service = _unblock->getConflictingServices();
	if (!conflict_service.empty())
	{
		std::string names_services;
		for (auto& service : conflict_service)
			names_services.append(service.getName()).append(",");
		names_services.pop_back();

		_window_warning_conflict_service->setDescription(utils::format(description, names_services));

		_window_warning_conflict_service->show();

		_window_warning_conflict_service->addEventYesNo(
			[ui_self = self, &conflict_service](JSArgs args)
			{
				if (JSToCPP<bool>(args[0]))
					for (auto& service : conflict_service)
						service.remove();

				conflict_service.clear();

				ui_self->_window_warning_conflict_service->hide();

				return true;
			}
		);
	}
}

void Ui::_checkWhitelist()
{
	if (!_window_wait_test_whitelist->isCreate())
	{
		_window_wait_test_whitelist->create(Localization::Str{ "str_please_wait" }, "str_window_wait_test_whitelist_description");
		_window_wait_test_whitelist->setType(SecondaryWindow::Type::Wait);
	}

	_window_wait_test_whitelist->show();

	if (!_window_warning_whitelist->isCreate())
	{
		_window_warning_whitelist->create(Localization::Str{ "str_warning" }, "str_window_whitelist_description");
		_window_warning_whitelist->setType(SecondaryWindow::Type::OK);
	}

	if (!_window_warning_no_internet->isCreate())
	{
		_window_warning_no_internet->create(Localization::Str{ "str_warning" }, "str_window_warning_no_internet_description");
		_window_warning_no_internet->setType(SecondaryWindow::Type::OK);
	}

	Core::get().addTaskParallel(
		[ui_self = self]
		{
			if (ui_self->_unblock->testUrl("https://yandex.ru") || ui_self->_unblock->testUrl("https://vk.com"))
			{
				const bool state_block =
					ui_self->_unblock->testUrl("https://google.com") || ui_self->_unblock->testUrl("https://2ip.ru") || ui_self->_unblock->testUrl("https://github.com");

				ui_self->_window_wait_test_whitelist->hide();

				if (!state_block)
				{
					ui_self->_window_warning_whitelist->show();
					ui_self->_window_warning_whitelist->addEventOk(
						[inner_self = ui_self](JSArgs)
						{
							inner_self->_window_warning_whitelist->hide();
							return true;
						}
					);
				}
			}
			else
			{
				ui_self->_window_wait_test_whitelist->hide();
				ui_self->_window_warning_no_internet->show();
				ui_self->_window_warning_no_internet->addEventOk(
					[inner_self = ui_self](JSArgs)
					{
						inner_self->_window_warning_no_internet->hide();
						return true;
					}
				);
			}
		}
	);
}
