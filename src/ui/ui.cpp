#include "ui.h"

#include "../core/timer.h"
#include "../unblock/unblock.h"
#include "../engine/version.hpp"

Ui::Ui(IEngineAPI* engine) : UiElements(), _engine(engine)
{
	_file_user_setting->open({ Core::get().userPath() / "setting" }, ".config", true);
	_file_service_list->open({ Core::get().configsPath() / "service_setting" }, ".config", true);

	_file_service_list->forLineParametersSection(
		"LIST",
		[this](std::string key, std::string /*value*/)
		{
			_unblock_list_enable_services.emplace(key, std::make_shared<CheckBox>(std::string{ "_unblock_to_list_" } + key));
			return false;
		}
	);

#ifndef DEBUG
	auto result = _file_user_setting->parameterSection<bool>("SUSTEM", "show_console");
	if (result && result.value())
		_engine->console();
	else
		Debug::initLogFile();
#endif

	_overlay = Overlay::Create(_engine->window(), _engine->window()->width(), _engine->window()->height(), 0, 0);
	_overlay->view()->LoadURL("file:///main.html");

	_overlay->view()->set_load_listener(this);
	_overlay->view()->set_view_listener(this);
}

Ui::~Ui()
{
	_overlay->view()->set_load_listener(nullptr);
	_overlay->view()->set_view_listener(nullptr);
	_engine = nullptr;
}

#define LOGS(method)                                                                                        \
	method(                                                                                                 \
		"Java/Script\n\tsource:\t%d\n\ttype:\t%d\n\tmessage:\t%s\n\tline_number:\t%d\n\tcolumn_number:\t%d" \
		"\n\tsource_id:\t%s\n\tnum_arguments:\t%d\n\t%s",                                                   \
		static_cast<u32>(msg.source()),                                                                     \
		static_cast<u32>(msg.type()),                                                                       \
		msg.message().utf8().data(),                                                                        \
		msg.line_number(),                                                                                  \
		msg.column_number(),                                                                                \
		msg.source_id().utf8().data(),                                                                      \
		msg.num_arguments(),                                                                                \
		text_msg.c_str()                                                                                    \
	);

#ifdef DEBUG
void Ui::OnAddConsoleMessage(View* /*caller*/, const ConsoleMessage& msg)
{
	std::string text_msg{ "MSG: " };
	uint32_t	num_args = msg.num_arguments();

	if (num_args > 0)
	{
		for (uint32_t i = 0; i < num_args; i++)
		{
			JSValue arg = static_cast<JSValue>(msg.argument_at(i));
			text_msg.append(static_cast<String>(arg.ToString()).utf8().data()).append(" ");
		}
	}

	if (msg.level() == kMessageLevel_Log)
		LOGS(Debug::ok)
	else if (msg.level() == kMessageLevel_Debug || msg.level() == kMessageLevel_Info)
		LOGS(Debug::info)
	else if (msg.level() == kMessageLevel_Warning)
		LOGS(Debug::warning)
	else if (msg.level() == kMessageLevel_Error)
		LOGS(Debug::error)
}
#else
void Ui::OnAddConsoleMessage(View* /*caller*/, const ConsoleMessage& msg)
{
	std::string text_msg{ "MSG: " };
	uint32_t	num_args = msg.num_arguments();

	if (num_args > 0)
	{
		for (uint32_t i = 0; i < num_args; i++)
		{
			JSValue arg = static_cast<JSValue>(msg.argument_at(i));
			text_msg.append(static_cast<String>(arg.ToString()).utf8().data()).append(" ");
		}
	}

	if (msg.level() == kMessageLevel_Log)
		LOGS(InputConsole::textOk)
	else if (msg.level() == kMessageLevel_Debug || msg.level() == kMessageLevel_Info)
		LOGS(InputConsole::textInfo)
	else if (msg.level() == kMessageLevel_Warning)
		LOGS(InputConsole::textWarning)
	else if (msg.level() == kMessageLevel_Error)
		LOGS(InputConsole::textError)
}
#endif

void Ui::OnWindowObjectReady(View* caller, uint64_t /*frame_id*/, bool /*is_main_frame*/, const String& /*url*/)
{
	auto locked_context = caller->LockJSContext();
	SetJSContext(locked_context->ctx());

	JSObject global		  = JSGlobalObject();
	global["RUN_CPP"]	  = JSValue(true);
	global["VERSION_APP"] = JSValue(VERSION_STR);
	global["CPPTaskRun"]  = BindJSCallback(&Ui::runTask);
	global["CPPLangText"] = BindJSCallbackWithRetval(&Ui::langText);
}

void Ui::OnDOMReady(View* caller, uint64_t /*frame_id*/, bool /*is_main_frame*/, const String& /*url*/)
{
	Core::setThreadJsID(GetCurrentThreadId());

	auto locked_context = caller->LockJSContext();
	SetJSContext(locked_context->ctx());

	BaseElement::initializeAll(caller);

	_link_to_github->create("footer", "str_link_to_github");
	_link_to_github->addEventClick(
		[](JSArgs)
		{
			Core::get().addTask([] { system("start https://github.com/MagilaWEB/unblock-youtube-discord"); });
			return false;
		}
	);

	_checkConflictService();

	_setting();

	// HOME
	_startService();
	_startServiceWindow();
	_stopService();
	_testing();
}

void Ui::OnResize(ultralight::Window* /*window*/, uint32_t width, uint32_t height)
{
	_overlay->Resize(width, height);
}

void Ui::OnClose(ultralight::Window* /*window*/)
{
	BaseElement::release();
	_engine->app()->Quit();
}

void Ui::runTask(const JSObject& /*obj*/, const JSArgs& /*args*/)
{
	auto& task = Core::get().getTaskJS();
	FAST_LOCK(Core::get().getTaskLockJS());
	while (!task.empty())
	{
		task.front()();
		task.pop_front();
	}
}

JSValue Ui::langText(const JSObject& /*obj*/, const JSArgs& args)
{
	if (!args[0].IsString())
	{
		Debug::warning(
			"The passed argument in LANG_TEXT is not a string, it is necessary to pass the string value of the text_id of the localization system."
		);
		return "";
	}

	const auto text_id = static_cast<String>(args[0].ToString());
	return Localization::Str{ text_id.utf8().data() }();
}
