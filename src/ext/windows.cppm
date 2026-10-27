module;

// The Windows headers are a macro soup (WM_*, ERROR_*, MAKEWORD, ...), so they
// stay a textual include in the global module fragment. Nothing from a header
// crosses the module boundary by itself, so every name the project uses is
// re-exported by hand below, and value macros become constants with the same
// names (see the bottom of the file) so that no macro has to cross either.
//
// winsock2.h must come before windows.h: otherwise windows.h pulls in the older
// winsock.h and both define the socket types.
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <dwmapi.h>
#include <iphlpapi.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <winreg.h>
#include <winsvc.h>

// UCRT headers: the project uses a few of their extensions and their streams
// (see the bottom of the file).
#include <stdio.h>
#include <time.h>

export module ext.windows;

// The whole standard library rides along: the wrapper is used by files that
// need both, and this saves writing two imports in every one of them.
export import std;

// --- Synchronization ---------------------------------------------------------
export using ::BOOL;
export using ::CRITICAL_SECTION;
export using ::DeleteCriticalSection;
export using ::EnterCriticalSection;
export using ::InitializeCriticalSection;
export using ::LeaveCriticalSection;
export using ::TryEnterCriticalSection;

export using ::SRWLOCK;
export using ::AcquireSRWLockExclusive;
export using ::AcquireSRWLockShared;
export using ::InitializeSRWLock;
export using ::ReleaseSRWLockExclusive;
export using ::ReleaseSRWLockShared;
export using ::TryAcquireSRWLockExclusive;
export using ::TryAcquireSRWLockShared;

// --- Console and window ------------------------------------------------------
export using ::GetConsoleWindow;
export using ::GetForegroundWindow;
export using ::GetKeyState;

export using ::AllocConsole;
export using ::AttachConsole;
export using ::FreeConsole;
export using ::GetConsoleMode;
export using ::GetStdHandle;
export using ::SetConsoleCP;
export using ::SetConsoleMode;
export using ::SetConsoleOutputCP;
export using ::SetConsoleTitleW;

export using ::CallWindowProcW;
export using ::DefWindowProcW;
export using ::GetCursorPos;
export using ::GetDpiForWindow;
export using ::GetSystemMenu;
export using ::GetSystemMetrics;
export using ::LoadImageW;
export using ::RemovePropW;
export using ::SendMessageW;
export using ::SetClassLongPtrW;
export using ::SetPropW;
export using ::SetWindowLongPtrW;
export using ::EnableMenuItem;
export using ::GetUserDefaultLocaleName;
export using ::MultiByteToWideChar;
export using ::WideCharToMultiByte;

// --- Clipboard and memory ----------------------------------------------------
export using ::CloseClipboard;
export using ::EmptyClipboard;
export using ::GlobalAlloc;
export using ::GlobalFree;
export using ::GlobalLock;
export using ::GlobalUnlock;
export using ::OpenClipboard;
export using ::SetClipboardData;

// --- Processes and services --------------------------------------------------
export using ::CloseHandle;
export using ::CreateFileW;
export using ::CreateMutexW;
export using ::CreateProcessW;
export using ::ExitProcess;
export using ::GetExitCodeProcess;
export using ::GetLastError;
export using ::GetModuleFileNameA;
export using ::GetModuleFileNameW;
export using ::GetTickCount64;
export using ::OpenProcess;
export using ::QueryFullProcessImageNameW;
export using ::Sleep;
export using ::TerminateProcess;
export using ::WaitForSingleObject;

export using ::ChangeServiceConfigW;
export using ::ChangeServiceConfig2W;
export using ::CloseServiceHandle;
export using ::ControlService;
export using ::CreateServiceW;
export using ::DeleteService;
export using ::EnumServicesStatusA;
export using ::QueryServiceConfigA;
export using ::QueryServiceStatusEx;

export using ::CreateToolhelp32Snapshot;
export using ::Process32FirstW;
export using ::Process32NextW;

// --- Security ----------------------------------------------------------------
export using ::AllocateAndInitializeSid;
export using ::CheckTokenMembership;
export using ::FreeSid;

