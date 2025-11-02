#pragma once

using namespace std::filesystem;

class CORE_API File final
{
	CriticalSection lock;

	using v_line_string = std::vector<std::string>;

	path		  _path_file{};
	std::fstream  _stream;
	v_line_string _line_string;
	bool		  _open_state{ false };
	bool		  _is_write{ false };

	struct ItParameters
	{
		bool					entered_section{ false };
		bool					section_end{ false };
		bool					ran_parameter{ false };
		v_line_string::iterator iterator;

		u32 i{ 0 };
	};

public:
	File() = default;
	~File();

	std::string name() const;

	bool isOpen() const;
	void open(path file, pcstr expansion, bool no_default_patch = false);
	void close();

	void forLine(std::function<bool(std::string str)> fn);
	void forLineSection(pcstr section, std::function<bool(std::string str)> fn);
	void forLineParametersSection(pcstr section, std::function<bool(std::string key, std::string value)> fn);

	template<typename TypeReturn>
	std::expected<TypeReturn, std::string> parameterSection(pcstr section, pcstr paramert);

	void writeSectionParameter(pcstr section, pcstr paramert, pcstr value_argument);

private:
	void _forLineSection(pcstr section, std::function<bool(ItParameters& it_opt)> fn);
	void _removeEmptyLine();
	void _writeToFile();
};
