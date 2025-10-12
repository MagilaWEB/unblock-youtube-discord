#include "pch.h"
#include "ui.h"
#include "../unblock/unblock.h"

Ui::Ui(IEngineAPI* engine) : _engine(engine)
{
	_overlay = Overlay::Create(_engine->window(), _engine->window()->width(), _engine->window()->height(), 0, 0);
	_overlay->view()->LoadURL("file:///main.html");

	_overlay->view()->set_load_listener(this);
	_overlay->view()->set_view_listener(this);
	
	_unblock = std::make_unique<Unblock>();
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
void Ui::OnAddConsoleMessage(View* caller, const ConsoleMessage& msg)
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
void Ui::OnAddConsoleMessage(View* caller, const ConsoleMessage& msg)
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

void Ui::OnWindowObjectReady(View* caller, uint64_t frame_id, bool is_main_frame, const String& url)
{
	auto locked_context = caller->LockJSContext();
	SetJSContext(locked_context->ctx());

	JSObject global	  = JSGlobalObject();
	global["RUN_CPP"] = JSValue(true);
	global["CPPTaskRun"] = BindJSCallback(&Ui::RunTask);
}

void Ui::OnDOMReady(View* caller, uint64_t frame_id, bool is_main_frame, const String& url)
{
	Core::setThreadJsID(GetCurrentThreadId());
	auto locked_context = caller->LockJSContext();
	SetJSContext(locked_context->ctx());
	BaseElement::initialize_all(caller);

	_testing();
}

void Ui::OnClose(ultralight::Window* window)
{
	BaseElement::release();
	_engine->app()->Quit();
}

void Ui::RunTask(const JSObject& obj, const JSArgs& args)
{
	CRITICAL_SECTION_RAII(Core::getTaskLockJS());
	auto& task = Core::getTaskJS();
	while (!task.empty())
	{
		task.back()();
		task.pop();
	}
}