// --- Exception handling and shell --------------------------------------------
export using ::AddVectoredExceptionHandler;
export using ::RemoveVectoredExceptionHandler;
export using ::SetUnhandledExceptionFilter;
export using ::MessageBoxA;
export using ::ShellExecuteA;
export using ::ShellExecuteExA;

export using ::GetDpiForMonitor;
export using ::DwmSetWindowAttribute;
export using ::EnumDisplayMonitors;
export using ::GetMonitorInfoW;

// --- Types -------------------------------------------------------------------
export using ::DWORD;
export using ::LONG;
export using ::HRESULT;
export using ::UINT;
export using ::ULONGLONG;
export using ::LRESULT;
export using ::WPARAM;
export using ::LPARAM;
export using ::LPVOID;
export using ::PVOID;
export using ::LPBYTE;
export using ::LPRECT;
export using ::LONG_PTR;
export using ::HANDLE;
export using ::HGLOBAL;
export using ::HWND;
export using ::HDC;
export using ::HMONITOR;
export using ::HICON;
export using ::PSID;
export using ::LPCSTR;
export using ::LPCWSTR;
export using ::LPWSTR;
export using ::LPSTR;
export using ::BYTE;
export using ::WORD;
export using ::ULONG;
export using ::u_long;
export using ::INT_PTR;
export using ::WNDPROC;
export using ::POINT;
export using ::RECT;
export using ::MONITORINFO;
export using ::SID_IDENTIFIER_AUTHORITY;
export using ::STARTUPINFOW;
export using ::PROCESS_INFORMATION;
export using ::PROCESSENTRY32W;
export using ::SHELLEXECUTEINFOA;
export using ::_EXCEPTION_POINTERS;
export using ::PEXCEPTION_RECORD;
export using ::PEXCEPTION_POINTERS;
export using ::LPTOP_LEVEL_EXCEPTION_FILTER;

export using ::SC_HANDLE;
export using ::SERVICE_DESCRIPTIONW;
export using ::SERVICE_STATUS;
export using ::SERVICE_STATUS_PROCESS;
export using ::QUERY_SERVICE_CONFIGA;
export using ::ENUM_SERVICE_STATUSA;

// Enumerators exported with a using-declaration (they are not macros).
export using ::MDT_EFFECTIVE_DPI;
export using ::SC_STATUS_PROCESS_INFO;
export using ::DWMWA_USE_IMMERSIVE_DARK_MODE;

// --- Networking (winsock) ----------------------------------------------------
export using ::WSADATA;
export using ::WSAStartup;
export using ::WSACleanup;

export using ::SOCKET;
export using ::sockaddr;
export using ::sockaddr_in;
export using ::sockaddr_in6;
export using ::in6addr_loopback;
export using ::fd_set;
export using ::timeval;

export using ::socket;
export using ::closesocket;
export using ::bind;
export using ::getsockname;
export using ::setsockopt;
export using ::ioctlsocket;
export using ::sendto;
export using ::recvfrom;
export using ::select;
export using ::htons;
export using ::htonl;
export using ::ntohs;

// --- IP Helper ---------------------------------------------------------------
export using ::GetIpForwardTable;
export using ::MIB_IPFORWARDTABLE;
export using ::MIB_IPFORWARDROW;
export using ::GetBestRoute;
export using ::GetAdaptersAddresses;
export using ::IP_ADAPTER_ADDRESSES;
export using ::IfOperStatusUp;

// --- Certificate chains (wincrypt) -------------------------------------------
export using ::PCCERT_CONTEXT;
export using ::HCERTSTORE;
export using ::CERT_CHAIN_PARA;
export using ::CERT_ENHKEY_USAGE;
export using ::PCCERT_CHAIN_CONTEXT;
export using ::CertCreateCertificateContext;
export using ::CertOpenStore;
export using ::CertFreeCertificateContext;
export using ::CertAddCertificateContextToStore;
export using ::CertGetCertificateChain;
export using ::CertFreeCertificateChain;
export using ::CertCloseStore;

// --- Dynamic libraries and registry ------------------------------------------
export using ::HMODULE;
export using ::FARPROC;
export using ::LoadLibraryW;
export using ::GetProcAddress;
export using ::FreeLibrary;
export using ::HKEY;
export using ::LSTATUS;
export using ::RegGetValueW;
export using ::GetCurrentProcessId;
export using ::SetConsoleCtrlHandler;

