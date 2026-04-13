#pragma once
typedef void	  CURL;
typedef long long curl_off_t;

class UNBLOCK_API DomainTesting final
{
	struct CurlDomain
	{
		CURL*		curl{ nullptr };
		std::string url{};

		double result_time_sec{ 0.0 };
		bool   tls_1_0 = false, tls_1_1 = false, tls_1_2 = false, tls_1_3 = false;

		void setTls(u32 version, bool state)
		{
			switch (version)
			{
			case 0:
				tls_1_0 = state;
				break;
			case 1:
				tls_1_1 = state;
				break;
			case 2:
				tls_1_2 = state;
				break;
			case 3:
				tls_1_3 = state;
				break;
			default:
				Debug::warning("TLS can only be versions 0, 1, 2, 3!");
			}
		}
	};

	File				   _file_test_domain{ false };
	std::list<CurlDomain>  _list_domain{};
	std::list<std::string> _section_opt_service_names{};

	std::atomic_uint _domain_ok{ 0 };
	std::atomic_uint _domain_error{ 0 };
	std::atomic_uint _max_wait_testing{ 0U };
	std::atomic_bool _is_testing{ false };
	std::atomic_bool _cancel_testing{ false };

	std::string _proxyIP{ "127.0.0.1" };
	u32			_proxyPORT{ 1'080 };

public:
	DomainTesting();
	~DomainTesting();

	void loadDomain();

	void test(bool base_test, std::function<void(std::string url, bool state)>&& callback);

	void changeProxy(std::string_view ip, u32 port);
	void changeOptionalServices(std::list<std::string> list_services);

	void cancelTesting();

	inline bool isTesting() { return _is_testing.load(); }
	inline bool isCancelTesting() { return _cancel_testing.load(); }

	u32	 successRate() const;
	u32	 errorRate() const;
	void printTestInfo() const;

	bool isConnectionUrl(CurlDomain& domain);

private:
	bool _loadFile(std::filesystem::path file);
	void _genericURLS(std::string base_name = "");
	void _appendURLS();
	void _clearURLS();
};
