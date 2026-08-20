#include "curl_client.h"
#include "net.h"
#include "zapret_helper.h"

// NOLINTNEXTLINE(bugprone-exception-escape) - helper crash is handled by the service manager restart
int main()
{
	Winsock	   winsock;
	CurlGlobal curl_global;

	ZapretHelper helper;
	return helper.run();
}
