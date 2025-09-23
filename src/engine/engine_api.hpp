#pragma once
typedef void App;

class IEngineAPI
{
protected:
	IEngineAPI() = default;

public:
	IEngineAPI(IEngineAPI&&) = default;
	virtual ~IEngineAPI()	 = default;
};
