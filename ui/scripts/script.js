try {
	!RUN_CPP
} catch (error) {
	var RUN_CPP = false;
}

if(!RUN_CPP)
{
	createButton("#home section", "start_test", "Запустить тестирование");
	addButtonEventClick("start_test", () =>{
		console.dir("addButtonEventClick start_test");
	})
	createButton("#home section", "start_service", "Запустить службы", true);
	createButton("#home section", "stop_service", "Остановить службы", true);

	createListUl(".success_domain", "success_domain", "Доступные домены:");
	const ul = getClassListUl("success_domain");
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

	createListUl(".success_service", "success_service", "Список активных сервисов:");
	createListUlLiAddSuccess("success_service", "Unblock YouTube", true);
	createListUlLiAddSuccess("success_service", "Unblock Discord", false);
	createListUlLiAddSuccess("success_service", "Unblock x.com", true);
	createListUlLiAddSuccess("success_service", "Unblock Proxy BayDPI", false);

	createListUl("#setting", "setting");
	createListUlLiAdd("setting", "Список Какойто текст 1");
	createListUlLiAdd("setting", "Список Какойто текст 2");
	createListUlLiAdd("setting", "Список Какойто текст 3");
	createListUlLiAdd("setting", "Список Какойто текст 4");

	// clearListUl("setting");

	createInput("#setting section", "testInput1", "number", "1080","Proxy DPI PORT",  "Какоето описание импута 1");
	createInput("#setting section", "testInput2", "text", "127.0.0.1", "Proxy DPI IP",  "Какоето описание импута 2");
	createInput("#setting section", "testInput3", "text", "text", "text",  "Какоето описание импута 3");

	addInputEventSubmit("testInput1", false,  new_value => {
		console.log(new_value);
	});

	createListSelect("#setting section", "test_select", "Название списка выбора опции.", "Описание списка выбора опции.")
	createSelectOption("test_select", "1", "опция 1");
	createSelectOption("test_select", "2", "опция 2");
	createSelectOption("test_select", "3", "опция 3", true);
	createSelectOption("test_select", "4", "опция 4");

	addSelectEventChange("test_select", value => { console.dir(value); });

	console.dir(getSelectSelectedOption("test_select"));

	createCheckBox("#setting section", "test1", "Proxy DPI", "Какоето описание 1");
	createCheckBox("#setting section", "test2", "Discord", "Какоето описание 2");
	createCheckBox("#setting section", "test3", "YouTube", "Какоето описание 3");
}
else
	setInterval(CPPTaskRun, 100);
