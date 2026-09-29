#pragma once

#include "../core/concepts.h"

class File final
{
	CriticalSection _lock;

	using LineStrings = std::vector<std::string>;
	using Sections	  = std::map<std::string, std::list<std::string>>;
	bool _info_debug{ true };

	std::filesystem::path _path_file{};
	std::fstream		  _stream;
	LineStrings			  _line_string;
	Sections			  _map_list_string;

	// Section order as they appear/are created in this File instance.
	std::vector<std::string> _section_order{};

	void _registerSectionOrder(std::string_view section);

	bool _open_state{ false };
	bool _is_write{ false };

public:
	File() = default;
	File(bool info_debug) : _info_debug(info_debug) {}
	~File();

	std::string			  name() const;
	std::filesystem::path getPath() const;

	size_t lineSize() const;

	bool isOpen() const;
	bool empty() const;
	void open();
	void open(std::filesystem::path file, std::string_view expansion, bool no_default_patch = false);
	void clear();
	void save();
	void close();

	inline auto begin() { return _line_string.begin(); }
	inline auto end() { return _line_string.end(); }

	void forLine(std::function<bool(std::string)> fn);
	void forLineSection(std::string_view section, std::function<bool(std::string&)> fn);
	void forLineParametersSection(std::string_view section, std::function<bool(std::string key, std::string value)> fn);

	std::optional<u32> positionSection(std::string_view section);

	template<concepts::ValidAll TypeReturn>
	std::expected<TypeReturn, std::string> parameterSection(std::string_view section, std::string parameter);

	std::expected<std::vector<std::string>, std::string> parameterSectionVector(std::string_view section, std::string parameter);

	void writeText(std::string_view str);
	void writeSectionParameter(std::string_view section, std::string parameter, std::string value_argument);

	void writeSectionParameterVector(std::string_view section, std::string parameter, const std::vector<std::string>& values);

private:
	void _normalize();
	void _removeEmptyLine();
};