// --- Calling-convention macros -----------------------------------------------
// WINAPI/CALLBACK/APIENTRY are not exported: a macro cannot cross a module
// boundary. The few units that declare a callback define them locally.

// --- Function-like macros ----------------------------------------------------
// The engine target is built with UNICODE defined, so these three wrappers
// forward to the W variants its macros would have selected (no string is passed
// either way, so the two variants are interchangeable here).
#undef SendMessage
export inline BOOL SendMessage(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
	return ::SendMessageW(hwnd, msg, w_param, l_param);
}

#undef PostMessage
export inline BOOL PostMessage(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
	return ::PostMessageW(hwnd, msg, w_param, l_param);
}

#undef SetClassLongPtr
export inline LONG_PTR SetClassLongPtr(HWND hwnd, int index, LONG_PTR value)
{
	return ::SetClassLongPtrW(hwnd, index, value);
}

// These three are called with narrow strings, and the core target is built
// without UNICODE, so they forward to the A variants.
#undef OpenSCManager
export inline SC_HANDLE OpenSCManager(LPCSTR machine, LPCSTR database, DWORD access)
{
	return ::OpenSCManagerA(machine, database, access);
}

#undef OpenService
export inline SC_HANDLE OpenService(SC_HANDLE manager, LPCSTR name, DWORD access)
{
	return ::OpenServiceA(manager, name, access);
}

#undef StartService
export inline BOOL StartService(SC_HANDLE service, DWORD argc, LPCSTR* argv)
{
	return ::StartServiceA(service, argc, argv);
}

#undef SUCCEEDED
export inline constexpr bool SUCCEEDED(HRESULT status)
{
	return status >= 0;
}

#undef MAKEWORD
export inline constexpr WORD MAKEWORD(BYTE low, BYTE high)
{
	return static_cast<WORD>(static_cast<WORD>(low) | (static_cast<WORD>(high) << 8));
}

// --- Value macros ------------------------------------------------------------
// Every macro below is #undef-ed first, because the macro from the header is
// still visible inside this module and would otherwise expand the declaration.
// The type matches the one the macro had in the header.

#undef TRUE
export inline constexpr BOOL TRUE = 1;
#undef FALSE
export inline constexpr BOOL FALSE = 0;

#undef CP_UTF8
export inline constexpr UINT CP_UTF8 = 65'001;
#undef GMEM_MOVEABLE
export inline constexpr UINT GMEM_MOVEABLE = 0x00'02;
#undef CF_UNICODETEXT
export inline constexpr UINT CF_UNICODETEXT = 13;
#undef MB_OK
export inline constexpr UINT MB_OK = 0x00'00'00'00L;

#undef SW_SHOWNORMAL
export inline constexpr int SW_SHOWNORMAL = 1;
#undef SW_NORMAL
export inline constexpr int SW_NORMAL = 1;
#undef MAX_PATH
export inline constexpr int MAX_PATH = 260;
#undef LOCALE_NAME_MAX_LENGTH
export inline constexpr int LOCALE_NAME_MAX_LENGTH = 85;

#undef STD_OUTPUT_HANDLE
export inline constexpr DWORD STD_OUTPUT_HANDLE = static_cast<DWORD>(-11);
#undef ATTACH_PARENT_PROCESS
export inline constexpr DWORD ATTACH_PARENT_PROCESS = static_cast<DWORD>(-1);
#undef INFINITE
export inline constexpr DWORD INFINITE = 0xFF'FF'FF'FF;
#undef ENABLE_VIRTUAL_TERMINAL_PROCESSING
export inline constexpr DWORD ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x00'04;

