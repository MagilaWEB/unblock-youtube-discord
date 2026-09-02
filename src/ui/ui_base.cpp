#include "ui_base.h"
#include "ui.h"
#include "../engine/version.hpp"

// ------------------ Constructor ------------------
UiBase::UiBase(IEngineAPI* engine) : _engine(engine)
{
}

void UiBase::postConstruct()
{
	auto ui = std::make_shared<Ui>(shared_from_this());
	ui->postConstruct();
	_ui = ui;
}

// ------------------ Destructor ------------------
UiBase::~UiBase()
{
	_ui.reset();
	_engine = nullptr;
}

// ------------------ Setup (JS bridge + window subscriptions) ------------------
void UiBase::setup(saucer::smartview* view)
{
	if (!view)
		return;

	// Global variables for the UI page (injected before the scripts load).
	view->inject(
		{ .code	 = "window.RUN_CPP = true; window.VERSION_APP = " + jsQuote(VERSION_STR) + ";",
		  .run_at = saucer::script::time::creation }
	);

	// JS -> CPP: translate a string by language key.
	view->expose("CPPLangText", [this](std::string text_id) { return langText(std::move(text_id)); });

	// Save window size to config.
	view->parent().on<saucer::window::event::resize>(
		[this](int width, int height)
		{
			_engine->userConfig()->writeSectionParameter("WINDOW", "width", std::to_string(width));
			_engine->userConfig()->writeSectionParameter("WINDOW", "height", std::to_string(height));
		}
	);

	// Window close — full UI reset (the engine shuts itself down on the last closed window).
	view->parent().on<saucer::window::event::closed>([this]() { _closeWindow(); });

	// DOM ready — build the widget tree.
	view->once<saucer::webview::event::dom_ready>([this]() { _domReady(); });
}

void UiBase::_closeWindow()
{
	BaseElement::release();
	_ui.reset();
}

void UiBase::_domReady()
{
	auto* view = _engine->webview();
	if (!view)
		return;

	// Page title shown in Task Manager for the WebView2 process: "Unblock <version>".
	view->execute("document.title = {}", jsQuote(std::string("Unblock ") + VERSION_STR));

	BaseElement::initializeAll(view);
	_ui->initialize();
}

void UiBase::OnClose(saucer::application*)
{
	BaseElement::release();
	_engine->quit();
}

// ------------------ Config ------------------
const std::shared_ptr<File>& UiBase::userConfig()
{
	return _engine->userConfig();
}

void UiBase::console(bool show)
{
	show ? _engine->showConsole() : _engine->hideConsole();
}

void UiBase::update()
{
	if (_ui)
		_ui->update();
}

std::string UiBase::langText(std::string_view text_id)
{
	if (text_id.empty())
	{
		Debug::warning("The passed argument in LANG_TEXT is empty");
		return "";
	}

	return Localization::Str{ text_id }();
}