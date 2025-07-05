#pragma once

class IEngineAPI
{
protected:
	IEngineAPI() = default;

public:
	IEngineAPI(IEngineAPI&&) = default;
	virtual ~IEngineAPI()	 = default;
};
