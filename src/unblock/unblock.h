#pragma once
#include "unblock_api.hpp"
#include "domain_testing.h"
#include "strategies_dpi.h"
#include "proxy_strategies_dpi.h"

#include "../core/service.h"

class UNBLOCK_API Unblock final : public IUnblockAPI
{
	Ptr<FileSystem>			_file_user_setting;
	Ptr<DomainTesting>		_domain_testing;
	Ptr<StrategiesDPI>		_strategies_dpi;
	Ptr<ProxyStrategiesDPI> _proxy_strategies_dpi;

	Service _unblock{ "unblock1", "winws.exe" };
	Service _unblock2{ "unblock2", "winws.exe" };
	Service _goodbay_dpi{ "GoodbyeDPI", "goodbyedpi.exe" };
	Service _bay_dpi{ "ByeDPI", "ciadpi.exe" };
	Service _win_divert{ "WinDivert" };

	ProxyData		   _proxy_data;
	DpiApplicationType _dpi_application_type{ DpiApplicationType::BASE };
	u32				   _dpi_fake_bin{ 0 };
	u32				   _type_strategy{ 0 };
	bool			   _accurate_test{ false };

	struct SuccessfulStrategy
	{
		u32 success{ 0 };
		u32 index_strategy{ 0 };
		u32 dpi_fake_bin{ 0 };
	};

	std::vector<SuccessfulStrategy> _successful_strategies;

public:
	Unblock();
	Unblock(Unblock&& Unblock) = delete;

	void changeAccurateTest(bool state);

	void changeDpiApplicationType(DpiApplicationType type) override;

	bool checkSavedConfiguration(bool proxy = false) override;

	void startAuto() override;

	void startManual() override;

	void startProxyManual() override;

	void proxyRemoveService() override;

	void allRemoveService() override;

	void allOpenService() override;

	bool testDomains(bool video = false, bool proxy = false);

private:
	void _startService();
	void _chooseStrategy();
};