#undef WM_CLOSE
export inline constexpr UINT WM_CLOSE = 0x00'10;
#undef WM_SETICON
export inline constexpr UINT WM_SETICON = 0x00'80;
#undef WM_EXITSIZEMOVE
export inline constexpr UINT WM_EXITSIZEMOVE = 0x02'32;
#undef SC_CLOSE
export inline constexpr UINT SC_CLOSE = 0xF0'60;
#undef MF_GRAYED
export inline constexpr UINT MF_GRAYED = 0x00'00'00'01L;
#undef IMAGE_ICON
export inline constexpr UINT IMAGE_ICON = 1;
#undef LR_LOADFROMFILE
export inline constexpr UINT LR_LOADFROMFILE = 0x00'00'00'10;
#undef LR_DEFAULTSIZE
export inline constexpr UINT LR_DEFAULTSIZE = 0x00'00'00'40;

#undef ICON_BIG
export inline constexpr int ICON_BIG = 1;
#undef ICON_SMALL
export inline constexpr int ICON_SMALL = 0;
#undef GWLP_WNDPROC
export inline constexpr int GWLP_WNDPROC = -4;
#undef GCLP_HICON
export inline constexpr int GCLP_HICON = -14;
#undef GCLP_HICONSM
export inline constexpr int GCLP_HICONSM = -34;
#undef SM_CXICON
export inline constexpr int SM_CXICON = 11;
#undef SM_CYICON
export inline constexpr int SM_CYICON = 12;
#undef SM_CXSMICON
export inline constexpr int SM_CXSMICON = 49;
#undef SM_CYSMICON
export inline constexpr int SM_CYSMICON = 50;

#undef CREATE_NO_WINDOW
export inline constexpr DWORD CREATE_NO_WINDOW = 0x08'00'00'00;
#undef OPEN_EXISTING
export inline constexpr DWORD OPEN_EXISTING = 3;
#undef FILE_ATTRIBUTE_NORMAL
export inline constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x00'00'00'80;
#undef GENERIC_READ
export inline constexpr DWORD GENERIC_READ = 0x80'00'00'00L;
#undef GENERIC_WRITE
export inline constexpr DWORD GENERIC_WRITE = 0x40'00'00'00L;
#undef WAIT_TIMEOUT
export inline constexpr DWORD WAIT_TIMEOUT = 258L;
#undef WAIT_OBJECT_0
export inline constexpr DWORD WAIT_OBJECT_0 = 0;
#undef TH32CS_SNAPPROCESS
export inline constexpr DWORD TH32CS_SNAPPROCESS = 0x00'00'00'02;
#undef PROCESS_QUERY_LIMITED_INFORMATION
export inline constexpr DWORD PROCESS_QUERY_LIMITED_INFORMATION = 0x10'00;
#undef SYNCHRONIZE
export inline constexpr DWORD SYNCHRONIZE = 0x00'10'00'00L;
#undef SEE_MASK_NOCLOSEPROCESS
export inline constexpr DWORD SEE_MASK_NOCLOSEPROCESS = 0x00'00'00'40;

#undef ERROR_ACCESS_DENIED
export inline constexpr DWORD ERROR_ACCESS_DENIED = 5L;
#undef ERROR_SHARING_VIOLATION
export inline constexpr DWORD ERROR_SHARING_VIOLATION = 32L;
#undef ERROR_LOCK_VIOLATION
export inline constexpr DWORD ERROR_LOCK_VIOLATION = 33L;
#undef ERROR_INSUFFICIENT_BUFFER
export inline constexpr DWORD ERROR_INSUFFICIENT_BUFFER = 122L;
#undef ERROR_MORE_DATA
export inline constexpr DWORD ERROR_MORE_DATA = 234L;

#undef EXCEPTION_ACCESS_VIOLATION
export inline constexpr DWORD EXCEPTION_ACCESS_VIOLATION = 0xC0'00'00'05L;
#undef EXCEPTION_ILLEGAL_INSTRUCTION
export inline constexpr DWORD EXCEPTION_ILLEGAL_INSTRUCTION = 0xC0'00'00'1DL;
#undef EXCEPTION_INT_DIVIDE_BY_ZERO
export inline constexpr DWORD EXCEPTION_INT_DIVIDE_BY_ZERO = 0xC0'00'00'94L;
#undef EXCEPTION_STACK_OVERFLOW
export inline constexpr DWORD EXCEPTION_STACK_OVERFLOW = 0xC0'00'00'FDL;
#undef EXCEPTION_CONTINUE_SEARCH
export inline constexpr int EXCEPTION_CONTINUE_SEARCH = 0x0;
#undef EXCEPTION_EXECUTE_HANDLER
export inline constexpr int EXCEPTION_EXECUTE_HANDLER = 0x1;

