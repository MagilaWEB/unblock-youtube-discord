#include "ui.h"

#include "../unblock/unblock.h"

void Ui::_testingInit()
{
	_active_service->create("#home section .info_unblock", "str_h2_active_service", true);

	_activeServiceUpdate();

	_list_domain->create("#home section .info_unblock", "str_h2_verified_domains");
	_list_domain_video->create("#home section .info_unblock", "str_h2_verified_domains_video");

	_list_proxy_domain->create("#home section .info_unblock", "str_h2_verified_proxy_domains");
	_list_proxy_domain_video->create("#home section .info_unblock", "str_h2_verified_proxy_domains_video");

	_start_testing->create(".buttons_start", "str_b_start_testing");
	_start_testing->addEventClick(
		[this](JSArgs)
		{
			if (_unblock->runTest<StrategiesDPI>() || _unblock->runTest<StrategiesDPI>(true))
				return false;

			if (_unblock->runTest<ProxyStrategiesDPI>() || _unblock->runTest<ProxyStrategiesDPI>(true))
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
	{
		_list_domain->show();
		_list_domain_video->show();
	}
	else
	{
		_list_domain->hide();
		_list_domain_video->hide();
	}

	if (_proxy_enable->getState())
	{
		_list_proxy_domain->show();
		_list_proxy_domain_video->show();
	}
	else
	{
		_list_proxy_domain->hide();
		_list_proxy_domain_video->hide();
	}

	// button start testing
	if (_unblock_enable->getState() || _proxy_enable->getState())
		_start_testing->show();
	else
		_start_testing->hide();
}

void Ui::_testingWindow()
{
	_window_wait_testing->create(Localization::Str{ "str_please_wait" }, "str_secondary_window_description_wait_domain");
	_window_wait_testing->setType(SecondaryWindow::Type::Wait);

	_window_wait_testing->addEventCancel(
		[this](JSArgs)
		{
			_unblock->testingDomainCancel<StrategiesDPI>();
			_unblock->testingDomainCancel<StrategiesDPI>(true);
			_unblock->testingDomainCancel<ProxyStrategiesDPI>();
			_unblock->testingDomainCancel<ProxyStrategiesDPI>(true);
			return false;
		}
	);

	if (_testing_domains_startup->getState())
		_testingServiceDomains();
}

void Ui::_activeServiceUpdate()
{
	_active_service->clear();
	_unblock->checkStateServices([this](pcstr name, bool state) { _active_service->createLiSuccess(name, state); });
}

void Ui::_testingServiceDomains()
{
	_list_domain->clear();
	_list_domain_video->clear();
	_list_proxy_domain->clear();
	_list_proxy_domain_video->clear();

	_window_wait_testing->show();

	if (_unblock_enable->getState())
	{
		Core::get().addTaskParallel(
			[this]
			{ _unblock->testingDomain<StrategiesDPI>([this](pcstr url, bool state) { _list_domain->createLiSuccess(url, state); }, false, false); }
		);

		for (auto& [name, check_box] : _unblock_list_enable_services)
		{
			if (name.contains("youtube") && check_box->getState())
			{
				Core::get().addTaskParallel(
					[this]
					{
						_unblock->testingDomain<StrategiesDPI>(
							[this](pcstr url, bool state) { _list_domain_video->createLiSuccess(url, state); },
							true,
							false
						);
					}
				);

				break;
			}
		}
	}

	if (_proxy_enable->getState())
	{
		Core::get().addTaskParallel(
			[this]
			{
				_unblock->testingDomain<ProxyStrategiesDPI>(
					[this](pcstr url, bool state) { _list_proxy_domain->createLiSuccess(url, state); },
					false,
					false
				);
			}
		);

		Core::get().addTaskParallel(
			[this]
			{
				_unblock->testingDomain<ProxyStrategiesDPI>(
					[this](pcstr url, bool state) { _list_proxy_domain_video->createLiSuccess(url, state); },
					true,
					false
				);
			}
		);
	}

	Core::get().taskComplete([this] { _window_wait_testing->hide(); });
}
