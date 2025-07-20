#include "pch.h"
#include "unblock.h"

Unblock::Unblock()
{
	_file_user_setting->open((Core::get().userPath() / "setting"), ".config", true);
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

bool Unblock::checkSavedConfiguration()
{
	_strategies_dpi->changeIgnoringHostlist(_dpi_application_type == DpiApplicationType::ALL);

	if (const auto config = _file_user_setting->parameterSection("remember_configuration", "config"))
	{
		InputConsole::textInfo("Обнаружена ранее используемая конфигурация!");
		InputConsole::textAsk("Применить сохранённую конфигурацию");
		if (InputConsole::getBool())
		{
			allRemoveService();

			if (const auto config_fake_bin = _file_user_setting->parameterSection("remember_configuration", "fake_bin"))
				_strategies_dpi->changeFakeKey(config_fake_bin.value().c_str());

			_strategies_dpi->changeStrategy(config.value().c_str());

			_startService();

			InputConsole::textAsk("Протестировать работоспособность");
			if (InputConsole::getBool())
			{
				testDomains();

				_domain_testing->printTestInfo();

				const auto success_rate = _domain_testing->successRate();
				if (success_rate < 90)
				{
					InputConsole::textWarning("С конфигурацией [%s] обнаружена проблема, успех ниже 90%%!", config.value().c_str());
					InputConsole::textInfo("Рекомендуется запустить подбор новой конфигурации!");
					InputConsole::textAsk("Перейти к подбору новой конфигурации");

					if (!InputConsole::getBool())
						return false;
				}
				else
				{
					InputConsole::textOk("Работоспособность свыше 90%%!");
					_testVideo();
					return false;
				}
			}
			else
				return false;
		}
	}

	return true;
}

void Unblock::startAuto()
{
	while (true)
	{
		allRemoveService();

		_chooseStrategy();

		if (_type_strategy > (_strategies_dpi->getStrategySize() - 1))
		{
			InputConsole::textWarning("Не удалось для вас подобрать подходящую конфигурацию, запуск самого успешного варианта.");

			u32					success_max{ 0 };
			SuccessfulStrategy* strategy{ nullptr };

			for (u32 i = 0; i < _successful_strategies.size(); i++)
			{
				auto& successful_strategy = _successful_strategies[i];
				if (success_max < successful_strategy.success)
				{
					success_max = successful_strategy.success;
					strategy	= &successful_strategy;
				}
			}

			if (strategy)
			{
				const auto& fake_bin_list = _strategies_dpi->getFakeBinList();

				_strategies_dpi->changeFakeKey(fake_bin_list[strategy->dpi_fake_bin].key);
				_strategies_dpi->changeStrategy(strategy->index_strategy);

				_file_user_setting->writeSectionParameter("remember_configuration", "config", _strategies_dpi->getStrategyFileName().c_str());
				_file_user_setting->writeSectionParameter("remember_configuration", "fake_bin", _strategies_dpi->getKeyFakeBin().c_str());

				_startService();

				_testVideo();
			}

			_dpi_fake_bin  = 0;
			_type_strategy = 0;

			break;
		}

		_strategies_dpi->changeStrategy(_type_strategy);

		_startService();

		testDomains();

		const auto success_rate = _domain_testing->successRate();

		if (success_rate <= 90)
			_successful_strategies.emplace_back(SuccessfulStrategy{ success_rate, _type_strategy, _dpi_fake_bin });
		else
		{
			_successful_strategies.emplace_back(SuccessfulStrategy{ success_rate, _type_strategy, _dpi_fake_bin });
			_file_user_setting->writeSectionParameter("remember_configuration", "config", _strategies_dpi->getStrategyFileName().c_str());
			_file_user_setting->writeSectionParameter("remember_configuration", "fake_bin", _strategies_dpi->getKeyFakeBin().c_str());

			_domain_testing->printTestInfo();
			InputConsole::textOk("Удалось подобрать конфигурацию [%s], результат теста выше 90%%.", _strategies_dpi->getStrategyFileName().c_str());
			InputConsole::textInfo("Служба уже настроена и заново открывать приложение не нужно даже после перезапуска ПК, пользуйтесь на здоровье!");
			InputConsole::pause();

			_testVideo();

			InputConsole::textPlease("проверьте работоспособность, если не работает можно продолжить подбор конфигурации", true, true);
			InputConsole::textAsk("Продолжить подбор конфигурации");
			if (!InputConsole::getBool())
				break;
		}

		_dpi_fake_bin++;
	}
}

void Unblock::startManual()
{
	bool ran{ true };
	while (ran)
	{
		InputConsole::textPlease("выберете под что подделывать ваш трафик", true, true);

		u32				 it{ 1 };
		std::vector<u32> list_it{};
		std::list<u8>	 list_kay_it{};

		bool finish{ false };

		const auto& fake_bin_list	= _strategies_dpi->getFakeBinList();
		auto		send_input_fake = [&]()
		{
			const u8 num = InputConsole::sendNum(list_kay_it);
			_strategies_dpi->changeFakeKey(fake_bin_list[list_it[num - 1]].key);
			it = 1;
			list_it.clear();
			list_kay_it.clear();
		};

		for (u32 i = 0; i < fake_bin_list.size(); i++, it++)
		{
			list_it.push_back(i);
			list_kay_it.push_back(it);

			InputConsole::textInfo("%d : %s", it, fake_bin_list[i].key.c_str());

			if (it == 9)
			{
				InputConsole::textInfo("0 : далее");
				list_kay_it.push_back(0);
				send_input_fake();
				if (finish)
					break;
			}
		}

		if (!finish)
			send_input_fake();
		else
			finish = false;

		InputConsole::textPlease("выберете одну из конфигураций", true, true);

		const auto& config_list = _strategies_dpi->getStrategyList();

		auto send_input_strategy = [&]()
		{
			list_kay_it.push_back(0);

			const u8 num = InputConsole::sendNum(list_kay_it);
			if (num > 0)
			{
				allRemoveService();
				_strategies_dpi->changeStrategy(list_it[num - 1]);

				_startService();

				InputConsole::textAsk("Протестировать работоспособность");
				if (InputConsole::getBool())
				{
					testDomains();

					_domain_testing->printTestInfo();

					const auto success_rate = _domain_testing->successRate();

					if (success_rate > 90)
					{
						InputConsole::textOk("Конфигурация успешно работает, результат теста %d%%.", success_rate);
						_testVideo();
					}
					else
					{
						InputConsole::textWarning("Конфигурация работает плохо, результат теста %d%%.", success_rate);
						InputConsole::textInfo("Выберите другую конфигурацию, или попробуйте автоматический режим.");
					}
				}

				InputConsole::textAsk("Выбрать другую конфигурацию");
				ran = InputConsole::getBool();

				_file_user_setting->writeSectionParameter("remember_configuration", "config", _strategies_dpi->getStrategyFileName().c_str());
				_file_user_setting->writeSectionParameter("remember_configuration", "fake_bin", _strategies_dpi->getKeyFakeBin().c_str());

				finish = true;
			}

			it = 0;
			list_it.clear();
			list_kay_it.clear();
		};

		for (u32 i = 0; i < config_list.size(); i++, it++)
		{
			list_it.push_back(i);
			list_kay_it.push_back(it);

			InputConsole::textInfo("%d : %s", it, config_list[i].c_str());

			if (it == 9)
			{
				InputConsole::textInfo("0 : далее");
				send_input_strategy();
				if (finish)
					break;
			}
		}

		if (!finish)
		{
			InputConsole::textInfo("0 : выход");
			send_input_strategy();
		}
	}
}

void Unblock::allRemoveService()
{
	_unblock.remove();
	_unblock2.remove();
	_goodbay_dpi.remove();
	_win_divert.remove();
}

void Unblock::testDomains(bool video) const
{
	if (!video)
	{
		if (_dpi_application_type == DpiApplicationType::ALL)
			_domain_testing->loadFile("domain_test_all");
		else
			_domain_testing->loadFile("domain_test_base");
	}
	else
		_domain_testing->loadFile("domain_video");

	InputConsole::textInfo("Тестирование списка доменов из файла [%s].", _domain_testing->fileName().c_str());

	_domain_testing->test(video);
}

void Unblock::_chooseStrategy()
{
	const auto& fake_bin_list = _strategies_dpi->getFakeBinList();

	if (_dpi_fake_bin >= fake_bin_list.size())
	{
		_dpi_fake_bin = 0;
		_type_strategy++;
	}

	_strategies_dpi->changeFakeKey(fake_bin_list[_dpi_fake_bin].key);
}

void Unblock::_startService()
{
	for (auto& [index, service] : indexStrategies)
	{
		const auto list = _strategies_dpi->getStrategy(index);
		if (!list.empty())
		{
			std::string str_service{ service };

			if (str_service.contains("unblock1"))
			{
				_unblock.setArgs(list);
				_unblock.create("DPI программное обеспечение для обхода блокировки.");
				_unblock.start();
			}

			if (str_service.contains("unblock2"))
			{
				_unblock2.setArgs(list);
				_unblock2.create("DPI программное обеспечение для обхода блокировки.");
				_unblock2.start();
			}

			if (str_service.contains("GoodbyeDPI"))
			{
				_goodbay_dpi.setArgs(list);
				_goodbay_dpi.create("GoodbyeDPI программное обеспечение для обхода блокировки.");
				_goodbay_dpi.start();
			}
		}
	}
}

void Unblock::_testVideo()
{
	InputConsole::textInfo("Вы можете протестировать доступность видеороликов на YouTube, отрицательный результат не гарантирует что доступа нет.");
	InputConsole::textAsk("Протестировать доступность видеороликов YouTube");
	if (InputConsole::getBool())
	{
		testDomains(true);

		const auto success_rate_video = _domain_testing->successRate();
		_domain_testing->printTestInfo();

		if (success_rate_video > 10)
			InputConsole::textOk("Видеоролики YouTube доступны с вероятностью [%d%%]", success_rate_video);
		else
			InputConsole::textWarning("Не удалось определить доступны ли видеоролики YouTube, это не означает что доступа совсем нет, это означает "
									  "что возможны перебои с загрузкой.");

		InputConsole::pause();
	}
}
