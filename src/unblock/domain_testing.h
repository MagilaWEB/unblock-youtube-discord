#pragma once
#include "../core/file_system.h"

typedef void CURL;

class UNBLOCK_API DomainTesting final
{
	struct CurlDomain
	{
		CURL*		curl{ nullptr };
		std::string url{};
	};

	Ptr<FileSystem>		  _file_test_domain;
	std::list<CurlDomain> _list_domain{};
	std::atomic_uint	  _domain_ok{ 0 };
	std::atomic_uint	  _domain_error{ 0 };
	std::atomic_bool	  _is_testing{ false };
	std::atomic_bool	  _accurate_test{ false };

public:
	DomainTesting() = default;
	~DomainTesting();

	void		test(bool test_video = false);
	std::string fileName() const;
	void		loadFile(std::filesystem::path file);

	void changeAccurateTest(bool state);
	u32	 successRate() const;
	u32	 errorRate() const;
	void printTestInfo() const;

	bool isConnectionUrl(const CurlDomain& domain) const;
	bool isConnectionUrlVideo(const CurlDomain& domain) const;
};
