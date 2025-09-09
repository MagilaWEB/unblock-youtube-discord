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

void Unblock::changeAccurateTest(bool state)
{
	_accurate_test = state;
	_domain_testing->changeAccurateTest(state);
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
				if (success_rate < MAX_SUCCESS_CONECTION)
				{
					InputConsole::textWarning(
						"С конфигурацией [%s] обнаружена проблема, успех ниже %d%%!",
						config.value().c_str(),
						MAX_SUCCESS_CONECTION
					);
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
	changeAccurateTest(false);

	while (true)
	{
		allRemoveService();

		_chooseStrategy();

		if (_type_strategy > (_strategies_dpi->getStrategySize() - 1))
		{
			if (!_accurate_test)
			{
				const u32 select = InputConsole::selectFromList(
					{ "Повторить подбор конфигурации в режиме точного тестирования.", "Выбрать конфигурацию в ручную." },
					[](u32)
					{
						InputConsole::textWarning("Не удалось подобрать для вас подходящую конфигурацию!");
						InputConsole::textAsk("Почему не удалось подобрать конфигурацию");
						InputConsole::textInfo("Причины могут быть разные.");
						InputConsole::textInfo(
							"Возможно у вас медленное интернет соединение, в таком случае вам поможет подбор в режиме точного тестирование."
						);
						InputConsole::textInfo("Возможно для обхода блокировок мешает брандмауэр.");
						InputConsole::textInfo("Включен VPN сервис.");
						InputConsole::textInfo("Или просто для вас нет подходящей конфигурации.");
						InputConsole::pause();

						InputConsole::textInfo("По умолчанию тестирование производится в режиме быстрого тестирования.");
						InputConsole::textInfo(
							"Вы можете автоматически подобрать конфигурацию в режиме точного тестирования, или выбрать конфигурацию в ручную."
						);
					}
				);

				changeAccurateTest(select == 0);

				if (_accurate_test)
				{
					_dpi_fake_bin  = 0;
					_type_strategy = 0;
					continue;
				}
				else
				{
					InputConsole::textPlease("выберите конфигурацию в ручную", true, true);
					InputConsole::pause();
					startManual();
					break;
				}
			}

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

		if (success_rate <= MAX_SUCCESS_CONECTION)
		{
			if (_accurate_test)
				_successful_strategies.emplace_back(SuccessfulStrategy{ success_rate, _type_strategy, _dpi_fake_bin });
		}
		else
		{
			_successful_strategies.emplace_back(SuccessfulStrategy{ success_rate, _type_strategy, _dpi_fake_bin });
			_file_user_setting->writeSectionParameter("remember_configuration", "config", _strategies_dpi->getStrategyFileName().c_str());
			_file_user_setting->writeSectionParameter("remember_configuration", "fake_bin", _strategies_dpi->getKeyFakeBin().c_str());

			_domain_testing->printTestInfo();
			InputConsole::textOk(
				"Удалось подобрать конфигурацию [%s], результат теста выше %d%%.",
				_strategies_dpi->getStrategyFileName().c_str(),
				MAX_SUCCESS_CONECTION
			);
			InputConsole::textInfo("Служба уже настроена и заново открывать приложение не нужно даже после перезапуска ПК, пользуйтесь на здоровье!");
			InputConsole::pause();

			_testVideo();

			const u32 select = InputConsole::selectFromList(
				{ "Продолжить подбор конфигурации.", "Выход." },
				[](u32)
				{ InputConsole::textPlease("проверьте работоспособность, если не работает можно продолжить подбор конфигурации", true, true); }
			);
			if (select == 1)
				break;
		}

		_dpi_fake_bin++;
	}
}

void Unblock::startManual()
{
	while (true)
	{
		const auto&			   fake_bin_list = _strategies_dpi->getFakeBinList();
		std::list<std::string> list_fake_bin;
		for (const auto& fake_bin : fake_bin_list)
			list_fake_bin.push_back(fake_bin.key);

		InputConsole::selectFromList(
			list_fake_bin,
			[&](u32 select)
			{
				InputConsole::textPlease("выберете под что подделывать ваш трафик", true, true);

				_strategies_dpi->changeFakeKey(fake_bin_list[select].key);
			}
		);

		const auto&			   config_list = _strategies_dpi->getStrategyList();
		std::list<std::string> config_list_name;
		for (const auto& name : config_list)
			config_list_name.push_back(name);

		InputConsole::selectFromList(
			config_list_name,
			[&](u32 select)
			{
				InputConsole::textPlease("выберете одну из конфигураций", true, true);

				_strategies_dpi->changeStrategy(select);
			}
		);

		allRemoveService();

		_startService();
		const u32 select = InputConsole::selectFromList(
			{ "Быстрый тест работоспособности.", "Точный тест работоспособности (Займет больше времени).", "Пропустить." }
		);
		if (select == 0 || select == 1)
		{
			changeAccurateTest(select == 1);
			testDomains();

			_domain_testing->printTestInfo();

			const auto success_rate = _domain_testing->successRate();

			if (success_rate > MAX_SUCCESS_CONECTION)
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

		if (InputConsole::selectFromList({ "Выбрать другую конфигурацию.", "Продолжить." }) == 0)
			continue;

		_file_user_setting->writeSectionParameter("remember_configuration", "config", _strategies_dpi->getStrategyFileName().c_str());
		_file_user_setting->writeSectionParameter("remember_configuration", "fake_bin", _strategies_dpi->getKeyFakeBin().c_str());

		break;
	}

	changeAccurateTest(false);
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
		if (_dpi_application_type == DpiApplicationType::ALL)
			_domain_testing->loadFile("domain_test_all");
		else
			_domain_testing->loadFile("domain_test_base");
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
	const u32 select = InputConsole::selectFromList(
		{ "Быстрый тест доступности видеороликов YouTube.", "Точный тест доступности видеороликов YouTube (Займет больше времени).", "Пропустить." },
		[](u32)
		{
			InputConsole::textInfo(
				"Вы можете протестировать доступность видеороликов на YouTube, отрицательный результат не гарантирует что доступа нет."
			);
		}
	);

	if (select == 0 || select == 1)
	{
		changeAccurateTest(select == 1);
		testDomains(true);

		const auto success_rate_video = _domain_testing->successRate();
		_domain_testing->printTestInfo();

		if (success_rate_video > 10)
			InputConsole::textOk("Видеоролики YouTube доступны с вероятностью [%d%%]", success_rate_video);
		else
			InputConsole::textWarning("Не удалось определить доступны ли видеоролики YouTube, это не означает что доступа совсем нет, это означает "
									  "что возможны перебои с загрузкой.");

		changeAccurateTest(false);

		InputConsole::pause();
	}
}
