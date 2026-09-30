#pragma once

#include "ag_bind.h"
#include "dns_config.h"

#include <string>
#include <utility>
#include <vector>

namespace dns
{
	// Storage backing the pointers inside ag_dnsproxy_settings. Must outlive
	// the settings struct: every .c_str()/.data() the DLL reads lives here.
	struct ProxySettingsBacking
	{
		ag_dnsproxy_settings			  settings{};
		std::vector<ag_upstream_options>  upstreams;
		std::vector<ag_upstream_options>  fallbacks;
		std::vector<std::string>		  upstream_strings;
		std::vector<const char*>		  bootstrap_ptrs;
		std::vector<ag_listener_settings> listeners;
		std::string						  listen_string;
		std::string						  listen_string6;
	};

	// Human-readable text for a proxy init result.
	const char* initResultText(ag_dnsproxy_init_result result);

	// Fills DLL settings from our config. ipv6 adds the ::1 listeners.
	// Returns nullptr when the library defaults call failed.
	ag_dnsproxy_settings* buildSettings(AgBind& ag, const DnsProxyConfig& config, ProxySettingsBacking& backing, bool ipv6);

	// System-store TLS chain verification for the DLL (no online revocation).
	ag_certificate_verification_result verifyCertificate(const ag_certificate_verification_event* event);

	// Runs one upstream check. Returns {exit_code, text}.
	std::pair<int, std::string> runTestUpstream(AgBind& ag, const std::string& value, const std::vector<std::string>& bootstrap);
}	 // namespace dns
