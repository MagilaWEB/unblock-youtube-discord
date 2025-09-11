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

	std::vector<std::string> getStrategy() const;
	void					 changeProxyData(const ProxyData& proxy_data);

private:
	void _saveStrategies(std::vector<std::string>& strategy_dpi, std::string str) override;

	std::optional<std::string> _getProxySetting(std::string str) const;
};
