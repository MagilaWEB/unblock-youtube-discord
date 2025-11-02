#pragma once
namespace ultralight
{
	class App;
	class Window;
}

class IEngineAPI
{
public:
	virtual void				console() = 0;
	virtual ultralight::App*	app()	  = 0;
	virtual ultralight::Window* window()  = 0;
};
