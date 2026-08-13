#include "curl_client.h"
#include "net.h"
#include "zapret_helper.h"

int main()
{
	Winsock	   winsock;
	CurlGlobal curl_global;

	ZapretHelper helper;
	return helper.run();
}
