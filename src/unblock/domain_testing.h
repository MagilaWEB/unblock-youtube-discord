#pragma once
typedef void	  CURL;
typedef long long curl_off_t;

class UNBLOCK_API DomainTesting final
{
	const bool _proxy{ false };

	struct CurlDomain
	{
		CURL*		curl{ nullptr };
		std::string url{};
	};

	File				   _file_test_domain{ false };
	std::list<CurlDomain>  _list_domain{};
	std::list<std::string> _section_opt_service_names{};

	std::atomic_uint _domain_ok{ 0 };
	std::atomic_uint _domain_error{ 0 };
	std::atomic_uint _max_wait_testing{ 5U };
	std::atomic_bool _is_testing{ false };
	std::atomic_bool _cancel_testing{ false };

	std::string _proxyIP{ "127.0.0.1" };
	u32			_proxyPORT{ 1'080 };

public:
	DomainTesting(bool enable_proxy = false);
	~DomainTesting();

	void loadDomain(bool video = false);

	void test(bool test_video, bool base_test, std::function<void(pcstr url, bool state)>&& callback);

	void changeProxy(std::string ip, u32 port);
	void changeAccurateTest(bool state);
	void changeMaxWaitTesting(u32 second);

	void changeOptionalServices(std::list<std::string> list_services);

	void cancelTesting();

	inline bool isTesting() { return _is_testing.load(); }
	inline bool isCancelTesting() { return _cancel_testing.load(); }

	u32	 successRate() const;
	u32	 errorRate() const;
	void printTestInfo() const;

	bool isConnectionUrl(const CurlDomain& domain);
	bool isConnectionUrlVideo(const CurlDomain& domain) const;

private:
	bool _loadFile(std::filesystem::path file);
	void _genericURLS(std::string base_name = "");
	void _appendURLS();
	void _clearURLS();
};
