#include "pch.h"
#include "ui.h"

Ui::Ui(std::shared_ptr<IEngineAPI> engine) : _engine(engine)
{
	_overlay = Overlay::Create(_engine->window(), _engine->window()->width(), _engine->window()->height(), 0, 0);
	_overlay->view()->LoadURL("file:///main.html");

	_engine->window()->set_listener(this);
	_overlay->view()->set_view_listener(this);
}

Ui::~Ui()
{
}

void Ui::Run()
{

}

void Ui::OnClose(ultralight::Window* window)
{
	_engine->app()->Quit();
}
