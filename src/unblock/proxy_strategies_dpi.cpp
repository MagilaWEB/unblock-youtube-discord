#include "proxy_strategies_dpi.h"

ProxyStrategiesDPI::ProxyStrategiesDPI()
{
	_patch_file = Core::get().configsPath() / "proxy_strategy";
	for (auto& entry : std::filesystem::directory_iterator(_patch_file))
		_strategy_files_list.push_back(entry.path().filename().string());
}

void ProxyStrategiesDPI::changeProxyData(const ProxyData& proxy_data)
{
	_proxy_data = proxy_data;
}

void ProxyStrategiesDPI::_saveStrategies(std::string str)
{
	if (auto new_str = _getProxySetting(str))
	{
		_strategy_dpi.push_back(new_str.value());
		return;
	}

	StrategiesDPIBase::_saveStrategies(str);
}

std::optional<std::string> ProxyStrategiesDPI::_getProxySetting(std::string str) const
{
	if (str.contains("%IP%"))
		return "-i" + _proxy_data.IP;

	if (str.contains("%PORT%"))
		return "-p" + std::to_string(_proxy_data.PORT);

	return std::nullopt;
}