#undef SC_MANAGER_ALL_ACCESS
export inline constexpr DWORD SC_MANAGER_ALL_ACCESS = 0xF'00'3F;
#undef SERVICE_CONFIG_DESCRIPTION
export inline constexpr DWORD SERVICE_CONFIG_DESCRIPTION = 1;
#undef SERVICE_NO_CHANGE
export inline constexpr DWORD SERVICE_NO_CHANGE = 0xFF'FF'FF'FF;
#undef SERVICE_CONTROL_STOP
export inline constexpr DWORD SERVICE_CONTROL_STOP = 0x00'00'00'01;
#undef SERVICE_STOPPED
export inline constexpr DWORD SERVICE_STOPPED = 0x00'00'00'01;
#undef SERVICE_START_PENDING
export inline constexpr DWORD SERVICE_START_PENDING = 0x00'00'00'02;
#undef SERVICE_STOP_PENDING
export inline constexpr DWORD SERVICE_STOP_PENDING = 0x00'00'00'03;
#undef SERVICE_RUNNING
export inline constexpr DWORD SERVICE_RUNNING = 0x00'00'00'04;
#undef SERVICE_CONTINUE_PENDING
export inline constexpr DWORD SERVICE_CONTINUE_PENDING = 0x00'00'00'05;
#undef SERVICE_AUTO_START
export inline constexpr DWORD SERVICE_AUTO_START = 0x00'00'00'02;
#undef SERVICE_ERROR_NORMAL
export inline constexpr DWORD SERVICE_ERROR_NORMAL = 0x00'00'00'01;
#undef SERVICE_WIN32_OWN_PROCESS
export inline constexpr DWORD SERVICE_WIN32_OWN_PROCESS = 0x00'00'00'10;
#undef SERVICE_WIN32
export inline constexpr DWORD SERVICE_WIN32 = 0x00'00'00'30;
#undef SERVICE_STATE_ALL
export inline constexpr DWORD SERVICE_STATE_ALL = 0x00'00'00'03;

#undef SECURITY_BUILTIN_DOMAIN_RID
export inline constexpr DWORD SECURITY_BUILTIN_DOMAIN_RID = 0x00'00'00'20L;
#undef DOMAIN_ALIAS_RID_ADMINS
export inline constexpr DWORD DOMAIN_ALIAS_RID_ADMINS = 0x00'00'02'20L;

// Macro in <winnt.h> that expands to a brace initializer.
#undef SECURITY_NT_AUTHORITY
export inline constexpr SID_IDENTIFIER_AUTHORITY SECURITY_NT_AUTHORITY{
	{ 0, 0, 0, 0, 0, 5 }
};

// Macro in <handleapi.h> that casts -1 to a handle; not constexpr because of
// the reinterpret_cast.
#undef INVALID_HANDLE_VALUE
export inline const HANDLE INVALID_HANDLE_VALUE = reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1));

// --- Networking, IP Helper, registry and crypto value macros -----------------
#undef AF_INET
export inline constexpr int AF_INET = 2;
#undef AF_INET6
export inline constexpr int AF_INET6 = 23;
#undef SOCK_DGRAM
export inline constexpr int SOCK_DGRAM = 2;
#undef SOL_SOCKET
export inline constexpr int SOL_SOCKET = 0xff'ff;
#undef SO_RCVBUF
export inline constexpr int SO_RCVBUF = 0x10'02;
#undef SO_SNDBUF
export inline constexpr int SO_SNDBUF = 0x10'01;
#undef SOCKET_ERROR
export inline constexpr int SOCKET_ERROR = -1;
// _IOW('f', 126, u_long): the header macro has type unsigned int.
#undef FIONBIO
export inline constexpr unsigned FIONBIO = 0x80'04'66'7e;
// (SOCKET)(~0): a cast of -1 to the pointer-sized SOCKET type.
#undef INVALID_SOCKET
export inline constexpr SOCKET INVALID_SOCKET = static_cast<SOCKET>(-1);
#undef INADDR_ANY
export inline constexpr ULONG INADDR_ANY = 0;
#undef INADDR_LOOPBACK
export inline constexpr int INADDR_LOOPBACK = 0x7f'00'00'01;

