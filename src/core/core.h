#pragma once

class CORE_API Core final
{
	Core();
	~Core() = default;

	std::filesystem::path _current_path{};
	std::filesystem::path _bin_path{};
	std::filesystem::path _binaries_path{};
	std::filesystem::path _configs_path{};
	std::filesystem::path _user_path{};

public:
	Core(Core&&) = delete;

public:
	static Core&		  get();
	void				  initialize(const std::string& command_line);

	std::filesystem::path currentPath() const;
	std::filesystem::path binPath() const;
	std::filesystem::path binariesPath() const;
	std::filesystem::path configsPath() const;
	std::filesystem::path userPath() const;
};
