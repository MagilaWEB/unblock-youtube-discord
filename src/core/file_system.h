#pragma once
class CORE_API FileSystem final
{
	std::filesystem::path _path_file{};
	std::fstream		  _stream;
	std::vector<std::string> _line_string;

public:
	FileSystem();
	~FileSystem();

	std::string name() const;

	bool isOpen() const;
	void open(std::filesystem::path file, pcstr expansion, bool no_default_patch = false);
	void close();

	void forLine(std::function<bool(std::string str)>&& fn);
	void forLineSection(pcstr section, std::function<bool(std::string str)>&& fn);
	void forLineParametrsSection(pcstr section, std::function<bool(std::string key, std::string value)>&& fn);
	std::expected<std::pair<std::string, std::string>, std::string> parametrSection(pcstr section, pcstr paramert);
};
