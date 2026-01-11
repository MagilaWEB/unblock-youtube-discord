#pragma once
namespace ultralight
{
	class App;
	class Window;
}

class IEngineAPI
{
protected:
	explicit IEngineAPI()					 = default;
	IEngineAPI(const IEngineAPI&)			 = default;
	IEngineAPI(IEngineAPI&&)				 = default;
	IEngineAPI& operator=(const IEngineAPI&) = default;
	IEngineAPI& operator=(IEngineAPI&&)		 = default;
	virtual ~IEngineAPI()					 = default;

public:
	virtual void				   console()	= 0;
	virtual std::shared_ptr<File>& userConfig() = 0;
	virtual ultralight::App*	   app()		= 0;
	virtual ultralight::Window*	   window()		= 0;
};
