#pragma once
#include "unblock_api.hpp"
#include "domain_testing.h"
#include "strategies_dpi.h"
#include "proxy_strategies_dpi.h"

#include "../core/service.h"

class UNBLOCK_API Unblock final : public IUnblockAPI
{
	Ptr<FileSystem>			_file_service;
	Ptr<FileSystem>			_file_user_setting;
	Service					_win_divert{ "WinDivert" };

	struct ServiceUnblock
	{
		Service unblock{ "", "winws.exe" };
		Service unblock_alt{ "", "winws.exe" };
		Service goodbay_dpi{ "", "goodbyedpi.exe" };

		ServiceUnblock(pcstr name)
		{
			unblock.setName(name);
			unblock.open();
			unblock_alt.setName(std::string{ name }.append("_alt").c_str());
			unblock_alt.open();
			goodbay_dpi.setName(std::string{ name }.append("_GoodbyeDPI").c_str());
			goodbay_dpi.open();
		}
		
	};

	std::map<pcstr, ServiceUnblock> _services;

public:
	Unblock();

private:
};
