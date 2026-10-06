#include "dns_proxy.h"

#include <windows.h>
#include <wincrypt.h>

#include <ranges>
#include <string>
#include <vector>

namespace dns
{
	const char* initResultText(ag_dnsproxy_init_result result)
	{
		switch (result)
		{
		case AGDPIR_PROXY_NOT_SET:
			return "proxy not set";
		case AGDPIR_EVENT_LOOP_NOT_SET:
			return "event loop not set";
		case AGDPIR_INVALID_ADDRESS:
			return "invalid address";
		case AGDPIR_EMPTY_PROXY:
			return "empty proxy";
		case AGDPIR_PROTOCOL_ERROR:
			return "protocol error";
		case AGDPIR_LISTENER_INIT_ERROR:
			return "listener init error (port busy?)";
		case AGDPIR_INVALID_IPV4:
			return "invalid IPv4";
		case AGDPIR_INVALID_IPV6:
			return "invalid IPv6";
		case AGDPIR_UPSTREAM_INIT_ERROR:
			return "upstream init error";
		case AGDPIR_FALLBACK_FILTER_INIT_ERROR:
			return "fallback filter init error";
		case AGDPIR_FILTER_LOAD_ERROR:
			return "filter load error";
		case AGDPIR_MEM_LIMIT_REACHED:
			return "memory limit reached";
		case AGDPIR_NON_UNIQUE_FILTER_ID:
			return "non-unique filter id";
		case AGDPIR_OK:
			return "ok";
		}
		return "unknown";
	}

