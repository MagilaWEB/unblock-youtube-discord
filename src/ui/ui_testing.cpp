#include "ui.h"

#include "../unblock/unblock.h"

void Ui::_testingInit()
{
	_active_service->create("#home section .info_unblock", "str_h2_active_service", true);

	_activeServiceUpdate();

	_list_domain->create("#home section .info_unblock", "str_h2_verified_domains");

	_start_testing->create(".buttons_start", "str_b_start_testing");
	_start_testing->addEventClick(
		[this](JSArgs)
		{
			if (_unblock.runTest())
				return false;

			_testingServiceDomains();
			return false;
		}
	);

	if (_updateCountStartStopButtonToCss)
		_updateCountStartStopButtonToCss({});

	_testingWindow();
	_testingUpdate();
}

void Ui::_testingUpdate()
{
	// list domains
	if (_unblock_enable->getState())
		_list_domain->show();
	else
		_list_domain->hide();

	// button start testing
	if (_unblock_enable->getState())
		_start_testing->show();
	else
		_start_testing->hide();
}

void Ui::_testingWindow()
{
	_window_wait_testing->create(Localization::Str{ "str_please_wait" }, "str_secondary_window_description_wait_domain");
	_window_wait_testing->setType(SecondaryWindow::Type::Wait);

	_list_domain_to_modal->create("#_window_wait_testing .description", "str_h2_verified_domains");

	_window_wait_testing->addEventCancel(
		[this](JSArgs)
		{
			_unblock.testingDomainCancel();
			return false;
		}
	);

	if (_testing_domains_startup->getState())
		_testingServiceDomains();
}

void Ui::_activeServiceUpdate()
{
	_active_service->clear();
	_unblock.checkStateServices([this](std::string_view name, bool state) { _active_service->createLiSuccess(name, state); });
}

void Ui::_testingServiceDomains()
{
	_list_domain->clear();

	if (_unblock_enable->getState())
	{
		_window_wait_testing->show();

		Core::get().addTaskParallel(
			[this]
			{
				_unblock.testingDomain(
					[this](std::string_view url, bool state)
					{
						_list_domain->createLiSuccess(url, state);
						_list_domain_to_modal->createLiSuccess(url, state);
					},
					false
				);
			}
		);

		Core::get().taskComplete(
			[this]
			{
				_window_wait_testing->hide();
				_list_domain_to_modal->clear();
			}
		);
	}
}
