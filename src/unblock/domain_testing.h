#pragma once
#include "../core/file_system.h"

class UNBLOCK_API DomainTesting final
{
	Ptr<FileSystem>		   _file_test_domain;
	std::list<std::string> _list_domain{};
	std::atomic_uint	   _domain_ok{ 0 };
	std::atomic_uint	   _domain_error{ 0 };
	std::atomic_bool	   _is_testing{ false };

public:
	DomainTesting() = default;
	~DomainTesting();

	void		test();
	std::string fileName() const;
	void		loadFile(std::filesystem::path file);

	u32	 successRate() const;
	void printTestInfo() const;

	bool isConnectionUrl(pcstr url) const;
};
