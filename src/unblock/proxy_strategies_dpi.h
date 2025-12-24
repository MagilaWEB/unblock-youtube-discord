#pragma once
#include "strategies_dpi_base.h"

struct ProxyData
{
	std::string IP{ "127.0.0.1" };
	u32			PORT{ 1'080 };
};

class ProxyStrategiesDPI final : public StrategiesDPIBase
{
	ProxyData _proxy_data;

public:
	ProxyStrategiesDPI();
	void changeProxyData(const ProxyData& proxy_data);

protected:
	void _saveStrategies(std::string str) override;

private:
	std::optional<std::string> _getProxySetting(std::string str) const;
};
