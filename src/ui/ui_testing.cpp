#include "ui.h"

#include "../unblock/unblock.h"

void Ui::_checkConflictService()
{
	_window_warning_conflict_service->create(Localization::Str{ "str_warning" }, "");
	_window_warning_conflict_service->setType(SecondaryWindow::Type::YesNo);

	static Localization::Str lang_disc{ "str_window_warning_conflict_service" };

	std::string description = lang_disc();

	auto & conflict_service = _unblock->getConflictingServices();
	if (!conflict_service.empty())
	{
#if __clang__
		[[clang::no_destroy]]
#endif
		static std::string names_services;

		for (auto& service : conflict_service)
			names_services.append(service.getName()).append(",");
		names_services.pop_back();

		description = utils::format(description.c_str(), names_services.c_str());
		_window_warning_conflict_service->setDescription(description.c_str());

		_window_warning_conflict_service->show();

		_window_warning_conflict_service->addEventYesNo(
			[this, &conflict_service](JSArgs args)
			{
				if (args[0].ToBoolean())
					for (auto& service : conflict_service)
						service.remove();

				conflict_service.clear();

				_window_warning_conflict_service->hide();

				return true;
			}
		);
	}
}

void Ui::_activeService()
{
	_active_service->remove();
	_active_service->create("#home section .info_unblock", "str_h2_active_service", true);
	_unblock->checkStateServices([this](pcstr name, bool state) { _active_service->createLiSuccess(name, state); });
}

void Ui::_testing()
{
	_list_domain->remove();
	_list_domain_video->remove();

	if (_unblock_enable->getState())
	{
		_list_domain_video->create("#home section .info_unblock", "str_h2_verified_domains_video", true);
		_list_domain->create("#home section .info_unblock", "str_h2_verified_domains", true);
	}

	_list_proxy_domain->remove();
	_list_proxy_domain_video->remove();

	if (_proxy_enable->getState())
	{
		_list_proxy_domain->create("#home section .info_unblock", "str_h2_verified_proxy_domains");
		_list_proxy_domain_video->create("#home section .info_unblock", "str_h2_verified_proxy_domains_video");
	}

	_start_testing->remove();

	if (_unblock_enable->getState() || _proxy_enable->getState())
		_start_testing->create("#home section .button_start_stop", "str_b_start_testing");

	_testingWindow();

	_activeService();
}

void Ui::_testingWindow()
{
	auto event_test_domain = [this]
	{
		_window_wait_testing->show();
		FastLock lock;

		{
			std::jthread _0{ [this, &lock]
							 {
								 lock.EnterShared();
								 if (_unblock_enable->getState())
								 {
									 lock.LeaveShared();
									 _unblock->testingDomain<StrategiesDPI>(
										 [this](pcstr url, bool state) { _list_domain->createLiSuccess(url, state); },
										 false,
										 false
									 );
									 return;
								 }
								 lock.LeaveShared();
							 } };

			std::jthread _1{ [this, &lock]
							 {
								 lock.EnterShared();
								 if (_unblock_enable->getState())
								 {
									 lock.LeaveShared();
									 _unblock->testingDomain<StrategiesDPI>(
										 [this](pcstr url, bool state) { _list_domain_video->createLiSuccess(url, state); },
										 true,
										 false
									 );
									 return;
								 }
								 lock.LeaveShared();
							 } };

			std::jthread _2{ [this, &lock]
							 {
								 lock.EnterShared();
								 if (_proxy_enable->getState())
								 {
									 lock.LeaveShared();
									 _unblock->testingDomain<ProxyStrategiesDPI>(
										 [this](pcstr url, bool state) { _list_proxy_domain->createLiSuccess(url, state); },
										 false,
										 false
									 );
									 return;
								 }
								 lock.LeaveShared();
							 } };

			std::jthread _3{ [this, &lock]
							 {
								 lock.EnterShared();
								 if (_proxy_enable->getState())
								 {
									 lock.LeaveShared();
									 _unblock->testingDomain<ProxyStrategiesDPI>(
										 [this](pcstr url, bool state) { _list_proxy_domain_video->createLiSuccess(url, state); },
										 true,
										 false
									 );
									 return;
								 }
								 lock.LeaveShared();
							 } };
		}

		_window_wait_testing->hide();
	};

	static std::atomic_bool created_window{ false };
	if (!created_window)
	{
		created_window = true;

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
			Core::get().addTask(event_test_domain);
	}

	_start_testing->addEventClick(
		[this, event_test_domain](JSArgs)
		{
			if (_unblock->runTest<StrategiesDPI>() || _unblock->runTest<StrategiesDPI>(true))
				return false;

			if (_unblock->runTest<ProxyStrategiesDPI>() || _unblock->runTest<ProxyStrategiesDPI>(true))
				return false;

			if (_unblock_enable->getState())
			{
				_list_domain_video->clear();
				_list_domain->clear();
			}
			Core::get().addTask(event_test_domain);
			return false;
		}
	);
}
