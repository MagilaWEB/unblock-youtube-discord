#include "pch.h"
#include "ui.h"

Ui::Ui()
{
	Platform::instance().set_file_system(GetPlatformFileSystem("./../ui/"));

	_app = App::Create();

	_window = Window::Create(_app->main_monitor(), 900, 600, false, kWindowFlags_Titled);
	_window->SetTitle("Unblock");

	_overlay = Overlay::Create(_window, _window->width(), _window->height(), 0, 0);
	_overlay->view()->LoadURL("file:///main.html");

	_window->set_listener(this);
	_overlay->view()->set_view_listener(this);

	 _app->Run();
}

Ui::~Ui()
{
}

void Ui::Run()
{
	_app->Run();
}

void Ui::OnClose(ultralight::Window* window)
{
	_app->Quit();
}