	// Fills DLL settings from our config. We build our own settings struct
	// from the library defaults (copying the scalar tuning values, not the
	// library-owned arrays), then free the defaults struct. This avoids both
	// a leak and a double-free of our own arrays on settings_free.
	ag_dnsproxy_settings* buildSettings(AgBind& ag, const DnsProxyConfig& config, ProxySettingsBacking& backing, bool ipv6)
	{
		ag_dnsproxy_settings* defaults = ag.settings_default();
		if (!defaults)
			return nullptr;

		backing.settings.blocked_response_ttl_secs	 = defaults->blocked_response_ttl_secs;
		backing.settings.adblock_rules_blocking_mode = defaults->adblock_rules_blocking_mode;
		backing.settings.hosts_rules_blocking_mode	 = defaults->hosts_rules_blocking_mode;
		backing.settings.dns_cache_size				 = defaults->dns_cache_size;
		backing.settings.upstream_timeout_ms		 = config.timeout_ms;
		ag.settings_free(defaults);

		backing.listen_string = config.listen;

		// One bootstrap list shared by every upstream. The DLL only consults
		// it when an upstream address is a hostname; plain-IP upstreams
		// ignore it.
		backing.upstreams.reserve(config.upstreams.size());
		backing.upstream_strings.reserve(config.upstreams.size());
		backing.bootstrap_ptrs.reserve(config.bootstrap.size());
		for (const auto& b : config.bootstrap)
			backing.bootstrap_ptrs.push_back(b.c_str());

		int32_t id = 0;
		for (const auto& [i, upstream] : std::views::enumerate(config.upstreams))
		{
			backing.upstream_strings.push_back(upstream);

			ag_upstream_options opt{};
			opt.address					 = backing.upstream_strings.back().c_str();
			opt.bootstrap.data			 = backing.bootstrap_ptrs.empty() ? nullptr : backing.bootstrap_ptrs.data();
			opt.bootstrap.size			 = static_cast<uint32_t>(backing.bootstrap_ptrs.size());
			opt.id						 = ++id;
			opt.outbound_interface_index = 0;

			// The first upstream is the primary resolver and answers every query.
			// The rest are only fallbacks. Ordering is the priority here, so the
			// first entry (GeoHide by default, for region spoofing) is actually
			// used instead of losing a parallel race to Cloudflare/Google.
			(i == 0 ? backing.upstreams : backing.fallbacks).push_back(opt);
		}

		backing.settings.upstreams.data = backing.upstreams.data();
		backing.settings.upstreams.size = static_cast<uint32_t>(backing.upstreams.size());
		backing.settings.fallbacks.data = backing.fallbacks.empty() ? nullptr : backing.fallbacks.data();
		backing.settings.fallbacks.size = static_cast<uint32_t>(backing.fallbacks.size());

		backing.listeners.clear();
		backing.listeners.push_back(ag_listener_settings{ backing.listen_string.c_str(), config.port, AGLP_UDP, false, 0, {} });
		backing.listeners.push_back(ag_listener_settings{ backing.listen_string.c_str(), config.port, AGLP_TCP, true, 30'000, {} });
		// IPv6 loopback too, so router-advertised IPv6 DNS (which Windows
		// prefers) is answered by us instead of bypassing the proxy.
		if (ipv6)
		{
			backing.listen_string6 = "::1";
			backing.listeners.push_back(ag_listener_settings{ backing.listen_string6.c_str(), config.port, AGLP_UDP, false, 0, {} });
			backing.listeners.push_back(ag_listener_settings{ backing.listen_string6.c_str(), config.port, AGLP_TCP, true, 30'000, {} });
		}

		backing.settings.listeners.data = backing.listeners.data();
		backing.settings.listeners.size = static_cast<uint32_t>(backing.listeners.size());

		backing.settings.block_ipv6							  = false;
		// Keep the bootstrapper IPv4-only: the upstream (GeoHide) is reached
		// over IPv4, and an AAAA answer on a broken-IPv6 host would stall it.
		// Client-facing IPv6 is handled by the ::1 listener above.
		backing.settings.ipv6_available						  = false;
		backing.settings.enable_dnssec_ok					  = false;
		backing.settings.enable_retransmission_handling		  = true;
		backing.settings.block_ech							  = false;
		backing.settings.block_h3_alpn						  = false;
		backing.settings.enable_parallel_upstream_queries	  = true;
		backing.settings.enable_fallback_on_upstreams_failure = true;
		backing.settings.enable_servfail_on_upstreams_failure = true;
		backing.settings.enable_http3						  = false;
		backing.settings.enable_post_quantum_cryptography	  = false;
		backing.settings.optimistic_cache					  = true;

		return &backing.settings;
	}

	// AdGuard DnsLibs does not carry platform roots: on Windows the host must
	// verify each TLS chain against the system certificate stores (that is how
	// Chromium, which this library is derived from, works). Without a callback
	// the DLL rejects every upstream certificate, so even a reachable DoH/DoT
	// server fails the handshake.
	ag_certificate_verification_result verifyCertificate(const ag_certificate_verification_event* event)
	{
		if (!event || !event->certificate.data || event->certificate.size == 0)
			return AGCVR_ERROR_CREATE_CERT;

		constexpr DWORD encoding = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;

		PCCERT_CONTEXT leaf = CertCreateCertificateContext(encoding, event->certificate.data, static_cast<DWORD>(event->certificate.size));
		if (!leaf)
			return AGCVR_ERROR_CREATE_CERT;

		HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, nullptr);
		if (!store)
		{
			CertFreeCertificateContext(leaf);
			return AGCVR_ERROR_ACCESS_TO_STORE;
		}

		bool ok = true;
		for (uint32_t i = 0; i < event->chain.size && ok; ++i)
		{
			const ag_buffer& item = event->chain.data[i];
			if (!item.data || item.size == 0)
				continue;

			PCCERT_CONTEXT cert = CertCreateCertificateContext(encoding, item.data, static_cast<DWORD>(item.size));
			if (!cert)
			{
				ok = false;
				break;
			}

			if (!CertAddCertificateContextToStore(store, cert, CERT_STORE_ADD_ALWAYS, nullptr))
				ok = false;

			CertFreeCertificateContext(cert);
		}

		ag_certificate_verification_result result = AGCVR_ERROR_CERT_VERIFICATION;

		if (ok)
		{
			CERT_CHAIN_PARA para{};
			para.cbSize = sizeof(para);

			LPSTR			  server_auth = const_cast<LPSTR>(szOID_PKIX_KP_SERVER_AUTH);
			CERT_ENHKEY_USAGE usage{};
			usage.cUsageIdentifier	   = 1;
			usage.rgpszUsageIdentifier = &server_auth;
			para.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
			para.RequestedUsage.Usage  = usage;

			// No revocation flags: an online CRL/OCSP check would add latency
			// and fail under filtering; chain trust against the system roots is
			// what matters here.
			PCCERT_CHAIN_CONTEXT chain = nullptr;
			if (CertGetCertificateChain(nullptr, leaf, nullptr, store, &para, 0, nullptr, &chain))
			{
				if (chain->TrustStatus.dwErrorStatus == CERT_TRUST_NO_ERROR)
					result = AGCVR_OK;
				CertFreeCertificateChain(chain);
			}
		}

		CertCloseStore(store, 0);
		CertFreeCertificateContext(leaf);

		return result;
	}

	std::pair<int, std::string> runTestUpstream(AgBind& ag, const std::string& value, const std::vector<std::string>& bootstrap)
	{
		if (!isValidUpstreamAddress(value))
			return { 2, "FAIL: bad upstream format" };

		if (bootstrap.empty() && upstreamNeedsBootstrap(value))
			return { 2, "FAIL: bootstrap required for hostname upstream" };

		ag_upstream_options opt{};
		opt.address = value.c_str();

		std::vector<const char*> boots;
		for (const auto& b : bootstrap)
			boots.push_back(b.c_str());
		opt.bootstrap.data = boots.empty() ? nullptr : boots.data();
		opt.bootstrap.size = static_cast<uint32_t>(boots.size());

		const char* error = ag.test_upstream(&opt, 10'000, false, verifyCertificate, false);
		if (!error)
			return { 0, "OK" };

		std::string text = std::string{ "FAIL: " } + error;
		ag.str_free(error);

		return { 1, std::move(text) };
	}
}	 // namespace dns
