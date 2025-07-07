#include "pch.h"
#include "unblock.h"

Unblock::Unblock()
{
	allOpenService();
}

void Unblock::allOpenService()
{
	_win_divert.open();
	_unblock.open();
	_unblock2.open();
	_goodbay_dpi.open();
}

void Unblock::changeDpiApplicationType(DpiApplicationType type)
{
	_dpi_application_type = type;
}

void Unblock::startAuto()
{
	_strategies_dpi->changeIgnoringHostlist(_dpi_application_type == DpiApplicationType::ALL);

	while (true)
	{
		allRemoveService();

		_chooseStrategy();

		if (_type_strategy > (_strategies_dpi->getStrategySize() - 1))
		{
			InputConsole::textWarning("Не удалось для вас подобрать подходящию конфигурацию, запуск самого успешного варианта.");

			u32					saccess_max{ 0 };
			SuccessfulStrategy* strategy{ nullptr };

			for (u32 i = 0; i < _successful_strategies.size(); i++)
			{
				auto& successful_strategy = _successful_strategies[i];
				if (saccess_max < successful_strategy.success)
				{
					saccess_max = successful_strategy.success;
					strategy	= &successful_strategy;
				}
			}

			if (strategy)
			{
				const auto feke_bin_list = _strategies_dpi->getFekeBinList();

				_strategies_dpi->changeFakeKey(feke_bin_list[strategy->dpi_feke_bin]);
				_strategies_dpi->changeStrategy(strategy->index_strategy);

				_startService();
			}

			_dpi_feke_bin  = 0;
			_type_strategy = 0;

			break;
		}

		_strategies_dpi->changeStrategy(_type_strategy);

		_startService();

		testDomains();

		const auto success_rate = _domain_testing->successRate();

		if (success_rate <= 90)
		{
			if (_base_success_rate < success_rate)
				_successful_strategies.emplace_back(SuccessfulStrategy{ success_rate, _type_strategy, _dpi_feke_bin });
		}
		else
		{
			_successful_strategies.emplace_back(SuccessfulStrategy{ success_rate, _type_strategy, _dpi_feke_bin });

			_domain_testing->printTestInfo();
			InputConsole::textOk("Удалось подобрать конфигурацию [%s], результат теста выше 90%%.", _strategies_dpi->getStrategyFileName().c_str());
			InputConsole::textInfo("Служба уже настроена и заново открывать приложение не нужно даже после перезапуска ПК, пользуйтесь на здоровье!");
			InputConsole::pause();

			InputConsole::textPlease("проверьте работоспособность, если не работает можно продолжить подбор конфигурации", true, true);
			InputConsole::textAsk("Продолжить подбор конфигурации");
			if (!InputConsole::getBool())
				break;
		}

		_dpi_feke_bin++;
	}
}

void Unblock::baseTestDomain()
{
	allRemoveService();

	InputConsole::textInfo(
		"Рекомендуется запустить тестирование доменов без обхода блокировок. Это поможет подобрать для вас наиболее эффективную конфигурацию."
	);
	InputConsole::textAsk("Запустить тестирование");

	if (InputConsole::getBool())
	{
		testDomains();

		_base_success_rate = _domain_testing->successRate();
		_domain_testing->printTestInfo();

		if (_base_success_rate < 80)
			InputConsole::textWarning("Результат [%d%%] ниже 80%%, ваш провайдер сильно ограничивает доступ к ресурсам.", _base_success_rate);

		InputConsole::pause();
	}
}

void Unblock::testDomains() const
{
	if (_dpi_application_type == DpiApplicationType::ALL)
		_domain_testing->loadFile("domain_test_all");
	else
		_domain_testing->loadFile("domain_test_base");

	InputConsole::textInfo("Тестирование списка доменов из файла [%s].", _domain_testing->fileName().c_str());

	_domain_testing->test();
}

void Unblock::allRemoveService()
{
	_unblock.remove();
	_unblock2.remove();
	_goodbay_dpi.remove();
	_win_divert.remove();
}

void Unblock::_chooseStrategy()
{
	const auto feke_bin_list = _strategies_dpi->getFekeBinList();

	if (_dpi_feke_bin >= feke_bin_list.size())
	{
		_dpi_feke_bin = 0;
		_type_strategy++;
	}

	_strategies_dpi->changeFakeKey(feke_bin_list[_dpi_feke_bin]);
}

void Unblock::_startService()
{
	for (auto& [index, service] : indexStrategies)
	{
		const auto list = _strategies_dpi->getStrategy(index);
		if (!list.empty())
		{
			if (service.contains("unblock1"))
			{
				_unblock.setArgs(list);
				_unblock.create("DPI программное обеспечение для обхода блокировки.");
				_unblock.start();
			}

			if (service.contains("unblock2"))
			{
				_unblock2.setArgs(list);
				_unblock2.create("DPI программное обеспечение для обхода блокировки.");
				_unblock2.start();
			}

			if (service.contains("GoodbyeDPI"))
			{
				_goodbay_dpi.setArgs(list);
				_goodbay_dpi.create("GoodbyeDPI программное обеспечение для обхода блокировки.");
				_goodbay_dpi.start();
			}
		}
	}
}
