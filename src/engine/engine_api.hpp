#pragma once

#include <memory>
#include <string>

#include <saucer/smartview.hpp>

#include "../core/file_system.h"

class IEngineAPI
{
protected:
	explicit IEngineAPI()						 = default;
	IEngineAPI(const IEngineAPI&)				 = default;
	IEngineAPI(IEngineAPI&&)					 = default;
	IEngineAPI& operator=(const IEngineAPI&)		 = default;
	IEngineAPI& operator=(IEngineAPI&&)			 = default;
	virtual ~IEngineAPI()						 = default;

public:
	virtual saucer::smartview*			   webview()  = 0;
	virtual std::shared_ptr<saucer::window> window()	 = 0;
	virtual void						   showConsole()  = 0;
	virtual void						   hideConsole()  = 0;
	virtual void						   quit()	 = 0;
	virtual std::shared_ptr<File>&		   userConfig()  = 0;
	virtual bool						   hasCyrillicOrSpaceInBinaryPath()  = 0;
};
