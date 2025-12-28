#pragma once

using namespace std::filesystem;

class CORE_API File final
{
	CriticalSection lock;

	using v_line_string = std::vector<std::string>;
	using v_sections = std::map<std::string, std::list<std::string>>;

	path		  _path_file{};
	std::fstream  _stream;
	v_line_string _line_string;
	v_sections	  _map_list_string;


	bool		  _open_state{ false };
	bool		  _is_write{ false };

public:
	File() = default;
	~File();

	std::string name() const;
	path		getPath() const;

	size_t lineSize() const;

	bool isOpen() const;
	void open(path file, pcstr expansion, bool no_default_patch = false);
	void clear();
	void save();
	void close();

	void forLine(std::function<bool(std::string)> fn);
	void forLineSection(pcstr section, std::function<bool(std::string&)> fn);
	void forLineParametersSection(pcstr section, std::function<bool(std::string key, std::string value)> fn);

	template<typename TypeReturn>
	std::expected<TypeReturn, std::string> parameterSection(pcstr section, pcstr paramert);

	void writeText(std::string str);
	void writeSectionParameter(pcstr section, pcstr paramert, pcstr value_argument);

private:
	void _normalize();
	void _removeEmptyLine();
};
