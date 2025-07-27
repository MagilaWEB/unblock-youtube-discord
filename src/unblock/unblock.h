#pragma once
#include "unblock_api.hpp"
#include "domain_testing.h"
#include "strategies_dpi.h"

#include "../core/service.h"

class UNBLOCK_API Unblock final : public IUnblockAPI
{
	Ptr<DomainTesting> _domain_testing;
	Ptr<StrategiesDPI> _strategies_dpi;
	Ptr<FileSystem>	   _file_user_setting;

	Service _unblock{ "unblock1", "winws.exe" };
	Service _unblock2{ "unblock2", "winws.exe" };
	Service _goodbay_dpi{ "GoodbyeDPI", "goodbyedpi.exe" };
	Service _win_divert{ "WinDivert" };

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

	bool checkSavedConfiguration() override;

	void startAuto() override;

	void startManual() override;

	void allOpenService() override;

	void allRemoveService() override;

	void testDomains(bool video = false) const;

private:
	void _startService();
	void _chooseStrategy();
	void _testVideo();
};
