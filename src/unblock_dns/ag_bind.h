#pragma once

// Minimal LoadLibrary bindings for AdguardDns64.dll (C API, ag_dns.h
// v2.10.2). The full header is vendored next to this file for the struct
// layouts; only the functions used by the wrapper are bound at runtime so
// a missing DLL fails with a clear error instead of a loader crash.

#include "ag_dns.h"

#include <windows.h>

#include <cstring>
#include <string>

struct AgBind
{
	HMODULE mod{ nullptr };

	// Proxy lifetime.
	decltype(&ag_dnsproxy_init)				   init{ nullptr };
	decltype(&ag_dnsproxy_deinit)			   deinit{ nullptr };
	decltype(&ag_dnsproxy_settings_get_default) settings_default{ nullptr };
	decltype(&ag_dnsproxy_settings_free)		 settings_free{ nullptr };

	// Upstream check for --test-upstream mode.
	decltype(&ag_test_upstream) test_upstream{ nullptr };

	// OS DNS switch.
	decltype(&ag_dns_set_if_nameserver)			set_if_nameserver{ nullptr };
	decltype(&ag_dns_get_if_nameserver)			get_if_nameserver{ nullptr };
	decltype(&ag_dns_get_preferred_adapter_guid) preferred_adapter_guid{ nullptr };

	// Logging / memory.
	decltype(&ag_set_log_level)	  set_log_level{ nullptr };
	decltype(&ag_set_log_callback) set_log_callback{ nullptr };
	decltype(&ag_get_capi_version) capi_version{ nullptr };
	decltype(&ag_str_free)		   str_free{ nullptr };

	bool load(const std::wstring& dll_path, std::string& error)
	{
		mod = LoadLibraryW(dll_path.c_str());
		if (!mod)
		{
			error = "Couldn't load AdguardDns64.dll";
			return false;
		}

#define AG_BIND(member, name)                                           \
	do                                                                  \
	{                                                                   \
		FARPROC proc = GetProcAddress(mod, #name);                      \
		if (!proc)                                                      \
		{                                                               \
			error = "AdguardDns64.dll has no export [" #name "]";       \
			FreeLibrary(mod);                                           \
			mod = nullptr;                                              \
			return false;                                               \
		}                                                               \
		static_assert(sizeof(member) == sizeof(proc));                  \
		std::memcpy(&member, &proc, sizeof(proc));                      \
	} while (false)

		AG_BIND(init, ag_dnsproxy_init);
		AG_BIND(deinit, ag_dnsproxy_deinit);
		AG_BIND(settings_default, ag_dnsproxy_settings_get_default);
		AG_BIND(settings_free, ag_dnsproxy_settings_free);
		AG_BIND(test_upstream, ag_test_upstream);
		AG_BIND(set_if_nameserver, ag_dns_set_if_nameserver);
		AG_BIND(get_if_nameserver, ag_dns_get_if_nameserver);
		AG_BIND(preferred_adapter_guid, ag_dns_get_preferred_adapter_guid);
		AG_BIND(set_log_level, ag_set_log_level);
		AG_BIND(set_log_callback, ag_set_log_callback);
		AG_BIND(capi_version, ag_get_capi_version);
		AG_BIND(str_free, ag_str_free);

#undef AG_BIND
		return true;
	}

	void unload()
	{
		if (mod)
		{
			FreeLibrary(mod);
			mod = nullptr;
		}
	}
};
