try {
	!RUN_CPP
} catch (error) {
	var RUN_CPP = false;
}

if (!RUN_CPP) {

	createButton("footer", "link_to_github", "Follow on GitHub");

	addButtonEventClick("link_to_github", () => {
		window.open("https://github.com/MagilaWEB/unblock-youtube-discord", '_blank');
	});

	createButton("footer", "link_to_github2", "Follow on GitHub2");

	addButtonEventClick("link_to_github2", () => {
		window.open("https://github.com/MagilaWEB/unblock-youtube-discord", '_blank');
	});

	createSecondaryWindow("start_test_window", "Какое-то окно", "Описание окна");

	setDescriptionSecondaryWindow("start_test_window", "Использование готовых инструментов без знаний с целью нарушить баланс этого мира не хакерство, а мошенничество.");

	setTypeSecondaryWindow("start_test_window", 2);

	createButton(".buttons_start", "start_test", "Запустить тестирование");

	addButtonEventClick("start_test", () => {
		showSecondaryWindow("start_test_window");

		setTimeout(() => {
			//hideSecondaryWindow("start_test_window");
		}, 3000)
	});

	createButton(".buttons_start", "start_service", "Запустить службы");
	createButton(".buttons_stop", "stop_service", "Остановить службы");

	createListUl("#start_test_window .description", "success_domain", "Доступные домены:");
	const ul = getListUl("success_domain");
	ul.addClass("resource_availability");
	createListUlLiAddSuccess("success_domain", "YouTube текст", true);
	createListUlLiAddSuccess("success_domain", "Discord текст", false);
	createListUlLiAddSuccess("success_domain", "x.com текст", true);
	createListUlLiAddSuccess("success_domain", "Proxy BayDPI текст", false);
	createListUlLiAddSuccess("success_domain", "Discord текст", false);
	createListUlLiAddSuccess("success_domain", "x.com текст", true);
	createListUlLiAddSuccess("success_domain", "Proxy BayDPI текст", false);
	createListUlLiAddSuccess("success_domain", "Discord текст", false);
	createListUlLiAddSuccess("success_domain", "x.com текст", true);
	createListUlLiAddSuccess("success_domain", "Proxy BayDPI текст", false);
	createListUlLiAddSuccess("success_domain", "Discord текст", false);
	createListUlLiAddSuccess("success_domain", "x.com текст", true);
	createListUlLiAddSuccess("success_domain", "Proxy BayDPI текст", false);

	createListUl(".info_unblock", "success_service2", "Список активных сервисов:", true);
	createListUl(".info_unblock", "success_service3", "Список активных сервисов:", true);
	createListUl(".info_unblock", "success_service4", "Список активных сервисов:", true);
	createListUl(".info_unblock", "success_service5", "Список активных сервисов:", true);

	createListUl(".info_unblock", "success_service", "Список активных сервисов:");
	createListUlLiAddSuccess("success_service", "Unblock YouTube", true);
	createListUlLiAddSuccess("success_service", "Unblock Discord", false);
	createListUlLiAddSuccess("success_service", "Unblock x.com", true);
	createListUlLiAddSuccess("success_service", "Unblock Proxy BayDPI", false);

	createCheckBox("#setting section .unblock", "unblock", "Включаем что то", "DA");

	createButton("#setting section .unblock", "test", "Запустить тест");

	createCheckBox("#setting section .unblock", "unblock2", "тут тоже", "Какоето описание (тут тоже)");

	createListSelect("#setting section .unblock", "unblock3", "Список выбора", "Какоето описание 2")
	createSelectOption("unblock3", 1, "Элемент 1");
	createSelectOption("unblock3", 2, "Элемент 2");
	createSelectOption("unblock3", 3, "Элемент 3");
	createSelectOption("unblock3", 4, "Элемент 4");
	createSelectOption("unblock3", 5, "Элемент 5");

	addInputEventSubmit("testInput1", false, new_value => {
		console.log(new_value);
	});

	createListSelect("#setting section .proxy", "test_select", "Название списка выбора опции.", "Описание списка выбора опции.")
	createSelectOption("test_select", "1", "опция 1");
	createSelectOption("test_select", "2", "опция 2");
	createSelectOption("test_select", "3", "опция 3", true);
	createSelectOption("test_select", "4", "опция 4");

	addSelectEventChange("test_select", value => { console.dir(value); });

	console.dir(getSelectSelectedOption("test_select"));

	updateCountStartStopButtonToCss();
}
else
	setInterval(CPPTaskRun, 30);
