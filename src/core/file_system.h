#pragma once

#include "../core/concepts.h"

using namespace std::filesystem;

class CORE_API File final
{
	CriticalSection lock;

	using v_line_string = std::vector<std::string>;
	using v_sections	= std::map<std::string, std::list<std::string>>;
	bool info_debug{ true };

	path		  _path_file{};
	std::fstream  _stream;
	v_line_string _line_string;
	v_sections	  _map_list_string;

	bool _open_state{ false };
	bool _is_write{ false };

public:
	File() = default;
	File(bool info_debug) : info_debug(info_debug) {}
	~File();

	std::string name() const;
	path		getPath() const;

	size_t lineSize() const;

	bool isOpen() const;
	bool empty() const;
	void open(path file, std::string_view expansion, bool no_default_patch = false);
	void clear();
	void save();
	void close();

	void forLine(std::function<bool(std::string)> fn);
	void forLineSection(std::string_view section, std::function<bool(std::string&)> fn);
	void forLineParametersSection(std::string_view section, std::function<bool(std::string key, std::string value)> fn);

	std::optional<u32> positionSection(std::string_view section);

	template<concepts::VallidALL TypeReturn>
	std::expected<TypeReturn, std::string_view> parameterSection(std::string_view section, std::string_view paramert);

	void writeText(std::string_view str);
	void writeSectionParameter(std::string_view section, std::string_view paramert, std::string_view value_argument);

private:
	void _normalize();
	void _removeEmptyLine();
};