#undef GAA_FLAG_SKIP_ANYCAST
export inline constexpr ULONG GAA_FLAG_SKIP_ANYCAST = 0x00'02;
#undef GAA_FLAG_SKIP_MULTICAST
export inline constexpr ULONG GAA_FLAG_SKIP_MULTICAST = 0x00'04;
#undef IF_TYPE_SOFTWARE_LOOPBACK
export inline constexpr int IF_TYPE_SOFTWARE_LOOPBACK = 24;
#undef IF_TYPE_TUNNEL
export inline constexpr int IF_TYPE_TUNNEL = 131;
#undef NO_ERROR
export inline constexpr DWORD NO_ERROR = 0;
#undef ERROR_SUCCESS
export inline constexpr DWORD ERROR_SUCCESS = 0;

#undef RRF_RT_REG_DWORD
export inline constexpr DWORD RRF_RT_REG_DWORD = 0x00'00'00'10;

#undef CTRL_C_EVENT
export inline constexpr int CTRL_C_EVENT = 0;
#undef CTRL_BREAK_EVENT
export inline constexpr int CTRL_BREAK_EVENT = 1;
#undef CTRL_CLOSE_EVENT
export inline constexpr int CTRL_CLOSE_EVENT = 2;
#undef CTRL_SHUTDOWN_EVENT
export inline constexpr int CTRL_SHUTDOWN_EVENT = 6;

#undef X509_ASN_ENCODING
export inline constexpr DWORD X509_ASN_ENCODING = 0x00'00'00'01;
#undef PKCS_7_ASN_ENCODING
export inline constexpr DWORD PKCS_7_ASN_ENCODING = 0x00'01'00'00;
#undef USAGE_MATCH_TYPE_AND
export inline constexpr DWORD USAGE_MATCH_TYPE_AND = 0x00'00'00'00;
#undef CERT_TRUST_NO_ERROR
export inline constexpr DWORD CERT_TRUST_NO_ERROR = 0x00'00'00'00;
#undef CERT_STORE_ADD_ALWAYS
export inline constexpr DWORD CERT_STORE_ADD_ALWAYS = 4;

// ((LPCSTR) 2): a cast of an integer to a pointer.
#undef CERT_STORE_PROV_MEMORY
export inline const LPCSTR CERT_STORE_PROV_MEMORY = reinterpret_cast<LPCSTR>(static_cast<INT_PTR>(2));
#undef szOID_PKIX_KP_SERVER_AUTH
export inline constexpr const char* szOID_PKIX_KP_SERVER_AUTH = "1.3.6.1.5.5.7.3.1";

// ((HKEY)(ULONG_PTR)((LONG)0x80000002)): the LONG cast sign-extends the low
// 32-bit value, which matters on x64.
#undef HKEY_LOCAL_MACHINE
export inline const HKEY HKEY_LOCAL_MACHINE = reinterpret_cast<HKEY>(static_cast<ULONG_PTR>(static_cast<LONG>(0x80'00'00'02)));

// UCRT extensions from <stdio.h>/<time.h> that have no std:: equivalent. They
// are ordinary C functions, so the header declares them and `using` re-exports
// them: a declaration written by hand in the purview is not worth the trouble.
export using ::__acrt_iob_func;
export using ::freopen_s;
export using ::_popen;
export using ::_pclose;
export using ::ctime_s;

// The C streams are macros in <stdio.h>, and a macro cannot cross a module
// boundary. Variables with the same names are exported instead, so a call site
// reads as it did before (`freopen_s(..., stdin)`); the macro is removed first,
// otherwise the declarations below would expand into it. They are dynamically
// initialized (__acrt_iob_func is not constexpr), so nothing may read them
// during static initialization.
#undef stdin
#undef stdout
#undef stderr
export inline std::FILE* stdin	= ::__acrt_iob_func(0);
export inline std::FILE* stdout = ::__acrt_iob_func(1);
export inline std::FILE* stderr = ::__acrt_iob_func(2);
