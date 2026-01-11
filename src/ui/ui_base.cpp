#include "ui_base.h"
#include "ui.h"

#include "../core/timer.h"
#include "../engine/version.hpp"


UiBase::UiBase(IEngineAPI* engine): _engine(engine)
{
	_ui = std::make_unique<Ui>(this);

	_overlay = Overlay::Create(_engine->window(), _engine->window()->width(), _engine->window()->height(), 0, 0);
	_overlay->view()->LoadURL("file:///main.html");

	_overlay->view()->set_load_listener(this);
	_overlay->view()->set_view_listener(this);
}

UiBase::~UiBase()
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
void UiBase::OnAddConsoleMessage(View* /*caller*/, const ConsoleMessage& msg)
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
void UiBase::OnAddConsoleMessage(View* /*caller*/, const ConsoleMessage& msg)
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

void UiBase::OnWindowObjectReady(View* caller, uint64_t /*frame_id*/, bool /*is_main_frame*/, const String& /*url*/)
{
	auto locked_context = caller->LockJSContext();
	SetJSContext(locked_context->ctx());

	JSObject global		  = JSGlobalObject();
	global["RUN_CPP"]	  = JSValue(true);
	global["VERSION_APP"] = JSValue(VERSION_STR);
	global["CPPTaskRun"]  = static_cast<JSCallback>(std::bind(&UiBase::runTask, this, std::placeholders::_1, std::placeholders::_2));
	global["CPPLangText"] = static_cast<JSCallbackWithRetval>(std::bind(&UiBase::langText, this, std::placeholders::_1, std::placeholders::_2));
}

void UiBase::OnDOMReady(View* caller, uint64_t /*frame_id*/, bool /*is_main_frame*/, const String& /*url*/)
{
	Core::setThreadJsID(GetCurrentThreadId());

	auto locked_context = caller->LockJSContext();
	SetJSContext(locked_context->ctx());

	BaseElement::initializeAll(caller);

	_ui->initialize();
}

void UiBase::OnResize(ultralight::Window* /*window*/, uint32_t width, uint32_t height)
{
	_overlay->Resize(width, height);
}

void UiBase::OnClose(ultralight::Window* /*window*/)
{
	BaseElement::release();
	_ui.release();
	_engine->app()->Quit();
}

const std::shared_ptr<File>& UiBase::userSetting()
{
	return _engine->userConfig();
}

void UiBase::runTask(const JSObject& /*obj*/, const JSArgs& /*args*/)
{
	auto& task = Core::get().getTaskJS();
	FAST_LOCK(Core::get().getTaskLockJS());
	while (!task.empty())
	{
		task.front()();
		task.pop_front();
	}

	_ui->jsUpdate();
}

JSValue UiBase::langText(const JSObject& /*obj*/, const JSArgs& args)
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
