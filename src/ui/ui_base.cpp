#include "ui_base.h"
#include "ui.h"
#include "../core/timer.h"
#include "../engine/version.hpp"

// ------------------ Constructor ------------------
UiBase::UiBase(IEngineAPI* engine) : _engine(engine)
{
	_ui = std::make_unique<Ui>(this);

	_overlay = Overlay::Create(_engine->window(), _engine->window()->width(), _engine->window()->height(), 0, 0);
	_overlay->view()->LoadURL("file:///main.html");

	_overlay->view()->set_load_listener(this);
	_overlay->view()->set_view_listener(this);
}

// ------------------ Destructor ------------------
UiBase::~UiBase()
{
	if (_overlay)
	{
		_overlay->view()->set_load_listener(nullptr);
		_overlay->view()->set_view_listener(nullptr);
	}
	_engine = nullptr;
}

// ------------------ WindowListener ------------------
void UiBase::OnResize(ultralight::Window*, uint32_t width, uint32_t height)
{
	if (_overlay)
		_overlay->Resize(width, height);

	_engine->userConfig()->writeSectionParameter("WINDOW", "width", std::to_string(width));
	_engine->userConfig()->writeSectionParameter("WINDOW", "height", std::to_string(height));
}

void UiBase::OnClose(ultralight::Window*)
{
	BaseElement::release();
	_ui.release();
	_engine->app()->Quit();
}

// ------------------ JS callbacks ------------------
void UiBase::closeWindow(const JSObject&, const JSArgs&)
{
	OnClose(_engine->window());
}

const std::shared_ptr<File>& UiBase::userSetting()
{
	return _engine->userConfig();
}

void UiBase::console(bool show)
{
	show ? _engine->showConsole() : _engine->hideConsole();
}

void UiBase::runTask(const JSObject&, const JSArgs&)
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

JSValue UiBase::langText(const JSObject&, const JSArgs& args)
{
	if (!args[0].IsString())
	{
		Debug::warning("The passed argument in LANG_TEXT is not a string");
		return "";
	}
	const auto text_id = static_cast<String>(args[0].ToString());
	return Localization::Str{ text_id.utf8().data() }().data();
}

// ------------------ LoadListener ------------------
void UiBase::OnWindowObjectReady(View* caller, uint64_t, bool, const String&)
{
	auto locked_context = caller->LockJSContext();
	SetJSContext(locked_context->ctx());

	JSObject global		   = JSGlobalObject();
	global["RUN_CPP"]	   = JSValue(true);
	global["VERSION_APP"]  = JSValue(VERSION_STR);
	global["CPPTaskRun"]   = static_cast<JSCallback>(std::bind(&UiBase::runTask, this, std::placeholders::_1, std::placeholders::_2));
	global["CPPLangText"]  = static_cast<JSCallbackWithRetval>(std::bind(&UiBase::langText, this, std::placeholders::_1, std::placeholders::_2));
	global["WINDOW_CLOSE"] = static_cast<JSCallback>(std::bind(&UiBase::closeWindow, this, std::placeholders::_1, std::placeholders::_2));
}

void UiBase::OnDOMReady(View* caller, uint64_t, bool, const String&)
{
	Core::setThreadJsID(GetCurrentThreadId());

	auto locked_context = caller->LockJSContext();
	SetJSContext(locked_context->ctx());

	BaseElement::initializeAll(caller);
	_ui->initialize();
}

// --------------- Console message (debug/release) ---------------
#define LOGS(method)                                                                                        \
	method(                                                                                                 \
		"Java/Script\n\tsource:\t{}\n\ttype:\t{}\n\tmessage:\t{}\n\tline_number:\t{}\n\tcolumn_number:\t{}" \
		"\n\tsource_id:\t{}\n\tnum_arguments:\t{}\n\t{}",                                                   \
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
