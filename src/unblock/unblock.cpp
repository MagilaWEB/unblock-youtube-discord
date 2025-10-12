#include "pch.h"
#include "unblock.h"

Unblock::Unblock()
{
	_file_service->open((Core::get().configsPath() / "service"), ".config", true);
	_file_service->forLineSection(
		"UNBLOCK",
		[this](auto str_line)
		{
			_services.emplace(str_line.c_str(), ServiceUnblock{ str_line.c_str() });
			Debug::ok("%s ", str_line.c_str());
			return false;
		}
	);

	/*_file_user_setting->open((Core::get().userPath() / "setting"), ".config", true);

	const auto IP = _file_user_setting->parameterSection("PROXY_DATA", "IP");
	if (IP)
		_proxy_data.IP = IP.value();

	const auto PORT = _file_user_setting->parameterSection("PROXY_DATA", "PORT");
	if (PORT)
	{
		try
		{
			_proxy_data.PORT = std::stoul(PORT.value());
		}
		catch (std::invalid_argument& error)
		{
			Debug::error("_proxy_data.PORT %s", true, error.what());
		}
		catch (std::out_of_range& error)
		{
			Debug::error("_proxy_data.PORT %s", true, error.what());
		}
		catch (...)
		{
			Debug::error("_proxy_data.PORT invalid_argument", true);
		}
	}*/

	_win_divert.open();
}
//
//void Unblock::changeAccurateTest(bool state)
//{
//	_accurate_test = state;
//	_domain_testing->changeAccurateTest(state);
//}
//
//void Unblock::changeDpiApplicationType(DpiApplicationType type)
//{
//	_dpi_application_type = type;
//}
//
//bool Unblock::checkSavedConfiguration(bool proxy)
//{
//	if (!proxy)
//		_strategies_dpi->changeIgnoringHostlist(_dpi_application_type == DpiApplicationType::ALL);
//
//	if (const auto config = _file_user_setting->parameterSection(proxy ? "PROXY_DATA" : "remember_configuration", "config"))
//	{
//		InputConsole::textInfo("Обнаружена ранее используемая конфигурация!");
//		InputConsole::textAsk("Применить сохранённую конфигурацию");
//		if (InputConsole::getBool())
//		{
//			if (proxy)
//			{
//				proxyRemoveService();
//
//				_proxy_strategies_dpi->changeStrategy(config.value().c_str());
//
//				_bay_dpi.setArgs(_proxy_strategies_dpi->getStrategy());
//				_bay_dpi.create("Proxy DPI программное обеспечение для обхода блокировки.");
//				_bay_dpi.start();
//			}
//			else
//			{
//				allRemoveService();
//
//				if (const auto config_fake_bin = _file_user_setting->parameterSection("remember_configuration", "fake_bin"))
//					_strategies_dpi->changeFakeKey(config_fake_bin.value().c_str());
//
//				_strategies_dpi->changeStrategy(config.value().c_str());
//
//				_startService();
//			}
//
//			if (!testDomains(false, proxy))
//				return false;
//		}
//	}
//
//	return true;
//}
//
//void Unblock::startAuto()
//{
//	changeAccurateTest(false);
//
//	while (true)
//	{
//		allRemoveService();
//
//		_chooseStrategy();
//
//		if (_type_strategy > (_strategies_dpi->getStrategySize() - 1))
//		{
//			if (!_accurate_test)
//			{
//				const u32 select = InputConsole::selectFromList(
//					{ "Повторить подбор конфигурации в режиме точного тестирования.", "Выбрать конфигурацию в ручную." },
//					[](u32)
//					{
//						InputConsole::textWarning("Не удалось подобрать для вас подходящую конфигурацию!");
//						InputConsole::textAsk("Почему не удалось подобрать конфигурацию");
//						InputConsole::textInfo("Причины могут быть разные.");
//						InputConsole::textInfo(
//							"Возможно у вас медленное интернет соединение, в таком случае вам поможет подбор в режиме точного тестирование."
//						);
//						InputConsole::textInfo("Возможно для обхода блокировок мешает брандмауэр.");
//						InputConsole::textInfo("Включен VPN сервис.");
//						InputConsole::textInfo("Или просто для вас нет подходящей конфигурации.");
//						InputConsole::textInfo(
//							"Вы можете автоматически подобрать конфигурацию в режиме точного тестирования, или выбрать конфигурацию в ручную."
//						);
//					}
//				);
//
//				changeAccurateTest(select == 0);
//
//				if (_accurate_test)
//				{
//					_dpi_fake_bin  = 0;
//					_type_strategy = 0;
//					continue;
//				}
//				else
//				{
//					startManual();
//					break;
//				}
//			}
//
//			InputConsole::textWarning("Не удалось для вас подобрать подходящую конфигурацию, запуск самого успешного варианта.");
//
//			u32					success_max{ 0 };
//			SuccessfulStrategy* strategy{ nullptr };
//
//			for (u32 i = 0; i < _successful_strategies.size(); i++)
//			{
//				auto& successful_strategy = _successful_strategies[i];
//				if (success_max < successful_strategy.success)
//				{
//					success_max = successful_strategy.success;
//					strategy	= &successful_strategy;
//				}
//			}
//
//			if (strategy)
//			{
//				const auto& fake_bin_list = _strategies_dpi->getFakeBinList();
//
//				_strategies_dpi->changeFakeKey(fake_bin_list[strategy->dpi_fake_bin].key);
//				_strategies_dpi->changeStrategy(strategy->index_strategy);
//
//				_file_user_setting->writeSectionParameter("remember_configuration", "config", _strategies_dpi->getStrategyFileName().c_str());
//				_file_user_setting->writeSectionParameter("remember_configuration", "fake_bin", _strategies_dpi->getKeyFakeBin().c_str());
//
//				_startService();
//
//				testDomains();
//				testDomains(true);
//			}
//
//			_dpi_fake_bin  = 0;
//			_type_strategy = 0;
//
//			break;
//		}
//
//		_strategies_dpi->changeStrategy(_type_strategy);
//
//		_startService();
//
//		if (_dpi_application_type == DpiApplicationType::ALL)
//			_domain_testing->loadFile("domain_test_all");
//		else
//			_domain_testing->loadFile("domain_test_base");
//
//		_domain_testing->test(false);
//
//		const auto success_rate = _domain_testing->successRate();
//
//		if (success_rate <= MAX_SUCCESS_CONECTION)
//		{
//			if (_accurate_test)
//				_successful_strategies.emplace_back(SuccessfulStrategy{ success_rate, _type_strategy, _dpi_fake_bin });
//		}
//		else
//		{
//			_successful_strategies.emplace_back(SuccessfulStrategy{ success_rate, _type_strategy, _dpi_fake_bin });
//			_file_user_setting->writeSectionParameter("remember_configuration", "config", _strategies_dpi->getStrategyFileName().c_str());
//			_file_user_setting->writeSectionParameter("remember_configuration", "fake_bin", _strategies_dpi->getKeyFakeBin().c_str());
//
//			_domain_testing->printTestInfo();
//			InputConsole::textOk(
//				"Удалось подобрать конфигурацию [%s], результат теста выше %d%%.",
//				_strategies_dpi->getStrategyFileName().c_str(),
//				MAX_SUCCESS_CONECTION
//			);
//			InputConsole::textInfo("Служба уже настроена и заново открывать приложение не нужно даже после перезапуска ПК, пользуйтесь на здоровье!");
//			InputConsole::pause();
//
//			testDomains(true);
//
//			const u32 select = InputConsole::selectFromList(
//				{ "Далее.", "Продолжить подбор конфигурации." },
//				[](u32)
//				{ InputConsole::textPlease("проверьте работоспособность, если не работает можно продолжить подбор конфигурации", true, true); }
//			);
//			if (select == 0)
//				break;
//		}
//
//		_dpi_fake_bin++;
//	}
//}
//
//void Unblock::startManual()
//{
//	while (true)
//	{
//		const auto&			   fake_bin_list = _strategies_dpi->getFakeBinList();
//		std::list<std::string> list_fake_bin;
//		for (const auto& fake_bin : fake_bin_list)
//			list_fake_bin.push_back(fake_bin.key);
//
//		InputConsole::selectFromList(
//			list_fake_bin,
//			[&](u32 select)
//			{
//				InputConsole::textPlease("выберете под что подделывать ваш трафик", true, true);
//
//				_strategies_dpi->changeFakeKey(fake_bin_list[select].key);
//			}
//		);
//
//		const auto&			   config_list = _strategies_dpi->getStrategyList();
//		std::list<std::string> config_list_name;
//		for (const auto& name : config_list)
//			config_list_name.push_back(name);
//
//		InputConsole::selectFromList(
//			config_list_name,
//			[&](u32 select)
//			{
//				InputConsole::textPlease("выберете одну из конфигураций", true, true);
//
//				_strategies_dpi->changeStrategy(select);
//			}
//		);
//
//		allRemoveService();
//
//		_startService();
//
//		testDomains();
//
//		if (InputConsole::selectFromList({ "Далее.", "Выбрать другую конфигурацию." }) == 1)
//			continue;
//
//		_file_user_setting->writeSectionParameter("remember_configuration", "config", _strategies_dpi->getStrategyFileName().c_str());
//		_file_user_setting->writeSectionParameter("remember_configuration", "fake_bin", _strategies_dpi->getKeyFakeBin().c_str());
//
//		break;
//	}
//
//	changeAccurateTest(false);
//}
//
//void Unblock::startProxyManual()
//{
//	changeDpiApplicationType(DpiApplicationType::BASE);
//
//	while (true)
//	{
//		u32 select = InputConsole::selectFromList(
//			{ "Далее.", "Изменить настройки proxy." },
//			[this](u32)
//			{
//				InputConsole::textInfo("Proxy Unblock это система обхода блокировок на основе BayDPI, позволяющая запустить локальный proxy сервер.");
//				InputConsole::textInfo("IP по умолчанию 127.0.0.1 , установлен IP:%s", _proxy_data.IP.c_str());
//				InputConsole::textInfo("PORT по умолчанию 1080 , установлен PORT:%d", _proxy_data.PORT);
//				InputConsole::textInfo("Авторизация не требуется, подключаться по протоколу SOCKS5.");
//				InputConsole::textInfo("Вы можете изменит параметры по умолчанию.");
//			}
//		);
//
//		if (select == 1)
//		{
//			InputConsole::textPlease("Введите IP формата 127.0.0.1", true, true);
//			const auto string_api = InputConsole::getString();
//			_proxy_data.IP		  = string_api;
//			_file_user_setting->writeSectionParameter("PROXY_DATA", "IP", _proxy_data.IP.c_str());
//
//			while (true)
//			{
//				InputConsole::textPlease("Введите желаемый PORT от 1080 до 65534", true, true);
//				const auto port = InputConsole::getU32();
//
//				if (port < 1'080)
//				{
//					InputConsole::textError("PORT не может быть меньше чем 1080!");
//					continue;
//				}
//
//				if (port >= u16(-1))
//				{
//					InputConsole::textError("PORT не может быть больше чем 65534!");
//					continue;
//				}
//
//				_proxy_data.PORT = port;
//				_file_user_setting->writeSectionParameter("PROXY_DATA", "PORT", std::to_string(_proxy_data.PORT).c_str());
//
//				break;
//			}
//			continue;
//		}
//
//		_proxy_strategies_dpi->changeProxyData(_proxy_data);
//
//		if (checkSavedConfiguration(true))
//		{
//			if (InputConsole::selectFromList({ "Автоматический подбор конфигурации.", "Выбрать в ручную." }) == 0)
//			{
//				_domain_testing->loadFile("domain_test_base");
//				_domain_testing->proxyEnable(true);
//				_domain_testing->changeProxy(_proxy_data.IP, _proxy_data.PORT);
//
//				changeAccurateTest(false);
//				const auto& config_list = _proxy_strategies_dpi->getStrategyList();
//				for (const auto& config : config_list)
//				{
//					_proxy_strategies_dpi->changeStrategy(config.c_str());
//					proxyRemoveService();
//
//					_bay_dpi.setArgs(_proxy_strategies_dpi->getStrategy());
//					_bay_dpi.create("Proxy DPI программное обеспечение для обхода блокировки.");
//					_bay_dpi.start();
//
//					_domain_testing->test(false);
//
//					const auto success_rate = _domain_testing->successRate();
//
//					if (success_rate > MAX_SUCCESS_CONECTION)
//					{
//						_domain_testing->printTestInfo();
//
//						InputConsole::textOk(
//							"Удалось подобрать конфигурацию [%s], результат теста выше %d%%.",
//							_proxy_strategies_dpi->getStrategyFileName().c_str(),
//							MAX_SUCCESS_CONECTION
//						);
//
//						InputConsole::textOk(
//							"Proxy доступен по IP:%s, PORT:%d, воспользуйтесь протоколом SOCKS5 для подключения, авторизация не нужна.",
//							_proxy_data.IP.c_str(),
//							_proxy_data.PORT
//						);
//
//						InputConsole::textInfo(
//							"Служба уже настроена и заново открывать приложение не нужно даже после перезапуска ПК, пользуйтесь на здоровье!"
//						);
//
//						InputConsole::pause();
//
//						testDomains(true);
//
//						const u32 select = InputConsole::selectFromList(
//							{ "Далее.", "Продолжить подбор конфигурации." },
//							[](u32)
//							{
//								InputConsole::textPlease(
//									"проверьте работоспособность, если не работает можно продолжить подбор конфигурации",
//									true,
//									true
//								);
//							}
//						);
//
//						if (select == 0)
//							break;
//					}
//				}
//
//				_domain_testing->proxyEnable(true);
//			}
//			else
//			{
//				const auto&			   config_list = _proxy_strategies_dpi->getStrategyList();
//				std::list<std::string> config_list_name;
//				for (const auto& name : config_list)
//					config_list_name.push_back(name);
//
//				InputConsole::selectFromList(
//					config_list_name,
//					[&](u32 select)
//					{
//						InputConsole::textPlease("выберете одну из конфигураций", true, true);
//
//						_proxy_strategies_dpi->changeStrategy(select);
//					}
//				);
//
//				proxyRemoveService();
//
//				_bay_dpi.setArgs(_proxy_strategies_dpi->getStrategy());
//				_bay_dpi.create("Proxy DPI программное обеспечение для обхода блокировки.");
//				_bay_dpi.start();
//
//				testDomains(false, true);
//				testDomains(true, true);
//			}
//
//			if (InputConsole::selectFromList({ "Далее.", "Выбрать другую конфигурацию." }) == 1)
//				continue;
//
//			_file_user_setting->writeSectionParameter("PROXY_DATA", "config", _proxy_strategies_dpi->getStrategyFileName().c_str());
//		}
//
//		break;
//	}
//}
//
//void Unblock::proxyRemoveService()
//{
//	_bay_dpi.remove();
//}
//
//void Unblock::allRemoveService()
//{
//	_unblock.remove();
//	_unblock2.remove();
//	_goodbay_dpi.remove();
//	_win_divert.remove();
//}
//
//bool Unblock::testDomains(bool video, bool proxy)
//{
//	u32 select{};
//
//	if (video)
//	{
//		select = InputConsole::selectFromList(
//			{ "Быстрый тест доступности видеороликов YouTube.",
//			  "Точный тест доступности видеороликов YouTube (Займет больше времени).",
//			  "Пропустить." },
//			[](u32)
//			{
//				InputConsole::textInfo(
//					"Вы можете протестировать доступность видеороликов на YouTube, отрицательный результат не гарантирует что доступа нет."
//				);
//			}
//		);
//	}
//	else
//	{
//		select = InputConsole::selectFromList(
//			{ "Быстрый тест работоспособности.", "Точный тест работоспособности (Займет больше времени).", "Пропустить." },
//			[](u32) { InputConsole::textInfo("Вы можете протестировать работоспособности."); }
//		);
//	}
//
//	if (select == 2)
//		return false;
//
//	changeAccurateTest(select == 1);
//
//	if (proxy)
//	{
//		_domain_testing->proxyEnable(true);
//		_domain_testing->changeProxy(_proxy_data.IP, _proxy_data.PORT);
//	}
//
//	if (!video)
//		if (_dpi_application_type == DpiApplicationType::ALL && !proxy)
//			_domain_testing->loadFile("domain_test_all");
//		else
//			_domain_testing->loadFile("domain_test_base");
//	else
//		_domain_testing->loadFile("domain_video");
//
//	InputConsole::textInfo("Тестирование списка доменов из файла [%s].", _domain_testing->fileName().c_str());
//
//	_domain_testing->test(video);
//
//	_domain_testing->printTestInfo();
//
//	_domain_testing->proxyEnable(false);
//
//	changeAccurateTest(false);
//
//	const auto success_rate = _domain_testing->successRate();
//
//	if (video)
//	{
//		if (success_rate > 10)
//			InputConsole::textOk("Видеоролики YouTube доступны с вероятностью [%d%%]", success_rate);
//		else
//			InputConsole::textWarning("Не удалось определить доступны ли видеоролики YouTube, это не означает что доступа совсем нет, это означает "
//									  "что возможны перебои с загрузкой.");
//		InputConsole::pause();
//		return false;
//	}
//	else if (success_rate < MAX_SUCCESS_CONECTION)
//	{
//		const auto config = proxy ? _proxy_strategies_dpi->getStrategyFileName() : _strategies_dpi->getStrategyFileName();
//		InputConsole::textWarning("С конфигурацией [%s] обнаружена проблема, успех ниже %d%%!", config.c_str(), MAX_SUCCESS_CONECTION);
//		InputConsole::textInfo("Рекомендуется запустить подбор новой конфигурации!");
//		InputConsole::textAsk("Перейти к подбору новой конфигурации");
//
//		if (!InputConsole::getBool())
//			return false;
//	}
//	else
//	{
//		InputConsole::textOk("Работоспособность свыше 90%%!");
//		InputConsole::pause();
//		return false;
//	}
//
//	return true;
//}
//
//void Unblock::_chooseStrategy()
//{
//	const auto& fake_bin_list = _strategies_dpi->getFakeBinList();
//
//	if (_dpi_fake_bin >= fake_bin_list.size())
//	{
//		_dpi_fake_bin = 0;
//		_type_strategy++;
//	}
//
//	_strategies_dpi->changeFakeKey(fake_bin_list[_dpi_fake_bin].key);
//}
//
//void Unblock::_startService()
//{
//	for (auto& [index, service] : indexStrategies)
//	{
//		const auto list = _strategies_dpi->getStrategy(index);
//		if (!list.empty())
//		{
//			std::string str_service{ service };
//
//			if (str_service.contains("unblock1"))
//			{
//				_unblock.setArgs(list);
//				_unblock.create("DPI программное обеспечение для обхода блокировки.");
//				_unblock.start();
//			}
//
//			if (str_service.contains("unblock2"))
//			{
//				_unblock2.setArgs(list);
//				_unblock2.create("DPI программное обеспечение для обхода блокировки.");
//				_unblock2.start();
//			}
//
//			if (str_service.contains("GoodbyeDPI"))
//			{
//				_goodbay_dpi.setArgs(list);
//				_goodbay_dpi.create("GoodbyeDPI программное обеспечение для обхода блокировки.");
//				_goodbay_dpi.start();
//			}
//		}
//	}
//}
