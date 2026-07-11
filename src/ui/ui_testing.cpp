#include "ui.h"

#include "../unblock/unblock.h"

void Ui::_testingInit()
{

	_list_domain->create("#zapret section", "str_h2_verified_domains");

	_start_testing_zapret->create("#zapret .common", "str_b_start_testing_zapret");
	_start_testing_zapret->addEventClick(
		[this](JSArgs)
		{
			if (_unblock.runTest())
				return false;

			_testingServiceDomains();
			return false;
		}
	);

	_testingWindow();
	_testingUpdate();
}

void Ui::_testingUpdate()
{
	// list domains
	_list_domain->show();

	// button start testing
	_start_testing_zapret->show();
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

void Ui::_testingServiceDomains()
{
	_list_domain->clear();

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
