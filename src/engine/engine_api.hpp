#pragma once
namespace ultralight
{
	class App;
	class Window;
}
class IEngineAPI
{
protected:
	IEngineAPI() = default;

public:
	IEngineAPI(IEngineAPI&&) = default;
	virtual ~IEngineAPI()	 = default;

	virtual ultralight::App*	app()	 = 0;
	virtual ultralight::Window* window() = 0;
};
