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

	createSecondaryWindow("start_test_window", "Какое-то окно", "Описание окна");

	setDescriptionSecondaryWindow("start_test_window", "sdasdasdadasdasdasdad");

	setTypeSecondaryWindow("start_test_window", 2);

	createButton("#home section .button_start_stop", "start_test", "Запустить тестирование");

	addButtonEventClick("start_test", () => {
		showSecondaryWindow("start_test_window");

		setTimeout(() => {
			//hideSecondaryWindow("start_test_window");
		}, 3000)
	});

	createButton("#home section .button_start_stop", "start_service", "Запустить службы");
	createButton("#home section .button_start_stop", "start_service_proxy", "Запустить службы proxy");
	createButton("#home section .button_start_stop", "stop_service", "Остановить службы");

	createListUl(".info_unblock .success_domain", "success_domain", "Доступные домены:");
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

	createListUl(".info_unblock .success_service", "success_service", "Список активных сервисов:");
	createListUlLiAddSuccess("success_service", "Unblock YouTube", true);
	createListUlLiAddSuccess("success_service", "Unblock Discord", false);
	createListUlLiAddSuccess("success_service", "Unblock x.com", true);
	createListUlLiAddSuccess("success_service", "Unblock Proxy BayDPI", false);

	createCheckBox("#setting section .unblock", "unblock", "Включаем что то", "DA");

	createCheckBox("#setting section .unblock", "unblock2", "тут тоже", "Какоето описание (тут тоже)");

	createListUl("#setting section .unblock", "setting");
	createListUlLiAdd("setting", "Список Какойто текст 1");
	createListUlLiAdd("setting", "Список Какойто текст 2");
	createListUlLiAdd("setting", "Список Какойто текст 3");
	createListUlLiAdd("setting", "Список Какойто текст 4");

	// clearListUl("setting");

	createCheckBox("#setting section .proxy", "test1", "Proxy DPI", "Какоето описание 1");

	createInput("#setting section .proxy", "testInput1", "number", "1080", "Proxy DPI PORT", "Какоето описание импута 1");
	createInput("#setting section .proxy", "testInput2", "ip", "127.0.0.1", "Proxy DPI IP", "Какоето описание импута 2");
	createInput("#setting section .proxy", "testInput3", "text", "text", "text", "Какоето описание импута 3");

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
}
else
	setInterval(CPPTaskRun, 100);
