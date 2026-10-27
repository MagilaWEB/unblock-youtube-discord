module;

// The vendored AdGuard C API (ag_dns.h) and the runtime LoadLibrary bindings
// around it (ag_bind.h) stay textual here; every name the rest of the project
// uses is re-exported by hand below. This is the only place either header is
// included, and <windows.h> is pulled in for the Win32 types the bindings need
// (HMODULE, FARPROC, LoadLibraryW, ...).
#include <windows.h>
#include "ag_dns.h"
#include "ag_bind.h"

export module unblock_dns.ag_bind;

export import std;

// The binding table itself.
export using ::AgBind;

// Fixed-width C integer types from <stdint.h>: ag_dns.h needs them and a
// module cannot carry the global C names, so they are re-exported by hand.
export using ::uint8_t;
export using ::uint16_t;
export using ::uint32_t;
export using ::int32_t;
export using ::int64_t;

// Structs from the ag_dns.h C API.
export using ::ag_buffer;
export using ::ag_dnsproxy;
export using ::ag_dnsproxy_events;
export using ::ag_dnsproxy_init_result;
export using ::ag_dnsproxy_settings;
export using ::ag_upstream_options;
export using ::ag_listener_settings;
export using ::ag_certificate_verification_event;
export using ::ag_certificate_verification_result;
export using ::ag_log_level;
export using ::ag_dns_request_processed_event;

// Enumerators (not macros), exported with a using-declaration.
export using ::AGLP_UDP;
export using ::AGLP_TCP;

export using ::AGLL_ERR;
export using ::AGLL_WARN;
export using ::AGLL_INFO;
export using ::AGLL_DEBUG;
export using ::AGLL_TRACE;

export using ::AGDPIR_PROXY_NOT_SET;
export using ::AGDPIR_EVENT_LOOP_NOT_SET;
export using ::AGDPIR_INVALID_ADDRESS;
export using ::AGDPIR_EMPTY_PROXY;
export using ::AGDPIR_PROTOCOL_ERROR;
export using ::AGDPIR_LISTENER_INIT_ERROR;
export using ::AGDPIR_INVALID_IPV4;
export using ::AGDPIR_INVALID_IPV6;
export using ::AGDPIR_UPSTREAM_INIT_ERROR;
export using ::AGDPIR_FALLBACK_FILTER_INIT_ERROR;
export using ::AGDPIR_FILTER_LOAD_ERROR;
export using ::AGDPIR_MEM_LIMIT_REACHED;
export using ::AGDPIR_NON_UNIQUE_FILTER_ID;
export using ::AGDPIR_OK;

export using ::AGCVR_OK;
export using ::AGCVR_ERROR_CREATE_CERT;
export using ::AGCVR_ERROR_ACCESS_TO_STORE;
export using ::AGCVR_ERROR_CERT_VERIFICATION;
