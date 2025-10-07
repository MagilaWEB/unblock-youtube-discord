try {
	!RUN_APP
} catch (error) {
	createBotton("#home section", "start_test", "Запустить тестирование");
	// bottonEventClick("start_test", () =>{
	// 	console.dir("start_test");
	// })
	createBotton("#home section", "start_service", "Запустить службы", true);
	createBotton("#home section", "stop_service", "Остановить службы", true);

	createListUl(".success_domain", "success_domain");
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

	createListUl(".success_service", "success_service");
	createListUlLiAddSuccess("success_service", "Unblock YouTube", true);
	createListUlLiAddSuccess("success_service", "Unblock Discord", false);
	createListUlLiAddSuccess("success_service", "Unblock x.com", true);
	createListUlLiAddSuccess("success_service", "Unblock Proxy BayDPI", false);

	createListUl("#seting", "seting");
	createListUlLiAdd("seting", "Список Какойто текст 1");
	createListUlLiAdd("seting", "Список Какойто текст 2");
	createListUlLiAdd("seting", "Список Какойто текст 3");
	createListUlLiAdd("seting", "Список Какойто текст 4");

	// clearListUl("seting");

	createInput("#seting section", "testInput1", "number", "1080","Proxy DPI PORT",  "Какоето описание импута 1");
	createInput("#seting section", "testInput2", "text", "127.0.0.1", "Proxy DPI IP",  "Какоето описание импута 2");
	createInput("#seting section", "testInput3", "text", "text", "text",  "Какоето описание импута 3");

	addInputEventSumbit("testInput1", false,  new_value => {
		console.log(new_value);
	});

	createCheckBox("#seting section", "test1", "Proxy DPI", "Какоето описание 1");
	createCheckBox("#seting section", "test2", "Discord", "Какоето описание 2");
	createCheckBox("#seting section", "test3", "YouTube", "Какоето описание 3");
}
