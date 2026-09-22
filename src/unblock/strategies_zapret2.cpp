#include "strategies_zapret2.h"

StrategiesZapret2::StrategiesZapret2()
{
	_file_lua_init.open(Core::get().configsPath() / "lua_init", ".config", true);
	_file_fake_bin_config.open(Core::get().configsPath() / "fake_bin", ".config", true);

	_file_fake_bin_config.forLineParametersSection(
		"FAKE",
		[this](std::string_view key, std::string_view value)
		{
			const auto path_file = Core::get().binariesPath() / "fake" / value;
			ASSERT_ARGS(std::filesystem::exists(path_file), "The [{}] file does not exist!", path_file.string());

			auto& fake = _fake_bin_params[std::string{ key }];
			fake.file  = path_file.string();
			return false;
		}
	);

	_file_fake_bin_config.close();
}

void StrategiesZapret2::_uploadStrategies()
{
	_uploadFromGenerator();

	_init_lua_to_zapret();
	_blob_init_to_zapret();

	_normalizeStrategyFinal();
}

void StrategiesZapret2::_saveStrategies(std::string_view str)
{
	if (_ignoringLineStrategy(str))
		return;

	StrategyConfigBase::_saveStrategies(str);

	auto& string_back = _strategy_dpi.back();
	_getAllPorts(string_back);
}

void StrategiesZapret2::_init_lua_to_zapret()
{
	static const std::filesystem::path _lua_dir{ Core::get().binariesPath() / "lua" };

	auto iter = [this] { return std::ranges::find(_strategy_dpi, "%INIT_LUA%"); };
	if (iter() != _strategy_dpi.end())
	{
		for (auto& line : _file_lua_init)
			_strategy_dpi.insert(iter(), "--lua-init=@\"" + (_lua_dir / line).string() + "\"");

		_strategy_dpi.erase(iter());
	}
}

void StrategiesZapret2::_blob_init_to_zapret()
{
	auto iter = [this] { return std::ranges::find(_strategy_dpi, "%INIT_BLOB%"); };
	if (iter() != _strategy_dpi.end())
	{
		for (auto& [key, data] : _fake_bin_params)
			if (data.init)
				data.init = false;

		static const std::regex pattern{ R"(:blob=([^:]+)|:seqovl_pattern=([^:]+)|:pattern=([^:]+))" };

		auto& file_strategy = *_file_strategy_dpi;
		for (auto& line : file_strategy)
		{
			for (auto& [key, data] : _fake_bin_params)
			{
				if (!data.init)
				{
					std::smatch match;
					if (std::regex_search(line, match, pattern))
						for (auto i : std::ranges::iota_view(1U, match.size()))
							if (match[i].str() == key)
								data.init = true;
				}
			}
		}

		for (auto& [key, data] : _fake_bin_params)
			if (data.init)
				_strategy_dpi.insert(iter(), "--blob=" + key + ":@\"" + data.file + "\"");

		for (auto& line : file_strategy)
		{
			std::smatch match;
			if (std::regex_search(line, match, pattern))
			{
				constexpr std::string_view fake_default[]{ "fake_default_tls", "fake_default_http", "fake_default_udp", "fake_default_quic" };
				for (auto i : std::ranges::iota_view(1U, match.size()))
				{
					auto key_fake = match[i].str();

					if (std::ranges::find(fake_default, key_fake) != std::end(fake_default))
						continue;

					if (_fake_bin_params.contains(key_fake))
						continue;

					auto find_blob_base = std::format("--blob={}", key_fake);
					auto is_it			= std::ranges::find_if(
						_strategy_dpi,
						[match, &find_blob_base](const std::string& line_str) { return line_str.contains(find_blob_base); }
					);

					if (is_it == _strategy_dpi.end())
					{
						Debug::warning(
							"The blob {} key does not exist in unblock, register it in the fake_bin.config config or use the existing ones."
							"The strategy string is [{}].",
							key_fake,
							line
						);
					}
				}
			}
		}

		_strategy_dpi.erase(iter());
	}
}

void StrategiesZapret2::_normalizeStrategyFinal()
{
	_numbering_active = false;
	_strategy_index	  = 0;

	for (auto& line : _strategy_dpi)
	{
		_luaDesyncNumberStrategy(line);

		static const std::regex port_vars{ R"(%[^%]+%)" };
		line = std::regex_replace(line, port_vars, "");

		_normalizeStrategyString(line);
	}

	std::erase_if(_strategy_dpi, [](const std::string& line) { return line.empty(); });

	if (_strategy_dpi.empty())
	{
		Debug::error("strategy dpi empty");
		return;
	}

	while (_strategy_dpi.back().starts_with("--new"))
		_strategy_dpi.pop_back();

	_strategy_dpi.emplace_back("--new");
	_strategy_dpi.emplace_back("--filter-udp=10000");
	_strategy_dpi.emplace_back("--ipset-ip=127.0.0.1/32");
	_strategy_dpi.emplace_back("--payload=all");
	_strategy_dpi.emplace_back("--lua-desync=zcheck");

	for (auto& line : _strategy_dpi)
		Debug::ok("{}", line);
}

void StrategiesZapret2::_luaDesyncNumberStrategy(std::string& str)
{
	constexpr std::string_view maker_start_strategy[]{ "--lua-desync=circular", "--lua-desync=auto_strategy" };
	static const std::regex	   reg_manual_strategy{ ":strategy=(\\d+)" };

	if (!_numbering_active)
	{
		for (auto marker : maker_start_strategy)
		{
			if (str.starts_with(marker))
			{
				_numbering_active = true;
				return;
			}
		}

		return;
	}

	if (str.empty() || str.starts_with("\n"))
		return;

	if (str.contains("--new"))
	{
		_numbering_active = false;
		_strategy_index	  = 0;
		return;
	}

	if (!str.starts_with("--lua-desync"))
		return;

	// manual numbering is respected: the counter follows the highest explicit number,
	// auto numbers continue after it without collisions. several instances may share
	// the same number - that is a composite strategy
	std::smatch manual;
	if (std::regex_search(str, manual, reg_manual_strategy))
	{
		const auto number = static_cast<u32>(std::stoul(manual[1].str()));

		if (number < _strategy_index)
			Debug::warning("manual strategy={} is lower than the counter {} in the line [{}]", number, _strategy_index, str);
		else
			_strategy_index = number;
	}
	else if (!str.contains(":final"))
	{
		_strategy_index++;
		str.append(std::format(":strategy={}", _strategy_index));
	}
}
