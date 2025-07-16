#pragma once
class CORE_API FileSystem final
{
	using v_line_string = std::vector<std::string>;

	std::filesystem::path	 _path_file{};
	std::fstream			 _stream;
	v_line_string			 _line_string;
	bool					 _open_state{ false };

	struct ItParameters
	{
		bool entered_section{ false };
		bool section_end{ false };
		bool ran_parameter{ false };

		v_line_string::iterator iterator;
	};

public:
	FileSystem();
	~FileSystem();

	std::string name() const;

	bool isOpen() const;
	void open(std::filesystem::path file, pcstr expansion, bool no_default_patch = false);
	void close();

	void forLine(std::function<bool(std::string str)>&& fn);
	void forLineSection(pcstr section, std::function<bool(std::string str)>&& fn);
	void forLineParametersSection(pcstr section, std::function<bool(std::string key, std::string value)>&& fn);

	std::expected<std::string, std::string> parameterSection(pcstr section, pcstr paramert);

	void writeSectionParameter(pcstr section, pcstr paramert, pcstr value_argument);

private:
	void _forLineSection(pcstr section, std::function<bool(ItParameters& it_opt)>&& fn);
	void _writeToFile();
};
