module;

#include <curl/curl.h>

export module ext.curl;

import std;

// curl's options, error codes and info tags are enumerators of global-scope
// enums (only the legacy aliases like CURLOPT_FILE or CURLOPT_ENCODING are
// macros, and the project does not use them), so they cross the module boundary
// as ordinary names. The list below is exactly what the project touches: it is
// the surface to check when curl is upgraded.

export using ::CURL;
export using ::CURLcode;
export using ::curl_off_t;
export using ::curl_slist;

export using ::curl_easy_init;
export using ::curl_easy_setopt;
export using ::curl_easy_perform;
export using ::curl_easy_getinfo;
export using ::curl_easy_cleanup;
export using ::curl_easy_recv;
export using ::curl_easy_send;

export using ::curl_slist_append;
export using ::curl_slist_free_all;

export using ::curl_global_init;
export using ::curl_global_cleanup;

export using ::CURLE_OK;
export using ::CURLE_AGAIN;
export using ::CURLE_COULDNT_CONNECT;
export using ::CURLE_COULDNT_RESOLVE_HOST;
export using ::CURLE_FAILED_INIT;
export using ::CURLE_OPERATION_TIMEDOUT;
export using ::CURLE_RECV_ERROR;
export using ::CURLE_SEND_ERROR;
export using ::CURLE_SSL_CONNECT_ERROR;

export using ::CURLINFO_EFFECTIVE_URL;
export using ::CURLINFO_RESPONSE_CODE;
export using ::CURLINFO_TOTAL_TIME;

export using ::CURLOPT_ACCEPT_ENCODING;
export using ::CURLOPT_CONNECT_ONLY;
export using ::CURLOPT_CONNECTTIMEOUT;
export using ::CURLOPT_FOLLOWLOCATION;
export using ::CURLOPT_FRESH_CONNECT;
export using ::CURLOPT_HTTPGET;
export using ::CURLOPT_HTTPHEADER;
export using ::CURLOPT_MAXREDIRS;
export using ::CURLOPT_NOBODY;
export using ::CURLOPT_NOPROGRESS;
export using ::CURLOPT_NOSIGNAL;
export using ::CURLOPT_SSL_VERIFYPEER;
export using ::CURLOPT_SSLVERSION;
export using ::CURLOPT_TIMEOUT;
export using ::CURLOPT_URL;
export using ::CURLOPT_USERAGENT;
export using ::CURLOPT_WRITEDATA;
export using ::CURLOPT_WRITEFUNCTION;
export using ::CURLOPT_XFERINFODATA;
export using ::CURLOPT_XFERINFOFUNCTION;

// CURL_GLOBAL_ALL and CURL_SSLVERSION_MAX_DEFAULT are object-like macros in
// curl.h, and macros do not cross a module boundary at all. They are captured
// before being dropped and re-declared as constants with the same name and type,
// so the call sites stay untouched.
inline constexpr auto ub_curl_global_all = CURL_GLOBAL_ALL;
inline constexpr auto ub_curl_sslv3_max  = CURL_SSLVERSION_MAX_DEFAULT;

#undef CURL_GLOBAL_ALL
#undef CURL_SSLVERSION_MAX_DEFAULT

export inline constexpr auto CURL_GLOBAL_ALL             = ub_curl_global_all;
export inline constexpr auto CURL_SSLVERSION_MAX_DEFAULT = ub_curl_sslv3_max;
